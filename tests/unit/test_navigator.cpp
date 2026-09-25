#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/catalogue.hpp"
#include "sim/controller.hpp"
#include "sim/departure.hpp"
#include "sim/navigator.hpp"
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
    };
    std::size_t refusals = 0;
    for (const auto& [text, says] : wrong) {
        check(refused(text, says), "refused, saying \"" + says + "\":\n" + text);
        ++refusals;
    }
    check(refusals == 12, "twelve ways wrong, and every one refused");
}

GLIDESLOPE_TEST(the_navigator_flies_an_orbit_round_its_centre_as_often_as_asked_either_way_in_calm_air_and_in_wind) {
    // A Cessna 3 km south of the centre of a 1,500 m circle, heading for it,
    // twice round and then on to a waypoint north of it.
    std::size_t flown = 0;
    for (const bool right : {false, true}) {
        for (const bool windy : {false, true}) {
            const FlightPlan plan = parse_flight_plan(
                std::string("aircraft c172p\nstart -33.90 151.20 3000 0 90\n"
                            "orbit CBD -33.8688 151.2093 1500 3000 90 2 ") +
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
            const std::string which = std::string(right ? "right" : "left") +
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
            // Stated from what was measured (PROJECT_STATUS.md).
            check(nearest_m >= 1500.0 - 100.0 && farthest_m <= 1500.0 + 100.0,
                  "on the circle within 100 m, " + which + ": " + std::to_string(nearest_m) +
                      " to " + std::to_string(farthest_m) + " m");
            check(worst_altitude_ft <= 50.0,
                  "at its altitude within 50 ft, " + which + ": " +
                      std::to_string(worst_altitude_ft));
            ++flown;
        }
    }
    check(flown == 4, "both ways round, in calm air and in wind: four orbits flown");
}

GLIDESLOPE_TEST(a_plan_that_takes_off_leaves_its_runway_and_flies_its_waypoints_in_every_light_aeroplane) {
    // From a runway at sea level on flat ground, to 500 ft above it, and then
    // to a waypoint 15 km off to the left of the runway's heading at 2,000 ft:
    // far enough for the slowest of them, the Cub, to have climbed to it.
    const std::filesystem::path data =
        std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const std::vector<std::string> light{"c172p", "c182", "pa28", "j3cub"};
    std::size_t flown = 0;
    for (const std::string& id : light) {
        const auto entry = glideslope::sim::find_aircraft(data, id);
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
        }
        std::fprintf(stderr,
                     "%s: taken off to %.0f ft, and passed OUT %.0f m off at %.0f ft, in %d s\n",
                     id.c_str(), handed_over_at_ft, closest_m, altitude_there_ft,
                     steps / steps_per_second);
        check(departing && took_off && handed_over_at_ft >= 500.0,
              id + " was taken off by the take-off autopilot, which handed it to the "
                   "plan at " + std::to_string(handed_over_at_ft) + " ft (500 asked)");
        check(controller.navigator() != nullptr && controller.navigator()->finished(),
              id + " flew the plan to its end after taking off");
        check(closest_m <= 100.0 && std::abs(altitude_there_ft - 2000.0) <= 50.0,
              id + " passed its waypoint " + std::to_string(closest_m) +
                  " m off (at most 100), at " + std::to_string(altitude_there_ft) +
                  " ft (within 50 of 2,000)");
        ++flown;
    }
    check(flown == light.size() && flown == 4, "all four light aeroplanes flown");
}
