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
constexpr double gravity_mps2 = 9.80665;
// **Round an orbit, and out to it from inside, the bank is asked for, not a
// heading**: a loiter law after ArduPilot's AP_L1_Control loiter (GPLv3, as
// this project is), itself from L1 guidance (Park, Deyst and How, "A New
// Nonlinear Guidance Logic for Trajectory Tracking", AIAA GNC 2004). Its
// structure is ArduPilot's rather than the paper's a = 2 V^2 sin(eta) / L1:
// the circle's centripetal acceleration, and a spring and a damper on how far
// off it is and how fast it moves off, set by a period and a damping ratio;
// the centripetal term's radius floored at half the circle's; and no pull
// back in while it goes round the wrong way. tan bank = (w^2 off + 2 zeta w
// outward + v_round^2 / (max(r / 2, d) cos crab)) / g, w = 2 pi / period:
// the circle over the ground asks v_round^2 / r square across the ground
// track, and the bank pulls square across the air's, the crab angle away -
// in calm air, ArduPilot's v_round^2 / r. (v_air v_round / r, the turn rate
// at the airspeed, was tried and measured worse: in a 20 kt wind the jets
// held their circles within 9 to 15 m with it, within 4 to 10 with this.)
// Its period is the time the heaviest
// aeroplane's bank needs: at 17 s, ArduPilot's for small aircraft, the
// 747-400 swung 7 km off its circle, at 25 s 1.9 km, and at 40 s it held
// within 8 m (PROJECT_STATUS.md, 2026-10-09). The heading law it replaced -
// turned in by what brought it back in twelve seconds, 45 degrees at most -
// asked a jet to turn faster than its heading loop, whose own time is about
// v / g, 11.5 s at 220 kt, could follow, and from a waypoint at the centre the
// 747-400 swung 6 km through its 7 km circle.
constexpr double loiter_period_s = 40.0;
constexpr double loiter_damping = 0.75;
// Outside the circle the loiter law has it within this, at most (below).
// Within its whole L1 distance - a kilometre for a jet at 220 kt - it took
// over the tangent line too soon, turned in at its most bank and swung a
// 747-400 350 m outside its circle after joining it; within 250 m every jet
// held within the 100 m it joined within.
constexpr double loiter_outside_m = 250.0;
// On an orbit's circle, and counting the turns round it: within this of it,
// either side, and going its way round (below). From outside it is flown to
// along the line that meets it at a tangent, the way round it is flown, so it
// is joined going round - flown straight at its centre, an aeroplane crossed
// it at right angles and could not turn onto it; from inside - a plan that
// flies to the orbit's centre first - the loiter law steers it out to it, and
// the turns spiralling out are not counted.
constexpr double orbit_joined_m = 100.0;
// **And going its way round**: its track within this of the circle's. Joined
// whichever way it was going, a jet that reached a waypoint on its circle
// heading across it was held to the circle from there, and turning round
// onto it swung kilometres through it (PROJECT_STATUS.md, 2026-10-09).
constexpr double orbit_joined_deg = 20.0;

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
            // The loiter law has it inside the circle, and outside it within
            // its L1 distance, zeta period v / pi, and loiter_outside_m, going
            // its way round; farther out, joined or not, the tangent line - L1
            // guidance's capture.
            // Flown along the tangent line that near, a Mosquito that had
            // crossed the circle not yet going its way followed the line round
            // 200 m outside it and never joined it; held by the loiter law
            // far outside, a Cherokee gliding at 30,000 ft, which could not
            // bank enough for her circle there, looped round and round 1.5 km
            // off it, never round its centre.
            const double ground_mps = std::hypot(north, east) * 0.3048;
            const double l1_m = loiter_damping * loiter_period_s * ground_mps / std::numbers::pi;
            const bool near_going_round =
                off_circle_m <= std::min(l1_m, loiter_outside_m) && std::abs(track_off_deg) < 90.0;
            if (off_circle_m > 0.0 && !near_going_round) {
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
            // On the circle, or inside it: the loiter law (above). Over the
            // ground, metres a second: outward from the centre, and round the
            // way the orbit is flown.
            const double v_north = north * 0.3048;
            const double v_east = east * 0.3048;
            const double out_north = std::cos(around * radians);
            const double out_east = std::sin(around * radians);
            const double outward_mps = v_north * out_north + v_east * out_east;
            const double way = to.orbit->right ? 1.0 : -1.0;
            const double round_mps = way * (v_east * out_north - v_north * out_east);
            const double omega = 2.0 * std::numbers::pi / loiter_period_s;
            double back = omega * omega * off_circle_m +
                          2.0 * loiter_damping * omega * outward_mps;
            if (round_mps < 0.0) {
                back = std::max(back, 0.0);
            }
            // Square across the ground track, which the bank - square across
            // the air's - meets at the crab angle between them.
            const double cos_crab = std::clamp(
                (air.north_fps * north + air.east_fps * east) /
                    std::max(air_fps * std::hypot(north, east), 1.0),
                0.5, 1.0);
            const double round_mps2 =
                round_mps * round_mps / (std::max(0.5 * r, from_centre_m) * cos_crab);
            modes.bank_deg = way * std::atan((back + round_mps2) / gravity_mps2) / radians;
            track = along;
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
    if (modes.bank_deg) {
        // Banked round an orbit: no heading, which would win over the bank.
        modes.heading_deg.reset();
    }
    return modes;
}

} // namespace glideslope::sim
