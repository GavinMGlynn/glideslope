#include "sim/navigator.hpp"

#include "sim/fixed_step.hpp"
#include "sim/turn_laws.hpp"

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
// Back towards the leg: 30 degrees for each kilometre off it
// (intercept_deg_per_metre, sim/turn_laws.hpp), 30 at most.
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

// **A waypoint with a leg after it is flown by, not over**: the turn onto
// the next leg begins before the waypoint, so that it ends on that leg
// rather than carried past it - a fly-by waypoint, which ICAO defines as "a
// waypoint which requires turn anticipation to allow tangential interception
// of the next segment" (Doc 8168, PANS-OPS), and whose distance of turn
// anticipation FAA Order 8260-44A gives as DTA = r tan(A / 2), A the course
// change, r the turn's radius: the arc of radius r tangent to both legs
// leaves this one r tan(A / 2) before the waypoint and meets the next as far
// past it. Here r is her turn at fly_by_bank_deg (sim/turn_laws.hpp), V^2 /
// (g tan 15), V her speed over the ground, and the turn begins earlier by
// half the time the bank takes to come at bank_rate_degps, as the turn lags
// an arc flown from the moment it begins by that much.
//
// **The arc is flown, not the heading at its end**: round its centre by the
// loiter law, which asks the bank the circle needs and corrects her onto it.
// Turned by the heading law instead - a degree of bank a degree off, at most
// 25 - the turn could only be started at the right moment, and no moment
// suited every aeroplane: the Cherokee at 74 kt, slow to roll, strayed a
// fifth of her turn's radius past the leg, and a jet at 300 kt, the heading
// law's roll-out and the leg's intercept together too lightly damped at her
// speed, a tenth (PROJECT_STATUS.md, 2026-10-13). Flown at 15 degrees, 10
// under the autopilot's most - the bank PANS-OPS builds departure and
// en-route turns at - the loiter law has those 10 to pull her back onto an
// arc her roll lag let her drift off. At 20 the airliners strayed two to
// three times as far. The arc is left once her track is within what her
// roll-out still turns her through of the next leg's: wings level is asked,
// and the leg's law takes her the rest of the way.
//
// **The last waypoint, one an orbit follows, and one marked `flyover` are
// flown over** (sim/plan.hpp), passed abeam as before; so is a turn too small
// to need an arc, within twice what the roll-out turns through.
//
// **A turn of more than 120 degrees is flown by on a tighter arc**, not
// flown over: 120 is the most a fly-by turn may be in PANS-OPS' RNAV
// criteria, and r tan(A / 2) grows without bound towards 180. Past 120 the
// arc keeps the lead the 120-degree turn has, r tan 60, its radius shrinking
// as the turn grows - down to the radius at the autopilot's most bank, 25
// degrees, which it reaches at 143 degrees - and from there the arc is the
// tightest she flies and the lead grows with it, up to its length at 160
// degrees, r(25) tan 80, and no more. So nothing jumps at any angle: until
// 2026-10-13 a turn of 120 degrees was led by 1.7 turn radii and one of 121
// not at all.
constexpr double widest_fly_by_deg = 120.0;
constexpr double longest_lead_deg = 160.0;
// **And no lead is more than half of either leg it joins**: led by more, the
// next waypoint's lead could begin before this one was reached, and a plan
// of short legs passed waypoints one after another on the same step. Capped,
// the arc is made tighter to fit, the loiter law asking what bank it needs,
// 25 at most.
constexpr double leg_share = 0.5;
// **And earlier again by the roll lag allowed for**: her bank lags the bank
// asked by up to this. Begun late, an aeroplane is outside her arc with less
// bank to spare to come back to it; begun early, inside it, the loiter law
// banks her less. Without it the Cherokee at 74 kt went past the leg
// further; at twice it every aeroplane strayed further, begun too early.
constexpr double roll_lag_allowed_s = 1.5;

double radius_at_m(double ground_mps, double bank_deg) {
    return ground_mps * ground_mps / (gravity_mps2 * std::tan(bank_deg * radians));
}

// What her heading still turns through as she rolls out of `bank_deg` at
// bank_rate_degps, degrees: half the time the roll-out takes at that bank's
// rate of turn, and as long again as her bank lags the bank asked,
// `roll_lag_s`.
double rolling_out_deg(double ground_mps, double bank_deg, double roll_lag_s = 0.0) {
    const double rate_degps =
        gravity_mps2 * std::tan(bank_deg * radians) / std::max(ground_mps, 1.0) / radians;
    return rate_degps * (0.5 * bank_deg / bank_rate_degps + roll_lag_s);
}

} // namespace

