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

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/navigator.hpp"

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

    // Hands the aircraft to the AI, holding what it is doing, or flying `plan`.
    void to_ai();
    void to_ai(FlightPlan plan);
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

    // The aircraft's controls for the next step. Call it once a step.
    Controls fly();

private:
    const Aircraft& a_;
    Flying flying_ = Flying::pilot;
    Controls pilot_;
    Controls applied_;
    bool catching_up_ = false;
    std::optional<Autopilot> autopilot_;
    std::optional<Navigator> navigator_;
};

} // namespace glideslope::sim
