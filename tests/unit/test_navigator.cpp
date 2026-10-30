#include "harness.hpp"
#include "copilot/copilot.hpp"
#include "frontend/briefs.hpp"

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/catalogue.hpp"
#include "sim/controller.hpp"
#include "sim/departure.hpp"
#include "sim/figures.hpp"
#include "sim/lander.hpp"
#include "sim/navigator.hpp"
#include "sim/orbit_trial.hpp"
#include "sim/terrain.hpp"
#include "sim/turn_laws.hpp"
#include "sim/weather.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <string>
#include <tuple>
#include <vector>

using glideslope::sim::FlightPlan;
using glideslope::sim::FlightPlanError;
using glideslope::sim::parse_flight_plan;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;

std::string plan_text(const std::string& name) {
    std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_PLANS_DIR) / name,
                     std::ios::binary);
    check(static_cast<bool>(in), "can read " + name);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

// Whether `text` is refused, naming `says` in why.
bool refused(const std::string& text, const std::string& says) {
    try {
        parse_flight_plan(text);
    } catch (const FlightPlanError& e) {
        return std::string(e.what()).find(says) != std::string::npos;
    }
    return false;
}

// Which end of the speeds a plan may ask the tightest-orbit tests fly.
enum class End { slowest, fastest };

// **Every aircraft in `space` round the tightest orbit a plan may ask at the
// slowest or the fastest speed a plan may ask of it** - its figures file's
// `<plan_speeds>`, which the planner, the copilot's routes and every plan
// file are held to - both ways round, in calm air and in a 10 kt wind from
// the west, across the circle: from 2 km outside it, heading for its centre,
// twice round and on (sim/orbit_trial.hpp). Each must hold its height within
// 50 ft and its speed within 5 kt from its first quarter-turn, and its
// circle from its first half-turn (the join: OrbitFlown) within `within_m`,
// or, where given, `within_fraction` of its radius. **The join is held too,
// more loosely**, from first reaching the circle: no more than 35% of the
// radius inside it nor 20% outside, or 150 m either way where that is more:
// it is on its circle within 100 m of it. A jet at 360 kt goes 30% inside
// and 15% outside, turning onto its circle from 2 km out heading for its
// centre; the Cub 100 m outside its 268 m.
constexpr double join_inside = 0.35;
constexpr double join_outside = 0.20;
constexpr double join_least_m = 150.0;
// The space is every case of every aircraft in it, and every one is flown.
void fly_the_tightest_orbits(const std::vector<glideslope::sim::CatalogueEntry>& space, End end,
                             double within_m, double within_fraction = 0.0,
                             const std::vector<bool>& ways = {false, true}) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    std::size_t flown = 0;
    std::string failures;
    for (const auto& entry : space) {
        const glideslope::sim::PlanSpeeds speeds =
            glideslope::sim::plan_speeds(data, entry.model);
        const double kts = end == End::slowest ? speeds.slowest_kts : speeds.fastest_kts;
        check(kts == std::round(kts), entry.id + "'s speeds are whole knots, as a plan writes them");
        for (const bool right : ways) {
        for (const bool windy : {false, true}) {
            glideslope::sim::OrbitTrial trial;
            trial.airspeed_kts = kts;
            trial.right = right;
            trial.windy = windy;
            const glideslope::sim::OrbitFlown f =
                glideslope::sim::fly_tightest_orbit(data, entry, trial);
            const double circle_m = within_fraction > 0.0 ? within_fraction * f.radius_m : within_m;
            char which[500];
            std::snprintf(which, sizeof which,
                          "%s round %.0f m at %.0f kt, %s, %s: %.2f turns, %.0f to %.0f m from "
                          "the centre (%+.0f to %+.0f, %.1f%%), joined %.0f%% inside to %.0f%% "
                          "outside, within %.0f ft, %.0f to %.0f kt",
                          entry.id.c_str(), f.radius_m, kts, right ? "right" : "left",
                          windy ? "in a 10 kt wind" : "in calm air", f.turns, f.nearest_m,
                          f.farthest_m, f.nearest_m - f.radius_m, f.farthest_m - f.radius_m,
                          100.0 * std::max(f.radius_m - f.nearest_m, f.farthest_m - f.radius_m) /
                              f.radius_m,
                          100.0 * (f.radius_m - f.join_nearest_m) / f.radius_m,
                          100.0 * (f.join_farthest_m - f.radius_m) / f.radius_m,
                          f.worst_height_ft, f.slowest_kts, f.fastest_kts);
            std::fprintf(stderr, "%s\n", which);
            const bool on_circle =
                f.nearest_m >= f.radius_m - circle_m && f.farthest_m <= f.radius_m + circle_m;
            const bool joined =
                f.join_nearest_m >= f.radius_m - std::max(join_inside * f.radius_m, join_least_m) &&
                f.join_farthest_m <= f.radius_m + std::max(join_outside * f.radius_m, join_least_m);
            if (!f.held(kts) || !on_circle || !joined) {
                failures += std::string("\n  ") + which;
            }
            ++flown;
        }
        }
    }
    const std::size_t cases = space.size() * ways.size() * 2;
    std::fprintf(stderr, "%zu aircraft x %zu ways round x 2 airs = %zu orbits: %zu flown\n",
                 space.size(), ways.size(), cases, flown);
    check(flown == cases && flown > 0,
          "every case flown: " + std::to_string(flown) + " of " + std::to_string(cases));
    check(failures.empty(),
          "each twice round and on, its height within 50 ft, its speed within 5 kt and its "
          "circle within " +
              (within_fraction > 0.0 ? std::to_string(within_fraction * 100.0) + "% of its radius"
                                     : std::to_string(within_m) + " m") +
              ":" + failures);
}

} // namespace

GLIDESLOPE_TEST(a_flight_plan_is_read_from_its_file_and_refused_where_it_is_wrong) {
    const FlightPlan plan = parse_flight_plan(plan_text("sydney-harbour.plan"));
    check(plan.aircraft == "c172p", "the plan is the Cessna's");
    check(plan.start && plan.start->latitude_deg == -33.905 &&
              plan.start->longitude_deg == 151.290 &&
              plan.start->altitude_ft == 3000.0 && plan.start->heading_deg == 0.0 &&
              plan.start->airspeed_kts == 100.0,
          "it starts off Bondi at 3,000 ft, heading north at 100 kt");
    check(plan.waypoints.size() == 4 && plan.waypoints[0].name == "THE_HEADS" &&
              plan.waypoints[1].name == "BRIDGE" &&
              plan.waypoints[2].name == "OLYMPIC" &&
              plan.waypoints[3].name == "AIRPORT",
          "four waypoints, in order");
    check(plan.waypoints[3].latitude_deg == -33.946 &&
              plan.waypoints[3].longitude_deg == 151.177 &&
              plan.waypoints[3].altitude_ft == 3000.0 &&
              plan.waypoints[3].airspeed_kts == 90.0,
          "each waypoint's place, altitude and airspeed");

    check(refused("aircraft c172p\nwaypoint A -33 151 3000 100\nfly home\n",
                  "line 3: no command \"fly\""),
          "a command it does not know, by its line");
    check(refused("aircraft c172p\nwaypoint A -33 151 3000\n", "line 2: waypoint NAME"),
          "a waypoint missing its airspeed");
    check(refused("aircraft c172p\nwaypoint A -33 151 3000 100 over\n",
                  "line 2: waypoint NAME LATITUDE LONGITUDE ALTITUDE_FT AIRSPEED_KT [flyover]"),
          "a waypoint whose seventh word is not flyover");
    {
        const FlightPlan marked = parse_flight_plan(
            "aircraft c172p\nwaypoint A -33 151 3000 100 flyover\nwaypoint B -33.1 151 3000 100\n");
        check(marked.waypoints.size() == 2 && marked.waypoints[0].fly_over &&
                  !marked.waypoints[1].fly_over,
              "a waypoint marked flyover is flown over, and one not marked is flown by");
    }
    check(refused("aircraft c172p\n# a comment\nwaypoint A -95 151 3000 100\n",
                  "line 3: the latitude must be a number"),
          "a latitude off the Earth");
    check(refused("aircraft c172p\nwaypoint A -33 151 3000 fast\n",
                  "the airspeed must be a number"),
          "an airspeed that is not a number");
    check(refused("aircraft c172p\n", "no waypoints"), "a plan with nowhere to go");
    check(refused("waypoint A -33 151 3000 100\n", "names no aircraft"),
          "a plan for no aircraft");
}

GLIDESLOPE_TEST(
    the_navigator_flies_a_plan_past_every_waypoint_in_calm_air_and_in_wind) {
    const FlightPlan plan = parse_flight_plan(plan_text("sydney-harbour.plan"));
    for (const bool windy : {false, true}) {
        glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, plan.aircraft);
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = plan.start->latitude_deg;
        ic.longitude_deg = plan.start->longitude_deg;
        ic.altitude_ft = plan.start->altitude_ft;
        ic.heading_deg = plan.start->heading_deg;
        ic.airspeed_kts = plan.start->airspeed_kts;
        ic.engine_running = true;
        aircraft.initialize(ic);
        if (windy) {
            // 20 kt from the south: behind it up the coast, across it along the
            // river, and against it back to the airport.
            glideslope::sim::Conditions wind;
            wind.wind_north_mps = 20.0 * 1852.0 / 3600.0;
            aircraft.set_weather(
                std::make_shared<glideslope::sim::SteadyWeather>(wind));
        }
        glideslope::sim::Controls controls;
        controls.throttle = 0.7;
        glideslope::sim::Autopilot autopilot(aircraft, controls);
        glideslope::sim::Navigator navigator(aircraft, plan);

        // Each waypoint's closest approach, and the altitude as it is passed:
        // a waypoint flown by is passed before she is nearest it, as the turn
        // onto the next leg begins (sim/navigator.cpp).
        std::vector<double> closest(plan.waypoints.size(),
                                    std::numeric_limits<double>::infinity());
        std::vector<double> altitude_there(plan.waypoints.size(), 0.0);
        std::vector<double> ground_mps_there(plan.waypoints.size(), 0.0);
        std::vector<std::size_t> order;
        int steps = 0;
        const int most_steps = 30 * 60 * steps_per_second;
        while (!navigator.finished() && steps < most_steps) {
            const std::size_t to = navigator.next();
            autopilot.set(navigator.steer());
            aircraft.set_controls(autopilot.fly());
            aircraft.step();
            ++steps;
            if (navigator.next() != to) {
                order.push_back(to);
                altitude_there[to] = aircraft.property("position/h-sl-ft");
                ground_mps_there[to] = std::hypot(aircraft.property("velocities/v-north-fps"),
                                                  aircraft.property("velocities/v-east-fps")) *
                                       0.3048;
            }
            for (std::size_t i = 0; i < plan.waypoints.size(); ++i) {
                closest[i] = std::min(
                    closest[i],
                    glideslope::sim::distance_m(aircraft.property("position/lat-geod-deg"),
                                                aircraft.property("position/long-gc-deg"),
                                                plan.waypoints[i].latitude_deg,
                                                plan.waypoints[i].longitude_deg));
            }
        }
        const std::string air = windy ? " in a 20 kt wind" : " in calm air";
        check(navigator.finished(), "the plan is flown to its end" + air + " in " +
                                        std::to_string(steps / steps_per_second) +
                                        " s");
        check(order == std::vector<std::size_t>{0, 1, 2, 3},
              "every waypoint is passed, in order" + air);
        // Stated from what was measured (PROJECT_STATUS.md): each waypoint passed
        // within 100 m, at its altitude within 50 ft - **and a waypoint flown
        // by within 100 m of where its fly-by arc passes it**: the arc the
        // navigator flies (sim::fly_by), at her speed over the ground as she
        // passed it, whose centre is sqrt(meets^2 + r^2) from the waypoint.
        // THE_HEADS, BRIDGE and OLYMPIC are landmarks the plan flies over: the
        // client's own test holds each within 100 m (client_autopilot.cmake),
        // and flown by, OLYMPIC, turned more than 120 degrees, was passed
        // 1,213 m off even with its miss capped (sim/turn_laws.hpp).
        for (std::size_t i = 0; i < plan.waypoints.size(); ++i) {
            double arc_passes_m = 0.0;
            const auto& w = plan.waypoints;
            if (i + 1 < w.size() && !w[i].fly_over) {
                const double from_lat = i == 0 ? plan.start->latitude_deg : w[i - 1].latitude_deg;
                const double from_lon =
                    i == 0 ? plan.start->longitude_deg : w[i - 1].longitude_deg;
                const double a_deg = std::remainder(
                    glideslope::sim::bearing_deg(w[i].latitude_deg, w[i].longitude_deg,
                                                 w[i + 1].latitude_deg, w[i + 1].longitude_deg) -
                        glideslope::sim::bearing_deg(w[i].latitude_deg, w[i].longitude_deg,
                                                     from_lat, from_lon) -
                        180.0,
                    360.0);
                const glideslope::sim::FlyBy turn = glideslope::sim::fly_by(
                    ground_mps_there[i], a_deg,
                    glideslope::sim::distance_m(from_lat, from_lon, w[i].latitude_deg,
                                                w[i].longitude_deg),
                    glideslope::sim::distance_m(w[i].latitude_deg, w[i].longitude_deg,
                                                w[i + 1].latitude_deg, w[i + 1].longitude_deg));
                if (turn.lead_m > 0.0) {
                    arc_passes_m = std::hypot(turn.meets_m, turn.radius_m) - turn.radius_m;
                }
            }
            std::fprintf(stderr,
                         "%s%s: passed %.1f m off (its arc %.1f), at %.0f ft (%.0f planned)\n",
                         plan.waypoints[i].name.c_str(), air.c_str(), closest[i], arc_passes_m,
                         altitude_there[i], plan.waypoints[i].altitude_ft);
            check(std::abs(closest[i] - arc_passes_m) <= 100.0 &&
                      std::abs(altitude_there[i] - plan.waypoints[i].altitude_ft) <=
                          50.0,
                  plan.waypoints[i].name + air + ": passed " +
                      std::to_string(closest[i]) + " m off (within 100 of its arc's " +
                      std::to_string(arc_passes_m) + "), at " +
                      std::to_string(altitude_there[i]) + " ft (within 50 of " +
                      std::to_string(plan.waypoints[i].altitude_ft) + ")");
        }
    }
}

GLIDESLOPE_TEST(an_orbit_and_a_take_off_are_read_from_a_plan_and_refused_where_they_are_wrong) {
    const FlightPlan plan = parse_flight_plan(
        "aircraft c172p\n"
        "runway 16R -33.9361 151.1702 21 160 2438\n"
        "takeoff 1000\n"
        "orbit CBD -33.8688 151.2093 1500 3000 90 2 left\n"
        "waypoint HOME -33.9 151.2 3000 100\n");
    check(!plan.start && plan.takeoff && plan.takeoff->runway.name == "16R" &&
              plan.takeoff->runway.threshold_lat_deg == -33.9361 &&
              plan.takeoff->runway.threshold_lon_deg == 151.1702 &&
              plan.takeoff->runway.elevation_ft == 21.0 &&
              plan.takeoff->runway.heading_deg == 160.0 &&
              plan.takeoff->runway.length_m == 2438.0 && plan.takeoff->to_ft == 1000.0,
          "the runway and the height it takes off to");
    check(plan.waypoints.size() == 2 && plan.waypoints[0].orbit &&
              plan.waypoints[0].name == "CBD" && plan.waypoints[0].latitude_deg == -33.8688 &&
              plan.waypoints[0].orbit->radius_m == 1500.0 &&
              plan.waypoints[0].altitude_ft == 3000.0 &&
              plan.waypoints[0].airspeed_kts == 90.0 && plan.waypoints[0].orbit->turns == 2 &&
              !plan.waypoints[0].orbit->right && !plan.waypoints[1].orbit,
          "the orbit, its radius, turns and way round, and a waypoint after it");

    const std::string head = "aircraft c172p\nwaypoint A -33 151 3000 100\n";
    const std::string runway = "runway 16R -33.9361 151.1702 21 160 2438\n";
    // Every way the new lines are refused, and how many.
    const std::vector<std::pair<std::string, std::string>> wrong{
        {head + "orbit A -33 151 1500 3000 90 2\n", "orbit NAME"},
        {head + "orbit A -33 151 1500 3000 90 2 sideways\n", "orbit NAME"},
        {head + "orbit A -33 151 50 3000 90 2 left\n", "the radius must be a number"},
        {head + "orbit A -33 151 1500 3000 90 1.5 left\n", "the turns must be whole"},
        {head + "orbit A -33 151 1500 3000 90 -1 left\n", "the turns must be a number"},
        {head + "runway 16R -33.9 151.2 21 160\ntakeoff 1000\n", "runway NAME"},
        {head + "runway 16R -33.9 151.2 21 400 2438\ntakeoff 1000\n",
         "the heading must be a number"},
        {head + runway + "takeoff\n", "takeoff HEIGHT_FT"},
        {head + runway + "takeoff 20\n", "the height must be a number"},
        {head + runway, "a runway and no take-off"},
        {head + "takeoff 1000\n", "no runway to take off from"},
        {head + runway + "takeoff 1000\nstart -33 151 3000 0 100\n",
         "both starts in the air and takes off"},
        {head + "orbit A -33 151 1100 3000 90 2 left\n", "too tight to fly at 90 kt: at least 1172 m"},
        {head + runway + runway + "takeoff 1000\n", "a second runway line"},
        {head + runway + "takeoff 1000\ntakeoff 800\n", "a second takeoff line"},
        {head + "start -33 151 3000 0 100\nstart -33 151 3000 0 100\n", "a second start line"},
        {head + "aircraft pa28\n", "a second aircraft line"},
    };
    std::size_t refusals = 0;
    for (const auto& [text, says] : wrong) {
        check(refused(text, says), "refused, saying \"" + says + "\":\n" + text);
        ++refusals;
    }
    check(refusals == 17, "seventeen ways wrong, and every one refused");
}

