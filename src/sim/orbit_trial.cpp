#include "sim/orbit_trial.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/navigator.hpp"
#include "sim/plan.hpp"
#include "sim/weather.hpp"

namespace glideslope::sim {

OrbitFlown fly_tightest_orbit(const std::filesystem::path& data, const CatalogueEntry& entry,
                              const OrbitTrial& trial) {
    constexpr int steps_per_second = 120;
    const double kts = trial.airspeed_kts;
    OrbitFlown out;
    out.radius_m = std::ceil(least_orbit_radius_m(kts));
    char lines[400];
    std::snprintf(lines, sizeof lines,
                  "aircraft %s\nstart %.6f 151.2093 3000 0 %.0f\n"
                  "orbit CBD -33.8688 151.2093 %.0f 3000 %.0f 2 %s\n"
                  "waypoint NORTH -33.70 151.2093 3000 %.0f\n",
                  entry.id.c_str(), -33.8688 - (out.radius_m + 2000.0) / 111195.0, kts,
                  out.radius_m, kts, trial.right ? "right" : "left", kts);
    const FlightPlan plan = parse_flight_plan(lines);
    Aircraft aircraft(data / "jsbsim", entry.model);
    InitialConditions ic;
    ic.latitude_deg = plan.start->latitude_deg;
    ic.longitude_deg = plan.start->longitude_deg;
    ic.altitude_ft = plan.start->altitude_ft;
    ic.heading_deg = plan.start->heading_deg;
    ic.airspeed_kts = plan.start->airspeed_kts;
    ic.engine_running = true;
    aircraft.initialize(ic);
    if (trial.windy) {
        Conditions wind;
        wind.wind_east_mps = 10.0 * 1852.0 / 3600.0;
        aircraft.set_weather(std::make_shared<SteadyWeather>(wind));
    }
    Controls controls;
    controls.throttle = entry.start_throttle;
    Autopilot autopilot(aircraft, controls);
    Navigator navigator(aircraft, plan);

    const Waypoint& centre = plan.waypoints[0];
    out.nearest_m = std::numeric_limits<double>::infinity();
    out.slowest_kts = std::numeric_limits<double>::infinity();
    int steps = 0;
    const int most_steps = 30 * 60 * steps_per_second;
    while (navigator.next() == 0 && steps < most_steps) {
        autopilot.set(navigator.steer());
        aircraft.set_controls(autopilot.fly());
        aircraft.step();
        ++steps;
        // Fallen 500 ft on the way to the circle, it will not hold it.
        if (trial.stop_when_lost &&
            std::abs(aircraft.property("position/h-sl-ft") - centre.altitude_ft) > 500.0) {
            out.worst_height_ft = std::max(out.worst_height_ft, 500.0);
            return out;
        }
        if (!navigator.circling()) {
            continue;
        }
        out.turns = std::max(out.turns, navigator.turns_flown());
        if (navigator.turns_flown() >= 0.25) {
            const double d = distance_m(centre.latitude_deg, centre.longitude_deg,
                                        aircraft.property("position/lat-geod-deg"),
                                        aircraft.property("position/long-gc-deg"));
            out.nearest_m = std::min(out.nearest_m, d);
            out.farthest_m = std::max(out.farthest_m, d);
            out.worst_height_ft =
                std::max(out.worst_height_ft,
                         std::abs(aircraft.property("position/h-sl-ft") - centre.altitude_ft));
            const double v = aircraft.property("velocities/vc-kts");
            out.slowest_kts = std::min(out.slowest_kts, v);
            out.fastest_kts = std::max(out.fastest_kts, v);
            if (trial.stop_when_lost &&
                (out.worst_height_ft > 50.0 || out.slowest_kts < kts - 5.0 ||
                 out.fastest_kts > kts + 5.0)) {
                return out;
            }
        }
    }
    out.passed_on = navigator.next() == 1 && out.turns >= 1.99 && out.turns <= 2.01;
    return out;
}

} // namespace glideslope::sim
