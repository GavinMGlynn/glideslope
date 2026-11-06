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
// until it is down, and only then comes down through its height.
//
// **An aircraft on a straight-in approach lands first, and a departure
// climbing out is given way to** (`follow`'s `straight_in`): "aircraft,
// while on final approach to land or while landing, have the right-of-way
// over other aircraft in flight" (14 CFR 91.113(g)), and a tower sequences
// the rest round them (JO 7110.65, 3-8-1). One in the circuit who has not
// yet turned base, with a straight-in on the approach to her runway or a
// departure climbing out from it, follows it as she follows a circuit
// ahead - offset right on her upwind leg, and down her downwind leg until
// it is down or climbed out. Turned base, she is committed, and it is the
// one behind.
//
// **Traffic close ahead on an approach sends her round early**, whoever she
// is (the Airplane Flying Handbook, chapter 8: with a conflict on final "an
// early go-around may be in order"; `close_ahead_on_approach`) - a
// straight-in closing on one going around ahead of her, or on one turned
// final from the circuit - and she joins the circuit's sequence behind it.
//
// **A departure is held** while one going round is still over the runway
// or climbing out ahead of it (`departure_held`), and released once she is
// on her downwind leg the minimum short of the threshold.
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
    // **Clear of a departure**: on her downwind leg (or past it) and 1.5 nm
    // or more short of the threshold - so that a departure climbing out
    // ahead of the threshold goes away from her, never nearer than the
    // minimum along the runway.
    bool clear_of_departures = false;
    // **Flown by her pilot**, a person, this step: she cannot be told to
    // land, and the one behind her may leave the circuit rather than hold
    // for her for ever (Controller::leaves_with, `behind_a_person`).
    bool by_hand = false;
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

// **Whether any of those ahead of `self` in her runway's sequence** - as
// `circuit_ahead_alt_ft` orders them - is flown by a person (`by_hand`).
bool behind_a_person(std::size_t self, const std::vector<std::optional<CirclePlace>>& all);

// **A person's aircraft, judged by where she is and how she flies**: what
// her pilot means is not the AI's to know, and a person is never told what
// to do. A tower has only what it sees too.
//
// **Straight in** (`straight_in_by_hand`): on final approach - "a flight
// path of a landing aircraft in the direction of landing along the
// extended runway centerline ... straight in to the runway" (the
// Pilot/Controller Glossary, final approach - VFR) - which 14 CFR
// 91.113(g) gives the right of way: in the air, higher over the runway
// than RunwayClear::occupied_ft; before the threshold and no further out
// than `by_hand_final_nm`; within the 1.5 nm minimum of the extended
// centreline; heading within `by_hand_heading_deg` of the runway's; and no
// higher than a three-degree glidepath and `close_ft`. Chosen, not sourced:
// the reach, the heading and the height.
bool straight_in_by_hand(const Runway& runway, const AircraftState& her);
// **Departing** (`departing_by_hand`): rolling down the runway in its
// direction faster than `by_hand_rolling_kts`, or in the air - higher over
// it than RunwayClear::occupied_ft - past the threshold and within
// `by_hand_departure_nm` past the runway's far end,
// within the minimum of its centreline and heading within
// `by_hand_heading_deg` of it - a take-off, and also a low approach past
// the threshold, which JO 7110.65 3-8-2 calls a departing aircraft from
// there. She is departing until she turns off the runway's heading - AC
// 90-66C and AIM 4-3-2: straight out, or a turn once beyond the departure
// end - or is that far past its end.
bool departing_by_hand(const Runway& runway, const AircraftState& her);
// **Clear of a departure**, a person in the circuit: the 1.5 nm minimum
// short of the threshold, as an AI's place is marked
// (CirclePlace::clear_of_departures), and not closing on it - heading 90
// degrees or more off the bearing to it - whatever her pattern's shape:
// either hand, wide, or an orbit, so long as she is behind the threshold.
// A departure climbing out ahead of the threshold then goes away from her.
// Judged afresh every step: turned towards the runway, she is not. Over or
// beyond the runway she never is: one orbiting over the climb-out holds
// the departure, as a tower would not release one into her.
bool clear_of_departures_by_hand(const Runway& runway, const AircraftState& her);
// **Gone from the circuit** (`left_the_circuit_by_hand`), a person's
// aircraft with a place in it: further than `left_the_circuit_nm` from the
// runway's threshold, horizontally - whatever her height and heading. Her
// place is dropped (Controller::fly), and the one behind her, holding
// outside, comes back to land. A tower's surface area is about 4 nm round
// (AIM 3-2-5: Class D, "usually" 4 nm; 5 statute miles in practice), and
// outside it a pilot need not ask to leave the tower's frequency (AIM
// 4-3-2): the tower has no more say in her. But a circuit is flown further
// out than that - 6.5 nm the furthest measured, a jet's downwind extended -
// and the one behind holds outside as far as 15 nm (HoldOutside, the
// furthest a test allows); so the reach is that and the 1.5 nm minimum,
// and a little: 17 nm, chosen, not sourced. Nearer, a person flying away
// down the approach passed the hold as the one behind set off down from
// it - at 8 nm two F-15Cs were 2,673 m and 474 ft apart, and at 10 nm a
// person's F-15C and a B-2A behind her 1,304 m and 464 ft.
bool left_the_circuit_by_hand(const Runway& runway, const AircraftState& her);
constexpr double left_the_circuit_nm = 17.0;
constexpr double by_hand_final_nm = 5.0;
constexpr double by_hand_departure_nm = 1.5;
constexpr double by_hand_heading_deg = 30.0;
constexpr double by_hand_rolling_kts = 30.0;

