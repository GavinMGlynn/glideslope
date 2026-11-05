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
// gear where the landing needs them. A faster aeroplane's circuit is higher
// and wider.
//
// **The circuit lessons' AI flies this circuit too** (tests/unit/
// test_lesson.cpp, fly_a_circuit), joined from a take-off rather than a
// go-around - one set of rules, two ways in (`CircuitEntry`): from the
// take-off on to crosswind at the take-off's turn height, climbing at its
// climb speed and rate with the take-off flap out. **It is flown that high over the
// highest ground of the circuit**, where the ground rises above the runway,
// as her terrain has it.
//
// **Number two follows** (`follow`): two or more going round to one runway
// are sequenced as a tower sequences them (FAA JO 7110.65, 3-8-1: "EXTEND
// DOWNWIND", "NUMBER (sequence), FOLLOW (traffic)"), in the order they went
// around by the caller's clock (`circuit_ahead_alt_ft`). One with traffic
// ahead of her:
//   - climbs on her upwind leg `following_offset_m` right of the
//     centreline, the side a pilot going around for another aeroplane
//     offsets to (sourced; the distance is the 1.5 nm minimum), and turns
//     crosswind only within 100 ft of her raised height;
//   - flies her circuit `above_ahead_ft` over the highest the one ahead has
//     flown hers - derived, not sourced: the separation minimum's 500 ft
//     (JO 7110.65, 7-9-4; sim/separation.hpp) and the monitor's 200 in hand;
//   - flies on down her downwind leg, not turning base, until everything
//     ahead has touched down (JO 7110.65, 3-10-3: no arrival crosses the
//     threshold until the one before has landed). The extension is the
//     tower's; waiting for the touch, and its limit, `longest_extension_nm`,
//     are chosen, not sourced. Past it she flies a racetrack back to her
//     base point at her raised height.
// The runway's own rule (sim/vacate.hpp) sends her round again if the one
// ahead is still on it. So she is apart from the one ahead on every leg
// until it is down, and only then comes down through its height. Anything
// else - an aircraft on the approach straight in, a departure - is left to
// the separation monitor and the runway's rule.
//
// It flies through the plain autopilot: what it gives is the modes to hold,
// and the flap and gear to set over what the autopilot flies.

#include "sim/aircraft.hpp"
#include "sim/lander.hpp"
#include "sim/plan.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace glideslope::sim {

// Where she is from a runway's threshold, in its own frame: along the
// landing direction in nautical miles (negative before the threshold), and
// right of the centreline in metres.
double along_runway_nm(const Runway& runway, double latitude_deg, double longitude_deg);
double across_runway_m(const Runway& runway, double latitude_deg, double longitude_deg);

// **How she comes to the circuit.** From a go-around (the default): on the
// upwind leg, climbing at twenty knots over the reference speed at about ten
// feet a minute a knot of it, with half the landing flap. From a take-off:
// on to the crosswind leg, at the climb speed and rate and the flap given.
struct CircuitEntry {
    bool from_take_off = false;
    std::optional<double> climb_kts;  // none: twenty knots over the reference
    std::optional<double> climb_fpm;  // none: ten a knot of the reference, 500 to 1,500
    std::optional<double> flaps;      // none: half the landing flap
};

// **Where she is in a circuit's sequence**: the runway she is going round
// to, when she joined it (sim time, s: her first go-around, kept through
// another), how far down the runway she was then (nm past the threshold),
// and the height her circuit is flown at, above sea level, ft - or the
// highest she has flown it, where she overshot it. Held from the go-around
// until she touches down.
struct CirclePlace {
    Runway runway;
    double since_s = 0.0;
    double along_nm = 0.0;
    double circuit_alt_ft = 0.0;
};

// Whether two runways are one: the same name and threshold.
bool same_runway(const Runway& a, const Runway& b);

// **The traffic `self` follows**, of every aircraft's place in order (none
// for one not in a circuit): the highest circuit height of those going round
// to her runway ahead of her - joined earlier; at the same time, further
// down the runway; and level with her there too, before her in the order -
// or none, she is number one.
std::optional<double> circuit_ahead_alt_ft(std::size_t self,
                                           const std::vector<std::optional<CirclePlace>>& all);

