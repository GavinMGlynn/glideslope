#pragma once

// Who flies an aircraft: its pilot, through the controls their hands and
// devices set, or the AI pilot - the autopilot, and a navigator when it has a
// plan. Every aircraft is a JSBSim instance and a controller, and handing an
// aircraft between the user and the AI is swapping who its controller listens
// to (REQUIREMENTS.md, section 3).
//
// **Handing over steps nothing.** To the AI: the autopilot engages from the
// controls the aircraft has (sim/autopilot.hpp). Back to the pilot: their
// controls are where their hands are, seldom where the AI had the aircraft's,
// so the aircraft's controls move from the AI's towards the pilot's at the pace
// of a pilot's hand - full travel in a second - until they meet, and follow the
// pilot's directly from then.
//
// **But the mixture stays where the AI left it until the pilot moves the
// lever.** The AI leans an engine high up (sim/leaner.hpp); a pilot who never
// touched the lever has it at full rich, and walked there high up the engine
// runs too rich to fire. So, taken back, the mixture keeps the ratio of air
// to fuel the AI left the engine at - richening as a descent thickens the air,
// up to full rich - until the pilot's lever moves from where it was at the
// take-back, and from then it is the lever's, reached at the hand's pace.
//
// **Taken back on a landing roll, the AI finishes the landing.** An aeroplane
// the AI was flying an approach in, taken by the pilot and handed back while
// she is still landing - rolling, bouncing, or in the flare - is given that
// approach's lander back, to land her to the stop with the brakes, the
// spoilers and the centreline, not the plain autopilot, which holds what she
// is doing and would never stop her. Its controls come from where the pilot
// left them at the same hand's pace, so taking her back steps nothing either.
// **One way, and this landing only**: the first step the pilot has her that
// she is not still landing on that runway - a go-around, a turn off, a stop -
// the lander is dropped, and a take-back after it is the plain autopilot.
//
// **A STOVL aeroplane's lift system is flown by her flight-control laws**,
// never by a lever: the F-35B's pilot has no nozzle lever. Lockheed Martin's
// and Pratt & Whitney's account of her control laws (Walker, Fuller and
// Wurth, "F-35B Integrated Flight-Propulsion Control Development", AIAA
// 2013-4243, its abstract) calls them "unified": in STOVL mode the stick
// commands rate of climb or flight path and the left-hand inceptor
// longitudinal acceleration, and the flight-control computer turns what
// they ask into the lift fan's, the roll posts' and the nozzle's commands.
// Here that computer is the transition law (sim/transition.hpp), and:
//
//   the pilot's STOVL mode selector (Controls::stovl) at 1 converts her and
//   slows her to a hover where she stops, at the height she had - no lower
//   than her least conversion height; on the ground, she stands converted.
//   In STOVL mode the stick's fore-and-aft asks for height: full back
//   climbs at the hover law's 10 ft/s, full forward sinks at its 6, and
//   asked down to `land_below_ft` she lets down and lands. The selector
//   back at 0 converts her back to wing-borne flight at her conversion
//   speed, and once she is, the controls are the pilot's again, reached
//   from the law's at a hand's pace; on the ground, at once. **And her
//   left-hand inceptor, the stick's sideways and the pedals**
//   (Transition::Inceptors): the throttle lever, once moved past
//   `lever_release` from where it was at the selector's last rising edge,
//   asks a ground speed either side of its
//   detent (`fcs/stovl-lever-detent`) under her translational rate command
//   and an acceleration above it; in the hover the stick's sideways asks a
//   ground speed to the side and the pedals a turn. Jet-borne, the stick's
//   sideways and the pedals are not flown yet.
//
//   a plan's vertical landing (FlightPlan::vertical_landing) is flown to
//   the point, and from `Transition::stops_within_ft` of it the transition
//   law converts her and stops her in a hover over it at `vertical_hover_ft`,
//   holds it the plan's seconds, and lets her down onto it.
//
// **The lift system is handed over as everything else is**: given to the AI
// in STOVL mode, the AI holds the hover she is in at the height asked; a
// plan given her then is flown - converted back at her conversion speed
// and flown wing-borne, or, its vertical landing nearer than she stops in,
// moved to its point in the hover at her translational rate command's
// speed (the first waypoints within `Transition::steer_within_ft` of her,
// flown by with no orbit, counted
// passed) and let down there. Taken back in it, she stays in STOVL mode
// until the pilot's selector moves from where it was at the take-back.

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/circuit.hpp"
#include "sim/vacate.hpp"
#include "sim/departure.hpp"
#include "sim/lander.hpp"
#include "sim/learnt.hpp"
#include "sim/navigator.hpp"
#include "sim/transition.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::sim {

// **What her figures tell a controller**: how she lands (`landing_speeds`),
// for a landing taken over on its roll, and how she takes off
// (`departure_speeds`), for a take-off taken over at lift-off - the one place
// the server's controllers (`controller_for`) and the client's (`Flight`)
// are told them from, so that a test of it is a test of theirs. Either is
// none where her figures do not give it, and `unread` says why: a hand-over
// there is the plain autopilot.
struct HerFigures {
    std::optional<ApproachSpeeds> lands;
    std::optional<DepartureSpeeds> takes_off;
    std::vector<std::string> unread;
};
HerFigures her_figures(const std::filesystem::path& data, const std::string& model);

