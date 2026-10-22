#include "net/state.hpp"

#include "net/inputs.hpp"
#include "net/inside.hpp"
#include "net/protocol.hpp"

#include <cmath>
#include <cstring>

namespace glideslope::net {
namespace {

bool a_number(double v) {
    return std::isfinite(v);
}

bool a_number(float v) {
    return std::isfinite(v);
}

// A height asked that is one: a number, nought or more, or exactly
// `no_stovl_law`.
bool a_height_asked(float v) {
    return a_number(v) && (v >= 0.0F || v == no_stovl_law);
}

// A pitch integral that is one: a number, and none without a law.
bool a_pitch_integral(const OwnMotion& m) {
    return !m.stovl_pitch_integral_deg ||
           (a_number(*m.stovl_pitch_integral_deg) && m.stovl_height_asked_ft != no_stovl_law);
}

// **An own motion that is one**: every number a number, the height asked
// one, and a pitch integral only with a law.
bool own_motion_is_one(const OwnMotion& m) {
    bool numbers = a_number(m.x_m) && a_number(m.y_m) && a_number(m.z_m);
    for (const float v : m.attitude) numbers = numbers && a_number(v);
    for (const float v : m.uvw_mps) numbers = numbers && a_number(v);
    for (const float v : m.pqr_radps) numbers = numbers && a_number(v);
    return numbers && a_height_asked(m.stovl_height_asked_ft) && a_pitch_integral(m);
}

// A client's own motion as a state update carries it (and `LAW_STATE`).
void write_own_motion(Writer& w, const OwnMotion& m) {
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
    w.u16(m.steps_into_input);
    w.f32(m.stovl_height_asked_ft);
    // Its flag, and the value - nought when there is none, so that what
    // is read writes back to the same bytes.
    w.u8(m.stovl_pitch_integral_deg ? 1 : 0);
    w.f32(m.stovl_pitch_integral_deg.value_or(0.0F));
}

bool read_own_motion(Reader& r, OwnMotion& m) {
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
    m.steps_into_input = r.u16();
    m.stovl_height_asked_ft = r.f32();
    const std::uint8_t has_integral = r.u8();
    const float integral = r.f32();
    if (!r.ok() || has_integral > 1) {
        return false;
    }
    if (has_integral == 1) {
        m.stovl_pitch_integral_deg = integral;
    } else {
        // None is written as nought, every bit of it.
        std::uint32_t bits = 0;
        std::memcpy(&bits, &integral, sizeof bits);
        if (bits != 0) {
            return false;
        }
    }
    return own_motion_is_one(m);
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
            !known_condition(static_cast<std::uint8_t>(a.condition)) ||
            (a.condition == Condition::engine_stopped) != (a.stopped_engine != no_engine)) {
            return std::nullopt;
        }
    }
    if (state.yours && !own_motion_is_one(*state.yours)) {
        return std::nullopt;
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
        w.u8(a.stopped_engine);
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
        write_own_motion(w, *state.yours);
    }
    w.u8(state.watched ? 1 : 0);
    if (state.watched) {
        const Watched& c = *state.watched;
        w.u8(c.aircraft);
        for (const double v : {c.aileron, c.elevator, c.rudder, c.throttle, c.flaps}) {
            w.u16(static_cast<std::uint16_t>(quantise(v)));
        }
        w.u16(static_cast<std::uint16_t>(c.gear ? quantise(*c.gear) : gear_fixed));
        w.u16(static_cast<std::uint16_t>(c.speedbrake ? quantise(*c.speedbrake)
                                                      : no_speedbrakes));
        for (const double v : {c.pitch_trim, c.roll_trim, c.yaw_trim}) {
            w.u16(static_cast<std::uint16_t>(quantise(v)));
        }
        w.u16(static_cast<std::uint16_t>(c.held_trim ? quantise(*c.held_trim) : no_held_trim));
        for (const double v : {c.mixture, c.propeller}) {
            w.u16(static_cast<std::uint16_t>(quantise(v)));
        }
    }
    return w.take();
}

bool known_condition(std::uint8_t value) {
    return value == static_cast<std::uint8_t>(Condition::flying) ||
           value == static_cast<std::uint8_t>(Condition::wrecked) ||
           value == static_cast<std::uint8_t>(Condition::engine_stopped);
}

namespace {

// The flags byte: which of the optional parts follow, and the two flags.
constexpr std::uint8_t flag_autopilot = 0x01;
constexpr std::uint8_t flag_asked = 0x02;
constexpr std::uint8_t flag_asked_hover = 0x04;
constexpr std::uint8_t flag_lever = 0x08;
constexpr std::uint8_t flag_hover = 0x10;
constexpr std::uint8_t flag_translating = 0x20;
constexpr std::uint8_t flag_selector = 0x40;
constexpr std::uint8_t flags_known = 0x7F;

// The law's numbers, in the document's order.
std::vector<double*> law_numbers(LawState& m) {
    std::vector<double*> out{&m.height_asked_ft,    &m.heading_deg,        &m.bank_held_deg,
                             &m.hover_base_ft,      &m.speed_asked_kt,     &m.pitch_from_deg,
                             &m.speed_from_kt,      &m.back_pitch_from_deg, &m.throttle_integral,
                             &m.angle_integral_deg, &m.elevator_integral,  &m.fan_integral,
                             &m.post_integral,      &m.pitch_integral_deg};
    for (double& v : m.controls) out.push_back(&v);
    for (double& v : m.lift) out.push_back(&v);
    for (double& v : m.positions) out.push_back(&v);
    return out;
}

std::vector<double*> hover_numbers(LawState::Hovering& h) {
    return {&h.standing_ft,       &h.heading_deg,  &h.latitude_deg, &h.longitude_deg,
            &h.throttle_integral, &h.fan_integral, &h.post_integral};
}

bool law_state_is_one(const LawState& m) {
    LawState copy = m;
    bool numbers = a_number(copy.simulation_time_s) && own_motion_is_one(copy.motion) &&
                   copy.motion.stovl_height_asked_ft != no_stovl_law;
    for (double* v : law_numbers(copy)) numbers = numbers && a_number(*v);
    if (copy.lever_at_selection) numbers = numbers && a_number(*copy.lever_at_selection);
    if (copy.selector_at_take_back) {
        numbers = numbers && a_number(*copy.selector_at_take_back);
    }
    if (copy.hover) {
        for (double* v : hover_numbers(*copy.hover)) numbers = numbers && a_number(*v);
    }
    return numbers && copy.stage < law_stages &&
           copy.hover.has_value() == (copy.stage == law_stage_hovering);
}

} // namespace