GLIDESLOPE_TEST(the_navigator_flies_an_orbit_round_its_centre_as_often_as_asked_either_way_in_calm_air_and_in_wind_down_to_the_tightest_allowed) {
    // A Cessna 3 km south of the centre of a circle, heading for it, twice
    // round and then on to a waypoint north of it: a circle of 1,500 m, and
    // the tightest a plan may ask at its 90 kt.
    const double tightest_m = std::ceil(glideslope::sim::least_orbit_radius_m(90.0));
    std::size_t flown = 0;
    for (const double radius_m : {1500.0, tightest_m}) {
    for (const bool right : {false, true}) {
        for (const bool windy : {false, true}) {
            const FlightPlan plan = parse_flight_plan(
                std::string("aircraft c172p\nstart -33.90 151.20 3000 0 90\n"
                            "orbit CBD -33.8688 151.2093 ") +
                std::to_string(static_cast<int>(radius_m)) + " 3000 90 2 " +
                (right ? "right" : "left") + "\nwaypoint NORTH -33.80 151.2093 3000 90\n");
            glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, plan.aircraft);
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = plan.start->latitude_deg;
            ic.longitude_deg = plan.start->longitude_deg;
            ic.altitude_ft = plan.start->altitude_ft;
            ic.heading_deg = plan.start->heading_deg;
            ic.airspeed_kts = plan.start->airspeed_kts;
            ic.engine_running = true;
            aircraft.initialize(ic);
            if (windy) {
                // 20 kt from the west: across the circle, so that one side of
                // it is flown downwind and the other up.
                glideslope::sim::Conditions wind;
                wind.wind_east_mps = 20.0 * 1852.0 / 3600.0;
                aircraft.set_weather(
                    std::make_shared<glideslope::sim::SteadyWeather>(wind));
            }
            glideslope::sim::Controls controls;
            controls.throttle = 0.7;
            glideslope::sim::Autopilot autopilot(aircraft, controls);
            glideslope::sim::Navigator navigator(aircraft, plan);

            const auto& centre = plan.waypoints[0];
            double nearest_m = std::numeric_limits<double>::infinity();
            double farthest_m = 0.0;
            double most_turns = 0.0;
            double worst_altitude_ft = 0.0;
            // How the bearing from the centre turned while circling: summed
            // clockwise, positive.
            double clockwise_deg = 0.0;
            double last_around = 0.0;
            bool was_circling = false;
            int steps = 0;
            const int most_steps = 20 * 60 * steps_per_second;
            while (navigator.next() == 0 && steps < most_steps) {
                autopilot.set(navigator.steer());
                aircraft.set_controls(autopilot.fly());
                aircraft.step();
                ++steps;
                if (!navigator.circling()) {
                    was_circling = false;
                    continue;
                }
                const double lat = aircraft.property("position/lat-geod-deg");
                const double lon = aircraft.property("position/long-gc-deg");
                const double around = glideslope::sim::bearing_deg(
                    centre.latitude_deg, centre.longitude_deg, lat, lon);
                if (was_circling) {
                    clockwise_deg += std::remainder(around - last_around, 360.0);
                }
                last_around = around;
                was_circling = true;
                most_turns = std::max(most_turns, navigator.turns_flown());
                // Held to the circle once it has joined it: after its first
                // quarter-turn, the 90 degrees it turns through to join.
                if (navigator.turns_flown() >= 0.25) {
                    const double d = glideslope::sim::distance_m(
                        centre.latitude_deg, centre.longitude_deg, lat, lon);
                    nearest_m = std::min(nearest_m, d);
                    farthest_m = std::max(farthest_m, d);
                    worst_altitude_ft = std::max(
                        worst_altitude_ft,
                        std::abs(aircraft.property("position/h-sl-ft") - centre.altitude_ft));
                }
            }
            const std::string which = std::to_string(static_cast<int>(radius_m)) + " m, " +
                                      (right ? "right" : "left") +
                                      (windy ? ", in a 20 kt wind" : ", in calm air");
            std::fprintf(stderr,
                         "orbit %s: %.2f turns in %d s, %.0f to %.0f m from the centre, "
                         "altitude within %.0f ft, turned %.0f degrees clockwise\n",
                         which.c_str(), most_turns, steps / steps_per_second, nearest_m,
                         farthest_m, worst_altitude_ft, clockwise_deg);
            check(navigator.next() == 1, "the orbit is left for the next waypoint, " + which);
            check(most_turns >= 1.99 && most_turns <= 2.01,
                  "twice round, " + which + ": " + std::to_string(most_turns));
            check(right ? clockwise_deg > 700.0 : clockwise_deg < -700.0,
                  "round the way asked, " + which + ": " + std::to_string(clockwise_deg) +
                      " degrees clockwise");
            // Stated from what was measured (PROJECT_STATUS.md): at worst
            // 61 m inside the tightest circle, in the wind (150 m outside it
            // before the navigator fed the bank forward).
            check(nearest_m >= radius_m - 80.0 && farthest_m <= radius_m + 80.0,
                  "on the circle within 80 m, " + which + ": " + std::to_string(nearest_m) +
                      " to " + std::to_string(farthest_m) + " m");
            check(worst_altitude_ft <= 50.0,
                  "at its altitude within 50 ft, " + which + ": " +
                      std::to_string(worst_altitude_ft));
            ++flown;
        }
    }
    }
    check(flown == 8, "two circles, both ways round, in calm air and in wind: eight orbits flown");
}

GLIDESLOPE_TEST(an_orbit_begun_from_its_centre_counts_its_turns_only_from_its_circle) {
    // A plan that flies to an orbit's centre first - as both models planned
    // "orbit the CBD": a waypoint there, then the orbit round it. From the
    // centre the aircraft is steered out to the circle, and the turns it
    // spirals out through are not counted: it joins the circle within 100 m
    // of it, and is held to it from its first quarter-turn - in calm air and
    // in a 20 kt crosswind, as the orbit flown from outside is. Counted from
    // the centre, a Cessna flew "round" a 521 m circle 164 to 492 m out.
    const double tightest_m = std::ceil(glideslope::sim::least_orbit_radius_m(90.0));
    std::size_t flown = 0;
    for (const double radius_m : {1500.0, tightest_m}) {
        for (const bool right : {false, true}) {
        for (const bool windy : {false, true}) {
            const FlightPlan plan = parse_flight_plan(
                std::string("aircraft c172p\nstart -33.8688 151.2093 3000 0 90\n"
                            "orbit CBD -33.8688 151.2093 ") +
                std::to_string(static_cast<int>(radius_m)) + " 3000 90 1 " +
                (right ? "right" : "left") + "\nwaypoint NORTH -33.80 151.2093 3000 90\n");
            glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, plan.aircraft);
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = plan.start->latitude_deg;
            ic.longitude_deg = plan.start->longitude_deg;
            ic.altitude_ft = plan.start->altitude_ft;
            ic.heading_deg = plan.start->heading_deg;
            ic.airspeed_kts = plan.start->airspeed_kts;
            ic.engine_running = true;
            aircraft.initialize(ic);
            if (windy) {
                glideslope::sim::Conditions wind;
                wind.wind_east_mps = 20.0 * 1852.0 / 3600.0;
                aircraft.set_weather(
                    std::make_shared<glideslope::sim::SteadyWeather>(wind));
            }
            glideslope::sim::Controls controls;
            controls.throttle = 0.7;
            glideslope::sim::Autopilot autopilot(aircraft, controls);
            glideslope::sim::Navigator navigator(aircraft, plan);

            const auto& centre = plan.waypoints[0];
            double joined_at_m = -1.0;
            double nearest_m = std::numeric_limits<double>::infinity();
            double farthest_m = 0.0;
            double most_turns = 0.0;
            int steps = 0;
            const int most_steps = 20 * 60 * steps_per_second;
            while (navigator.next() == 0 && steps < most_steps) {
                autopilot.set(navigator.steer());
                aircraft.set_controls(autopilot.fly());
                aircraft.step();
                ++steps;
                if (!navigator.circling()) {
                    continue;
                }
                const double d = glideslope::sim::distance_m(
                    centre.latitude_deg, centre.longitude_deg,
                    aircraft.property("position/lat-geod-deg"),
                    aircraft.property("position/long-gc-deg"));
                if (joined_at_m < 0.0) {
                    joined_at_m = d;
                }
                most_turns = std::max(most_turns, navigator.turns_flown());
                if (navigator.turns_flown() >= 0.25) {
                    nearest_m = std::min(nearest_m, d);
                    farthest_m = std::max(farthest_m, d);
                }
            }
            const std::string which = std::to_string(static_cast<int>(radius_m)) + " m, " +
                                      (right ? "right" : "left") +
                                      (windy ? ", in a 20 kt wind" : ", in calm air");
            std::fprintf(stderr,
                         "orbit %s from its centre: joined %.0f m out, %.2f turns, %.0f to "
                         "%.0f m from the centre\n",
                         which.c_str(), joined_at_m, most_turns, nearest_m, farthest_m);
            check(navigator.next() == 1, "the orbit is left for the next waypoint, " + which);
            // Either side: since 2026-10-09 it is joined only going its way
            // round, which steered out from the centre may be just outside.
            check(joined_at_m >= radius_m - 101.0 && joined_at_m <= radius_m + 101.0,
                  "joined the circle within 100 m of it, " + which + ": " +
                      std::to_string(joined_at_m) + " m out");
            check(most_turns >= 0.99 && most_turns <= 1.01,
                  "once round, " + which + ": " + std::to_string(most_turns));
            check(nearest_m >= radius_m - 110.0 && farthest_m <= radius_m + 110.0,
                  "on the circle within 110 m, " + which + ": " + std::to_string(nearest_m) +
                      " to " + std::to_string(farthest_m) + " m");
            ++flown;
        }
        }
    }
    check(flown == 8, "two circles, both ways round, in calm air and in wind, each begun from "
                      "its centre: eight flown");
}

namespace {

// **The tightest-orbit tests split the catalogue by class**, four ways, so
// that no one of them flies long on CI: every class in exactly one group, which
// the light aeroplanes' test checks.
using glideslope::sim::AircraftClass;
const std::vector<std::vector<AircraftClass>> tightest_orbit_groups = {
    {AircraftClass::light_aircraft},
    {AircraftClass::airliner, AircraftClass::business_jet},
    {AircraftClass::fighter, AircraftClass::bomber},
    {AircraftClass::second_world_war, AircraftClass::seaplane},
};

} // namespace

// **What a plan may ask of each aircraft is in its figures file**, and the
// slowest is never below its approach speed - the slowest any plan was let
// fly before - nor the fastest above a fifth over its start speed, the
// fastest any plan was let fly before. Every aircraft in the catalogue.
GLIDESLOPE_TEST(every_aircraft_gives_the_speeds_a_plan_may_fly_it_at_none_beyond_what_plans_were_let_fly_before) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    std::size_t covered = 0;
    std::size_t without_approach = 0;
    const auto catalogue = glideslope::sim::read_catalogue(data);
    for (const auto& e : catalogue) {
        const glideslope::sim::PlanSpeeds speeds = glideslope::sim::plan_speeds(data, e.model);
        std::printf("%s: %.0f to %.0f kt\n", e.id.c_str(), speeds.slowest_kts, speeds.fastest_kts);
        check(speeds.slowest_kts > 0.0 && speeds.fastest_kts > speeds.slowest_kts,
              e.id + "'s plan speeds rise from above 0");
        check(speeds.fastest_kts <= std::round(1.2 * e.start_airspeed_kts),
              e.id + "'s fastest is no more than a fifth over its start speed");
        try {
            const double vref = glideslope::sim::approach_speeds(data, e.model).vref_kts;
            check(speeds.slowest_kts >= std::round(vref),
                  e.id + "'s slowest is not below its approach speed, " + std::to_string(vref));
        } catch (const std::runtime_error&) {
            // The 747-400 and the F-22 publish no stall speed, so have none.
            ++without_approach;
        }
        ++covered;
    }
    check(covered == catalogue.size() && covered == 16,
          "every one of the sixteen aircraft: " + std::to_string(covered));
    check(without_approach == 2, "two without an approach speed, the 747-400 and the F-22: " +
                                     std::to_string(without_approach));
}

// **A plan file is held to the aircraft flown and to its speeds as it is
// read** (sim::refuse_what_it_cannot_fly, which the server, the client and
// `glideslope_cli fly-plan` each call), as a model's plan is: the A320 at
// her approach speed, a flaps-down figure, at which she comes down 500 ft
// clean, at a waypoint or at the start; faster than the Mosquito makes at
// 3,000 ft; and the Cessna's plan flown in an A320. Every plan file in the
// tree is walked: each is flown within its speeds, but for the one kept to
// be refused.
GLIDESLOPE_TEST(a_plan_file_asking_a_speed_its_aircraft_cannot_hold_clean_is_refused_and_none_in_the_data_does) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const auto verdict = [&](const std::string& text, const std::string& flown = "") {
        try {
            const FlightPlan plan = parse_flight_plan(text);
            glideslope::sim::refuse_what_it_cannot_fly(data, plan,
                                                       flown.empty() ? plan.aircraft : flown);
        } catch (const FlightPlanError& e) {
            return std::string(e.what());
        }
        return std::string();
    };
    const auto a320 = [](double kts, double start_kts = 250) {
        return "aircraft a320\nstart -33.95 151.2093 3000 0 " + std::to_string(start_kts) +
               "\nwaypoint A -33.90 151.2093 3000 250\n"
               "orbit CBD -33.8688 151.2093 14000 3000 " + std::to_string(kts) + " 2 left\n";
    };
    const std::string slow = verdict(a320(147));
    check(slow.find("CBD is flown at 147 kt, outside 162 to 300 kt") != std::string::npos,
          "the A320 at 147 kt is refused, naming the waypoint and her speeds: " + slow);
    check(verdict(a320(162)).empty() && verdict(a320(300)).empty(),
          "at 162 and at 300 kt, her slowest and fastest, she is not");
    check(verdict(a320(161)).find("outside 162 to 300") != std::string::npos &&
              verdict(a320(301)).find("outside 162 to 300") != std::string::npos,
          "a knot either side of them she is");
    const std::string start = verdict(a320(250, 147));
    check(start.find("the start is at 147 kt, outside 162 to 300 kt") != std::string::npos,
          "started at 147 kt she is refused, the start named: " + start);
    const std::string fast =
        verdict("aircraft mosquito-fb6\nwaypoint A -33.90 151.2093 3000 270\n");
    check(fast.find("A is flown at 270 kt, outside 129 to 264 kt") != std::string::npos,
          "the Mosquito at 270 kt, more than she makes, is refused: " + fast);
    const std::string other = verdict(plan_text("sydney-harbour.plan"), "a320");
    check(other.find("the plan is for the c172p, and the aircraft flown is the a320") !=
              std::string::npos,
          "the Cessna's plan flown in an A320 is refused: " + other);
    check(verdict(plan_text("sydney-harbour.plan"), "c172p").empty(),
          "and flown in the Cessna it is not");

    // Every .plan file in the tree's assets and tests. Refused, only those
    // named here, each with why; tests/data/copilot holds recordings, not
    // plan files, whose plans the planner checks as each plays back.
    const std::filesystem::path project(GLIDESLOPE_TEST_PROJECT_DIR);
    const std::vector<std::string> kept_to_be_refused = {
        "tests/data/plans/a320-at-its-approach-speed.plan"}; // the CLI's refusal test's
    std::vector<std::string> found;
    for (const char* top : {"assets", "tests"}) {
        for (const auto& file : std::filesystem::recursive_directory_iterator(project / top)) {
            if (file.path().extension() != ".plan") {
                continue;
            }
            const std::string name = file.path().lexically_relative(project).generic_string();
            found.push_back(name);
            std::ifstream in(file.path(), std::ios::binary);
            const std::string why = verdict(std::string(std::istreambuf_iterator<char>(in), {}));
            const bool kept = std::find(kept_to_be_refused.begin(), kept_to_be_refused.end(),
                                        name) != kept_to_be_refused.end();
            std::printf("%s: %s\n", name.c_str(), why.empty() ? "flown" : why.c_str());
            check(kept ? !why.empty() : why.empty(),
                  name + (kept ? " is refused, as kept to be" : " is flown within its speeds") +
                      ": " + (why.empty() ? "it was not refused" : why));
        }
    }
    for (const std::string& name : kept_to_be_refused) {
        check(std::find(found.begin(), found.end(), name) != found.end(),
              "what is kept to be refused is in the tree: " + name);
    }
    check(std::find(found.begin(), found.end(), "assets/plans/sydney-harbour.plan") != found.end(),
          "the walk reaches the data's plans");
}

// **A slowest or fastest written too cautiously, or by hand, is caught**:
// `glideslope_cli plan-speeds` writes the first speed that held with 5 kt
// to spare. Past the fastest, 10 kt is the last speed it saw not hold - a
// matter of power, which moves by tenths of a knot between platforms.
// **Past the slowest, 15 kt: the step under the first speed that held, not
// the step next to it.** The step next to it is the cliff, where a wing
// just departs or just holds, and which side it falls varies by platform:
// with the trials' gear up (2026-10-10), the A320 at 152 kt departed turning
// right on Linux and held all four ways on macOS (CI run 37972664910), and
// the 747-400 held 210 on macOS. 15 past catches a figure written 10 kt or
// more too cautiously (the A320's put at 172, it holds 157: seen red); one
// written 5 kt too cautiously lands on the cliff and is caught where the
// platform falls that side. plan-speeds' own 5 kt keeps every figure a step
// clear of the cliff. Every aircraft whose slowest is above where its
// search begins - its approach speed, at least for the weight a plan flies
// it at and 10 kt over its stall warning there (sim::least_plan_slowest_kts,
// since 2026-10-10) - or that has none, is flown there, and every one whose
// fastest is below a fifth over its start speed: none may hold what a plan
// asks (sim::holds_plan_speed - the orbit four ways, a heading in calm air
// and a crosswind). The rest are at the bound plans were held to before,
// which nothing is asked past.
namespace {

// One aircraft of the test below: its slowest tried 15 kt slower where it is
// above where its search begins (or it has none), its fastest 10 kt faster where
// it is below a fifth over its start speed; each bound tried or at the old
// bound, and none held.
enum class Bounds { both, slowest, fastest };
void holds_nothing_one_step_past(const std::string& id, Bounds bounds = Bounds::both) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const auto e = glideslope::sim::find_aircraft(data, id);
    std::string failures;
    std::size_t tried = 0;
    std::size_t at_the_old_bound = 0;
    const glideslope::sim::PlanSpeeds speeds = glideslope::sim::plan_speeds(data, e.model);
    // **Where the search for its slowest begins** (sim::least_plan_slowest_kts):
    // its approach speed, or that for the weight a plan flies it at, or its
    // stall warning there plus the gust allowance, whichever is most. 0 for
    // the 747-400 and the F-22, which have no stall: their slowest was sought.
    const double approach = glideslope::sim::least_plan_slowest_kts(data, e);
    const auto past = [&](double kts, const char* which) {
        // **Past the fastest, power in hand is asked first**: it is the last
        // and cheapest of what holds_plan_speed asks, and where it fails
        // nothing else need be flown - the S.23 at 151 kt held its orbits and
        // its heading, 108 s in linux-debug, and then had 151.9 kt level.
        bool held = false;
        if (kts > speeds.fastest_kts) {
            const double level = glideslope::sim::full_throttle_level_kts(data, e, kts);
            std::printf("  %s %.0f kt: %.1f kt level at full throttle\n", e.id.c_str(), kts,
                        level);
            held = level >= kts + glideslope::sim::plan_speed_power_margin_kts &&
                   glideslope::sim::holds_plan_speed(data, e, kts, [](const std::string& line) {
                       std::printf("  %s\n", line.c_str());
                   });
        } else {
            // **Below the slowest, the heading in a crosswind is asked
            // first**, two minutes each where the orbits are many: the F-22
            // at 245 kt held its four orbits, 69 s in linux-debug, and then
            // swung 1.6 degrees of sideslip in the crosswind.
            held = true;
            for (const bool windy : {false, true}) {
                const auto f = glideslope::sim::fly_heading_in_crosswind(data, e, kts, windy);
                std::printf("  %s %.0f kt heading north, %s: sideslip %+.2f to %+.2f, heading "
                            "within %.2f\n",
                            e.id.c_str(), kts, windy ? "20 kt crosswind" : "calm",
                            f.least_sideslip_deg, f.most_sideslip_deg, f.worst_heading_deg);
                held = held && f.held();
            }
            held = held && glideslope::sim::holds_plan_speed(data, e, kts,
                                                             [](const std::string& line) {
                                                                 std::printf("  %s\n",
                                                                             line.c_str());
                                                             });
        }
        std::printf("%s past its %s, at %.0f kt: %s\n", e.id.c_str(), which, kts,
                    held ? "held" : "not held");
        if (held) {
            failures += "\n  " + e.id + " holds " + std::to_string(kts) + " kt, past its " +
                        which;
        }
        ++tried;
    };
    if (bounds == Bounds::fastest) {
        ++at_the_old_bound; // its own test's
    } else if (speeds.slowest_kts > approach) {
        past(speeds.slowest_kts - 15.0, "slowest");
    } else {
        ++at_the_old_bound;
        std::printf("%s's slowest is at the old bound, where its search begins\n", e.id.c_str());
    }
    if (bounds == Bounds::slowest) {
        ++at_the_old_bound; // its own test's
    } else if (speeds.fastest_kts < std::round(1.2 * e.start_airspeed_kts)) {
        past(speeds.fastest_kts + 10.0, "fastest");
    } else {
        ++at_the_old_bound;
        std::printf("%s's fastest is at the old bound, a fifth over its start speed\n",
                    e.id.c_str());
    }
    check(tried + at_the_old_bound == 2, e.id + "'s slowest and fastest tried or at the old bound");
    check(failures.empty(),
          "none held 15 kt under its slowest nor 10 over its fastest:" + failures);
}

} // namespace

