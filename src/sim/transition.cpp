#include "sim/transition.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace glideslope::sim {

namespace {

constexpr double dt = 1.0 / static_cast<double>(steps_per_second);
constexpr double pi = 3.14159265358979323846;
constexpr double rad_per_deg = pi / 180.0;
constexpr double deg_per_rad = 180.0 / pi;
constexpr double kt_per_fps = 3600.0 / 6076.12;

// **How fast she slows and gathers speed jet-borne**, knots a second: not
// published - estimated. Two knots a second takes her from 200 knots to a
// hover in a hundred seconds, about a mile and a half; Code One's account
// gives the sequence and the speeds, not the time.
constexpr double slow_kt_per_s = 2.0;
constexpr double gather_kt_per_s = 3.0;

// The speed under which, the speed asked come to nothing, the hover law
// takes her; and the most the speed asked runs past her conversion speed
// accelerating, so that she reaches it.
constexpr double hover_kt = 3.0;
constexpr double past_conversion_kt = 20.0;

// **The attitudes she gathers speed at and converts back to**, degrees.
// Jet-borne she gathers speed toward `gathering_pitch_deg` by her
// conversion speed: low, so that her jets carry more of her and the
// throttle that holds her height gives her the thrust to accelerate - at 8
// degrees she stuck at 195 KCAS, at 11.5 at 166 (PROJECT_STATUS,
// 2026-10-13). Converting back, as the fan's lift goes her attitude is
// raised toward `wing_borne_pitch_deg`, what she is trimmed at wing-borne
// at her conversion speed and 150 ft - 11.7 degrees at her vertical-landing
// loading, measured on her model, not published. Slowing, she starts from
// the attitude she has as the conversion ends.
constexpr double gathering_pitch_deg = 4.0;
constexpr double wing_borne_pitch_deg = 11.7;

// The thrust's angle from straight aft, degrees: the nozzle swivels to 95
// (Rolls-Royce), the fan's vane box from 41.75 to 104 (Wiegand et al.,
// AIAA 2018-3368); both are swung to the one angle, each within its travel.
constexpr double nozzle_most_deg = 95.0;
constexpr double fan_least_deg = 41.75;
constexpr double fan_most_deg = 104.0;
constexpr double nozzle_down_deg = 90.0;

// The speed on the thrust's angle: degrees a knot too fast, and a knot a
// second.
constexpr double angle_gain = 2.0;
constexpr double angle_integral = 0.3;

// The height on the throttle: the hover law's (sim/hover.cpp).
constexpr double height_gain = 0.4;
constexpr double climb_fps = 10.0;
constexpr double sink_fps = 6.0;
constexpr double rate_gain = 0.03;
constexpr double rate_integral = 0.02;
constexpr double throttle_per_s = 0.5;

// The attitude: on the stabilator, the test pilot's gains (sim/test_pilot.cpp)
// a degree and a degree a second; on the fan's share, the hover law's a
// radian and a radian a second.
constexpr double elevator_gain = 0.05, elevator_rate_gain = 0.03,
                 elevator_integral = 0.02;
constexpr double pitch_gain = 8.0, pitch_rate_gain = 8.0, pitch_integral = 2.0;
constexpr double aileron_gain = 0.04, aileron_rate_gain = 0.02;
constexpr double roll_gain = 3.0, roll_rate_gain = 2.0, roll_integral = 1.0;
constexpr double yaw_gain = 2.0, yaw_rate_gain = 3.0;
constexpr double rudder_gain = 0.1;
constexpr double control_per_s = 2.0;

double toward(double from, double to, double most) {
    return from + std::clamp(to - from, -most, most);
}

double ahead_kt(const Aircraft& a, double heading_deg) {
    const double psi = heading_deg * rad_per_deg;
    return (a.property("velocities/v-north-fps") * std::cos(psi) +
            a.property("velocities/v-east-fps") * std::sin(psi)) *
           kt_per_fps;
}

double ground_kt(const Aircraft& a) {
    return std::hypot(a.property("velocities/v-north-fps"),
                      a.property("velocities/v-east-fps")) *
           kt_per_fps;
}

} // namespace

