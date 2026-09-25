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
constexpr double bank_rate_degps = 5.0;
// **But no more bank than the aeroplane can sustain.** A level turn needs
// its weight over the cosine of the bank in lift, and the induced drag that
// lift costs grows with its square, so a turn wants power a straight line
// does not. Near its ceiling a light aeroplane has little to spare: the
// altitude hold keeps the height by pitching up, the speed bleeds away, and
// at twenty-five degrees a Cessna 182 turning once round near its ceiling
// slowed from 82 knots to 60, onto the back of its drag curve.
//
// So **while the throttle can give no more, a turn may spend 3 knots of the
// aeroplane's energy and then no more**: from there the bank the heading may
// ask for is what holds the energy - its height and its airspeed together -
// where it is, found by an integral on how far the energy is from there and
// how fast it is going, never below 10 degrees. The energy is counted from
// when the wings leave level, so what the throttle made up before it reached
// its stop is not spent twice. When the throttle has more to give, or the
// wings come level, the limit goes back to 25 degrees at 5 degrees a second,
// so in every other flight nothing here binds.
//
// The energy is Lambregts' total energy (Vertical flight path and speed
// control autopilot design using total energy principles, AIAA 83-2239), and
// the rule is the sustained turn's: how much load factor - how much bank - an
// aeroplane holds without losing height or speed is set by the power it has
// in excess (Hurt, Aerodynamics for Naval Aviators, NAVWEPS 00-80T-80).
// Airliners' flight guidance limits bank by fixed schedules instead - the
// A320's reduced at low speed and with an engine out - and light aircraft
// autopilots by a fixed limit (22 degrees for Garmin's GFC 700); this finds
// the limit from what the aeroplane is doing, because nothing tells the
// autopilot its power.
constexpr double least_sustained_bank_deg = 10.0;
constexpr double turn_may_spend_fps = 3.0 * 1852.0 / 3600.0 / 0.3048;
constexpr double bank_per_energy_ft = 0.05;   // degrees a second, per foot
constexpr double bank_per_energy_fpm = 0.02;  // degrees a second, per ft/min
constexpr double banked_deg = 5.0;            // a turn, rather than wings level
constexpr double energy_filter_s = 1.0;
constexpr double throttle_stop = 0.999;
constexpr double g_fps2 = 32.174;
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
// **No loop moves a control faster than a pilot's hand: its full travel in a
// second, 1/120 of it in a step.** In ordinary flight none of them comes near
// this and the limit never binds. It binds where the laws are reading a fast
// moving measurement - recovering from a spiral, the rudder's term follows
// sideslip swinging tens of degrees a second - and there it once moved the
// rudder from one stop to the other, 2.0 of travel, in a single frame.
constexpr double a_hands_pace = 1.0 / static_cast<double>(steps_per_second);
// Airspeed to throttle, which moves at a quarter of its travel a second -
// slower than a hand, because an engine does not care to be slammed.
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
    const double climb_fpm = a_.property("velocities/h-dot-fps") * 60.0;
    bank_command_deg_ = a_.property("attitude/phi-deg");
    {
        const double v = a_.property("velocities/vt-fps");
        last_energy_ft_ = a_.property("position/h-sl-ft") + v * v / (2.0 * g_fps2);
        turn_energy_ft_ = last_energy_ft_;
    }
    // **Engaging steps nothing, whatever attitude it is handed.** The loop
    // starts commanding the attitude the aeroplane has, even when that is
    // outside the envelope it is allowed to ask for, and walks into the
    // envelope at its own pitch rate over the frames after. Seeding it to
    // the clamped value instead left the first frame asking for a pitch it
    // could not have and the elevator jumped by the difference: 0.21 of its
    // travel at nineteen degrees nose up, 0.80 out of a diving turn, where a
    // pilot's hand moves 0.017 in a frame.
    //
    // The integrals below are only a starting guess - what was roughly
    // holding the aeroplane. They used to carry a term cancelling the law's
    // own damping so that the first step landed exactly on the handed
    // control, and **that cancellation broke whenever the damping term was
    // larger than the integral's own limit**: a Mosquito handed over skidding
    // in its landing roll seeded a rudder integral of 9, kept 1 of it, and
    // slammed the rudder to its stop - the full travel, in one frame. The
    // first step in `fly()` now measures what the laws actually give and
    // carries the difference as an offset instead, which cannot break.
    pitch_command_deg_ = a_.property("attitude/theta-deg");
    pitch_integral_deg_ = pitch_command_deg_ + pitch_per_fpm * climb_fpm;
    elevator_trim_ = controls.elevator;
    rudder_integral_ = controls.rudder;
    throttle_integral_ = controls.throttle;
}

