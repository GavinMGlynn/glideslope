#include "sim/circuit.hpp"
#include "sim/separation.hpp"
#include "sim/terrain.hpp"
#include "sim/vacate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace glideslope::sim {

namespace {

constexpr double degrees = 180.0 / 3.14159265358979323846;
constexpr double metres_per_nm = 1852.0;

// How far her heading is off `deg`, 0 to 180.
double off_deg(double heading_deg, double deg) {
    const double d = std::fmod(std::abs(heading_deg - deg), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

// Whether `other`, the `i`th, is ahead of `mine`, the `self`th, in a
// sequence: joined earlier; at the same time, further down the runway; and
// level there too, before her in the order.
bool ahead_in_sequence(const CirclePlace& other, std::size_t i, const CirclePlace& mine,
                       std::size_t self) {
    return other.since_s < mine.since_s ||
           (other.since_s == mine.since_s &&
            (other.along_nm > mine.along_nm || (other.along_nm == mine.along_nm && i < self)));
}

} // namespace

double along_runway_nm(const Runway& r, double latitude_deg, double longitude_deg) {
    const double north_m = (latitude_deg - r.threshold_lat_deg) *
                           metres_per_degree_latitude(r.threshold_lat_deg);
    const double east_m = (longitude_deg - r.threshold_lon_deg) *
                          metres_per_degree_longitude(r.threshold_lat_deg);
    const double h = r.heading_deg / degrees;
    return (north_m * std::cos(h) + east_m * std::sin(h)) / metres_per_nm;
}

double across_runway_m(const Runway& r, double latitude_deg, double longitude_deg) {
    const double north_m = (latitude_deg - r.threshold_lat_deg) *
                           metres_per_degree_latitude(r.threshold_lat_deg);
    const double east_m = (longitude_deg - r.threshold_lon_deg) *
                          metres_per_degree_longitude(r.threshold_lat_deg);
    const double h = r.heading_deg / degrees;
    return east_m * std::cos(h) - north_m * std::sin(h);
}

bool same_runway(const Runway& a, const Runway& b) {
    return a.name == b.name && a.threshold_lat_deg == b.threshold_lat_deg &&
           a.threshold_lon_deg == b.threshold_lon_deg;
}

std::optional<double> circuit_ahead_alt_ft(std::size_t self,
                                           const std::vector<std::optional<CirclePlace>>& all) {
    if (self >= all.size() || !all[self]) {
        return std::nullopt;
    }
    const CirclePlace& mine = *all[self];
    std::optional<double> highest;
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (i == self || !all[i] || !same_runway(all[i]->runway, mine.runway)) {
            continue;
        }
        if (ahead_in_sequence(*all[i], i, mine, self)) {
            highest = std::max(highest.value_or(all[i]->circuit_alt_ft), all[i]->circuit_alt_ft);
        }
    }
    return highest;
}

GoAroundCircuit::GoAroundCircuit(const Aircraft& aircraft, const Runway& runway,
                                 const ApproachSpeeds& speeds, const CircuitEntry& entry)
    // Her speeds for what she weighs (`for_weight`); already so from a
    // lander, and scaled again they are the same.
    : a_(aircraft), runway_(runway),
      speeds_(for_weight(speeds, aircraft.property("inertia/weight-lbs"))), entry_(entry),
      leg_(entry.from_take_off ? Leg::crosswind : Leg::upwind) {
    // **A faster aeroplane flies a bigger circuit, and the two numbers that
    // make it are one number**: circuit height from a thousand feet for a
    // light aeroplane to fifteen hundred for a jet, and the downwind leg left
    // where a three-degree glidepath passes through it, a third of a mile
    // more to be a little above the path at the hand-over.
    circuit_ft_ = std::clamp(1000.0 + (speeds_.vref_kts - 60.0) * 8.0, 1000.0, 1500.0);
    // Her turn at the downwind speed and a twenty-five-degree bank.
    const double downwind_mps = (speeds_.vref_kts + 20.0) * 0.514444;
    turn_radius_m_ = downwind_mps * downwind_mps / (9.80665 * std::tan(25.0 / degrees));
    // **Circuit height over the ground it is flown over**, not only over the
    // runway: the highest ground of her terrain - the collision DEM, on a
    // server or a client - over the whole of the circuit, sampled every
    // 250 m from eight miles before the threshold to four past the runway's
    // end and from 500 m right of the centreline to two turns and two
    // kilometres left of it, and the circuit raised to be its height over
    // that where the ground rises above the runway. The downwind leg is then
    // left further out, where the glidepath meets the raised height.
    if (const auto& terrain = a_.terrain()) {
        constexpr double feet_per_metre = 3.280839895013123;
        constexpr double spacing_m = 250.0;
        const double h = runway_.heading_deg / degrees;
        const double lat_m = metres_per_degree_latitude(runway_.threshold_lat_deg);
        const double lon_m = metres_per_degree_longitude(runway_.threshold_lat_deg);
        double highest_ft = runway_.elevation_ft;
        for (double along = -8.0 * metres_per_nm;
             along <= std::max(runway_.length_m, 3000.0) + 4.0 * metres_per_nm;
             along += spacing_m) {
            for (double across = 500.0; across >= -(4.0 * turn_radius_m_ + 2000.0);
                 across -= spacing_m) {
                const double north = along * std::cos(h) - across * std::sin(h);
                const double east = along * std::sin(h) + across * std::cos(h);
                highest_ft = std::max(highest_ft,
                                      terrain->height_m(runway_.threshold_lat_deg + north / lat_m,
                                                        runway_.threshold_lon_deg + east / lon_m) *
                                          feet_per_metre);
            }
        }
        circuit_ft_ += highest_ft - runway_.elevation_ft;
    }
    leave_downwind_nm_ = circuit_ft_ / 318.0 + 0.33;
}

bool behind_a_person(std::size_t self, const std::vector<std::optional<CirclePlace>>& all) {
    if (self >= all.size() || !all[self]) {
        return false;
    }
    const CirclePlace& mine = *all[self];
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (i != self && all[i] && all[i]->by_hand && same_runway(all[i]->runway, mine.runway) &&
            ahead_in_sequence(*all[i], i, mine, self)) {
            return true;
        }
    }
    return false;
}

