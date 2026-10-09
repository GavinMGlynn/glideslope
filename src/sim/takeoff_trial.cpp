#include "sim/takeoff_trial.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>

#include "sim/aircraft.hpp"
#include "sim/crash.hpp"
#include "sim/figures.hpp"
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
        // **Climbed out**, where the take-off was over when these trials
        // were written: it now climbs on until its take-off flap is up too.
        if (departure.climbed_out()) {
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

MeasuredTakeoff measure_takeoff_speeds(const std::filesystem::path& data,
                                       const CatalogueEntry& entry, double from_kts,
                                       const std::function<void(const std::string&)>& say) {
    const PublishedFigures figures =
        read_published_figures(data / "figures" / (entry.model + ".xml"));
    MeasuredTakeoff out;
    for (const FigureSpec& spec : figures.figures) {
        if (spec.flight == "takeoff_field_length") {
            const auto flap = spec.conditions.find("flaps_deg");
            out.flaps_deg = flap == spec.conditions.end() ? 0.0 : flap->second;
        }
    }
    const auto trial = [&](double rotate_kts, double climb_kts) {
        DepartureSpeeds speeds;
        speeds.rotate_kts = rotate_kts;
        speeds.climb_kts = climb_kts;
        speeds.initial_climb_kts = climb_kts;
        // **Measured at the take-off flap**, as the speeds are written
        // (`flaps_deg`): the flap stays out to where the climb away is read
        // (Departure::climbed_out).
        speeds.flaps_up_ft = std::numeric_limits<double>::infinity();
        speeds.flap = figures.flaps_full_deg > 0.0
                          ? std::clamp(out.flaps_deg / figures.flaps_full_deg, 0.0, 1.0)
                          : 0.0;
        TakeoffFlown t = fly_takeoff_trial(data, entry, speeds);
        const bool lifted = t.lifted_off(speeds);
        char line[300];
        std::snprintf(line, sizeof line,
                      "%s: rotate %.0f, climb %.0f kt: %s, unstuck %.0f m at %.0f kt, slowest "
                      "%.0f kt airborne, %.0f kt at 500 ft, %.0f s, %.0f lb%s%s",
                      entry.id.c_str(), rotate_kts, climb_kts,
                      lifted ? "lifted off" : "did not lift off", t.unstuck_m, t.unstuck_kts,
                      t.slowest_airborne_kts, t.handed_over_kts, t.seconds, t.weight_lbs,
                      t.wrecked.empty() ? "" : ", wrecked: ", t.wrecked.c_str());
        say(line);
        return std::make_pair(lifted, t);
    };
    double rotate = 0.0;
    for (double kts = from_kts; kts <= 300.0; kts += 5.0) {
        const auto [lifted, t] = trial(kts, kts + 20.0);
        if (lifted) {
            rotate = kts;
            out.weight_lbs = std::round(t.weight_lbs);
            break;
        }
    }
    if (rotate == 0.0) {
        out.why_not = "no rotation up to 300 kt holds";
        return out;
    }
    out.rotate_kts = rotate + 5.0;
    // **The climb away: the slowest it climbs away at.** Each asked is at
    // 500 ft at least where its nose, held no higher than the take-off
    // autopilot holds it, lets it be: the 747-400 at about 197 kt asked
    // anything under 195, the F-22A, at military power, at 216 asked 125.
    double slowest = std::numeric_limits<double>::infinity();
    for (double kts = out.rotate_kts + 10.0; kts <= out.rotate_kts + 100.0; kts += 5.0) {
        const auto [lifted, t] = trial(out.rotate_kts, kts);
        if (lifted) {
            slowest = std::min(slowest, t.handed_over_kts);
        }
    }
    if (!std::isfinite(slowest)) {
        out.why_not = "no take-off climbed away";
        return out;
    }
    out.slowest_away_kts = slowest;
    out.climb_kts = std::ceil(slowest / 5.0) * 5.0;
    return out;
}

} // namespace glideslope::sim
