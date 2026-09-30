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

// Every control of `from` a hand's step towards `to`; whether all got there.
bool towards(Controls& from, const Controls& to) {
    bool met = true;
    for (auto [control, wanted] :
         {std::pair{&from.aileron, to.aileron},
          std::pair{&from.elevator, to.elevator},
          std::pair{&from.rudder, to.rudder},
          std::pair{&from.throttle, to.throttle},
          std::pair{&from.mixture, to.mixture},
          std::pair{&from.flaps, to.flaps},
          std::pair{&from.left_brake, to.left_brake},
          std::pair{&from.right_brake, to.right_brake},
          std::pair{&from.pitch_trim, to.pitch_trim},
          std::pair{&from.propeller, to.propeller},
          std::pair{&from.gear, to.gear},
          std::pair{&from.supercharger, to.supercharger},
          std::pair{&from.throttle_offset[0], to.throttle_offset[0]},
          std::pair{&from.throttle_offset[1], to.throttle_offset[1]},
          std::pair{&from.cooling_flaps[0], to.cooling_flaps[0]},
          std::pair{&from.cooling_flaps[1], to.cooling_flaps[1]},
          std::pair{&from.speedbrake, to.speedbrake}}) {
        met = towards(*control, wanted) && met;
    }
    return met;
}

} // namespace

Controller::Controller(const Aircraft& aircraft, const Controls& controls)
    : a_(aircraft), pilot_(controls), applied_(controls) {}

void Controller::engage() {
    flying_ = Flying::ai;
    catching_up_ = false;
    easing_in_ = false;
    autopilot_.emplace(a_, applied_);
    navigator_.reset();
    departure_.reset();
    lander_.reset();
    learnt_.reset();
    landing_.reset();
}

void Controller::to_ai() {
    std::optional<Lander> landing = std::move(landing_);
    engage();
    // **On the landing roll, the landing.** Still landing - rolling, or in a
    // bounce - after an approach the AI flew: the approach's own lander is
    // given her back and goes on from where it was, knowing where she touched
    // and the autobrake it set for the runway left, to the stop.
    if (landing && landing->still_landing(pilot_.throttle)) {
        landing->resume(applied_.throttle);
        lander_.emplace(std::move(*landing));
        easing_in_ = true;
    }
}

void Controller::to_ai(FlightPlan plan) {
    engage();
    navigator_.emplace(a_, std::move(plan));
}

void Controller::to_ai_take_off(const Runway& runway, const DepartureSpeeds& speeds,
                                double to_ft) {
    engage();
    departure_.emplace(a_, runway, speeds, to_ft);
}

void Controller::to_ai_flying(FlightPlan plan, const DepartureSpeeds& speeds) {
    if (!plan.takeoff) {
        to_ai(std::move(plan));
        return;
    }
    const FlightPlan::TakeOff takeoff = *plan.takeoff;
    to_ai_take_off(takeoff.runway, speeds, takeoff.to_ft);
    navigator_.emplace(a_, std::move(plan));
}

void Controller::to_ai_approach(const Runway& runway, const ApproachSpeeds& speeds,
                                double glidepath_deg) {
    engage();
    lander_.emplace(a_, runway, speeds, glidepath_deg);
}

void Controller::to_ai_learnt_approach(const Runway& runway, const ApproachSpeeds& speeds,
                                       std::shared_ptr<const LearntPolicy> policy) {
    engage();
    learnt_.emplace(a_, runway, std::move(policy), speeds);
    easing_in_ = true;
}

void Controller::to_pilot() {
    flying_ = Flying::pilot;
    catching_up_ = true;
    easing_in_ = false;
    autopilot_.reset();
    navigator_.reset();
    departure_.reset();
    learnt_.reset();
    // An approach not yet landed to the stop is kept, for a take-back on its
    // roll to finish.
    landing_.reset();
    if (lander_ && lander_->stage() != Lander::Stage::stopped) {
        landing_.emplace(std::move(*lander_));
    }
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
            if (navigator_) {
                navigator_->begin_here();
            }
        }
        if (learnt_) {
            if (learnt_->stage() != LearntLander::Stage::stopped) {
                const Controls landing = learnt_->fly();
                if (easing_in_) {
                    easing_in_ = !towards(applied_, landing);
                } else {
                    applied_ = landing;
                }
                return applied_;
            }
            learnt_.reset();
            autopilot_.emplace(a_, applied_);
        }
        if (lander_) {
            if (lander_->stage() != Lander::Stage::stopped) {
                const Controls landing = lander_->fly();
                if (easing_in_) {
                    easing_in_ = !towards(applied_, landing);
                } else {
                    applied_ = landing;
                }
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
    if (landing_) {
        landing_->watch();
        if (!landing_->still_landing(pilot_.throttle)) {
            landing_.reset();
        }
    }
    if (catching_up_) {
        // Every control on its way to where the pilot has it.
        catching_up_ = !towards(applied_, pilot_);
        return applied_;
    }
    applied_ = pilot_;
    return applied_;
}

} // namespace glideslope::sim
