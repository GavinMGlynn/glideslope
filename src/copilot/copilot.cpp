#include "copilot/copilot.hpp"

#include "copilot/planner.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace glideslope::copilot {

namespace {

std::string fixed(double d, int places) {
    char text[48];
    std::snprintf(text, sizeof text, "%.*f", places, d);
    return text;
}

std::string whole(double d) {
    return fixed(d, 0);
}

// A waypoint as a plan's line.
std::string waypoint_line(const sim::Waypoint& w) {
    std::string out = (w.orbit ? "orbit " : "waypoint ") + w.name + " " + fixed(w.latitude_deg, 4) +
                      " " + fixed(w.longitude_deg, 4) + " ";
    if (w.orbit) {
        out += whole(w.orbit->radius_m) + " ";
    }
    out += whole(w.altitude_ft) + " " + whole(w.airspeed_kts);
    if (w.orbit) {
        out += " " + std::to_string(w.orbit->turns) + (w.orbit->right ? " right" : " left");
    }
    return out;
}

constexpr double least_height_ft = 500.0;
constexpr double farthest_m = 200000.0;

std::string trimmed(const std::string& line) {
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = line.find_last_not_of(" \t\r");
    return line.substr(first, last - first + 1);
}

} // namespace

std::string copilot_instructions() {
    return "You are the copilot of an aircraft in a flight simulator, in command of its "
           "autopilot. You never fly the aircraft yourself: an autopilot flies the route you "
           "give it. You are told how the flight is going now and then, and whenever something "
           "happens, and you answer with what the autopilot is to fly from now on.\n"
           "\n"
           "Answer with nothing but one of these, one command a line, no comments, no "
           "Markdown. Either\n"
           "\n"
           "  keep\n"
           "\n"
           "alone, to go on flying the route as it is; or a whole new route, flown from where "
           "the aircraft is now and replacing the old one:\n"
           "\n"
           "  glide AIRSPEED_KT\n"
           "  waypoint NAME LATITUDE LONGITUDE ALTITUDE_FT AIRSPEED_KT\n"
           "  orbit NAME LATITUDE LONGITUDE RADIUS_M ALTITUDE_FT AIRSPEED_KT TURNS left|right\n"
           "\n"
           "- Waypoints are flown in order. A waypoint is flown to and passed. An orbit is flown "
           "to and then round, TURNS times (whole; 0 means round and round until told "
           "otherwise), turning left or right, at RADIUS_M metres from its centre - no tighter "
           "than the aircraft can turn at its airspeed, as given.\n"
           "- To follow something on the ground - a coast, a river, a road - put waypoints along "
           "it, every 2 to 5 km, close enough that the straight legs between them stay on it.\n"
           "- `glide`, first, only when the engine has stopped, and then always: the aircraft "
           "cannot hold its height, and the autopilot holds AIRSPEED_KT with the elevator while "
           "the route steers it down. Glide to a runway nearby that it can reach - a light "
           "aircraft glides about 1.5 km for each 1,000 ft it is above the ground - and orbit "
           "over it to lose the height left. A glide's waypoints' altitudes and airspeeds are "
           "not flown: give each the field's elevation and the glide's airspeed, and an orbit "
           "a radius wide enough for the glide's airspeed.\n"
           "- Latitudes and longitudes are WGS84 decimal degrees, south and west negative. Use "
           "what you know of where places are.\n"
           "- Altitudes are feet above mean sea level; airspeeds are knots, calibrated, within "
           "the aircraft's speeds as given.\n"
           "- Names are one word each, letters, digits and underscores.\n"
           "\n"
           "An example of a new route, for a different flight:\n"
           "\n"
           "waypoint WARRAGAMBA -33.8889 150.5936 3500 105\n"
           "orbit KATOOMBA -33.7120 150.3120 1500 4500 100 1 right\n";
}

