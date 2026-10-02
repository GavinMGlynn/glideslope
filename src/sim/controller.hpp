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
#include "sim/departure.hpp"
#include "sim/lander.hpp"
#include "sim/learnt.hpp"
#include "sim/navigator.hpp"

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
    // the stop.
    void to_ai();
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
    // Whether the AI's autopilot is flying it now - not a take-off, a
    // landing, a glide or its pilot - so that a limit on its height can be
    // flown. A glide cannot climb to a floor: it is given way to instead.
    bool autopilot_flying() const {
        return flying_ == Flying::ai && autopilot_ && !departure_ && !lander_ && !learnt_ &&
               !glide_kts_;
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
    std::optional<LearntLander> learnt_;
    // The approach's lander while the pilot has her, kept from the hand-over
    // so a take-back on its landing roll can finish it; dropped the first
    // step she is not still landing, and when the AI is given her again, for
    // this or for anything else.
    std::optional<Lander> landing_;
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