std::vector<std::size_t> persons_in_her_circuit(std::size_t self,
                                                const std::vector<std::optional<CirclePlace>>& all) {
    std::vector<std::size_t> out;
    if (self >= all.size() || !all[self]) {
        return out;
    }
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (i != self && all[i] && all[i]->by_hand && same_runway(all[i]->runway, all[self]->runway)) {
            out.push_back(i);
        }
    }
    return out;
}

std::string leave_the_circuit_for(const std::string& who, const Runway& runway) {
    long heading = std::lround(std::fmod(runway.heading_deg + leave_off_runway_deg, 360.0));
    heading = heading == 0 ? 360 : heading;
    char three[8];
    std::snprintf(three, sizeof three, "%03ld", heading);
    return std::string("leave the circuit on the dead side, heading ") + three +
           " true; rejoin when " + who + ", with priority, has landed on " + runway.name;
}

bool straight_in_by_hand(const Runway& runway, const AircraftState& her) {
    const double above_ft = her.altitude_ft - runway.elevation_ft;
    const double along_nm = along_runway_nm(runway, her.latitude_deg, her.longitude_deg);
    // A three-degree path's height, nm before the threshold: 318 ft a mile.
    return above_ft > RunwayClear::occupied_ft && along_nm <= 0.0 &&
           along_nm >= -by_hand_final_nm &&
           std::abs(across_runway_m(runway, her.latitude_deg, her.longitude_deg)) <
               Separation::minimum_m &&
           off_deg(her.heading_deg, runway.heading_deg) <= by_hand_heading_deg &&
           above_ft <= -along_nm * 318.0 + close_ft;
}

bool departing_by_hand(const Runway& runway, const AircraftState& her) {
    const double above_ft = her.altitude_ft - runway.elevation_ft;
    if (off_deg(her.heading_deg, runway.heading_deg) > by_hand_heading_deg) {
        return false;
    }
    if (on_runway(runway, her.latitude_deg, her.longitude_deg, above_ft)) {
        return her.airspeed_kts >= by_hand_rolling_kts;
    }
    // Off the runway, she is departing only in the air: stopped beside it,
    // lined up with it, she is not traffic.
    const double along_m = along_runway_nm(runway, her.latitude_deg, her.longitude_deg) *
                           metres_per_nm;
    return above_ft > RunwayClear::occupied_ft && along_m >= 0.0 && along_m <= runway.length_m + by_hand_departure_nm * metres_per_nm &&
           std::abs(across_runway_m(runway, her.latitude_deg, her.longitude_deg)) <
               Separation::minimum_m;
}

bool clear_of_departures_by_hand(const Runway& runway, const AircraftState& her) {
    // Not closing on the threshold: heading 90 degrees or more off the
    // bearing to it.
    const double to_threshold_deg = bearing_deg(her.latitude_deg, her.longitude_deg,
                                                runway.threshold_lat_deg, runway.threshold_lon_deg);
    return off_deg(her.heading_deg, to_threshold_deg) >= 90.0 &&
           -along_runway_nm(runway, her.latitude_deg, her.longitude_deg) * metres_per_nm >=
               Separation::minimum_m;
}