class Controller {
public:
    enum class Flying { pilot, ai };

    // `aircraft`, flown by its pilot, whose controls are `controls` now.
    Controller(const Aircraft& aircraft, const Controls& controls);

    Flying flying() const {
        return flying_;
    }

    // Whether, handed back, the controls are still on their way to where the
    // pilot has them.
    bool catching_up() const {
        return catching_up_;
    }

    // The pilot's controls this step, whoever is flying.
    void set_pilot(const Controls& controls) {
        pilot_ = controls;
    }

    // Hands the aircraft to the AI, holding what it is doing, or flying `plan`
    // - or, on the landing roll of an approach it was given, landing her to
    // the stop; or on a roll the pilot landed her on, with no approach given,
    // where it has been told how she lands (`lands_with`), the same.
    void to_ai();
    // **How she is landed**, for a landing the pilot made and the AI is
    // given on its roll: her approach speeds, from her figures
    // (`approach_speeds`). Without them - an aeroplane that publishes no
    // stall speed - such a take-back is the plain autopilot.
    void lands_with(const ApproachSpeeds& speeds) { landing_speeds_ = speeds; }
    // **And the learnt landing she is handed to at its gate**, for a plan
    // that ends in a landing (`land`): none lands her with the approach
    // autopilot alone. A plan's landing needs `lands_with`: without approach
    // speeds the plan's last waypoint is flown on, as with no landing.
    void lands_learnt(std::shared_ptr<const LearntPolicy> policy) {
        landing_policy_ = std::move(policy);
    }
    // The runway the learnt landing was handed her for, if it has been.
    const Runway* learnt_runway() const {
        return learnt_ && gate_runway_ ? &*gate_runway_ : nullptr;
    }
    // **Which runway she is rolling on**, for that same take-back: asked once,
    // as she is handed over, it gives the runway under her wheels - the
    // world's runways are not the simulation's to know (world/runway_ground.hpp
    // has `runway_rolled_on`) - and she is braked for what is left of it.
    // Without one, or where it finds none, the runway is her track and the
    // brakes are set to autobrake 3.
    using RunwayUnder = std::function<std::optional<Runway>(const Aircraft&)>;
    void finds_runways_with(RunwayUnder runway_under) { runway_under_ = std::move(runway_under); }
    // **How she takes off**, for a take-off the pilot made and hands to the
    // AI just after lift-off (`lifting_off`): her departure speeds, from her
    // figures (`departure_speeds`). Handed over so, she is climbed away by
    // the take-off's own law (sim::Departure::from_lift_off) to
    // `lift_off_climb_to_ft` above the ground she left, her flap brought up
    // as a take-off brings it, and then given the plan or the hold as any
    // hand-over is - not the plain autopilot at once, which holds the height
    // she was handed over at, and put seven of thirteen landplanes handed
    // over on the step their wheels left back on the runway within 30 s.
    // Without them such a hand-over is the plain autopilot.
    void takes_off_with(const DepartureSpeeds& speeds) { takeoff_speeds_ = speeds; }
    // Both, from `her_figures`: what each gives.
    void told(const HerFigures& figures);
    // How she was told she takes off, or null.
    const DepartureSpeeds* takeoff_speeds() const {
        return takeoff_speeds_ ? &*takeoff_speeds_ : nullptr;
    }
    const ApproachSpeeds* landing_speeds() const {
        return landing_speeds_ ? &*landing_speeds_ : nullptr;
    }
    // **Just after lift-off**, as a hand-over now would find her: off her
    // wheels and out of the water, with the pilot's throttle more than half
    // open - the line the roll's hand-over draws between a landing and a
    // take-off - never since her wheels left higher than
    // `lift_off_below_ft` above the ground she left, and either within
    // `lift_off_settling_s` of them, whatever her climb, or climbing at more
    // than `lift_off_climbing_fpm` within `lift_off_within_s`.
    bool lifting_off() const;
    // **The thresholds**: 400 ft is the height below which a take-off path
    // changes nothing of her configuration (14 CFR 25.111(c)(4)) and the
    // highest any class raises its take-off flap from
    // (DepartureSpeeds::flaps_up_ft) - above it the take-off's work is done
    // but for the flap, and a hold there holds her clear of the ground.
    // **40 s is how long a take-off takes to settle into its climb**, and a
    // quarter more: flown off by the take-off's law, a PA-28 accelerates
    // level at 20 ft in ground effect and first climbs steadily 31.6 s after
    // her wheels leave, the A380 sinks at 1,100 ft/min at 8 s, the Mosquito
    // holds 100 ft from 7 to 12 s - so before it her climb says nothing, and
    // a hand-over then at power and low down is a take-off. After it, 100
    // ft/min is what the stall recovery counts as level (`notice_a_stall`),
    // so a level hand-over is never a climb. Five minutes is the most rated
    // take-off power may be used for (14 CFR 1.1), where a take-off's own
    // law ends (sim::Departure). 500 ft is the take-off lessons' end; the
    // AI's own take-off ends at 1,000 by default (`ai_take_off_to_ft`).
    static constexpr double lift_off_below_ft = 400.0;
    static constexpr double lift_off_climbing_fpm = 100.0;
    static constexpr double lift_off_throttle = 0.5;
    static constexpr double lift_off_within_s = 300.0;
    static constexpr double lift_off_settling_s = 40.0;
    static constexpr double lift_off_climb_to_ft = 500.0;
    void to_ai(FlightPlan plan);

