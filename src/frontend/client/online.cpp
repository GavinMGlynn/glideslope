#include "online.hpp"

#include "world/geodesy.hpp"

#include <algorithm>
#include <cmath>
#include <span>

namespace glideslope::client {
namespace {

constexpr double inputs_every_s = 1.0 / 30.0;
constexpr double radians = 3.14159265358979323846 / 180.0;

// North-east-down about `origin`: a position less the origin, or a velocity
// as it is.
void to_local(const world::Ecef& origin, double x, double y, double z, bool position,
              double& n, double& e, double& d) {
    const world::Geodetic g = world::to_geodetic(origin);
    const double lat = g.latitude_deg * radians;
    const double lon = g.longitude_deg * radians;
    if (position) {
        x -= origin.x;
        y -= origin.y;
        z -= origin.z;
    }
    n = -std::sin(lat) * std::cos(lon) * x - std::sin(lat) * std::sin(lon) * y +
        std::cos(lat) * z;
    e = -std::sin(lon) * x + std::cos(lon) * y;
    d = -std::cos(lat) * std::cos(lon) * x - std::cos(lat) * std::sin(lon) * y -
        std::sin(lat) * z;
}

world::Ecef from_local(const world::Ecef& origin, double n, double e, double d) {
    const world::Geodetic g = world::to_geodetic(origin);
    const double lat = g.latitude_deg * radians;
    const double lon = g.longitude_deg * radians;
    return {origin.x - std::sin(lat) * std::cos(lon) * n - std::sin(lon) * e -
                std::cos(lat) * std::cos(lon) * d,
            origin.y - std::sin(lat) * std::sin(lon) * n + std::cos(lon) * e -
                std::cos(lat) * std::sin(lon) * d,
            origin.z + std::cos(lat) * n - std::sin(lat) * d};
}

} // namespace

sim::Motion motion_of(const net::OwnMotion& y) {
    sim::Motion m;
    m.location_ecef_m = {y.x_m, y.y_m, y.z_m};
    for (std::size_t i = 0; i < 4; ++i) {
        m.attitude_local[i] = static_cast<double>(y.attitude[i]);
    }
    for (std::size_t i = 0; i < 3; ++i) {
        m.uvw_mps[i] = static_cast<double>(y.uvw_mps[i]);
        m.pqr_radps[i] = static_cast<double>(y.pqr_radps[i]);
    }
    return m;
}

std::optional<Joined> Online::joined_by(const net::StatePacket& state) const {
    if (!state.yours || state.your_aircraft == net::no_aircraft) {
        return std::nullopt;
    }
    const auto found = session_.roster().find(state.your_aircraft);
    if (found == session_.roster().end()) {
        return std::nullopt;
    }
    Joined joined;
    joined.number = state.your_aircraft;
    joined.aircraft_id = found->second.id;
    joined.motion = motion_of(*state.yours);
    return joined;
}

sim::Controls Online::fly(double local_s, const sim::Controls& stick, Flight& flight) {
    // **What is sent is what is flown**: rounded as the wire rounds it, so
    // that this client and the server fly the same numbers.
    if (local_s - sent_at_s_ >= inputs_every_s) {
        sent_at_s_ = local_s;
        ++sequence_;
        const net::ControlList sent = net::as_sent(stick.as_list());
        flying_ = sim::Controls::from_list(sent);
        sending_.add(sequence_, sent);
        const std::vector<std::uint8_t> packet = sending_.packet();
        session_.send_inputs(std::span<const std::uint8_t>(packet.data(), packet.size()));
        flight.set_input_sequence(sequence_);
    }
    session_.poll(local_s);
    for (const net::StatePacket& state : session_.take_states()) {
        heard(state, local_s, flight);
    }
    return flying_;
}

void Online::heard(const net::StatePacket& state, double local_s, Flight& flight) {
    clock_.heard(state.simulation_time_s, local_s);
    // **Its own, from the newest word only**: an update older than one
    // already used would put it back to where the newer one had moved it
    // from, with the inputs since already let go.
    if (state.yours && state.your_aircraft == mine_ &&
        (!reconciled_s_ || state.simulation_time_s > *reconciled_s_)) {
        reconciled_s_ = state.simulation_time_s;
        const auto c = flight.reconcile(motion_of(*state.yours), state.last_input_applied);
        ++corrections_;
        worst_correction_m_ = std::max(worst_correction_m_, c.moved_m);
        if (c.snapped) {
            ++snapped_;
        }
    }
    // **Everybody else, to be drawn behind the clock.**
    for (const net::AircraftState& a : state.aircraft) {
        if (a.index == mine_) {
            continue;
        }
        if (!origin_) {
            origin_ = world::Ecef{a.x_m, a.y_m, a.z_m};
        }
        net::RemoteState r;
        r.time_s = state.simulation_time_s;
        to_local(*origin_, a.x_m, a.y_m, a.z_m, true, r.north_m, r.east_m, r.down_m);
        to_local(*origin_, static_cast<double>(a.vx_mps), static_cast<double>(a.vy_mps),
                 static_cast<double>(a.vz_mps), false, r.north_mps, r.east_mps, r.down_mps);
        r.heading_deg = static_cast<double>(a.heading_deg);
        r.pitch_deg = static_cast<double>(a.pitch_deg);
        r.roll_deg = static_cast<double>(a.roll_deg);
        shown_[a.index].received(r);
        wrecked_[a.index] = a.condition == net::Condition::wrecked;
    }
    // One no longer in the updates is no longer in the sky.
    for (auto it = shown_.begin(); it != shown_.end();) {
        const bool still = std::any_of(state.aircraft.begin(), state.aircraft.end(),
                                       [&](const net::AircraftState& a) {
                                           return a.index == it->first;
                                       });
        it = still ? std::next(it) : shown_.erase(it);
    }
}

std::vector<Other> Online::others(double local_s) {
    std::vector<Other> out;
    if (!clock_.known() || !origin_) {
        return out;
    }
    const double now = clock_.now(local_s);
    for (auto& [number, shown] : shown_) {
        if (!shown.known()) {
            continue;
        }
        const net::RemoteState at = shown.at(now);
        Other o;
        o.number = number;
        const auto found = session_.roster().find(number);
        if (found != session_.roster().end()) {
            o.aircraft_id = found->second.id;
        }
        o.centre = from_local(*origin_, at.north_m, at.east_m, at.down_m);
        o.heading_deg = at.heading_deg;
        o.pitch_deg = at.pitch_deg;
        o.roll_deg = at.roll_deg;
        o.wrecked = wrecked_[number];
        out.push_back(o);
    }
    return out;
}

} // namespace glideslope::client