Transition::Transition(const Aircraft& aircraft, const Hover::Command& flying)
    : a_(aircraft), last_(flying) {
    if (!a_.has_lift_system()) {
        throw std::logic_error("only an aircraft with a lift system converts");
    }
    convert_kcas_ = a_.property("fcs/stovl-convert-kcas");
    stovl_throttle_ = a_.property("fcs/stovl-throttle-max");
    heading_deg_ = a_.state().heading_deg;
    const bool converted = a_.property("fcs/stovl-pos-norm") >= 1.0;
    const bool nozzle_down = a_.property("fcs/nozzle-pos-deg") >= nozzle_down_deg - 0.5;
    if (a_.property("gear/wow") > 0.5) {
        stage_ = Stage::hovering;
        hover_ = std::make_unique<Hover>(a_);
        hover_base_ft_ = a_.state().height_above_ground_ft;
    } else if (converted && nozzle_down && ground_kt(a_) < hover_kt) {
        become_hovering(flying);
    } else if (a_.property("fcs/stovl-pos-norm") > 0.0) {
        become_jet_borne(flying);
    } else {
        become_wing_borne(flying);
    }
}

void Transition::become_wing_borne(const Hover::Command& flying) {
    stage_ = Stage::wing_borne;
    hover_.reset();
    autopilot_ = std::make_unique<Autopilot>(a_, flying.controls);
}

void Transition::become_jet_borne(const Hover::Command& flying) {
    stage_ = Stage::jet_borne;
    autopilot_.reset();
    const bool from_hover = hover_ != nullptr;
    hover_.reset();
    heading_deg_ = a_.state().heading_deg;
    speed_from_kt_ = std::max(ahead_kt(a_, heading_deg_), 0.0);
    speed_asked_kt_ = speed_from_kt_;
    pitch_from_deg_ = from_hover ? 0.0 : a_.state().pitch_deg;
    throttle_integral_ = std::clamp(flying.controls.throttle, 0.0, stovl_throttle_);
    angle_integral_deg_ =
        from_hover ? nozzle_down_deg
                   : std::clamp(flying.lift.nozzle_deg, 0.0, nozzle_most_deg);
    elevator_integral_ = flying.controls.elevator;
    fan_integral_ = std::clamp(flying.lift.fan, -0.5, 0.5);
    post_integral_ = std::clamp(flying.lift.roll_post, -0.5, 0.5);
}

void Transition::become_hovering(const Hover::Command& flying) {
    stage_ = Stage::hovering;
    autopilot_.reset();
    Hover::Command handed = flying;
    handed.lift.fan_deg = nozzle_down_deg;
    hover_ = std::make_unique<Hover>(a_, handed);
    hover_base_ft_ = a_.state().height_above_ground_ft;
}

Hover::Command Transition::step(const Target& target) {
    const double kcas = a_.property("velocities/vc-kts");
    const double conversion = a_.property("fcs/stovl-pos-norm");
    // Where the target sends her from the stage she is in.
    switch (stage_) {
    case Stage::wing_borne:
        if (target.hover && kcas <= convert_kcas_ + 2.0) {
            stage_ = Stage::converting;
            converting_back_ = false;
        }
        break;
    case Stage::converting:
        converting_back_ = !target.hover && (converting_back_ || kcas >= convert_kcas_);
        if (target.hover && conversion >= 1.0) {
            become_jet_borne(last_);
        } else if (!target.hover && conversion <= 0.0) {
            stage_ = Stage::wing_borne;
            if (!autopilot_) {
                autopilot_ = std::make_unique<Autopilot>(a_, last_.controls);
            }
        }
        break;
    case Stage::jet_borne:
        // At her conversion speed she converts back, the jet-borne law
        // still flying her until the fan has stopped.
        if (!target.hover && kcas >= convert_kcas_) {
            stage_ = Stage::converting;
            converting_back_ = true;
        } else if (target.hover && speed_asked_kt_ <= 0.0 && ground_kt(a_) < hover_kt &&
                   a_.property("fcs/nozzle-pos-deg") >= nozzle_down_deg - 0.5) {
            become_hovering(last_);
        }
        break;
    case Stage::hovering:
        if (!target.hover && a_.property("gear/wow") < 0.5) {
            become_jet_borne(last_);
        }
        break;
    }

    Hover::Command c;
    switch (stage_) {
    case Stage::wing_borne: c = fly_wing_borne(target, false); break;
    case Stage::converting:
        c = autopilot_ ? fly_wing_borne(target, !converting_back_)
                       : fly_jet_borne(target);
        break;
    case Stage::jet_borne: c = fly_jet_borne(target); break;
    case Stage::hovering:
        c = hover_->step({target.height_ft - hover_base_ft_, false});
        break;
    }
    last_ = c;
    return c;
}