std::string situation_text(const Brief& b, const Situation& now) {
    std::string out =
        "The aircraft: " + b.aircraft + ", a " + b.aircraft_name + ". Its speeds: " +
        whole(b.approach_kts) + " kt on the approach, " + whole(b.climb_kts) + " kt best climb, " +
        whole(b.cruise_kts) + " kt cruise. Every airspeed from " + whole(b.approach_kts) + " to " +
        whole(b.cruise_kts * 1.2) + " kt; a glide from " + whole(b.approach_kts) + " to " +
        whole(b.climb_kts) + " kt. An orbit's radius must be at least " +
        whole(std::ceil(sim::least_orbit_radius_m(b.approach_kts))) + " m at " +
        whole(b.approach_kts) + " kt and " +
        whole(std::ceil(sim::least_orbit_radius_m(b.cruise_kts))) + " m at " +
        whole(b.cruise_kts) + " kt: the least grows with the square of the airspeed.\n\n";
    out += "The pilot asked: " + b.task + "\n\n";
    out += "Now, " + whole(now.seconds) + " s since you were engaged, you are asked because " +
           now.event + ".\n";
    out += "  At " + fixed(now.latitude_deg, 4) + " " + fixed(now.longitude_deg, 4) + ", " +
           whole(now.altitude_ft) + " ft above sea level, the ground beneath at " +
           whole(now.ground_ft) + " ft.\n";
    // The vertical speed signed, not "climbing" or "descending": a word that
    // turned on a number's sign would make a flight played back on another
    // machine, level to within a foot a minute, ask in other words.
    out += "  Heading " + whole(now.heading_deg) + " true at " + whole(now.airspeed_kts) +
           " kt, vertical speed " + whole(now.vertical_speed_fpm) +
           " ft a minute (negative descending).\n";
    out += std::string("  The engine is ") + (now.engine_running ? "running" : "stopped") + ".\n";
    if (now.gliding_kts) {
        out += "  Gliding at " + whole(*now.gliding_kts) + " kt.\n";
    }
    if (now.route.empty()) {
        out += "\nNo route is flown: the autopilot holds what the aircraft is doing.\n";
    } else {
        out += "\nThe route being flown, from the waypoint it is flying to now:\n\n";
        for (const sim::Waypoint& w : now.route) {
            out += waypoint_line(w) + "\n";
        }
    }
    if (!now.fields.empty()) {
        out += "\nRunways nearby, nearest first: the airport, and a runway line - the landing "
               "threshold, its elevation in feet, heading and length in metres:\n\n";
        for (const world::RunwayEnd& end : now.fields) {
            const double d = sim::distance_m(now.latitude_deg, now.longitude_deg, end.latitude_deg,
                                             end.longitude_deg);
            const double bearing = sim::bearing_deg(now.latitude_deg, now.longitude_deg,
                                                    end.latitude_deg, end.longitude_deg);
            out += end.airport + " " + runway_line(end) + ", " + fixed(d / 1000.0, 1) +
                   " km away on a bearing of " + whole(bearing) + "\n";
        }
    }
    return out;
}

Change read_change(const Brief& b, const Situation& now, const std::string& answer) {
    Change out;
    out.text = unfenced(answer);
    std::istringstream in(out.text);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        if (std::string t = trimmed(line); !t.empty()) {
            lines.push_back(t);
        }
    }
    if (lines.size() == 1 && lines[0] == "keep") {
        return out;
    }
    if (lines.empty()) {
        throw sim::FlightPlanError("the answer is empty: `keep`, or a route");
    }
    out.keep = false;
    std::string route;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::istringstream words(lines[i]);
        std::string word;
        words >> word;
        if (word == "glide") {
            double kts = 0.0;
            std::string more;
            if (i != 0 || !(words >> kts) || (words >> more)) {
                throw sim::FlightPlanError("`glide AIRSPEED_KT` is the first line of a route, "
                                           "and alone on it: `" + lines[i] + "`");
            }
            out.glide_kts = kts;
        } else if (word == "waypoint" || word == "orbit") {
            route += lines[i] + "\n";
        } else if (word == "keep") {
            throw sim::FlightPlanError("`keep` is an answer alone, not part of a route");
        } else {
            throw sim::FlightPlanError("`" + lines[i] +
                                       "` is none of keep, glide, waypoint or orbit");
        }
    }
    // The route as a plan flown from where the aircraft is: the plan's own
    // reading checks each line, and refuses an orbit too tight for its speed.
    const std::string plan = "aircraft " + b.aircraft + "\nstart " + fixed(now.latitude_deg, 6) +
                             " " + fixed(now.longitude_deg, 6) + " " + whole(now.altitude_ft) +
                             " " + whole(now.heading_deg) + " " +
                             whole(std::max(now.airspeed_kts, 1.0)) + "\n" + route;
    out.plan = sim::parse_flight_plan(plan);
    return out;
}

