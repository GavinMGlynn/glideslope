#include "sim/transition.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace glideslope::sim {

namespace {

constexpr double dt = 1.0 / static_cast<double>(steps_per_second);
constexpr double pi = 3.14159265358979323846;
constexpr double rad_per_deg = pi / 180.0;
constexpr double deg_per_rad = 180.0 / pi;
constexpr double kt_per_fps = 3600.0 / 6076.12;
constexpr double fps_per_kt = 6076.12 / 3600.0;
// Sea-level density, slugs a cubic foot: a calibrated speed's dynamic
// pressure, low and slow, as near as matters.
constexpr double sea_level_slug_ft3 = 0.0023769;

// The speed under which, the speed asked come to nothing, the hover law
// takes her; the most the speed asked runs past her conversion speed
// gathering speed, so that she reaches it; and how far short of her least
// conversion height she may be and still convert.
constexpr double hover_kt = 3.0;
constexpr double past_conversion_kt = 20.0;
constexpr double height_slack_ft = 2.0;

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

// **Her pilot's stick jet-borne**: the roll rate it asks at its stop, and the
// most bank it may hold - choices of this law, the bank under the 30 degrees
// her verification allows (nothing published gives the F-35B's); and
// gravity, feet a second a second.
constexpr double jet_roll_most_degps = 10.0;
constexpr double jet_bank_most_deg = 20.0;
constexpr double gravity_fps2 = 32.174;

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
    least_ft_ = a_.property("fcs/stovl-convert-least-ft");
    stovl_throttle_ = a_.property("fcs/stovl-throttle-max");
    nozzle_most_deg_ = a_.property("fcs/nozzle-max-deg");
    fan_least_deg_ = a_.property("fcs/fan-min-deg");
    fan_most_deg_ = a_.property("fcs/fan-max-deg");
    slow_kt_per_s_ = a_.property("fcs/transition-slow-kt-per-s");
    gather_kt_per_s_ = a_.property("fcs/transition-gather-kt-per-s");
    gathering_pitch_deg_ = a_.property("fcs/transition-gathering-pitch-deg");
    lift_slope_per_rad_ = a_.property("fcs/transition-lift-slope-per-rad");
    trc_kt_ = a_.property("fcs/stovl-trc-kt");
    trc_aft_kt_ = a_.property("fcs/stovl-trc-aft-kt");
    trc_side_kt_ = a_.property("fcs/stovl-trc-side-kt");
    turn_degps_ = a_.property("fcs/stovl-turn-degps");
    heading_deg_ = a_.state().heading_deg;
    const double conversion = a_.property("fcs/stovl-pos-norm");
    const bool nozzle_down = a_.property("fcs/nozzle-pos-deg") >= nozzle_down_deg - 0.5;
    if (a_.property("gear/wow") > 0.5) {
        stage_ = Stage::hovering;
        hover_.emplace(a_);
        hover_base_ft_ = a_.state().height_above_ground_ft;
    } else if (conversion >= 1.0 && nozzle_down && ground_kt(a_) < hover_kt) {
        become_hovering(flying);
    } else if (conversion > 0.0) {
        become_jet_borne(flying);
    } else {
        stage_ = Stage::wing_borne;
        autopilot_.emplace(a_, flying.controls);
    }
}

Transition::Carried Transition::carried() const {
    Carried out;
    if (autopilot_) {
        out.pitch_integral_deg = autopilot_->pitch_integral_deg();
    }
    return out;
}

void Transition::carry(const Carried& carried) {
    if (autopilot_ && carried.pitch_integral_deg) {
        autopilot_->put_pitch_integral_deg(*carried.pitch_integral_deg);
    }
}

double Transition::wing_borne_pitch_deg() const {
    const double v_fps = convert_kcas_ * fps_per_kt;
    const double q_psf = 0.5 * sea_level_slug_ft3 * v_fps * v_fps;
    return a_.property("inertia/weight-lbs") /
           (q_psf * a_.property("metrics/Sw-sqft") * lift_slope_per_rad_) * deg_per_rad;
}

