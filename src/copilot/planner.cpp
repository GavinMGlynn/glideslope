#include "copilot/planner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace glideslope::copilot {

namespace {

// Six decimal places of a degree: about a tenth of a metre.
std::string degrees(double d) {
    char text[32];
    std::snprintf(text, sizeof text, "%.6f", d);
    return text;
}

std::string whole(double d) {
    char text[32];
    std::snprintf(text, sizeof text, "%.0f", d);
    return text;
}

// The answer less any Markdown fence a model puts round it.
std::string unfenced(const std::string& answer) {
    std::istringstream in(answer);
    std::string out;
    for (std::string line; std::getline(in, line);) {
        const auto first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line.compare(first, 3, "```") == 0) {
            continue;
        }
        out += line;
        out += '\n';
    }
    return out;
}

constexpr double least_height_ft = 500.0;
constexpr double farthest_m = 200000.0;

} // namespace

std::string runway_line(const world::RunwayEnd& end) {
    return "runway " + end.ident + " " + degrees(end.latitude_deg) + " " +
           degrees(end.longitude_deg) + " " + whole(end.elevation_ft) + " " +
           whole(end.heading_deg) + " " + whole(end.length_m);
}

std::string planning_instructions() {
    return "You plan flights for a flight simulator. An autopilot flies the plan you write; "
           "you never fly the aircraft yourself.\n"
           "\n"
           "Answer with the plan and nothing else: one command a line, no comments, no "
           "Markdown. The commands are:\n"
           "\n"
           "  aircraft ID\n"
           "  runway NAME LATITUDE LONGITUDE ELEVATION_FT HEADING_DEG LENGTH_M\n"
           "  takeoff HEIGHT_FT\n"
           "  waypoint NAME LATITUDE LONGITUDE ALTITUDE_FT AIRSPEED_KT\n"
           "  orbit NAME LATITUDE LONGITUDE RADIUS_M ALTITUDE_FT AIRSPEED_KT TURNS left|right\n"
           "\n"
           "- The first line is `aircraft` with the aircraft's id as given.\n"
           "- The aircraft stands on the ground, so the plan takes off: copy exactly one of the "
           "runway lines given, choosing the one that suits the flight, and follow it with "
           "`takeoff HEIGHT_FT`, the height above the runway, from 500 to 1500, at which the "
           "take-off is done and the plan's waypoints begin.\n"
           "- Then the waypoints, flown in order. A waypoint is flown to and passed. An orbit is "
           "flown to and then round, TURNS times (whole; 0 means round and round until told "
           "otherwise), turning left or right, at RADIUS_M metres from its centre - no tighter "
           "than the aircraft can turn at its airspeed, as given.\n"
           "- Latitudes and longitudes are WGS84 decimal degrees, south and west negative. Use "
           "what you know of where places are.\n"
           "- Altitudes are feet above mean sea level; airspeeds are knots, calibrated, within "
           "the aircraft's speeds as given.\n"
           "- Names are one word each, letters, digits and underscores.\n"
           "\n"
           "An example, for a different aircraft and airport:\n"
           "\n"
           "aircraft pa28\n"
           "runway 11 -33.595001 150.924003 81 107 1044\n"
           "takeoff 500\n"
           "waypoint WARRAGAMBA -33.8889 150.5936 3500 105\n"
           "orbit KATOOMBA -33.7120 150.3120 1500 4500 100 1 right\n";
}

std::string planning_request(const PlanRequest& r) {
    std::string out = "The aircraft: " + r.aircraft + ", a " + r.aircraft_name + ". Its speeds: " +
                      whole(r.approach_kts) + " kt on the approach, " + whole(r.climb_kts) +
                      " kt best climb, " + whole(r.cruise_kts) +
                      " kt cruise. Plan every airspeed from " + whole(r.approach_kts) + " to " +
                      whole(r.cruise_kts * 1.2) + " kt. An orbit's radius must be at least " +
                      whole(std::ceil(sim::least_orbit_radius_m(r.approach_kts))) + " m at " +
                      whole(r.approach_kts) + " kt, " +
                      whole(std::ceil(sim::least_orbit_radius_m(r.cruise_kts))) + " m at " +
                      whole(r.cruise_kts) + " kt and " +
                      whole(std::ceil(sim::least_orbit_radius_m(r.cruise_kts * 1.2))) + " m at " +
                      whole(r.cruise_kts * 1.2) +
                      " kt: the least grows with the square of the airspeed.\n\n";
    out += "It stands at " + r.airport + ". Its runways, one line for each way of taking off:\n\n";
    for (const world::RunwayEnd& end : r.runways) {
        if (plannable(end)) {
            out += runway_line(end) + "\n";
        }
    }
    out += "\nThe pilot says: " + r.command + "\n";
    return out;
}