bool left_the_circuit_by_hand(const Runway& runway, const AircraftState& her) {
    return distance_m(her.latitude_deg, her.longitude_deg, runway.threshold_lat_deg,
                      runway.threshold_lon_deg) > left_the_circuit_nm * metres_per_nm;
}

Runway reciprocal(const Runway& runway) {
    Runway r = runway;
    const double h = runway.heading_deg / degrees;
    r.threshold_lat_deg = runway.threshold_lat_deg +
                          runway.length_m * std::cos(h) /
                              metres_per_degree_latitude(runway.threshold_lat_deg);
    r.threshold_lon_deg = runway.threshold_lon_deg +
                          runway.length_m * std::sin(h) /
                              metres_per_degree_longitude(runway.threshold_lat_deg);
    r.heading_deg = std::fmod(runway.heading_deg + 180.0, 360.0);
    r.name = runway.name + " reciprocal";
    return r;
}

bool using_by_hand(const Runway& runway, const AircraftState& her) {
    const Runway other = reciprocal(runway);
    return straight_in_by_hand(runway, her) || departing_by_hand(runway, her) ||
           straight_in_by_hand(other, her) || departing_by_hand(other, her);
}

namespace {
bool climbing_out_one_way(const Runway& runway, const AircraftState& her) {
    if (departing_by_hand(runway, her)) {
        return true;
    }
    const double along_m =
        along_runway_nm(runway, her.latitude_deg, her.longitude_deg) * metres_per_nm;
    return her.altitude_ft - runway.elevation_ft > RunwayClear::occupied_ft &&
           off_deg(her.heading_deg, runway.heading_deg) <= by_hand_heading_deg &&
           along_m >= 0.0 && along_m <= runway.length_m + over_departure_nm * metres_per_nm &&
           std::abs(across_runway_m(runway, her.latitude_deg, her.longitude_deg)) <
               Separation::minimum_m;
}
} // namespace

bool climbing_out_by_hand(const Runway& runway, const AircraftState& her) {
    return climbing_out_one_way(runway, her) || climbing_out_one_way(reciprocal(runway), her);
}

bool ahead_of_an_approach_by_hand(const Runway& runway, const AircraftState& her) {
    const auto lined_up = [&her](const Runway& r) {
        const double along_m =
            along_runway_nm(r, her.latitude_deg, her.longitude_deg) * metres_per_nm;
        return on_runway(r, her.latitude_deg, her.longitude_deg, her.altitude_ft - r.elevation_ft) &&
               along_m <= lined_up_m && off_deg(her.heading_deg, r.heading_deg) <= by_hand_heading_deg;
    };
    return lined_up(runway) || lined_up(reciprocal(runway)) || climbing_out_by_hand(runway, her);
}

HoldOutside::HoldOutside(const Aircraft& aircraft, const Runway& runway,
                         const ApproachSpeeds& speeds, double at_least_alt_ft)
    : a_(aircraft), runway_(runway),
      speeds_(for_weight(speeds, aircraft.property("inertia/weight-lbs"))) {
    const AircraftState s = a_.state();
    const double lat_m = metres_per_degree_latitude(runway_.threshold_lat_deg);
    const double lon_m = metres_per_degree_longitude(runway_.threshold_lat_deg);
    // The fix, on the bearing from the threshold she left at.
    const double out = bearing_deg(runway_.threshold_lat_deg, runway_.threshold_lon_deg,
                                   s.latitude_deg, s.longitude_deg) /
                       degrees;
    fix_lat_deg_ = runway_.threshold_lat_deg + fix_nm * metres_per_nm * std::cos(out) / lat_m;
    fix_lon_deg_ = runway_.threshold_lon_deg + fix_nm * metres_per_nm * std::sin(out) / lon_m;
    inbound_deg_ = std::fmod(out * degrees + 180.0, 360.0);
    speed_kts_ = std::clamp(1.4 * speeds_.vref_kts, speeds_.vref_kts + 20.0,
                            std::max(200.0, speeds_.vref_kts + 20.0));
    // 1,000 ft over the highest ground within 4 nm of the hold: the fix,
    // and a leg and a turn beyond it.
    double highest_ft = runway_.elevation_ft;
    if (const auto& terrain = a_.terrain()) {
        constexpr double feet_per_metre = 3.280839895013123;
        const double leg_m = speed_kts_ * 0.514444 * leg_s;
        const double reach_m = 4.0 * metres_per_nm + leg_m + 2000.0;
        for (double north = -reach_m; north <= reach_m; north += 250.0) {
            for (double east = -reach_m; east <= reach_m; east += 250.0) {
                if (std::hypot(north, east) > reach_m) {
                    continue;
                }
                highest_ft = std::max(highest_ft, terrain->height_m(fix_lat_deg_ + north / lat_m,
                                                                    fix_lon_deg_ + east / lon_m) *
                                                      feet_per_metre);
            }
        }
    }
    left_at_alt_ft_ = at_least_alt_ft;
    altitude_ft_ = std::max(at_least_alt_ft, highest_ft + 1000.0);
    best_alt_ft_ = s.altitude_ft;
    best_at_s_ = s.sim_time_s;
}

