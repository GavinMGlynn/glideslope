#pragma once

// **Round again after a go-around**: the circuit flown from the climb a
// go-around ends in (sim::Lander's `go_around_ft`) back to the approach.
//
// What a pilot does (the FAA's Airplane Flying Handbook, FAA-H-8083-3C,
// chapter 9, "Go-arounds", and chapter 8's traffic pattern): the power and the
// attitude first, which the lander has done; then the configuration cleaned
// up in order - the flap to its go-around setting, half the landing flap, and,
// climbing, the gear up where it retracts - and the climb straight ahead on
// the runway's heading to a left-hand circuit at circuit height. The legs:
//
//   upwind     the runway's heading, climbing to circuit height
//   crosswind  left, square to the runway, until the downwind leg will be far
//              enough out to turn base and final from
//   downwind   the other way, at circuit height and twenty knots over the
//              reference speed, until the glidepath is as high as she is
//   base       left again, slowing to 1.4 times the landing stall
//   intercept  thirty degrees on to the final course
//
// and then she is the approach's: `on_final` says so, and the caller hands her
// to a sim::Lander on the same runway, which puts the landing flap and the
// gear where the landing needs them. The circuit's sizes are the ones the
// circuit lessons' AI flies (tests/unit/test_lesson.cpp, fly_a_circuit): a
// faster aeroplane's is higher and wider.
//
// It flies through the plain autopilot: what it gives is the modes to hold,
// and the flap and gear to set over what the autopilot flies.

#include "sim/aircraft.hpp"
#include "sim/lander.hpp"
#include "sim/plan.hpp"

namespace glideslope::sim {

class GoAroundCircuit {
public:
    enum class Leg { upwind, crosswind, downwind, base, intercept, final };

    GoAroundCircuit(const Aircraft& aircraft, const Runway& runway, const ApproachSpeeds& speeds);

    // This step's modes for the autopilot, the leg moved on first where she
    // has reached the end of hers.
    AutopilotModes modes();
    // The configuration over the autopilot's controls: the go-around flap,
    // and the gear up once she climbs, where it retracts.
    void configure(Controls& controls) const;

    Leg leg() const { return leg_; }
    bool on_final() const { return leg_ == Leg::final; }
    const Runway& runway() const { return runway_; }
    const ApproachSpeeds& speeds() const { return speeds_; }
    // Feet over the runway the circuit is flown at.
    double circuit_ft() const { return circuit_ft_; }

private:
    double along_nm() const;  // past the threshold, negative before it
    double across_m() const;  // right of the centreline

    const Aircraft& a_;
    Runway runway_;
    ApproachSpeeds speeds_;
    Leg leg_ = Leg::upwind;
    double circuit_ft_ = 1000.0;
    double turn_radius_m_ = 0.0;
    double leave_downwind_nm_ = 0.0;
    bool gear_up_ = false;
};

} // namespace glideslope::sim
