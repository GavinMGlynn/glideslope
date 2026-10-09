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

// **The way on to a plan's final approach**: six and then four miles out on
// `runway`'s extended centreline, on the glidepath the approach autopilot
// flies - aimed `aim_m` past the threshold - at ten knots over the reference
// speed. Joined at four miles, she is lined up and on the glidepath, and the
// approach autopilot has two miles to put the landing flap out and slow her
// before the learnt landing's gate. The landing itself kept, so that the
// controller knows what these legs are for.
FlightPlan final_legs(const Runway& runway, const ApproachSpeeds& speeds) {
    FlightPlan legs;
    legs.aircraft = "final";
    legs.landing = runway;
    constexpr double metres_per_nm = 1852.0;
    constexpr double metres_per_degree = 111320.0;
    const double heading = runway.heading_deg * std::numbers::pi / 180.0;
    const double lat = runway.threshold_lat_deg * std::numbers::pi / 180.0;
    for (const double out_nm : {6.0, 4.0}) {
        const double out_m = out_nm * metres_per_nm;
        Waypoint w;
        w.name = out_nm > 5.0 ? "FINAL_6NM" : "FINAL_4NM";
        w.latitude_deg = runway.threshold_lat_deg - out_m * std::cos(heading) / metres_per_degree;
        w.longitude_deg = runway.threshold_lon_deg -
                          out_m * std::sin(heading) / (metres_per_degree * std::cos(lat));
        w.altitude_ft = runway.elevation_ft + (out_m + speeds.aim_m) *
                                                  std::tan(3.0 * std::numbers::pi / 180.0) /
                                                  0.3048;
        w.airspeed_kts = speeds.vref_kts + 10.0;
        legs.waypoints.push_back(w);
    }
    return legs;
}

} // namespace

Controller::Controller(const Aircraft& aircraft, const Controls& controls)
    : a_(aircraft), pilot_(controls), applied_(controls) {}

