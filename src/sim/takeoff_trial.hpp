#pragma once

// **A take-off, flown to see whether its speeds hold**: an aircraft at the
// weight its model flies a plan at, standing on the threshold of a level
// 3,500 m runway at sea level in calm air, taken off by the take-off autopilot
// (sim/departure.hpp) at the rotation and climb-away speeds given, to 500 ft.
// `glideslope_cli takeoff-speeds` sweeps it to measure the speeds an aircraft
// that publishes none takes off at (`<takeoff_speeds>` in its figures file),
// as `plan-speeds` sweeps the tightest orbit (sim/orbit_trial.hpp).

#include <filesystem>
#include <functional>
#include <string>

#include "sim/catalogue.hpp"
#include "sim/departure.hpp"

namespace glideslope::sim {

inline constexpr double takeoff_trial_runway_m = 3500.0;

struct TakeoffFlown {
    bool handed_over = false;   // reached 500 ft above the runway
    std::string wrecked;        // why, or empty
    double unstuck_m = 0.0;     // how far down the runway the wheels left it
    double unstuck_kts = 0.0;   // and the airspeed they left it at
    double slowest_airborne_kts = 0.0; // the least airspeed once off the ground
    double handed_over_kts = 0.0;      // the airspeed at 500 ft
    double weight_lbs = 0.0;           // what it weighed standing on the runway
    double seconds = 0.0;

    // **Whether the rotation held**: handed over unwrecked, off the ground
    // within the runway and within 15 kt of the rotation speed - lifted by
    // the rotation, not by the speed it ran on to - and never slower once off
    // it than 5 kt under the rotation speed.
    bool lifted_off(const DepartureSpeeds& speeds) const;
    // **Whether both held**: the rotation, and at 500 ft no slower than 10 kt
    // under the climb-away speed - the slowest it climbs away at, measured;
    // neither aeroplane measured can be held slower than its own.
    bool held(const DepartureSpeeds& speeds) const;
};

TakeoffFlown fly_takeoff_trial(const std::filesystem::path& data, const CatalogueEntry& entry,
                               const DepartureSpeeds& speeds);

// **The take-off speeds of an aircraft that publishes none, measured** -
// what `glideslope_cli takeoff-speeds` prints and `<takeoff_speeds>` holds -
// at its take-off field length's flap, or none, and the weight its model
// flies a plan at. The rotation is sought at speeds rising from `from_kts`
// by 5 kt, each climbing away at 20 more, until one lifts off by its
// rotation (TakeoffFlown::lifted_off), and written with 5 kt to spare,
// plan-speeds' rule; the climb away is the slowest speed at 500 ft of any
// asked from 10 to 100 kt over that, rounded up to 5 kt. Each trial is told
// to `say`, a line.
struct MeasuredTakeoff {
    double rotate_kts = 0.0;       // to write: the first that held, and 5
    double climb_kts = 0.0;        // to write: the slowest away, rounded up
    double slowest_away_kts = 0.0; // as flown
    double flaps_deg = 0.0;
    double weight_lbs = 0.0;
    std::string why_not;           // empty where both were found
};
MeasuredTakeoff measure_takeoff_speeds(const std::filesystem::path& data,
                                       const CatalogueEntry& entry, double from_kts,
                                       const std::function<void(const std::string&)>& say);

} // namespace glideslope::sim
