#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/navigator.hpp"
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