    // **The AI pilot can take off and land, not only hold and navigate.**
    // Handing it a runway gives it the take-off autopilot or the approach
    // autopilot instead of the plain one - which is what lets an instructor
    // demonstrate a take-off or an approach and then hand the aeroplane over.
    // Without these the AI could only be given an aeroplane already flying.
    //
    // `to_ai_take_off` flies from where the aeroplane stands to `to_ft` above
    // the runway; `to_ai_approach` flies it down the glidepath to a stop.
    // Both end by holding what the aeroplane is doing, so a demonstration
    // that runs past its end does not fall out of the sky.
    //
    // **Its height is 1,000 ft by default** (`ai_take_off_to_ft`): the
    // acceleration height a training flight cleans up at - "During training
    // flights, 1,000 feet AFE is normally used as the acceleration height
    // to initiate thrust reduction and flap retraction" (Boeing's 737
    // Classic Flight Crew Training Manual, FCT 737 CL (TM) revision 20, 30
    // June 2021, 3.28, Flap Retraction Schedule) - and clear of the take-off
    // lessons' end at 500 ft, so that a demonstration's climb away is judged
    // and not its level-off. Until 2026-11-06 it was 500: a jet still has her
    // flap out at her lead to it, levels off there and cleans up level once
    // she is (sim::Departure::level_off_at_end), and the 737-300 accelerated
    // past the take-off lesson's band for her climb a few feet short of 500.
    static constexpr double ai_take_off_to_ft = 1000.0;
    void to_ai_take_off(const Runway& runway, const DepartureSpeeds& speeds,
                        double to_ft = ai_take_off_to_ft);
    // **A plan that takes off** (`takeoff` in it): the take-off, from the
    // plan's runway to its height, and then the plan, flown from where the
    // take-off left the aeroplane. A plan without one is `to_ai(plan)`.
    void to_ai_flying(FlightPlan plan, const DepartureSpeeds& speeds);
    void to_ai_approach(const Runway& runway, const ApproachSpeeds& speeds,
                        double glidepath_deg = 3.0);

    // **The landing learnt by reinforcement learning** (sim/learnt.hpp): the
    // policy flies her from where she is - an approach gate - to the touch,
    // and the approach autopilot rolls her out to a stop. Its controls are
    // reached from the ones she has at a hand's pace, full travel in a second,
    // so the hand-over steps nothing. **While they are on their way** - up to
    // two seconds, for the elevator's full travel - the policy's `previous`
    // input is the action it asked for, not the controls she has, which it
    // never met in training: there the controls were always its own.
    // Handed back to the pilot, it is dropped (a take-back on its roll gets
    // the plain autopilot, not this).
    // Throws std::invalid_argument for a policy trained on another aircraft,
    // and hands nothing over.
    void to_ai_learnt_approach(const Runway& runway, const ApproachSpeeds& speeds,
                               std::shared_ptr<const LearntPolicy> policy);
    // **An approach handed to the learnt landing at its gate**: the approach
    // autopilot flies her down the glidepath, and the first step she is at
    // `policy`'s gate (sim::outside_learnt_gate) the policy takes her from
    // there, as to_ai_learnt_approach does - eased in, no step. Past the
    // gate without meeting it - flared, or under 1.7 miles out - she is left
    // to the approach autopilot, which lands her as to_ai_approach does.
    // How the server's own AI aircraft are landed by it.
    void to_ai_approach(const Runway& runway, const ApproachSpeeds& speeds,
                        std::shared_ptr<const LearntPolicy> learnt_at_gate,
                        double glidepath_deg = 3.0);

    // **A new plan while the AI flies**, from where the aircraft is: the
    // navigator is replaced and the autopilot kept, so nothing it holds is
    // dropped and the aircraft does not lurch. Flying a take-off, an
    // approach or a learnt landing, or not flying at all, it is `to_ai(plan)`:
    // that is let go, and the autopilot engaged from the controls it had.
    void replan(FlightPlan plan);
    // **A glide**: the plan's route steered at `airspeed_kts`, held by the
    // vertical speed asked of the autopilot, its heights not flown - for an
    // engine that has stopped. None flies the plan's heights again, and so
    // does handing the aircraft over either way (to_ai, to_pilot and the
    // rest): a glide ends with the route it came with.
    void set_glide(std::optional<double> airspeed_kts);
    std::optional<double> glide() const {
        return glide_kts_;
    }

