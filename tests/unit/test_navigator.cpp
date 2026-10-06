#include "harness.hpp"

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
#include "sim/weather.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
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
// or, where given, `within_fraction` of its radius.
// The space is every case of every aircraft in it, and every one is flown.
void fly_the_tightest_orbits(const std::vector<glideslope::sim::CatalogueEntry>& space, End end,
                             double within_m, double within_fraction = 0.0) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    std::size_t flown = 0;
    std::string failures;
    for (const auto& entry : space) {
        const glideslope::sim::PlanSpeeds speeds =
            glideslope::sim::plan_speeds(data, entry.model);
        const double kts = end == End::slowest ? speeds.slowest_kts : speeds.fastest_kts;
        check(kts == std::round(kts), entry.id + "'s speeds are whole knots, as a plan writes them");
        for (const bool right : {false, true}) {
        for (const bool windy : {false, true}) {
            glideslope::sim::OrbitTrial trial;
            trial.airspeed_kts = kts;
            trial.right = right;
            trial.windy = windy;
            const glideslope::sim::OrbitFlown f =
                glideslope::sim::fly_tightest_orbit(data, entry, trial);
            const double circle_m = within_fraction > 0.0 ? within_fraction * f.radius_m : within_m;
            char which[400];
            std::snprintf(which, sizeof which,
                          "%s round %.0f m at %.0f kt, %s, %s: %.2f turns, %.0f to %.0f m from "
                          "the centre (%+.0f to %+.0f, %.1f%%), within %.0f ft, %.0f to %.0f kt",
                          entry.id.c_str(), f.radius_m, kts, right ? "right" : "left",
                          windy ? "in a 10 kt wind" : "in calm air", f.turns, f.nearest_m,
                          f.farthest_m, f.nearest_m - f.radius_m, f.farthest_m - f.radius_m,
                          100.0 * std::max(f.radius_m - f.nearest_m, f.farthest_m - f.radius_m) /
                              f.radius_m,
                          f.worst_height_ft, f.slowest_kts, f.fastest_kts);
            std::fprintf(stderr, "%s\n", which);
            const bool on_circle =
                f.nearest_m >= f.radius_m - circle_m && f.farthest_m <= f.radius_m + circle_m;
            if (!f.held(kts) || !on_circle) {
                failures += std::string("\n  ") + which;
            }
            ++flown;
        }
        }
    }
    std::fprintf(stderr, "%zu aircraft x 2 ways round x 2 airs = %zu orbits: %zu flown\n",
                 space.size(), space.size() * 4, flown);
    check(flown == space.size() * 4 && flown > 0,
          "every case flown: " + std::to_string(flown) + " of " +
              std::to_string(space.size() * 4));
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

        // Each waypoint's closest approach while it is the one being flown to,
        // and the altitude there.
        std::vector<double> closest(plan.waypoints.size(),
                                    std::numeric_limits<double>::infinity());
        std::vector<double> altitude_there(plan.waypoints.size(), 0.0);
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
            }
            if (navigator.finished()) {
                break;
            }
            const auto& p = plan.waypoints[navigator.next()];
            const double d =
                glideslope::sim::distance_m(aircraft.property("position/lat-geod-deg"),
                                            aircraft.property("position/long-gc-deg"),
                                            p.latitude_deg, p.longitude_deg);
            if (d < closest[navigator.next()]) {
                closest[navigator.next()] = d;
                altitude_there[navigator.next()] =
                    aircraft.property("position/h-sl-ft");
            }
        }
        const std::string air = windy ? " in a 20 kt wind" : " in calm air";
        check(navigator.finished(), "the plan is flown to its end" + air + " in " +
                                        std::to_string(steps / steps_per_second) +
                                        " s");
        check(order == std::vector<std::size_t>{0, 1, 2, 3},
              "every waypoint is passed, in order" + air);
        // Stated from what was measured (PROJECT_STATUS.md): each waypoint passed
        // within 100 m, at its altitude within 50 ft.
        for (std::size_t i = 0; i < plan.waypoints.size(); ++i) {
            std::fprintf(stderr, "%s%s: passed %.1f m off, at %.0f ft (%.0f planned)\n",
                         plan.waypoints[i].name.c_str(), air.c_str(), closest[i],
                         altitude_there[i], plan.waypoints[i].altitude_ft);
            check(closest[i] <= 100.0 &&
                      std::abs(altitude_there[i] - plan.waypoints[i].altitude_ft) <=
                          50.0,
                  plan.waypoints[i].name + air + ": passed " +
                      std::to_string(closest[i]) + " m off (at most 100), at " +
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
            check(joined_at_m >= radius_m - 101.0 && joined_at_m <= radius_m + 1.0,
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

// Every aircraft in the catalogue of one of `classes`.
std::vector<glideslope::sim::CatalogueEntry> of_classes(const std::vector<AircraftClass>& classes) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    std::vector<glideslope::sim::CatalogueEntry> out;
    for (const auto& e : glideslope::sim::read_catalogue(data)) {
        if (std::find(classes.begin(), classes.end(), e.aircraft_class) != classes.end()) {
            out.push_back(e);
        }
    }
    return out;
}

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
        verdict("aircraft mosquito-fb6\nwaypoint A -33.90 151.2093 3000 240\n");
    check(fast.find("A is flown at 240 kt, outside 123 to 229 kt") != std::string::npos,
          "the Mosquito at 240 kt, more than she makes, is refused: " + fast);
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
// to spare, so 10 kt past it is the last speed it saw not hold. Every
// aircraft whose slowest is above its approach speed (or that has none) is
// flown there, and every one whose fastest is below a fifth over its start
// speed: none may hold what a plan asks (sim::holds_plan_speed - the orbit
// four ways, a heading in calm air and a crosswind). The rest are at the
// bound plans were held to before, which nothing is asked past.
GLIDESLOPE_TEST(no_aircraft_holds_what_a_plan_asks_one_step_past_its_slowest_or_fastest) {
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    std::string failures;
    std::size_t tried = 0;
    std::size_t at_the_old_bound = 0;
    const auto catalogue = glideslope::sim::read_catalogue(data);
    for (const auto& e : catalogue) {
        const glideslope::sim::PlanSpeeds speeds = glideslope::sim::plan_speeds(data, e.model);
        double approach = 0.0;
        try {
            approach = std::round(glideslope::sim::approach_speeds(data, e.model).vref_kts);
        } catch (const std::runtime_error&) {
            // The 747-400 and the F-22 have none: their slowest was sought.
        }
        const auto past = [&](double kts, const char* which) {
            const bool held = glideslope::sim::holds_plan_speed(
                data, e, kts, [](const std::string& line) { std::printf("  %s\n", line.c_str()); });
            std::printf("%s 10 kt past its %s, at %.0f kt: %s\n", e.id.c_str(), which, kts,
                        held ? "held" : "not held");
            if (held) {
                failures += "\n  " + e.id + " holds " + std::to_string(kts) + " kt, 10 past its " +
                            which;
            }
            ++tried;
        };
        if (speeds.slowest_kts > approach) {
            past(speeds.slowest_kts - 10.0, "slowest");
        } else {
            ++at_the_old_bound;
        }
        if (speeds.fastest_kts < std::round(1.2 * e.start_airspeed_kts)) {
            past(speeds.fastest_kts + 10.0, "fastest");
        } else {
            ++at_the_old_bound;
        }
    }
    std::printf("%zu bounds tried 10 kt past, %zu at the old bound, of %zu\n", tried,
                at_the_old_bound, 2 * catalogue.size());
    check(tried + at_the_old_bound == 2 * catalogue.size() && tried > 0,
          "every aircraft's slowest and fastest tried or at the old bound");
    check(failures.empty(), "none holds 10 kt past what its file gives:" + failures);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_every_light_aeroplane) {
    // Claude's plan for the Cessna, 521 m at 60 kt, was flown 94 to 127 m
    // inside its circle: the navigator led the tangent by five seconds, more
    // than the autopilot needs ahead of it to bank for so tight a circle.
    // Every light aeroplane in the catalogue, so that one added is flown.
    // Stated from what was measured (PROJECT_STATUS.md): at worst 50 m off;
    // held to 60.
    std::vector<AircraftClass> grouped;
    for (const auto& group : tightest_orbit_groups) {
        grouped.insert(grouped.end(), group.begin(), group.end());
    }
    std::sort(grouped.begin(), grouped.end());
    check(grouped.size() == glideslope::sim::aircraft_class_count &&
              std::adjacent_find(grouped.begin(), grouped.end()) == grouped.end(),
          "the tightest-orbit tests take every class of aircraft, each once: " +
              std::to_string(grouped.size()) + " of " +
              std::to_string(glideslope::sim::aircraft_class_count));
    fly_the_tightest_orbits(of_classes(tightest_orbit_groups[0]), End::slowest, 60.0);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_every_airliner_and_business_jet) {
    // Until each had its own slowest, a plan could fly a jet clean at its
    // approach speed, a flaps-down figure: five of the six came down to the
    // ground round this orbit, and the A320 lost 600 ft. Stated from what was
    // measured (PROJECT_STATUS.md).
    fly_the_tightest_orbits(of_classes(tightest_orbit_groups[1]), End::slowest, 0.0, 0.05);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_every_fighter_and_bomber) {
    // Until each had its own slowest, the F-35B came down to the ground round
    // this orbit at its approach speed, and the B-2 stalled turning towards
    // it; the F-22 had no approach speed to fly. Stated from what was
    // measured (PROJECT_STATUS.md).
    fly_the_tightest_orbits(of_classes(tightest_orbit_groups[2]), End::slowest, 0.0, 0.05);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_every_warbird_and_flying_boat) {
    // Stated from what was measured (PROJECT_STATUS.md): at worst 36 m off;
    // held to 60, as the light aeroplanes are.
    fly_the_tightest_orbits(of_classes(tightest_orbit_groups[3]), End::slowest, 60.0);
}

// **At the fastest a plan may ask**, the height and speed are held as at the
// slowest, and so is the circle: the light aeroplanes', the Mosquito's and
// the S.23's to 60 m, the jets' to a share of the radius stated from what was
// measured (PROJECT_STATUS.md): at worst 1.3%, the A380's 172 m of 13 km at
// 300 kt, and the fighters' 0.9%, held to 2%. The navigator turned in
// towards the circle by
// 90 degrees a kilometre whatever the speed, and the jets swung through their
// 13 to 19 km circles by up to 18% of the radius (3.3 km, the F-15C at 360
// kt); it now turns in by what brings it back in twelve seconds at its
// airspeed (sim/navigator.cpp).
GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_every_light_aeroplane) {
    fly_the_tightest_orbits(of_classes(tightest_orbit_groups[0]), End::fastest, 60.0);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_every_airliner_and_business_jet) {
    fly_the_tightest_orbits(of_classes(tightest_orbit_groups[1]), End::fastest, 0.0, 0.02);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_every_fighter_and_bomber) {
    fly_the_tightest_orbits(of_classes(tightest_orbit_groups[2]), End::fastest, 0.0, 0.02);
}

GLIDESLOPE_TEST(the_tightest_orbit_at_the_fastest_speed_a_plan_may_ask_is_flown_by_every_warbird_and_flying_boat) {
    fly_the_tightest_orbits(of_classes(tightest_orbit_groups[3]), End::fastest, 60.0);
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
            }
            const glideslope::sim::Navigator* navigator = controller.navigator();
            if (navigator == nullptr || navigator->finished()) {
                break;
            }
            if (!took_off) {
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
        }
        std::fprintf(stderr,
                     "%s: taken off to %.0f ft, and passed OUT %.0f m off at %.0f ft, in %d s, "
                     "straying at most %.0f m from the leg from where it was handed over\n",
                     id.c_str(), handed_over_at_ft, closest_m, altitude_there_ft,
                     steps / steps_per_second, worst_off_leg_m);
        check(departing && took_off && handed_over_at_ft >= 500.0,
              id + " was taken off by the take-off autopilot, which handed it to the "
                   "plan at " + std::to_string(handed_over_at_ft) + " ft (500 asked)");
        check(controller.navigator() != nullptr && controller.navigator()->finished(),
              id + " flew the plan to its end after taking off");
        // Stated from what was measured: at most 81 m, where flying the leg
        // from the threshold instead strays 219 to 383 m.
        check(worst_off_leg_m <= 150.0,
              id + " flew its first leg from where the take-off handed over, straying " +
                  std::to_string(worst_off_leg_m) + " m from it (at most 150)");
        check(closest_m <= 100.0 && std::abs(altitude_there_ft - 2000.0) <= 50.0,
              id + " passed its waypoint " + std::to_string(closest_m) +
                  " m off (at most 100), at " + std::to_string(altitude_there_ft) +
                  " ft (within 50 of 2,000)");
        ++flown;
    }
    check(!light.empty() && flown == light.size(),
          "every light aeroplane in the catalogue flown: " + std::to_string(flown));
}
