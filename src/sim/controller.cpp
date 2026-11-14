#include "sim/controller.hpp"
#include "sim/separation.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <limits>
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
          std::pair{&from.speedbrake, to.speedbrake},
          std::pair{&from.reverse, to.reverse},
          std::pair{&from.stovl, to.stovl},
          std::pair{&from.roll_trim, to.roll_trim},
          std::pair{&from.yaw_trim, to.yaw_trim}}) {
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

HerFigures her_figures(const std::filesystem::path& data, const std::string& model) {
    HerFigures figures;
    try {
        figures.lands = landing_speeds(data, model);
        if (!figures.lands) {
            figures.unread.push_back("no approach speeds: her figures publish no stall speed");
        }
    } catch (const std::exception& e) {
        figures.unread.push_back(std::string("no approach speeds: ") + e.what());
    }
    try {
        figures.takes_off = departure_speeds(data, model);
    } catch (const std::exception& e) {
        figures.unread.push_back(std::string("no departure speeds: ") + e.what());
    }
    return figures;
}

void Controller::told(const HerFigures& figures) {
    if (figures.lands) {
        lands_with(*figures.lands);
    }
    if (figures.takes_off) {
        takes_off_with(*figures.takes_off);
    }
}

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
    holding_out_.reset();
    asks_the_circuit_cleared_ = false;
    rejoin_.reset();
    circling_.reset();
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
    //
    // **On her wheels, or in a skip off them**: her wheels bore weight within
    // the last `skip_s`, and she is no more than `skip_ft` higher than she
    // was then. Handed over a step after her wheels left the runway in a
    // skip, a 787 on a wet runway was given the plain autopilot and ran
    // 22.7 km.
    const bool in_a_skip =
        wheels_down_at_s_ && a_.state().sim_time_s - *wheels_down_at_s_ <= skip_s &&
        a_.property("position/h-agl-ft") - wheels_down_agl_ft_ <= skip_ft;
    if (landing_speeds_ && (a_.property("gear/wow") > 0.5 || in_a_skip) && !a_.in_water() &&
        std::abs(a_.property("velocities/vg-fps")) >= 1.0 && pilot_.throttle <= 0.5) {
        const std::optional<Runway> under = runway_under_ ? runway_under_(a_) : std::nullopt;
        lander_.emplace(under ? Lander::on_its_roll(a_, *landing_speeds_, *under)
                              : Lander::on_its_roll(a_, *landing_speeds_));
        lander_->resume(applied_.throttle);
        lander_->hand_mixture(applied_.mixture);
        easing_in_ = true;
        return;
    }
    climb_out_if_lifting_off();
}

bool Controller::lifting_off() const {
    if (!wheels_down_at_s_ || a_.property("gear/wow") > 0.5 || a_.in_water() ||
        pilot_.throttle <= lift_off_throttle) {
        return false;
    }
    const double since_s = a_.state().sim_time_s - *wheels_down_at_s_;
    const double ground_ft = wheels_down_alt_ft_ - wheels_down_agl_ft_;
    const bool climbing = a_.property("velocities/h-dot-fps") * 60.0 > lift_off_climbing_fpm;
    return since_s <= lift_off_within_s && (since_s <= lift_off_settling_s || climbing) &&
           highest_since_wheels_ft_ - ground_ft <= lift_off_below_ft &&
           a_.state().altitude_ft - ground_ft <= lift_off_below_ft;
}

bool Controller::climb_out_if_lifting_off() {
    if (!takeoff_speeds_ || !lifting_off()) {
        return false;
    }
    // The runway she left: the ground under her wheels as they last bore
    // weight, along the track she was rolling on.
    Runway left;
    left.name = "her take-off";
    left.threshold_lat_deg = wheels_down_lat_deg_;
    left.threshold_lon_deg = wheels_down_lon_deg_;
    left.elevation_ft = wheels_down_alt_ft_ - wheels_down_agl_ft_;
    left.heading_deg = wheels_down_track_deg_;
    departure_.emplace(Departure::from_lift_off(a_, left, wheels_down_agl_ft_, *takeoff_speeds_,
                                                applied_, lift_off_climb_to_ft));
    departure_->hand_mixture(applied_.mixture);
    easing_in_ = true;
    return true;
}

FlightPlan Controller::to_its_point(FlightPlan plan) const {
    if (plan.vertical_landing && a_.has_lift_system() && !plan.waypoints.empty()) {
        Waypoint point = plan.waypoints.back();
        // **Given her in STOVL mode, the waypoints she hovers by are passed**:
        // the first waypoints, within `Transition::steer_within_ft` of her -
        // nearer than she could turn for converted back - each flown by
        // with no orbit, are counted passed, once, as the plan is given. A
        // waypoint flown over, or orbited, is flown however near it is.
        if (stovl_) {
            const AircraftState s = a_.state();
            std::size_t k = 0;
            while (k < plan.waypoints.size() && !plan.waypoints[k].fly_over &&
                   !plan.waypoints[k].orbit &&
                   distance_m(s.latitude_deg, s.longitude_deg, plan.waypoints[k].latitude_deg,
                              plan.waypoints[k].longitude_deg) /
                           0.3048 <=
                       Transition::steer_within_ft) {
                ++k;
            }
            plan.waypoints.erase(plan.waypoints.begin(),
                                 plan.waypoints.begin() + static_cast<std::ptrdiff_t>(k));
        }
        point.name = plan.vertical_landing->name;
        point.latitude_deg = plan.vertical_landing->latitude_deg;
        point.longitude_deg = plan.vertical_landing->longitude_deg;
        point.fly_over = true;
        point.orbit.reset();
        plan.waypoints.push_back(point);
    }
    return plan;
}