Hover::Command Transition::fly_wing_borne(const Target& target, bool converted) {
    const AircraftState s = a_.state();
    AutopilotModes modes;
    modes.heading_deg = heading_deg_;
    modes.altitude_ft = s.altitude_ft + (target.height_ft - s.height_above_ground_ft);
    modes.airspeed_kts = target.hover ? convert_kcas_ : target.kcas;
    autopilot_->set(modes);
    Hover::Command c;
    c.controls = autopilot_->fly();
    c.controls.gear = 0.0;
    c.lift.stovl = converted ? 1.0 : 0.0;
    c.lift.nozzle_deg = 0.0;
    c.lift.fan_deg = fan_least_deg;
    // The fan is ahead of her centre of gravity, and its thrust follows the
    // throttle the autopilot moves for the speed: while it turns, its share
    // holds the attitude the autopilot asks, as the jet-borne law's does.
    if (a_.property("fcs/stovl-pos-norm") > 0.0) {
        const double pitch_error =
            (autopilot_->pitch_asked_deg() - s.pitch_deg) * rad_per_deg;
        fan_integral_ =
            std::clamp(fan_integral_ + pitch_integral * pitch_error * dt, -0.5, 0.5);
        const double fan =
            fan_integral_ + pitch_gain * pitch_error - pitch_rate_gain * s.q_radps;
        c.lift.fan =
            toward(last_.lift.fan, std::clamp(fan, -1.0, 1.0), control_per_s * dt);
    }
    return c;
}