    // Where the take-off or the approach has got to, or nothing when the AI
    // is not flying one.
    const Departure* departure() const { return departure_ ? &*departure_ : nullptr; }
    const Lander* lander() const { return lander_ ? &*lander_ : nullptr; }
    // **Go around**, where the AI is flying an approach: it climbs away and
    // flies round to the same runway again (sim/circuit.hpp).
    // From the learnt landing too, while she is in the air. `why`, where it
    // is the AI's own reason (an approach not stabilized), for the lander's
    // `why_gone_around`.
    void go_around(std::string why = {});
    // **Whether the runway she is landing on is clear**, asked of the
    // caller - the other aircraft are not the controller's to know - once
    // a step on an approach below RunwayClear::decide_ft (sim/vacate.hpp):
    // not clear, she goes around. Without it, nothing is asked.
    using RunwayClearQuery = std::function<bool(const Runway&)>;
    void clears_with(RunwayClearQuery clear) { runway_clear_ = std::move(clear); }
    // **Landed and stopped, she taxis off the runway** (sim::Vacate) and
    // stops beside it, rather than holding where she stopped.
    // Where she may stop, if told (sim::Vacate::SpotFree).
    void vacates_runways(Vacate::SpotFree spot_free = {}) {
        vacates_ = true;
        spot_free_ = std::move(spot_free);
    }
    const Vacate* vacate() const { return vacate_ ? &*vacate_ : nullptr; }
    // The circuit a go-around is being flown round, if one is.
    const GoAroundCircuit* circuit() const { return circuit_ ? &*circuit_ : nullptr; }
    // **Her place in a circuit's sequence** (sim::CirclePlace), from her
    // go-around until she touches down - round the circuit and down its
    // approach, and kept if her pilot takes her - or none.
    const std::optional<CirclePlace>& circling() const { return circling_; }
    // **Whom she follows round the circuit**, asked of the caller - the
    // other aircraft are not the controller's to know - every step she flies
    // one: the height of the highest circuit ahead of her in the sequence
    // (sim::circuit_ahead_alt_ft), or none. Without it, nothing is asked
    // and she is number one. `clock`: the time her place is taken at, one
    // clock for every aircraft the caller sequences - each aircraft's own
    // starts when it was made, so a later one's would put it ahead.
    // **Taken over in a circuit, she keeps her place for as long as she
    // flies**, as a tower holds the one behind for a pilot still in its
    // circuit: the one behind holds in her racetrack, over the highest her
    // pilot flies her, until her wheels touch. Nothing asks her pilot to
    // land, nor offers the one behind another runway: she leaves the circuit
    // instead (`leaves_with`).
    using CircuitAheadQuery = std::function<std::optional<double>()>;
    void follows_with(CircuitAheadQuery ahead, std::function<double()> clock) {
        circuit_ahead_ = std::move(ahead);
        sequence_clock_ = std::move(clock);
    }
    // **An aircraft on a straight-in approach, or departing**, asked of the
    // caller every step she flies a circuit: whether one is on the approach
    // to the runway she is going round to, or climbing out from it
    // (another's `straight_in` or `departing`). One is, and she has not
    // turned base, she follows it (sim/circuit.hpp). Without it,
    // nothing is asked.
    using StraightInQuery = std::function<bool(const Runway&)>;
    void gives_way_with(StraightInQuery straight_in) { straight_in_ = std::move(straight_in); }
    // **Traffic close ahead on her approach**, asked of the caller every
    // step she flies one (sim::close_ahead_on_approach of each other
    // aircraft): there is, and she goes around early. Without it, nothing
    // is asked.
    using CloseAheadQuery = std::function<bool(const Runway&)>;
    void spaced_with(CloseAheadQuery close) { close_ahead_ = std::move(close); }
    // **A person lined up or departing ahead of her approach**, asked of
    // the caller every step she flies one (sim::ahead_of_an_approach_by_hand
    // of every person's aircraft): there is, and she breaks
    // off at once - goes around. Nothing holds a person's take-off;
    // left to the runway's rule at 400 ft, a jet went around over a Cub
    // just lifted off, overtook her and came within 1,760 m and 347 ft.
    // Without it, nothing is asked.
    using DepartureAheadQuery = std::function<bool(const Runway&)>;
    void breaks_off_with(DepartureAheadQuery departure) {
        departure_ahead_ = std::move(departure);
    }
    // **A person climbing out from the runway she is going round to**,
    // asked of the caller every step she flies a circuit
    // (sim::climbing_out_by_hand of every person's aircraft): there is, and
    // her upwind leg goes on, offset right, until the person has gone -
    // she does not turn crosswind across the climb (GoAroundCircuit::follow).
    // A person only lined up is not climbing out: held for her, the circuit
    // would never reach the downwind leg that releases her. Without it,
    // nothing is asked.
    using ClimbingOutQuery = std::function<bool(const Runway&)>;
    void holds_upwind_with(ClimbingOutQuery climbing) { climbing_out_ = std::move(climbing); }
    // **A departure held for the circuit**, asked of the caller every step
    // before her take-off roll begins (sim::departure_held of every
    // aircraft's place): held, she stands on the runway with the brakes on
    // and the power off; released once, she rolls and is not held again.
    // Without it, nothing is asked.
    using DepartureHoldQuery = std::function<bool(const Runway&)>;
    void holds_with(DepartureHoldQuery hold) { hold_ = std::move(hold); }
    // **Behind a person who stays in the circuit, she leaves it**, asked of
    // the caller every step she flies a circuit (sim::behind_a_person of
    // every aircraft's place): a person cannot be told to land, so a
    // tower's other means are the AI's - "remain outside the Class D" (AIM
    // 3-2-5). Having flown `racetracks_behind_a_person` whole racetracks
    // behind her, on her next outbound leg - outside the circuit and 700 ft
    // over the highest the person flies it - she leaves for a standard hold
    // outside it (sim::HoldOutside), cleaned up, keeping her place. Once
    // the person is down (no person ahead of her place), she comes back:
    // the legs to final and the approach. Without it, nothing is asked and
    // she holds in her racetrack until the person lands.
    using BehindAPersonQuery = std::function<bool()>;
    void leaves_with(BehindAPersonQuery behind) { behind_a_person_ = std::move(behind); }
    static constexpr int racetracks_behind_a_person = 1;
    // Whether she has left a circuit behind a person, and the sim time she
    // did, s.
    std::optional<double> left_the_circuit_s() const { return left_the_circuit_s_; }
    // The hold she is flying outside the circuit, if she is; and the sim
    // time she left it to come back and land, s.
    const HoldOutside* hold() const { return holding_out_ ? &*holding_out_ : nullptr; }
    std::optional<double> rejoined_at_s() const { return rejoined_at_s_; }
    // **Short of fuel, she comes back from her hold to land** whether or not
    // the person ahead is down: what is in her tanks lasts less than
    // `minimum_fuel_s` at what her engines burn now. "Minimum fuel" (AIM
    // 5-5-15; ICAO's MINIMUM FUEL): no undue delay can be accepted. An
    // F-22A put on final at her landing weight, held 15 minutes behind a
    // person flying away, ran her tanks dry on her approach back and fell
    // 0.3 nm short at 2,783 ft/min. The ten minutes are chosen: the legs to
    // final and the approach from a hold 9 nm out, and a go-around.
    static constexpr double minimum_fuel_s = 600.0;
    bool short_of_fuel() const;
    // **How long her way back to land takes, s**, waiting with nowhere to
    // divert: from where she is to the six-mile point and down the final to
    // the threshold, flown at her approach's reference speed - slower than
    // she flies most of it, which allows for the more she burns there
    // gear down. She stops waiting for the circuit when what she has lasts
    // less than this and `minimum_fuel_s` (ICAO's MAYDAY FUEL: her fuel on
    // landing below the reserve). Nought when not holding.
    double fuel_back_s() const;
    // How long what is in her tanks lasts at what her engines burn now, s
    // (infinite: burning nothing).
    double fuel_lasts_s() const;
    // Whether she came back from her hold short of fuel.
    bool came_back_short_of_fuel() const { return came_back_short_; }
    // Whether she has priority, come back short of fuel or with nowhere to
    // divert: first in her runway's sequence, never sent to hold again.
    bool has_priority() const { return priority_; }
    // Whether she came back from a hold out of her reach, with nowhere to
    // divert.
    bool came_back_out_of_reach() const { return came_back_out_of_reach_; }
    // **Whether she asks for her runway's circuit to be cleared of a
    // person**: her hold out of her reach with nowhere to divert, come down
    // where she is and waiting, while a person is ahead of her in that
    // runway's sequence (`leaves_with`). A server tells that person to leave
    // the circuit (net::TrafficInstruction); she comes back once the
    // person's place is dropped, or once she is short of fuel whatever the
    // person does.
    bool asks_the_circuit_cleared() const { return asks_the_circuit_cleared_; }
    // **Her hold out of her reach, she diverts** (HoldOutside::out_of_reach):
    // asked once, as she gives her hold up, for the runways the caller knows
    // of within her fuel's reach - where she is, and how far she can fly at
    // her hold's speed in her fuel less `minimum_fuel_s` (a server: the
    // runway ends of its world's runways that near, world::runway_ends_
    // within) - she leaves for the nearest suitable one
    // (sim::choose_diversion) - her place in the sequence dropped, cleaned
    // up at her hold's speed and at the way's height, joining its extended
    // centreline and then its legs to final (six and four miles out) and its
    // approach, as a landing from her hold is flown; there a straight-in,
    // given way to and spaced as any is. `needs_m`: her landing's need,
    // metres (0: none refused for length). **Where none is suitable, or
    // nothing is asked, she comes back at once** to the runway she held for,
    // as one short of fuel does (`short_of_fuel`): holding on where she is
    // only burns what she will need there.
    // An end told with no elevation (NaN) is given it by `elevation`
    // (feet, at its threshold) only when it is judged, nearest first
    // (world::diversion_runways).
    using AlternatesQuery =
        std::function<std::vector<Runway>(double latitude_deg, double longitude_deg,
                                          double within_m)>;
    using ElevationQuery = std::function<double(double latitude_deg, double longitude_deg)>;
    void diverts_with(AlternatesQuery alternates, ElevationQuery elevation, double needs_m) {
        alternates_ = std::move(alternates);
        alternate_elevation_ = std::move(elevation);
        alternate_needs_m_ = needs_m;
    }
    // Where she diverted to, and the way there, if she has.
    const std::optional<Diversion>& diverted_to() const { return diverted_to_; }
    // Whether her take-off roll has begun (none: no take-off).
    bool rolling() const { return departure_ && released_; }
    // **The runway she is departing**: her take-off rolling or climbing
    // out, until it is over - or none.
    std::optional<Runway> departing() const;
    // Why she last went around, as her approach's lander gave it (empty:
    // the runway not clear, or never).
    const std::string& why_gone_around() const { return why_gone_around_; }
    // **The runway she is flying a straight-in approach to**: the AI's
    // approach, or her learnt landing, in the air and not from a circuit's
    // sequence (`circling`) - or none.
    std::optional<Runway> straight_in() const;
    // How many times this approach has gone around for being unstabilized;
    // from StabilizedApproach::most_go_arounds on, the gate is waived.
    int unstable_go_arounds() const { return unstable_go_arounds_; }
    const LearntLander* learnt() const { return learnt_ ? &*learnt_ : nullptr; }
    // Hands it back to the pilot.
    void to_pilot();