bool Controller::too_heavy_to_hover() const {
    return a_.property("inertia/weight-lbs") > a_.property("fcs/stovl-hover-most-lbs");
}

bool Controller::vertical_landing_due() const {
    if (!navigator_ || !navigator_->plan().vertical_landing || departure_ || lander_ ||
        learnt_ || circuit_ || vacate_ || glide_kts_ || too_heavy_to_hover()) {
        return false;
    }
    if (navigator_->finished()) {
        return true;
    }
    // Flying to the point, the last waypoint `to_its_point` added.
    if (navigator_->next() + 1 < navigator_->plan().waypoints.size()) {
        return false;
    }
    const FlightPlan::VerticalLanding& v = *navigator_->plan().vertical_landing;
    const AircraftState s = a_.state();
    const double ft =
        distance_m(s.latitude_deg, s.longitude_deg, v.latitude_deg, v.longitude_deg) / 0.3048;
    return ft <= Transition::stops_within_ft(a_, a_.property("velocities/vc-kts"));
}

std::optional<Controls> Controller::fly_lift_system() {
    const bool on_ground = a_.property("gear/wow") > 0.5;
    const double least_ft = a_.property("fcs/stovl-convert-least-ft");
    // Taken back in STOVL mode: held there until the selector moves.
    bool selected = pilot_.stovl >= 0.5;
    if (flying_ == Flying::pilot && selector_at_take_back_) {
        if (pilot_.stovl != *selector_at_take_back_) {
            selector_at_take_back_.reset();
        } else {
            selected = true;
        }
    }
    // **The lever is latched at every selection**: on each rising edge of
    // the pilot's selector - the first, and again after a 1, 0, 1 part-way
    // through a conversion - the lever is not the pilot's speed until it has
    // moved more than `lever_release` from where it was then.
    if (flying_ == Flying::pilot) {
        if (selected && !selector_was_) {
            lever_at_selection_ = pilot_.throttle;
        }
        selector_was_ = selected;
    }
    if (!stovl_) {
        const bool wanted = flying_ == Flying::ai ? vertical_landing_due() : selected;
        if (!wanted) {
            return std::nullopt;
        }
        // **On the ground, heavier than she can hover, STOVL mode does
        // nothing**: the transition law, made on her wheels, stands her
        // converted for a vertical take-off she could not hold. Her lift
        // system is left as it is and the controls are the pilot's - she
        // may still roll for a take-off on the wing - until she is in the
        // air, where the law holds her wing-borne (`too_heavy_to_hover`).
        if (on_ground && too_heavy_to_hover()) {
            return std::nullopt;
        }
        // **Seeded from what she flies, never from the stick**: in STOVL
        // mode the pilot's stick asks a height and a speed, not the
        // surfaces, so the transition law begins from the elevator, the
        // ailerons, the rudder and the throttle the aircraft has. A
        // controller made on the step the selector arrived (the server's,
        // `controller_for`) holds the pilot's stick as what it applied -
        // full forward, it seeded the wing-borne autopilot's elevator at
        // full nose-down, and from 300 KCAS she dived 334 ft in five
        // seconds; through a server, into the ground (PROJECT_STATUS,
        // 2026-10-14).
        Controls seed = applied_;
        seed.elevator = -a_.property("fcs/elevator-cmd-norm");
        seed.aileron = a_.property("fcs/aileron-cmd-norm");
        seed.rudder = a_.property("fcs/rudder-cmd-norm");
        seed.throttle = a_.property("fcs/throttle-cmd-norm[0]");
        stovl_.emplace(a_, Hover::Command{seed, held_lift_});
        // The lever is the pilot's speed once it moves from where it was.
        lever_at_selection_ = pilot_.throttle;
        const double agl = a_.state().height_above_ground_ft;
        stovl_height_ft_ = on_ground ? agl : std::max(agl, least_ft);
        hovering_since_s_.reset();
    }
    const bool hovering = stovl_->stage() == Transition::Stage::hovering;
    if (!hovering) {
        hovering_since_s_.reset();
    } else if (!hovering_since_s_) {
        hovering_since_s_ = a_.state().sim_time_s;
    }
    Transition::Target target;
    target.kcas = a_.property("fcs/stovl-convert-kcas");
    // **A plan given her in STOVL mode** that is not yet at its vertical
    // landing - a plan of waypoints, or a pad farther than she stops in -
    // is flown wing-borne: she converts back at her conversion speed, and
    // the plan is flown from there.
    // The plan's waypoints she passes meanwhile are counted passed, so that
    // one she hovers over is not flown back to.
    bool plan_to_fly = false;
    if (flying_ == Flying::ai && navigator_ && !navigator_->finished() && !vertical_landing_due()) {
        navigator_->steer();
        plan_to_fly = !navigator_->finished() && !vertical_landing_due();
    }
    if (plan_to_fly) {
        target.hover = false;
        target.height_ft = std::max(stovl_height_ft_, least_ft);
    } else if (flying_ == Flying::ai) {
        if (navigator_ && navigator_->plan().vertical_landing) {
            const FlightPlan::VerticalLanding& v = *navigator_->plan().vertical_landing;
            target.height_ft = std::max(vertical_hover_ft, least_ft);
            // Kept as the height asked, so that a pilot taking her back in
            // the hover holds it, not the height she was at when it began.
            stovl_height_ft_ = target.height_ft;
            target.over = Transition::Point{v.latitude_deg, v.longitude_deg};
            target.land = hovering_since_s_ &&
                          a_.state().sim_time_s - *hovering_since_s_ >= v.hover_s;
        } else {
            // Holding the hover she was handed in.
            target.height_ft = stovl_height_ft_;
            target.land = hovering && stovl_height_ft_ <= land_below_ft;
        }
    } else if (selected) {
        // **The stick asks for height**: back to climb, forward to sink.
        const double e = std::clamp(pilot_.elevator, -1.0, 1.0);
        const double fps = e >= 0.0 ? e * stick_climb_fps : e * stick_sink_fps;
        stovl_height_ft_ += fps / static_cast<double>(steps_per_second);
        stovl_height_ft_ = std::max(stovl_height_ft_, hovering ? 0.0 : least_ft);
        target.height_ft = stovl_height_ft_;
        target.land = hovering && stovl_height_ft_ <= land_below_ft;
        // **The left-hand inceptor, the stick's sideways and the pedals**
        // (Transition::Inceptors): the lever's travel from its detent once
        // it has moved from where it was when STOVL mode was selected, or
        // she was taken back in it.
        if (lever_at_selection_ && std::abs(pilot_.throttle - *lever_at_selection_) > lever_release) {
            lever_at_selection_.reset();
        }
        Transition::Inceptors inceptors;
        if (!lever_at_selection_) {
            inceptors.lever = lever_travel(pilot_.throttle, a_.property("fcs/stovl-lever-detent"));
        }
        inceptors.side = std::clamp(pilot_.aileron, -1.0, 1.0);
        inceptors.pedals = std::clamp(pilot_.rudder, -1.0, 1.0);
        target.pilot = inceptors;
        // **Heavier than she can hover**, STOVL mode is flown wing-borne at
        // her conversion speed, the stick still asking her height: slowed
        // to a hover, the lift system could not hold her.
        if (too_heavy_to_hover() && !hovering) {
            target.hover = false;
            target.land = false;
            target.pilot.reset();
        }
    } else {
        target.hover = false;
        target.height_ft = std::max(stovl_height_ft_, least_ft);
        if (on_ground) {
            // On the ground the selector back is conventional flight at
            // once: the fan's doors close, the nozzle aft.
            held_lift_ = LiftSystem{};
            lift_ = held_lift_;
            stovl_.reset();
            catching_up_ = true;
            return std::nullopt;
        }
    }
    const Hover::Command c = stovl_->step(target);
    applied_ = c.controls;
    applied_.stovl = target.hover ? 1.0 : 0.0;
    held_lift_ = c.lift;
    lift_ = c.lift;
    // Converted back and wing-borne: the controls are the pilot's again, at
    // a hand's pace.
    if (!target.hover && stovl_->stage() == Transition::Stage::wing_borne &&
        a_.property("fcs/stovl-pos-norm") <= 0.0 && !(flying_ == Flying::pilot && selected)) {
        stovl_.reset();
        if (flying_ == Flying::ai) {
            // The AI's plan from here: the autopilot from the controls the
            // transition law left, the leg from where she is.
            autopilot_.emplace(a_, applied_);
            if (navigator_) {
                navigator_->begin_here();
            }
        } else {
            catching_up_ = true;
        }
        return std::nullopt;
    }
    return applied_;
}

