#include "sim/controller.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>

namespace glideslope::sim {

namespace {

// A pilot's hand: full travel in a second.
constexpr double hand_per_step = 1.0 / static_cast<double>(steps_per_second);

// `from` moved towards `to` by a hand's step at most; whether it got there.
bool towards(double& from, double to) {
    from += std::clamp(to - from, -hand_per_step, hand_per_step);
    return from == to;
}

} // namespace

Controller::Controller(const Aircraft& aircraft, const Controls& controls)
    : a_(aircraft), pilot_(controls), applied_(controls) {}

void Controller::to_ai() {
    flying_ = Flying::ai;
    catching_up_ = false;
    autopilot_.emplace(a_, applied_);
    navigator_.reset();
    departure_.reset();
    lander_.reset();
}

void Controller::to_ai(FlightPlan plan) {
    to_ai();
    navigator_.emplace(a_, std::move(plan));
}

void Controller::to_ai_take_off(const Runway& runway, const DepartureSpeeds& speeds,
                                double to_ft) {
    to_ai();
    departure_.emplace(a_, runway, speeds, to_ft);
}

void Controller::to_ai_approach(const Runway& runway, const ApproachSpeeds& speeds,
                                double glidepath_deg) {
    to_ai();
    lander_.emplace(a_, runway, speeds, glidepath_deg);
}

void Controller::to_pilot() {
    flying_ = Flying::pilot;
    catching_up_ = true;
    autopilot_.reset();
    navigator_.reset();
    departure_.reset();
    lander_.reset();
}

Controls Controller::fly() {
    if (flying_ == Flying::ai) {
        // **A take-off or an approach flies itself until it is over**, and
        // then the plain autopilot holds what the aeroplane is doing. The
        // autopilot is engaged from the controls the departure or the landing
        // left, so the aeroplane does not lurch at the moment the AI stops
        // taking off and starts flying.
        if (departure_) {
            if (departure_->stage() != Departure::Stage::done) {
                applied_ = departure_->fly();
                return applied_;
            }
            departure_.reset();
            autopilot_.emplace(a_, applied_);
        }
        if (lander_) {
            if (lander_->stage() != Lander::Stage::stopped) {
                applied_ = lander_->fly();
                return applied_;
            }
            lander_.reset();
            autopilot_.emplace(a_, applied_);
        }
        if (navigator_) {
            autopilot_->set(navigator_->steer());
        }
        applied_ = autopilot_->fly();
        return applied_;
    }
    if (catching_up_) {
        // Every control on its way to where the pilot has it.
        bool met = true;
        for (auto [control, wanted] :
             {std::pair{&applied_.aileron, pilot_.aileron},
              std::pair{&applied_.elevator, pilot_.elevator},
              std::pair{&applied_.rudder, pilot_.rudder},
              std::pair{&applied_.throttle, pilot_.throttle},
              std::pair{&applied_.mixture, pilot_.mixture},
              std::pair{&applied_.flaps, pilot_.flaps},
              std::pair{&applied_.left_brake, pilot_.left_brake},
              std::pair{&applied_.right_brake, pilot_.right_brake},
              std::pair{&applied_.pitch_trim, pilot_.pitch_trim},
              std::pair{&applied_.propeller, pilot_.propeller},
              std::pair{&applied_.gear, pilot_.gear},
              std::pair{&applied_.supercharger, pilot_.supercharger},
              std::pair{&applied_.throttle_offset[0], pilot_.throttle_offset[0]},
              std::pair{&applied_.throttle_offset[1], pilot_.throttle_offset[1]},
              std::pair{&applied_.cooling_flaps[0], pilot_.cooling_flaps[0]},
              std::pair{&applied_.cooling_flaps[1], pilot_.cooling_flaps[1]},
              std::pair{&applied_.speedbrake, pilot_.speedbrake}}) {
            met = towards(*control, wanted) && met;
        }
        catching_up_ = !met;
        return applied_;
    }
    applied_ = pilot_;
    return applied_;
}

} // namespace glideslope::sim
