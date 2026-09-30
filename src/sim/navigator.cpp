#include "sim/navigator.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <sstream>

namespace glideslope::sim {

namespace {

constexpr double earth_radius_m = 6371000.0;
constexpr double radians = std::numbers::pi / 180.0;
constexpr double dt = 1.0 / static_cast<double>(steps_per_second);
// Back towards the leg: 30 degrees for each kilometre off it, 30 at most.
constexpr double intercept_per_metre = 30.0 / 1000.0;
constexpr double most_intercept_deg = 30.0;
// The drift is averaged over five seconds, and measured only moving.
constexpr double drift_average_s = 5.0;
constexpr double least_speed_fps = 10.0;
// Towards an orbit's circle: 90 degrees for each kilometre off it, 45 at most;
// and along the tangent where the aircraft will be this long ahead, which the
// heading it is asked for takes about that long to become.
constexpr double orbit_intercept_per_metre = 90.0 / 1000.0;
constexpr double most_orbit_intercept_deg = 45.0;
constexpr double gravity_mps2 = 9.80665;
// On an orbit's circle, and counting the turns round it: within this of it.
// Reached from outside, that is where it is first crossed; from inside - a
// plan that flies to the orbit's centre first - the aircraft is steered out
// to it, and the turns spiralling out are not counted.
constexpr double orbit_joined_m = 100.0;
constexpr double orbit_trim_per_metre_s = 0.09 / 60.0;
constexpr double most_orbit_trim_deg = 15.0;

double normalised(double degrees) {
    const double d = std::fmod(degrees, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}

} // namespace

Navigator::Navigator(const Aircraft& aircraft, FlightPlan plan)
    : a_(aircraft), plan_(std::move(plan)) {
    from_latitude_deg_ = a_.property("position/lat-geod-deg");
    from_longitude_deg_ = a_.property("position/long-gc-deg");
    last_track_deg_ = a_.property("attitude/psi-deg");
}

void Navigator::begin_here() {
    from_latitude_deg_ = a_.property("position/lat-geod-deg");
    from_longitude_deg_ = a_.property("position/long-gc-deg");
    last_track_deg_ = a_.property("attitude/psi-deg");
}

AutopilotModes Navigator::steer() {
    const double lat = a_.property("position/lat-geod-deg");
    const double lon = a_.property("position/long-gc-deg");

    // The drift: where the aircraft goes against where it points.
    const double north = a_.property("velocities/v-north-fps");
    const double east = a_.property("velocities/v-east-fps");
    if (std::hypot(north, east) > least_speed_fps) {
        const double track = std::atan2(east, north) / radians;
        const double drift =
            std::remainder(track - a_.property("attitude/psi-deg"), 360.0);
        drift_deg_ += (drift - drift_deg_) * dt / drift_average_s;
    }
    // The wind: where the aircraft goes against where the air takes it, along
    // its heading at its true airspeed. Unlike the drift it does not change as
    // the aircraft turns, so round an orbit it is what the heading allows for.
    const double true_fps = a_.property("velocities/vtrue-fps");
    if (true_fps > least_speed_fps) {
        const double psi = a_.property("attitude/psi-deg") * radians;
        wind_north_fps_ += (north - true_fps * std::cos(psi) - wind_north_fps_) * dt / drift_average_s;
        wind_east_fps_ += (east - true_fps * std::sin(psi) - wind_east_fps_) * dt / drift_average_s;
    }
    bool orbiting = false;

    AutopilotModes modes;
    double track = last_track_deg_;
    double off_m = 0.0;
    while (!finished()) {
        const Waypoint& to = plan_.waypoints[next_];
        if (to.orbit) {
            const double from_centre_m =
                distance_m(to.latitude_deg, to.longitude_deg, lat, lon);
            const double around =
                bearing_deg(to.latitude_deg, to.longitude_deg, lat, lon);
            const double r = to.orbit->radius_m;
            const double off_circle_m = from_centre_m - r;
            if (!circling_ && std::abs(off_circle_m) <= orbit_joined_m) {
                circling_ = true;
                around_deg_ = around;
                turned_deg_ = 0.0;
                trim_deg_ = 0.0;
            }
            if (circling_) {
                // How far round, the way it is flown.
                const double moved = std::remainder(around - around_deg_, 360.0);
                turned_deg_ += to.orbit->right ? moved : -moved;
                around_deg_ = around;
                if (to.orbit->turns > 0 &&
                    turned_deg_ >= 360.0 * static_cast<double>(to.orbit->turns)) {
                    // Round as often as asked: on from here.
                    circling_ = false;
                    turned_deg_ = 0.0;
                    from_latitude_deg_ = lat;
                    from_longitude_deg_ = lon;
                    ++next_;
                    continue;
                }
            }
            if (!circling_ && off_circle_m > 0.0) {
                // Outside: along the line that meets the circle at a tangent,
                // the way round it is flown, so it is joined going its way.
                const double to_centre = bearing_deg(lat, lon, to.latitude_deg, to.longitude_deg);
                const double touch = std::asin(std::min(r / from_centre_m, 1.0)) / radians;
                track = to.orbit->right ? to_centre - touch : to_centre + touch;
                off_m = 0.0;
                orbiting = true;
                modes.altitude_ft = to.altitude_ft;
                modes.airspeed_kts = to.airspeed_kts;
                break;
            }
            {
                if (circling_ && std::abs(off_circle_m) <= orbit_joined_m) {
                    trim_deg_ = std::clamp(trim_deg_ + orbit_trim_per_metre_s * off_circle_m * dt,
                                           -most_orbit_trim_deg, most_orbit_trim_deg);
                }
                // Along the tangent a few seconds on, turned in towards the
                // circle - to the right of the tangent, flying round to the
                // right - or out.
                const double speed_mps = std::hypot(north, east) * 0.3048;
                const double bank_deg =
                    std::atan(speed_mps * speed_mps / (gravity_mps2 * to.orbit->radius_m)) /
                    radians;
                const double lead_deg = heading_off_for_bank_deg(bank_deg);
                const double in = std::clamp(
                    orbit_intercept_per_metre * (from_centre_m - to.orbit->radius_m),
                    -most_orbit_intercept_deg, most_orbit_intercept_deg);
                const double turned_in = lead_deg + in + trim_deg_;
                track = to.orbit->right ? around + 90.0 + turned_in : around - 90.0 - turned_in;
                off_m = 0.0;
                orbiting = true;
                modes.altitude_ft = to.altitude_ft;
                modes.airspeed_kts = to.airspeed_kts;
                break;
            }
        }
        // The leg, and where the aircraft is along and across it.
        const double leg = distance_m(from_latitude_deg_, from_longitude_deg_,
                                      to.latitude_deg, to.longitude_deg) /
                           earth_radius_m;
        const double flown =
            distance_m(from_latitude_deg_, from_longitude_deg_, lat, lon) /
            earth_radius_m;
        const double angle =
            (bearing_deg(from_latitude_deg_, from_longitude_deg_, lat, lon) -
             bearing_deg(from_latitude_deg_, from_longitude_deg_, to.latitude_deg,
                         to.longitude_deg)) *
            radians;
        const double across = std::asin(std::sin(flown) * std::sin(angle));
        const double along = std::copysign(
            std::acos(std::clamp(std::cos(flown) / std::cos(across), -1.0, 1.0)),
            std::cos(angle));
        const double ahead_m = (leg - along) * earth_radius_m;
        if (ahead_m <= 0.0) {
            // Abeam: passed.
            from_latitude_deg_ = to.latitude_deg;
            from_longitude_deg_ = to.longitude_deg;
            ++next_;
            continue;
        }
        off_m = across * earth_radius_m;
        // The leg's track where the aircraft is abeam of it: the bearing to the
        // waypoint turned back by the angle the offset makes.
        track = bearing_deg(lat, lon, to.latitude_deg, to.longitude_deg) +
                std::atan2(off_m, ahead_m) / radians;
        modes.altitude_ft = to.altitude_ft;
        modes.airspeed_kts = to.airspeed_kts;
        break;
    }
    if (finished()) {
        const Waypoint& last = plan_.waypoints.back();
        modes.altitude_ft = last.altitude_ft;
        modes.airspeed_kts = last.airspeed_kts;
        off_m = 0.0;
    }
    last_track_deg_ = track;
    const double course = track - std::clamp(intercept_per_metre * off_m,
                                             -most_intercept_deg, most_intercept_deg);
    if (orbiting && true_fps > least_speed_fps) {
        // The heading whose air velocity and the wind together go along the
        // track: turned into the wind across it.
        const double t = course * radians;
        const double across = wind_east_fps_ * std::cos(t) - wind_north_fps_ * std::sin(t);
        modes.heading_deg =
            normalised(course - std::asin(std::clamp(across / true_fps, -1.0, 1.0)) / radians);
    } else {
        modes.heading_deg = normalised(course - drift_deg_);
    }
    return modes;
}

} // namespace glideslope::sim