Controller::PilotsLaws Controller::pilots_laws() const {
    PilotsLaws out;
    out.pilot = pilot_;
    out.applied = applied_;
    out.catching_up = catching_up_;
    out.mixture_held = mixture_held_;
    out.held_mixture = held_mixture_;
    out.held_delta = held_delta_;
    out.lever_at_take_back = lever_at_take_back_;
    if (stovl_) {
        out.stovl.emplace(*stovl_);
    }
    out.lift = lift_;
    out.held_lift = held_lift_;
    out.stovl_height_ft = stovl_height_ft_;
    out.hovering_since_s = hovering_since_s_;
    out.selector_at_take_back = selector_at_take_back_;
    out.lever_at_selection = lever_at_selection_;
    out.selector_was = selector_was_;
    return out;
}

void Controller::restore(const PilotsLaws& laws) {
    pilot_ = laws.pilot;
    applied_ = laws.applied;
    catching_up_ = laws.catching_up;
    mixture_held_ = laws.mixture_held;
    held_mixture_ = laws.held_mixture;
    held_delta_ = laws.held_delta;
    lever_at_take_back_ = laws.lever_at_take_back;
    // A transition law holds the aircraft by reference, and so is built
    // again from the copy rather than assigned.
    stovl_.reset();
    if (laws.stovl) {
        stovl_.emplace(*laws.stovl);
    }
    lift_ = laws.lift;
    held_lift_ = laws.held_lift;
    stovl_height_ft_ = laws.stovl_height_ft;
    hovering_since_s_ = laws.hovering_since_s;
    selector_at_take_back_ = laws.selector_at_take_back;
    lever_at_selection_ = laws.lever_at_selection;
    selector_was_ = laws.selector_was;
}

std::optional<Controller::LawState> Controller::law_state() const {
    if (!stovl_) {
        return std::nullopt;
    }
    LawState out;
    out.law = stovl_->state();
    out.height_asked_ft = stovl_height_ft_;
    out.commands = held_lift_;
    out.positions = a_.lift_positions();
    out.lever_at_selection = lever_at_selection_;
    out.selector_at_take_back = selector_at_take_back_;
    return out;
}

