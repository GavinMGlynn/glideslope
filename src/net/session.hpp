#pragma once

// **A client's session with a server**: the handshake, the keys, and what
// arrives afterwards.
//
// This is the client half of the transport, in one place so that both
// frontends use the same one. It was written inside `glideslope_cli` first,
// which meant the client with the window could not connect to anything at
// all; a session belongs to the network, not to one program's `main`.
//
// **It owns its socket and nothing else.** It does not step an aircraft, does
// not draw and does not sleep: `poll()` is called as often as the caller
// likes and does what has arrived since the last one.
//
// **A handshake over UDP has to expect its first datagram to be lost**, and a
// server that is not listening yet answers nothing at all, so `connect()`
// resends the same initiation until it is answered or it gives up. The same
// initiation each time - `Noise_IK` makes one, and a second would be a second
// handshake, which a server treats as a duplicate.
//
// **A session the server has let go is joined again**, by itself, on the
// rules `glideslope_cli connect` keeps (docs/TRANSPORT.md): knocked on from
// this end when nothing has opened for a second; a `BAD_HANDSHAKE` believed
// only from the server's address after `quiet_before_believing_s` of
// nothing; then a fresh initiation with the same static key, the old
// session's keys kept to go back to if anything opens under them. The
// operator's drop - the server's sealed `LEAVING`, or `DROPPED` - ends it,
// and so does `SERVER_FULL`: `standing()` says which, and nothing more is
// sent.

#include "net/handshake.hpp"
#include "net/keys.hpp"
#include "net/messages.hpp"
#include "net/rejoin.hpp"
#include "net/reliable.hpp"
#include "net/sealing.hpp"
#include "net/state.hpp"
#include "net/told.hpp"
#include "platform/socket.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::net {

class ClientSession {
public:
    // **Completes a handshake with the server at `where`.** Nothing if the
    // address is not one, the key is not one, no socket can be opened, the
    // server refuses, or nothing answers within `give_up_after_s`.
    //
    // `resend_every_s` is how often the initiation goes out again while
    // nothing has come back.
    //
    // `aircraft` is the aeroplane asked for, by its catalogue id, in the
    // initiation (net::write_asked_aircraft) and every one joining again;
    // empty, the server's plan's.
    static std::optional<ClientSession> connect(const std::string& where,
                                                const std::string& key_hex,
                                                double give_up_after_s = 5.0,
                                                double resend_every_s = 0.25,
                                                const std::string& aircraft = "");

    // Read whatever has arrived: answer the server's knocking, and take in
    // any state update. `now_s` is the client's own clock, in seconds.
    // Knocks when the session has gone quiet, and, let go, joins again.
    void poll(double now_s);

    // **How the session stands.** `joined` is in one (the first, or one
    // joined again); `joining_again` has been let go and is asking for a new
    // one; the rest are ends, and nothing is sent after them: `dropped` - the
    // server's `LEAVING`, which is its operator's drop or a newer session for
    // this key taking over, or `DROPPED` when joining again, `refused` (`SERVER_FULL`), `gave_up` (a minute unanswered).
    enum class Standing { joined, joining_again, dropped, refused, gave_up };
    Standing standing() const { return standing_; }
    // How many times the server has let it go, and it has joined again; how
    // long nothing had opened when the last refusal was believed; how many
    // times it went back to the old session, which was not gone after all.
    int let_go() const { return let_go_; }
    int joined_again() const { return joined_again_; }
    double quiet_when_let_go_s() const { return quiet_when_let_go_s_; }
    int went_back() const { return went_back_; }
    // How many datagrams opened under the old session while joining again
    // that were not its answer to a knock - held from before it was let go,
    // and not gone back for (net::Rejoin).
    int stale_while_joining_again() const { return stale_; }

