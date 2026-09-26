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
    // The watched aircraft's controls, by the time they were true.
    if (state.watched && state.watched->aircraft == watching_) {
        watched_[state.simulation_time_s] = *state.watched;
        ++watched_heard_;
        while (watched_.size() > 64) {
            watched_.erase(watched_.begin());
        }
    }
    // **Another aircraft is this client's own now**: one it took over. The
    // flight is not put right by it - it is another aircraft - but made it,
    // by the caller, from this update's motion. Only by the newest word: one
    // the network held back from before the take-over names the aircraft
    // given up, and would take that over again (the command-line client did,
    // through 200 ms of jitter, 2026-09-26).
    if (state.yours && mine_ != net::no_aircraft && state.your_aircraft != mine_ &&
        state.your_aircraft != net::no_aircraft &&
        (!reconciled_s_ || state.simulation_time_s > *reconciled_s_)) {
        if (auto joined = joined_by(state)) {
            mine_ = state.your_aircraft;
            taken_ = std::move(joined);
            taken_at_ = sequence_;
            reconciled_s_ = state.simulation_time_s;
            shown_.erase(mine_);
            watch(net::no_aircraft);
            for (const net::AircraftState& a : state.aircraft) {
                if (a.index == mine_) {
                    own_ai_flying_ = a.controller == net::Controller::ai;
                }
            }
            return;
        }
    }
    // **Its own, from the newest word only**: an update older than one
    // already used would put it back to where the newer one had moved it
    // from, with the inputs since already let go.
    if (state.yours && state.your_aircraft == mine_ &&
        (!reconciled_s_ || state.simulation_time_s > *reconciled_s_)) {
        // **The first word since joining is where it is**, not a correction:
        // a machine slow to build its flight after joining heard nothing of
        // it for seconds while the server flew it on, and was then put right
        // by the whole way flown - 46 m, too far to hide.
        if (!reconciled_s_) {
            reconciled_s_ = state.simulation_time_s;
            flight.adopt(motion_of(*state.yours));
        } else {
            reconciled_s_ = state.simulation_time_s;
            const auto c = flight.reconcile(motion_of(*state.yours), state.last_input_applied);
            ++corrections_;
            worst_correction_m_ = std::max(worst_correction_m_, c.moved_m);
            if (c.snapped) {
                ++snapped_;
            }
        }
    }
    // **Everybody else, to be drawn behind the clock**, and what the server
    // says of this client's own.
    if (state.yours && state.your_aircraft == mine_) {
        applied_ = std::max(applied_, state.last_input_applied);
    }
    for (const net::AircraftState& a : state.aircraft) {
        if (a.index == mine_) {
            // Who flies it now, by the newest word only: an older one may be
            // from before it was taken over, when the AI did.
            if (!reconciled_s_ || state.simulation_time_s >= *reconciled_s_) {
                own_ai_flying_ = a.controller == net::Controller::ai;
            }
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
        ai_[a.index] = a.controller == net::Controller::ai;
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
        o.north_mps = at.north_mps;
        o.east_mps = at.east_mps;
        o.down_mps = at.down_mps;
        o.ai_flying = ai_[number];
        o.wrecked = wrecked_[number];
        out.push_back(o);
    }
    return out;
}

void Online::watch(std::uint8_t number) {
    watching_ = number;
    watched_.clear();
    net::Watch w;
    w.aircraft = number;
    const std::vector<std::uint8_t> body = net::write(w);
    session_.send_message(std::span<const std::uint8_t>(body.data(), body.size()));
}

std::optional<net::Watched> Online::watched_controls(double local_s) const {
    if (!clock_.known() || watched_.empty()) {
        return std::nullopt;
    }
    const double at = clock_.now(local_s) - net::shown_behind_s;
    const auto after = watched_.lower_bound(at);
    if (after == watched_.end()) {
        // Past the newest: held where it was, as a gauge would be.
        return watched_.rbegin()->second;
    }
    if (after == watched_.begin()) {
        // **Before the oldest kept: held there**, as past the newest. A client
        // whose clock runs more than the 64 kept behind them - a slow debug
        // build on a Windows runner did - would otherwise show no controls at
        // all, for as long as it ran.
        return watched_.begin()->second;
    }
    const auto before = std::prev(after);
    const double span = after->first - before->first;
    const double k = span > 0.0 ? (at - before->first) / span : 0.0;
    const auto mix = [k](double a, double b) { return a + k * (b - a); };
    net::Watched out = before->second;
    out.aileron = mix(before->second.aileron, after->second.aileron);
    out.elevator = mix(before->second.elevator, after->second.elevator);
    out.rudder = mix(before->second.rudder, after->second.rudder);
    out.throttle = mix(before->second.throttle, after->second.throttle);
    out.flaps = mix(before->second.flaps, after->second.flaps);
    return out;
}

void Online::take_over(std::uint8_t number) {
    net::ControllerSwap swap;
    swap.aircraft = number;
    swap.to = net::Controller::person;
    const std::vector<std::uint8_t> body = net::write(swap);
    session_.send_message(std::span<const std::uint8_t>(body.data(), body.size()));
}

std::optional<Joined> Online::taken_over() {
    std::optional<Joined> out;
    out.swap(taken_);
    return out;
}

} // namespace glideslope::client