void Controller::take_law(const LawState& told) {
    stovl_.reset();
    stovl_.emplace(a_, told.law);
    stovl_height_ft_ = told.height_asked_ft;
    held_lift_ = told.commands;
    lift_ = told.commands;
    applied_ = told.law.last.controls;
    applied_.stovl = told.law.last_asked_hover.value_or(true) ? 1.0 : 0.0;
    catching_up_ = false;
    lever_at_selection_ = told.lever_at_selection;
    selector_at_take_back_ = told.selector_at_take_back;
    selector_was_ = true;
    hovering_since_s_.reset();
}

double lever_travel(double lever, double detent) {
    const double off = lever - detent;
    if (std::abs(off) < Controller::lever_detent_width) {
        return 0.0;
    }
    return std::clamp(off >= 0.0 ? off / std::max(1.0 - detent, 1e-6) : off / std::max(detent, 1e-6),
                      -1.0, 1.0);
}

void Controller::to_ai(FlightPlan plan) {
    engage();
    navigator_.emplace(a_, to_its_point(std::move(plan)));
    // **Just after lift-off, the climb-out first**, and the plan from where
    // it leaves her, as a plan that takes off is flown (`to_ai_flying`) -
    // but not in STOVL mode, where the transition law flies her first.
    if (!stovl_) {
        climb_out_if_lifting_off();
    }
}

void Controller::replan(FlightPlan plan) {
    // A take-off, an approach or a learnt landing let go: the autopilot is
    // engaged afresh from the controls they had, which steps nothing.
    if (flying_ != Flying::ai || departure_ || lander_ || learnt_) {
        to_ai(std::move(plan));
        return;
    }
    on_final_legs_ = false;
    navigator_.emplace(a_, to_its_point(std::move(plan)));
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
// same margin the stall lessons give theirs - or sooner, where her figures
// give her own warning sooner (ApproachSpeeds::warning_margin_kts: the
// Mosquito's buffet, 10 knots over). The stall is her published one
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
    constexpr double level_within_fpm = 100.0;
    constexpr int recovered_for = 5 * steps_per_second;
    // **At what she weighs now** (`for_weight`): the figures' stall is for
    // the loading it was measured at, and a B-2A at 327,000 lb stalls far
    // above her light loading's 95 kt.
    const ApproachSpeeds speeds = for_weight(*landing_speeds_, a_.property("inertia/weight-lbs"));
    const double stall_kts = speeds.stall_kts;
    const double warning_kts = stall_warning_kts(stall_kts, speeds.warning_margin_kts);
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
    released_ = false;
    departure_.emplace(a_, runway, speeds, to_ft);
    departure_->limit_speed(fastest_kts_);
    departure_->hand_mixture(applied_.mixture);
    // **With no plan after it, she levels off at its height** (sim::
    // Departure::level_off_at_end); a plan climbs on to its own.
    departure_->level_off_at_end(true);
}

void Controller::to_ai_flying(FlightPlan plan, const DepartureSpeeds& speeds) {
    if (!plan.takeoff) {
        to_ai(std::move(plan));
        return;
    }
    const FlightPlan::TakeOff takeoff = *plan.takeoff;
    to_ai_take_off(takeoff.runway, speeds, takeoff.to_ft);
    departure_->level_off_at_end(false);
    navigator_.emplace(a_, to_its_point(std::move(plan)));
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
    // **And off the runway once stopped**, as an AI's landing is
    // (vacates_runways): without the runway, a player's aircraft the learnt
    // landing had stopped stayed on it, and the next player on final came
    // down to find her there (2026-11-11).
    gate_runway_ = runway;
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
    selector_at_take_back_.reset();
    if (stovl_) {
        selector_at_take_back_ = pilot_.stovl;
        lever_at_selection_ = pilot_.throttle;
        // Held in STOVL mode by the take-back: not a selection to latch at.
        selector_was_ = true;
    }
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
    holding_out_.reset();
    asks_the_circuit_cleared_ = false;
    rejoin_.reset();
    // **Her place in a circuit's sequence is kept**: taken over in the
    // circuit, she is still ahead of the one behind until she is down
    // (below) or handed back to the AI (`engage`).
    vacate_.reset();
}

double Controller::sequence_now() const {
    return sequence_clock_ ? sequence_clock_() : a_.state().sim_time_s;
}

void Controller::go_around(std::string why) {
    // **From the learnt landing too**, while it is still in the air: an
    // approach lander takes her from where she is and flies the go-around,
    // as from a balloon - to the gate's runway at the speed for her weight,
    // or, handed the learnt landing directly (`to_ai_learnt_approach`), to
    // its own runway at its own speeds.
    if (learnt_ && learnt_->stage() == LearntLander::Stage::flying && !learnt_->touched()) {
        lander_.emplace(a_, gate_runway_ ? *gate_runway_ : learnt_->rollout().runway(),
                        gate_speeds_ ? *gate_speeds_ : learnt_->rollout().speeds());
        lander_->hand_mixture(applied_.mixture);
        learnt_.reset();
        easing_in_ = true;
    }
    if (lander_) {
        lander_->go_around(std::move(why));
    }
}

std::optional<Runway> Controller::departing() const {
    if (flying_ != Flying::ai || !departure_ || !released_ ||
        departure_->stage() == Departure::Stage::done) {
        return std::nullopt;
    }
    return departure_->runway();
}