// **The fastest a plan may ask leaves power in hand**: every aircraft flies
// level at full throttle at least 5 kt faster than its figures file's
// fastest (sim::holds_plan_speed). The C182's 144 kt, written before her
// engine was rated at 2,400 rpm, was 0.3 kt short of what she makes, and on
// Windows she swung 150 m off her circle there. Every aircraft is flown.
GLIDESLOPE_TEST(every_aircrafts_fastest_plan_speed_leaves_5_kt_in_hand_at_full_throttle) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const auto catalogue = glideslope::sim::read_catalogue(data);
    std::size_t flown = 0;
    std::string failures;
    for (const auto& e : catalogue) {
        const double fastest = glideslope::sim::plan_speeds(data, e.model).fastest_kts;
        const double level = glideslope::sim::full_throttle_level_kts(data, e, fastest);
        std::printf("%s: fastest %.0f kt, level at full throttle %.1f kt\n", e.id.c_str(),
                    fastest, level);
        if (level < fastest + glideslope::sim::plan_speed_power_margin_kts) {
            failures += "\n  " + e.id + ": " + std::to_string(level) + " kt at full throttle, " +
                        std::to_string(fastest) + " asked";
        }
        ++flown;
    }
    std::printf("%zu aircraft: %zu flown\n", catalogue.size(), flown);
    check(flown == catalogue.size() && flown > 0, "every aircraft flown");
    check(failures.empty(), "each leaves 5 kt in hand:" + failures);
}

namespace {

// **Each aircraft's tightest orbits are flown in a test of its own**, and so
// is one step past its slowest and fastest, so that none flies long on CI:
// by class, the airliners' and business jets' fastest orbits ran past 900 s
// in CI's linux-debug, and every aircraft one step past took 1,282 s. Every
// aircraft in the catalogue has its three, which this list and the test
// below assert. The S.23's and the F-22's, the slowest of them, are split
// again: each orbit each way round, and each bound one step past on its own.
const std::vector<std::string> orbit_tested = {
    "c172p",
    "c182",
    "pa28",
    "j3cub",
    "short_s23",
    "mosquito-fb6",
    "737-300",
    "747-400",
    "787-8",
    "a320",
    "a380",
    "learjet35a",
    "b2",
    "f15c",
    "f22",
    "f35b",
};

// **How near its circle each must stay**, by its tightest-orbit group: the
// light aeroplanes, the Mosquito and the S.23 within 60 m, at either end
// (at worst 50 and 36 m measured, PROJECT_STATUS.md); the jets within 5% of
// the radius at their slowest and 2% at their fastest (at worst 1.3%, the
// A380's 172 m of 13 km at 300 kt, and the fighters' 0.9%).
void fly_its_tightest_orbits(const std::string& id, End end,
                             const std::vector<bool>& ways = {false, true}) {
    const auto entry = glideslope::sim::find_aircraft(
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path(), id);
    const auto in = [&](const std::vector<AircraftClass>& group) {
        return std::find(group.begin(), group.end(), entry.aircraft_class) != group.end();
    };
    if (in(tightest_orbit_groups[1]) || in(tightest_orbit_groups[2])) {
        fly_the_tightest_orbits({entry}, end, 0.0, end == End::slowest ? 0.05 : 0.02, ways);
    } else {
        check(in(tightest_orbit_groups[0]) || in(tightest_orbit_groups[3]),
              id + " is in a tightest-orbit group");
        fly_the_tightest_orbits({entry}, end, 60.0, 0.0, ways);
    }
}

} // namespace