    // While the AI flies: its autopilot, to be told what to hold. Null when the
    // pilot flies.
    Autopilot* autopilot() {
        return autopilot_ ? &*autopilot_ : nullptr;
    }
    const Navigator* navigator() const {
        return navigator_ ? &*navigator_ : nullptr;
    }

    // **A floor and a ceiling on the height the AI's autopilot flies to**
    // (sim/separation.hpp), kept until set again, through whatever plan or
    // hold it flies; none for either is no limit. A take-off or a landing is
    // not limited: only the autopilot is.
    void limit_height(std::optional<double> floor_ft, std::optional<double> ceiling_ft) {
        floor_ft_ = floor_ft;
        ceiling_ft_ = ceiling_ft;
    }
    // **A heading the AI's autopilot turns to instead of its own**
    // (Autopilot::turn_away), kept until set again; none is its own.
    void turn_away(std::optional<double> heading_deg) { away_deg_ = heading_deg; }
    // **The fastest the AI's autopilot may hold** (Autopilot::limit_speed),
    // kept through whatever it flies: her fastest a plan may ask. Only the
    // plain autopilot - holding, navigating, or round a go-around's circuit
    // - raises its speed for a climb; the take-off, the approach, the flare,
    // the roll-out, the learnt landing and the vacating fly their own laws.
    void limit_speed(std::optional<double> fastest_kts) { fastest_kts_ = fastest_kts; }
    // **The speeds a plan may fly her at** (sim::plan_speeds), measured at
    // her model's weight: a plan's or a route's speed under the slowest is
    // flown at the slowest for what she weighs now (sim::for_weight) - so
    // loaded heavier than her model, she is flown faster, as her stall is
    // higher. Not given, a plan's speeds are flown as asked.
    void plans_within(std::optional<PlanSpeeds> speeds) { plan_speeds_ = speeds; }
    // Whether the AI's autopilot is flying it now - not a take-off, a
    // landing, a glide or its pilot - so that a limit on its height can be
    // flown. A glide cannot climb to a floor: it is given way to instead.
    bool autopilot_flying() const {
        return flying_ == Flying::ai && autopilot_ && !departure_ && !lander_ && !learnt_ &&
               !vacate_ && !glide_kts_;
    }