std::optional<Runway> Controller::straight_in() const {
    if (flying_ != Flying::ai || circling_) {
        return std::nullopt;
    }
    if (lander_ && !lander_->touched() &&
        (lander_->stage() == Lander::Stage::approach || lander_->stage() == Lander::Stage::flare)) {
        return lander_->runway();
    }
    if (learnt_ && learnt_->stage() == LearntLander::Stage::flying && !learnt_->touched() &&
        gate_runway_) {
        return *gate_runway_;
    }
    return std::nullopt;
}

void Controller::join_the_sequence() {
    if (circling_ && same_runway(circling_->runway, lander_->runway())) {
        return;
    }
    const AircraftState s = a_.state();
    // From the caller's clock, which every aircraft shares: her own starts
    // when she was made.
    circling_ = CirclePlace{
        lander_->runway(), sequence_now(),
        along_runway_nm(lander_->runway(), s.latitude_deg, s.longitude_deg),
        lander_->runway().elevation_ft +
            GoAroundCircuit(a_, lander_->runway(), lander_->speeds()).circuit_ft()};
}

double Controller::burning_pps() const {
    double pps = 0.0;
    for (int e = 0; e < a_.engine_count(); ++e) {
        const std::string flow = "propulsion/engine[" + std::to_string(e) + "]/fuel-flow-rate-pps";
        if (a_.has_property(flow)) {
            pps += a_.property(flow);
        }
    }
    return pps;
}

double Controller::fuel_lasts_s() const {
    const double pps = burning_pps();
    return pps > 0.0 ? a_.property("propulsion/total-fuel-lbs") / pps
                     : std::numeric_limits<double>::infinity();
}

void Controller::measure_circuit_burn() {
    const double now_s = a_.state().sim_time_s;
    // Step by step only: a gap - out of the circuit and back into it - is
    // not flown in it.
    const double dt_s = now_s - circuit_burn_last_s_;
    if (dt_s > 0.0 && dt_s < 0.1) {
        circuit_burn_lbs_ += burning_pps() * dt_s;
        circuit_burn_s_ += dt_s;
    }
    circuit_burn_last_s_ = now_s;
}

double Controller::circuit_burn_pps() const {
    return circuit_burn_s_ > 0.0 ? circuit_burn_lbs_ / circuit_burn_s_ : 0.0;
}

double Controller::going_burn_pps() const {
    return std::max(burning_pps(), circuit_burn_pps());
}

double Controller::fuel_to_go_lbs() const {
    return (minimum_fuel_s + fuel_back_s()) * going_burn_pps();
}

bool Controller::short_of_fuel() const {
    return a_.property("propulsion/total-fuel-lbs") < minimum_fuel_s * going_burn_pps();
}

double Controller::fuel_back_s() const {
    if (!holding_out_) {
        return 0.0;
    }
    const Runway& r = holding_out_->runway();
    const AircraftState s = a_.state();
    const double h = r.heading_deg * std::numbers::pi / 180.0;
    constexpr double out_m = 6.0 * 1852.0;
    const double six_lat =
        r.threshold_lat_deg - out_m * std::cos(h) / metres_per_degree_latitude(r.threshold_lat_deg);
    const double six_lon = r.threshold_lon_deg - out_m * std::sin(h) /
                                                     metres_per_degree_longitude(r.threshold_lat_deg);
    const double way_m = distance_m(s.latitude_deg, s.longitude_deg, six_lat, six_lon) + out_m;
    // No reference speed to fly it at: no time known, so none is added.
    const double vref_kts = holding_out_->speeds().vref_kts;
    if (vref_kts <= 0.0) {
        return 0.0;
    }
    // **And her turns**, at a standard rate of 3 degrees a second: from her
    // heading to the six-mile point, and there onto the final. Without
    // them, the A320 landed with 1,805 lb of her 1,806 lb reserve and the
    // F-35B 1,315 of 1,332 (2026-11-11).
    const auto off_deg = [](double a_deg, double b_deg) {
        return std::abs(std::remainder(a_deg - b_deg, 360.0));
    };
    const double to_six_deg =
        bearing_deg(s.latitude_deg, s.longitude_deg, six_lat, six_lon);
    const double turns_s =
        (off_deg(to_six_deg, s.heading_deg) + off_deg(r.heading_deg, to_six_deg)) / 3.0;
    return way_m / (vref_kts * 1852.0 / 3600.0) + turns_s;
}

bool Controller::runway_not_clear(const Runway& runway) const {
    return runway_clear_ && a_.state().height_above_ground_ft < RunwayClear::decide_ft &&
           !runway_clear_(runway);
}

