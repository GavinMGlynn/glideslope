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
// **Round an orbit the heading asked for is kept ahead of the tangent by what
// the autopilot needs to bank for the circle**, atan(v_air v_ground / g r)
// (sim::heading_off_for_bank_deg); turned in towards the circle by the angle
// that would bring it back in twelve seconds at its airspeed, 45 degrees at
// most; and trimmed by an integral on that, a sixtieth of it a second, 15
// degrees at most, while it is turned in by 9 or less. Turned in by a fixed
// 90 degrees a kilometre, a jet at 360 kt swung 3.5 km either side of its
// 18.8 km circle; twenty seconds let a C172P at 100 kt run 67 m wide, and
// fifteen the S.23 at 86 kt 67 m inside (PROJECT_STATUS.md).
constexpr double orbit_closing_s = 12.0;
constexpr double most_orbit_intercept_deg = 45.0;
constexpr double orbit_trim_per_s = 1.0 / 60.0;
constexpr double most_orbit_trim_deg = 15.0;
constexpr double most_trimmed_in_deg = 9.0;
constexpr double gravity_mps2 = 9.80665;
// On an orbit's circle, and counting the turns round it: within this of it,
// either side. From outside it is flown to along the line that meets it at a
// tangent, the way round it is flown, so it is joined going round - flown
// straight at its centre, an aeroplane crossed it at right angles and could
// not turn onto it; from inside - a plan that flies to the orbit's centre
// first - the aircraft is steered out to it, and the turns spiralling out are
// not counted.
constexpr double orbit_joined_m = 100.0;
// **And going its way round**: its track within this of the circle's. Joined
// whichever way it was going, a jet that reached a waypoint on its circle
// heading across it was held to the circle from there, and turning round
// onto it swung kilometres through it (PROJECT_STATUS.md, 2026-10-09).
constexpr double orbit_joined_deg = 20.0;
// **The bank it is turned onto the circle at**, from inside it or swung
// outside it: less than the autopilot's most, 25 degrees, which the
// tightest orbit allowed - two and a half times the circle turned at 25 -
// leaves room for.
constexpr double capture_bank_deg = 15.0;

double normalised(double degrees) {
    const double d = std::fmod(degrees, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}

// **Where the air carries the aircraft, over the ground**: its velocity
// through the air, north and east, feet a second - the body's air-relative
// velocity turned through its attitude, so its sideslip and its climb through
// the air are counted, not its heading taken for where it goes.
struct AirVelocity {
    double north_fps = 0.0;
    double east_fps = 0.0;
};

AirVelocity air_velocity(const Aircraft& a) {
    const double u = a.property("velocities/u-aero-fps");
    const double v = a.property("velocities/v-aero-fps");
    const double w = a.property("velocities/w-aero-fps");
    const double phi = a.property("attitude/phi-deg") * radians;
    const double theta = a.property("attitude/theta-deg") * radians;
    const double psi = a.property("attitude/psi-deg") * radians;
    const double sf = std::sin(phi), cf = std::cos(phi);
    const double st = std::sin(theta), ct = std::cos(theta);
    const double ss = std::sin(psi), cs = std::cos(psi);
    AirVelocity out;
    out.north_fps = u * ct * cs + v * (sf * st * cs - cf * ss) + w * (cf * st * cs + sf * ss);
    out.east_fps = u * ct * ss + v * (sf * st * ss + cf * cs) + w * (cf * st * ss - sf * cs);
    return out;
}

} // namespace

Navigator::Navigator(const Aircraft& aircraft, FlightPlan plan)
    : a_(aircraft), plan_(std::move(plan)) {
    begin_here();
}

