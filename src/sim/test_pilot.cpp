#include "sim/test_pilot.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <numbers>

namespace glideslope::sim {

namespace {

constexpr double dt = 1.0 / static_cast<double>(steps_per_second);

double clamp_unit(double x) {
    return std::clamp(x, -1.0, 1.0);
}

double degrees(double radians) {
    return radians * 180.0 / std::numbers::pi;
}

} // namespace

double TestPilot::pitch_to(double theta_deg) {
    const double error = theta_deg - a_.property("attitude/theta-deg");
    const double q_degps = degrees(a_.property("velocities/q-rad_sec"));
    trim_ = std::clamp(trim_ + 0.02 * error * dt, -1.0, 1.0);
    return clamp_unit(trim_ + 0.05 * error - 0.03 * q_degps);
}

double TestPilot::pitch_for_speed(double kcas) {
    const double error = a_.property("velocities/vc-kts") - kcas;
    speed_integral_ = std::clamp(speed_integral_ + error * dt, -1000.0, 1000.0);
    return std::clamp(0.6 * error + 0.03 * speed_integral_, -15.0, 20.0);
}

double TestPilot::pitch_for_altitude(double altitude_ft) {
    const double error = altitude_ft - a_.property("position/h-sl-ft");
    const double climb_fps = a_.property("velocities/h-dot-fps");
    altitude_integral_ = std::clamp(altitude_integral_ + error * dt, -3000.0, 3000.0);
    return std::clamp(0.01 * error - 0.08 * climb_fps + 0.0005 * altitude_integral_,
                      -10.0, 15.0);
}

double TestPilot::roll_to(double phi_deg) const {
    const double phi = a_.property("attitude/phi-deg");
    const double p_degps = degrees(a_.property("velocities/p-rad_sec"));
    return clamp_unit(0.04 * (phi_deg - phi) - 0.02 * p_degps);
}

double TestPilot::coordinate() {
    const double beta = a_.property("aero/beta-deg");
    rudder_integral_ = std::clamp(rudder_integral_ - 0.05 * beta * dt, -1.0, 1.0);
    return clamp_unit(-0.1 * beta + rudder_integral_);
}

} // namespace glideslope::sim