    // The aircraft's controls for the next step. Call it once a step.
    Controls fly();
    // **Her lift system's commands for the next step**, from the last
    // `fly`, while her flight-control laws fly it; else null, and her lift
    // system keeps what it was last given. `apply` below puts both on her.
    const LiftSystem* lift() const {
        return lift_ ? &*lift_ : nullptr;
    }
    // **What a pilot's controller carries from step to step**, for a
    // prediction that flies a pilot's inputs again from the server's word
    // (sim::Prediction): the controls applied and on their way to the
    // pilot's, the mixture held after a take-back, and the lift system's
    // laws - the transition law, with its autopilot, hover law and their
    // integrals - and what they were asked. Copied out before a step and put
    // back before the steps are flown again, they are flown again as they
    // were. Only while the pilot flies: the AI's state is not in it.
    struct PilotsLaws {
        Controls pilot;
        Controls applied;
        bool catching_up = false;
        bool mixture_held = false;
        double held_mixture = 1.0;
        double held_delta = 1.0;
        std::optional<double> lever_at_take_back;
        std::optional<Transition> stovl;
        std::optional<LiftSystem> lift;
        LiftSystem held_lift;
        double stovl_height_ft = 0.0;
        std::optional<double> hovering_since_s;
        std::optional<double> selector_at_take_back;
        std::optional<double> lever_at_selection;
        bool selector_was = false;
    };
    PilotsLaws pilots_laws() const;
    void restore(const PilotsLaws& laws);
    // **Her STOVL law as it flies her now, for a client joining her**: the
    // transition law's state, the height asked, the lift system's commands
    // and where it stands, and the lever latched at the selection - what a
    // prediction begun on a model just loaded cannot work out (net::LawState).
    // None while no law flies her.
    struct LawState {
        Transition::State law;
        double height_asked_ft = 0.0;
        LiftSystem commands;
        Aircraft::LiftPositions positions;
        std::optional<double> lever_at_selection;
        // Taken back or over in STOVL mode, the selector then, held until
        // it moves.
        std::optional<double> selector_at_take_back;
    };
    std::optional<LawState> law_state() const;
    // **Flown on from a law told**, as her pilot's: the law made again from
    // `told`, its height asked and lever latch, and the lift system's
    // commands as the law last gave them. Where she stands is the caller's
    // (Aircraft::seat_lift_system); sim::Prediction does both.
    void take_law(const LawState& told);
    // The transition law, while it flies her lift system; else null.
    const Transition* transition() const {
        return stovl_ ? &*stovl_ : nullptr;
    }
    // In STOVL mode, the height asked of the transition law above the
    // ground beneath her, ft.
    double stovl_height_asked_ft() const {
        return stovl_height_ft_;
    }
    // **The thresholds**: the height under which the stick's height asks
    // for a landing; the height a plan's vertical landing hovers at - the
    // F-35B's first hover's, "a zero airspeed hover at 150 feet above the
    // runway" (sim/transition.hpp); and the stick's climb and sink, the
    // hover law's (sim/hover.cpp).
    static constexpr double land_below_ft = 20.0;
    static constexpr double vertical_hover_ft = 150.0;
    static constexpr double stick_climb_fps = 10.0;
    static constexpr double stick_sink_fps = 6.0;
    // Within this of her lever's detent, the lever is at it; and the lever
    // latched at a selection is the pilot's once it has moved more than
    // `lever_release` from where it was (a hand resting on it, or a
    // keyboard's or a network's rounding, moves it less).
    static constexpr double lever_detent_width = 0.02;
    static constexpr double lever_release = 0.05;