FlyBy fly_by(double ground_mps, double turn_deg, double in_m, double out_m, double wind_mps) {
    const double a = std::abs(turn_deg);
    if (a <= 2.0 * rolling_out_deg(ground_mps, fly_by_bank_deg) || a >= 179.0) {
        return {};
    }
    const double half = std::tan(0.5 * a * radians);
    const double widest_m = radius_at_m(ground_mps, fly_by_bank_deg);
    const double tightest_m = radius_at_m(ground_mps, most_bank_deg);
    FlyBy t;
    if (a <= widest_fly_by_deg) {
        t.radius_m = widest_m;
    } else {
        t.radius_m = std::max(widest_m * std::tan(0.5 * widest_fly_by_deg * radians) / half,
                              tightest_m);
    }
    t.meets_m = std::min(t.radius_m * half,
                         tightest_m * std::tan(0.5 * longest_lead_deg * radians));
    // **No fly-by passes its waypoint further off than most_fly_by_miss_m**
    // (sim/turn_laws.hpp) where she can help it: where its arc would, the arc
    // is tightened until it passes that near, r = miss / (1 / cos(A / 2) - 1),
    // and the lead with it - but no tighter than her arc at
    // fly_by_cap_bank_deg, the nearest she can pass and still turn onto the
    // next leg with bank to spare. Flown over instead, a jet's 90-degree turn
    // swung 1.1 turn radii past the next leg.
    // At the fastest she may go over the ground through the turn, her speed
    // through the air and the wind's together: worked at her speed as the
    // turn began, across a 20 kt wind, the 172P turning onto it from behind
    // needed more than 25 degrees and swung 0.142 of her radius past the leg.
    if (std::hypot(t.meets_m, t.radius_m) - t.radius_m > most_fly_by_miss_m) {
        const double r =
            std::max(most_fly_by_miss_m / (1.0 / std::cos(0.5 * a * radians) - 1.0),
                     radius_at_m(ground_mps + wind_mps, fly_by_cap_bank_deg));
        if (r < t.radius_m) {
            t.radius_m = r;
            t.meets_m = std::min(r * half, tightest_m * std::tan(0.5 * longest_lead_deg * radians));
        }
    }
    const double roll_in_m =
        ground_mps * (0.5 * fly_by_bank_deg / bank_rate_degps + roll_lag_allowed_s);
    t.lead_m = t.meets_m + roll_in_m;
    const double most_m = leg_share * std::min(in_m, out_m);
    if (t.lead_m > most_m) {
        t.lead_m = most_m;
        t.meets_m = std::max(most_m - roll_in_m, 0.0);
        t.radius_m = t.meets_m / half;
    }
    if (t.radius_m < 1.0) {
        return {};
    }
    return t;
}