Controls Autopilot::fly() {
    Controls c = last_;

    // Altitude, to the vertical speed wanted.
    const double climb_fpm = a_.property("velocities/h-dot-fps") * 60.0;
    double climb_wanted = modes_.vertical_speed_fpm;
    if (modes_.altitude_ft) {
        const double rate = std::abs(modes_.vertical_speed_fpm);
        climb_wanted = std::clamp(
            fpm_per_foot * (*modes_.altitude_ft - a_.property("position/h-sl-ft")),
            -rate, rate);
    }

    // The aeroplane's energy, as the height it would have with its true
    // airspeed climbed away, and its rate, smoothed over a second.
    const double speed_fps = a_.property("velocities/vt-fps");
    const double energy_ft =
        a_.property("position/h-sl-ft") + speed_fps * speed_fps / (2.0 * g_fps2);
    energy_fpm_ += ((energy_ft - last_energy_ft_) / dt * 60.0 - energy_fpm_) * dt /
                   energy_filter_s;
    last_energy_ft_ = energy_ft;

    // Heading, through bank, to aileron.
    const double phi = a_.property("attitude/phi-deg");
    const double p = degrees(a_.property("velocities/p-rad_sec"));
    if (std::abs(bank_command_deg_) < banked_deg) {
        turn_energy_ft_ = energy_ft;
        spent_ = false;
    } else {
        // A descent asked for is energy the turn is not spending.
        turn_energy_ft_ += std::min(climb_wanted, 0.0) / 60.0 * dt;
    }
    const double above_ft =
        energy_ft - (turn_energy_ft_ - speed_fps * turn_may_spend_fps / g_fps2);
    if (last_.throttle < throttle_stop || std::abs(bank_command_deg_) < banked_deg) {
        sustained_bank_deg_ =
            toward(sustained_bank_deg_, most_bank_deg, bank_rate_degps * dt);
    } else {
        spent_ = spent_ || above_ft <= 0.0;
        if (spent_) {
            sustained_bank_deg_ = std::clamp(
                sustained_bank_deg_ +
                    (bank_per_energy_ft * above_ft + bank_per_energy_fpm * energy_fpm_) * dt,
                least_sustained_bank_deg, most_bank_deg);
        }
    }
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
                                 -sustained_bank_deg_, sustained_bank_deg_);
    }
    bank_command_deg_ = toward(bank_command_deg_, bank_wanted, bank_rate_degps * dt);
    c.aileron = aileron_per_degree * (bank_command_deg_ - phi) - aileron_per_degps * p;

    // The ball, to rudder.
    const double beta = a_.property("aero/beta-deg");
    rudder_integral_ =
        std::clamp(rudder_integral_ - rudder_integral_rate * beta * dt, -1.0, 1.0);
    c.rudder = -rudder_per_degree * beta + rudder_integral_;

    // Vertical speed, through pitch, to elevator.
    const double climb_off = climb_wanted - climb_fpm;
    // **The envelope bounds what is asked for, not where the loop starts.**
    // Clamping the command itself would snap an aeroplane handed over
    // outside the envelope straight to its edge in one frame, which is a
    // jolt; clamping the target lets the command walk there at the pitch
    // rate, which is the autopilot taking over rather than grabbing.
    const double pitch_wanted = std::clamp(
        pitch_integral_deg_ + pitch_per_fpm * climb_off, least_pitch_deg,
        most_pitch_deg);
    const double pitch_next =
        toward(pitch_command_deg_, pitch_wanted, pitch_rate_degps * dt);
    // The integral winds only while the pitch asked for is the pitch given.
    if (pitch_next == pitch_wanted) {
        pitch_integral_deg_ += pitch_integral_per_fpm * climb_off * dt;
    }
    pitch_command_deg_ = pitch_next;
    const double theta_off = pitch_command_deg_ - a_.property("attitude/theta-deg");
    const double q = degrees(a_.property("velocities/q-rad_sec"));
    elevator_trim_ = std::clamp(elevator_trim_ + trim_rate * theta_off * dt, -1.0, 1.0);
    c.elevator =
        elevator_trim_ + elevator_per_degree * theta_off - elevator_per_degps * q;

    // **What the laws differ from the controls they were handed, on the very
    // first step, is an offset that fades over two seconds.** `last_` still
    // holds those handed controls here, because it is only replaced at the
    // end of this function. Measuring the offset rather than deriving it is
    // what makes it exact: there is no term to get wrong, and no limit for it
    // to fall foul of. The throttle is not in this - its law is already rate
    // limited from `last_.throttle`, so it cannot step.
    if (engaging_) {
        engaging_ = false;
        aileron_offset_ = last_.aileron - c.aileron;
        elevator_offset_ = last_.elevator - c.elevator;
        rudder_offset_ = last_.rudder - c.rudder;
    }
    c.aileron = std::clamp(c.aileron + aileron_offset_ * fade_, -1.0, 1.0);
    c.elevator = std::clamp(c.elevator + elevator_offset_ * fade_, -1.0, 1.0);
    c.rudder = std::clamp(c.rudder + rudder_offset_ * fade_, -1.0, 1.0);
    fade_ *= std::exp(-dt / offset_fade_s);
    c.aileron = toward(last_.aileron, c.aileron, a_hands_pace);
    c.elevator = toward(last_.elevator, c.elevator, a_hands_pace);
    c.rudder = toward(last_.rudder, c.rudder, a_hands_pace);

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
