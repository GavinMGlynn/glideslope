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

double number(const std::string& word, int line, const char* what, double low,
              double high) {
    char* end = nullptr;
    const double v = std::strtod(word.c_str(), &end);
    if (word.empty() || *end != '\0' || !(v >= low && v <= high)) {
        throw FlightPlanError("line " + std::to_string(line) + ": " + what +
                              " must be a number from " + std::to_string(low) + " to " +
                              std::to_string(high) + ", not \"" + word + "\"");
    }
    return v;
}

double normalised(double degrees) {
    const double d = std::fmod(degrees, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}

} // namespace

FlightPlan parse_flight_plan(std::string_view text) {
    FlightPlan plan;
    std::istringstream in{std::string(text)};
    int line_number = 0;
    for (std::string line; std::getline(in, line);) {
        ++line_number;
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line.erase(hash);
        }
        std::istringstream words(line);
        std::vector<std::string> w;
        for (std::string word; words >> word;) {
            w.push_back(word);
        }
        if (w.empty()) {
            continue;
        }
        const auto wrong = [&](const std::string& why) {
            return FlightPlanError("line " + std::to_string(line_number) + ": " + why);
        };
        if (w[0] == "aircraft") {
            if (w.size() != 2) {
                throw wrong("aircraft NAME");
            }
            plan.aircraft = w[1];
        } else if (w[0] == "start") {
            if (w.size() != 6) {
                throw wrong(
                    "start LATITUDE LONGITUDE ALTITUDE_FT HEADING_DEG AIRSPEED_KT");
            }
            FlightPlan::Start s;
            s.latitude_deg = number(w[1], line_number, "the latitude", -90.0, 90.0);
            s.longitude_deg = number(w[2], line_number, "the longitude", -180.0, 180.0);
            s.altitude_ft =
                number(w[3], line_number, "the altitude", -1500.0, 100000.0);
            s.heading_deg = number(w[4], line_number, "the heading", 0.0, 360.0);
            s.airspeed_kts = number(w[5], line_number, "the airspeed", 1.0, 1000.0);
            plan.start = s;
        } else if (w[0] == "waypoint") {
            if (w.size() != 6) {
                throw wrong("waypoint NAME LATITUDE LONGITUDE ALTITUDE_FT AIRSPEED_KT");
            }
            Waypoint p;
            p.name = w[1];
            p.latitude_deg = number(w[2], line_number, "the latitude", -90.0, 90.0);
            p.longitude_deg = number(w[3], line_number, "the longitude", -180.0, 180.0);
            p.altitude_ft =
                number(w[4], line_number, "the altitude", -1500.0, 100000.0);
            p.airspeed_kts = number(w[5], line_number, "the airspeed", 1.0, 1000.0);
            plan.waypoints.push_back(p);
        } else {
            throw wrong("no command \"" + w[0] + "\"");
        }
    }
    if (plan.aircraft.empty()) {
        throw FlightPlanError("the plan names no aircraft");
    }
    if (plan.waypoints.empty()) {
        throw FlightPlanError("the plan has no waypoints");
    }
    return plan;
}

double distance_m(double latitude_1, double longitude_1, double latitude_2,
                  double longitude_2) {
    const double p1 = latitude_1 * radians;
    const double p2 = latitude_2 * radians;
    const double dp = p2 - p1;
    const double dl = (longitude_2 - longitude_1) * radians;
    const double h = std::sin(dp / 2) * std::sin(dp / 2) +
                     std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2.0 * earth_radius_m * std::asin(std::min(1.0, std::sqrt(h)));
}

double bearing_deg(double latitude_1, double longitude_1, double latitude_2,
                   double longitude_2) {
    const double p1 = latitude_1 * radians;
    const double p2 = latitude_2 * radians;
    const double dl = (longitude_2 - longitude_1) * radians;
    const double y = std::sin(dl) * std::cos(p2);
    const double x =
        std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
    return normalised(std::atan2(y, x) / radians);
}

Navigator::Navigator(const Aircraft& aircraft, FlightPlan plan)
    : a_(aircraft), plan_(std::move(plan)) {
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

    AutopilotModes modes;
    double track = last_track_deg_;
    double off_m = 0.0;
    while (!finished()) {
        const Waypoint& to = plan_.waypoints[next_];
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
    modes.heading_deg = normalised(course - drift_deg_);
    return modes;
}

} // namespace glideslope::sim