Controls Controller::fly() {
    // Her pilot's, with a place in a circuit's sequence: the one behind is
    // kept over the highest she is flown.
    // Her place is marked clear of departures as a pilot flies a downwind
    // leg (sim::clear_of_departures_by_hand), afresh every step, and marked
    // a person's.
    if (circling_) {
        circling_->by_hand = flying_ == Flying::pilot;
    }
    if (flying_ == Flying::pilot && circling_) {
        const AircraftState s = a_.state();
        circling_->circuit_alt_ft = std::max(circling_->circuit_alt_ft, s.altitude_ft);
        circling_->clear_of_departures = clear_of_departures_by_hand(circling_->runway, s);
        // **Flown away from the circuit, she has left it**
        // (sim::left_the_circuit_by_hand): her place is dropped, and the
        // one behind, holding outside, comes back to land.
        if (left_the_circuit_by_hand(circling_->runway, s)) {
            circling_.reset();
        }
    }
    if (a_.property("gear/wow") > 0.5) {
        // Down, whoever flies her: out of the circuit's sequence.
        circling_.reset();
        asks_the_circuit_cleared_ = false;
        const AircraftState s = a_.state();
        wheels_down_at_s_ = s.sim_time_s;
        wheels_down_agl_ft_ = a_.property("position/h-agl-ft");
        wheels_down_lat_deg_ = s.latitude_deg;
        wheels_down_lon_deg_ = s.longitude_deg;
        wheels_down_alt_ft_ = s.altitude_ft;
        // Her track over the ground; her heading if she is barely moving.
        const double v_north = a_.property("velocities/v-north-fps");
        const double v_east = a_.property("velocities/v-east-fps");
        const double track_deg = std::hypot(v_north, v_east) > 1.0
                                     ? std::atan2(v_east, v_north) * 180.0 / std::numbers::pi
                                     : s.heading_deg;
        wheels_down_track_deg_ = std::fmod(track_deg + 360.0, 360.0);
        highest_since_wheels_ft_ = s.altitude_ft;
    } else {
        highest_since_wheels_ft_ = std::max(highest_since_wheels_ft_, a_.state().altitude_ft);
    }
    lift_.reset();
    if (a_.has_lift_system()) {
        if (const std::optional<Controls> lifted = fly_lift_system()) {
            return *lifted;
        }
    }
    if (flying_ == Flying::ai) {
        // **Down, she is out of the circuit's sequence**: the one behind
        // may turn base.
        if (circling_ && (vacate_ || (lander_ && lander_->touched()) ||
                          (learnt_ && learnt_->touched()))) {
            circling_.reset();
        }
        // **Into the sequence as she goes around**: her place is from the
        // moment she does - her first go-around; round again, she keeps it -
        // and, two in one step, the one further down the runway first.
        if (lander_ && lander_->stage() == Lander::Stage::go_around) {
            join_the_sequence();
            why_gone_around_ = lander_->why_gone_around();
        }
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
            // **Held for the circuit's sequence** before her roll: standing
            // with the brakes on and the power off.
            if (!released_ && departure_->stage() == Departure::Stage::roll && hold_ &&
                hold_(departure_->runway())) {
                applied_.throttle = 0.0;
                applied_.left_brake = 1.0;
                applied_.right_brake = 1.0;
                return applied_;
            }
            released_ = true;
            if (departure_->stage() != Departure::Stage::done) {
                // Taken over at lift-off, its controls reached from the
                // pilot's at a hand's pace, as a landing's are.
                const Controls takeoff = departure_->fly();
                if (easing_in_) {
                    easing_in_ = !towards(applied_, takeoff);
                } else {
                    applied_ = takeoff;
                }
                return applied_;
            }
            easing_in_ = false;
            const std::optional<double> level_ft =
                departure_->levels_off() ? std::optional<double>(departure_->level_off_ft())
                                         : std::nullopt;
            departure_.reset();
            autopilot_.emplace(a_, applied_);
            // **Handed over short of her height, she is levelled off at it**,
            // not where she was handed over: closed on at up to the climb she
            // was handed over with, not stepped down to 700 ft/min, and not
            // eased (below) - the lead is the easing. Eased, a C182 went
            // 132 ft past; stepped, the F-22A 852.
            if (level_ft) {
                AutopilotModes modes = autopilot_->modes();
                modes.altitude_ft = *level_ft;
                modes.vertical_speed_fpm = std::max(
                    modes.vertical_speed_fpm, a_.property("velocities/h-dot-fps") * 60.0);
                autopilot_->set(modes);
                autopilot_->capture();
            }
            // **A light aeroplane's climb is eased down to the plan's**
            // (Autopilot::ease_climb): handed over at full throttle on her
            // best-climb floor. A jet climbs away at thousands of feet a
            // minute, which 25 ft/min a second would take minutes to bring
            // down; the F-22A, eased, climbed past her orbit's height to
            // 14,000 ft.
            if (a_.climb_floor_kts() && !level_ft) {
                autopilot_->ease_climb(a_.property("velocities/h-dot-fps") * 60.0);
            }
            if (navigator_) {
                navigator_->begin_here();
            }
        }
        // **Traffic close ahead on final: an early go-around**, as the
        // Airplane Flying Handbook has it (sim::close_ahead_on_approach).
        if (learnt_ && learnt_->stage() == LearntLander::Stage::flying && !learnt_->touched() &&
            gate_runway_ && close_ahead_ && close_ahead_(*gate_runway_)) {
            go_around("traffic close ahead on final");
            return fly();
        }
        if (learnt_ && learnt_->stage() == LearntLander::Stage::flying && !learnt_->touched() &&
            gate_runway_ && runway_not_clear(*gate_runway_)) {
            // **The runway not clear on short final: go around.**
            go_around();
            return fly();
        }
        // **The learnt landing is judged by the stabilized-approach gate**
        // (StabilizedApproach), as the approach autopilot is: two seconds
        // running off its speed or bound past the touchdown zone, below
        // 500 ft, and she goes around (LearntLander::unstabilized).
        if (learnt_ && learnt_->stage() == LearntLander::Stage::flying && !learnt_->touched() &&
            !learnt_->unstabilized().empty()) {
            std::string why = learnt_->unstabilized();
            ++unstable_go_arounds_;
            go_around(std::move(why));
            return fly();
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
        if (lander_ && !lander_->touched() && lander_->stage() == Lander::Stage::approach &&
            close_ahead_ && close_ahead_(lander_->runway())) {
            lander_->go_around("traffic close ahead on final");
        }
        if (lander_ && !lander_->touched() && lander_->stage() == Lander::Stage::approach &&
            departure_ahead_ && departure_ahead_(lander_->runway())) {
            lander_->go_around("a person departing ahead");
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
                // Gone around and done with it in one step - from high up,
                // traffic close ahead (`spaced_with`) - she is in the
                // sequence all the same.
                join_the_sequence();
                why_gone_around_ = lander_->why_gone_around();
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
            measure_circuit_burn();
            // **Behind a person who stays in the circuit, she leaves it**
            // (`leaves_with`): on her outbound leg, a whole racetrack flown,
            // for a hold outside it (sim::HoldOutside), keeping her place.
            if (behind_a_person_ &&
                !priority_ && circuit_->racetracks() > racetracks_behind_a_person &&
                circuit_->leg() == GoAroundCircuit::Leg::outbound && behind_a_person_()) {
                left_the_circuit_s_ = a_.state().sim_time_s;
                holding_out_.emplace(a_, circuit_->runway(), circuit_->speeds(),
                              circuit_->runway().elevation_ft + circuit_->circuit_ft());
                circuit_.reset();
                navigator_.reset();
                return fly();
            }
            // **Following traffic ahead in the sequence**: raised over its
            // circuit, and down the downwind leg until it is down.
            // And a straight-in on the approach to her runway, until she
            // turns base.
            circuit_->follow(circuit_ahead_ ? circuit_ahead_() : std::nullopt,
                             straight_in_ && straight_in_(circuit_->runway()),
                             climbing_out_ && climbing_out_(circuit_->runway()));
            // The highest she flies it: where the autopilot overshoots her
            // circuit's height levelling off, the one behind keeps clear of
            // where she is.
            // Clear of a departure, on her downwind leg or past it and
            // 1.5 nm or more short of the threshold: one released then
            // climbs out ahead of the threshold, away from her, never nearer
            // than the minimum along the runway (sim::departure_held).
            {
                using Leg = GoAroundCircuit::Leg;
                const Leg leg = circuit_->leg();
                const AircraftState s = a_.state();
                circling_->clear_of_departures =
                    leg != Leg::upwind && leg != Leg::crosswind &&
                    (circling_->clear_of_departures ||
                     -along_runway_nm(circuit_->runway(), s.latitude_deg, s.longitude_deg) *
                             1852.0 >=
                         Separation::minimum_m);
            }
            circling_->circuit_alt_ft =
                std::max({circling_->circuit_alt_ft,
                          circuit_->runway().elevation_ft + circuit_->circuit_ft(),
                          a_.state().altitude_ft});
            autopilot_->set(circuit_->modes());
            notice_a_stall();
            autopilot_->limit_height(floor_ft_, ceiling_ft_);
            autopilot_->turn_away(away_deg_);
            autopilot_->limit_speed(fastest_kts_);
            applied_ = autopilot_->fly();
            circuit_->configure(applied_);
            return applied_;
        }
        // **Holding outside the circuit** until the person ahead is down,
        // and then **back to land**: the legs to final (6 and 4 nm out on
        // the extended centreline) and the approach, her place kept so
        // that she is still ahead of anything behind her.
        if (holding_out_) {
            const bool short_of_it = short_of_fuel();
            // **Out of her reach, she diverts** (`diverts_with`), asked once.
            if (holding_out_->out_of_reach() && alternates_ && !alternates_asked_ &&
                !short_of_it) {
                alternates_asked_ = true;
                const double within_s = std::min(fuel_lasts_s(), 1.0e9) - minimum_fuel_s;
                const AircraftState here = a_.state();
                diverted_to_ = choose_diversion(
                    a_, holding_out_->runway(), holding_out_->speeds(),
                    alternates_(here.latitude_deg, here.longitude_deg,
                                std::max(0.0, holding_out_->speed_kts() * 1852.0 / 3600.0 *
                                                  within_s)),
                    holding_out_->left_at_alt_ft(), holding_out_->altitude_ft(),
                    alternate_needs_m_, holding_out_->speed_kts(), within_s,
                    alternate_elevation_);
                if (diverted_to_) {
                    // Out of the sequence she held for: she is not coming
                    // back to it.
                    circling_.reset();
                    FlightPlan way = final_legs(diverted_to_->runway, holding_out_->speeds());
                    Waypoint join;
                    join.name = "DIVERT";
                    join.latitude_deg = diverted_to_->join_lat_deg;
                    join.longitude_deg = diverted_to_->join_lon_deg;
                    join.altitude_ft = diverted_to_->route_alt_ft;
                    join.airspeed_kts = holding_out_->speed_kts();
                    way.waypoints.insert(way.waypoints.begin(), join);
                    rejoined_at_s_ = a_.state().sim_time_s;
                    rejoin_.emplace(Rejoin{diverted_to_->runway, holding_out_->speeds()});
                    navigator_.emplace(a_, std::move(way));
                    navigator_->begin_here();
                    diverting_en_route_ = true;
                    holding_out_.reset();
                }
            }
        }
        if (holding_out_) {
            const bool short_of_it = short_of_fuel();
            // **Out of her reach with nowhere to divert, she comes back at
            // once**, as short of fuel.
            const bool nowhere = holding_out_->out_of_reach();
            // **Down first, where she is**: given up high, she comes down in
            // right turns over the ground she climbed over to the height she
            // left her circuit at, so that she joins the legs to final at
            // their height - from 6,000 ft an F-22A came in 5 nm long and
            // went around.
            if (nowhere && !holding_out_->come_down_to_leaving()) {
                autopilot_->set(holding_out_->modes());
                notice_a_stall();
                autopilot_->limit_height(floor_ft_, ceiling_ft_);
                autopilot_->turn_away(away_deg_);
                autopilot_->limit_speed(fastest_kts_);
                applied_ = autopilot_->fly();
                holding_out_->configure(applied_);
                return applied_;
            }
            // **With nowhere to divert, she waits for the circuit to be
            // cleared** (`asks_the_circuit_cleared`): come down, she circles
            // where she is, her height the one she left her circuit at, and
            // asks for the person ahead to be told to leave the circuit - a
            // tower moving other traffic out of the pattern for a fuel
            // emergency - until that person's place is dropped
            // (sim::left_the_circuit_by_hand) or she is short of fuel; then
            // back with priority. Her final crosses any downwind the person
            // flies: coming back while the person stayed, all 15 landplanes
            // lost the minimum (2026-11-11).
            // **Waiting, until her fuel on landing would fall below the
            // reserve** (ICAO's MAYDAY FUEL; AIM 5-5-15's emergency fuel):
            // the reserve, and the time back to the threshold at her
            // approach's speed (`fuel_back_s`).
            const bool must_go =
                nowhere && a_.property("propulsion/total-fuel-lbs") < fuel_to_go_lbs();
            if (behind_a_person_ && behind_a_person_() && !short_of_it && !must_go) {
                asks_the_circuit_cleared_ = nowhere;
                autopilot_->set(holding_out_->modes());
                notice_a_stall();
                autopilot_->limit_height(floor_ft_, ceiling_ft_);
                autopilot_->turn_away(away_deg_);
                autopilot_->limit_speed(fastest_kts_);
                applied_ = autopilot_->fly();
                holding_out_->configure(applied_);
                return applied_;
            }
            // Short of fuel with a person ahead, she comes back at once and
            // still asks, until she is down: the person is told to leave.
            const bool emergency = short_of_it || must_go;
            asks_the_circuit_cleared_ = emergency && behind_a_person_ && behind_a_person_();
            rejoined_at_s_ = a_.state().sim_time_s;
            came_back_short_ = emergency;
            came_back_out_of_reach_ = nowhere && !emergency;
            // **Coming back short of fuel or with nowhere to go, she has
            // priority** (AIM 5-5-15's emergency fuel; ICAO's MAYDAY FUEL):
            // first in her runway's sequence, followed and not following,
            // and never sent out to hold again.
            if ((short_of_it || nowhere) && circling_) {
                circling_->since_s = -std::numeric_limits<double>::infinity();
                priority_ = true;
            }
            rejoin_.emplace(Rejoin{holding_out_->runway(), holding_out_->speeds()});
            navigator_.emplace(a_, final_legs(rejoin_->runway, rejoin_->speeds));
            navigator_->begin_here();
            holding_out_.reset();
        }
        if (rejoin_ && navigator_ && navigator_->finished()) {
            lander_.emplace(a_, rejoin_->runway, rejoin_->speeds);
            lander_->hand_mixture(applied_.mixture);
            navigator_.reset();
            rejoin_.reset();
            applied_ = lander_->fly();
            return applied_;
        }
        if (rejoin_ && navigator_) {
            // The legs to final flown at the approach's speed with half the
            // landing flap, as the circuit's are, until the approach puts
            // out what the landing needs.
            autopilot_->set(navigator_->steer());
            notice_a_stall();
            autopilot_->limit_height(floor_ft_, ceiling_ft_);
            autopilot_->turn_away(away_deg_);
            autopilot_->limit_speed(fastest_kts_);
            applied_ = autopilot_->fly();
            // Diverting, the way there is flown as her hold was, cleaned
            // up, until she has joined the extended centreline.
            diverting_en_route_ = diverting_en_route_ && navigator_->next() == 0;
            if (diverting_en_route_) {
                applied_.flaps = 0.0;
                applied_.speedbrake = 0.0;
                if (a_.gear_retracts()) {
                    applied_.gear = 0.0;
                }
                return applied_;
            }
            applied_.flaps = 0.5 * rejoin_->speeds.flap;
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
            } else if (modes.airspeed_kts && !on_final_legs_) {
                // **No slower than her plan's slowest for what she weighs**
                // (plans_within); then, **in gusts or turbulence, half the
                // gust factor faster** (sim::in_gusts), up to her plan's
                // fastest: a plan's or a route's speed, as her climb out
                // is. The legs to final are the approach's, and flown at
                // its speed.
                double kts = *modes.airspeed_kts;
                if (plan_speeds_) {
                    kts = std::max(
                        kts, for_weight(*plan_speeds_, a_.property("inertia/weight-lbs")).slowest_kts);
                }
                modes.airspeed_kts = in_gusts(kts, a_.gust_factor_kt(), fastest_kts_);
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

bool put_lift_system(Aircraft& aircraft, const LiftSystem& lift) {
    if (!aircraft.has_lift_system()) {
        return false;
    }
    if (lift.stovl > 0.0 && aircraft.property("velocities/vc-kts") >
                                aircraft.property("fcs/stovl-limit-kcas")) {
        LiftSystem closed = lift;
        closed.stovl = 0.0;
        aircraft.set_lift_system(closed);
        return false;
    }
    aircraft.set_lift_system(lift);
    return true;
}

Controls apply(Aircraft& aircraft, Controller& controller) {
    const Controls flown = controller.fly();
    aircraft.set_controls(flown);
    if (const LiftSystem* lift = controller.lift()) {
        put_lift_system(aircraft, *lift);
    }
    return flown;
}

} // namespace glideslope::sim