    // **Whether the AI is recovering from a stall it noticed coming**
    // (controller.cpp, `notice_a_stall`).
    bool recovering_from_a_stall() const { return before_the_stall_.has_value(); }
    // How many stalls it has noticed and recovered from, and handed back.
    int stalls_recovered() const { return stalls_noticed_; }

private:
    // The AI engaged with the plain autopilot, and nothing else.
    void engage();
    // **The lift system's laws**, while they fly her: the controls, and
    // `lift_` set; none while they do not.
    std::optional<Controls> fly_lift_system();
    // Whether the AI's plan's vertical landing is to be begun now.
    bool vertical_landing_due() const;
    // Heavier than her model's `fcs/stovl-hover-most-lbs`: no hover is
    // asked of her - a pilot's STOVL mode is flown wing-borne at her
    // conversion speed, the stick asking her height, and a plan's vertical
    // landing is not begun.
    bool too_heavy_to_hover() const;
    // A plan with a vertical landing flown to its point: the point added as
    // its last waypoint, at the height and speed of the one before.
    FlightPlan to_its_point(FlightPlan plan) const;
    std::optional<Transition> stovl_;
    std::optional<LiftSystem> lift_;
    LiftSystem held_lift_;
    double stovl_height_ft_ = 0.0;
    std::optional<double> hovering_since_s_;
    // Taken back in STOVL mode, the selector then, held until it moves.
    std::optional<double> selector_at_take_back_;
    // Selected or taken back in STOVL mode, the throttle lever then, held
    // until it moves: an unmoved lever is not the pilot's speed.
    std::optional<double> lever_at_selection_;
    // Whether the selector asked STOVL mode last step, for its rising edge.
    bool selector_was_ = false;
    // A stall warning noticed, and recovered from (`landing_speeds_`'s stall).
    void notice_a_stall();
    // What the autopilot was flying when the warning came, while it recovers;
    // and how many steps it has been recovered for.
    std::optional<AutopilotModes> before_the_stall_;
    int recovered_steps_ = 0;
    // Watching for a stall again: false from a hand-back until the speed is
    // clear of the warning by the warning's own margin again.
    bool stall_armed_ = true;
    int stalls_noticed_ = 0;