bool HoldOutside::come_down_to_leaving() {
    altitude_ft_ = left_at_alt_ft_;
    return std::abs(a_.state().altitude_ft - altitude_ft_) <= sets_off_within_ft;
}

bool HoldOutside::climbing_first(const AircraftState& s) {
    if (out_of_reach_) {
        return true;
    }
    if (laps_ != 0 || s.altitude_ft >= altitude_ft_ - sets_off_within_ft) {
        return false;
    }
    if (s.altitude_ft >= best_alt_ft_ + stalled_gain_ft) {
        best_alt_ft_ = s.altitude_ft;
        best_at_s_ = s.sim_time_s;
    } else if (s.sim_time_s - best_at_s_ >= stalled_s) {
        // **Out of her reach**: her best, held where she is.
        out_of_reach_ = true;
        altitude_ft_ = std::max(best_alt_ft_, s.altitude_ft);
    }
    return true;
}

AutopilotModes HoldOutside::modes() {
    const AircraftState s = a_.state();
    const double to_fix_m =
        distance_m(s.latitude_deg, s.longitude_deg, fix_lat_deg_, fix_lon_deg_);
    const double to_fix_deg =
        bearing_deg(s.latitude_deg, s.longitude_deg, fix_lat_deg_, fix_lon_deg_);
    const double outbound_deg = std::fmod(inbound_deg_ + 180.0, 360.0);
    switch (leg_) {
    case Leg::to_fix:
        // Not round it until she has climbed to her height (below): left
        // within 2 nm of the fix with it behind her - a racetrack's
        // outbound leg 7 nm out - she counted a lap at once, skipped the
        // climb where she was, and flew the hold's outbound leg on towards
        // the high ground below its height.
        if (climbing_first(s)) {
            break;
        }
        // Past the fix: within 2 nm of it and it behind her - abeam or
        // over it. Only "over it" (within half a mile and opening), a wide
        // turner circled it, never closer: a B-2A, a Learjet 35A and a
        // Mosquito never crossed it in 600 s.
        if (to_fix_m < 2.0 * metres_per_nm && off_deg(s.heading_deg, to_fix_deg) > 90.0) {
            leg_ = Leg::turn_out;
            ++laps_;
        }
        break;
    case Leg::turn_out:
        if (off_deg(s.heading_deg, outbound_deg) < 15.0) {
            leg_ = Leg::outbound;
            leg_began_s_ = s.sim_time_s;
        }
        break;
    case Leg::outbound:
        if (s.sim_time_s - leg_began_s_ >= leg_s) {
            leg_ = Leg::turn_in;
        }
        break;
    case Leg::turn_in:
        if (off_deg(s.heading_deg, to_fix_deg) < 15.0) {
            leg_ = Leg::to_fix;
        }
        break;
    }
    AutopilotModes m;
    m.altitude_ft = altitude_ft_;
    m.vertical_speed_fpm = std::clamp(speeds_.vref_kts * 10.0, 500.0, 1500.0);
    m.airspeed_kts = speed_kts_;
    switch (leg_) {
    case Leg::to_fix:
        // **Up to her height before she goes**: where the hold is raised over
        // high ground, she climbs in right turns where she is - outside
        // the circuit, over ground her circuit was cleared for - rather
        // than climbing towards the high ground.
        if (climbing_first(s)) {
            m.bank_deg = bank_deg;
            break;
        }
        m.heading_deg = to_fix_deg;
        break;
    case Leg::outbound:
        m.heading_deg = outbound_deg;
        break;
    case Leg::turn_out:
    case Leg::turn_in:
        // Right turns, at a set bank.
        m.bank_deg = bank_deg;
        break;
    }
    return m;
}

void HoldOutside::configure(Controls& controls) const {
    controls.flaps = 0.0;
    controls.speedbrake = 0.0;
    if (a_.gear_retracts()) {
        controls.gear = 0.0;
    }
}