class GoAroundCircuit {
public:
    // **How far above the circuit ahead she flies hers**: the separation
    // minimum, 500 ft, and the 200 the monitor keeps in hand.
    static constexpr double above_ahead_ft = 700.0;

    // turn_out, outbound and turn_in: the racetrack flown from a downwind
    // leg extended as far as it goes (`longest_extension_nm`).
    enum class Leg { upwind, crosswind, downwind, base, intercept, final, turn_out, outbound, turn_in };

    // **The furthest she extends her downwind leg past where she would turn
    // base**, following traffic: chosen, not sourced. No published figure
    // was found; a pilot's account of a tower's "extend downwind" runs to
    // three or four miles, and a tower that wants more asks for a 360 or a
    // go-around. Past it she flies a racetrack back to where she would have
    // turned base, outside her own circuit and at her raised height.
    static constexpr double longest_extension_nm = 2.0;
    // **How far right of the centreline she flies her upwind leg,
    // following**: the side away from the circuit, where a pilot going
    // around for another aeroplane offsets (the Airplane Flying Handbook,
    // FAA-H-8083-3, 8-11 to 8-13, as flight-training checklists cite it).
    // The distance is the separation minimum's 1.5 nm (sim/separation.hpp):
    // every leg of a left-hand circuit ahead lies on or left of the
    // centreline, so on her upwind leg she is never nearer it than that.
    static constexpr double following_offset_m = 2778.0;

    GoAroundCircuit(const Aircraft& aircraft, const Runway& runway, const ApproachSpeeds& speeds,
                    const CircuitEntry& entry = {});

    // This step's modes for the autopilot, the leg moved on first where she
    // has reached the end of hers.
    AutopilotModes modes();
    // The configuration over the autopilot's controls: the go-around flap,
    // and the gear up once she climbs, where it retracts.
    void configure(Controls& controls) const;

    // **Traffic ahead in the sequence**, told every step: the height of the
    // highest circuit ahead of her, above sea level, or none. With traffic
    // ahead her circuit is raised to `above_ahead_ft` over it (and stays so)
    // and she flies on down the downwind leg; with none, she turns base where
    // her circuit has her.
    void follow(std::optional<double> ahead_alt_ft);
    bool following() const { return following_; }
    // How many racetracks she has flown from a downwind extended its
    // furthest.
    int racetracks() const { return racetracks_; }

    Leg leg() const { return leg_; }
    bool on_final() const { return leg_ == Leg::final; }
    const Runway& runway() const { return runway_; }
    const ApproachSpeeds& speeds() const { return speeds_; }
    // Feet over the runway the circuit is flown at: over the highest ground
    // of the circuit, where that rises above the runway.
    double circuit_ft() const { return circuit_ft_; }
    // How far before the threshold, nm, she turns base from downwind.
    double leave_downwind_nm() const { return leave_downwind_nm_; }

private:
    double along_nm() const;  // past the threshold, negative before it
    double across_m() const;  // right of the centreline
    // **The speed base and the turn on to final are flown at**: 1.4 times
    // the landing stall, which is the approach speed's 1.3 and a little -
    // or, where the approach is a manual's and faster than 1.3 times the
    // stall (the F-35B's, 2026-10-30), the same margin over it, so that
    // final is never joined slower than the approach is flown.
    double base_kts() const;


    const Aircraft& a_;
    Runway runway_;
    ApproachSpeeds speeds_;
    CircuitEntry entry_;
    Leg leg_ = Leg::upwind;
    double circuit_ft_ = 1000.0;
    double turn_radius_m_ = 0.0;
    double leave_downwind_nm_ = 0.0;
    bool gear_up_ = false;
    bool following_ = false;
    double downwind_m_ = 0.0; // across, where the racetrack left downwind
    int racetracks_ = 0;
};

} // namespace glideslope::sim
