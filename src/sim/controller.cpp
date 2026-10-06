#include "sim/controller.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

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
    mixture_held_ = false;
    catching_up_ = false;
    easing_in_ = false;
    autopilot_.emplace(a_, applied_);
    navigator_.reset();
    departure_.reset();
    lander_.reset();
    learnt_.reset();
    landing_.reset();
    // A glide is for the route it came with, and ends with it.
    glide_kts_.reset();
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
        return;
    }
    // **And on a roll the pilot landed her on**, no approach given: rolling
    // on her wheels with the pilot's throttle no more than half open - not a
    // take-off - she is landed to the stop on the line she is rolling along,
    // not handed the plain autopilot, which holds what she is doing and
    // would never stop her.
    if (landing_speeds_ && a_.property("gear/wow") > 0.5 && !a_.in_water() &&
        std::abs(a_.property("velocities/vg-fps")) >= 1.0 && pilot_.throttle <= 0.5) {
        lander_.emplace(Lander::on_its_roll(a_, *landing_speeds_));
        lander_->resume(applied_.throttle);
        lander_->hand_mixture(applied_.mixture);
        easing_in_ = true;
    }
}

void Controller::to_ai(FlightPlan plan) {
    engage();
    navigator_.emplace(a_, std::move(plan));
}

void Controller::replan(FlightPlan plan) {
    // A take-off, an approach or a learnt landing let go: the autopilot is
    // engaged afresh from the controls they had, which steps nothing.
    if (flying_ != Flying::ai || departure_ || lander_ || learnt_) {
        to_ai(std::move(plan));
        return;
    }
    navigator_.emplace(a_, std::move(plan));
}

void Controller::set_glide(std::optional<double> airspeed_kts) {
    if (airspeed_kts && !glide_kts_) {
        // From the vertical speed the aircraft has, so nothing jumps.
        glide_sink_fpm_ = std::min(a_.property("velocities/h-dot-fps") * 60.0, 0.0);
        glide_last_kts_ = a_.property("velocities/vc-kts");
        glide_trend_kts_per_s_ = 0.0;
    }
    glide_kts_ = airspeed_kts;
}

// **A glide is the airspeed flown by the vertical speed**: faster than the
// glide, less sink is asked for, and the nose comes up; slower, more. The
// sink the glide settles at is found by an integral, from the vertical speed
// the glide began with. Not the autopilot's own airspeed on the elevator
// (AutopilotModes::speed_on_elevator), which is its stall recovery: it keeps
// the wing below the greatest angle of attack it has seen, so an aeroplane
// that has only cruised cannot be slowed by it to a glide - a Cessna asked for
// 68 kt from 100 swung between 73 and 84 kt for a minute.
AutopilotModes Controller::gliding(AutopilotModes modes) {
    constexpr double fpm_per_knot = 80.0;
    constexpr double fpm_per_knot_second = 4.0;
    constexpr double fpm_per_knot_a_second = 250.0; // the speed's trend, to damp it
    constexpr double trend_filter_s = 1.0;
    // **The steepest sink asked is a flight path, not a rate**: 2,500 ft/min,
    // or 12 degrees down where that is more. Held to 2,500 ft/min from 30,000
    // ft, where a fighter gliding at 170 KCAS is doing some 270 knots true,
    // the F-15C gliding round her tightest orbit at every speed from 170 to
    // 200 kt sagged to 112 kt and settled into her deep stall at 43 degrees
    // of alpha, and the F-35B at 204 departed (glides_without_stalling,
    // test_navigator.cpp). **Nothing here knows where the ground is**: the
    // glide trial stops at 1,000 ft, which is that trial's protection only;
    // a copilot's glide is kept off the ground by its route, as before.
    constexpr double steepest_glide_deg = 12.0;
    const double least_fpm =
        -std::max(2500.0, a_.property("velocities/vt-fps") * 60.0 *
                              std::sin(steepest_glide_deg * std::numbers::pi / 180.0));
    constexpr double most_fpm = 500.0;
    const double dt = 1.0 / static_cast<double>(steps_per_second);
    const double kts = a_.property("velocities/vc-kts");
    glide_trend_kts_per_s_ +=
        ((kts - glide_last_kts_) / dt - glide_trend_kts_per_s_) * dt / trend_filter_s;
    glide_last_kts_ = kts;
    const double over_kts = kts - *glide_kts_;
    // A glide's own sink is a descent: the integral finds it between level
    // and the steepest asked for.
    glide_sink_fpm_ =
        std::clamp(glide_sink_fpm_ + fpm_per_knot_second * over_kts * dt, least_fpm, 0.0);
    modes.vertical_speed_fpm =
        std::clamp(glide_sink_fpm_ + fpm_per_knot * over_kts +
                       fpm_per_knot_a_second * glide_trend_kts_per_s_,
                   least_fpm, most_fpm);
    // The glide's airspeed is still asked for, which the throttle cannot
    // give with the engine stopped: asked for, it lowers the least speed the
    // autopilot holds a climb or descent to (Aircraft::climb_floor_kts), which
    // is above a light aircraft's best glide.
    modes.altitude_ft.reset();
    modes.airspeed_kts = *glide_kts_;
    modes.speed_on_elevator = false;
    return modes;
}