// **Whether a departure from `runway` is held** for the circuit's sequence:
// while anything going round to it is not yet clear of departures
// (CirclePlace::clear_of_departures). A tower holds a departure until the
// one going around - a departing aircraft from the moment she crosses the
// threshold (JO 7110.65, 3-8-2) - "has departed and crossed the runway end
// or turned to avert any conflict" (3-9-6); which turn is clear of one
// climbing out after her is chosen: her downwind leg, the 1.5 nm minimum
// short of the threshold.
bool departure_held(const Runway& runway, const std::vector<std::optional<CirclePlace>>& all);

// **Whether `other` is close ahead of `me` on her approach to `runway`**,
// close enough that she goes around early: further along the landing
// direction than she is, within the 1.5 nm minimum of the extended
// centreline, no higher than she is and within `close_ft` below her - a
// go-around climbs away from what is below her, and into what is above -
// and within `close_m` of her -
// and in the air, higher over the runway than RunwayClear::occupied_ft:
// on the runway it is the runway's rule (sim/vacate.hpp), and stopped
// beside it, it is not traffic.
// `close_ft` is the vertical minimum (sim/separation.hpp) and the 200 ft
// the monitor keeps in hand; `close_m` the horizontal minimum and the
// greater of half a mile and a minute's closing on the one ahead - a
// minute, because her go-around climbs at 500 ft/min or more (the circuit's
// least), so that she has the vertical minimum before she could have
// closed to the horizontal one. Derived, not sourced.
bool close_ahead_on_approach(const Runway& runway, const AircraftState& me,
                             const AircraftState& other);
double close_m(const Runway& runway, const AircraftState& me, const AircraftState& other);
constexpr double close_ft = 500.0 + 200.0;
constexpr double close_in_hand_m = 926.0;
constexpr double close_lead_s = 60.0;

// **The reciprocal of a runway**: the same pavement landed the other way -
// its threshold at the far end, its heading turned round.
Runway reciprocal(const Runway& runway);
// **Whether a person is using `runway` or its reciprocal**, straight in or
// departing (`straight_in_by_hand`, `departing_by_hand`): one taking off
// the other way is in the circuit's way as much as one taking off with it.
bool using_by_hand(const Runway& runway, const AircraftState& her);
// **A person climbing out** (`climbing_out_by_hand`): departing `runway`
// or its reciprocal (`departing_by_hand`), or on out in the air along its
// heading, within the minimum of its centreline, to `over_departure_nm`
// past its far end. A circuit going round to it does not turn crosswind
// across her climb while she is (GoAroundCircuit::follow): turned once she
// was past `by_hand_departure_nm`, a 747-400's crosswind leg crossed a
// C182's climb 440 ft over her, 797 m off. Chosen.
bool climbing_out_by_hand(const Runway& runway, const AircraftState& her);
constexpr double over_departure_nm = 5.0;
// **A person ahead of an approach, to break off for**
// (`ahead_of_an_approach_by_hand`): climbing out from `runway` or its
// reciprocal (`climbing_out_by_hand`), or lined up on it - on the runway within `lined_up_m` of a threshold,
// heading within `by_hand_heading_deg` of the runway's - however fast.
// Waiting for her roll to reach `by_hand_rolling_kts`, an F-15C 2 nm out
// broke off only at 1.2 nm and 480 ft, as a Cub lifted off 2,375 m ahead.
// Lined up further down the runway, she is landed and rolling out, and the
// runway's rule (sim/vacate.hpp) is the one that applies. Chosen.
bool ahead_of_an_approach_by_hand(const Runway& runway, const AircraftState& her);
constexpr double lined_up_m = 300.0;

