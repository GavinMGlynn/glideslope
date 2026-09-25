#include "sim/plan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <sstream>

namespace glideslope::sim {

namespace {

constexpr double earth_radius_m = 6371000.0;
constexpr double radians = std::numbers::pi / 180.0;

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
    std::optional<Runway> runway;
    std::optional<double> takeoff_to_ft;
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
        // **Each of these once**: a second would quietly replace the first.
        if ((w[0] == "aircraft" && !plan.aircraft.empty()) || (w[0] == "start" && plan.start) ||
            (w[0] == "runway" && runway) || (w[0] == "takeoff" && takeoff_to_ft)) {
            throw wrong("a second " + w[0] + " line");
        }
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
        } else if (w[0] == "orbit") {
            if (w.size() != 9 || (w[8] != "left" && w[8] != "right")) {
                throw wrong("orbit NAME LATITUDE LONGITUDE RADIUS_M ALTITUDE_FT AIRSPEED_KT "
                            "TURNS left|right");
            }
            Waypoint p;
            p.name = w[1];
            p.latitude_deg = number(w[2], line_number, "the latitude", -90.0, 90.0);
            p.longitude_deg = number(w[3], line_number, "the longitude", -180.0, 180.0);
            Waypoint::Orbit o;
            o.radius_m = number(w[4], line_number, "the radius", 200.0, 50000.0);
            p.altitude_ft =
                number(w[5], line_number, "the altitude", -1500.0, 100000.0);
            p.airspeed_kts = number(w[6], line_number, "the airspeed", 1.0, 1000.0);
            const double turns = number(w[7], line_number, "the turns", 0.0, 1000.0);
            if (turns != std::floor(turns)) {
                throw wrong("the turns must be whole, not \"" + w[7] + "\"");
            }
            o.turns = static_cast<int>(turns);
            o.right = w[8] == "right";
            if (o.radius_m < least_orbit_radius_m(p.airspeed_kts)) {
                throw wrong("the orbit's radius, " + w[4] + " m, is too tight to fly at " + w[6] +
                            " kt: at least " +
                            std::to_string(static_cast<int>(
                                std::ceil(least_orbit_radius_m(p.airspeed_kts)))) +
                            " m");
            }
            p.orbit = o;
            plan.waypoints.push_back(p);
        } else if (w[0] == "runway") {
            if (w.size() != 7) {
                throw wrong("runway NAME LATITUDE LONGITUDE ELEVATION_FT HEADING_DEG LENGTH_M");
            }
            Runway r;
            r.name = w[1];
            r.threshold_lat_deg = number(w[2], line_number, "the latitude", -90.0, 90.0);
            r.threshold_lon_deg = number(w[3], line_number, "the longitude", -180.0, 180.0);
            r.elevation_ft = number(w[4], line_number, "the elevation", -1500.0, 20000.0);
            r.heading_deg = number(w[5], line_number, "the heading", 0.0, 360.0);
            r.length_m = number(w[6], line_number, "the length", 100.0, 10000.0);
            runway = r;
        } else if (w[0] == "takeoff") {
            if (w.size() != 2) {
                throw wrong("takeoff HEIGHT_FT");
            }
            takeoff_to_ft = number(w[1], line_number, "the height", 100.0, 10000.0);
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
    if (runway.has_value() != takeoff_to_ft.has_value()) {
        throw FlightPlanError(runway ? "the plan has a runway and no take-off from it"
                                     : "the plan takes off with no runway to take off from");
    }
    if (runway) {
        if (plan.start) {
            throw FlightPlanError("the plan both starts in the air and takes off");
        }
        plan.takeoff = FlightPlan::TakeOff{*runway, *takeoff_to_ft};
    }
    return plan;
}

double least_orbit_radius_m(double airspeed_kts) {
    const double v = airspeed_kts * 1852.0 / 3600.0;
    return 2.5 * v * v / (9.80665 * std::tan(most_bank_deg * radians));
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

} // namespace glideslope::sim
