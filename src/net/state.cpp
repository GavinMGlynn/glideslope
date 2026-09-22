#include "net/state.hpp"

#include "net/inside.hpp"
#include "net/protocol.hpp"

#include <cmath>

namespace glideslope::net {
namespace {

// The kind byte, the clock, the input sequence and the count.
constexpr std::size_t header_bytes = 1 + 8 + 4 + 1;
// An index, a controller, three doubles and six floats.
constexpr std::size_t per_aircraft_bytes = 1 + 1 + 3 * 8 + 6 * 4;

bool a_number(double v) {
    return std::isfinite(v);
}

bool a_number(float v) {
    return std::isfinite(v);
}

} // namespace

std::size_t state_bytes(std::size_t count) {
    return header_bytes + count * per_aircraft_bytes;
}

std::optional<std::vector<std::uint8_t>> write_state(const StatePacket& state) {
    if (state.aircraft.size() > most_aircraft_in_a_state) {
        return std::nullopt;
    }
    // **A number that is not one is refused here rather than on the wire.**
    // Writing a NaN would put it where the reader has to catch it, and a
    // caller that has one has a bug worth failing on.
    if (!a_number(state.simulation_time_s)) {
        return std::nullopt;
    }
    for (const AircraftState& a : state.aircraft) {
        if (!a_number(a.x_m) || !a_number(a.y_m) || !a_number(a.z_m) ||
            !a_number(a.vx_mps) || !a_number(a.vy_mps) || !a_number(a.vz_mps) ||
            !a_number(a.heading_deg) || !a_number(a.pitch_deg) ||
            !a_number(a.roll_deg)) {
            return std::nullopt;
        }
        if (!known_controller(static_cast<std::uint8_t>(a.controller))) {
            return std::nullopt;
        }
    }

    Writer w;
    w.u8(static_cast<std::uint8_t>(Inside::state));
    w.f64(state.simulation_time_s);
    w.u32(state.last_input_applied);
    w.u8(static_cast<std::uint8_t>(state.aircraft.size()));
    for (const AircraftState& a : state.aircraft) {
        w.u8(a.index);
        w.u8(static_cast<std::uint8_t>(a.controller));
        w.f64(a.x_m);
        w.f64(a.y_m);
        w.f64(a.z_m);
        w.f32(a.vx_mps);
        w.f32(a.vy_mps);
        w.f32(a.vz_mps);
        w.f32(a.heading_deg);
        w.f32(a.pitch_deg);
        w.f32(a.roll_deg);
    }
    return w.take();
}

std::optional<StatePacket> read_state(std::span<const std::uint8_t> body) {
    Reader r(body);
    if (r.u8() != static_cast<std::uint8_t>(Inside::state)) {
        return std::nullopt;
    }
    StatePacket out;
    out.simulation_time_s = r.f64();
    out.last_input_applied = r.u32();
    const std::uint8_t count = r.u8();
    if (!r.ok() || count > most_aircraft_in_a_state) {
        return std::nullopt;
    }
    if (!a_number(out.simulation_time_s)) {
        return std::nullopt;
    }
    out.aircraft.reserve(count);
    for (std::uint8_t i = 0; i < count; ++i) {
        AircraftState a;
        a.index = r.u8();
        const std::uint8_t controller = r.u8();
        if (!known_controller(controller)) {
            return std::nullopt;
        }
        a.controller = static_cast<Controller>(controller);
        a.x_m = r.f64();
        a.y_m = r.f64();
        a.z_m = r.f64();
        a.vx_mps = r.f32();
        a.vy_mps = r.f32();
        a.vz_mps = r.f32();
        a.heading_deg = r.f32();
        a.pitch_deg = r.f32();
        a.roll_deg = r.f32();
        if (!r.ok()) {
            return std::nullopt;
        }
        if (!a_number(a.x_m) || !a_number(a.y_m) || !a_number(a.z_m) ||
            !a_number(a.vx_mps) || !a_number(a.vy_mps) || !a_number(a.vz_mps) ||
            !a_number(a.heading_deg) || !a_number(a.pitch_deg) ||
            !a_number(a.roll_deg)) {
            return std::nullopt;
        }
        out.aircraft.push_back(a);
    }
    // **Anything trailing means it is not this packet.** A reader that
    // ignored the tail would take a longer thing for a shorter one.
    if (!r.done()) {
        return std::nullopt;
    }
    return out;
}

} // namespace glideslope::net
