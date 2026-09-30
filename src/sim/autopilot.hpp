#pragma once

// The autopilot: holds a heading, an altitude, an airspeed and a vertical
// speed, through the controls a pilot moves - aileron, elevator, rudder and
// throttle - as a light aircraft's autopilot does (REQUIREMENTS.md, section 5,
// the first layer).
//
// **Loops within loops**, each on the aircraft's state as its instruments
// show it:
//
//   heading -> bank, 25 degrees at most - and no more than the aeroplane can
//     sustain, when its throttle has no more to give - with an integral near
//     the heading that finds the bank it needs held -> aileron, damped by the
//     roll rate, with an integral that finds the aileron the bank needs held;
//   the ball -> rudder, with an integral that finds what a turn needs;
//   altitude -> vertical speed, at most the climb rate asked for -> pitch,
//     with an integral -> elevator, damped by the pitch rate, with an
//     integral that finds the trim;
//   airspeed -> throttle, with an integral;
//   and, where the aircraft has a mixture lever, the mixture leaned for best
//     power by the engine's answer (sim/leaner.hpp);
//
// or, when asked to recover from a stall, airspeed -> pitch, with an
// integral, and the throttle to its stop, until the caller lets it go.
//
// **Asked for a height it cannot hold, it gives up height, not airspeed.**
// When the climb asked for would take the airspeed below the aeroplane's
// best-climb speed - or the speed asked for, where that is slower - the climb
// is held back so the airspeed stays there, the throttle opens to its stop,
// and the aeroplane climbs as far as it can at that speed, or comes down,
// rather than pitching up into the stall. Only for a light aeroplane, flaps
// and gear up: the best-climb speed is its own published one, read when the
// model loads (sim::Aircraft's `climb_floor_kts`).
//
// **Engaging it steps nothing.** It starts from the controls the aircraft has
// and the attitude it is in: each integral is set so the first controls it
// gives are those, and the bank, pitch and throttle it asks for move from
// where they are at a rate a pilot would - so taking the aircraft from a pilot
// jolts neither it nor them.
//
// It is not the test pilot (sim/test_pilot.hpp), which holds one condition
// steadily so a check can read one number off. This flies.

#include "sim/aircraft.hpp"
#include "sim/leaner.hpp"
#include "sim/plan.hpp"

#include <optional>

namespace glideslope::sim {

class Autopilot {
public:
    // Engaged on `aircraft` as it is now, flying with `controls`.
    Autopilot(const Aircraft& aircraft, const Controls& controls);

    void set(const AutopilotModes& modes) {
        modes_ = modes;
    }
    const AutopilotModes& modes() const {
        return modes_;
    }

    // The controls for the next step, from the aircraft's state now. Each call
    // advances the loops by one 120 Hz step, so call it once a step.
    Controls fly();

private:
    const Aircraft& a_;
    AutopilotModes modes_;
    Controls last_;
    // Holding the speed rather than the height: the most climb the altitude
    // hold may ask for, found by an integral on the airspeed, while it binds.
    bool holding_speed_ = false;
    double climb_limit_fpm_ = 0.0;
    double last_kts_ = 0.0;
    double kts_per_s_ = 0.0; // the airspeed's trend, smoothed over a second
    double turn_allowance_kts_ = 0.0; // what a turn may spend, until it is back
    double bank_command_deg_ = 0.0;
    double bank_integral_deg_ = 0.0;
    // The most bank the aeroplane sustains, as its energy says: the energy,
    // in feet, at the start of the turn and a step ago, its rate, and whether
    // the turn has spent what it may.
    double sustained_bank_deg_ = 25.0;
    double turn_energy_ft_ = 0.0;
    double last_energy_ft_ = 0.0;
    double energy_fpm_ = 0.0;
    bool spent_ = false;
    double pitch_command_deg_ = 0.0;
    double pitch_integral_deg_ = 0.0;
    // The airspeed on the elevator (AutopilotModes::speed_on_elevator): the
    // pitch it holds the speed at, found by an integral.
    bool was_on_speed_ = false;
    double speed_integral_deg_ = 0.0;
    // Where the wing's lift has been seen to peak in this configuration: the
    // greatest lift coefficient, the angle of attack it came at, and the
    // flaps and gear it was seen with.
    double most_lift_ = -1e9;
    double stall_alpha_deg_ = 90.0;
    double lift_config_ = -1e9;
    double elevator_trim_ = 0.0;
    double aileron_trim_ = 0.0;
    double rudder_integral_ = 0.0;
    double throttle_integral_ = 0.0;
    // Engaging steps nothing: the first step measures what the laws give
    // against the controls handed over, and that difference fades out.
    bool engaging_ = true;
    double fade_ = 1.0;
    double aileron_offset_ = 0.0;
    double elevator_offset_ = 0.0;
    double rudder_offset_ = 0.0;
    // The mixture, leaned for best power, where the aircraft has a lever to.
    std::optional<MixtureLeaner> leaner_;
};

// **How far ahead of the aircraft a heading must be held for the autopilot to
// bank `bank_deg`** and hold it there, turning: in a steady turn its integral
// near the heading winds to its most, 5 degrees, and a degree of bank for each
// degree off gives the rest. The sign is the bank's. For the navigator, which
// flies an orbit by keeping the heading it asks for this far ahead. **It
// assumes the integral at its limit**, which it reaches only while the
// heading is within 10 degrees of it - so for a bank below about 15 degrees.
// Beyond that the integral winds no further, and the bank falls short by
// what it lacks of its 5 degrees. Every tightest orbit a plan allows wants
// about 11.5 degrees.
double heading_off_for_bank_deg(double bank_deg);

} // namespace glideslope::sim