void Controller::to_ai_take_off(const Runway& runway, const DepartureSpeeds& speeds,
                                double to_ft) {
    engage();
    departure_.emplace(a_, runway, speeds, to_ft);
    departure_->hand_mixture(applied_.mixture);
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
    lander_->hand_mixture(applied_.mixture);
}

void Controller::to_ai_learnt_approach(const Runway& runway, const ApproachSpeeds& speeds,
                                       std::shared_ptr<const LearntPolicy> policy) {
    // Made first, so that a policy for another aircraft is refused before
    // anything the AI or the pilot had is let go.
    LearntLander landing(a_, runway, std::move(policy), speeds);
    engage();
    learnt_.emplace(std::move(landing));
    easing_in_ = true;
}

void Controller::to_pilot() {
    flying_ = Flying::pilot;
    glide_kts_.reset();
    catching_up_ = true;
    // The mixture the AI left, held as the ratio it gives (JSBSim meters the
    // fuel as the lever over the pressure ratio) until the lever moves.
    mixture_held_ = a_.mixture_lever();
    held_mixture_ = applied_.mixture;
    held_delta_ = std::max(a_.property("atmosphere/delta"), 1e-3);
    lever_at_take_back_.reset();
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
            if (lander_->stage() != Lander::Stage::stopped && !lander_->gone_around()) {
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
            AutopilotModes modes = navigator_->steer();
            if (glide_kts_) {
                modes = gliding(modes);
            }
            autopilot_->set(modes);
        }
        autopilot_->limit_height(floor_ft_, ceiling_ft_);
        applied_ = autopilot_->fly();
        return applied_;
    }
    if (landing_) {
        landing_->watch();
        if (!landing_->still_landing(pilot_.throttle)) {
            landing_.reset();
        }
    }
    Controls wanted = pilot_;
    if (mixture_held_) {
        if (!lever_at_take_back_) {
            lever_at_take_back_ = pilot_.mixture;
        }
        const double held = std::min(
            held_mixture_ * a_.property("atmosphere/delta") / held_delta_, 1.0);
        // **The lever moved, or the held ratio has reached the lever's** -
        // within the two hundredths the leaner feels the peak by, so that a
        // mixture the AI left at full rich, feeling, is the pilot's at once:
        // the mixture is the pilot's again.
        if (pilot_.mixture != *lever_at_take_back_ || held >= pilot_.mixture - 0.02) {
            mixture_held_ = false;
            catching_up_ = true;
        } else {
            wanted.mixture = held;
        }
    }
    if (catching_up_) {
        // Every control on its way to where the pilot has it.
        catching_up_ = !towards(applied_, wanted);
        return applied_;
    }
    applied_ = wanted;
    return applied_;
}

} // namespace glideslope::sim
