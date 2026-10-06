#pragma once

// **The tightest orbit a plan may ask, flown**: an aircraft started in the
// air 2 km outside the circle, heading for its centre, flown by the
// navigator and the autopilot twice round and on - the flight the orbit
// tests make, and the one `glideslope_cli plan-speeds` sweeps to measure the
// slowest and fastest each aircraft may be planned at (its `<plan_speeds>`,
// in its figures file).

#include <filesystem>
#include <functional>
#include <string>

#include "sim/catalogue.hpp"

namespace glideslope::sim {

struct OrbitTrial {
    double airspeed_kts = 0.0; // asked of the plan, in whole knots as it writes them
    bool right = false;        // the way round
    bool windy = false;        // a 10 kt wind from the west, or calm air
    // Stop as soon as its height or speed is lost - by more than 50 ft or
    // 5 kt, from its first quarter-turn - rather than flying it out: for a
    // sweep, which wants only whether it held.
    bool stop_when_lost = false;
};

struct OrbitFlown {
    double radius_m = 0.0;      // the tightest the plan reader allows at the speed
    double turns = 0.0;         // the most turns the navigator counted
    bool passed_on = false;     // round twice and on to the next waypoint
    // From the first half-turn on. Begun 2 km outside heading for the
    // centre, a jet at 360 kt, turning on 7.9 km at 25 degrees of bank, goes
    // kilometres inside an 18.8 km circle before it is round onto it.
    double nearest_m = 0.0;     // from the centre
    double farthest_m = 0.0;
    // And the join, from first reaching the circle on, so that it is seen.
    double join_nearest_m = 0.0;
    double join_farthest_m = 0.0;
    // From the first quarter-turn on:
    double worst_height_ft = 0.0; // off the orbit's height, either way
    double slowest_kts = 0.0;     // calibrated
    double fastest_kts = 0.0;

    // Its height within 50 ft and its speed within 5 kt of what was asked.
    bool held(double asked_kts) const {
        return passed_on && worst_height_ft <= 50.0 && slowest_kts >= asked_kts - 5.0 &&
               fastest_kts <= asked_kts + 5.0;
    }
};

// **A heading held in a crosswind**: `entry` at 3,000 ft heading north at
// `airspeed_kts`, the autopilot alone holding the heading, the height and
// the speed for two minutes, in calm air or in a 20 kt wind from the west
// arriving all at once - the crosswind test's flight (test_autopilot.cpp),
// and one `glideslope_cli plan-speeds` asks of every speed it tries.
struct CrosswindFlown {
    // After `settle_s`:
    double least_sideslip_deg = 0.0;
    double most_sideslip_deg = 0.0;
    double worst_heading_deg = 0.0; // off north, either way
    double worst_height_ft = 0.0;
    double slowest_kts = 0.0;
    // From the start:
    double most_sideslip_ever_deg = 0.0;

    // Its sideslip within a degree and its heading within two.
    bool held() const {
        return -least_sideslip_deg <= 1.0 && most_sideslip_deg <= 1.0 &&
               worst_heading_deg <= 2.0;
    }
};
CrosswindFlown fly_heading_in_crosswind(const std::filesystem::path& data,
                                        const CatalogueEntry& entry, double airspeed_kts,
                                        bool windy, double settle_s = 30.0);

// Flies `entry` round the tightest orbit allowed at `trial.airspeed_kts`, at
// 3,000 ft over Sydney, clean, with the throttle it starts at in the
// catalogue. `data` is the data directory (with jsbsim/ in it). Thirty
// minutes at most.
OrbitFlown fly_tightest_orbit(const std::filesystem::path& data, const CatalogueEntry& entry,
                              const OrbitTrial& trial);

// **The fastest `entry` flies level at full throttle**, KCAS: at 3,000 ft
// over Sydney in calm air, heading north, from `from_kts`, the autopilot
// holding the height and asked for far more speed than it has, until the
// speed settles - its average over thirty seconds moving less than 0.1 kt
// from the thirty before - or fifteen minutes have gone.
double full_throttle_level_kts(const std::filesystem::path& data, const CatalogueEntry& entry,
                               double from_kts);

// What a plan's speed must leave in hand at full throttle (below).
inline constexpr double plan_speed_power_margin_kts = 5.0;

// **Whether `entry` holds `airspeed_kts` as a plan may ask it**: round the
// tightest orbit at it both ways, in calm air and in a 10 kt wind, its
// height within 50 ft and its speed within 5 kt (each stopped once lost);
// a heading held in calm air and a 20 kt crosswind; and **power in hand**:
// its full-throttle level speed at least 5 kt above it. Without that margin
// a speed held 5 kt short counted as held - the C182 at 144 kt, which makes
// 143 at full throttle, and on Windows, its speed sagging to 139 round its
// circle, swung 150 m either side of it. Stops at the first that does not
// hold. `said`, if given, is told each flight in a line. What `glideslope_cli
// plan-speeds` asks of every speed it tries.
bool holds_plan_speed(const std::filesystem::path& data, const CatalogueEntry& entry,
                      double airspeed_kts,
                      const std::function<void(const std::string&)>& said = {});

} // namespace glideslope::sim