void Controller::engage() {
    flying_ = Flying::ai;
    before_the_stall_.reset();
    stall_armed_ = true;
    mixture_held_ = false;
    catching_up_ = false;
    easing_in_ = false;
    autopilot_.emplace(a_, applied_);
    navigator_.reset();
    departure_.reset();
    lander_.reset();
    circuit_.reset();
    vacate_.reset();
    learnt_.reset();
    at_gate_.reset();
    on_final_legs_ = false;
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
        const std::optional<Runway> under = runway_under_ ? runway_under_(a_) : std::nullopt;
        lander_.emplace(under ? Lander::on_its_roll(a_, *landing_speeds_, *under)
                              : Lander::on_its_roll(a_, *landing_speeds_));
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
    on_final_legs_ = false;
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

// **The AI pilot notices a stall coming, and recovers from it** - at the
// first sign of it, its warning, which the rules have sound no less than 5
// knots or 5% above the stall, whichever is more (14 CFR 25.207(c)), the
// same margin the stall lessons give theirs. The stall is her published one
// with everything down (`lands_with`), scaled to what she weighs now: the
// lowest she has, so clean or banked the warning comes late, never early; an aeroplane that
// publishes none is not watched. **The recovery is the autopilot's own**
// (AutopilotModes::speed_on_elevator): the nose down until the wing is
// unloaded, full power, and the speed flown to her approach speed - the
// FAA's recovery (Airplane Flying Handbook, FAA-H-8083-3C, chapter 5;
// AC 120-109A). **Recovered** - at that speed and no longer descending, for
// five seconds - what she was flying is flown again, from where she is, at
// no less than that speed. Nothing is noticed on the ground or the water,
// nor while the autopilot is already flying a stall recovery it was asked
// for, which is a lesson's.
void Controller::notice_a_stall() {
    if (!landing_speeds_ || landing_speeds_->stall_kts <= 0.0) {
        return;
    }
    constexpr double least_margin_kts = 5.0;
    constexpr double margin_of_stall = 0.05;
    constexpr double level_within_fpm = 100.0;
    constexpr int recovered_for = 5 * steps_per_second;
    // **At what she weighs now** (`for_weight`): the figures' stall is for
    // the loading it was measured at, and a B-2A at 327,000 lb stalls far
    // above her light loading's 95 kt.
    const ApproachSpeeds speeds = for_weight(*landing_speeds_, a_.property("inertia/weight-lbs"));
    const double stall_kts = speeds.stall_kts;
    const double warning_kts =
        stall_kts + std::max(least_margin_kts, margin_of_stall * stall_kts);
    const double recovered_kts = speeds.vref_kts;
    const double kts = a_.property("velocities/vc-kts");
    if (!before_the_stall_) {
        // **Noticed again only once clear of the warning by its own margin
        // again**: handed back at her approach speed, a gust or a plan's
        // slow speed would otherwise have her noticed again at once.
        if (!stall_armed_) {
            stall_armed_ = kts >= warning_kts + (warning_kts - stall_kts);
        }
        const bool airborne = a_.property("gear/wow") < 0.5 && !a_.in_water();
        if (!stall_armed_ || !airborne || autopilot_->modes().speed_on_elevator ||
            kts > warning_kts) {
            return;
        }
        before_the_stall_ = autopilot_->modes();
        recovered_steps_ = 0;
    }
    const bool flying_again = kts >= recovered_kts &&
                              a_.property("velocities/h-dot-fps") * 60.0 >= -level_within_fpm;
    recovered_steps_ = flying_again ? recovered_steps_ + 1 : 0;
    if (recovered_steps_ >= recovered_for) {
        // What she was flying, from where she is: a height held is held
        // where she has come to, and a speed asked below the one she is
        // recovered at - what slowed her - is raised to it.
        AutopilotModes modes = *before_the_stall_;
        before_the_stall_.reset();
        stall_armed_ = false;
        ++stalls_noticed_;
        if (modes.altitude_ft) {
            modes.altitude_ft = a_.property("position/h-sl-ft");
        }
        if (modes.airspeed_kts) {
            modes.airspeed_kts = std::max(*modes.airspeed_kts, recovered_kts);
        }
        autopilot_->set(modes);
        return;
    }
    AutopilotModes modes = autopilot_->modes();
    modes.altitude_ft.reset();
    modes.airspeed_kts = recovered_kts;
    modes.speed_on_elevator = true;
    autopilot_->set(modes);
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
    unstable_go_arounds_ = 0;
    lander_.emplace(a_, runway, speeds, glidepath_deg);
    lander_->hand_mixture(applied_.mixture);
}

void Controller::to_ai_approach(const Runway& runway, const ApproachSpeeds& speeds,
                                std::shared_ptr<const LearntPolicy> learnt_at_gate,
                                double glidepath_deg) {
    // **Down to the learnt landing's gate at its policy's own speed**, not
    // the one for what she weighs (sim::for_weight): its gate admits her
    // only within -3/+8 kt of the speed it was trained at, over the weights
    // it was trained at - the C172P's 59.8 kt from 1,730 lb to her model's
    // 1,880 (`trained_lbs`), where scaled for 1,880 she would come down at
    // 52.9 and never meet it. Gone around, or landed from past it, she is
    // flown at the speed for her weight (`gate_speeds_`); and at a weight
    // it was not trained at, which its gate refuses, from the start.
    ApproachSpeeds to_gate = speeds;
    if (learnt_at_gate && learnt_at_gate->trained_for(a_.property("inertia/weight-lbs"))) {
        to_gate.vref_kts = learnt_at_gate->vref_kts;
        to_gate.reference_lbs = 0.0;
    }
    to_ai_approach(runway, to_gate, glidepath_deg);
    at_gate_ = std::move(learnt_at_gate);
    gate_runway_ = runway;
    gate_speeds_ = speeds;
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
    before_the_stall_.reset();
    stall_armed_ = true;
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
    at_gate_.reset();
    // An approach not yet landed to the stop is kept, for a take-back on its
    // roll to finish.
    landing_.reset();
    if (lander_ && lander_->stage() != Lander::Stage::stopped) {
        landing_.emplace(std::move(*lander_));
    }
    lander_.reset();
    circuit_.reset();
    vacate_.reset();
}

void Controller::go_around() {
    // **From the learnt landing too**, while it is still in the air: an
    // approach lander takes her from where she is and flies the go-around,
    // as from a balloon.
    if (learnt_ && learnt_->stage() == LearntLander::Stage::flying && !learnt_->touched() &&
        gate_runway_ && gate_speeds_) {
        lander_.emplace(a_, *gate_runway_, *gate_speeds_);
        lander_->hand_mixture(applied_.mixture);
        learnt_.reset();
        easing_in_ = true;
    }
    if (lander_) {
        lander_->go_around();
    }
}

bool Controller::runway_not_clear(const Runway& runway) const {
    return runway_clear_ && a_.state().height_above_ground_ft < RunwayClear::decide_ft &&
           !runway_clear_(runway);
}

Controls Controller::fly() {
    if (flying_ == Flying::ai) {
        // **Landed, she taxis off the runway** and stops beside it, where
        // she is told to (`vacates_runways`).
        if (vacate_) {
            applied_ = vacate_->fly();
            return applied_;
        }
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
            // **A light aeroplane's climb is eased down to the plan's**
            // (Autopilot::ease_climb): handed over at full throttle on her
            // best-climb floor. A jet climbs away at thousands of feet a
            // minute, which 25 ft/min a second would take minutes to bring
            // down; the F-22A, eased, climbed past her orbit's height to
            // 14,000 ft.
            if (a_.climb_floor_kts()) {
                autopilot_->ease_climb(a_.property("velocities/h-dot-fps") * 60.0);
            }
            if (navigator_) {
                navigator_->begin_here();
            }
        }
        if (learnt_ && learnt_->stage() == LearntLander::Stage::flying && !learnt_->touched() &&
            gate_runway_ && runway_not_clear(*gate_runway_)) {
            // **The runway not clear on short final: go around.**
            go_around();
            return fly();
        }
        // **The learnt landing is not judged by the stabilized-approach
        // gate** (StabilizedApproach): its policy was trained to land, not
        // to hold its speed, and from all 160 corners of its own gate it
        // passes 500 ft up to 35 kt over the reference speed - and lands
        // within its limits. Its gate box is what admits it.
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
            if (vacates_ && gate_runway_) {
                vacate_.emplace(a_, *gate_runway_, applied_, spot_free_);
                learnt_.reset();
                return fly();
            }
            learnt_.reset();
            autopilot_.emplace(a_, applied_);
        }
        if (lander_ && at_gate_) {
            // **At the learnt landing's gate, handed to it**; past it
            // unmet, left to the approach autopilot.
            if (lander_->stage() != Lander::Stage::approach) {
                at_gate_.reset();
            } else if (outside_learnt_gate(a_, *gate_runway_, *at_gate_).empty()) {
                LearntLander landing(a_, *gate_runway_, std::move(at_gate_), *gate_speeds_);
                at_gate_.reset();
                lander_.reset();
                learnt_.emplace(std::move(landing));
                easing_in_ = true;
                return fly();
            }
        }
        if (lander_ && !lander_->touched() &&
            (lander_->stage() == Lander::Stage::approach ||
             lander_->stage() == Lander::Stage::flare) &&
            runway_not_clear(lander_->runway())) {
            lander_->go_around();
        }
        if (lander_) {
            if (lander_->stage() == Lander::Stage::stopped && vacates_) {
                vacate_.emplace(a_, lander_->runway(), applied_, spot_free_);
                lander_.reset();
                return fly();
            }
            if (lander_->stage() != Lander::Stage::stopped && !lander_->gone_around()) {
                const Controls landing = lander_->fly();
                if (easing_in_) {
                    easing_in_ = !towards(applied_, landing);
                } else {
                    applied_ = landing;
                }
                return applied_;
            }
            // **Gone around, she is flown round again** to the same runway
            // (sim/circuit.hpp), not left climbing on the plain autopilot.
            if (lander_->gone_around()) {
                unstable_go_arounds_ += lander_->went_around_unstabilized() ? 1 : 0;
                circuit_.emplace(a_, lander_->runway(), lander_->speeds());
            }
            lander_.reset();
            autopilot_.emplace(a_, applied_);
        }
        if (circuit_) {
            if (circuit_->on_final()) {
                lander_.emplace(a_, circuit_->runway(), circuit_->speeds());
                lander_->hand_mixture(applied_.mixture);
                if (unstable_go_arounds_ >= StabilizedApproach::most_go_arounds) {
                    lander_->waive_the_gate();
                }
                circuit_.reset();
                applied_ = lander_->fly();
                return applied_;
            }
            autopilot_->set(circuit_->modes());
            notice_a_stall();
            autopilot_->limit_height(floor_ft_, ceiling_ft_);
            autopilot_->turn_away(away_deg_);
            autopilot_->limit_speed(fastest_kts_);
            applied_ = autopilot_->fly();
            circuit_->configure(applied_);
            return applied_;
        }
        // **A plan that ends in a landing**: its waypoints passed, the way on
        // to the final approach; that passed, the approach - and the learnt
        // landing at its gate, if she has one.
        if (navigator_ && navigator_->finished() && navigator_->plan().landing &&
            landing_speeds_ && !glide_kts_) {
            const Runway runway = *navigator_->plan().landing;
            if (!on_final_legs_) {
                navigator_.emplace(
                    a_, final_legs(runway, for_weight(*landing_speeds_,
                                                      a_.property("inertia/weight-lbs"))));
                on_final_legs_ = true;
            } else if (landing_policy_) {
                to_ai_approach(runway, *landing_speeds_, landing_policy_);
                return fly();
            } else {
                to_ai_approach(runway, *landing_speeds_);
                return fly();
            }
        }
        if (navigator_) {
            AutopilotModes modes = navigator_->steer();
            if (glide_kts_) {
                modes = gliding(modes);
            }
            autopilot_->set(modes);
        }
        notice_a_stall();
        autopilot_->limit_height(floor_ft_, ceiling_ft_);
        autopilot_->turn_away(away_deg_);
        autopilot_->limit_speed(fastest_kts_);
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