std::string change_refusal(const Brief& b, const Situation& now, const Change& change) {
    if (change.keep) {
        if (!now.engine_running && !now.gliding_kts) {
            return "the engine has stopped and nothing glides: the route must glide";
        }
        return {};
    }
    if (change.glide_kts) {
        if (now.engine_running) {
            return "the engine is running: a glide is only for an engine that has stopped";
        }
        if (*change.glide_kts < b.approach_kts - 0.5 || *change.glide_kts > b.climb_kts + 0.5) {
            return "a glide at " + whole(*change.glide_kts) + " kt, outside " +
                   whole(b.approach_kts) + " to " + whole(b.climb_kts) + " kt";
        }
    } else if (!now.engine_running) {
        return "the engine has stopped: the route must begin with `glide AIRSPEED_KT`";
    }
    const double least_ft = std::max(0.0, now.ground_ft) + least_height_ft;
    const double slowest = b.approach_kts;
    const double fastest = b.cruise_kts * 1.2;
    for (const sim::Waypoint& w : change.plan.waypoints) {
        // **A glide flies neither a waypoint's height nor its airspeed**, so
        // neither is held to anything - a glide ends low, over its field -
        // but an orbit is flown at the glide's airspeed, and must be wide
        // enough for it.
        if (change.glide_kts) {
            if (w.orbit && w.orbit->radius_m < sim::least_orbit_radius_m(*change.glide_kts)) {
                return "the orbit " + w.name + ", " + whole(w.orbit->radius_m) +
                       " m, is too tight to glide round at " + whole(*change.glide_kts) +
                       " kt: at least " +
                       whole(std::ceil(sim::least_orbit_radius_m(*change.glide_kts))) + " m";
            }
        } else if (w.altitude_ft < least_ft) {
            return w.name + " is at " + whole(w.altitude_ft) + " ft, below " + whole(least_ft) +
                   " ft, 500 ft above the sea and the ground beneath the aircraft";
        }
        if (!change.glide_kts && (w.airspeed_kts < slowest - 0.5 || w.airspeed_kts > fastest + 0.5)) {
            return w.name + " is flown at " + whole(w.airspeed_kts) + " kt, outside " +
                   whole(slowest) + " to " + whole(fastest) + " kt";
        }
        const double away =
            sim::distance_m(now.latitude_deg, now.longitude_deg, w.latitude_deg, w.longitude_deg);
        if (away > farthest_m) {
            return w.name + " is " + whole(away / 1000.0) + " km from the aircraft, more than " +
                   whole(farthest_m / 1000.0);
        }
    }
    return {};
}

Change decide(Provider& provider, const Brief& brief, const Situation& now) {
    const std::string instructions = copilot_instructions();
    std::vector<Turn> conversation{{"user", situation_text(brief, now)}};
    std::vector<std::string> refused;
    for (int attempt = 1; attempt <= most_attempts; ++attempt) {
        const std::string answer = provider.answer(instructions, conversation);
        std::string why;
        Change change;
        try {
            change = read_change(brief, now, answer);
            why = change_refusal(brief, now, change);
        } catch (const sim::FlightPlanError& e) {
            why = e.what();
        }
        if (why.empty()) {
            change.attempts = attempt;
            change.refused = std::move(refused);
            return change;
        }
        refused.push_back(why);
        conversation.push_back({"assistant", answer});
        conversation.push_back(
            {"user", "That cannot be flown: " + why + ". Answer again, whole."});
    }
    std::string all;
    for (const std::string& why : refused) {
        all += "\n  " + why;
    }
    throw ProviderError(provider.name() + "'s " + std::to_string(most_attempts) +
                        " answers were each refused:" + all);
}

Copilot::Copilot(std::unique_ptr<Provider> provider, Brief brief)
    : provider_(std::move(provider)), brief_(std::move(brief)) {}

Copilot::~Copilot() {
    if (pending_.valid()) {
        pending_.wait();
    }
}

bool Copilot::ask(Situation now) {
    if (pending_.valid()) {
        return false;
    }
    pending_ = std::async(std::launch::async, [this, now = std::move(now)] {
        return decide(*provider_, brief_, now);
    });
    return true;
}

std::optional<Change> Copilot::answered() {
    if (!pending_.valid() ||
        pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return std::nullopt;
    }
    return pending_.get();
}

} // namespace glideslope::copilot