namespace {

constexpr double earth_radius_ft = 20902231.0;

// Feet north and east of `here` to `there`, on a sphere, as the hover law
// reckons its drift.
std::pair<double, double> north_east_ft(double lat_deg, double lon_deg,
                                        const Transition::Point& there) {
    const double north = (there.latitude_deg - lat_deg) * rad_per_deg * earth_radius_ft;
    const double east = (there.longitude_deg - lon_deg) * rad_per_deg * earth_radius_ft *
                        std::cos(lat_deg * rad_per_deg);
    return {north, east};
}

} // namespace

double Transition::stops_within_ft(const Aircraft& aircraft, double kcas) {
    const double convert_kcas = aircraft.property("fcs/stovl-convert-kcas");
    const double v_fps = std::max(kcas, convert_kcas) * fps_per_kt;
    const double rate_fps2 = aircraft.property("fcs/transition-slow-kt-per-s") * fps_per_kt;
    return 1.5 * v_fps * v_fps / (2.0 * rate_fps2) + 30.0 * convert_kcas * fps_per_kt;
}

void Transition::steer_at(const Target& target) {
    if (!target.hover || !target.over) {
        return;
    }
    const AircraftState s = a_.state();
    const auto [north, east] = north_east_ft(s.latitude_deg, s.longitude_deg, *target.over);
    if (std::hypot(north, east) > steer_within_ft) {
        heading_deg_ = std::fmod(std::atan2(east, north) * deg_per_rad + 360.0, 360.0);
    }
}

std::optional<double> Transition::ahead_of_point_ft(const Target& target) const {
    if (!target.over) {
        return std::nullopt;
    }
    const AircraftState s = a_.state();
    const auto [north, east] = north_east_ft(s.latitude_deg, s.longitude_deg, *target.over);
    const double psi = heading_deg_ * rad_per_deg;
    return north * std::cos(psi) + east * std::sin(psi);
}

double Transition::conversion_height_ft(const Target& target) const {
    return std::max(target.height_ft, least_ft_);
}

void Transition::seed_jet_borne(const Hover::Command& flying, bool from_hover) {
    const AircraftState s = a_.state();
    speed_from_kt_ = std::max(ahead_kt(a_, heading_deg_), 0.0);
    speed_asked_kt_ = speed_from_kt_;
    pitch_from_deg_ = from_hover ? 0.0 : s.pitch_deg;
    back_pitch_from_deg_ = s.pitch_deg;
    throttle_integral_ = std::clamp(flying.controls.throttle, 0.0, stovl_throttle_);
    // The thrust's angle the command had: the nozzle's, until it is at its
    // stop, and past it the fan's.
    const double angle = flying.lift.nozzle_deg < nozzle_most_deg_
                             ? flying.lift.nozzle_deg
                             : flying.lift.fan_deg;
    angle_integral_deg_ =
        from_hover ? nozzle_down_deg : std::clamp(angle, 0.0, fan_most_deg_);
    elevator_integral_ = flying.controls.elevator;
    fan_integral_ = std::clamp(flying.lift.fan, -0.5, 0.5);
    post_integral_ = std::clamp(flying.lift.roll_post, -0.5, 0.5);
}

void Transition::become_jet_borne(const Hover::Command& flying) {
    stage_ = Stage::jet_borne;
    autopilot_.reset();
    const bool from_hover = hover_.has_value();
    hover_.reset();
    heading_deg_ = a_.state().heading_deg;
    bank_held_deg_ = 0.0;
    seed_jet_borne(flying, from_hover);
}

void Transition::become_hovering(const Hover::Command& flying) {
    stage_ = Stage::hovering;
    autopilot_.reset();
    Hover::Command handed = flying;
    handed.lift.fan_deg = nozzle_down_deg;
    hover_.emplace(a_, handed);
    hover_base_ft_ = a_.state().height_above_ground_ft;
}

void Transition::fly_lever(double lever) {
    const double u = std::clamp(lever, -1.0, 1.0);
    if (speed_asked_kt_ > trc_kt_ || (speed_asked_kt_ >= trc_kt_ && u > 0.0)) {
        // Above her translational rate command: an acceleration.
        speed_asked_kt_ = std::min(
            speed_asked_kt_ + u * (u > 0.0 ? gather_kt_per_s_ : slow_kt_per_s_) * dt,
            convert_kcas_);
        return;
    }
    // Under it: a ground speed, reached at her rates.
    const double wanted = u * (u >= 0.0 ? trc_kt_ : trc_aft_kt_);
    speed_asked_kt_ = wanted > speed_asked_kt_
                          ? std::min(speed_asked_kt_ + gather_kt_per_s_ * dt, wanted)
                          : std::max(speed_asked_kt_ - slow_kt_per_s_ * dt, wanted);
}

