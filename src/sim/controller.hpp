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

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/circuit.hpp"
#include "sim/vacate.hpp"
#include "sim/departure.hpp"
#include "sim/lander.hpp"
#include "sim/learnt.hpp"
#include "sim/navigator.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace glideslope::sim {

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
    void to_ai_take_off(const Runway& runway, const DepartureSpeeds& speeds,
                        double to_ft = 500.0);
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
    // gate without meeting it - flared, or under 1.6 miles out - she is left
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
    // From the learnt landing too, while she is in the air.
    void go_around();
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
    // **The fastest the AI's autopilot may hold** (Autopilot::limit_speed),
    // kept through whatever it flies: her fastest a plan may ask. Only the
    // plain autopilot - holding, navigating, or round a go-around's circuit
    // - raises its speed for a climb; the take-off, the approach, the flare,
    // the roll-out, the learnt landing and the vacating fly their own laws.
    void limit_speed(std::optional<double> fastest_kts) { fastest_kts_ = fastest_kts; }
    // Whether the AI's autopilot is flying it now - not a take-off, a
    // landing, a glide or its pilot - so that a limit on its height can be
    // flown. A glide cannot climb to a floor: it is given way to instead.
    bool autopilot_flying() const {
        return flying_ == Flying::ai && autopilot_ && !departure_ && !lander_ && !learnt_ &&
               !vacate_ && !glide_kts_;
    }

    // The aircraft's controls for the next step. Call it once a step.
    Controls fly();

private:
    // The AI engaged with the plain autopilot, and nothing else.
    void engage();

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
    // Off the runway after landing, and stopped beside it.
    std::optional<Vacate> vacate_;
    bool vacates_ = false;
    std::optional<double> fastest_kts_;
    Vacate::SpotFree spot_free_;
    RunwayClearQuery runway_clear_;
    bool runway_not_clear(const Runway& runway) const;
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

} // namespace glideslope::sim