std::string refusal(const PlanRequest& r, const sim::FlightPlan& plan) {
    if (plan.aircraft != r.aircraft) {
        return "the plan is for aircraft " + plan.aircraft + ", not " + r.aircraft;
    }
    if (!plan.takeoff) {
        return "the plan does not take off, and the aircraft is on the ground";
    }
    // The runway exactly as given: its line written out again must be one of
    // the lines it was given.
    const sim::Runway& runway = plan.takeoff->runway;
    const world::RunwayEnd* from = nullptr;
    for (const world::RunwayEnd& end : r.runways) {
        if (!plannable(end)) {
            continue;
        }
        world::RunwayEnd as_written = end;
        as_written.latitude_deg = runway.threshold_lat_deg;
        as_written.longitude_deg = runway.threshold_lon_deg;
        as_written.elevation_ft = runway.elevation_ft;
        as_written.heading_deg = runway.heading_deg;
        as_written.length_m = runway.length_m;
        as_written.ident = runway.name;
        if (runway_line(as_written) == runway_line(end)) {
            from = &end;
        }
    }
    if (from == nullptr) {
        return "the runway " + runway.name + " is not one of the runway lines given, exactly";
    }
    const double least_ft = runway.elevation_ft + least_height_ft;
    const double slowest = r.approach_kts;
    const double fastest = r.cruise_kts * 1.2;
    for (const sim::Waypoint& w : plan.waypoints) {
        if (w.altitude_ft < least_ft) {
            return w.name + " is at " + whole(w.altitude_ft) + " ft, below " + whole(least_ft) +
                   " ft, 500 ft above the runway";
        }
        if (w.airspeed_kts < slowest - 0.5 || w.airspeed_kts > fastest + 0.5) {
            return w.name + " is flown at " + whole(w.airspeed_kts) + " kt, outside " +
                   whole(slowest) + " to " + whole(fastest) + " kt";
        }
        const double away =
            sim::distance_m(from->latitude_deg, from->longitude_deg, w.latitude_deg, w.longitude_deg);
        if (away > farthest_m) {
            return w.name + " is " + whole(away / 1000.0) + " km from the runway, more than " +
                   whole(farthest_m / 1000.0);
        }
    }
    return {};
}

bool plannable(const world::RunwayEnd& end) {
    return !std::isnan(end.elevation_ft);
}

Planned plan_from_words(Provider& provider, const PlanRequest& request) {
    if (std::none_of(request.runways.begin(), request.runways.end(), plannable)) {
        throw ProviderError("none of " + request.airport +
                            "'s runways says its elevation, which a plan's heights are "
                            "checked against");
    }
    const std::string instructions = planning_instructions();
    std::vector<Turn> conversation{{"user", planning_request(request)}};
    Planned out;
    for (int attempt = 1; attempt <= most_attempts; ++attempt) {
        const std::string answer = provider.answer(instructions, conversation);
        out.attempts = attempt;
        const std::string text = unfenced(answer);
        std::string why;
        try {
            out.plan = sim::parse_flight_plan(text);
            why = refusal(request, out.plan);
        } catch (const sim::FlightPlanError& e) {
            why = e.what();
        }
        if (why.empty()) {
            out.text = text;
            return out;
        }
        out.refused.push_back(why);
        conversation.push_back({"assistant", answer});
        conversation.push_back(
            {"user", "That plan cannot be flown: " + why + ". Write the whole plan again."});
    }
    std::string all;
    for (const std::string& why : out.refused) {
        all += "\n  " + why;
    }
    throw ProviderError(provider.name() + "'s " + std::to_string(most_attempts) +
                        " plans were each refused:" + all);
}

} // namespace glideslope::copilot
