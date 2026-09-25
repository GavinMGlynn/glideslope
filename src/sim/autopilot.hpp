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
//     roll rate;
//   the ball -> rudder, with an integral that finds what a turn needs;
//   altitude -> vertical speed, at most the climb rate asked for -> pitch,
//     with an integral -> elevator, damped by the pitch rate, with an
//     integral that finds the trim;
//   airspeed -> throttle, with an integral.
//
// **Asked for a height it cannot hold, it gives up height, not airspeed.**
// When the climb asked for would take the airspeed below the aeroplane's
// best-climb speed - or the speed asked for, where that is slower - the climb
// is held back so the airspeed stays there, the throttle opens to its stop,
// and the aeroplane climbs as far as it can at that speed, or comes down,
// rather than pitching up into the stall. The best-climb speed is the
// aeroplane's own, from its published figures (sim/departure.hpp's
// `departure_speeds`), in the figures directory beside the JSBSim root it was
// loaded from; an aeroplane that publishes none has no such floor.
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

#include <optional>

namespace glideslope::sim {

// The most the autopilot banks to turn, whatever it is asked.
inline constexpr double most_bank_deg = 25.0;

// What the autopilot holds.
struct AutopilotModes {
    // Degrees true; none holds the wings level.
    std::optional<double> heading_deg;
    // Feet above sea level; none holds the vertical speed instead.
    std::optional<double> altitude_ft;
    // Feet a minute: held when there is no altitude, and the rate an altitude
    // is climbed or descended to, whichever way it lies, when there is.
    double vertical_speed_fpm = 700.0;
    // Knots calibrated; none leaves the throttle where it is.
    std::optional<double> airspeed_kts;
};

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
    // The best-climb speed, knots calibrated; none where the aeroplane
    // publishes no climb speed.
    std::optional<double> best_climb_kts_;
    // Holding the speed rather than the height: the most climb the altitude
    // hold may ask for, found by an integral on the airspeed, while it binds.
    bool holding_speed_ = false;
    double climb_limit_fpm_ = 0.0;
    double last_kts_ = 0.0;
    double kts_per_s_ = 0.0; // the airspeed's trend, smoothed over a second
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
    double elevator_trim_ = 0.0;
    double rudder_integral_ = 0.0;
    double throttle_integral_ = 0.0;
    // Engaging steps nothing: the first step measures what the laws give
    // against the controls handed over, and that difference fades out.
    bool engaging_ = true;
    double fade_ = 1.0;
    double aileron_offset_ = 0.0;
    double elevator_offset_ = 0.0;
    double rudder_offset_ = 0.0;
};

} // namespace glideslope::sim
