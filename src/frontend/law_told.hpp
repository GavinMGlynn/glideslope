#pragma once

// **Her STOVL law's state as the wire carries it** (`LAW_STATE`,
// net::LawState) and as the simulation holds it (sim::Controller::LawState),
// each made from the other: the server says it to a client it gives an
// aircraft whose law flies her, and the client's prediction is put there
// (sim::Prediction::reconcile). Shared by the server and both clients so
// that the three cannot carry it differently.

#include "net/state.hpp"
#include "sim/controller.hpp"
#include "sim/transition.hpp"

#include <cstdint>

namespace glideslope::frontend {

// The controls the law last gave her that the wire carries, in its order.
inline std::array<double*, 9> told_controls(sim::Controls& c) {
    return {&c.elevator, &c.aileron,    &c.rudder,    &c.throttle, &c.flaps,
            &c.gear,     &c.pitch_trim, &c.roll_trim, &c.yaw_trim};
}
inline std::array<double*, 7> told_lift(sim::LiftSystem& l) {
    return {&l.stovl, &l.nozzle_deg, &l.fan_deg, &l.fan, &l.roll_post, &l.nozzle_yaw, &l.split};
}

inline net::LawState law_said(const sim::Controller::LawState& s, std::uint8_t aircraft,
                              double simulation_time_s, std::uint32_t last_input_applied,
                              const net::OwnMotion& motion) {
    net::LawState out;
    out.aircraft = aircraft;
    out.simulation_time_s = simulation_time_s;
    out.last_input_applied = last_input_applied;
    out.motion = motion;
    const sim::Transition::State& t = s.law;
    out.stage = static_cast<std::uint8_t>(t.stage);
    out.autopilot = t.autopilot;
    out.last_asked_hover = t.last_asked_hover;
    out.height_asked_ft = s.height_asked_ft;
    out.heading_deg = t.heading_deg;
    out.bank_held_deg = t.bank_held_deg;
    out.hover_base_ft = t.hover_base_ft;
    out.speed_asked_kt = t.speed_asked_kt;
    out.pitch_from_deg = t.pitch_from_deg;
    out.speed_from_kt = t.speed_from_kt;
    out.back_pitch_from_deg = t.back_pitch_from_deg;
    out.throttle_integral = t.throttle_integral;
    out.angle_integral_deg = t.angle_integral_deg;
    out.elevator_integral = t.elevator_integral;
    out.fan_integral = t.fan_integral;
    out.post_integral = t.post_integral;
    out.pitch_integral_deg = t.pitch_integral_deg;
    out.lever_at_selection = s.lever_at_selection;
    out.selector_at_take_back = s.selector_at_take_back;
    sim::Controls controls = t.last.controls;
    const auto c = told_controls(controls);
    for (std::size_t i = 0; i < c.size(); ++i) out.controls[i] = *c[i];
    sim::LiftSystem lift = t.last.lift;
    const auto l = told_lift(lift);
    for (std::size_t i = 0; i < l.size(); ++i) out.lift[i] = *l[i];
    out.positions = {s.positions.conversion, s.positions.nozzle_deg, s.positions.fan_deg};
    if (t.hover) {
        net::LawState::Hovering h;
        h.standing_ft = t.hover->standing_ft;
        h.heading_deg = t.hover->heading_deg;
        h.latitude_deg = t.hover->latitude_deg;
        h.longitude_deg = t.hover->longitude_deg;
        h.throttle_integral = t.hover->throttle_integral;
        h.fan_integral = t.hover->fan_integral;
        h.post_integral = t.hover->post_integral;
        h.translating = t.hover->translating;
        out.hover = h;
    }
    return out;
}

// **The law as told, for the simulation**: the controls the wire does not
// carry are left at a model's defaults, and the commands are the law's last.
inline sim::Controller::LawState law_heard(const net::LawState& m) {
    sim::Controller::LawState s;
    sim::Transition::State& t = s.law;
    t.stage = static_cast<sim::Transition::Stage>(m.stage);
    t.autopilot = m.autopilot;
    t.last_asked_hover = m.last_asked_hover;
    s.height_asked_ft = m.height_asked_ft;
    t.heading_deg = m.heading_deg;
    t.bank_held_deg = m.bank_held_deg;
    t.hover_base_ft = m.hover_base_ft;
    t.speed_asked_kt = m.speed_asked_kt;
    t.pitch_from_deg = m.pitch_from_deg;
    t.speed_from_kt = m.speed_from_kt;
    t.back_pitch_from_deg = m.back_pitch_from_deg;
    t.throttle_integral = m.throttle_integral;
    t.angle_integral_deg = m.angle_integral_deg;
    t.elevator_integral = m.elevator_integral;
    t.fan_integral = m.fan_integral;
    t.post_integral = m.post_integral;
    t.pitch_integral_deg = m.pitch_integral_deg;
    s.lever_at_selection = m.lever_at_selection;
    s.selector_at_take_back = m.selector_at_take_back;
    const auto c = told_controls(t.last.controls);
    for (std::size_t i = 0; i < c.size(); ++i) *c[i] = m.controls[i];
    const auto l = told_lift(t.last.lift);
    for (std::size_t i = 0; i < l.size(); ++i) *l[i] = m.lift[i];
    s.commands = t.last.lift;
    s.positions = {m.positions[0], m.positions[1], m.positions[2]};
    if (m.hover) {
        sim::Hover::State h;
        h.standing_ft = m.hover->standing_ft;
        h.heading_deg = m.hover->heading_deg;
        h.latitude_deg = m.hover->latitude_deg;
        h.longitude_deg = m.hover->longitude_deg;
        h.throttle_integral = m.hover->throttle_integral;
        h.fan_integral = m.hover->fan_integral;
        h.post_integral = m.hover->post_integral;
        h.translating = m.hover->translating;
        h.last = t.last;
        t.hover = h;
    }
    return s;
}

} // namespace glideslope::frontend
