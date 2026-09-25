#include "net/state.hpp"

#include "net/inputs.hpp"
#include "net/inside.hpp"
#include "net/protocol.hpp"

#include <cmath>

namespace glideslope::net {
namespace {

bool a_number(double v) {
    return std::isfinite(v);
}

bool a_number(float v) {
    return std::isfinite(v);
}

} // namespace

std::size_t state_bytes(std::size_t count, bool with_yours, bool with_watched) {
    // The flags that say whether this client's motion follows, and the
    // watched aircraft's controls, are always there.
    return state_header_bytes + count * state_per_aircraft_bytes +
           (with_yours ? own_motion_bytes : 1) + (with_watched ? watched_bytes : 1);
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
        if (!known_controller(static_cast<std::uint8_t>(a.controller)) ||
            !known_condition(static_cast<std::uint8_t>(a.condition))) {
            return std::nullopt;
        }
    }
    if (state.yours) {
        const OwnMotion& m = *state.yours;
        bool numbers = a_number(m.x_m) && a_number(m.y_m) && a_number(m.z_m);
        for (const float v : m.attitude) numbers = numbers && a_number(v);
        for (const float v : m.uvw_mps) numbers = numbers && a_number(v);
        for (const float v : m.pqr_radps) numbers = numbers && a_number(v);
        if (!numbers) {
            return std::nullopt;
        }
    }

    Writer w;
    w.u8(static_cast<std::uint8_t>(Inside::state));
    w.f64(state.simulation_time_s);
    w.u32(state.last_input_applied);
    w.u8(state.your_aircraft);
    w.u8(static_cast<std::uint8_t>(state.aircraft.size()));
    for (const AircraftState& a : state.aircraft) {
        w.u8(a.index);
        w.u8(static_cast<std::uint8_t>(a.controller));
        w.u8(static_cast<std::uint8_t>(a.condition));
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
    w.u8(state.yours ? 1 : 0);
    if (state.yours) {
        const OwnMotion& m = *state.yours;
        w.f64(m.x_m);
        w.f64(m.y_m);
        w.f64(m.z_m);
        for (const float v : m.attitude) {
            w.f32(v);
        }
        for (const float v : m.uvw_mps) {
            w.f32(v);
        }
        for (const float v : m.pqr_radps) {
            w.f32(v);
        }
    }
    w.u8(state.watched ? 1 : 0);
    if (state.watched) {
        const Watched& c = *state.watched;
        w.u8(c.aircraft);
        for (const double v : {c.aileron, c.elevator, c.rudder, c.throttle, c.flaps}) {
            w.u16(static_cast<std::uint16_t>(quantise(v)));
        }
        w.u16(static_cast<std::uint16_t>(c.gear ? quantise(*c.gear) : gear_fixed));
    }
    return w.take();
}

bool known_condition(std::uint8_t value) {
    return value == static_cast<std::uint8_t>(Condition::flying) ||
           value == static_cast<std::uint8_t>(Condition::wrecked);
}

std::optional<StatePacket> read_state(std::span<const std::uint8_t> body) {
    Reader r(body);
    if (r.u8() != static_cast<std::uint8_t>(Inside::state)) {
        return std::nullopt;
    }
    StatePacket out;
    out.simulation_time_s = r.f64();
    out.last_input_applied = r.u32();
    out.your_aircraft = r.u8();
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
        const std::uint8_t condition = r.u8();
        if (!known_condition(condition)) {
            return std::nullopt;
        }
        a.condition = static_cast<Condition>(condition);
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
    // **This client's own motion**, after a flag saying whether it is there.
    const std::uint8_t has_yours = r.u8();
    if (!r.ok() || has_yours > 1) {
        return std::nullopt;
    }
    if (has_yours == 1) {
        OwnMotion m;
        m.x_m = r.f64();
        m.y_m = r.f64();
        m.z_m = r.f64();
        for (float& v : m.attitude) {
            v = r.f32();
        }
        for (float& v : m.uvw_mps) {
            v = r.f32();
        }
        for (float& v : m.pqr_radps) {
            v = r.f32();
        }
        if (!r.ok() || !a_number(m.x_m) || !a_number(m.y_m) || !a_number(m.z_m)) {
            return std::nullopt;
        }
        for (const float v : m.attitude) {
            if (!a_number(v)) return std::nullopt;
        }
        for (const float v : m.uvw_mps) {
            if (!a_number(v)) return std::nullopt;
        }
        for (const float v : m.pqr_radps) {
            if (!a_number(v)) return std::nullopt;
        }
        out.yours = m;
    }
    // **The watched aircraft's controls**, after a flag of their own.
    const std::uint8_t has_watched = r.u8();
    if (!r.ok() || has_watched > 1) {
        return std::nullopt;
    }
    if (has_watched == 1) {
        Watched c;
        c.aircraft = r.u8();
        // The value that means "fixed" is not a control: nothing writes it
        // for one, and a control read from it would lie outside -1 to 1.
        bool controls = true;
        for (double* v : {&c.aileron, &c.elevator, &c.rudder, &c.throttle, &c.flaps}) {
            const auto wire = static_cast<std::int16_t>(r.u16());
            controls = controls && wire != gear_fixed;
            *v = unquantise(wire);
        }
        if (!controls) {
            return std::nullopt;
        }
        const auto gear = static_cast<std::int16_t>(r.u16());
        if (!r.ok()) {
            return std::nullopt;
        }
        if (gear != gear_fixed) {
            c.gear = unquantise(gear);
        }
        out.watched = c;
    }
    // **Anything trailing means it is not this packet.** A reader that
    // ignored the tail would take a longer thing for a shorter one.
    if (!r.done()) {
        return std::nullopt;
    }
    return out;
}

} // namespace glideslope::net