namespace {

// The highest ground within `within_m` of the course from one place to
// another, ft: sampled every 500 m along it, and across it to `within_m`
// either side, the course run on `within_m` past each end. A flat frame
// about the first place: what is measured here is tens of miles.
double highest_near_course_ft(Terrain& terrain, double lat_1, double lon_1, double lat_2,
                              double lon_2, double within_m) {
    constexpr double feet_per_metre = 3.280839895013123;
    constexpr double spacing_m = 500.0;
    const double lat_m = metres_per_degree_latitude(lat_1);
    const double lon_m = metres_per_degree_longitude(lat_1);
    const double north_2 = (lat_2 - lat_1) * lat_m;
    const double east_2 = (lon_2 - lon_1) * lon_m;
    const double length_m = std::hypot(north_2, east_2);
    const double un = length_m > 0.0 ? north_2 / length_m : 1.0;
    const double ue = length_m > 0.0 ? east_2 / length_m : 0.0;
    double highest_ft = -1.0e9;
    for (double along = -within_m; along <= length_m + within_m; along += spacing_m) {
        for (double across = -within_m; across <= within_m; across += spacing_m) {
            const double north = along * un - across * ue;
            const double east = along * ue + across * un;
            highest_ft = std::max(highest_ft, terrain.height_m(lat_1 + north / lat_m,
                                                               lon_1 + east / lon_m) *
                                                  feet_per_metre);
        }
    }
    return highest_ft;
}

} // namespace

std::optional<Diversion> choose_diversion(const Aircraft& aircraft, const Runway& holding_for,
                                          const ApproachSpeeds& speeds,
                                          const std::vector<Runway>& known, double floor_alt_ft,
                                          double best_alt_ft, double needs_m, double kts,
                                          double within_s,
                                          const std::function<double(double, double)>&
                                              elevation_ft) {
    constexpr double within_m = 4.0 * metres_per_nm; // 14 CFR 91.177(a)(2)(ii)
    constexpr double clearance_ft = 1000.0;
    const AircraftState s = aircraft.state();
    std::vector<std::pair<double, const Runway*>> nearest;
    for (const Runway& r : known) {
        if (distance_m(r.threshold_lat_deg, r.threshold_lon_deg, holding_for.threshold_lat_deg,
                       holding_for.threshold_lon_deg) < apart_nm * metres_per_nm ||
            (needs_m > 0.0 && r.length_m < needs_m)) {
            continue;
        }
        nearest.emplace_back(
            distance_m(s.latitude_deg, s.longitude_deg, r.threshold_lat_deg, r.threshold_lon_deg),
            &r);
    }
    std::stable_sort(nearest.begin(), nearest.end(),
                     [](const auto& x, const auto& y) { return x.first < y.first; });
    const auto& terrain = aircraft.terrain();
    for (const auto& [straight_m, known_r] : nearest) {
        // Her fuel first, by the straight line - no shorter than any way
        // there - before any ground is sampled.
        if (kts <= 0.0 || straight_m / metres_per_nm / kts * 3600.0 > within_s) {
            continue;
        }
        // Its elevation, looked up now it is judged.
        Runway judged = *known_r;
        if (std::isnan(judged.elevation_ft)) {
            if (!elevation_ft) {
                continue;
            }
            judged.elevation_ft = elevation_ft(judged.threshold_lat_deg, judged.threshold_lon_deg);
        }
        const Runway* r = &judged;
        // The glidepath's height at six miles, as the legs to final have it.
        const double six_m = 6.0 * metres_per_nm;
        const double g6_ft =
            r->elevation_ft + (six_m + speeds.aim_m) * std::tan(3.0 / degrees) * 3.280839895013123;
        const double h = r->heading_deg / degrees;
        const double lat_m = metres_per_degree_latitude(r->threshold_lat_deg);
        const double lon_m = metres_per_degree_longitude(r->threshold_lat_deg);
        const auto out_along = [&](double out_nm, double& lat, double& lon) {
            lat = r->threshold_lat_deg - out_nm * metres_per_nm * std::cos(h) / lat_m;
            lon = r->threshold_lon_deg - out_nm * metres_per_nm * std::sin(h) / lon_m;
        };
        double six_lat = 0.0;
        double six_lon = 0.0;
        out_along(6.0, six_lat, six_lon);
        // The approach: from where she joins down to six miles, over the
        // ground near it - judged at the six-mile point's height, its
        // lowest.
        // The way there first, which sets the height she joins at.
        double route_alt_ft = floor_alt_ft;
        double join_out_nm = 6.0;
        double join_lat = six_lat;
        double join_lon = six_lon;
        // Twice: the join moves out with the way's height, and the way's
        // ground is judged to where it then is.
        for (int pass = 0; pass < 2; ++pass) {
            const double ground_ft =
                terrain ? highest_near_course_ft(*terrain, s.latitude_deg, s.longitude_deg,
                                                 join_lat, join_lon, within_m)
                        : -1.0e9;
            route_alt_ft = std::max(floor_alt_ft, ground_ft + clearance_ft);
            join_out_nm = 6.0 + std::max(0.0, route_alt_ft - g6_ft) / 318.0;
            out_along(join_out_nm, join_lat, join_lon);
        }
        if (route_alt_ft > best_alt_ft) {
            continue;
        }
        if (terrain && highest_near_course_ft(*terrain, join_lat, join_lon, six_lat, six_lon,
                                              within_m) +
                               clearance_ft >
                           g6_ft) {
            continue;
        }
        const double route_nm =
            (distance_m(s.latitude_deg, s.longitude_deg, join_lat, join_lon) +
             distance_m(join_lat, join_lon, r->threshold_lat_deg, r->threshold_lon_deg)) /
            metres_per_nm;
        if (kts <= 0.0 || route_nm / kts * 3600.0 > within_s) {
            continue;
        }
        Diversion d;
        d.runway = *r;
        d.route_alt_ft = route_alt_ft;
        d.join_lat_deg = join_lat;
        d.join_lon_deg = join_lon;
        d.join_out_nm = join_out_nm;
        d.route_nm = route_nm;
        return d;
    }
    return std::nullopt;
}

