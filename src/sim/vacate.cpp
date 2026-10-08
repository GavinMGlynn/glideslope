#include "sim/vacate.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace glideslope::sim {

namespace {

constexpr double degrees = 180.0 / 3.14159265358979323846;
constexpr double fps_per_kt = 1.68781;


} // namespace

OnRunway on_runway_frame(const Runway& runway, double latitude_deg, double longitude_deg) {
    const double north_m = (latitude_deg - runway.threshold_lat_deg) *
                           metres_per_degree_latitude(runway.threshold_lat_deg);
    const double east_m = (longitude_deg - runway.threshold_lon_deg) *
                          metres_per_degree_longitude(runway.threshold_lat_deg);
    const double h = runway.heading_deg / degrees;
    return {north_m * std::cos(h) + east_m * std::sin(h),
            east_m * std::cos(h) - north_m * std::sin(h)};
}

bool on_runway(const Runway& runway, double latitude_deg, double longitude_deg,
               double height_ft) {
    const OnRunway at = on_runway_frame(runway, latitude_deg, longitude_deg);
    return at.along_m >= -400.0 && at.along_m <= runway.length_m &&
           std::abs(at.across_m) <= RunwayClear::occupied_half_width_m &&
           height_ft <= RunwayClear::occupied_ft;
}

bool beside_runway(const Runway& runway, double latitude_deg, double longitude_deg,
                   double height_ft, double along_m, double side) {
    const OnRunway at = on_runway_frame(runway, latitude_deg, longitude_deg);
    const double out_m = side * at.across_m;
    return height_ft <= RunwayClear::occupied_ft &&
           out_m > RunwayClear::occupied_half_width_m && out_m < 500.0 &&
           std::abs(at.along_m - along_m) < Vacate::spot_along_m;
}

Vacate::Vacate(const Aircraft& aircraft, const Runway& runway, const Controls& controls,
               SpotFree spot_free)
    : a_(aircraft), runway_(runway), c_(controls), spot_free_(std::move(spot_free)) {
    c_.speedbrake = 0.0;
    c_.elevator = 0.0;
    c_.aileron = 0.0;
    throttle_integral_ = 0.0;
}

Controls Vacate::fly() {
    const AircraftState s = a_.state();
    const double rolling_kts = a_.property("velocities/vg-fps") / fps_per_kt;
    const OnRunway at = on_runway_frame(runway_, s.latitude_deg, s.longitude_deg);
    constexpr double dt = 1.0 / static_cast<double>(steps_per_second);

    if (stage_ == Stage::rolling_on) {
        if (!spot_free_ || spot_free_(runway_, at.along_m, 1.0)) {
            stage_ = Stage::turning_off;
        } else if (at.along_m >= runway_.length_m - turn_by_end_m) {
            // **Out of runway with the right side taken: the left**, if it
            // is free; if neither is, the right all the same.
            if (spot_free_(runway_, at.along_m, -1.0)) {
                side_ = -1.0;
            }
            stage_ = Stage::turning_off;
        }
    }
    if (stage_ == Stage::turning_off && side_ * at.across_m >= RunwayClear::clear_m) {
        stage_ = Stage::stopping;
    }
    if (stage_ == Stage::stopping && rolling_kts < 0.3) {
        stage_ = Stage::clear;
    }
    if (stage_ == Stage::stopping || stage_ == Stage::clear) {
        // **Stopped gently, and then held**: the power off, the brakes a
        // fifth on until she is all but still - full brakes at a taxi's
        // speed stood a Mosquito on her nose - and then full on.
        c_.throttle = 0.0;
        c_.rudder = 0.0;
        const double brake = rolling_kts < 0.5 ? 1.0 : 0.2;
        c_.left_brake = brake;
        c_.right_brake = brake;
        return c_;
    }

    // **Steered off to the right**, by the law the landing's roll holds the
    // centreline with (sim/lander.cpp): the model's rudder yaws the nose
    // left for a positive command, and past half its travel the brake on
    // the side the nose is wanted comes in.
    // **The turn led, not snatched**: the heading she is steered to moves
    // round from the runway's at `turn_degps`, and only while she rolls. Asked
    // for the whole ninety degrees from a standstill, a Cessna stood with
    // her nosewheel hard over and the inside brake on, and the power she
    // was given never broke her away.
    // And never more than `lead_deg` ahead of where her nose is: at a
    // walking pace a light aeroplane's nosewheel turns her slowly, and a
    // heading run on ahead had the inside brake on until she stopped again.
    const double turned_deg =
        side_ * std::remainder(s.heading_deg - runway_.heading_deg, 360.0);
    if (stage_ == Stage::turning_off && rolling_kts > 2.0) {
        led_deg_ = std::min({led_deg_ + turn_degps * dt, turn_off_deg, turned_deg + lead_deg});
    }
    // Rolling on, the centreline: back to it over 30 m, no more than ten
    // degrees off.
    const double want_deg =
        stage_ == Stage::rolling_on
            ? runway_.heading_deg - std::clamp(std::atan(at.across_m / 30.0) * degrees, -10.0, 10.0)
            : runway_.heading_deg + side_ * led_deg_;
    const double error = std::remainder(want_deg - s.heading_deg, 360.0);
    const double r_degps = s.r_radps * degrees;
    c_.rudder = -std::clamp(0.10 * error - 0.30 * r_degps, -1.0, 1.0);

    // **At a walking pace**: the throttle on the speed over the ground, and
    // the brakes where idle power alone is too much - a jet's idle thrust
    // rolls her faster than a taxi.
    const double short_kts = taxi_kts - rolling_kts;
    throttle_integral_ = std::clamp(throttle_integral_ + 0.01 * short_kts * dt, 0.0, 0.8);
    c_.throttle = std::clamp(throttle_integral_ + 0.03 * short_kts, 0.0, 0.8);
    const double brake = std::clamp(-0.15 * (short_kts + 2.0), 0.0, 1.0);
    // The brake steers only while she rolls: at a standstill it holds her
    // there.
    const double steer =
        rolling_kts > 3.0 ? std::clamp((std::abs(c_.rudder) - 0.5) / 0.5, 0.0, 1.0) : 0.0;
    const double inside = std::clamp(brake + 0.6 * steer, 0.0, 1.0);
    const double outside = brake * (1.0 - steer);
    c_.left_brake = c_.rudder > 0.0 ? inside : outside;
    c_.right_brake = c_.rudder > 0.0 ? outside : inside;
    return c_;
}

} // namespace glideslope::sim