    // **How long a session may go without anything opening under it before
    // a refusal is believed**: the server knocks once a second and this end
    // once a second after one of nothing, so three of the server's knocks
    // and two of its own gone unanswered - a session that is not working,
    // whatever the refusal says. A forged one while it works moves nothing.
    static constexpr double quiet_before_believing_s = 3.0;
    static constexpr double knock_after_quiet_s = 1.0;
    // Initiations again every quarter of a second, for a minute at most.
    static constexpr double join_again_every_s = Rejoin::every_s;
    static constexpr double give_up_joining_again_s = 60.0;

    // **A test flag's work** (`glideslope --stall-after`): from now, as a
    // process stopped or a laptop shut, it sends nothing and answers
    // nothing, and what arrives is read and thrown away, until nothing has
    // come for three seconds - the server's knocks have stopped. Then it
    // knocks once a second; the server's refusal is the let-go, believed as
    // any other, and anything else heard stalls it again.
    void stall_until_let_go() { stalling_ = true; stall_heard_s_.reset(); }
    bool stalling() const { return stalling_; }

    // The server's key, which is who this session is with.
    const PublicKey& theirs() const { return theirs_; }
    // How many of the server's knocks have been answered, and how many state
    // updates have arrived. What a client shows to say it is connected.
    int answered() const { return answered_; }
    int heard() const { return heard_; }

    // Where every aircraft was in the newest state update, and which of them
    // is this client's own - `no_aircraft` until the server says.
    const std::vector<AircraftState>& aircraft() const { return aircraft_; }
    // **Every state update since the last time this was asked**, oldest
    // first - at most `most_states_kept` of them, which is a few seconds; the
    // newest, when more came.
    std::vector<StatePacket> take_states();
    static constexpr std::size_t most_states_kept = 128;
    // **What each aircraft is**, by its number, as the server's `AIRCRAFT`
    // messages said. An aircraft said to be somewhere before it has been
    // introduced is not in here yet.
    const std::map<std::uint8_t, AircraftDefinition>& roster() const { return roster_; }
    // **Who the last `CONTROLLER_SWAP` for an aircraft handed it to**, by its
    // number, or `nobody` if none has been heard: what says the learnt
    // landing has it, which a state update gives only as the AI.
    Controller last_swap(std::uint8_t aircraft) const {
        const auto it = swaps_.find(aircraft);
        return it == swaps_.end() ? Controller::nobody : it->second;
    }
    // **What the server has said the session is** (net::Told): the ground
    // it collides on, the session, the lobby and the weather it flies. A
    // session joined again is told afresh.
    const Told& told() const { return told_; }
    // **The take-overs the server has refused since last asked**, by the
    // aircraft each asked for (`TAKE_OVER_REFUSED`), oldest first.
    std::vector<std::uint8_t> take_refusals() {
        std::vector<std::uint8_t> out;
        out.swap(refusals_);
        return out;
    }
    // **The learnt landings the server has refused since last asked**, and
    // why (`LEARNT_LANDING_REFUSED`), oldest first.
    std::vector<LearntLandingRefused> take_learnt_refusals() {
        std::vector<LearntLandingRefused> out;
        out.swap(learnt_refusals_);
        return out;
    }
    std::uint8_t your_aircraft() const { return mine_; }
    double simulation_time_s() const { return clock_s_; }

    // Send this client's inputs, already packed by `InputSender::packet()`.
    void send_inputs(std::span<const std::uint8_t> packet);
    // Send a message that must arrive (`net::write` of one), repeated until it
    // has. **Only in a session**: false, and nothing queued, while joining
    // again or after an end - a session joined again is a new reliable
    // stream, and a message queued for the old one would never arrive.
    // Messages the old session had not yet had acknowledged when it was let
    // go are lost with it; the caller asks again if it still wants them.
    bool send_message(std::span<const std::uint8_t> body) {
        return standing_ == Standing::joined && sealing_ && reliable_.send(body);
    }