    const Aircraft& a_;
    Flying flying_ = Flying::pilot;
    Controls pilot_;
    Controls applied_;
    bool catching_up_ = false;
    std::optional<Autopilot> autopilot_;
    std::optional<Navigator> navigator_;
    std::optional<Departure> departure_;
    std::optional<Lander> lander_;
    // A go-around flown round to the approach again.
    std::optional<GoAroundCircuit> circuit_;
    std::optional<CirclePlace> circling_;
    CircuitAheadQuery circuit_ahead_;
    std::function<double()> sequence_clock_;
    // The sequence's time now: the caller's clock, or her own.
    double sequence_now() const;
    int unstable_go_arounds_ = 0;
    // Off the runway after landing, and stopped beside it.
    std::optional<Vacate> vacate_;
    bool vacates_ = false;
    std::optional<double> fastest_kts_;
    std::optional<PlanSpeeds> plan_speeds_;
    Vacate::SpotFree spot_free_;
    RunwayClearQuery runway_clear_;
    StraightInQuery straight_in_;
    BehindAPersonQuery behind_a_person_;
    std::optional<double> left_the_circuit_s_;
    std::optional<double> rejoined_at_s_;
    bool came_back_short_ = false;
    bool came_back_out_of_reach_ = false;
    bool asks_the_circuit_cleared_ = false;
    bool priority_ = false;
    std::optional<HoldOutside> holding_out_;
    struct Rejoin {
        Runway runway;
        ApproachSpeeds speeds;
    };
    std::optional<Rejoin> rejoin_;
    AlternatesQuery alternates_;
    ElevationQuery alternate_elevation_;
    double alternate_needs_m_ = 0.0;
    bool alternates_asked_ = false;
    std::optional<Diversion> diverted_to_;
    // Diverting: the way there flown cleaned up, until she has joined the
    // extended centreline.
    bool diverting_en_route_ = false;
    DepartureHoldQuery hold_;
    std::string why_gone_around_;
    bool released_ = false;
    CloseAheadQuery close_ahead_;
    DepartureAheadQuery departure_ahead_;
    ClimbingOutQuery climbing_out_;
    bool runway_not_clear(const Runway& runway) const;
    // Into the circuit's sequence as she goes around from `lander_`'s
    // approach, unless she is in it for that runway already.
    void join_the_sequence();
    std::optional<LearntLander> learnt_;
    // The learnt landing an approach is handed to at its gate, while it is
    // still to be met: with the runway and speeds it lands with.
    std::shared_ptr<const LearntPolicy> at_gate_;
    std::optional<Runway> gate_runway_;
    std::optional<ApproachSpeeds> gate_speeds_;
    // The approach's lander while the pilot has her, kept from the hand-over
    // so a take-back on its landing roll can finish it; dropped the first
    // step she is not still landing, and when the AI is given her again, for
    // this or for anything else.
    std::optional<Lander> landing_;
    std::optional<ApproachSpeeds> landing_speeds_;
    // When her wheels last bore weight, as fly() saw them, and her height
    // over the ground then: a hand-over within a second of it and no more
    // than three feet above is a hand-over on the roll.
    static constexpr double skip_s = 1.0;
    static constexpr double skip_ft = 3.0;
    std::optional<double> wheels_down_at_s_;
    double wheels_down_agl_ft_ = 0.0;
    // And where they were, along which track, and how high she has been
    // since they left: the ground a take-off left, for `lifting_off`.
    double wheels_down_lat_deg_ = 0.0;
    double wheels_down_lon_deg_ = 0.0;
    double wheels_down_alt_ft_ = 0.0;
    double wheels_down_track_deg_ = 0.0;
    double highest_since_wheels_ft_ = 0.0;
    std::optional<DepartureSpeeds> takeoff_speeds_;
    // Handed over just after lift-off, the take-off's climb-out; whether it
    // was given.
    bool climb_out_if_lifting_off();
    std::shared_ptr<const LearntPolicy> landing_policy_;
    // Flying a plan's way on to its final approach (`land`): six and four
    // miles out on the centreline.
    bool on_final_legs_ = false;
    RunwayUnder runway_under_;
    // The lander's controls, reached from the pilot's at a hand's pace after
    // a take-back on the roll.
    bool easing_in_ = false;
    std::optional<double> glide_kts_;
    std::optional<double> floor_ft_;
    std::optional<double> ceiling_ft_;
    std::optional<double> away_deg_;
    double glide_sink_fpm_ = 0.0;
    double glide_last_kts_ = 0.0;
    double glide_trend_kts_per_s_ = 0.0;
    AutopilotModes gliding(AutopilotModes modes);
    // Taken back, the mixture the AI left and the air's pressure ratio then,
    // held until the pilot's lever moves from where it was at the take-back.
    bool mixture_held_ = false;
    double held_mixture_ = 1.0;
    double held_delta_ = 1.0;
    std::optional<double> lever_at_take_back_;
};

// **A lift system's commands put on `aircraft`, never thrown**: as
// Aircraft::set_lift_system, but STOVL mode asked above the airspeed her
// model clears it to (`fcs/stovl-limit-kcas`) - which the transition law
// never asks, converting at her conversion speed, but a dive while
// converting could meet - is flown with the fan's doors closing (STOVL mode
// 0) and the rest as asked, and an aircraft without a lift system is left
// alone. Whether the commands went on as asked.
bool put_lift_system(Aircraft& aircraft, const LiftSystem& lift);

// **A lever's travel from its detent**, -1 at its aft stop to 1 at its
// forward stop, 0 within `Controller::lever_detent_width` of the detent: the
// F-35B's left-hand inceptor in STOVL mode (Transition::Inceptors).
double lever_travel(double lever, double detent);

// **One step's commands put on `aircraft`**: `controller`'s controls, and
// its lift system's while its laws fly one (`put_lift_system`, so nothing
// here throws for her envelope). What each caller does once a step, so that
// the server's, the client's prediction and the tools' are the same.
// Returns the controls put on her.
Controls apply(Aircraft& aircraft, Controller& controller);

} // namespace glideslope::sim
