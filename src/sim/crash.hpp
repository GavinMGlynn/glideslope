#pragma once

// **When an aircraft has crashed**, which the server decides (REQUIREMENTS.md
// 6.4: collisions are resolved on the server, and its result wins).
//
// Two kinds, each a rule and not a tuning:
//
// - **The ground.** An aircraft touches the ground sinking faster than 10 ft/s,
//   which is the descent landing gear is certified to take - 14 CFR 25.473's
//   limit descent velocity for a transport aeroplane, and the most the old
//   23.473 formula, 4.4 (W/S)^1/4 ft/s held between 7 and 10, gave a light
//   one - or any part of its airframe that is not a wheel
//   touches it, or a landplane comes down on water. A flying boat's hull on
//   water is where it belongs, and is none of these.
// - **Each other.** Two aircraft whose centres come closer than the mean of
//   their wingspans: a sphere of half its span around each, which is the size
//   the flight model gives it (`metrics/bw-ft`), and which at 120 steps a
//   second two jets closing at 800 knots cannot pass through between steps.
//
// A wreck is the caller's business: the server stops it where it hit, tells
// every client, and starts it again a few seconds later.

#include "sim/aircraft.hpp"

#include <optional>
#include <string>

namespace glideslope::sim {

// The descent the gear takes, in feet a second.
inline constexpr double gear_takes_fps = 10.0;

// Watches one aircraft, step by step, for a crash into the ground.
class GroundJudge {
public:
    // `alights_on_water`: a flying boat or a floatplane, whose hull or floats
    // on water are not a crash.
    explicit GroundJudge(bool alights_on_water) : water_is_home_(alights_on_water) {}

    // After each step. What happened, if this step wrecked it; nothing if
    // not. The first call only learns what is touching already - an aircraft
    // started on the ground has not hit it.
    std::optional<std::string> judge(const Aircraft& aircraft);

    // Forget what was touching, as for an aircraft started again.
    void reset() { started_ = false; }

private:
    bool water_is_home_;
    bool started_ = false;
    bool touching_ = false;
    double sink_fps_ = 0.0; // the last step's, before this one's contact
};

// Whether two aircraft, at these Earth-centred positions in metres and with
// these wingspans in feet, have collided.
bool collided(const double a_m[3], double a_span_ft, const double b_m[3], double b_span_ft);

} // namespace glideslope::sim