bool departure_held(const Runway& runway, const std::vector<std::optional<CirclePlace>>& all) {
    return std::any_of(all.begin(), all.end(), [&runway](const std::optional<CirclePlace>& p) {
        return p && same_runway(p->runway, runway) && !p->clear_of_departures;
    });
}

bool close_ahead_on_approach(const Runway& runway, const AircraftState& me,
                             const AircraftState& other) {
    // On the runway it is the runway's rule; on the ground beside it, or
    // no higher, it is not traffic.
    if (other.altitude_ft - runway.elevation_ft <= RunwayClear::occupied_ft) {
        return false;
    }
    const double ahead_nm = along_runway_nm(runway, other.latitude_deg, other.longitude_deg) -
                            along_runway_nm(runway, me.latitude_deg, me.longitude_deg);
    return ahead_nm > 0.0 &&
           std::abs(across_runway_m(runway, other.latitude_deg, other.longitude_deg)) <
               Separation::minimum_m &&
           me.altitude_ft >= other.altitude_ft && me.altitude_ft - other.altitude_ft < close_ft &&
           distance_m(me.latitude_deg, me.longitude_deg, other.latitude_deg,
                      other.longitude_deg) < close_m(runway, me, other);
}

double close_m(const Runway& runway, const AircraftState& me, const AircraftState& other) {
    // Each one's speed along the landing direction, from her airspeed and
    // heading: calm air's ground speed, near enough to judge a closing by.
    const auto along_kts = [&runway](const AircraftState& s) {
        return s.airspeed_kts * std::cos((s.heading_deg - runway.heading_deg) / degrees);
    };
    const double closing_mps = std::max(0.0, along_kts(me) - along_kts(other)) * 0.514444;
    return Separation::minimum_m + std::max(close_in_hand_m, closing_mps * close_lead_s);
}

void GoAroundCircuit::follow(std::optional<double> ahead_alt_ft, bool straight_in,
                             bool climbing_out) {
    climbing_out_ = climbing_out && leg_ == Leg::upwind;
    // **A straight-in is followed only until she turns base**: past it she
    // is committed, and it is the one behind.
    const bool before_base = leg_ != Leg::base && leg_ != Leg::intercept && leg_ != Leg::final;
    const bool giving_way = straight_in && before_base;
    following_ = ahead_alt_ft.has_value() || giving_way;
    if (ahead_alt_ft) {
        // Raised, and kept raised: the downwind leg is left where the
        // glidepath meets the height she is at.
        const double above_ft = *ahead_alt_ft + above_ahead_ft - runway_.elevation_ft;
        if (above_ft > circuit_ft_) {
            circuit_ft_ = above_ft;
            leave_downwind_nm_ = circuit_ft_ / 318.0 + 0.33;
        }
    }
    if (following_) {
        clear_the_racetrack_ground();
    }
}

