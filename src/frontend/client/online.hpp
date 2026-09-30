#pragma once

// The client with the window, flown on a server.
//
// **The server owns the aircraft; this client predicts it.** It joins, waits
// to be told which aircraft is its own and what aeroplane that is (`AIRCRAFT`),
// and the flight is built there and set to the server's motion. From then on
// every frame: the stick is sent thirty times a second, rounded as the wire
// rounds it, and the flight flies exactly what was sent under the sequence it
// was sent with; every state update is read, the newest put the flight right
// (sim::Prediction), and every other aircraft is kept to be drawn 100 ms
// behind the session's clock (net::Interpolated, net::SessionClock).
//
// **It draws nothing.** It says where every other aircraft is to be drawn;
// what they look like is the caller's.

#include "flight.hpp"
#include "net/inputs.hpp"
#include "net/interpolation.hpp"
#include "net/messages.hpp"
#include "net/session.hpp"
#include "net/state.hpp"
#include "sim/aircraft.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <string>
#include <thread>
#include <vector>

namespace glideslope::client {

// What the server gave this client: its aircraft's number, what aeroplane it
// is, and where it is.
struct Joined {
    std::uint8_t number = net::no_aircraft;
    std::string aircraft_id;
    sim::Motion motion;
    // The server's step the motion was true at: what a take-over flies it on
    // from (Flight::adopt).
    std::uint64_t server_steps = 0;
    // Given by joining again after the server let this client go, not by a
    // take-over.
    bool again = false;
};

// Another aircraft, where it is to be drawn now.
struct Other {
    std::uint8_t number = 0;
    std::string aircraft_id; // empty until the server has said
    world::Ecef centre;
    double heading_deg = 0.0;
    double pitch_deg = 0.0;
    double roll_deg = 0.0;
    // Its velocity over the Earth, north, east and down, metres a second.
    double north_mps = 0.0;
    double east_mps = 0.0;
    double down_mps = 0.0;
    // **How fast where it is drawn moves**, in the Earth-centred frame, per
    // second of this machine's clock: the interpolation's own path, not the
    // velocity an update reports, which in a turn or with jitter is not where
    // it is drawn next (net::Interpolated::path_velocity).
    std::array<double, 3> path_mps{};
    bool ai_flying = false;
    bool wrecked = false;
};

class Online {
public:
    explicit Online(net::ClientSession session) : session_(std::move(session)) {}

    // **Waits to be given an aircraft**: until a state update carries this
    // client's own motion and the server has said what aeroplane it is, or
    // `give_up_after_s` passes. `local_s` reads this machine's clock.
    template <typename Clock>
    std::optional<Joined> join(double give_up_after_s, Clock local_s);

    // **One frame, before its ticks are flown.** Sends the stick if an input
    // is due. Returns the controls to fly this frame: the stick as it was
    // last sent.
    sim::Controls fly(double local_s, const sim::Controls& stick, Flight& flight);

    // **The same frame, after its ticks are flown.** Reads what has arrived,
    // and puts `flight` right once, from the newest word on it; the older
    // ones give the clocks' difference alone. After the ticks and not
    // before: a word heard at the end of a long frame is about a moment its
    // ticks have not yet reached, and heard before them it put the
    // prediction where the server was, with nothing to replay, for the ticks
    // to fly it on past. And once: put right from every word in turn, each
    // replaying the inputs since, a long frame made the next one longer.
    // Together, 26.8 m on CI at a frame of 1.7 s, and 33 m here at frames
    // held 0.7 s (PROJECT_STATUS.md, 2026-09-30).
    void hear(double local_s, Flight& flight);

    // **Kept in the session and nothing more**, for a client the server gave
    // no aircraft: its knocking answered, what arrives read and let go.
    void idle(double local_s) {
        session_.poll(local_s);
        noticed();
        (void)session_.take_states();
    }

    // **Kept in the session while the flight is built**: the server's pings
    // answered and what must arrive acknowledged, and every update left
    // waiting, to be heard in order when flying begins - as they were before
    // there was anything to keep it, so the flight is put right from all of
    // them and not from the newest alone.
    void keep(double local_s) {
        session_.poll(local_s);
        noticed();
    }

