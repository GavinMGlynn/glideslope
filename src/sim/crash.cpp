#include "sim/crash.hpp"

#include <cmath>
#include <cstdio>

namespace glideslope::sim {

namespace {
constexpr double feet_per_metre = 3.28083989501312;
}

std::optional<std::string> GroundJudge::judge(const Aircraft& aircraft) {
    const AircraftState s = aircraft.state();
    const Aircraft::Contact c = aircraft.contact();
    const bool on_water = aircraft.in_water();
    const bool touching = c.wheels || c.airframe || on_water;
    const double sink_before_fps = sink_fps_;
    sink_fps_ = -s.climb_rate_fpm / 60.0;
    if (!started_) {
        started_ = true;
        touching_ = touching;
        return std::nullopt;
    }
    const bool just_touched = touching && !touching_;
    touching_ = touching;

    // A landplane on water is ditched (sim::Aircraft); a flying boat's hull
    // on water reads as in_water and is no crash.
    if (s.ditched) {
        return std::string("came down on water");
    }
    if (c.airframe && !(water_is_home_ && on_water)) {
        return std::string("struck the ground with its airframe");
    }
    if (just_touched && sink_before_fps > gear_takes_fps) {
        char why[64];
        std::snprintf(why, sizeof why, "hit the ground sinking at %.0f ft/min",
                      sink_before_fps * 60.0);
        return std::string(why);
    }
    return std::nullopt;
}

bool collided(const double a_m[3], double a_span_ft, const double b_m[3], double b_span_ft) {
    const double dx = a_m[0] - b_m[0];
    const double dy = a_m[1] - b_m[1];
    const double dz = a_m[2] - b_m[2];
    const double apart_m = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double reach_m = (a_span_ft + b_span_ft) / 2.0 / feet_per_metre;
    return apart_m < reach_m;
}

} // namespace glideslope::sim