// **Holding outside the circuit**, behind a person who stays in it
// (Controller::leaves_with): a tower's "remain outside the Class D" (AIM
// 3-2-5) flown as a standard holding pattern (AIM 5-3-8): right turns,
// one-minute legs at or below 14,000 ft, no faster than 200 kt at or below
// 6,000 ft.
//   - The fix: `fix_nm` from the threshold, on the bearing she left at -
//     so that, turning out over it towards the runway, she stays beyond
//     the furthest any circuit is flown (6.5 nm, measured) and a Class D
//     surface area's usual 4 nm. Chosen, not sourced: at 8 nm a long hold
//     of the F-15C's came within 6.0 nm.
//   - The inbound leg towards the runway, through the fix, and the
//     outbound leg away from it: the hold lies beyond the fix.
//   - Cleaned up: flap up, gear up where it retracts. Flown at 1.4 times
//     her reference speed - a clean wing's margin over its stall, chosen -
//     at least twenty knots over it and at most 200 kt, or the reference
//     and twenty where that is faster.
//   - Her height: 700 ft over the highest the person ahead flies (she is
//     held there, raised), and 1,000 ft over the highest ground within
//     4 nm of the hold - 14 CFR 91.177(a)(2)(ii)'s IFR minimum, the
//     ground sampled every 250 m within 4 nm and a leg's reach of the fix,
//     once, as the hold is set up. More than `sets_off_within_ft` below
//     that height she climbs in right turns where she is - over ground her
//     circuit was cleared for - before she sets off for the fix, and is
//     not round the fix until she has.
//   - **Out of her reach**: where her climb stalls - less than
//     `stalled_gain_ft` gained in `stalled_s`, her ceiling, chosen - she
//     gives the hold up: she holds the best height she reached, circling
//     where she is, never setting off over ground she cannot clear, and
//     says so (`out_of_reach`). It ends as any hold does, the person down
//     or gone (Controller::leaves_with).
class HoldOutside {
public:
    static constexpr double sets_off_within_ft = 200.0;
    static constexpr double stalled_gain_ft = 100.0;
    static constexpr double stalled_s = 120.0;
    static constexpr double fix_nm = 9.0;
    static constexpr double leg_s = 60.0;
    static constexpr double bank_deg = 25.0;
    enum class Leg { to_fix, turn_out, outbound, turn_in };

    // `at_least_alt_ft`: the height she leaves her circuit at, above sea
    // level.
    HoldOutside(const Aircraft& aircraft, const Runway& runway, const ApproachSpeeds& speeds,
                double at_least_alt_ft);
    AutopilotModes modes();
    void configure(Controls& controls) const;
    Leg leg() const { return leg_; }
    const Runway& runway() const { return runway_; }
    const ApproachSpeeds& speeds() const { return speeds_; }
    double altitude_ft() const { return altitude_ft_; }
    double speed_kts() const { return speed_kts_; }
    double fix_lat_deg() const { return fix_lat_deg_; }
    double fix_lon_deg() const { return fix_lon_deg_; }
    // How many times she has crossed the fix.
    int laps() const { return laps_; }
    // Whether her hold's height was out of her reach, and she holds her
    // best instead where she is.
    bool out_of_reach() const { return out_of_reach_; }

private:
    const Aircraft& a_;
    Runway runway_;
    ApproachSpeeds speeds_;
    double fix_lat_deg_ = 0.0;
    double fix_lon_deg_ = 0.0;
    double inbound_deg_ = 0.0;
    double altitude_ft_ = 0.0;
    double speed_kts_ = 0.0;
    Leg leg_ = Leg::to_fix;
    double leg_began_s_ = 0.0;
    int laps_ = 0;
    // The best height of her climb before the hold, and when she last
    // gained `stalled_gain_ft` on it.
    double best_alt_ft_ = 0.0;
    double best_at_s_ = 0.0;
    bool out_of_reach_ = false;
    // Still to climb before setting off, where she is: and her climb
    // judged, giving the hold up where it has stalled.
    bool climbing_first(const AircraftState& s);
};

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
    // `straight_in`: an aircraft on a straight-in approach to her runway -
    // followed until she turns base; from there she is committed and
    // it is not.
    // `climbing_out`: a person climbing out from her runway ahead of her
    // (sim::climbing_out_by_hand) - she flies on up her upwind leg, offset
    // right, and turns crosswind only once the person has gone: the AIM's
    // crosswind turn is "beyond the departure end", and a tower's
    // departure goes once the one before has "turned to avert any
    // conflict" (JO 7110.65 3-9-6); turning across the climb is not that.
    void follow(std::optional<double> ahead_alt_ft, bool straight_in = false,
                bool climbing_out = false);
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
    bool climbing_out_ = false;
    // Across, where the racetrack first left the downwind leg: the line it
    // joins again every time round.
    std::optional<double> downwind_m_;
    int racetracks_ = 0;
};

} // namespace glideslope::sim