Hover::Command Transition::step(const Target& target) {
    const double kcas = a_.property("velocities/vc-kts");
    const double conversion = a_.property("fcs/stovl-pos-norm");
    const double height = a_.state().height_above_ground_ft;
    const bool high_enough = height >= least_ft_ - height_slack_ft;
    const bool reversed =
        last_asked_hover_.has_value() && *last_asked_hover_ != target.hover;
    last_asked_hover_ = target.hover;
    // Steered wing-borne only: jet-borne her law flies her speed along her
    // heading and nothing across it, so a turn there leaves her drifting
    // sideways at what she had.
    if (stage_ == Stage::wing_borne || (stage_ == Stage::converting && autopilot_)) {
        steer_at(target);
    }
    // Where the ask sends her from the stage she is in.
    switch (stage_) {
    case Stage::wing_borne:
        if (target.hover && kcas <= convert_kcas_ + 2.0 && high_enough) {
            stage_ = Stage::converting;
        }
        break;
    case Stage::converting:
        // Which way the fan's doors go is the ask's alone.
        if (target.hover && conversion >= 1.0) {
            become_jet_borne(last_);
        } else if (!target.hover && conversion <= 0.0) {
            stage_ = Stage::wing_borne;
            if (!autopilot_) {
                autopilot_.emplace(a_, last_.controls);
            }
        } else if (reversed && !autopilot_) {
            seed_jet_borne(last_, false);
        }
        break;
    case Stage::jet_borne:
        if (reversed) {
            seed_jet_borne(last_, false);
        }
        // At her conversion speed, and high enough, she converts back, the
        // jet-borne law still flying her until the fan has stopped.
        if (!target.hover && kcas >= convert_kcas_ && high_enough) {
            stage_ = Stage::converting;
            back_pitch_from_deg_ = a_.state().pitch_deg;
        } else if (target.hover && speed_asked_kt_ <= 0.0 && ground_kt(a_) < hover_kt &&
                   a_.property("fcs/nozzle-pos-deg") >= nozzle_down_deg - 0.5) {
            become_hovering(last_);
            if (target.over) {
                hover_->hold_over(target.over->latitude_deg, target.over->longitude_deg);
            }
        }
        break;
    case Stage::hovering:
        if (!target.hover && a_.property("gear/wow") < 0.5) {
            become_jet_borne(last_);
        } else if (target.hover && target.pilot && target.pilot->lever &&
                   a_.property("gear/wow") < 0.5) {
            // The pilot's lever moves the hover, and past her translational
            // rate command she is jet-borne again, gathering speed.
            fly_lever(*target.pilot->lever);
            if (speed_asked_kt_ > trc_kt_) {
                become_jet_borne(last_);
            }
        } else {
            speed_asked_kt_ = 0.0;
        }
        break;
    }

    Hover::Command c;
    switch (stage_) {
    case Stage::wing_borne: c = fly_wing_borne(target, false); break;
    case Stage::converting:
        c = autopilot_ ? fly_wing_borne(target, target.hover)
                       : fly_jet_borne(target, !target.hover);
        break;
    case Stage::jet_borne: c = fly_jet_borne(target, false); break;
    case Stage::hovering: {
        Hover::Target asked{target.height_ft - hover_base_ft_, target.land, 0.0, 0.0, 0.0};
        if (target.over && !target.pilot) {
            // **Over a point not yet under her, she is moved to it in the
            // hover** at her translational rate command's speed, slowing at
            // her own rate to stop over it: the point the hover law holds
            // moves to it, and the hover law is told how fast.
            const auto [north, east] =
                north_east_ft(hover_->latitude_deg(), hover_->longitude_deg(), *target.over);
            const double away_ft = std::hypot(north, east);
            const double v_kt = std::min(
                trc_kt_, std::sqrt(2.0 * slow_kt_per_s_ * fps_per_kt * away_ft) * kt_per_fps);
            if (v_kt * fps_per_kt * dt >= away_ft || v_kt < 0.5) {
                hover_->hold_over(target.over->latitude_deg, target.over->longitude_deg);
            } else {
                const double bearing = std::atan2(east, north);
                const double psi = hover_->heading_deg() * rad_per_deg;
                asked.ahead_kt = v_kt * std::cos(bearing - psi);
                asked.right_kt = v_kt * std::sin(bearing - psi);
            }
        }
        if (target.pilot) {
            asked.ahead_kt = target.pilot->lever ? speed_asked_kt_ : 0.0;
            asked.right_kt = std::clamp(target.pilot->side, -1.0, 1.0) * trc_side_kt_;
            asked.turn_degps = std::clamp(target.pilot->pedals, -1.0, 1.0) * turn_degps_;
        }
        c = hover_->step(asked);
        break;
    }
    }
    last_ = c;
    return c;
}