Hover::Command Transition::fly_jet_borne(const Target& target) {
    const AircraftState s = a_.state();
    Hover::Command c = last_;
    // Converting back: the jet-borne law flies her on as the fan stops.
    const bool back = stage_ == Stage::converting && converting_back_;
    c.lift.stovl = back ? 0.0 : 1.0;
    c.lift.split = 0.0;
    c.controls.gear = back ? 0.0 : 1.0;
    c.controls.left_brake = c.controls.right_brake = 0.0;

    // The speed asked, moving at its rate toward a hover or her conversion
    // speed and past it.
    if (target.hover) {
        speed_asked_kt_ = std::max(speed_asked_kt_ - slow_kt_per_s * dt, 0.0);
    } else {
        speed_asked_kt_ = std::min(speed_asked_kt_ + gather_kt_per_s * dt,
                                   convert_kcas_ + past_conversion_kt);
    }
    const double speed = ahead_kt(a_, heading_deg_);

    // Height, on the throttle.
    const double height = s.height_above_ground_ft;
    const double wanted_fps =
        std::clamp(height_gain * (target.height_ft - height), -sink_fps, climb_fps);
    const double rate_error = wanted_fps - s.climb_rate_fpm / 60.0;
    throttle_integral_ = std::clamp(
        throttle_integral_ + rate_integral * rate_error * dt, 0.0, stovl_throttle_);
    const double throttle =
        std::clamp(throttle_integral_ + rate_gain * rate_error, 0.0, stovl_throttle_);
    c.controls.throttle = toward(c.controls.throttle, throttle, throttle_per_s * dt);

    // Speed, on the thrust's angle: too fast, swung further down.
    const double speed_error = speed - speed_asked_kt_;
    angle_integral_deg_ = std::clamp(
        angle_integral_deg_ + angle_integral * speed_error * dt, 0.0, fan_most_deg);
    double angle =
        std::clamp(angle_integral_deg_ + angle_gain * speed_error, 0.0, fan_most_deg);
    // Come to a stop asked for a hover, straight down for the hover law.
    if (target.hover && speed_asked_kt_ <= 0.0 && ground_kt(a_) < hover_kt) {
        angle = nozzle_down_deg;
    }
    if (back) {
        angle = 0.0;
    }
    c.lift.nozzle_deg = std::min(angle, nozzle_most_deg);
    c.lift.fan_deg = std::clamp(angle, fan_least_deg, fan_most_deg);

    // Attitude: from the attitude she flies wing-borne to level as the speed
    // goes, on the stabilator and the fan's share together.
    const double pitch_top_deg = target.hover ? pitch_from_deg_ : gathering_pitch_deg;
    const double speed_top_kt =
        target.hover ? std::max(speed_from_kt_, 1.0) : convert_kcas_;
    double pitch_wanted_deg =
        pitch_top_deg * std::clamp(speed / speed_top_kt, 0.0, 1.0);
    if (back) {
        const double gone =
            1.0 - std::clamp(a_.property("fcs/stovl-pos-norm"), 0.0, 1.0);
        pitch_wanted_deg =
            gathering_pitch_deg + (wing_borne_pitch_deg - gathering_pitch_deg) * gone;
    }
    const double pitch_error_deg = pitch_wanted_deg - s.pitch_deg;
    const double q_degps = s.q_radps * deg_per_rad;
    elevator_integral_ = std::clamp(
        elevator_integral_ + elevator_integral * pitch_error_deg * dt, -1.0, 1.0);
    c.controls.elevator =
        std::clamp(elevator_integral_ + elevator_gain * pitch_error_deg -
                       elevator_rate_gain * q_degps,
                   -1.0, 1.0);
    const double pitch_error = pitch_error_deg * rad_per_deg;
    fan_integral_ =
        std::clamp(fan_integral_ + pitch_integral * pitch_error * dt, -0.5, 0.5);
    const double fan =
        fan_integral_ + pitch_gain * pitch_error - pitch_rate_gain * s.q_radps;

    // Wings level, on the ailerons and the posts.
    const double roll_error = -s.roll_deg * rad_per_deg;
    c.controls.aileron = std::clamp(aileron_gain * -s.roll_deg -
                                        aileron_rate_gain * s.p_radps * deg_per_rad,
                                    -1.0, 1.0);
    post_integral_ =
        std::clamp(post_integral_ + roll_integral * roll_error * dt, -0.5, 0.5);
    const double post =
        post_integral_ + roll_gain * roll_error - roll_rate_gain * s.p_radps;

    // Heading, on the nozzle's swivel; the ball, on the rudder as the air
    // over it allows.
    const double heading_error =
        std::remainder(heading_deg_ - s.heading_deg, 360.0) * rad_per_deg;
    const double yaw = yaw_gain * heading_error - yaw_rate_gain * s.r_radps;
    c.controls.rudder = std::clamp(-rudder_gain * a_.property("aero/beta-deg") *
                                       std::clamp(speed / 100.0, 0.0, 1.0),
                                   -1.0, 1.0);

    const double most = control_per_s * dt;
    c.lift.fan = toward(c.lift.fan, std::clamp(fan, -1.0, 1.0), most);
    c.lift.roll_post = toward(c.lift.roll_post, std::clamp(post, -1.0, 1.0), most);
    c.lift.nozzle_yaw = toward(c.lift.nozzle_yaw, std::clamp(yaw, -1.0, 1.0), most);
    return c;
}

} // namespace glideslope::sim