void GoAroundCircuit::clear_the_racetrack_ground() {
    const auto& terrain = a_.terrain();
    if (!terrain || circuit_ft_ <= racetrack_sampled_ft_ + resampled_over_ft) {
        return;
    }
    constexpr double feet_per_metre = 3.280839895013123;
    constexpr double spacing_m = 250.0;
    const double h = runway_.heading_deg / degrees;
    const double lat_m = metres_per_degree_latitude(runway_.threshold_lat_deg);
    const double lon_m = metres_per_degree_longitude(runway_.threshold_lat_deg);
    // Up to twenty raises: each makes a bigger racetrack, sampled again.
    for (int raised = 0; raised < 20; ++raised) {
        // Sampled for a circuit `resampled_over_ft` higher than hers, so
        // that a raise short of it, not sampled again, is inside the area.
        const double area_ft = circuit_ft_ + resampled_over_ft;
        // Her downwind speed made true at that height (ISA, the
        // troposphere), and her turn at it.
        const double alt_ft = runway_.elevation_ft + area_ft;
        const double sigma = std::pow(std::max(0.1, 1.0 - 6.8756e-6 * alt_ft), 4.2559);
        const double tas_mps = (speeds_.vref_kts + 20.0) * 0.514444 / std::sqrt(sigma);
        const double turn_m = tas_mps * tas_mps / (9.80665 * std::tan(25.0 / degrees));
        const double turns_away_nm =
            area_ft / 318.0 + 0.33 +
            std::max(0.0, longest_extension_nm - 2.0 * turn_radius_m_ / metres_per_nm);
        const double reach_m = turns_away_nm * metres_per_nm + reach_turns * turn_m;
        double highest_ft = runway_.elevation_ft;
        for (double along = 0.0; along >= -reach_m; along -= spacing_m) {
            for (double across = 500.0; across >= -(4.0 * turn_m + 2000.0); across -= spacing_m) {
                const double north = along * std::cos(h) - across * std::sin(h);
                const double east = along * std::sin(h) + across * std::cos(h);
                highest_ft = std::max(highest_ft,
                                      terrain->height_m(runway_.threshold_lat_deg + north / lat_m,
                                                        runway_.threshold_lon_deg + east / lon_m) *
                                          feet_per_metre);
            }
        }
        racetrack_sampled_ft_ = circuit_ft_;
        racetrack_sampled_nm_ = reach_m / metres_per_nm;
        const double over_ft = highest_ft + racetrack_clearance_ft - runway_.elevation_ft;
        if (over_ft <= circuit_ft_) {
            return;
        }
        circuit_ft_ = over_ft;
        leave_downwind_nm_ = circuit_ft_ / 318.0 + 0.33;
    }
}

double GoAroundCircuit::along_nm() const {
    const AircraftState s = a_.state();
    return along_runway_nm(runway_, s.latitude_deg, s.longitude_deg);
}

double GoAroundCircuit::base_kts() const {
    if (speeds_.stall_kts <= 0.0) {
        return speeds_.vref_kts * 1.08;
    }
    if (speeds_.vref_kts > 1.3 * speeds_.stall_kts) {
        return speeds_.vref_kts * 1.4 / 1.3;
    }
    return speeds_.stall_kts * 1.4;
}

double GoAroundCircuit::across_m() const {
    const AircraftState s = a_.state();
    return across_runway_m(runway_, s.latitude_deg, s.longitude_deg);
}

