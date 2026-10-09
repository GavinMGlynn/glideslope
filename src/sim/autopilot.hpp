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
//   the ball -> rudder, with an integral that finds what a turn needs, and
//     a yaw damper: the yaw rate, washed out, -> rudder;
//   altitude -> vertical speed, at most the climb rate asked for -> pitch,
//     with an integral -> elevator, damped by the pitch rate, with an
//     integral that finds the trim;
//   airspeed -> throttle, with an integral;
//   and, where the aircraft has a mixture lever, the mixture leaned for best
//     power by the engine's answer (sim/leaner.hpp);
//
// or, when asked to recover from a stall, the throttle to its stop and the
// sink a speed short allows -> pitch, the wing held below its stall and below
// the angle that pulls 1.6 g, and the flaps taken up at a hand's pace to the
// go-around setting where the aeroplane's figures give one, until the caller
// lets it go.
//
// **Handed an upset - banked past 45 degrees - it rolls level before it
// pulls**: no turn is asked until the wings are level, the bank asked comes
// back three times a turn's rate, the nose is not raised and the elevator's
// trim is not wound until then (autopilot.cpp, `upset_bank_deg`).
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

    // **A floor and a ceiling on the height it flies to**, kept apart from
    // the modes (sim/separation.hpp): the altitude asked for is flown only
    // between them, and with none - a vertical speed held - it climbs no
    // higher than the ceiling and descends no lower than the floor. Nothing
    // else changes: the rate it climbs or descends at is the one asked for.
    // Either may be none, and setting them again replaces both.
    void limit_height(std::optional<double> floor_ft, std::optional<double> ceiling_ft) {
        floor_ft_ = floor_ft;
        ceiling_ft_ = ceiling_ft;
    }
    // **A heading to turn to instead of the one asked** (sim/separation.hpp:
    // turned away from two it is squeezed between), kept apart from the
    // modes as the height's limits are; none flies the modes' heading. The
    // turn is the heading loop's own - its bank, and the rate it rolls at.
    // Released, the heading loop's integral - found on the away heading's
    // turn - is let go, so the plan's heading is flown afresh.
    void turn_away(std::optional<double> heading_deg) {
        if (away_deg_ && !heading_deg) {
            bank_integral_deg_ = 0.0;
        }
        away_deg_ = heading_deg;
    }
    // The pitch it asks for, degrees, and the elevator's trim it has found:
    // for tests of the upset rule.
    double pitch_asked_deg() const { return pitch_command_deg_; }
    double elevator_trim() const { return elevator_trim_; }
    // How many steps the elevator's trim has been held from winding nose-up
    // because the wing was seen to stall (autopilot.cpp): for tests.
    long trim_held_steps() const { return trim_held_steps_; }
    // How many times the speed floor has taken over before the throttle
    // was at its stop (`early_hold_`), since it was made.
    long early_holds() const { return early_holds_; }
    // **The fastest it may hold**: the most the speed asked may be raised
    // to for a climb the nose cannot give (autopilot.cpp: with the nose at
    // its highest and the climb short), the aircraft's fastest a plan may
    // ask (sim::plan_speeds). None: never raised. No model here gives a
    // flap or gear speed, so this is the only bound beside 40 kt.
    void limit_speed(std::optional<double> fastest_kts) { fastest_kts_ = fastest_kts; }
    // **Handed over climbing `climb_fpm`**: a climb asked that is less is
    // eased down to from it, not stepped to (autopilot.cpp,
    // `ease_climb_fpm_per_s`). Once the climb asked is reached, or is more,
    // it is flown as asked.
    void ease_climb(double climb_fpm) { eased_climb_fpm_ = climb_fpm; }
    // How far the speed asked has been raised for a climb now, knots.
    double climb_speed_kts() const { return climb_speed_kts_; }
    // The height it is flying to, within its limits, or none when it holds
    // a vertical speed.
    std::optional<double> height_flown_to_ft() const;

    // The controls for the next step, from the aircraft's state now. Each call
    // advances the loops by one 120 Hz step, so call it once a step.
    Controls fly();

private:
    const Aircraft& a_;
    AutopilotModes modes_;
    std::optional<double> floor_ft_;
    std::optional<double> ceiling_ft_;
    std::optional<double> fastest_kts_;
    std::optional<double> away_deg_;
    std::optional<double> eased_climb_fpm_;
    bool seed_climb_ = true; // the climb loop's integral, seeded on the first step
    Controls last_;
    // Holding the speed rather than the height: the most climb the altitude
    // hold may ask for, found by an integral on the airspeed, while it binds.
    bool holding_speed_ = false;
    // Engaged before the throttle reached its stop, because it could not in
    // time: the climb held where it was until the speed is near the least.
    bool early_hold_ = false;
    long early_holds_ = 0;
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
    // The nose at the envelope's highest pitch and the climb short of what
    // is asked; and the knots the speed asked has risen by for it.
    bool nose_at_stop_ = false;
    double climb_speed_kts_ = 0.0;
    // The airspeed on the elevator (AutopilotModes::speed_on_elevator): the
    // pitch it holds the sink it allows at, found by an integral; and the
    // sink it has added while the speed came too slowly.
    bool was_on_speed_ = false;
    double speed_integral_deg_ = 0.0;
    double sink_integral_fpm_ = 0.0;
    // A stall's entry has flown past the angle its lift peaked at.
    bool past_the_peak_ = false;
    // The wing seen going over its lift's peak, and the trends that show it.
    bool seen_to_stall_ = false;
    double alpha_trend_ = 0.0;
    double lift_trend_ = 0.0;
    double last_alpha_ = 0.0;
    double last_lift_ = 0.0;
    long trim_held_steps_ = 0;
    // Where the wing's lift has been seen to peak in this configuration: the
    // greatest lift coefficient, the angle of attack it came at, and the
    // flaps and gear it was seen with.
    double most_lift_ = -1e9;
    double stall_alpha_deg_ = 90.0;
    double lift_config_ = -1e9;
    double elevator_trim_ = 0.0;
    double aileron_trim_ = 0.0;
    double rudder_integral_ = 0.0;
    // The yaw rate's slow part, which the yaw damper washes out (degrees a
    // second).
    double steady_yaw_rate_degps_ = 0.0;
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

} // namespace glideslope::sim
