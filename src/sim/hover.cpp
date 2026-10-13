#include "sim/hover.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace glideslope::sim {

namespace {

constexpr double dt = 1.0 / static_cast<double>(steps_per_second);
constexpr double pi = 3.14159265358979323846;
constexpr double rad_per_deg = pi / 180.0;
constexpr double earth_radius_ft = 20902231.0;

// The nozzle straight down, and the throttle's top in STOVL mode (her
// model's flight controls hold it there too: tools/make_f35b.py).
constexpr double nozzle_down_deg = 90.0;
constexpr double stovl_throttle = 0.99;

// Height: the climb or sink asked of the height's error, a second, within a
// climb and a sink; and the throttle on the rate's error, proportional and
// integral, a foot a second. Near the ground on a landing the sink asked
// falls to `touchdown_sink_fps`.
constexpr double height_gain = 0.4;
constexpr double climb_fps = 10.0;
constexpr double sink_fps = 6.0;
constexpr double touchdown_sink_fps = 3.0;
constexpr double flare_height_ft = 30.0;
constexpr double rate_gain = 0.03;
constexpr double rate_integral = 0.02;

// Attitude, a radian and a radian a second, each command's travel; with an
// integral that trims out a centre of gravity off the lift's centre.
constexpr double pitch_gain = 8.0, pitch_rate_gain = 8.0, pitch_integral = 2.0;
constexpr double roll_gain = 3.0, roll_rate_gain = 2.0, roll_integral = 1.0;
constexpr double yaw_gain = 2.0, yaw_rate_gain = 3.0;

// Position: the attitude asked, radians, a foot a second of drift and a foot
// from the point, and the most asked.
constexpr double drift_gain = 0.03;
constexpr double place_gain = 0.003;
constexpr double most_tilt_rad = 6.0 * rad_per_deg;

double toward(double from, double to, double most) {
    return from + std::clamp(to - from, -most, most);
}

} // namespace

Hover::Hover(const Aircraft& aircraft) : Hover(aircraft, Pace{}) {}

Hover::Hover(const Aircraft& aircraft, Pace pace) : a_(aircraft), pace_(pace) {
    if (!a_.has_lift_system()) {
        throw std::logic_error("only an aircraft with a lift system hovers");
    }
    const AircraftState s = a_.state();
    standing_ft_ = s.height_above_ground_ft;
    heading_deg_ = s.heading_deg;
    latitude_deg_ = s.latitude_deg;
    longitude_deg_ = s.longitude_deg;
    last_.controls.throttle = 0.0;
    last_.controls.gear = 1.0;
}

double Hover::height_ft() const {
    return a_.state().height_above_ground_ft - standing_ft_;
}

double Hover::drift_ft() const {
    const AircraftState s = a_.state();
    const double north = (s.latitude_deg - latitude_deg_) * rad_per_deg * earth_radius_ft;
    const double east = (s.longitude_deg - longitude_deg_) * rad_per_deg * earth_radius_ft *
                        std::cos(s.latitude_deg * rad_per_deg);
    return std::hypot(north, east);
}

Hover::Command Hover::step(const Target& target) {
    const AircraftState s = a_.state();
    const bool on_ground = a_.property("gear/wow") > 0.5;
    Command c = last_;
    c.lift.stovl = 1.0;
    c.lift.nozzle_deg = nozzle_down_deg;
    c.controls.gear = 1.0;
    c.controls.left_brake = c.controls.right_brake = 1.0;
    // Nothing is flown until the conversion is done.
    if (a_.property("fcs/stovl-pos-norm") < 1.0 || a_.property("fcs/nozzle-pos-deg") < nozzle_down_deg - 0.5) {
        c.controls.throttle = toward(c.controls.throttle, 0.0, pace_.throttle_per_s * dt);
        last_ = c;
        return c;
    }

    // Height.
    const double height = s.height_above_ground_ft - standing_ft_;
    double wanted_fps = std::clamp(height_gain * (target.height_ft - height), -sink_fps, climb_fps);
    if (target.land) {
        wanted_fps = height > flare_height_ft ? -sink_fps : -touchdown_sink_fps;
    }
    const double rate_error = wanted_fps - s.climb_rate_fpm / 60.0;
    if (!(target.land && on_ground)) {
        throttle_integral_ = std::clamp(throttle_integral_ + rate_integral * rate_error * dt, 0.0, stovl_throttle);
    }
    double throttle = std::clamp(throttle_integral_ + rate_gain * rate_error, 0.0, stovl_throttle);
    if (target.land && on_ground) {
        throttle = 0.0;
    }
    c.controls.throttle = toward(c.controls.throttle, throttle, pace_.throttle_per_s * dt);

    // Where it is and how it moves, along its heading and across it.
    const double psi = heading_deg_ * rad_per_deg;
    const double north = (s.latitude_deg - latitude_deg_) * rad_per_deg * earth_radius_ft;
    const double east = (s.longitude_deg - longitude_deg_) * rad_per_deg * earth_radius_ft *
                        std::cos(s.latitude_deg * rad_per_deg);
    const double v_north = a_.property("velocities/v-north-fps");
    const double v_east = a_.property("velocities/v-east-fps");
    const double ahead = north * std::cos(psi) + east * std::sin(psi);
    const double right = -north * std::sin(psi) + east * std::cos(psi);
    const double v_ahead = v_north * std::cos(psi) + v_east * std::sin(psi);
    const double v_right = -v_north * std::sin(psi) + v_east * std::cos(psi);

    // Tilted back to stop going ahead, and over to stop going right.
    const double pitch_wanted =
        std::clamp(drift_gain * v_ahead + place_gain * ahead, -most_tilt_rad, most_tilt_rad);
    const double roll_wanted =
        std::clamp(-(drift_gain * v_right + place_gain * right), -most_tilt_rad, most_tilt_rad);
    const double pitch_error = pitch_wanted - s.pitch_deg * rad_per_deg;
    const double roll_error = roll_wanted - s.roll_deg * rad_per_deg;
    const double heading_error = std::remainder(heading_deg_ - s.heading_deg, 360.0) * rad_per_deg;
    if (!on_ground) {
        fan_integral_ = std::clamp(fan_integral_ + pitch_integral * pitch_error * dt, -0.5, 0.5);
        post_integral_ = std::clamp(post_integral_ + roll_integral * roll_error * dt, -0.5, 0.5);
    }
    const double fan = on_ground ? fan_integral_
                                 : fan_integral_ + pitch_gain * pitch_error - pitch_rate_gain * s.q_radps;
    const double post = on_ground ? post_integral_
                                  : post_integral_ + roll_gain * roll_error - roll_rate_gain * s.p_radps;
    const double yaw = on_ground ? 0.0 : yaw_gain * heading_error - yaw_rate_gain * s.r_radps;
    const double most = pace_.control_per_s * dt;
    c.lift.fan = toward(c.lift.fan, std::clamp(fan, -1.0, 1.0), most);
    c.lift.roll_post = toward(c.lift.roll_post, std::clamp(post, -1.0, 1.0), most);
    c.lift.nozzle_yaw = toward(c.lift.nozzle_yaw, std::clamp(yaw, -1.0, 1.0), most);
    last_ = c;
    return c;
}

} // namespace glideslope::sim