    // **Says goodbye** (`LEAVING`, `leaving_copies` times, each sealed
    // afresh), so that the server lets the session go at once rather than
    // after its `--timeout` of silence, and ends the session here: nothing
    // is sent or read after it. A session that has already left, or never
    // began, sends nothing.
    void leave();
    // **A session going away says goodbye**, whatever way its owner goes -
    // the window closed, a flight finished, an error returned - so that no
    // clean exit leaves a server waiting on the silence. A crash does not
    // run this, and the server's timeout is for that.
    ~ClientSession() { leave(); }
    ClientSession(ClientSession&&) noexcept = default;
    // Not assigned over: the session assigned over would have to say
    // goodbye first, and nothing needs it.
    ClientSession& operator=(ClientSession&&) = delete;
    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;

    // Send the initiation once more, as a network that duplicates a datagram
    // would. A test flag's work; a server answers it with what it already
    // said.
    void send_the_initiation_again();

    // **A test's forger's tools** (`glideslope_cli connect --forge-leaving`):
    // a whole `SEALED` datagram of `plaintext` under this session's keys,
    // not sent; and any datagram sent from this session's address. Together
    // they make one session's goodbye arrive from another's address.
    std::vector<std::uint8_t> sealed(std::span<const std::uint8_t> plaintext);
    void send_from_here(std::span<const std::uint8_t> datagram);

private:
    ClientSession() = default;

    std::unique_ptr<platform::UdpSocket> socket_;
    platform::Address server_;
    PublicKey theirs_;
    // This end's static key, which a session joined again is under too, and
    // the initiation's payload, which it asks with too.
    KeyPair mine_key_;
    std::vector<std::uint8_t> asked_;
    Standing standing_ = Standing::joined;
    // By this end's clock: when anything last opened, and it last knocked.
    std::optional<double> last_opened_s_;
    double knocked_s_ = -1.0e9;
    std::uint64_t knock_token_ = 0; // counted up from 1
    int let_go_ = 0;
    int joined_again_ = 0;
    int went_back_ = 0;
    double quiet_when_let_go_s_ = 0.0;
    // Joining again (net::Rejoin): when it began, and the old session's
    // keys, to go back to if the old session answers.
    std::unique_ptr<Rejoin> rejoin_;
    double again_began_s_ = 0.0;
    int stale_ = 0;
    std::unique_ptr<Sealer> old_sealing_;
    std::unique_ptr<Unsealer> old_opening_;
    bool stalling_ = false;
    std::optional<double> stall_heard_s_;
    std::unique_ptr<Sealer> sealing_;
    std::unique_ptr<Unsealer> opening_;
    std::vector<std::uint8_t> initiation_;
    int answered_ = 0;
    int heard_ = 0;
    std::uint8_t mine_ = no_aircraft;
    double clock_s_ = 0.0;
    std::uint32_t applied_ = 0;
    std::vector<AircraftState> aircraft_;
    std::vector<StatePacket> fresh_;
    Reliable reliable_;
    std::map<std::uint8_t, AircraftDefinition> roster_;
    std::map<std::uint8_t, Controller> swaps_;
    Told told_;
    std::vector<std::uint8_t> refusals_;
    std::vector<LearntLandingRefused> learnt_refusals_;
    // **Until anything has opened under this session, it knocks**: a sealed
    // `PING` every `prove_every_s` from the first `poll()`. A server sends a
    // session nothing but its handshake answer until something sealed under
    // it has opened (TRANSPORT.md), so a lost first datagram would otherwise
    // leave both ends waiting on the other.
    static constexpr double prove_every_s = 0.25;
    bool opened_any_ = false;
    std::optional<double> proved_at_s_;
    std::uint64_t proving_token_ = 0;

    void read_what_arrived(double now_s);
    void knock_on(std::uint64_t token);
    void prove_at_once();
    void let_go_at(double now_s);
    void keep_joining_again(double now_s);
};

} // namespace glideslope::net