std::optional<std::vector<std::uint8_t>> write_law_state(const LawState& m) {
    if (!law_state_is_one(m)) {
        return std::nullopt;
    }
    LawState copy = m;
    Writer w;
    w.u8(static_cast<std::uint8_t>(Message::law_state));
    w.u8(copy.aircraft);
    w.f64(copy.simulation_time_s);
    w.u32(copy.last_input_applied);
    write_own_motion(w, copy.motion);
    w.u8(copy.stage);
    std::uint8_t flags = 0;
    if (copy.autopilot) flags |= flag_autopilot;
    if (copy.last_asked_hover) flags |= flag_asked;
    if (copy.last_asked_hover.value_or(false)) flags |= flag_asked_hover;
    if (copy.lever_at_selection) flags |= flag_lever;
    if (copy.hover) flags |= flag_hover;
    if (copy.hover && copy.hover->translating) flags |= flag_translating;
    if (copy.selector_at_take_back) flags |= flag_selector;
    w.u8(flags);
    for (double* v : law_numbers(copy)) w.f64(*v);
    w.f64(copy.lever_at_selection.value_or(0.0));
    w.f64(copy.selector_at_take_back.value_or(0.0));
    if (copy.hover) {
        for (double* v : hover_numbers(*copy.hover)) w.f64(*v);
    }
    return w.take();
}

bool read(std::span<const std::uint8_t> body, LawState& out) {
    Reader r(body);
    if (r.u8() != static_cast<std::uint8_t>(Message::law_state)) {
        return false;
    }
    LawState got;
    got.aircraft = r.u8();
    got.simulation_time_s = r.f64();
    got.last_input_applied = r.u32();
    if (!r.ok() || !read_own_motion(r, got.motion)) {
        return false;
    }
    got.stage = r.u8();
    const std::uint8_t flags = r.u8();
    if (!r.ok() || (flags & ~flags_known) != 0 ||
        ((flags & flag_asked) == 0 && (flags & flag_asked_hover) != 0) ||
        ((flags & flag_hover) == 0 && (flags & flag_translating) != 0)) {
        return false;
    }
    got.autopilot = (flags & flag_autopilot) != 0;
    if ((flags & flag_asked) != 0) {
        got.last_asked_hover = (flags & flag_asked_hover) != 0;
    }
    for (double* v : law_numbers(got)) *v = r.f64();
    // None is written as nought, every bit of it.
    const double lever = r.f64();
    if ((flags & flag_lever) != 0) {
        got.lever_at_selection = lever;
    } else if (std::signbit(lever) || lever != 0.0) {
        return false;
    }
    const double selector = r.f64();
    if ((flags & flag_selector) != 0) {
        got.selector_at_take_back = selector;
    } else if (std::signbit(selector) || selector != 0.0) {
        return false;
    }
    if ((flags & flag_hover) != 0) {
        LawState::Hovering h;
        h.translating = (flags & flag_translating) != 0;
        for (double* v : hover_numbers(h)) *v = r.f64();
        got.hover = h;
    }
    if (!r.done() || !law_state_is_one(got)) {
        return false;
    }
    out = std::move(got);
    return true;
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
        // An engine named exactly when one is said to have stopped.
        a.stopped_engine = r.u8();
        if ((a.condition == Condition::engine_stopped) != (a.stopped_engine != no_engine)) {
            return std::nullopt;
        }
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
        if (!read_own_motion(r, m)) {
            return std::nullopt;
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
        const auto speedbrake = static_cast<std::int16_t>(r.u16());
        // The pitch, roll and yaw trim, last (since version 0F): controls,
        // never "fixed".
        for (double* v : {&c.pitch_trim, &c.roll_trim, &c.yaw_trim}) {
            const auto wire = static_cast<std::int16_t>(r.u16());
            if (!r.ok() || wire == gear_fixed) {
                return std::nullopt;
            }
            *v = unquantise(wire);
        }
        // The held trim, after them (since version 0F), or none.
        const auto held_trim = static_cast<std::int16_t>(r.u16());
        if (held_trim != no_held_trim) {
            c.held_trim = unquantise(held_trim);
        }
        // The mixture and propeller levers, last (since 0F): controls.
        for (double* v : {&c.mixture, &c.propeller}) {
            const auto wire = static_cast<std::int16_t>(r.u16());
            if (!r.ok() || wire == gear_fixed) {
                return std::nullopt;
            }
            *v = unquantise(wire);
        }
        if (gear != gear_fixed) {
            c.gear = unquantise(gear);
        }
        if (speedbrake != no_speedbrakes) {
            c.speedbrake = unquantise(speedbrake);
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