void Navigator::begin_here() {
    from_latitude_deg_ = a_.property("position/lat-geod-deg");
    from_longitude_deg_ = a_.property("position/long-gc-deg");
    last_track_deg_ = a_.property("attitude/psi-deg");
    // The wind as it is now, not calm: a plan taken over in the air, or
    // after a take-off, starts from the wind there.
    const AirVelocity air = air_velocity(a_);
    wind_north_fps_ = a_.property("velocities/v-north-fps") - air.north_fps;
    wind_east_fps_ = a_.property("velocities/v-east-fps") - air.east_fps;
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
    // The wind: where the aircraft goes against where the air takes it.
    // Unlike the drift it does not change as the aircraft turns, so round an
    // orbit it is what the heading allows for.
    const AirVelocity air = air_velocity(a_);
    const double air_fps = std::hypot(air.north_fps, air.east_fps);
    if (air_fps > least_speed_fps) {
        wind_north_fps_ += (north - air.north_fps - wind_north_fps_) * dt / drift_average_s;
        wind_east_fps_ += (east - air.east_fps - wind_east_fps_) * dt / drift_average_s;
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
            // Its track over the ground off the circle's, the way round it is
            // flown.
            const double along = to.orbit->right ? around + 90.0 : around - 90.0;
            const double track_off_deg =
                std::remainder(std::atan2(east, north) / radians - along, 360.0);
            if (!circling_ && std::abs(off_circle_m) <= orbit_joined_m &&
                std::abs(track_off_deg) <= orbit_joined_deg) {
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
            const double air_mps = air_fps * 0.3048;
            // Degrees turned in for each metre off the circle.
            const double in_per_metre =
                std::atan(1.0 / (std::max(air_mps, 1.0) * orbit_closing_s)) / radians;
            if (circling_ && std::abs(in_per_metre * off_circle_m) <= most_trimmed_in_deg) {
                trim_deg_ = std::clamp(trim_deg_ + orbit_trim_per_s * in_per_metre *
                                                       off_circle_m * dt,
                                       -most_orbit_trim_deg, most_orbit_trim_deg);
            }
            // Along the tangent, ahead of it by the heading the autopilot needs
            // to bank for the circle, turned in towards the circle - to the
            // right of the tangent, flying round to the right - or out. Round
            // a circle over the ground the track turns at the ground speed
            // over the radius, and the air is turned through that at the
            // airspeed: tan bank = v_air v_ground / g r.
            const double ground_mps = std::hypot(north, east) * 0.3048;
            // The rate the track turns at, round the way the orbit is flown:
            // the circle's.
            double in = std::clamp(in_per_metre * off_circle_m, -most_orbit_intercept_deg,
                                   most_orbit_intercept_deg);
            double turning = ground_mps / r;
            // **No more than it can turn out of onto the circle**: the angle
            // to it of the arc it would turn along at the capture bank, met
            // at a tangent - curving round the way the circle does from
            // inside, the other way from outside. Closing faster, it could
            // not turn onto the circle in time, and swung through it.
            const double turn_m = std::min(air_mps * ground_mps / (gravity_mps2 *
                                                                   std::tan(capture_bank_deg * radians)),
                                           0.9 * r);
            const double rho = std::max(from_centre_m, 1.0);
            const bool inside = off_circle_m < 0.0;
            const double centres_m = inside ? r - turn_m : r + turn_m;
            const double cos_at = std::clamp(
                (rho * rho + turn_m * turn_m - centres_m * centres_m) / (2.0 * rho * turn_m),
                -1.0, 1.0);
            const double most_in_deg =
                inside ? std::acos(cos_at) / radians : 180.0 - std::acos(cos_at) / radians;
            if (std::abs(in) > most_in_deg) {
                in = std::copysign(most_in_deg, in);
                // Along the arc.
                turning = std::copysign(gravity_mps2 * std::tan(capture_bank_deg * radians) / std::max(air_mps, 1.0), inside ? 1.0 : -1.0);
            }
            // Ahead by the heading the autopilot needs to bank for that turn:
            // over the ground the track turns at it, and the air is turned
            // through it at the airspeed - tan bank = v_air rate / g; round
            // the circle, v_air v_ground / g r.
            const double bank_deg = std::atan(air_mps * turning / gravity_mps2) / radians;
            const double lead_deg = heading_off_for_bank_deg(bank_deg);
            const double turned_in = lead_deg + in + trim_deg_;
            track = to.orbit->right ? around + 90.0 + turned_in : around - 90.0 - turned_in;
            off_m = 0.0;
            orbiting = true;
            modes.altitude_ft = to.altitude_ft;
            modes.airspeed_kts = to.airspeed_kts;
            break;
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
    if (orbiting && air_fps > least_speed_fps) {
        // The heading whose air velocity and the wind together go along the
        // track: turned into the wind across it.
        const double t = course * radians;
        const double across = wind_east_fps_ * std::cos(t) - wind_north_fps_ * std::sin(t);
        modes.heading_deg =
            normalised(course - std::asin(std::clamp(across / air_fps, -1.0, 1.0)) / radians);
    } else {
        modes.heading_deg = normalised(course - drift_deg_);
    }
    return modes;
}

} // namespace glideslope::sim