    // **Leaving the session**: goodbye said to the server, which lets this
    // client go at once. Nothing is sent or read after it. The session says
    // it by itself as it goes, if this was never called.
    void leave() { session_.leave(); }

    // **The session, as it stands**: let go and joined again, or ended by
    // the server's operator (net::ClientSession::standing).
    const net::ClientSession& session() const { return session_; }
    // A test's stall (`--stall-after`): net::ClientSession::stall_until_let_go.
    void stall() { session_.stall_until_let_go(); }

    // Every other aircraft, where it is to be drawn at `local_s`.
    std::vector<Other> others(double local_s);

    // **Riding along** in the aircraft numbered `number` (`WATCH`), or in
    // none with `net::no_aircraft`: told its controls from then on.
    void watch(std::uint8_t number);
    std::uint8_t watching() const { return watching_; }
    // **Taking over** the aircraft numbered `number`, an AI's: asked of the
    // server, which may refuse. When it is done, the next update gives this
    // client that aircraft as its own, and `taken_over` says which.
    void take_over(std::uint8_t number);
    // The aircraft taken over since last asked - or given by joining again,
    // which may be under its old number - and what it is, or nothing:
    // the caller's flight becomes it (Flight::adopt, or a new Flight where it
    // is another aeroplane).
    std::optional<Joined> taken_over();
    // **Handing its own aircraft to the AI pilot, or taking it back**
    // (`CONTROLLER_SWAP` for its own number): asked of the server, which
    // decides. Handed over, its own is no longer predicted - nothing sent
    // flies it - and is among `others`, drawn from the updates as any other
    // is; taken back, the flight is put where the next update says and
    // predicted again from there.
    void hand_over(bool to_ai);
    // **Its copilot's route** (`COPILOT_ROUTE`) for its own aircraft: the
    // model was asked on this machine, with the player's key; only the route
    // goes. The server checks it, and flies it with its AI - handing the
    // aircraft over first if the player was flying it - or refuses it.
    void send_route(net::CopilotRoute route);
    // Its own aircraft as the newest update had it, and that update's time
    // on the session's clock: what its copilot is told.
    const std::optional<std::pair<double, net::AircraftState>>& own_heard() const {
        return own_heard_;
    }
    // Who the server said flies it changed since last asked: the caller draws
    // a frame of the switch, and says so.
    bool switched() {
        const bool out = switched_;
        switched_ = false;
        return out;
    }
    std::uint8_t mine() const { return mine_; }
    // **What the server last said of this client's own aircraft**: whether
    // the AI is flying it, and whether, since it was taken over, the server
    // has applied an input this client sent - which it does only if it is
    // flying it by them.
    bool own_ai_flying() const { return own_ai_flying_; }
    // The last input sent.
    std::uint32_t sequence() const { return sequence_; }
    // Whether it was last had by taking it back from the AI (rather than
    // taking another over): what `flown_since_taken_over` counts from.
    bool taken_back() const { return taken_back_; }
    // Whether it was last had by joining again.
    bool had_by_joining_again() const { return rejoined_; }
    bool flown_since_taken_over() const {
        return taken_at_ && applied_ > *taken_at_;
    }
    // **Gone back to its old session** after a refusal it believed - forged,
    // or a blip - and whether the server has since applied an input sent
    // after it went back: the same session, flown on from here.
    bool gone_back() const { return back_at_.has_value(); }
    // The last input the server has said it applied, and the last sent
    // when it went back: what a test that fails is shown.
    std::uint32_t applied() const { return applied_; }
    std::uint32_t back_at() const { return back_at_ ? *back_at_ : 0; }
    bool flown_since_going_back() const { return back_at_ && applied_ > *back_at_; }

    // Its controls at `local_s`, 100 ms behind the clock as its position is,
    // or nothing before two updates of them either side have come.
    std::optional<net::Watched> watched_controls(double local_s) const;
    // How many updates have carried the watched aircraft's controls.
    std::size_t watched_heard() const { return watched_heard_; }