namespace {

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

// **The loiter law's bank** (above), degrees, right wing down positive: for
// a circle of `radius_m` flown `right` or left, from `from_centre_m` off its
// centre on the bearing `around_deg` from it, going `north_fps` and
// `east_fps` over the ground with `air` its velocity through the air.
double loiter_bank_deg(double from_centre_m, double around_deg, double north_fps,
                       double east_fps, const AirVelocity& air, double radius_m, bool right) {
    // Over the ground, metres a second: outward from the centre, and round the
    // way the circle is flown.
    const double v_north = north_fps * 0.3048;
    const double v_east = east_fps * 0.3048;
    const double out_north = std::cos(around_deg * radians);
    const double out_east = std::sin(around_deg * radians);
    const double outward_mps = v_north * out_north + v_east * out_east;
    const double way = right ? 1.0 : -1.0;
    const double round_mps = way * (v_east * out_north - v_north * out_east);
    const double omega = 2.0 * std::numbers::pi / loiter_period_s;
    double back = omega * omega * (from_centre_m - radius_m) +
                  2.0 * loiter_damping * omega * outward_mps;
    if (round_mps < 0.0) {
        back = std::max(back, 0.0);
    }
    // Square across the ground track, which the bank - square across the
    // air's - meets at the crab angle between them.
    const double air_fps = std::hypot(air.north_fps, air.east_fps);
    const double cos_crab = std::clamp(
        (air.north_fps * north_fps + air.east_fps * east_fps) /
            std::max(air_fps * std::hypot(north_fps, east_fps), 1.0),
        0.5, 1.0);
    const double round_mps2 =
        round_mps * round_mps / (std::max(0.5 * radius_m, from_centre_m) * cos_crab);
    return way * std::atan((back + round_mps2) / gravity_mps2) / radians;
}

// Where `distance_m` along the great circle leaving (`latitude_deg`,
// `longitude_deg`) on `bearing` comes to, on the sphere.
struct Place {
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
};

Place along_from(double latitude_deg, double longitude_deg, double bearing, double distance) {
    const double d = distance / earth_radius_m;
    const double p1 = latitude_deg * radians;
    const double b = bearing * radians;
    const double p2 =
        std::asin(std::sin(p1) * std::cos(d) + std::cos(p1) * std::sin(d) * std::cos(b));
    const double dl = std::atan2(std::sin(b) * std::sin(d) * std::cos(p1),
                                 std::cos(d) - std::sin(p1) * std::sin(p2));
    return {p2 / radians, std::remainder(longitude_deg + dl / radians, 360.0)};
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
    turning_ = false;
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
                    turning_ = false;
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
            // On the circle, or inside it: the loiter law (above).
            modes.bank_deg = loiter_bank_deg(from_centre_m, around, north, east, air, r,
                                             to.orbit->right);
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
        // Flown by where a leg follows it (fly_by): the turn onto that leg
        // begins its lead short of it.
        const double ground_mps = std::hypot(north, east) * 0.3048;
        FlyBy turn;
        double turn_deg = 0.0;
        double outbound = 0.0;
        if (!to.fly_over && next_ + 1 < plan_.waypoints.size() &&
            !plan_.waypoints[next_ + 1].orbit) {
            const Waypoint& then = plan_.waypoints[next_ + 1];
            // The leg's course arriving at the waypoint, or, along a leg too
            // short to have one, her track over the ground.
            const double inbound =
                leg * earth_radius_m > 1.0
                    ? bearing_deg(to.latitude_deg, to.longitude_deg, from_latitude_deg_,
                                  from_longitude_deg_) +
                          180.0
                    : std::atan2(east, north) / radians;
            outbound =
                bearing_deg(to.latitude_deg, to.longitude_deg, then.latitude_deg, then.longitude_deg);
            turn_deg = std::remainder(outbound - inbound, 360.0);
            turn = fly_by(ground_mps, turn_deg, leg * earth_radius_m,
                          distance_m(to.latitude_deg, to.longitude_deg, then.latitude_deg,
                                     then.longitude_deg),
                          std::hypot(wind_north_fps_, wind_east_fps_) * 0.3048);
        }
        if (ahead_m <= turn.lead_m) {
            // Abeam, or as near as the turn onto the next leg begins: passed.
            turning_ = turn.lead_m > 0.0;
            if (turning_) {
                // The arc tangent to the next leg where it meets it, its
                // centre square to the leg from there, on the side turned to.
                turn_right_ = turn_deg > 0.0;
                turn_radius_m_ = turn.radius_m;
                turn_bank_deg_ = std::min(
                    std::atan(ground_mps * ground_mps / (gravity_mps2 * turn.radius_m)) / radians,
                    most_bank_deg);
                turn_lead_m_ = turn.lead_m;
                turn_s_ = 0.0;
                roll_lag_s_ = -1.0;
                rolling_out_ = false;
                const Place meets =
                    along_from(to.latitude_deg, to.longitude_deg, outbound, turn.meets_m);
                const Place centre = along_from(meets.latitude_deg, meets.longitude_deg,
                                                outbound + (turn_right_ ? 90.0 : -90.0),
                                                turn.radius_m);
                turn_centre_latitude_deg_ = centre.latitude_deg;
                turn_centre_longitude_deg_ = centre.longitude_deg;
            }
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
        // **Round a fly-by turn's arc** (fly_by) until her track is
        // within what her roll-out turns her through of the leg's, then wings
        // level until they are, or her track is within half a degree of the
        // leg's; or - should she never come round - until she is twice the
        // lead past its waypoint. Then the leg's law.
        if (turning_) {
            const double still_to_turn_deg =
                (turn_right_ ? 1.0 : -1.0) *
                std::remainder(track - std::atan2(east, north) / radians, 360.0);
            // **Her roll lag, measured as she rolls in**: how much longer
            // than the bank asked her bank took to come to half the arc's.
            // The Short S.23 at 100 kt took 1.5 s longer, and rolled out as
            // late: left the arc on the roll-out alone, she turned 16 degrees
            // past it against the 7 allowed for, and came onto the leg from
            // 79 m inside it (with the arc at 20 degrees).
            const double phi = std::abs(a_.property("attitude/phi-deg"));
            turn_s_ += dt;
            if (roll_lag_s_ < 0.0 && phi >= 0.5 * fly_by_bank_deg) {
                roll_lag_s_ = std::max(turn_s_ - 0.5 * fly_by_bank_deg / bank_rate_degps, 0.0);
            }
            if (rolling_out_ || still_to_turn_deg <= rolling_out_deg(ground_mps, turn_bank_deg_,
                                                                     std::max(roll_lag_s_, 0.0))) {
                // **Rolled out at the roll rate**, wings level asked, as
                // rolling_out_deg has it: handed to the heading law's bank of
                // a degree a degree off instead, she closed the last of her
                // turn slowly, crossing V^2 / g sin(what was left) towards
                // the leg against the arc's r (1 - cos(what was left)).
                rolling_out_ = true;
            }
            if (along * earth_radius_m >= 2.0 * turn_lead_m_ || still_to_turn_deg <= 0.5 ||
                (rolling_out_ && phi < 1.0)) {
                turning_ = false;
            } else if (rolling_out_) {
                modes.bank_deg = 0.0;
                off_m = 0.0;
            } else {
                modes.bank_deg = loiter_bank_deg(
                    distance_m(turn_centre_latitude_deg_, turn_centre_longitude_deg_, lat, lon),
                    bearing_deg(turn_centre_latitude_deg_, turn_centre_longitude_deg_, lat, lon),
                    north, east, air, turn_radius_m_, turn_right_);
                off_m = 0.0;
            }
        }
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
    const double course = track - std::clamp(intercept_deg_per_metre * off_m,
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
