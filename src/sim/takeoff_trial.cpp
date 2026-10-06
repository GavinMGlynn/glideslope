#include "sim/takeoff_trial.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>

#include "sim/aircraft.hpp"
#include "sim/crash.hpp"
#include "sim/terrain.hpp"

namespace glideslope::sim {

bool TakeoffFlown::lifted_off(const DepartureSpeeds& speeds) const {
    return handed_over && wrecked.empty() && unstuck_m > 0.0 &&
           unstuck_m <= takeoff_trial_runway_m && unstuck_kts <= speeds.rotate_kts + 15.0 &&
           slowest_airborne_kts >= speeds.rotate_kts - 5.0;
}

bool TakeoffFlown::held(const DepartureSpeeds& speeds) const {
    return lifted_off(speeds) && handed_over_kts >= speeds.climb_kts - 10.0;
}

TakeoffFlown fly_takeoff_trial(const std::filesystem::path& data, const CatalogueEntry& entry,
                               const DepartureSpeeds& speeds) {
    constexpr int steps_per_second = 120;
    Runway runway;
    runway.name = "trial";
    runway.threshold_lat_deg = -33.9461;
    runway.threshold_lon_deg = 151.1772;
    runway.elevation_ft = 0.0;
    runway.heading_deg = 70.0;
    runway.length_m = takeoff_trial_runway_m;

    Aircraft aircraft(data / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<FunctionTerrain>([](double, double) { return 0.0; },
                                                           [](double, double) { return false; }));
    InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg;
    ic.longitude_deg = runway.threshold_lon_deg;
    ic.altitude_ft = runway.elevation_ft;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = 0.0;
    ic.engine_running = true;
    ic.gear = 1.0;
    aircraft.initialize(ic);

    TakeoffFlown out;
    out.weight_lbs = aircraft.property("inertia/weight-lbs");
    out.slowest_airborne_kts = std::numeric_limits<double>::infinity();
    Departure departure(aircraft, runway, speeds);
    GroundJudge judge(false);
    int step = 0;
    for (; step < 300 * steps_per_second; ++step) {
        aircraft.set_controls(departure.fly());
        aircraft.step();
        if (std::optional<std::string> why = judge.judge(aircraft)) {
            out.wrecked = *why;
            break;
        }
        if (departure.unstuck_along_m() > 0.0) {
            if (out.unstuck_kts == 0.0) {
                out.unstuck_kts = aircraft.state().airspeed_kts;
            }
            out.slowest_airborne_kts =
                std::min(out.slowest_airborne_kts, aircraft.state().airspeed_kts);
        }
        if (departure.stage() == Departure::Stage::done) {
            out.handed_over = true;
            out.handed_over_kts = aircraft.state().airspeed_kts;
            break;
        }
    }
    out.unstuck_m = departure.unstuck_along_m();
    if (out.unstuck_m <= 0.0) {
        out.slowest_airborne_kts = 0.0;
    }
    out.seconds = static_cast<double>(step) / steps_per_second;
    return out;
}

} // namespace glideslope::sim