Hover::Command Transition::fly_wing_borne(const Target& target, bool converted) {
    const AircraftState s = a_.state();
    AutopilotModes modes;
    modes.heading_deg = heading_deg_;
    // Asked for a hover, or converting, at her least conversion height at
    // the lowest; and at her conversion speed, or no slower (the header says
    // why).
    const double height_ft = target.hover || stage_ == Stage::converting
                                 ? conversion_height_ft(target)
                                 : target.height_ft;
    modes.altitude_ft = s.altitude_ft + (height_ft - s.height_above_ground_ft);
    modes.airspeed_kts =
        target.hover ? convert_kcas_ : std::max(target.kcas, convert_kcas_);
    autopilot_->set(modes);
    Hover::Command c;
    c.controls = autopilot_->fly();
    c.controls.gear = 0.0;
    c.lift.stovl = converted ? 1.0 : 0.0;
    c.lift.nozzle_deg = 0.0;
    c.lift.fan_deg = fan_least_deg_;
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

Hover::Command Transition::fly_jet_borne(const Target& target, bool back) {
    const AircraftState s = a_.state();
    Hover::Command c = last_;
    // Converting back: the jet-borne law flies her on as the fan stops.
    c.lift.stovl = back ? 0.0 : 1.0;
    c.lift.split = 0.0;
    c.controls.gear = back ? 0.0 : 1.0;
    c.controls.left_brake = c.controls.right_brake = 0.0;

    // The speed asked, moving at her rate toward a hover or past her
    // conversion speed.
    if (target.hover && target.over) {
        // Over a point: no faster than she stops in by it, at her rate,
        // and never faster than she was. Past it, nothing.
        const double ahead_ft = std::max(*ahead_of_point_ft(target), 0.0);
        const double rate_fps2 = slow_kt_per_s_ * fps_per_kt;
        const double stops_kt = std::sqrt(2.0 * rate_fps2 * ahead_ft) * kt_per_fps;
        speed_asked_kt_ = stops_kt < hover_kt ? 0.0 : std::min(speed_asked_kt_, stops_kt);
    } else if (target.hover && target.pilot && target.pilot->lever) {
        fly_lever(*target.pilot->lever);
    } else if (target.hover) {
        speed_asked_kt_ = std::max(speed_asked_kt_ - slow_kt_per_s_ * dt, 0.0);
    } else {
        speed_asked_kt_ = std::min(speed_asked_kt_ + gather_kt_per_s_ * dt,
                                   convert_kcas_ + past_conversion_kt);
    }
    const double speed = ahead_kt(a_, heading_deg_);

    // Height, on the throttle: gathering speed, no lower than she converts.
    const double height_ft =
        target.hover ? target.height_ft : conversion_height_ft(target);
    const double wanted_fps = std::clamp(
        height_gain * (height_ft - s.height_above_ground_ft), -sink_fps, climb_fps);
    const double rate_error = wanted_fps - s.climb_rate_fpm / 60.0;
    throttle_integral_ = std::clamp(
        throttle_integral_ + rate_integral * rate_error * dt, 0.0, stovl_throttle_);
    const double throttle =
        std::clamp(throttle_integral_ + rate_gain * rate_error, 0.0, stovl_throttle_);
    c.controls.throttle = toward(c.controls.throttle, throttle, throttle_per_s * dt);

    // Speed, on the thrust's angle: too fast, swung further down; each
    // within its travel.
    const double speed_error = speed - speed_asked_kt_;
    angle_integral_deg_ = std::clamp(
        angle_integral_deg_ + angle_integral * speed_error * dt, 0.0, fan_most_deg_);
    double angle =
        std::clamp(angle_integral_deg_ + angle_gain * speed_error, 0.0, fan_most_deg_);
    // Come to a stop asked for a hover, straight down for the hover law.
    if (target.hover && speed_asked_kt_ <= 0.0 && ground_kt(a_) < hover_kt) {
        angle = nozzle_down_deg;
    }
    if (back) {
        angle = 0.0;
    }
    c.lift.nozzle_deg = std::min(angle, nozzle_most_deg_);
    c.lift.fan_deg = std::clamp(angle, fan_least_deg_, fan_most_deg_);

    // Attitude, on the stabilator and the fan's share together: slowing,
    // from the attitude she had when asked down to level as the speed goes;
    // gathering speed, toward her gathering attitude by her conversion
    // speed; converting back, toward her wing-borne attitude as the fan's
    // lift goes.
    double pitch_wanted_deg = 0.0;
    if (back) {
        const double gone =
            1.0 - std::clamp(a_.property("fcs/stovl-pos-norm"), 0.0, 1.0);
        pitch_wanted_deg = back_pitch_from_deg_ +
                           (wing_borne_pitch_deg() - back_pitch_from_deg_) * gone;
    } else if (target.hover) {
        pitch_wanted_deg = pitch_from_deg_ *
                           std::clamp(speed / std::max(speed_from_kt_, 1.0), 0.0, 1.0);
    } else {
        const double share = std::clamp(
            (speed - speed_from_kt_) / std::max(convert_kcas_ - speed_from_kt_, 1.0),
            0.0, 1.0);
        pitch_wanted_deg =
            pitch_from_deg_ + (gathering_pitch_deg_ - pitch_from_deg_) * share;
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

    // **Her pilot's stick sideways and pedals** (Target::pilot), asked for
    // a hover or to fly on jet-borne, never converting back: the stick a
    // roll rate, in proportion, to `jet_roll_most_degps`, and centred the
    // bank it left held - the VAAC Harrier's unified law, which hers
    // descend from - no further over than `jet_bank_most_deg`; the heading she holds
    // turning with the bank she has as a coordinated turn would - g tan
    // bank over her speed, no slower than her translational rate command's,
    // so that the turn stays bounded as she slows; the pedals a turn of the
    // heading, in proportion, to her hover's turn (`fcs/stovl-turn-degps`)
    // at her translational rate command's speed and less as the speed
    // grows, the same sideways acceleration - wings level, a skid the
    // speed law along her new heading takes up.
    double bank_wanted_deg = 0.0;
    double turn_radps = 0.0;
    if (target.hover && target.pilot && !back) {
        bank_held_deg_ = std::clamp(bank_held_deg_ + std::clamp(target.pilot->side, -1.0, 1.0) *
                                                         jet_roll_most_degps * dt,
                                    -jet_bank_most_deg, jet_bank_most_deg);
        bank_wanted_deg = bank_held_deg_;
        const double v_fps = std::max(speed, trc_kt_) * fps_per_kt;
        turn_radps = gravity_fps2 * std::tan(s.roll_deg * rad_per_deg) / v_fps +
                     std::clamp(target.pilot->pedals, -1.0, 1.0) * turn_degps_ * rad_per_deg *
                         std::pow(trc_kt_ / std::max(speed, trc_kt_), 2.0);
        heading_deg_ = std::fmod(heading_deg_ + turn_radps * deg_per_rad * dt + 360.0, 360.0);
    } else {
        // Levelled, by no pilot or converting back or her selector out of
        // STOVL mode: no bank is held over to when the stick is hers again.
        // Kept, the selector out and back in at 60 kt rolled her to the 20
        // degrees she had been left at.
        bank_held_deg_ = 0.0;
    }

    // Wings level, or the bank asked, on the ailerons and the posts.
    const double roll_error = (bank_wanted_deg - s.roll_deg) * rad_per_deg;
    c.controls.aileron =
        std::clamp(aileron_gain * (bank_wanted_deg - s.roll_deg) -
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
    const double yaw = yaw_gain * heading_error - yaw_rate_gain * (s.r_radps - turn_radps);
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
