#pragma once

#include "sim/aircraft.hpp"

namespace glideslope::sim {

// The test pilot.
//
// Just enough flying to hold the conditions a check needs: a pitch attitude, and
// above it a speed or an altitude; a bank angle; and the ball in the middle. It
// is not the product's autopilot (Phase 4) and is not meant to fly like a
// person - it is meant to hold one condition steadily so that one number can be
// read off. Each call reads the aircraft's state as it is now and advances the
// pilot's own integrals by one 120 Hz step, so call each at most once a step.
class TestPilot {
public:
    explicit TestPilot(const Aircraft& aircraft) : a_(aircraft) {}

    // Elevator for a pitch attitude, with an integral that finds the trim.
    double pitch_to(double theta_deg);

    // A pitch attitude that brings calibrated airspeed to `kcas`: nose up when
    // fast. Slow, so that it settles rather than chases the phugoid.
    double pitch_for_speed(double kcas);

    // A pitch attitude that brings the aircraft to `altitude_ft` and holds it.
    double pitch_for_altitude(double altitude_ft);

    // Aileron for a bank angle.
    double roll_to(double phi_deg) const;

    // Rudder against sideslip, with an integral that finds the rudder a steady
    // turn needs. Without the integral the aircraft slipped half a degree in a
    // 30-degree turn and turned 2% slower than its bank demanded.
    double coordinate();

    // Rudder, on the ground, for a heading: nosewheel steering and rudder are
    // one control, and without it the take-off roll wanders off the runway.
    double steer_to(double heading_deg) const;

private:
    const Aircraft& a_;
    double trim_ = 0.0;
    double speed_integral_ = 0.0;
    double altitude_integral_ = 0.0;
    double rudder_integral_ = 0.0;
};

} // namespace glideslope::sim