    // **Put right since last asked** by a correction small enough to hide
    // (sim::snap_beyond_m): what is shown takes it up over the next frames.
    bool corrected() {
        const bool out = corrected_;
        corrected_ = false;
        return out;
    }
    std::size_t corrections() const { return corrections_; }
    // **What it heard of its own, predicted**: how many words, over how long
    // of the server's time from the first to the last, and how many frames
    // were put right from them - one correction each, from the newest; a
    // frame whose words ended with the aircraft handed to the AI is not
    // counted. A test holds the words to the server's rate, whatever the
    // frame rate.
    std::size_t own_words_heard() const { return own_words_; }
    double own_words_span_s() const {
        return own_words_ > 0 ? last_own_word_s_ - first_own_word_s_ : 0.0;
    }
    std::size_t frames_that_heard_own() const { return frames_heard_own_; }
    std::size_t snapped() const { return snapped_; }
    double worst_correction_m() const { return worst_correction_m_; }

private:
    void heard(const net::StatePacket& state, double local_s, Flight& flight);
    std::optional<Joined> joined_by(const net::StatePacket& state) const;
    // **A session joined again is a new start**: nothing it said of this
    // client's own aircraft holds, and the next word of one is taken as a
    // take-over is, even under the old number.
    void noticed();

    net::ClientSession session_;
    net::InputSender sending_;
    std::uint32_t sequence_ = 0;
    double sent_at_s_ = -1.0;
    sim::Controls flying_;
    std::uint8_t mine_ = net::no_aircraft;
    std::optional<Joined> taken_;
    // The input sent last when the take-over was heard, and the last the
    // server has applied.
    std::optional<std::uint32_t> taken_at_;
    std::uint32_t applied_ = 0;
    bool own_ai_flying_ = false;
    bool switched_ = false;
    bool taken_back_ = false;
    bool rejoined_ = false;
    // Joined again, and not yet told which aircraft is its own.
    bool rejoining_ = false;
    int joined_again_seen_ = 0;
    // How many times the session went back, as last noticed, and the last
    // input sent when it was.
    int went_back_seen_ = 0;
    std::optional<std::uint32_t> back_at_;
    // Taken back, and not yet put where the server says it is.
    bool resuming_ = false;
    std::optional<double> reconciled_s_;
    // **The newest word on its own aircraft this frame has heard**, to be put
    // right from once all of them are (`hear`).
    struct OwnWord {
        sim::Motion motion;
        std::uint32_t last_applied = 0;
        std::size_t steps_into = 0;
        std::uint64_t server_steps = 0;
        // Where it is, not a correction: the first word since joining, or
        // since it was taken back from the AI.
        bool adopt = false;
    };
    std::optional<OwnWord> own_word_;
    net::SessionClock clock_;
    // A local frame to interpolate in: north-east-down about where this
    // client joined.
    std::optional<world::Ecef> origin_;
    std::map<std::uint8_t, net::Interpolated> shown_;
    std::map<std::uint8_t, bool> wrecked_;
    std::map<std::uint8_t, bool> ai_;
    std::uint8_t watching_ = net::no_aircraft;
    std::map<double, net::Watched> watched_;
    std::size_t watched_heard_ = 0;
    std::size_t corrections_ = 0;
    std::size_t own_words_ = 0;
    double first_own_word_s_ = 0.0;
    double last_own_word_s_ = 0.0;
    std::size_t frames_heard_own_ = 0;
    bool corrected_ = false;
    std::size_t snapped_ = 0;
    double worst_correction_m_ = 0.0;
    std::optional<std::pair<double, net::AircraftState>> own_heard_;
};

// A state update's own motion, as the simulation takes it.
sim::Motion motion_of(const net::OwnMotion& yours);

template <typename Clock>
std::optional<Joined> Online::join(double give_up_after_s, Clock local_s) {
    const double began = local_s();
    std::optional<net::StatePacket> newest;
    while (local_s() - began < give_up_after_s) {
        session_.poll(local_s());
        noticed();
        for (net::StatePacket& state : session_.take_states()) {
            if (state.yours) {
                newest = std::move(state);
            }
        }
        if (newest) {
            if (auto joined = joined_by(*newest)) {
                mine_ = joined->number;
                return joined;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return std::nullopt;
}

} // namespace glideslope::client
