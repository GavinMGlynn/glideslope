#pragma once

// **The tightest orbit a plan may ask, flown**: an aircraft started in the
// air 2 km outside the circle, heading for its centre, flown by the
// navigator and the autopilot twice round and on - the flight the orbit
// tests make, and the one `glideslope_cli plan-speeds` sweeps to measure the
// slowest and fastest each aircraft may be planned at (its `<plan_speeds>`,
// in its figures file).

#include <filesystem>
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
    // From the first quarter-turn on:
    double nearest_m = 0.0;     // from the centre
    double farthest_m = 0.0;
    double worst_height_ft = 0.0; // off the orbit's height, either way
    double slowest_kts = 0.0;     // calibrated
    double fastest_kts = 0.0;

    // Its height within 50 ft and its speed within 5 kt of what was asked.
    bool held(double asked_kts) const {
        return passed_on && worst_height_ft <= 50.0 && slowest_kts >= asked_kts - 5.0 &&
               fastest_kts <= asked_kts + 5.0;
    }
};

// Flies `entry` round the tightest orbit allowed at `trial.airspeed_kts`, at
// 3,000 ft over Sydney, clean, with the throttle it starts at in the
// catalogue. `data` is the data directory (with jsbsim/ in it). Thirty
// minutes at most.
OrbitFlown fly_tightest_orbit(const std::filesystem::path& data, const CatalogueEntry& entry,
                              const OrbitTrial& trial);

} // namespace glideslope::sim
