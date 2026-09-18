#include "sim/autopilot.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace glideslope::sim {

namespace {

constexpr double dt = 1.0 / static_cast<double>(steps_per_second);

// Heading to bank: a degree of bank for each degree off, 25 at most, moving
// at 5 degrees a second.
constexpr double bank_per_degree = 1.0;
constexpr double most_bank_deg = 25.0;
constexpr double bank_rate_degps = 5.0;
// And the bank a heading needs held - against the propeller's slipstream and
// torque - found by an integral within 10 degrees of it, 5 degrees at most.
constexpr double bank_integral_per_degree = 0.1;
constexpr double bank_integral_within_deg = 10.0;
constexpr double most_bank_integral_deg = 5.0;
// Bank to aileron, and the roll rate's damping; an aileron offset at engaging,
// fading over two seconds.
constexpr double aileron_per_degree = 0.04;
constexpr double aileron_per_degps = 0.02;
constexpr double offset_fade_s = 2.0;
// The ball to rudder.
constexpr double rudder_per_degree = 0.1;
constexpr double rudder_integral_rate = 0.05;
// Altitude to vertical speed: 3 ft/min for each foot off.
constexpr double fpm_per_foot = 3.0;
// Vertical speed to pitch, which moves at 3 degrees a second at most, within
// -10 and 15 degrees.
constexpr double pitch_per_fpm = 0.004;
constexpr double pitch_integral_per_fpm = 0.002;
constexpr double pitch_rate_degps = 3.0;
constexpr double least_pitch_deg = -10.0;
constexpr double most_pitch_deg = 15.0;
// Pitch to elevator, the pitch rate's damping, and the trim the integral finds.
constexpr double elevator_per_degree = 0.05;
constexpr double elevator_per_degps = 0.03;
constexpr double trim_rate = 0.02;
// Airspeed to throttle, which moves at a quarter of its travel a second.
constexpr double throttle_per_knot = 0.08;
constexpr double throttle_integral_per_knot = 0.02;
constexpr double throttle_rate = 0.25;

double degrees(double radians) {
    return radians * 180.0 / std::numbers::pi;
}

double toward(double from, double to, double most) {
    return from + std::clamp(to - from, -most, most);
}

} // namespace

Autopilot::Autopilot(const Aircraft& aircraft, const Controls& controls)
    : a_(aircraft), last_(controls) {
    // Holding what the aircraft is doing now.
    modes_.heading_deg = a_.property("attitude/psi-deg");
    modes_.altitude_ft = a_.property("position/h-sl-ft");
    modes_.airspeed_kts = a_.property("velocities/vc-kts");
    // Each loop set to give the controls the aircraft has.
    const double p = degrees(a_.property("velocities/p-rad_sec"));
    const double q = degrees(a_.property("velocities/q-rad_sec"));
    const double climb_fpm = a_.property("velocities/h-dot-fps") * 60.0;
    bank_command_deg_ = a_.property("attitude/phi-deg");
    aileron_offset_ = controls.aileron + aileron_per_degps * p;
    pitch_command_deg_ = a_.property("attitude/theta-deg");
    pitch_integral_deg_ = pitch_command_deg_ + pitch_per_fpm * climb_fpm;
    elevator_trim_ = controls.elevator + elevator_per_degps * q;
    rudder_integral_ =
        controls.rudder + rudder_per_degree * a_.property("aero/beta-deg");
    throttle_integral_ = controls.throttle;
}

Controls Autopilot::fly() {
    Controls c = last_;

    // Heading, through bank, to aileron.
    const double phi = a_.property("attitude/phi-deg");
    const double p = degrees(a_.property("velocities/p-rad_sec"));
    double bank_wanted = 0.0;
    if (modes_.heading_deg) {
        const double off = std::remainder(
            *modes_.heading_deg - a_.property("attitude/psi-deg"), 360.0);
        if (std::abs(off) < bank_integral_within_deg) {
            bank_integral_deg_ =
                std::clamp(bank_integral_deg_ + bank_integral_per_degree * off * dt,
                           -most_bank_integral_deg, most_bank_integral_deg);
        }
        bank_wanted = std::clamp(bank_per_degree * off + bank_integral_deg_,
                                 -most_bank_deg, most_bank_deg);
    }
    bank_command_deg_ = toward(bank_command_deg_, bank_wanted, bank_rate_degps * dt);
    aileron_offset_ *= std::exp(-dt / offset_fade_s);
    c.aileron = std::clamp(aileron_per_degree * (bank_command_deg_ - phi) -
                               aileron_per_degps * p + aileron_offset_,
                           -1.0, 1.0);

    // The ball, to rudder.
    const double beta = a_.property("aero/beta-deg");
    rudder_integral_ =
        std::clamp(rudder_integral_ - rudder_integral_rate * beta * dt, -1.0, 1.0);
    c.rudder = std::clamp(-rudder_per_degree * beta + rudder_integral_, -1.0, 1.0);

    // Altitude, through vertical speed and pitch, to elevator.
    const double climb_fpm = a_.property("velocities/h-dot-fps") * 60.0;
    double climb_wanted = modes_.vertical_speed_fpm;
    if (modes_.altitude_ft) {
        const double rate = std::abs(modes_.vertical_speed_fpm);
        climb_wanted = std::clamp(
            fpm_per_foot * (*modes_.altitude_ft - a_.property("position/h-sl-ft")),
            -rate, rate);
    }
    const double climb_off = climb_wanted - climb_fpm;
    const double pitch_wanted = pitch_integral_deg_ + pitch_per_fpm * climb_off;
    const double pitch_next =
        std::clamp(toward(pitch_command_deg_, pitch_wanted, pitch_rate_degps * dt),
                   least_pitch_deg, most_pitch_deg);
    // The integral winds only while the pitch asked for is the pitch given.
    if (pitch_next == pitch_wanted) {
        pitch_integral_deg_ += pitch_integral_per_fpm * climb_off * dt;
    }
    pitch_command_deg_ = pitch_next;
    const double theta_off = pitch_command_deg_ - a_.property("attitude/theta-deg");
    const double q = degrees(a_.property("velocities/q-rad_sec"));
    elevator_trim_ = std::clamp(elevator_trim_ + trim_rate * theta_off * dt, -1.0, 1.0);
    c.elevator = std::clamp(elevator_trim_ + elevator_per_degree * theta_off -
                                elevator_per_degps * q,
                            -1.0, 1.0);

    // Airspeed, to throttle.
    if (modes_.airspeed_kts) {
        const double speed_off =
            *modes_.airspeed_kts - a_.property("velocities/vc-kts");
        const double wanted = throttle_integral_ + throttle_per_knot * speed_off;
        const double next =
            std::clamp(toward(last_.throttle, wanted, throttle_rate * dt), 0.0, 1.0);
        if (next == wanted) {
            throttle_integral_ += throttle_integral_per_knot * speed_off * dt;
        }
        c.throttle = next;
    }

    last_ = c;
    return c;
}

} // namespace glideslope::sim