// Claude's plan for the Cessna, 521 m at 60 kt, was flown 94 to 127 m inside
// its circle: the navigator led the tangent by five seconds, more than the
// autopilot needs ahead of it to bank for so tight a circle. Until each had
// its own slowest, a plan could fly a jet clean at its approach speed, a
// flaps-down figure: five of the six airliners and business jets came down
// to the ground round this orbit, and the A320 lost 600 ft; the F-35B came
// down too, and the B-2 stalled turning towards it. At the fastest, the
// navigator turned in towards the circle by 90 degrees a kilometre whatever
// the speed, and the jets swung through their 13 to 19 km circles by up to
// 18% of the radius (3.3 km, the F-15C at 360 kt); it turned in by what
// brought it back in twelve seconds at its airspeed, and since 2026-10-09 asks
// the autopilot for a bank from a loiter law after ArduPilot's (sim/navigator.cpp).
GLIDESLOPE_TEST(every_aircraft_has_its_own_tests_of_its_tightest_orbits_and_one_step_past_them) {
    std::vector<AircraftClass> grouped;
    for (const auto& group : tightest_orbit_groups) {
        grouped.insert(grouped.end(), group.begin(), group.end());
    }
    std::sort(grouped.begin(), grouped.end());
    check(grouped.size() == glideslope::sim::aircraft_class_count &&
              std::adjacent_find(grouped.begin(), grouped.end()) == grouped.end(),
          "the tightest-orbit groups take every class of aircraft, each once: " +
              std::to_string(grouped.size()) + " of " +
              std::to_string(glideslope::sim::aircraft_class_count));
    std::vector<std::string> catalogue;
    for (const auto& e : glideslope::sim::read_catalogue(
             std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path())) {
        catalogue.push_back(e.id);
    }
    std::vector<std::string> tested = orbit_tested;
    std::sort(catalogue.begin(), catalogue.end());
    std::sort(tested.begin(), tested.end());
    check(tested == catalogue && std::adjacent_find(tested.begin(), tested.end()) == tested.end(),
          "each of the " + std::to_string(catalogue.size()) +
              " aircraft has its own tests, once: " + std::to_string(tested.size()));
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_cessna_172p) {
    fly_its_tightest_orbits("c172p", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_cessna_172p) {
    fly_its_tightest_orbits("c172p", End::fastest);
}

GLIDESLOPE_TEST(the_cessna_172p_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("c172p");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_cessna_182s) {
    fly_its_tightest_orbits("c182", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_cessna_182s) {
    fly_its_tightest_orbits("c182", End::fastest);
}

GLIDESLOPE_TEST(the_cessna_182s_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("c182");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_piper_pa28) {
    fly_its_tightest_orbits("pa28", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_piper_pa28) {
    fly_its_tightest_orbits("pa28", End::fastest);
}

GLIDESLOPE_TEST(the_piper_pa28_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("pa28");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_piper_j3_cub) {
    fly_its_tightest_orbits("j3cub", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_piper_j3_cub) {
    fly_its_tightest_orbits("j3cub", End::fastest);
}

GLIDESLOPE_TEST(the_piper_j3_cub_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("j3cub");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_turning_left_by_the_short_s23) {
    fly_its_tightest_orbits("short_s23", End::slowest, {false});
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_turning_right_by_the_short_s23) {
    fly_its_tightest_orbits("short_s23", End::slowest, {true});
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_turning_left_by_the_short_s23) {
    fly_its_tightest_orbits("short_s23", End::fastest, {false});
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_turning_right_by_the_short_s23) {
    fly_its_tightest_orbits("short_s23", End::fastest, {true});
}

GLIDESLOPE_TEST(the_short_s23_holds_nothing_a_plan_asks_one_step_past_its_slowest) {
    holds_nothing_one_step_past("short_s23", Bounds::slowest);
}

GLIDESLOPE_TEST(the_short_s23_holds_nothing_a_plan_asks_one_step_past_its_fastest) {
    holds_nothing_one_step_past("short_s23", Bounds::fastest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_mosquito_fb6) {
    fly_its_tightest_orbits("mosquito-fb6", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_mosquito_fb6) {
    fly_its_tightest_orbits("mosquito-fb6", End::fastest);
}

GLIDESLOPE_TEST(the_mosquito_fb6_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("mosquito-fb6");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_boeing_737_300) {
    fly_its_tightest_orbits("737-300", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_boeing_737_300) {
    fly_its_tightest_orbits("737-300", End::fastest);
}

GLIDESLOPE_TEST(the_boeing_737_300_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("737-300");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_boeing_747_400) {
    fly_its_tightest_orbits("747-400", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_boeing_747_400) {
    fly_its_tightest_orbits("747-400", End::fastest);
}

GLIDESLOPE_TEST(the_boeing_747_400_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("747-400");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_boeing_787_8) {
    fly_its_tightest_orbits("787-8", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_boeing_787_8) {
    fly_its_tightest_orbits("787-8", End::fastest);
}

GLIDESLOPE_TEST(the_boeing_787_8_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("787-8");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_airbus_a320) {
    fly_its_tightest_orbits("a320", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_airbus_a320) {
    fly_its_tightest_orbits("a320", End::fastest);
}

GLIDESLOPE_TEST(the_airbus_a320_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("a320");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_airbus_a380) {
    fly_its_tightest_orbits("a380", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_airbus_a380) {
    fly_its_tightest_orbits("a380", End::fastest);
}

GLIDESLOPE_TEST(the_airbus_a380_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("a380");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_learjet_35a) {
    fly_its_tightest_orbits("learjet35a", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_learjet_35a) {
    fly_its_tightest_orbits("learjet35a", End::fastest);
}

GLIDESLOPE_TEST(the_learjet_35a_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("learjet35a");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_b2a) {
    fly_its_tightest_orbits("b2", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_b2a) {
    fly_its_tightest_orbits("b2", End::fastest);
}

GLIDESLOPE_TEST(the_b2a_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("b2");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_f15c) {
    fly_its_tightest_orbits("f15c", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_f15c) {
    fly_its_tightest_orbits("f15c", End::fastest);
}

GLIDESLOPE_TEST(the_f15c_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("f15c");
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_turning_left_by_the_f22a) {
    fly_its_tightest_orbits("f22", End::slowest, {false});
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_turning_right_by_the_f22a) {
    fly_its_tightest_orbits("f22", End::slowest, {true});
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_turning_left_by_the_f22a) {
    fly_its_tightest_orbits("f22", End::fastest, {false});
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_turning_right_by_the_f22a) {
    fly_its_tightest_orbits("f22", End::fastest, {true});
}

GLIDESLOPE_TEST(the_f22a_holds_nothing_a_plan_asks_one_step_past_its_slowest) {
    holds_nothing_one_step_past("f22", Bounds::slowest);
}

GLIDESLOPE_TEST(the_f22a_holds_nothing_a_plan_asks_one_step_past_its_fastest) {
    holds_nothing_one_step_past("f22", Bounds::fastest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_the_f35b) {
    fly_its_tightest_orbits("f35b", End::slowest);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_the_f35b) {
    fly_its_tightest_orbits("f35b", End::fastest);
}

GLIDESLOPE_TEST(the_f35b_holds_nothing_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    holds_nothing_one_step_past("f35b");
}

namespace {

// **A jet's orbit entered from a waypoint**, as a model plans "orbit the CBD":
// a waypoint, then the orbit (sim::fly_orbit_from_waypoint). Flying Claude's
// plans round 7 and 9.4 km circles, the 747-400 and the F-22A swung 460 m
// off them (PROJECT_STATUS.md, 2026-10-06). Each jet at the speed Claude
// asked of both, 220 kt - or the nearest a plan may ask of it - round the
// tightest circle a plan may ask at it, which is what the planner offers a
// model. **In calm air**, entered from a waypoint at its centre, half way
// out, on the circle and two radii out, arriving at it from every eighth of
// the compass, both ways round. **In a 20 kt wind from the west**, entered
// from its centre and from on the circle arriving from each quarter, both
// ways round. Each joins its circle going its way, holds it within the 100 m
// it joined within from there, within 30 m after the first quarter-turn, and
// its height within 50 ft, once round.
// **The 30 m**: every jet held within 9 m in calm air on Linux; the rest is
// a margin for other platforms' floating point and for the wind, small
// enough that a navigator that let one wander 50 m fails.
// **Left out, by name**: a waypoint at the centre is flown at from north
// alone - from the centre, every other arrival is the same flight turned, in
// calm air; in wind, half way out, two radii out and the diagonal arrivals,
// which calm air covers - the wind changes the speed round, which the
// entries from the centre and on the circle already meet in every quarter.
// **Five tests a jet**, so that none runs long on CI: in calm air, one for
// each quarter of the compass it arrives heading - north and north-east
// (with the centre's), east and south-east, south and south-west, west and
// north-west - and one in the wind. As one test each, three passed ctest's
// 900 s in CI's linux-debug; in halves of the compass, the F-22A's ran 291
// and 315 s locally in linux-debug, four at a time.
// `every_jet_enters_every_orbit_entry_once_across_its_five_tests` holds the
// five to the whole space, each entry once.
const std::vector<std::string> entering_jets = {
    "737-300", "747-400", "787-8", "a320", "a380", "learjet35a", "b2", "f15c", "f22", "f35b",
};
constexpr double entered_within_m = 30.0;
// Joined within 100 m of it (sim/navigator.cpp), it is held to that from there.
constexpr double joined_within_m = 100.0;
constexpr double entry_wind_kts = 20.0;

enum class EntryPart { calm_north, calm_east, calm_south, calm_west, windy };
constexpr EntryPart entry_parts[] = {EntryPart::calm_north, EntryPart::calm_east,
                                     EntryPart::calm_south, EntryPart::calm_west,
                                     EntryPart::windy};

struct EntryCase {
    bool windy = false;
    bool right = false;
    double from_centre = 0.0; // radii
    int eighth = 0;           // arriving at 45 degrees times this
    bool operator<(const EntryCase& o) const {
        return std::tie(windy, right, from_centre, eighth) <
               std::tie(o.windy, o.right, o.from_centre, o.eighth);
    }
};

// The whole space: calm air, both ways round, the centre once and three
// places from eight ways (50); wind, both ways round, the centre once and on
// the circle from four (10).
std::vector<EntryCase> every_entry() {
    std::vector<EntryCase> out;
    for (const bool windy : {false, true}) {
        const std::vector<double> from_centre =
            windy ? std::vector<double>{0.0, 1.0} : std::vector<double>{0.0, 0.5, 1.0, 2.0};
        // Calm air every eighth of the compass, wind every quarter.
        const int step = windy ? 2 : 1;
        for (const bool right : {false, true}) {
            for (const double from : from_centre) {
                for (int eighth = 0; eighth < 8; eighth += step) {
                    if (from == 0.0 && eighth > 0) {
                        continue; // from the centre, flown from north alone (above)
                    }
                    out.push_back({windy, right, from, eighth});
                }
            }
        }
    }
    return out;
}
constexpr std::size_t every_entry_count = 2 * (1 + 3 * 8) + 2 * (1 + 1 * 4);

bool in_part(const EntryCase& c, EntryPart part) {
    switch (part) {
    case EntryPart::calm_north:
        return !c.windy && c.eighth / 2 == 0;
    case EntryPart::calm_east:
        return !c.windy && c.eighth / 2 == 1;
    case EntryPart::calm_south:
        return !c.windy && c.eighth / 2 == 2;
    case EntryPart::calm_west:
        return !c.windy && c.eighth / 2 == 3;
    case EntryPart::windy:
        return c.windy;
    }
    return false;
}

std::vector<EntryCase> entries_in(EntryPart part) {
    std::vector<EntryCase> out;
    for (const EntryCase& c : every_entry()) {
        if (in_part(c, part)) {
            out.push_back(c);
        }
    }
    return out;
}

void enter_its_orbits(const std::string& id, EntryPart part) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const auto entry = glideslope::sim::find_aircraft(data, id);
    const glideslope::sim::PlanSpeeds speeds = glideslope::sim::plan_speeds(data, entry.model);
    const double kts = std::clamp(220.0, speeds.slowest_kts, speeds.fastest_kts);
    const double radius_m = std::ceil(glideslope::sim::least_orbit_radius_m(kts));
    const std::vector<EntryCase> cases = entries_in(part);
    std::size_t flown = 0;
    double worst_m = 0.0;
    double worst_join_m = 0.0;
    std::string failures;
    for (const EntryCase& c : cases) {
        glideslope::sim::OrbitEntry trial;
        trial.airspeed_kts = kts;
        trial.radius_m = radius_m;
        trial.from_centre = c.from_centre;
        trial.arriving_deg = 45.0 * c.eighth;
        trial.right = c.right;
        trial.wind_kts = c.windy ? entry_wind_kts : 0.0;
        const glideslope::sim::OrbitEntered f =
            glideslope::sim::fly_orbit_from_waypoint(data, entry, trial);
        ++flown;
        const double off_m = f.worst_off_m(radius_m);
        const double join_off_m = f.worst_join_off_m(radius_m);
        worst_m = std::max(worst_m, off_m);
        worst_join_m = std::max(worst_join_m, join_off_m);
        char which[400];
        std::snprintf(which, sizeof which,
                      "%s round %.0f m at %.0f kt %s %s, from a waypoint %.1f radii out arriving "
                      "at %03.0f: %.2f turns, %.0f to %.0f m from joining (%.0f off), %.0f to "
                      "%.0f m from a quarter-turn (%.0f off), within %.0f ft",
                      id.c_str(), radius_m, kts, c.right ? "right" : "left",
                      c.windy ? "in a 20 kt wind" : "in calm air", c.from_centre,
                      trial.arriving_deg, f.turns, f.join_nearest_m, f.join_farthest_m,
                      join_off_m, f.nearest_m, f.farthest_m, off_m, f.worst_height_ft);
        std::fprintf(stderr, "%s\n", which);
        if (f.left_tables || !f.joined || f.turns < 0.99 || off_m > entered_within_m ||
            join_off_m > joined_within_m || f.worst_height_ft > 50.0) {
            failures += std::string("\n  ") + which;
        }
    }
    // In calm air each quarter is three places from two ways, both ways round
    // (12), and the north's the centre's too (14); in wind, the centre and on
    // the circle from four (10).
    const std::size_t expected = part == EntryPart::calm_north ? 14
                                 : part == EntryPart::windy    ? 10
                                                               : 12;
    std::fprintf(stderr,
                 "%s: %zu of %zu entries flown; at worst %.0f m off its circle from joining it "
                 "and %.0f m from a quarter-turn\n",
                 id.c_str(), flown, expected, worst_join_m, worst_m);
    check(cases.size() == expected && flown == expected,
          id + ": every entry of this part flown, " + std::to_string(flown) + " of " +
              std::to_string(expected));
    check(failures.empty(), id + " joins its circle going its way and holds it within 100 m "
                                 "from there, within 30 m after the first quarter-turn, and its "
                                 "height within 50 ft, once round:" +
                                failures);
}

} // namespace

// The jets entering are the catalogue's airliners, business jets, fighters
// and bombers, each once.
GLIDESLOPE_TEST(every_jet_has_its_own_test_of_entering_an_orbit_from_a_waypoint) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    std::vector<std::string> jets;
    for (const auto& e : glideslope::sim::read_catalogue(data)) {
        if (e.aircraft_class == AircraftClass::airliner ||
            e.aircraft_class == AircraftClass::business_jet ||
            e.aircraft_class == AircraftClass::fighter || e.aircraft_class == AircraftClass::bomber) {
            jets.push_back(e.id);
        }
    }
    std::vector<std::string> tested = entering_jets;
    std::sort(jets.begin(), jets.end());
    std::sort(tested.begin(), tested.end());
    check(tested == jets && std::adjacent_find(tested.begin(), tested.end()) == tested.end(),
          "each of the " + std::to_string(jets.size()) + " jets has its own test, once: " +
              std::to_string(tested.size()));
}

// **Each jet's five tests fly the whole space between them, each entry
// once**: every entry is in exactly one part, and the parts together are all
// 60 - which each jet's five tests, flying their part each, then cover.
GLIDESLOPE_TEST(every_jet_enters_every_orbit_entry_once_across_its_five_tests) {
    const std::vector<EntryCase> space = every_entry();
    std::map<EntryCase, int> seen;
    std::size_t in_parts = 0;
    for (const EntryPart part : entry_parts) {
        for (const EntryCase& c : entries_in(part)) {
            ++seen[c];
            ++in_parts;
        }
    }
    std::size_t once = 0;
    for (const EntryCase& c : space) {
        const auto found = seen.find(c);
        once += found != seen.end() && found->second == 1 ? std::size_t{1} : std::size_t{0};
    }
    std::fprintf(stderr, "%zu entries in the space, %zu in the five parts, %zu once\n",
                 space.size(), in_parts, once);
    check(space.size() == every_entry_count && in_parts == every_entry_count &&
              once == every_entry_count && seen.size() == every_entry_count,
          "the five parts take all " + std::to_string(every_entry_count) +
              " entries, each once: " + std::to_string(space.size()) + " in the space, " +
              std::to_string(in_parts) + " in the parts, " + std::to_string(once) + " once");
}

GLIDESLOPE_TEST(the_boeing_737_300_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("737-300", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_boeing_737_300_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("737-300", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_boeing_737_300_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("737-300", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_boeing_737_300_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("737-300", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_boeing_737_300_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("737-300", EntryPart::windy);
}
GLIDESLOPE_TEST(the_boeing_747_400_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("747-400", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_boeing_747_400_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("747-400", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_boeing_747_400_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("747-400", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_boeing_747_400_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("747-400", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_boeing_747_400_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("747-400", EntryPart::windy);
}
GLIDESLOPE_TEST(the_boeing_787_8_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("787-8", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_boeing_787_8_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("787-8", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_boeing_787_8_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("787-8", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_boeing_787_8_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("787-8", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_boeing_787_8_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("787-8", EntryPart::windy);
}
GLIDESLOPE_TEST(the_airbus_a320_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("a320", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_airbus_a320_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("a320", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_airbus_a320_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("a320", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_airbus_a320_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("a320", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_airbus_a320_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("a320", EntryPart::windy);
}
GLIDESLOPE_TEST(the_airbus_a380_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("a380", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_airbus_a380_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("a380", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_airbus_a380_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("a380", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_airbus_a380_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("a380", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_airbus_a380_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("a380", EntryPart::windy);
}
GLIDESLOPE_TEST(the_learjet_35a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("learjet35a", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_learjet_35a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("learjet35a", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_learjet_35a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("learjet35a", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_learjet_35a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("learjet35a", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_learjet_35a_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("learjet35a", EntryPart::windy);
}
GLIDESLOPE_TEST(the_b2a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("b2", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_b2a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("b2", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_b2a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("b2", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_b2a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("b2", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_b2a_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("b2", EntryPart::windy);
}
GLIDESLOPE_TEST(the_f15c_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("f15c", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_f15c_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("f15c", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_f15c_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("f15c", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_f15c_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("f15c", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_f15c_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("f15c", EntryPart::windy);
}
GLIDESLOPE_TEST(the_f22a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("f22", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_f22a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("f22", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_f22a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("f22", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_f22a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("f22", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_f22a_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("f22", EntryPart::windy);
}
GLIDESLOPE_TEST(the_f35b_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle) {
    enter_its_orbits("f35b", EntryPart::calm_north);
}
GLIDESLOPE_TEST(the_f35b_enters_an_orbit_from_a_waypoint_in_calm_air_heading_east_or_southeast_and_holds_its_circle) {
    enter_its_orbits("f35b", EntryPart::calm_east);
}
GLIDESLOPE_TEST(the_f35b_enters_an_orbit_from_a_waypoint_in_calm_air_heading_south_or_southwest_and_holds_its_circle) {
    enter_its_orbits("f35b", EntryPart::calm_south);
}
GLIDESLOPE_TEST(the_f35b_enters_an_orbit_from_a_waypoint_in_calm_air_heading_west_or_northwest_and_holds_its_circle) {
    enter_its_orbits("f35b", EntryPart::calm_west);
}
GLIDESLOPE_TEST(the_f35b_enters_an_orbit_from_a_waypoint_in_a_20_kt_wind_and_holds_its_circle) {
    enter_its_orbits("f35b", EntryPart::windy);
}

namespace {

// **A band of the glide tests**: the speeds a glide may be asked of an
// aircraft from `from_kts` to `to_kts`, one way round or both, flown in a
// test of its own so that none flies long on CI. First by class, then by
// aircraft, the airliners' and business jets' took 1,661 s in CI's
// linux-debug, and the A380 alone 238 s here, about 1,000 on CI; each band
// is sized to under 45 s in this machine's linux-debug, about 190 on CI.
enum class Ways { both, left, right };
struct GlideBand {
    const char* id;
    Ways ways;
    double from_kts;
    double to_kts;
};

const std::vector<GlideBand> glide_bands = {
    {"c172p", Ways::both, 60, 75},
    {"c182", Ways::both, 64, 82},
    {"pa28", Ways::both, 64, 74},
    {"j3cub", Ways::both, 46, 48},
    {"short_s23", Ways::left, 86, 100},
    {"short_s23", Ways::right, 86, 100},
    {"mosquito-fb6", Ways::both, 129, 148},
    {"737-300", Ways::left, 172, 192},
    {"737-300", Ways::left, 197, 217},
    {"737-300", Ways::left, 222, 242},
    {"737-300", Ways::left, 247, 260},
    {"737-300", Ways::right, 172, 192},
    {"737-300", Ways::right, 197, 217},
    {"737-300", Ways::right, 222, 242},
    {"737-300", Ways::right, 247, 260},
    {"747-400", Ways::both, 235, 235},
    {"787-8", Ways::left, 203, 218},
    {"787-8", Ways::left, 223, 238},
    {"787-8", Ways::left, 243, 258},
    {"787-8", Ways::left, 263, 278},
    {"787-8", Ways::left, 283, 298},
    {"787-8", Ways::left, 300, 300},
    {"787-8", Ways::right, 203, 218},
    {"787-8", Ways::right, 223, 238},
    {"787-8", Ways::right, 243, 258},
    {"787-8", Ways::right, 263, 278},
    {"787-8", Ways::right, 283, 298},
    {"787-8", Ways::right, 300, 300},
    {"a320", Ways::left, 167, 182},
    {"a320", Ways::left, 187, 202},
    {"a320", Ways::left, 207, 222},
    {"a320", Ways::left, 227, 242},
    {"a320", Ways::left, 247, 262},
    {"a320", Ways::left, 267, 282},
    {"a320", Ways::left, 287, 300},
    {"a320", Ways::right, 167, 182},
    {"a320", Ways::right, 187, 202},
    {"a320", Ways::right, 207, 222},
    {"a320", Ways::right, 227, 242},
    {"a320", Ways::right, 247, 262},
    {"a320", Ways::right, 267, 282},
    {"a320", Ways::right, 287, 300},
    {"a380", Ways::left, 201, 216},
    {"a380", Ways::left, 221, 236},
    {"a380", Ways::left, 241, 256},
    {"a380", Ways::left, 261, 270},
    {"a380", Ways::right, 201, 216},
    {"a380", Ways::right, 221, 236},
    {"a380", Ways::right, 241, 256},
    {"a380", Ways::right, 261, 270},
    {"learjet35a", Ways::left, 142, 167},
    {"learjet35a", Ways::left, 172, 197},
    {"learjet35a", Ways::left, 202, 227},
    {"learjet35a", Ways::left, 232, 240},
    {"learjet35a", Ways::right, 142, 167},
    {"learjet35a", Ways::right, 172, 197},
    {"learjet35a", Ways::right, 202, 227},
    {"learjet35a", Ways::right, 232, 240},
    {"b2", Ways::left, 169, 179},
    {"b2", Ways::left, 184, 204},
    {"b2", Ways::left, 209, 230},
    {"b2", Ways::right, 169, 179},
    {"b2", Ways::right, 184, 204},
    {"b2", Ways::right, 209, 230},
    {"f15c", Ways::left, 170, 185},
    {"f15c", Ways::left, 190, 200},
    {"f15c", Ways::right, 170, 185},
    {"f15c", Ways::right, 190, 200},
    {"f22", Ways::both, 225, 225},
    {"f35b", Ways::both, 188, 204},
};

std::vector<bool> ways_of(Ways w) {
    return w == Ways::both ? std::vector<bool>{false, true}
                           : std::vector<bool>{w == Ways::right};
}

// The speeds a copilot's glide may ask of `brief`'s aircraft: every 5 kt from
// its slowest, and its fastest.
std::vector<double> glide_speeds_asked(const glideslope::copilot::Brief& brief) {
    const auto glide = glideslope::copilot::glide_speeds(brief);
    std::vector<double> asked;
    for (double kts = std::round(glide.slowest_kts); kts < glide.fastest_kts - 0.5; kts += 5.0) {
        asked.push_back(kts);
    }
    asked.push_back(std::round(glide.fastest_kts));
    return asked;
}

// **The glides of one band, round the tightest orbit, unstalled** (from
// 30,000 ft on the circle, its engines stopped: sim::glide_tightest_orbit):
// it goes round (GlideOrbitFlown::round - once, or half way where 1,000 ft
// comes first, as on the fastest jets' circles of 8 to 13 km) and does not
// stall. **The band that begins at its slowest glide also pins its figures
// file's slowest**: never below the slowest a route may fly it, and where
// above it, 5 kt slower does not glide round unstalled the band's ways.
// `unexplained`, where given, says why none of the band is expected to
// glide; one of it must still fail, or the name is stale.
void glides_in_band(const char* id, Ways ways, double from_kts,
                    const char* unexplained = nullptr) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const auto entry = glideslope::sim::find_aircraft(data, id);
    std::string failures;
    const auto fly = [&](double kts, bool right, const char* note) {
        const auto g = glideslope::sim::glide_tightest_orbit(data, entry, kts, right);
        std::printf("%-13s glide %3.0f kt %-5s%s: %5.0f m, round %.2f, alpha %5.1f (lift peaked "
                    "at %5.1f), %5.1f to %5.1f kt, down to %5.0f ft%s\n",
                    entry.id.c_str(), kts, right ? "right" : "left", note, g.radius_m, g.turns,
                    g.most_alpha_deg, g.alpha_at_most_lift_deg, g.slowest_kts, g.fastest_kts,
                    g.lowest_ft, g.stalled() ? ", STALLED" : g.round() ? "" : ", NOT ROUND");
        return !g.stalled() && g.round();
    };
    const GlideBand* band = nullptr;
    for (const GlideBand& b : glide_bands) {
        if (std::string(b.id) == id && b.ways == ways && b.from_kts == from_kts) {
            band = &b;
        }
    }
    check(band != nullptr, std::string(id) + "'s band from " + std::to_string(from_kts) +
                               " kt is in the list");
    glideslope::copilot::Brief brief = glideslope::frontend::brief_for(data, entry.id);
    const double floor_kts = brief.glide_slowest_kts;
    const std::vector<double> asked = glide_speeds_asked(brief);
    if (from_kts == asked.front()) {
        brief.glide_slowest_kts = 0.0;
        const double routed_kts = std::round(glideslope::copilot::slowest_routed_kts(brief));
        if (floor_kts < routed_kts - 0.5) {
            failures += "\n  " + entry.id + "'s slowest glide, " + std::to_string(floor_kts) +
                        " kt, is below the slowest a route may fly it";
        } else if (floor_kts > routed_kts + 0.5) {
            bool below = true;
            for (const bool right : ways_of(ways)) {
                below = below && fly(floor_kts - 5.0, right, " (5 below)");
            }
            if (below) {
                failures += "\n  " + entry.id + " glides at " +
                            std::to_string(floor_kts - 5.0) + " kt, 5 below its file's slowest";
            }
        }
    }
    std::size_t flown = 0;
    bool all_went = true;
    for (const double kts : asked) {
        if (kts < band->from_kts || kts > band->to_kts) {
            continue;
        }
        for (const bool right : ways_of(ways)) {
            ++flown;
            if (!fly(kts, right, "")) {
                all_went = false;
                if (unexplained == nullptr) {
                    failures += "\n  " + entry.id + " at " +
                                std::to_string(static_cast<int>(kts)) + " kt " +
                                (right ? "right" : "left") + ": stalled or not round";
                }
            }
        }
    }
    if (unexplained != nullptr) {
        std::printf("%s named, not yet: %s\n", id, unexplained);
        if (all_went) {
            failures += "\n  " + entry.id + " is named as gliding nowhere and now glides: take " +
                        "its name off";
        }
    }
    std::printf("%s from %.0f to %.0f kt: %zu glides\n", id, band->from_kts, band->to_kts,
                flown);
    check(flown > 0, "the band holds a speed a glide may be asked at");
    check(failures.empty(), "each glides round unstalled, or is named:" + failures);
}

} // namespace

// **Every speed a glide may be asked of every aircraft, each way round, is in
// exactly one band**, and every band holds one: so the band tests between
// them fly every glide. Before a glide's slowest was the slowest a route may
// fly it, a jet could be asked to glide clean at its approach speed, a
// flaps-down figure: the 737-300 at 137 to 162 kt stalled round this orbit,
// past 50 degrees of alpha. Before each file gave its own slowest glide, the
// 747-400, 787-8, A320, F-15C and Mosquito stalled at the slowest a route may
// fly them (PROJECT_STATUS.md).
GLIDESLOPE_TEST(every_glide_of_every_aircraft_each_way_round_is_flown_by_exactly_one_glide_test) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    std::size_t space = 0;
    std::size_t covered = 0;
    std::string failures;
    for (const auto& e : glideslope::sim::read_catalogue(data)) {
        const std::vector<double> asked =
            glide_speeds_asked(glideslope::frontend::brief_for(data, e.id));
        for (const double kts : asked) {
            for (const bool right : {false, true}) {
                ++space;
                std::size_t in = 0;
                for (const GlideBand& b : glide_bands) {
                    const std::vector<bool> w = ways_of(b.ways);
                    in += e.id == b.id && kts >= b.from_kts && kts <= b.to_kts &&
                                  std::find(w.begin(), w.end(), right) != w.end()
                              ? 1U
                              : 0U;
                }
                covered += in == 1 ? 1U : 0U;
                if (in != 1) {
                    failures += "\n  " + e.id + " at " + std::to_string(kts) + " kt " +
                                (right ? "right" : "left") + " in " + std::to_string(in) +
                                " bands";
                }
            }
        }
    }
    for (const GlideBand& b : glide_bands) {
        const std::vector<double> asked =
            glide_speeds_asked(glideslope::frontend::brief_for(data, b.id));
        if (std::none_of(asked.begin(), asked.end(),
                         [&](double kts) { return kts >= b.from_kts && kts <= b.to_kts; })) {
            failures += "\n  " + std::string(b.id) + "'s band from " +
                        std::to_string(b.from_kts) + " kt holds no speed asked";
        }
    }
    std::printf("%zu glides, each way, of every aircraft: %zu in exactly one of %zu bands\n",
                space, covered, glide_bands.size());
    check(failures.empty() && covered == space && space > 0,
          "every glide in exactly one band:" + failures);
}

// **Every aircraft's slowest glide, each way round, begins a glide test**, and
// is the slowest a glide may be asked of it: so the band that begins there
// pins its figures file's `<glide_speeds>` to what `glideslope_cli
// glide-speeds` measures (glides_in_band), for every aircraft and not only
// where the figure happens to be the first speed asked. A figure under the
// slowest a route may fly it is never asked - the brief takes the higher -
// and so goes unchecked: the B-2A's 159 kt and the Mosquito's 128 were
// measured from floors since raised to 169 and 129, and stood until
// 2026-10-13 (PROJECT_STATUS.md).
GLIDESLOPE_TEST(every_aircrafts_slowest_glide_each_way_round_is_the_first_asked_and_begins_the_glide_test_that_pins_it) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    std::size_t space = 0;
    std::size_t covered = 0;
    std::string failures;
    for (const auto& e : glideslope::sim::read_catalogue(data)) {
        glideslope::copilot::Brief brief = glideslope::frontend::brief_for(data, e.id);
        const double figure_kts = brief.glide_slowest_kts;
        const double first_kts = glide_speeds_asked(brief).front();
        brief.glide_slowest_kts = 0.0;
        const double routed_kts = std::round(glideslope::copilot::slowest_routed_kts(brief));
        for (const bool right : {false, true}) {
            ++space;
            bool begins = false;
            for (const GlideBand& b : glide_bands) {
                const std::vector<bool> w = ways_of(b.ways);
                begins = begins || (e.id == b.id && b.from_kts == figure_kts &&
                                    std::find(w.begin(), w.end(), right) != w.end());
            }
            const bool asked = std::abs(figure_kts - first_kts) < 0.5 &&
                               figure_kts > routed_kts - 0.5;
            covered += begins && asked ? 1U : 0U;
            if (!asked) {
                failures += "\n  " + e.id + "'s slowest glide, " + std::to_string(figure_kts) +
                            " kt, is not the first asked, " + std::to_string(first_kts) +
                            " (a route's slowest " + std::to_string(routed_kts) + ")";
            } else if (!begins) {
                failures += "\n  " + e.id + "'s slowest glide, " + std::to_string(figure_kts) +
                            " kt, begins no band turning " + (right ? "right" : "left");
            }
        }
    }
    std::printf("%zu aircraft-ways: %zu with a slowest glide that is asked and begins a band\n",
                space, covered);
    check(failures.empty() && covered == space && space > 0,
          "every slowest glide asked, and pinned by its band:" + failures);
}

GLIDESLOPE_TEST(the_cessna_172p_glides_round_its_tightest_orbit_from_60_to_75_kt_without_stalling) {
    glides_in_band("c172p", Ways::both, 60);
}

GLIDESLOPE_TEST(the_cessna_182s_glides_round_its_tightest_orbit_from_64_to_82_kt_without_stalling) {
    glides_in_band("c182", Ways::both, 64);
}

GLIDESLOPE_TEST(the_piper_pa28_glides_round_its_tightest_orbit_from_64_to_74_kt_without_stalling) {
    glides_in_band("pa28", Ways::both, 64);
}

GLIDESLOPE_TEST(the_piper_j3_cub_glides_round_its_tightest_orbit_from_46_to_48_kt_without_stalling) {
    glides_in_band("j3cub", Ways::both, 46);
}

GLIDESLOPE_TEST(the_short_s23_glides_round_its_tightest_orbit_turning_left_from_86_to_100_kt_without_stalling) {
    glides_in_band("short_s23", Ways::left, 86);
}

GLIDESLOPE_TEST(the_short_s23_glides_round_its_tightest_orbit_turning_right_from_86_to_100_kt_without_stalling) {
    glides_in_band("short_s23", Ways::right, 86);
}

GLIDESLOPE_TEST(the_mosquito_fb6_glides_round_its_tightest_orbit_from_129_to_148_kt_without_stalling) {
    glides_in_band("mosquito-fb6", Ways::both, 129);
}

GLIDESLOPE_TEST(the_boeing_737_300_glides_round_its_tightest_orbit_turning_left_from_172_to_192_kt_without_stalling) {
    glides_in_band("737-300", Ways::left, 172);
}

GLIDESLOPE_TEST(the_boeing_737_300_glides_round_its_tightest_orbit_turning_left_from_197_to_217_kt_without_stalling) {
    glides_in_band("737-300", Ways::left, 197);
}

GLIDESLOPE_TEST(the_boeing_737_300_glides_round_its_tightest_orbit_turning_left_from_222_to_242_kt_without_stalling) {
    glides_in_band("737-300", Ways::left, 222);
}

GLIDESLOPE_TEST(the_boeing_737_300_glides_round_its_tightest_orbit_turning_left_from_247_to_260_kt_without_stalling) {
    glides_in_band("737-300", Ways::left, 247);
}

GLIDESLOPE_TEST(the_boeing_737_300_glides_round_its_tightest_orbit_turning_right_from_172_to_192_kt_without_stalling) {
    glides_in_band("737-300", Ways::right, 172);
}

GLIDESLOPE_TEST(the_boeing_737_300_glides_round_its_tightest_orbit_turning_right_from_197_to_217_kt_without_stalling) {
    glides_in_band("737-300", Ways::right, 197);
}

GLIDESLOPE_TEST(the_boeing_737_300_glides_round_its_tightest_orbit_turning_right_from_222_to_242_kt_without_stalling) {
    glides_in_band("737-300", Ways::right, 222);
}

GLIDESLOPE_TEST(the_boeing_737_300_glides_round_its_tightest_orbit_turning_right_from_247_to_260_kt_without_stalling) {
    glides_in_band("737-300", Ways::right, 247);
}

GLIDESLOPE_TEST(the_boeing_747_400_glides_round_its_tightest_orbit_at_235_kt_without_stalling) {
    glides_in_band("747-400", Ways::both, 235);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_left_from_203_to_218_kt_without_stalling) {
    glides_in_band("787-8", Ways::left, 203);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_left_from_223_to_238_kt_without_stalling) {
    glides_in_band("787-8", Ways::left, 223);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_left_from_243_to_258_kt_without_stalling) {
    glides_in_band("787-8", Ways::left, 243);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_left_from_263_to_278_kt_without_stalling) {
    glides_in_band("787-8", Ways::left, 263);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_left_from_283_to_298_kt_without_stalling) {
    glides_in_band("787-8", Ways::left, 283);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_left_at_300_kt_without_stalling) {
    glides_in_band("787-8", Ways::left, 300);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_right_from_203_to_218_kt_without_stalling) {
    glides_in_band("787-8", Ways::right, 203);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_right_from_223_to_238_kt_without_stalling) {
    glides_in_band("787-8", Ways::right, 223);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_right_from_243_to_258_kt_without_stalling) {
    glides_in_band("787-8", Ways::right, 243);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_right_from_263_to_278_kt_without_stalling) {
    glides_in_band("787-8", Ways::right, 263);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_right_from_283_to_298_kt_without_stalling) {
    glides_in_band("787-8", Ways::right, 283);
}

GLIDESLOPE_TEST(the_boeing_787_8_glides_round_its_tightest_orbit_turning_right_at_300_kt_without_stalling) {
    glides_in_band("787-8", Ways::right, 300);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_left_from_167_to_182_kt_without_stalling) {
    glides_in_band("a320", Ways::left, 167);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_left_from_187_to_202_kt_without_stalling) {
    glides_in_band("a320", Ways::left, 187);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_left_from_207_to_222_kt_without_stalling) {
    glides_in_band("a320", Ways::left, 207);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_left_from_227_to_242_kt_without_stalling) {
    glides_in_band("a320", Ways::left, 227);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_left_from_247_to_262_kt_without_stalling) {
    glides_in_band("a320", Ways::left, 247);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_left_from_267_to_282_kt_without_stalling) {
    glides_in_band("a320", Ways::left, 267);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_left_from_287_to_300_kt_without_stalling) {
    glides_in_band("a320", Ways::left, 287);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_right_from_167_to_182_kt_without_stalling) {
    glides_in_band("a320", Ways::right, 167);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_right_from_187_to_202_kt_without_stalling) {
    glides_in_band("a320", Ways::right, 187);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_right_from_207_to_222_kt_without_stalling) {
    glides_in_band("a320", Ways::right, 207);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_right_from_227_to_242_kt_without_stalling) {
    glides_in_band("a320", Ways::right, 227);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_right_from_247_to_262_kt_without_stalling) {
    glides_in_band("a320", Ways::right, 247);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_right_from_267_to_282_kt_without_stalling) {
    glides_in_band("a320", Ways::right, 267);
}

GLIDESLOPE_TEST(the_airbus_a320_glides_round_its_tightest_orbit_turning_right_from_287_to_300_kt_without_stalling) {
    glides_in_band("a320", Ways::right, 287);
}

GLIDESLOPE_TEST(the_airbus_a380_glides_round_its_tightest_orbit_turning_left_from_201_to_216_kt_without_stalling) {
    glides_in_band("a380", Ways::left, 201);
}

GLIDESLOPE_TEST(the_airbus_a380_glides_round_its_tightest_orbit_turning_left_from_221_to_236_kt_without_stalling) {
    glides_in_band("a380", Ways::left, 221);
}

GLIDESLOPE_TEST(the_airbus_a380_glides_round_its_tightest_orbit_turning_left_from_241_to_256_kt_without_stalling) {
    glides_in_band("a380", Ways::left, 241);
}

GLIDESLOPE_TEST(the_airbus_a380_glides_round_its_tightest_orbit_turning_left_from_261_to_270_kt_without_stalling) {
    glides_in_band("a380", Ways::left, 261);
}

GLIDESLOPE_TEST(the_airbus_a380_glides_round_its_tightest_orbit_turning_right_from_201_to_216_kt_without_stalling) {
    glides_in_band("a380", Ways::right, 201);
}

GLIDESLOPE_TEST(the_airbus_a380_glides_round_its_tightest_orbit_turning_right_from_221_to_236_kt_without_stalling) {
    glides_in_band("a380", Ways::right, 221);
}

GLIDESLOPE_TEST(the_airbus_a380_glides_round_its_tightest_orbit_turning_right_from_241_to_256_kt_without_stalling) {
    glides_in_band("a380", Ways::right, 241);
}

GLIDESLOPE_TEST(the_airbus_a380_glides_round_its_tightest_orbit_turning_right_from_261_to_270_kt_without_stalling) {
    glides_in_band("a380", Ways::right, 261);
}

GLIDESLOPE_TEST(the_learjet_35a_glides_round_its_tightest_orbit_turning_left_from_142_to_167_kt_without_stalling) {
    glides_in_band("learjet35a", Ways::left, 142);
}

GLIDESLOPE_TEST(the_learjet_35a_glides_round_its_tightest_orbit_turning_left_from_172_to_197_kt_without_stalling) {
    glides_in_band("learjet35a", Ways::left, 172);
}

GLIDESLOPE_TEST(the_learjet_35a_glides_round_its_tightest_orbit_turning_left_from_202_to_227_kt_without_stalling) {
    glides_in_band("learjet35a", Ways::left, 202);
}

GLIDESLOPE_TEST(the_learjet_35a_glides_round_its_tightest_orbit_turning_left_from_232_to_240_kt_without_stalling) {
    glides_in_band("learjet35a", Ways::left, 232);
}

GLIDESLOPE_TEST(the_learjet_35a_glides_round_its_tightest_orbit_turning_right_from_142_to_167_kt_without_stalling) {
    glides_in_band("learjet35a", Ways::right, 142);
}

GLIDESLOPE_TEST(the_learjet_35a_glides_round_its_tightest_orbit_turning_right_from_172_to_197_kt_without_stalling) {
    glides_in_band("learjet35a", Ways::right, 172);
}

GLIDESLOPE_TEST(the_learjet_35a_glides_round_its_tightest_orbit_turning_right_from_202_to_227_kt_without_stalling) {
    glides_in_band("learjet35a", Ways::right, 202);
}

GLIDESLOPE_TEST(the_learjet_35a_glides_round_its_tightest_orbit_turning_right_from_232_to_240_kt_without_stalling) {
    glides_in_band("learjet35a", Ways::right, 232);
}

GLIDESLOPE_TEST(the_b2a_glides_round_its_tightest_orbit_turning_left_from_169_to_179_kt_without_stalling) {
    glides_in_band("b2", Ways::left, 169);
}

GLIDESLOPE_TEST(the_b2a_glides_round_its_tightest_orbit_turning_left_from_184_to_204_kt_without_stalling) {
    glides_in_band("b2", Ways::left, 184);
}

GLIDESLOPE_TEST(the_b2a_glides_round_its_tightest_orbit_turning_left_from_209_to_230_kt_without_stalling) {
    glides_in_band("b2", Ways::left, 209);
}

GLIDESLOPE_TEST(the_b2a_glides_round_its_tightest_orbit_turning_right_from_169_to_179_kt_without_stalling) {
    glides_in_band("b2", Ways::right, 169);
}

GLIDESLOPE_TEST(the_b2a_glides_round_its_tightest_orbit_turning_right_from_184_to_204_kt_without_stalling) {
    glides_in_band("b2", Ways::right, 184);
}

GLIDESLOPE_TEST(the_b2a_glides_round_its_tightest_orbit_turning_right_from_209_to_230_kt_without_stalling) {
    glides_in_band("b2", Ways::right, 209);
}

GLIDESLOPE_TEST(the_f15c_glides_round_its_tightest_orbit_turning_left_from_170_to_185_kt_without_stalling) {
    glides_in_band("f15c", Ways::left, 170);
}

GLIDESLOPE_TEST(the_f15c_glides_round_its_tightest_orbit_turning_left_from_190_to_200_kt_without_stalling) {
    glides_in_band("f15c", Ways::left, 190);
}

GLIDESLOPE_TEST(the_f15c_glides_round_its_tightest_orbit_turning_right_from_170_to_185_kt_without_stalling) {
    glides_in_band("f15c", Ways::right, 170);
}

GLIDESLOPE_TEST(the_f15c_glides_round_its_tightest_orbit_turning_right_from_190_to_200_kt_without_stalling) {
    glides_in_band("f15c", Ways::right, 190);
}

// **The F-22A glides at 225 kt**, her one glide: 220, 5 below, departs, and
// her climb speed, 220, is under it. She glided nowhere while the trial
// flew her gear down (2026-10-10).
GLIDESLOPE_TEST(the_f22a_glides_round_its_tightest_orbit_at_225_kt_without_stalling) {
    glides_in_band("f22", Ways::both, 225);
}

GLIDESLOPE_TEST(the_f35b_glides_round_its_tightest_orbit_from_188_to_204_kt_without_stalling) {
    glides_in_band("f35b", Ways::both, 188);
}

namespace {

// **How far a turn onto a leg that starts where she is carries her off it**,
// from the autopilot's and navigator's own laws alone, at a constant true
// airspeed: the bank asked is a degree for each degree her heading is off
// the course (25 at most) and is rolled to at 5 degrees a second; the
// course is the leg's turned back 30 degrees for each kilometre off it; she
// turns at g tan(bank) / V. The figures are the ones the autopilot and the
// navigator fly by (sim/turn_laws.hpp, and most_bank_deg), so retuning them
// moves this with them. The bank limit is the autopilot's full one: a turn
// that has spent its 3 kt may bank less (sim/autopilot.cpp), but these
// never bank past 18.6 degrees.
// Her bank is taken to be the bank asked, at once. A leg starting where she
// is cannot be anticipated, so this is the least the turn onto it strays
// under those laws - and since at a given bank she turns at a rate going as
// 1/V while covering V a second, it grows with the square of her speed.
double turn_onto_leg_strays_m(double true_mps, double turn_deg) {
    constexpr double step_s = 1.0 / 120.0;
    constexpr double radians = std::numbers::pi / 180.0;
    double off_course_deg = turn_deg; // her track off the leg's
    double bank_deg = 0.0;
    double off_m = 0.0;
    double most_m = 0.0;
    for (int step = 0; step < 60 * 120; ++step) {
        using namespace glideslope::sim;
        const double wanted =
            std::clamp(bank_per_degree_off * (off_course_deg + intercept_deg_per_metre * off_m),
                       -most_bank_deg, most_bank_deg);
        bank_deg += std::clamp(wanted - bank_deg, -bank_rate_degps * step_s,
                               bank_rate_degps * step_s);
        off_course_deg -=
            9.80665 * std::tan(bank_deg * radians) / true_mps / radians * step_s;
        off_m += true_mps * std::sin(off_course_deg * radians) * step_s;
        most_m = std::max(most_m, std::abs(off_m));
    }
    return most_m;
}

} // namespace

namespace {

// **A turn at a waypoint is flown by, onto the next leg**: each aircraft in
// `group` - a tightest-orbit group of classes, so that the four tests
// together fly the catalogue - flown level at 5,000 ft in calm air at her
// climb speed and at her cruise speed, north along a leg and then turned 90
// degrees right at its waypoint onto a leg east. Her climb speed is the best
// climb her take-off speeds give (sim::departure_speeds) and her cruise her
// catalogue start speed, each held within the speeds a plan may ask of her
// (her figures file's <plan_speeds>), which is what a plan may fly her at.
// **She strays off the next leg less than a tenth of her turn's radius** at
// that speed and the autopilot's most bank, 25 degrees: the farthest she goes
// past it, anywhere, and the farthest she is off it either side from where
// the fly-by's arc meets it - its radius past the waypoint, at the bank the
// navigator flies it at (fly_by_bank_deg) - on.
void turns_onto_the_next_leg(const std::vector<AircraftClass>& group) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    constexpr double radians = std::numbers::pi / 180.0;
    constexpr double earth_m = 6371000.0;
    constexpr double metres_per_degree = earth_m * radians;
    const auto turn_radius_m = [&](double true_mps) {
        return true_mps * true_mps / (9.80665 * std::tan(glideslope::sim::most_bank_deg * radians));
    };
    std::vector<glideslope::sim::CatalogueEntry> space;
    for (const auto& e : glideslope::sim::read_catalogue(data)) {
        if (std::find(group.begin(), group.end(), e.aircraft_class) != group.end()) {
            space.push_back(e);
        }
    }
    std::size_t flown = 0;
    std::string failures;
    for (const auto& entry : space) {
        const glideslope::sim::PlanSpeeds speeds = glideslope::sim::plan_speeds(data, entry.model);
        const double climb_kts = glideslope::sim::departure_speeds(data, entry.model).climb_kts;
        const std::vector<std::pair<std::string, double>> asked{
            {"climb", climb_kts}, {"cruise", entry.start_airspeed_kts}};
        for (const auto& [which, asked_kts] : asked) {
            const double kts =
                std::round(std::clamp(asked_kts, speeds.slowest_kts, speeds.fastest_kts));
            // Her turn's radius at about her true airspeed there, for the
            // legs' lengths: long enough to settle on the first and to fly
            // the second well past her turn.
            const double r_guess = turn_radius_m(kts * 1852.0 / 3600.0 * 1.08);
            const double lat0 = -34.0;
            const double lon0 = 151.0;
            const double turn_lat = lat0 + (3.0 * r_guess + 4000.0) / metres_per_degree;
            const double end_lon =
                lon0 + (5.0 * r_guess + 4000.0) / (metres_per_degree * std::cos(turn_lat * radians));
            char text[400];
            std::snprintf(text, sizeof text,
                          "aircraft %s\nstart %.6f %.6f 5000 0 %.0f\n"
                          "waypoint TURN %.6f %.6f 5000 %.0f\nwaypoint END %.6f %.6f 5000 %.0f\n",
                          entry.id.c_str(), lat0, lon0, kts, turn_lat, lon0, kts, turn_lat,
                          end_lon, kts);
            const FlightPlan plan = parse_flight_plan(text);
            glideslope::sim::Aircraft aircraft(data / "jsbsim", entry.model);
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = lat0;
            ic.longitude_deg = lon0;
            ic.altitude_ft = 5000.0;
            ic.heading_deg = 0.0;
            ic.airspeed_kts = kts;
            ic.engine_running = true;
            ic.gear = 0.0; // clean: up, where it retracts
            aircraft.initialize(ic);
            glideslope::sim::Controls controls;
            controls.throttle = entry.start_throttle;
            glideslope::sim::Autopilot autopilot(aircraft, controls);
            glideslope::sim::Navigator navigator(aircraft, plan);
            double true_mps = 0.0;
            double past_m = 0.0;      // the farthest past the next leg
            double after_m = 0.0;     // the farthest off it from the arc's end on
            double turned_at_m = 0.0; // how far short of the waypoint the turn began
            int steps = 0;
            const int most_steps = 20 * 60 * steps_per_second;
            while (!navigator.finished() && steps < most_steps) {
                const std::size_t was = navigator.next();
                autopilot.set(navigator.steer());
                aircraft.set_controls(autopilot.fly());
                aircraft.step();
                ++steps;
                const double lat = aircraft.property("position/lat-geod-deg");
                const double lon = aircraft.property("position/long-gc-deg");
                if (was == 0 && navigator.next() == 1) {
                    true_mps = aircraft.property("velocities/vt-fps") * 0.3048;
                    turned_at_m = glideslope::sim::distance_m(lat, lon, turn_lat, lon0);
                }
                if (navigator.next() == 0) {
                    continue;
                }
                // Off the next leg, on the sphere: right of it positive, the
                // side she turns onto it from.
                const double from = glideslope::sim::distance_m(turn_lat, lon0, lat, lon) / earth_m;
                const double angle =
                    (glideslope::sim::bearing_deg(turn_lat, lon0, lat, lon) -
                     glideslope::sim::bearing_deg(turn_lat, lon0, turn_lat, end_lon)) *
                    radians;
                const double across = std::asin(std::sin(from) * std::sin(angle));
                const double along = std::copysign(
                    std::acos(std::clamp(std::cos(from) / std::cos(across), -1.0, 1.0)),
                    std::cos(angle));
                const double off_m = across * earth_m;
                past_m = std::max(past_m, -off_m);
                // Where the fly-by's arc, at the bank the navigator turns
                // it at, meets the next leg: its radius past the waypoint.
                const double arc_m = true_mps * true_mps /
                                     (9.80665 * std::tan(glideslope::sim::fly_by_bank_deg * radians));
                if (along * earth_m >= arc_m) {
                    after_m = std::max(after_m, std::abs(off_m));
                }
            }
            const double radius_m = turn_radius_m(true_mps);
            const double stray_m = std::max(past_m, after_m);
            char line[500];
            std::snprintf(line, sizeof line,
                          "%s at her %s speed, %.0f KCAS (%.0f asked), %.1f m/s true: turned "
                          "%.0f m short of the waypoint, strayed %.0f m past the next leg and "
                          "%.0f m off it from the arc's end, %.3f of her %.0f m turn radius",
                          entry.id.c_str(), which.c_str(), kts, asked_kts, true_mps, turned_at_m,
                          past_m, after_m, stray_m / radius_m, radius_m);
            std::fprintf(stderr, "%s\n", line);
            if (!navigator.finished() || true_mps <= 0.0 || stray_m >= 0.1 * radius_m) {
                failures += std::string("\n  ") + line;
            }
            ++flown;
        }
    }
    const std::size_t cases = space.size() * 2;
    std::fprintf(stderr, "%zu aircraft x 2 speeds = %zu turns: %zu flown\n", space.size(), cases,
                 flown);
    check(!space.empty() && flown == cases,
          "every aircraft of the group at both speeds: " + std::to_string(flown) + " of " +
              std::to_string(cases));
    check(failures.empty(),
          "each flew its plan to the end, straying off the next leg less than a tenth of its "
          "turn radius:" +
              failures);
}

} // namespace

// The four groups take every class once, which
// every_aircraft_has_its_own_tests_of_its_tightest_orbits_and_one_step_past_them
// asserts, so these four turn every aircraft in the catalogue.
GLIDESLOPE_TEST(every_light_aeroplane_turning_90_degrees_at_a_waypoint_at_her_climb_and_cruise_speeds_strays_off_the_next_leg_less_than_a_tenth_of_her_turn_radius) {
    turns_onto_the_next_leg(tightest_orbit_groups[0]);
}

GLIDESLOPE_TEST(every_airliner_and_business_jet_turning_90_degrees_at_a_waypoint_at_her_climb_and_cruise_speeds_strays_off_the_next_leg_less_than_a_tenth_of_her_turn_radius) {
    turns_onto_the_next_leg(tightest_orbit_groups[1]);
}

GLIDESLOPE_TEST(every_fighter_and_bomber_turning_90_degrees_at_a_waypoint_at_her_climb_and_cruise_speeds_strays_off_the_next_leg_less_than_a_tenth_of_her_turn_radius) {
    turns_onto_the_next_leg(tightest_orbit_groups[2]);
}

GLIDESLOPE_TEST(every_warbird_and_seaplane_turning_90_degrees_at_a_waypoint_at_her_climb_and_cruise_speeds_strays_off_the_next_leg_less_than_a_tenth_of_her_turn_radius) {
    turns_onto_the_next_leg(tightest_orbit_groups[3]);
}

namespace {

// **One aircraft of each class**, the first the catalogue holds of it: the
// turns below are flown by these, and every class has one.
std::vector<glideslope::sim::CatalogueEntry> one_of_each_class() {
    std::vector<glideslope::sim::CatalogueEntry> out;
    for (const auto& e : glideslope::sim::read_catalogue(
             std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path())) {
        if (std::none_of(out.begin(), out.end(), [&](const auto& o) {
                return o.aircraft_class == e.aircraft_class;
            })) {
            out.push_back(e);
        }
    }
    return out;
}

// Her cruise speed - her catalogue start speed - held to what a plan may fly
// her at.
double cruise_kts(const glideslope::sim::CatalogueEntry& e) {
    const glideslope::sim::PlanSpeeds speeds = glideslope::sim::plan_speeds(
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path(), e.model);
    return std::round(std::clamp(e.start_airspeed_kts, speeds.slowest_kts, speeds.fastest_kts));
}

struct TurnFlown {
    bool finished = false;
    bool one_waypoint_a_step = true; // never two passed on one step
    // The most any waypoint with a leg after it was passed short of itself,
    // along the leg to it, as a share of half the shorter leg it joins.
    double most_lead_share = 0.0;
    double radius_m = 0.0;           // her turn's at 25 degrees, true airspeed
    double past_m = 0.0;             // the farthest past the turn's next leg
    double after_m = 0.0;            // the farthest off it from where its arc meets it
    std::vector<double> closest_m;   // each waypoint's nearest
    std::vector<double> arc_passes_m; // and where its arc passes it
};

// **A plan of legs flown level at 5,000 ft**, from north-bound at `kts`,
// each leg `legs_m[k]` long on the bearing `bearings_deg[k]`, the waypoints
// at their ends, the first `fly_over` if asked; in a wind from the west of
// `wind_kts`. The stray is measured off the leg after the first waypoint.
TurnFlown fly_legs(const glideslope::sim::CatalogueEntry& e, double kts,
                   const std::vector<double>& bearings_deg, const std::vector<double>& legs_m,
                   double wind_kts = 0.0, bool fly_over = false) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    constexpr double radians = std::numbers::pi / 180.0;
    constexpr double earth_m = 6371000.0;
    std::vector<std::pair<double, double>> at{{-34.0, 151.0}};
    for (std::size_t k = 0; k < legs_m.size(); ++k) {
        const auto [lat, lon] = at.back();
        at.emplace_back(lat + legs_m[k] * std::cos(bearings_deg[k] * radians) / (earth_m * radians),
                        lon + legs_m[k] * std::sin(bearings_deg[k] * radians) /
                                  (earth_m * radians * std::cos(lat * radians)));
    }
    std::string text = "aircraft " + e.id + "\n";
    char line[200];
    std::snprintf(line, sizeof line, "start %.7f %.7f 5000 %.1f %.0f\n", at[0].first,
                  at[0].second, bearings_deg[0], kts);
    text += line;
    for (std::size_t k = 1; k < at.size(); ++k) {
        std::snprintf(line, sizeof line, "waypoint W%zu %.7f %.7f 5000 %.0f%s\n", k, at[k].first,
                      at[k].second, kts, k == 1 && fly_over ? " flyover" : "");
        text += line;
    }
    const FlightPlan plan = parse_flight_plan(text);
    glideslope::sim::Aircraft aircraft(data / "jsbsim", e.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = at[0].first;
    ic.longitude_deg = at[0].second;
    ic.altitude_ft = 5000.0;
    ic.heading_deg = bearings_deg[0];
    ic.airspeed_kts = kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    aircraft.initialize(ic);
    if (wind_kts > 0.0) {
        glideslope::sim::Conditions wind;
        wind.wind_east_mps = wind_kts * 1852.0 / 3600.0;
        aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(wind));
    }
    glideslope::sim::Controls controls;
    controls.throttle = e.start_throttle;
    glideslope::sim::Autopilot autopilot(aircraft, controls);
    glideslope::sim::Navigator navigator(aircraft, plan);
    TurnFlown f;
    f.closest_m.assign(plan.waypoints.size(), std::numeric_limits<double>::infinity());
    f.arc_passes_m.assign(plan.waypoints.size(), 0.0);
    const double turn_deg =
        legs_m.size() > 1 ? std::remainder(bearings_deg[1] - bearings_deg[0], 360.0) : 0.0;
    const double way = turn_deg >= 0.0 ? 1.0 : -1.0;
    double meets_m = 0.0;
    int steps = 0;
    const int most_steps = 30 * 60 * steps_per_second;
    while (!navigator.finished() && steps < most_steps) {
        const std::size_t was = navigator.next();
        autopilot.set(navigator.steer());
        aircraft.set_controls(autopilot.fly());
        aircraft.step();
        ++steps;
        const double lat = aircraft.property("position/lat-geod-deg");
        const double lon = aircraft.property("position/long-gc-deg");
        f.one_waypoint_a_step = f.one_waypoint_a_step && navigator.next() <= was + 1;
        if (navigator.next() != was && was + 1 < plan.waypoints.size()) {
            // Where the arc it turns on passes the waypoint, at her speed
            // over the ground now.
            const double ground_mps = std::hypot(aircraft.property("velocities/v-north-fps"),
                                                 aircraft.property("velocities/v-east-fps")) *
                                      0.3048;
            const glideslope::sim::FlyBy turn = glideslope::sim::fly_by(
                ground_mps, std::remainder(bearings_deg[was + 1] - bearings_deg[was], 360.0),
                legs_m[was], legs_m[was + 1]);
            const double short_m = glideslope::sim::distance_m(
                                       lat, lon, plan.waypoints[was].latitude_deg,
                                       plan.waypoints[was].longitude_deg) *
                                   std::cos((glideslope::sim::bearing_deg(
                                                 lat, lon, plan.waypoints[was].latitude_deg,
                                                 plan.waypoints[was].longitude_deg) -
                                             bearings_deg[was]) *
                                            radians);
            f.most_lead_share = std::max(
                f.most_lead_share, short_m / (0.5 * std::min(legs_m[was], legs_m[was + 1])));
            if (turn.lead_m > 0.0 && !plan.waypoints[was].fly_over) {
                f.arc_passes_m[was] = std::hypot(turn.meets_m, turn.radius_m) - turn.radius_m;
            }
            if (was == 0) {
                const double true_mps = aircraft.property("velocities/vt-fps") * 0.3048;
                f.radius_m = true_mps * true_mps /
                             (9.80665 * std::tan(glideslope::sim::most_bank_deg * radians));
                meets_m = plan.waypoints[0].fly_over || turn.lead_m <= 0.0 ? f.radius_m
                                                                          : turn.meets_m;
            }
        }
        for (std::size_t k = 0; k < plan.waypoints.size(); ++k) {
            f.closest_m[k] = std::min(f.closest_m[k],
                                      glideslope::sim::distance_m(lat, lon,
                                                                  plan.waypoints[k].latitude_deg,
                                                                  plan.waypoints[k].longitude_deg));
        }
        if (navigator.next() != 1 || legs_m.size() < 2) {
            continue;
        }
        // Off the leg after the first waypoint: right of it positive.
        const double from = glideslope::sim::distance_m(at[1].first, at[1].second, lat, lon) / earth_m;
        const double angle = (glideslope::sim::bearing_deg(at[1].first, at[1].second, lat, lon) -
                              glideslope::sim::bearing_deg(at[1].first, at[1].second,
                                                           at[2].first, at[2].second)) *
                             radians;
        const double across = std::asin(std::sin(from) * std::sin(angle));
        const double along = std::copysign(
            std::acos(std::clamp(std::cos(from) / std::cos(across), -1.0, 1.0)), std::cos(angle));
        // Past it is on the side away from the turn's: left of it, turning right.
        f.past_m = std::max(f.past_m, -way * across * earth_m);
        if (along * earth_m >= meets_m) {
            f.after_m = std::max(f.after_m, std::abs(across) * earth_m);
        }
    }
    f.finished = navigator.finished();
    return f;
}

} // namespace

// **A waypoint marked `flyover` is flown over**: one aircraft of each class,
// at her cruise speed, north along 8 km to a waypoint marked flyover and then
// turned 90 degrees right, passes it within 100 m - as she passes an ordinary
// waypoint with no leg after it. Flown by, the same turn passes it 0.4 of her
// turn's radius off (sim::fly_by), hundreds of metres to kilometres.
GLIDESLOPE_TEST(a_waypoint_marked_flyover_is_passed_within_100_m_by_an_aircraft_of_every_class) {
    const auto space = one_of_each_class();
    std::size_t flown = 0;
    std::string failures;
    for (const auto& e : space) {
        const double kts = cruise_kts(e);
        const TurnFlown f = fly_legs(e, kts, {0.0, 90.0}, {8000.0, 8000.0}, 0.0, true);
        char line[200];
        std::snprintf(line, sizeof line, "%s at %.0f kt: passed W1, flown over, %.1f m off", e.id.c_str(),
                      kts, f.closest_m[0]);
        std::fprintf(stderr, "%s\n", line);
        if (!f.finished || f.closest_m[0] > 100.0) {
            failures += std::string("\n  ") + line;
        }
        ++flown;
    }
    check(space.size() == glideslope::sim::aircraft_class_count && flown == space.size(),
          "one aircraft of each of the " + std::to_string(glideslope::sim::aircraft_class_count) +
              " classes flown: " + std::to_string(flown));
    check(failures.empty(), "each flew over its waypoint within 100 m:" + failures);
}

// **No fly-by cuts its waypoint by more than most_fly_by_miss_m**
// (sim/turn_laws.hpp, 278 m): one aircraft of each class, at her climb speed
// and her cruise speed (each held to what a plan may fly her at), turning 45,
// 90, 120 and 150 degrees, right and left, from 4 turn radii of leg onto 6,
// passes the waypoint within it - flown by on an arc tightened to pass that
// near - or, where even 22 degrees of bank cannot (fly_by_cap_bank_deg), as
// near as her arc at 22 passes it, the nearest she can with bank to spare;
// within 25 m of either for her flight off her arc. Each turn's
// stray off the next leg is held to its bound as in the angles' test: a
// tenth of her radius at 25 degrees to 120, 0.15 at 150: past 120 degrees
// the arc tightens to keep the lead the 120-degree turn has, down to the
// radius at 25 degrees (sim::fly_by), so 150 is flown with no bank to spare
// and strays most. These take in, at her cruise speed, every turn of the
// angles test they replaced (45, 120 and 150 degrees, cruise only), which
// would have run past 900 s in linux-debug beside them. **One test for each
// angle and speed**, 14 turns each, so that none runs long on CI: together,
// 112 turns; split by angle alone, 28 each, the slowest was estimated at
// 650 s of linux-debug's 900, too near for a slow runner; and before that,
// in linux-debug the whole ran past 900 s.
namespace {

const std::vector<double> fly_by_cap_angles = {45.0, 90.0, 120.0, 150.0};

enum class Speed { climb, cruise };

void passes_within_the_fly_by_cap(double angle, Speed speed) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const auto space = one_of_each_class();
    constexpr double radians = std::numbers::pi / 180.0;
    const double cap_m = glideslope::sim::most_fly_by_miss_m;
    std::size_t flown = 0;
    std::size_t flown_over = 0;
    double worst_miss_m = 0.0;
    double worst_over_allowed = 0.0;
    double worst_stray = 0.0;
    std::string failures;
    for (const auto& e : space) {
        const glideslope::sim::PlanSpeeds speeds = glideslope::sim::plan_speeds(data, e.model);
        const double climb = std::round(
            std::clamp(glideslope::sim::departure_speeds(data, e.model).climb_kts,
                       speeds.slowest_kts, speeds.fastest_kts));
        for (const double kts : {speed == Speed::climb ? climb : cruise_kts(e)}) {
            const double true_mps = kts * 1852.0 / 3600.0 * 1.08;
            const double r = true_mps * true_mps / (9.80665 * std::tan(15.0 * radians));
            {
                for (const double way : {1.0, -1.0}) {
                    const TurnFlown f = fly_legs(e, kts, {0.0, way * angle},
                                                 {4.0 * r + 3000.0, 6.0 * r + 3000.0});
                    const double stray = std::max(f.past_m, f.after_m) / f.radius_m;
                    const double bound = angle > 140.0 ? 0.15 : 0.1;
                    if (f.arc_passes_m[0] <= 0.0) {
                        ++flown_over;
                    }
                    worst_miss_m = std::max(worst_miss_m, f.closest_m[0]);
                    worst_stray = std::max(worst_stray, stray / bound);
                    char line[220];
                    std::snprintf(line, sizeof line,
                                  "%s at %.0f kt turning %+.0f: passed it %.0f m off (%s), "
                                  "strayed %.3f of her %.0f m radius (at most %.2f)",
                                  e.id.c_str(), kts, way * angle, f.closest_m[0],
                                  f.arc_passes_m[0] > 0.0 ? "flown by" : "flown over", stray,
                                  f.radius_m, bound);
                    std::fprintf(stderr, "%s\n", line);
                    // Where the cap could not bind, the miss of her arc at
                    // fly_by_cap_bank_deg, at her true airspeed as she passed
                    // it; and 25 m for her flight off her arc, which is at
                    // most 4 m past the cap (the 172P at 75 kt), and 1% of
                    // the arc for her speed over the ground against the
                    // true airspeed it is worked from here (the F-15C at
                    // 300 kt, 2 m over 6,934).
                    const double tightest_m =
                        f.radius_m * std::tan(glideslope::sim::most_bank_deg * radians) /
                        std::tan(glideslope::sim::fly_by_cap_bank_deg * radians);
                    const double arc_m =
                        tightest_m * (1.0 / std::cos(0.5 * angle * radians) - 1.0);
                    const double allowed_m = std::max(cap_m, 1.01 * arc_m) + 25.0;
                    worst_over_allowed = std::max(worst_over_allowed, f.closest_m[0] / allowed_m);
                    if (!f.finished || !(f.closest_m[0] <= allowed_m) || !(stray < bound)) {
                        failures += std::string("\n  ") + line;
                    }
                    ++flown;
                }
            }
        }
    }
    const std::size_t cases = space.size() * 2;
    std::fprintf(stderr,
                 "%zu aircraft x 2 ways at %.0f degrees = %zu turns: %zu flown, %zu flown "
                 "over; nearest pass at worst %.0f m, %.2f of what it is allowed; stray at worst "
                 "%.2f of its bound\n",
                 space.size(), angle, cases, flown, flown_over, worst_miss_m,
                 worst_over_allowed, worst_stray);
    check(space.size() == glideslope::sim::aircraft_class_count && flown == cases,
          "one aircraft of each class, both ways: " +
              std::to_string(flown) + " of " + std::to_string(cases));
    check(flown_over == 0, "none flown over: " + std::to_string(flown_over));
    check(failures.empty(), "each passed its waypoint within " + std::to_string(cap_m) +
                                " m, or its 22-degree arc's miss where that is more, and 25 m, "
                                "and strayed within its bound:" +
                                failures);
}

} // namespace

GLIDESLOPE_TEST(a_turn_of_45_degrees_at_a_waypoint_at_her_climb_speed_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class) {
    passes_within_the_fly_by_cap(45.0, Speed::climb);
}

GLIDESLOPE_TEST(a_turn_of_45_degrees_at_a_waypoint_at_her_cruise_speed_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class) {
    passes_within_the_fly_by_cap(45.0, Speed::cruise);
}

GLIDESLOPE_TEST(a_turn_of_90_degrees_at_a_waypoint_at_her_climb_speed_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class) {
    passes_within_the_fly_by_cap(90.0, Speed::climb);
}

GLIDESLOPE_TEST(a_turn_of_90_degrees_at_a_waypoint_at_her_cruise_speed_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class) {
    passes_within_the_fly_by_cap(90.0, Speed::cruise);
}

GLIDESLOPE_TEST(a_turn_of_120_degrees_at_a_waypoint_at_her_climb_speed_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class) {
    passes_within_the_fly_by_cap(120.0, Speed::climb);
}

GLIDESLOPE_TEST(a_turn_of_120_degrees_at_a_waypoint_at_her_cruise_speed_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class) {
    passes_within_the_fly_by_cap(120.0, Speed::cruise);
}

GLIDESLOPE_TEST(a_turn_of_150_degrees_at_a_waypoint_at_her_climb_speed_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class) {
    passes_within_the_fly_by_cap(150.0, Speed::climb);
}

GLIDESLOPE_TEST(a_turn_of_150_degrees_at_a_waypoint_at_her_cruise_speed_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class) {
    passes_within_the_fly_by_cap(150.0, Speed::cruise);
}

// **The eight fly-by cap tests together turn 112 times**, as the one they
// were split from did: four angles at two speeds, each 14 turns - an aircraft
// of each of the seven classes, both ways.
GLIDESLOPE_TEST(the_fly_by_cap_tests_together_turn_an_aircraft_of_every_class_112_times_at_four_angles) {
    check(fly_by_cap_angles == std::vector<double>{45.0, 90.0, 120.0, 150.0},
          "the four angles, 45, 90, 120 and 150 degrees");
    const std::size_t tests = fly_by_cap_angles.size() * 2; // climb and cruise
    const std::size_t turns = tests * one_of_each_class().size() * 2;
    check(tests == 8 && turns == 112,
          "eight tests of four angles x two speeds, x seven classes x two ways: " +
                            std::to_string(turns));
}

// **In a 20 kt wind across the first leg**: from the west, the leg north, and
// turned 90 degrees right - onto a tailwind, her speed over the ground and so
// her arc's radius growing through the turn - and left, onto a headwind. A
// light aeroplane, an airliner and a fighter (the first of each class), at
// her cruise speed: each strays less than a tenth of her radius at 25
// degrees, as in calm air. Measured (2026-10-13): at most the 172P's 0.044,
// turning onto the tailwind.
GLIDESLOPE_TEST(a_turn_at_a_waypoint_in_a_20_kt_crosswind_strays_within_its_bound) {
    const auto all = one_of_each_class();
    std::vector<glideslope::sim::CatalogueEntry> space;
    for (const auto& e : all) {
        if (e.aircraft_class == glideslope::sim::AircraftClass::light_aircraft ||
            e.aircraft_class == glideslope::sim::AircraftClass::airliner ||
            e.aircraft_class == glideslope::sim::AircraftClass::fighter) {
            space.push_back(e);
        }
    }
    constexpr double radians = std::numbers::pi / 180.0;
    std::size_t flown = 0;
    std::string failures;
    for (const auto& e : space) {
        const double kts = cruise_kts(e);
        const double true_mps = kts * 1852.0 / 3600.0 * 1.08;
        const double r = true_mps * true_mps / (9.80665 * std::tan(15.0 * radians));
        for (const double way : {1.0, -1.0}) {
            const TurnFlown f =
                fly_legs(e, kts, {0.0, way * 90.0}, {4.0 * r + 3000.0, 6.0 * r + 3000.0}, 20.0);
            const double stray = std::max(f.past_m, f.after_m) / f.radius_m;
            char line[200];
            std::snprintf(line, sizeof line,
                          "%s at %.0f kt turning %+.0f in a 20 kt wind from the west: %.0f m "
                          "past, %.0f m off after, %.3f of her %.0f m radius",
                          e.id.c_str(), kts, way * 90.0, f.past_m, f.after_m, stray, f.radius_m);
            std::fprintf(stderr, "%s\n", line);
            if (!f.finished || !(stray < 0.1)) {
                failures += std::string("\n  ") + line;
            }
            ++flown;
        }
    }
    check(space.size() == 3 && flown == 6,
          "a light aeroplane, an airliner and a fighter, both ways: " + std::to_string(flown));
    check(failures.empty(), "each within its bound:" + failures);
}

// **Short legs do not run waypoints together**: one aircraft of each class at
// her cruise speed, zigzagging 90 degrees right and left over four legs each
// 1.6 turn radii at 15 degrees and 7 s long - shorter than twice the lead a
// full arc would want - so each lead is held to half a leg and its arc
// tightened to fit (sim::fly_by). Each waypoint is passed on a step of its
// own, in order, and within 100 m of where its tightened arc passes it.
// Measured (2026-10-13): within 15 m, every one.
GLIDESLOPE_TEST(a_plan_of_legs_shorter_than_their_turns_passes_each_waypoint_in_turn_near_its_arc) {
    const auto space = one_of_each_class();
    constexpr double radians = std::numbers::pi / 180.0;
    std::size_t flown = 0;
    std::string failures;
    for (const auto& e : space) {
        const double kts = cruise_kts(e);
        const double true_mps = kts * 1852.0 / 3600.0 * 1.08;
        const double r = true_mps * true_mps / (9.80665 * std::tan(15.0 * radians));
        // Each lead held to half a leg, its arc 0.8 of her radius at 15
        // degrees: under 19 degrees of bank, which she can fly.
        const double short_m = 1.6 * r + 2.0 * true_mps * 3.5;
        const TurnFlown f = fly_legs(e, kts, {0.0, 90.0, 0.0, 90.0, 0.0, 90.0},
                                     {4.0 * r, short_m, short_m, short_m, short_m, 4.0 * r});
        std::string worst;
        bool near = true;
        for (std::size_t k = 0; k + 1 < f.closest_m.size(); ++k) {
            near = near && std::abs(f.closest_m[k] - f.arc_passes_m[k]) <= 100.0;
            char part[80];
            std::snprintf(part, sizeof part, " W%zu %.0f (arc %.0f)", k + 1, f.closest_m[k],
                          f.arc_passes_m[k]);
            worst += part;
        }
        const std::string line =
            e.id + " over legs of " + std::to_string(static_cast<int>(short_m)) + " m: passed" +
            worst + (f.one_waypoint_a_step ? ", each on its own step" : ", two on one step");
        std::fprintf(stderr, "%s\n", line.c_str());
        if (!f.finished || !f.one_waypoint_a_step || !near) {
            failures += "\n  " + line;
        }
        // **And legs far shorter than a turn** - 0.4 of the radius at 15
        // degrees, which no arc she can fly fits: each waypoint is passed no
        // further short of itself than half the shorter leg it joins, so no
        // lead begins before the waypoint before it is reached. Not held to
        // the arcs, which want more than 25 degrees of bank, nor to a step of
        // its own each: overshooting a leg shorter than her tightest turn,
        // she comes abeam of the next waypoint too.
        const double shortest_m = 0.4 * r;
        const TurnFlown g = fly_legs(e, kts, {0.0, 90.0, 0.0, 90.0, 0.0, 90.0},
                                     {4.0 * r, shortest_m, shortest_m, shortest_m, shortest_m,
                                      4.0 * r});
        char shortest[200];
        std::snprintf(shortest, sizeof shortest,
                      "%s over legs of %.0f m: passed at most %.2f of half a leg short",
                      e.id.c_str(), shortest_m, g.most_lead_share);
        std::fprintf(stderr, "%s\n", shortest);
        if (!g.finished || !(g.most_lead_share <= 1.01)) {
            failures += std::string("\n  ") + shortest;
        }
        flown += 2;
    }
    check(space.size() == glideslope::sim::aircraft_class_count && flown == 2 * space.size(),
          "one aircraft of each class, over both lengths of leg: " + std::to_string(flown));
    check(failures.empty(),
          "each passed every waypoint on a step of its own and within 100 m of its arc, and "
          "over the shortest legs no more than half a leg short of it:" +
              failures);
}

GLIDESLOPE_TEST(a_plan_that_takes_off_leaves_its_runway_and_flies_its_waypoints_in_every_light_aeroplane) {
    // From a runway at sea level on flat ground, to 500 ft above it, and then
    // to a waypoint 15 km off to the left of the runway's heading at 2,000 ft:
    // far enough for the slowest of them, the Cub, to have climbed to it.
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    // Every light aeroplane the catalogue holds, so that one added is flown.
    std::vector<glideslope::sim::CatalogueEntry> light;
    for (const auto& e : glideslope::sim::read_catalogue(data)) {
        if (e.aircraft_class == glideslope::sim::AircraftClass::light_aircraft && !e.seaplane) {
            light.push_back(e);
        }
    }
    std::size_t flown = 0;
    for (const auto& entry : light) {
        const std::string& id = entry.id;
        const FlightPlan plan = parse_flight_plan(
            "aircraft " + id +
            "\nrunway 07 -33.9461 151.1772 0 70 3000\ntakeoff 500\n"
            "waypoint OUT -33.85 151.30 2000 80\n");
        glideslope::sim::Aircraft aircraft(data / "jsbsim", entry.model);
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = plan.takeoff->runway.threshold_lat_deg;
        ic.longitude_deg = plan.takeoff->runway.threshold_lon_deg;
        ic.altitude_ft = plan.takeoff->runway.elevation_ft;
        ic.terrain_elevation_ft = plan.takeoff->runway.elevation_ft;
        ic.heading_deg = plan.takeoff->runway.heading_deg;
        ic.airspeed_kts = 0.0;
        ic.engine_running = true;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        glideslope::sim::Controller controller(aircraft, glideslope::sim::Controls{});
        // The speed the take-off climbs her away at (Departure::speeds), read
        // while it flies her: a light aeroplane's published best-climb speed,
        // whatever she weighs.
        double climb_kts = 0.0;
        double slowest_climb_kcas = std::numeric_limits<double>::infinity();
        controller.to_ai_flying(plan, glideslope::sim::departure_speeds(data, entry.model));

        // **The take-off autopilot flies it off**: flying from the first step,
        // and handing over to the plan only once through its height.
        const bool departing = controller.departure() != nullptr;
        bool took_off = false;
        double handed_over_at_ft = 0.0;
        // **The first leg is flown from where the take-off handed over**, not
        // from the threshold: the most the aircraft strays from the great
        // circle between there and the waypoint.
        double handed_over_lat = 0.0;
        double handed_over_lon = 0.0;
        double handed_over_true_mps = 0.0;
        double handed_over_track_deg = 0.0;
        double worst_off_leg_m = 0.0;
        double closest_m = std::numeric_limits<double>::infinity();
        double altitude_there_ft = 0.0;
        int steps = 0;
        const int most_steps = 15 * 60 * steps_per_second;
        while (steps < most_steps) {
            aircraft.set_controls(controller.fly());
            aircraft.step();
            ++steps;
            if (!took_off && controller.departure() == nullptr) {
                took_off = true;
                handed_over_at_ft = aircraft.property("position/h-sl-ft");
                handed_over_lat = aircraft.property("position/lat-geod-deg");
                handed_over_lon = aircraft.property("position/long-gc-deg");
                handed_over_true_mps = aircraft.property("velocities/vt-fps") * 0.3048;
                handed_over_track_deg =
                    std::atan2(aircraft.property("velocities/v-east-fps"),
                               aircraft.property("velocities/v-north-fps")) *
                    180.0 / std::numbers::pi;
            }
            const glideslope::sim::Navigator* navigator = controller.navigator();
            if (navigator == nullptr || navigator->finished()) {
                break;
            }
            if (!took_off) {
                climb_kts = controller.departure()->speeds().initial_climb_kts;
                continue;
            }
            const double d = glideslope::sim::distance_m(
                aircraft.property("position/lat-geod-deg"),
                aircraft.property("position/long-gc-deg"), plan.waypoints[0].latitude_deg,
                plan.waypoints[0].longitude_deg);
            if (d < closest_m) {
                closest_m = d;
                altitude_there_ft = aircraft.property("position/h-sl-ft");
            }
            // Off the leg: the cross-track distance, on the sphere.
            const double r = 6371000.0;
            const double from = glideslope::sim::distance_m(
                                    handed_over_lat, handed_over_lon,
                                    aircraft.property("position/lat-geod-deg"),
                                    aircraft.property("position/long-gc-deg")) /
                                r;
            const double angle =
                (glideslope::sim::bearing_deg(handed_over_lat, handed_over_lon,
                                              aircraft.property("position/lat-geod-deg"),
                                              aircraft.property("position/long-gc-deg")) -
                 glideslope::sim::bearing_deg(handed_over_lat, handed_over_lon,
                                              plan.waypoints[0].latitude_deg,
                                              plan.waypoints[0].longitude_deg)) *
                std::numbers::pi / 180.0;
            worst_off_leg_m =
                std::max(worst_off_leg_m, std::abs(std::asin(std::sin(from) * std::sin(angle))) * r);
            // The first leg's climb: from the hand-over until she is within
            // 50 ft of the waypoint's height.
            if (aircraft.property("position/h-sl-ft") < 1950.0) {
                slowest_climb_kcas = std::min(slowest_climb_kcas, aircraft.state().airspeed_kts);
            }
        }
        std::fprintf(stderr,
                     "%s: taken off to %.0f ft, and passed OUT %.0f m off at %.0f ft, in %d s, "
                     "straying at most %.0f m from the leg from where it was handed over; "
                     "climbing it at no less than %.1f KCAS (climb speed %.1f)\n",
                     id.c_str(), handed_over_at_ft, closest_m, altitude_there_ft,
                     steps / steps_per_second, worst_off_leg_m, slowest_climb_kcas, climb_kts);
        // **The first leg is never climbed more than 5 kt under her climb
        // speed**: the speed the take-off climbs her away at, which she is
        // handed over at, and the autopilot's floor holds her at clean.
        // Five, not the 2 the straight climb is held to (the next test),
        // because this leg turns, and a turn may spend 3 kt of it
        // (sim/autopilot.cpp). Asked for 80 kt here, she climbs faster than
        // it where she can. Measured (2026-10-10): the 172P 0.6 under, the
        // 182S 2.5 (in her turn), the Cub 0.3, the Cherokee 0.7. Before the
        // take-off climbed her at her published speed, she was handed over
        // under the floor, and the Cub, 8 kt under it, sagged 4.8 kt further
        // while the floor wound her climb back. With the take-off flap kept
        // out the Cherokee bled to 51.5 KCAS against her 63.8, then her
        // climb speed for her weight.
        check(climb_kts > 0.0 && slowest_climb_kcas >= climb_kts - 5.0,
              id + " climbed its first leg at no less than " +
                  std::to_string(slowest_climb_kcas) + " KCAS, against its climb speed of " +
                  std::to_string(climb_kts) + " (at most 5 under)");
        check(departing && took_off && handed_over_at_ft >= 500.0,
              id + " was taken off by the take-off autopilot, which handed it to the "
                   "plan at " + std::to_string(handed_over_at_ft) + " ft (500 asked)");
        check(controller.navigator() != nullptr && controller.navigator()->finished(),
              id + " flew the plan to its end after taking off");
        // **What strays it is the turn onto the leg**, and the faster she
        // flies it the further. The leg starts where she is handed over,
        // still on the runway's heading, 18 to 24 degrees right of it, so
        // nothing can anticipate the turn: she rolls in at 5 degrees a
        // second, a degree of bank per degree off (the 172P's bank peaks at
        // 18.6, never the 25 allowed), and is carried off the leg until her
        // track has come round, 8 s later. At a given bank she turns at
        // g tan(bank) / V and covers V a second, so the stray goes with the
        // square of her speed: handed over at her published climb speed
        // rather than the slower one for her weight (2026-10-09), the
        // 172P's 34.3 -> 38.7 m/s true is 1.27 times the square, and she
        // strays 63 -> 80 m. Flying the autopilot's and navigator's laws
        // alone at her speed (turn_onto_leg_strays_m) predicts every stray,
        // at either speed, within 2 m (2026-10-13): at her published speed,
        // predicted / flown, the 172P 78 / 80, the 182S 79 / 80, the Cub
        // 39 / 40, the Cherokee 62 / 60; at the slower one 62 / 63, 63 /
        // 65, 28 / 30, 47 / 46. So nothing else strays her, and the check
        // below holds every one within 4 m of it: more is something else
        // carrying her off the leg. Four metres, not a share of it, because
        // what the model leaves out - the bank lagging the bank asked, her
        // speed changing, her sideslip as she rolls - is a metre or two
        // whatever the stray (the most is the Cherokee's 2.0 of 61.5, the
        // Cub's 1.0 of 39.3), and twice that leaves room for another
        // platform's flight to differ by as much again; a share would give
        // the Cub 2 m to the Cherokee's 3. The bound is still tight enough
        // to see a turn flown wrong: the autopilot rolling at 4 degrees a
        // second rather than 5 strays the 172P 85.3 m against the 78.3
        // predicted, and at 3, 93.5; the 100 m bound alone saw neither.
        // (The "slower climb strays further" that
        // stood here was the Cherokee's take-off flap kept out, 151 m, since
        // brought up before the hand-over - sim/departure.hpp.) Flying the
        // leg from the threshold instead strays 219 to 383 m.
        const double turn_deg =
            std::remainder(handed_over_track_deg -
                               glideslope::sim::bearing_deg(handed_over_lat, handed_over_lon,
                                                            plan.waypoints[0].latitude_deg,
                                                            plan.waypoints[0].longitude_deg),
                           360.0);
        const double predicted_m = turn_onto_leg_strays_m(handed_over_true_mps, turn_deg);
        check(worst_off_leg_m <= 100.0,
              id + " flew its first leg from where the take-off handed over, straying " +
                  std::to_string(worst_off_leg_m) + " m from it (at most 100)");
        check(predicted_m > 0.0 && std::abs(worst_off_leg_m - predicted_m) <= 4.0,
              id + " strayed " + std::to_string(worst_off_leg_m) +
                  " m from its first leg, against the " + std::to_string(predicted_m) +
                  " m its turn onto it at " + std::to_string(handed_over_true_mps) +
                  " m/s true and " + std::to_string(turn_deg) +
                  " degrees predicts (within 4)");
        check(closest_m <= 100.0 && std::abs(altitude_there_ft - 2000.0) <= 50.0,
              id + " passed its waypoint " + std::to_string(closest_m) +
                  " m off (at most 100), at " + std::to_string(altitude_there_ft) +
                  " ft (within 50 of 2,000)");
        ++flown;
    }
    check(!light.empty() && flown == light.size(),
          "every light aeroplane in the catalogue flown: " + std::to_string(flown));
}

// **The take-off hands her to the plan at the speed the plan will hold.**
// Every light aeroplane in the catalogue, at her model's own loading (well
// under her handbook's weight: the 172P 1,879 lb against 2,400, the Cub 751
// against 1,220), taken off by a plan to 500 ft and flown straight on down
// the runway's heading to a waypoint 15 km off at 2,000 ft, asked at her
// climb speed. Her climb speed is her handbook's best rate of climb, the
// speed its climb figure was published at, for any lesser weight
// (DepartureSpeeds::climb_for_any_weight): the take-off climbs her away at
// it, and the autopilot's best-climb floor holds her at it after.
// - **No step at the hand-over**: she is handed over within 1 kt of it.
// - **The first leg is climbed at it**: within 2 kt of it from the
//   hand-over until she is within 50 ft of the waypoint's height.
// Measured (linux-release, 2026-10-10), handed over at / slowest / fastest
// against her climb speed, KCAS: the 172P 74.8 / 74.8 / 75.8 against 75.4;
// the 182S 81.4 / 81.4 / 83.1 against 82.0; the Cub 47.5 / 47.5 / 48.9
// against 47.8; the Cherokee 73.5 / 73.1 / 75.0 against 73.9. Before the
// autopilot seeded its climb loop with the pitch she had and eased the
// take-off's climb down to the plan's (sim/autopilot.cpp), she was pitched
// up as she was handed over and ran on past her speed as the plan's 700
// ft/min took over from the take-off's full-power climb: the 182S 80.6 to
// 84.0, the 172P down to 73.6.
// Straight ahead because a turn may spend 3 kt (sim/autopilot.cpp) and this
// is about the climb's speed, not the turn's. Before, the take-off climbed
// her at her published speed scaled by the square root of her weight and
// handed her to a floor at the published speed: the 172P at 66.7 against
// 75.4, and the Cub at 39.7 against 47.8, sagging to 34.9.
GLIDESLOPE_TEST(every_light_aeroplane_is_handed_to_its_plan_at_its_climb_speed_and_climbs_its_first_leg_at_it) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    // The space: every light aeroplane the catalogue holds, by name, so that
    // one added or taken away is seen here.
    const std::vector<std::string> expected{"c172p", "c182", "j3cub", "pa28"};
    std::vector<glideslope::sim::CatalogueEntry> light;
    for (const auto& e : glideslope::sim::read_catalogue(data)) {
        if (e.aircraft_class == glideslope::sim::AircraftClass::light_aircraft) {
            light.push_back(e);
        }
    }
    std::vector<std::string> named;
    for (const auto& e : light) {
        named.push_back(e.id);
    }
    std::sort(named.begin(), named.end());
    std::string listed;
    for (const auto& n : named) {
        listed += " " + n;
    }
    check(named == expected,
          "the light aeroplanes are the four this test names: found" + listed);
    constexpr double handed_within_kts = 1.0;
    constexpr double within_kts = 2.0;
    std::size_t flown = 0;
    for (const auto& entry : light) {
        const std::string& id = entry.id;
        const glideslope::sim::DepartureSpeeds speeds =
            glideslope::sim::departure_speeds(data, entry.model);
        const double climb_kts = speeds.climb_kts;
        const glideslope::sim::PlanSpeeds allowed = glideslope::sim::plan_speeds(data, entry.model);
        check(glideslope::sim::within_plan_speeds(allowed, std::round(climb_kts)),
              id + "'s climb speed, " + std::to_string(climb_kts) +
                  " KCAS, is one a plan may ask for");
        // 15 km down the runway's heading, 070, from its threshold.
        const double lat = -33.9461 + 15000.0 * std::cos(70.0 * std::numbers::pi / 180.0) /
                                          111320.0;
        const double lon = 151.1772 + 15000.0 * std::sin(70.0 * std::numbers::pi / 180.0) /
                                          (111320.0 * std::cos(33.92 * std::numbers::pi / 180.0));
        char waypoint[160];
        std::snprintf(waypoint, sizeof waypoint, "waypoint OUT %.5f %.5f 2000 %d\n", lat, lon,
                      static_cast<int>(std::lround(climb_kts)));
        const FlightPlan plan = parse_flight_plan(
            "aircraft " + id + "\nrunway 07 -33.9461 151.1772 0 70 3000\ntakeoff 500\n" +
            waypoint);
        glideslope::sim::Aircraft aircraft(data / "jsbsim", entry.model);
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = plan.takeoff->runway.threshold_lat_deg;
        ic.longitude_deg = plan.takeoff->runway.threshold_lon_deg;
        ic.altitude_ft = plan.takeoff->runway.elevation_ft;
        ic.terrain_elevation_ft = plan.takeoff->runway.elevation_ft;
        ic.heading_deg = plan.takeoff->runway.heading_deg;
        ic.airspeed_kts = 0.0;
        ic.engine_running = true;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        // The plan's autopilot holds her at the same speed the take-off
        // climbs her at.
        check(aircraft.climb_floor_kts() && *aircraft.climb_floor_kts() == climb_kts,
              id + "'s autopilot floor is her climb speed, " + std::to_string(climb_kts));
        glideslope::sim::Controller controller(aircraft, glideslope::sim::Controls{});
        controller.to_ai_flying(plan, speeds);
        double take_off_climb_kts = 0.0;
        bool took_off = false;
        double handed_over_kcas = 0.0;
        double slowest_kcas = std::numeric_limits<double>::infinity();
        double fastest_kcas = 0.0;
        double reached_ft = 0.0;
        int steps = 0;
        const int most_steps = 15 * 60 * steps_per_second;
        while (steps < most_steps) {
            aircraft.set_controls(controller.fly());
            aircraft.step();
            ++steps;
            const glideslope::sim::Navigator* navigator = controller.navigator();
            if (!took_off && controller.departure() != nullptr) {
                take_off_climb_kts = controller.departure()->speeds().initial_climb_kts;
                continue;
            }
            if (!took_off) {
                took_off = true;
                handed_over_kcas = aircraft.state().airspeed_kts;
            }
            const double h_ft = aircraft.property("position/h-sl-ft");
            reached_ft = std::max(reached_ft, h_ft);
            if (h_ft >= 1950.0 || navigator == nullptr || navigator->finished()) {
                break;
            }
            slowest_kcas = std::min(slowest_kcas, aircraft.state().airspeed_kts);
            fastest_kcas = std::max(fastest_kcas, aircraft.state().airspeed_kts);
        }
        std::fprintf(stderr,
                     "%s: climb speed %.1f KCAS, the take-off's %.1f; handed over at %.1f; "
                     "first leg climbed at %.1f to %.1f, to %.0f ft\n",
                     id.c_str(), climb_kts, take_off_climb_kts, handed_over_kcas, slowest_kcas,
                     fastest_kcas, reached_ft);
        check(took_off && std::abs(handed_over_kcas - climb_kts) <= handed_within_kts,
              id + " was handed to the plan at " + std::to_string(handed_over_kcas) +
                  " KCAS, against her climb speed of " + std::to_string(climb_kts) +
                  " (within 1)");
        check(reached_ft >= 1950.0, id + " climbed her first leg to " +
                                        std::to_string(reached_ft) + " ft (1,950 asked)");
        check(slowest_kcas >= climb_kts - within_kts && fastest_kcas <= climb_kts + within_kts,
              id + " climbed her first leg at " + std::to_string(slowest_kcas) + " to " +
                  std::to_string(fastest_kcas) + " KCAS, against her climb speed of " +
                  std::to_string(climb_kts) + " (within 2)");
        ++flown;
    }
    check(flown == expected.size(), "every light aeroplane flown: " + std::to_string(flown) +
                                        " of " + std::to_string(expected.size()));
}

// **The take-off flap comes up before the plan has her, and stays up.** Every
// aircraft the catalogue holds - a seaplane from water - is taken off by a
// plan to 500 ft and flown on to a waypoint 15 km off at 3,000 ft. By the
// hand-over to the plan the flap lever is up and the flaps have stopped
// there; from the moment they are up to the waypoint they never come out
// again, and she is never slower than her flaps-up stall: the stall at the
// least flap her figures publish (for the airliners that is their take-off
// flap's, a little below the clean stall; the 747-400 and the F-22A publish
// none, and are named below). The lever moves no faster than a
// hand, a 120th of its travel a step, and it stops at every notch between
// the take-off flap and up for at least a second.
GLIDESLOPE_TEST(every_aircraft_raises_its_take_off_flap_before_the_plan_has_it_and_keeps_above_its_flaps_up_stall) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const std::vector<glideslope::sim::CatalogueEntry> catalogue =
        glideslope::sim::read_catalogue(data);
    std::string failures;
    const auto expect = [&](bool condition, const std::string& what) {
        if (!condition) {
            failures += "\n  " + what;
        }
    };
    std::size_t flown = 0;
    std::size_t with_flap = 0;
    // **The notches each lever stops at on the way up**, between its
    // take-off flap and up, from its model's flap kinematic: the 737's 1
    // and 2 under its 5, the A320's 1, 2 and 5 under its 10, one each under
    // the 747's 20, the A380's 17, the 182S's 20 and the Cherokee's 25.
    // None for the 172P, 787 and Learjet, whose take-off flap is their
    // first notch, nor for the seven that take off clean.
    const std::map<std::string, std::size_t> notches_between{
        {"737-300", 2}, {"747-400", 1}, {"787-8", 0},     {"a320", 3},
        {"a380", 1},    {"b2", 0},      {"c172p", 0},     {"c182", 1},
        {"f15c", 0},    {"f22", 0},     {"f35b", 0},      {"j3cub", 0},
        {"learjet35a", 0}, {"mosquito-fb6", 0}, {"pa28", 1}, {"short_s23", 0}};
    for (const auto& entry : catalogue) {
        const std::string& id = entry.id;
        const glideslope::sim::DepartureSpeeds speeds =
            glideslope::sim::departure_speeds(data, entry.model);
        const glideslope::sim::PlanSpeeds allowed = glideslope::sim::plan_speeds(data, entry.model);
        const double leg_kts = std::clamp(speeds.climb_kts, allowed.slowest_kts, allowed.fastest_kts);
        // Her flaps-up stall: the stall at the least flap her figures publish.
        const glideslope::sim::PublishedFigures figures =
            glideslope::sim::read_published_figures(data / "figures" / (entry.model + ".xml"));
        double stall_kcas = 0.0;
        double stall_flap_deg = std::numeric_limits<double>::infinity();
        for (const auto& spec : figures.figures) {
            if (spec.flight != "stall_speed") {
                continue;
            }
            const auto at = spec.conditions.find("flaps_deg");
            const double flap_deg = at == spec.conditions.end() ? 0.0 : at->second;
            if (flap_deg < stall_flap_deg) {
                stall_flap_deg = flap_deg;
                stall_kcas = spec.published;
            }
        }
        const FlightPlan plan = parse_flight_plan(
            "aircraft " + id + "\nrunway 07 -33.9461 151.1772 0 70 3000\ntakeoff 500\n" +
            "waypoint OUT -33.85 151.30 3000 " + std::to_string(static_cast<int>(leg_kts)) + "\n");
        glideslope::sim::Aircraft aircraft(data / "jsbsim", entry.model);
        const bool seaplane = entry.seaplane;
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [seaplane](double, double) { return seaplane; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = plan.takeoff->runway.threshold_lat_deg;
        ic.longitude_deg = plan.takeoff->runway.threshold_lon_deg;
        ic.altitude_ft = plan.takeoff->runway.elevation_ft;
        ic.terrain_elevation_ft = plan.takeoff->runway.elevation_ft;
        ic.heading_deg = plan.takeoff->runway.heading_deg;
        ic.airspeed_kts = 0.0;
        ic.engine_running = true;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        glideslope::sim::Controller controller(aircraft, glideslope::sim::Controls{});
        controller.to_ai_flying(plan, speeds);

        // **Flaps the pilot works**: an aeroplane whose figures give its
        // flaps no travel has none to work. The F-22A's model moves its own
        // flaperons as its flight controls see fit, under fcs/flap-pos-deg,
        // and no lever reaches them.
        const bool has_position =
            figures.flaps_full_deg > 0.0 && aircraft.has_property("fcs/flap-pos-deg");
        const auto flap_deg = [&] {
            return has_position ? aircraft.property("fcs/flap-pos-deg") : 0.0;
        };
        // The notches the lever must stop at on the way up.
        std::vector<double> stops;
        for (const double notch : aircraft.flap_notches()) {
            if (notch > 1e-9 && notch < speeds.flap - 1e-9) {
                stops.push_back(notch);
            }
        }
        std::vector<int> held_at(stops.size(), 0);
        double last_lever = speeds.flap;
        double fastest_lever = 0.0;
        bool flaps_were_up = speeds.flap <= 0.0;
        bool out_again = false;
        double slowest_up_kcas = std::numeric_limits<double>::infinity();
        bool handed_over = false;
        double handed_lever = -1.0;
        double handed_deg = -1.0;
        double handed_ft = 0.0;
        double up_at_ft = 0.0;
        bool airborne = false;
        bool reached = false;
        int steps = 0;
        double first_lever = -1.0;
        for (; steps < 15 * 60 * steps_per_second; ++steps) {
            const glideslope::sim::Controls c = controller.fly();
            if (steps == 0) {
                first_lever = c.flaps;
            }
            aircraft.set_controls(c);
            aircraft.step();
            const double height_ft = aircraft.property("position/h-sl-ft");
            airborne = airborne || height_ft > 20.0;
            fastest_lever = std::max(fastest_lever, std::abs(c.flaps - last_lever));
            for (std::size_t i = 0; i < stops.size(); ++i) {
                if (std::abs(c.flaps - stops[i]) < 1e-9 && std::abs(last_lever - stops[i]) < 1e-9) {
                    ++held_at[i];
                }
            }
            last_lever = c.flaps;
            if (!handed_over && controller.departure() == nullptr) {
                handed_over = true;
                handed_lever = c.flaps;
                handed_deg = flap_deg();
                handed_ft = height_ft;
            }
            if (airborne && !flaps_were_up && c.flaps <= 0.0 && flap_deg() <= 0.5) {
                flaps_were_up = true;
                up_at_ft = height_ft;
            }
            if (flaps_were_up && airborne) {
                out_again = out_again || c.flaps > 0.0 || flap_deg() > 0.5;
                slowest_up_kcas = std::min(slowest_up_kcas, aircraft.state().airspeed_kts);
            }
            const glideslope::sim::Navigator* navigator = controller.navigator();
            if (handed_over && navigator != nullptr && navigator->finished()) {
                reached = true;
                break;
            }
        }
        std::fprintf(stderr,
                     "%s: take-off flap %.3f (%zu notches between it and up); up at %.0f ft; "
                     "handed over at %.0f ft with the lever at %.3f and the flaps at %.2f deg; "
                     "slowest with them up %.1f KCAS against a stall of %.1f; fastest lever "
                     "%.5f a step; %s in %d s\n",
                     id.c_str(), speeds.flap, stops.size(), up_at_ft, handed_ft, handed_lever,
                     handed_deg, slowest_up_kcas, stall_kcas, fastest_lever,
                     reached ? "reached OUT" : "did not reach OUT", steps / steps_per_second);
        if (speeds.flap > 0.0) {
            ++with_flap;
        }
        expect(first_lever == speeds.flap,
               id + " began its take-off with its lever at " + std::to_string(first_lever) +
                   ", its take-off flap " + std::to_string(speeds.flap));
        const auto expected = notches_between.find(id);
        expect(expected != notches_between.end() && expected->second == stops.size(),
               id + " has " + std::to_string(stops.size()) +
                   " notches between its take-off flap and up, not the number stated");
        expect(handed_over && reached, id + " was taken off and flew its plan to OUT");
        // **The take-off climbs past its 500 ft while the flap comes up**,
        // the airliners to 1,056-1,194 ft, but never past 1,500 ft above
        // the runway, where it ends whatever the flap (Departure, 14 CFR
        // 25.111(a)); 50 ft in hand for the step that crosses it.
        expect(handed_ft <= 1550.0,
               id + " was handed over at " + std::to_string(handed_ft) + " ft (1,500 at most)");
        expect(handed_lever == 0.0 && handed_deg <= 0.5,
               id + " was handed to the plan with its lever at " + std::to_string(handed_lever) +
                   " and its flaps at " + std::to_string(handed_deg) + " deg (up asked)");
        expect(flaps_were_up && !out_again,
               id + "'s flaps came up and stayed up to OUT");
        // **Two publish no stall**, and their speeds were measured from their
        // models (`<takeoff_speeds>`): the 747-400 and the F-22A. They are
        // left out of the stall bound and nothing else, and the test says
        // so if either comes to publish one.
        const bool no_stall_published = id == "747-400" || id == "f22";
        expect(no_stall_published == (stall_kcas <= 0.0),
               id + (no_stall_published ? " is named as publishing no stall, and publishes one"
                                        : " publishes no stall, and is not named"));
        expect(no_stall_published || slowest_up_kcas >= stall_kcas,
               id + " was never slower than its flaps-up stall with them up: " +
                   std::to_string(slowest_up_kcas) + " KCAS against " + std::to_string(stall_kcas));
        expect(fastest_lever <= 1.0 / steps_per_second + 1e-9,
               id + "'s lever moved " + std::to_string(fastest_lever) + " in a step (at most 1/120)");
        for (std::size_t i = 0; i < stops.size(); ++i) {
            expect(held_at[i] >= steps_per_second,
                   id + "'s lever stood at its notch " + std::to_string(stops[i]) + " for " +
                       std::to_string(held_at[i]) + " steps (a second asked)");
        }
        ++flown;
    }
    std::fprintf(stderr, "%zu of %zu aircraft flown, %zu taking off with flap\n", flown,
                 catalogue.size(), with_flap);
    // **The space, as numbers**: the catalogue's sixteen, every one flown,
    // nine of them taking off with flap - so one added, or a take-off flap
    // gained or lost, is seen here.
    check(catalogue.size() == 16 && flown == catalogue.size() &&
              notches_between.size() == catalogue.size(),
          "every aircraft in the catalogue flown: " + std::to_string(flown) + " of " +
              std::to_string(catalogue.size()) + " (16 stated)");
    check(with_flap == 9, "nine take off with flap: " + std::to_string(with_flap));
    check(failures.empty(), "every aircraft's take-off flap up before the plan:" + failures);
}

// **A plan that ends in a landing** (`land`): read - the runway whole, as a
// take-off's is - and refused wherever it is wrong, each way built.
GLIDESLOPE_TEST(a_landing_is_read_from_a_plan_and_refused_where_it_is_wrong) {
    const FlightPlan plan = parse_flight_plan(
        "aircraft c172p\n"
        "start -33.75 151.12 2500 168 90\n"
        "waypoint A -33.8 151.14 2200 80\n"
        "land YSSY_16R -33.929401 151.171997 8 167.9 3962\n");
    check(plan.landing && plan.landing->name == "YSSY_16R" &&
              plan.landing->threshold_lat_deg == -33.929401 &&
              plan.landing->threshold_lon_deg == 151.171997 &&
              plan.landing->elevation_ft == 8.0 && plan.landing->heading_deg == 167.9 &&
              plan.landing->length_m == 3962.0 && plan.waypoints.size() == 1,
          "the runway landed on, after the waypoints");
    check(!parse_flight_plan("aircraft c172p\nwaypoint A -33 151 3000 100\n").landing,
          "a plan without one lands nowhere");

    const std::string head = "aircraft c172p\nwaypoint A -33 151 3000 100\n";
    const std::string land = "land R -33.9 151.2 8 168 3962\n";
    const std::vector<std::pair<std::string, std::string>> wrong{
        {head + "land R -33.9 151.2 8 168\n", "land NAME"},
        {head + "land R -33.9 151.2 8 168 3962 more\n", "land NAME"},
        {head + "land R -95 151.2 8 168 3962\n", "the latitude must be a number"},
        {head + "land R -33.9 151.2 8 400 3962\n", "the heading must be a number"},
        {head + "land R -33.9 151.2 8 168 50\n", "the length must be a number"},
        {head + "land R -33.9 151.2 30000 168 3962\n", "the elevation must be a number"},
        {head + land + land, "a second land line"},
        {"aircraft c172p\n" + land, "no waypoints"},
        {head + land + "waypoint B -33 151 3000 100\n", "`land` is the last line"},
    };
    std::size_t refusals = 0;
    for (const auto& [text, says] : wrong) {
        check(refused(text, says), "refused, saying \"" + says + "\":\n" + text);
        ++refusals;
    }
    check(refusals == 9, "nine ways wrong, and every one refused");
}

// **A plan that ends in a vertical landing** (`land_vertically`): read - the
// point and the hover's seconds - and refused wherever it is wrong, each way
// built: its words, each number out of its range, a second one, one after
// `land` or before it, and a line after it.
GLIDESLOPE_TEST(a_vertical_landing_is_read_from_a_plan_and_refused_where_it_is_wrong) {
    const FlightPlan plan = parse_flight_plan(
        "aircraft f35b\n"
        "start -33.9461 151.1772 1000 70 250\n"
        "waypoint EAST -33.9122 151.2895 1000 250\n"
        "land_vertically PAD -33.8629 151.4527 30\n");
    check(plan.vertical_landing && plan.vertical_landing->name == "PAD" &&
              plan.vertical_landing->latitude_deg == -33.8629 &&
              plan.vertical_landing->longitude_deg == 151.4527 &&
              plan.vertical_landing->hover_s == 30.0 && !plan.landing &&
              plan.waypoints.size() == 1,
          "the point landed on and the hover's seconds, after the waypoints");
    check(!parse_flight_plan("aircraft f35b\nwaypoint A -33 151 3000 100\n").vertical_landing,
          "a plan without one lands nowhere");

    const std::string head = "aircraft f35b\nwaypoint A -33 151 3000 100\n";
    const std::string pad = "land_vertically P -33.9 151.2 30\n";
    const std::string land = "land R -33.9 151.2 8 168 3962\n";
    const std::vector<std::pair<std::string, std::string>> wrong{
        {head + "land_vertically P -33.9 151.2\n", "land_vertically NAME"},
        {head + "land_vertically P -33.9 151.2 30 more\n", "land_vertically NAME"},
        {head + "land_vertically P -95 151.2 30\n", "the latitude must be a number"},
        {head + "land_vertically P -33.9 190 30\n", "the longitude must be a number"},
        {head + "land_vertically P -33.9 151.2 -1\n", "the hover's seconds must be a number"},
        {head + "land_vertically P -33.9 151.2 601\n", "the hover's seconds must be a number"},
        {head + pad + pad, "a second land_vertically line"},
        {head + land + pad, "`land` is the last line"},
        {head + pad + land, "`land_vertically` is the last line"},
        {head + pad + "waypoint B -33 151 3000 100\n", "`land_vertically` is the last line"},
        {"aircraft f35b\n" + pad, "no waypoints"},
    };
    std::size_t refusals = 0;
    for (const auto& [text, says] : wrong) {
        check(refused(text, says), "refused, saying \"" + says + "\":\n" + text);
        ++refusals;
    }
    check(refusals == 11, "eleven ways wrong, and every one refused");
}

// **A plan names her loading** (`loading NAME`), as a player's `--loading`
// does: read, and held to her figures by name where it is flown
// (sim::refuse_what_it_cannot_fly). Every named loading of every aircraft in
// the catalogue is accepted for her, the count stated; one her figures do
// not name is refused, naming it and the ones they do; and its words, and a
// second one, are refused as it is read.
GLIDESLOPE_TEST(a_plans_loading_is_one_her_figures_name_or_the_plan_is_refused_by_it) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const FlightPlan plan = parse_flight_plan(
        "aircraft f35b\nloading vertical_landing\nwaypoint A -33.9 151.2 1000 250\n");
    check(plan.loading == "vertical_landing", "her loading is read: " + plan.loading);
    check(parse_flight_plan("aircraft f35b\nwaypoint A -33.9 151.2 1000 250\n").loading.empty(),
          "a plan naming none flies her model's own");

    // The loading's refusal, or none: the speeds are another check's.
    const auto loading_refusal = [&](const std::string& id, const std::string& name) {
        try {
            FlightPlan p = parse_flight_plan("aircraft " + id + "\nloading " + name +
                                             "\nwaypoint A -33.9 151.2 3000 100\n");
            glideslope::sim::refuse_what_it_cannot_fly(data, p, id);
        } catch (const FlightPlanError& e) {
            const std::string why = e.what();
            return why.find("loading") != std::string::npos ? why : std::string();
        }
        return std::string();
    };
    std::size_t aircraft = 0;
    std::size_t accepted = 0;
    std::size_t named = 0;
    for (const glideslope::sim::CatalogueEntry& e : glideslope::sim::read_catalogue(data)) {
        ++aircraft;
        const std::filesystem::path file = data / "figures" / (e.model + ".xml");
        if (std::filesystem::exists(file)) {
            for (const auto& [name, loading] :
                 glideslope::sim::read_published_figures(file).loadings) {
                if (name.empty()) {
                    continue;
                }
                ++named;
                const std::string why = loading_refusal(e.id, name);
                check(why.empty(), e.id + "'s " + name + " is flown: " + why);
                accepted += why.empty() ? 1U : 0U;
            }
        }
        const std::string unknown = loading_refusal(e.id, "no_such_loading");
        check(unknown.find("the plan's loading no_such_loading is not one the " + e.id +
                           "'s figures name") != std::string::npos,
              e.id + " refuses a loading her figures do not name, by name: " + unknown);
    }
    check(named == 41, "the catalogue's aircraft name 41 loadings, not " + std::to_string(named) +
                           ": a loading added or removed is counted here");
    check(aircraft == 16 && accepted == named && named > 0,
          "every named loading of the 16 aircraft is flown: " + std::to_string(accepted) +
              " of " + std::to_string(named) + ", of " + std::to_string(aircraft) + " aircraft");
    const std::string f35b = loading_refusal("f35b", "heavy");
    check(f35b.find("they name") != std::string::npos &&
              f35b.find("vertical_landing") != std::string::npos,
          "and says the ones her figures do name: " + f35b);
    std::printf("%zu named loadings of %zu aircraft flown from a plan; an unknown one refused "
                "for each\n",
                accepted, aircraft);

    const std::string head = "aircraft f35b\nwaypoint A -33 151 3000 250\n";
    check(refused("aircraft f35b\nloading\n" + head.substr(14), "loading NAME"),
          "a loading with no name is refused");
    check(refused("aircraft f35b\nloading a b\n" + head.substr(14), "loading NAME"),
          "a loading of two words is refused");
    check(refused("aircraft f35b\nloading full\nloading combat\n" + head.substr(14),
                  "a second loading line"),
          "a second loading is refused");
}
