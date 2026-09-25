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
#include "net/session.hpp"
#include "sim/aircraft.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
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

    // **One frame.** Sends the stick if an input is due, reads what has
    // arrived, and puts `flight` right from the newest word on it. Returns
    // the controls to fly this frame: the stick as it was last sent.
    sim::Controls fly(double local_s, const sim::Controls& stick, Flight& flight);

    // **Kept in the session and nothing more**, for a client the server gave
    // no aircraft: its knocking answered, what arrives read and let go.
    void idle(double local_s) {
        session_.poll(local_s);
        (void)session_.take_states();
    }

    // Every other aircraft, where it is to be drawn at `local_s`.
    std::vector<Other> others(double local_s);

    // **Riding along** in the aircraft numbered `number` (`WATCH`), or in
    // none with `net::no_aircraft`: told its controls from then on.
    void watch(std::uint8_t number);
    std::uint8_t watching() const { return watching_; }
    // Its controls at `local_s`, 100 ms behind the clock as its position is,
    // or nothing before two updates of them either side have come.
    std::optional<net::Watched> watched_controls(double local_s) const;
    // How many updates have carried the watched aircraft's controls.
    std::size_t watched_heard() const { return watched_heard_; }

    std::size_t corrections() const { return corrections_; }
    std::size_t snapped() const { return snapped_; }
    double worst_correction_m() const { return worst_correction_m_; }

private:
    void heard(const net::StatePacket& state, double local_s, Flight& flight);
    std::optional<Joined> joined_by(const net::StatePacket& state) const;

    net::ClientSession session_;
    net::InputSender sending_;
    std::uint32_t sequence_ = 0;
    double sent_at_s_ = -1.0;
    sim::Controls flying_;
    std::uint8_t mine_ = net::no_aircraft;
    std::optional<double> reconciled_s_;
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
    std::size_t snapped_ = 0;
    double worst_correction_m_ = 0.0;
};

// A state update's own motion, as the simulation takes it.
sim::Motion motion_of(const net::OwnMotion& yours);

template <typename Clock>
std::optional<Joined> Online::join(double give_up_after_s, Clock local_s) {
    const double began = local_s();
    std::optional<net::StatePacket> newest;
    while (local_s() - began < give_up_after_s) {
        session_.poll(local_s());
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