AutopilotModes GoAroundCircuit::modes() {
    const double agl_ft = a_.state().altitude_ft - runway_.elevation_ft;
    // Moved on a leg where she has reached the end of hers.
    // Following, she turns crosswind only at her raised height, which is
    // 600 ft or more over the circuit ahead: she crosses its legs above it.
    if (leg_ == Leg::upwind && !climbing_out_ &&
        agl_ft >= circuit_ft_ - (following_ ? 100.0 : 300.0) && along_nm() > 0.0) {
        leg_ = Leg::crosswind;
    } else if (leg_ == Leg::crosswind &&
               across_m() <= -(2.0 * turn_radius_m_ + 600.0) &&
               agl_ft >= circuit_ft_ - 100.0) {
        // Far enough out to turn base and final from, and at circuit height:
        // downwind is flown level.
        leg_ = Leg::downwind;
    } else if (leg_ == Leg::downwind && along_nm() <= -leave_downwind_nm_ && !following_) {
        leg_ = Leg::base;
    } else if (leg_ == Leg::downwind && following_ &&
               along_nm() <= -(leave_downwind_nm_ +
                               std::max(0.0, longest_extension_nm -
                                                 2.0 * turn_radius_m_ / metres_per_nm))) {
        // Begun two turns' radius short of it: she rolls into her turn away
        // and carries on round it about one and a half (measured), and the
        // racetrack's far end is to be inside the extension.
        // **Following traffic, she extends her downwind until it is down** -
        // but no further than `longest_extension_nm`: there she turns away
        // from the runway and flies back up a racetrack outside her own
        // circuit, still at her raised height, to join the downwind leg
        // again where she would have turned base.
        // The downwind line is taken once, the first time: taken again each
        // time round, every turn back on to it carried her a turn's radius
        // nearer the runway, and four racetracks walked an F-15C across it
        // to join final from its far side.
        if (!downwind_m_) {
            downwind_m_ = across_m();
        }
        leg_ = Leg::turn_out;
    } else if (leg_ == Leg::turn_out && across_m() <= *downwind_m_ - 2.0 * turn_radius_m_) {
        leg_ = Leg::outbound;
        ++racetracks_;
    } else if (leg_ == Leg::outbound && along_nm() >= -leave_downwind_nm_) {
        leg_ = Leg::turn_in;
    } else if (leg_ == Leg::turn_in && across_m() >= *downwind_m_ - turn_radius_m_) {
        // A turn's radius short of the line: her right turn on to the
        // downwind heading rolls her out on it.
        leg_ = Leg::downwind;
    } else if (leg_ == Leg::base && across_m() >= -(2.0 * turn_radius_m_ + 200.0)) {
        leg_ = Leg::intercept;
    } else if (leg_ == Leg::intercept && across_m() >= -turn_radius_m_) {
        leg_ = Leg::final;
    }
    // **The gear up once she is climbing**, where it retracts: a positive
    // rate of climb, as the handbook has it.
    if (!gear_up_ && a_.gear_retracts() && a_.state().climb_rate_fpm > 100.0) {
        gear_up_ = true;
    }
    AutopilotModes m;
    m.altitude_ft = runway_.elevation_ft + circuit_ft_;
    // A climb she can make: about ten feet a minute a knot of her reference
    // speed - 500 for a Cub, 1,500 for a jet.
    // Or the take-off's, joined from one.
    m.vertical_speed_fpm =
        entry_.climb_fpm.value_or(std::clamp(speeds_.vref_kts * 10.0, 500.0, 1500.0));
    m.airspeed_kts = speeds_.vref_kts + 20.0;
    switch (leg_) {
    case Leg::upwind:
        m.heading_deg = runway_.heading_deg;
        if (following_) {
            // **Offset to the side away from the circuit**, as a pilot going
            // around for another aeroplane does: `following_offset_m` right
            // of the centreline, steered on to it at up to 45 degrees.
            const double off_m = following_offset_m - across_m();
            m.heading_deg = runway_.heading_deg +
                            std::clamp(std::atan2(off_m, 1000.0) * degrees, -45.0, 45.0);
        }
        m.airspeed_kts = entry_.climb_kts.value_or(*m.airspeed_kts);
        break;
    case Leg::crosswind:
        m.heading_deg = runway_.heading_deg - 90.0;
        m.airspeed_kts = entry_.climb_kts.value_or(*m.airspeed_kts);
        break;
    case Leg::downwind:
        m.heading_deg = runway_.heading_deg - 180.0;
        break;
    // The racetrack: right turns, away from the runway and back, so that
    // she never crosses her own final approach.
    case Leg::turn_out:
        m.heading_deg = runway_.heading_deg - 90.0;
        break;
    case Leg::outbound:
        m.heading_deg = runway_.heading_deg;
        break;
    case Leg::turn_in:
        m.heading_deg = runway_.heading_deg + 90.0;
        break;
    case Leg::base:
        m.heading_deg = runway_.heading_deg + 90.0;
        // Slowing, as a pilot does on base: 1.4 times the stall, or the same
        // margin over the approach speed where it is flown faster (base_kts).
        m.airspeed_kts = base_kts();
        break;
    case Leg::intercept:
    case Leg::final:
        m.heading_deg = runway_.heading_deg + 30.0;
        m.airspeed_kts = base_kts();
        break;
    }
    m.heading_deg = std::fmod(*m.heading_deg + 360.0, 360.0);
    return m;
}

void GoAroundCircuit::configure(Controls& controls) const {
    // The flap to its go-around setting, half the landing flap, round the
    // whole circuit; the approach puts the landing flap out again.
    // Joined from a take-off, the take-off flap stays out round the
    // pattern, as a jet's downwind leg is flown with it.
    controls.flaps = entry_.flaps.value_or(0.5 * speeds_.flap);
    controls.speedbrake = 0.0;
    if (gear_up_) {
        controls.gear = 0.0;
    }
}

} // namespace glideslope::sim
