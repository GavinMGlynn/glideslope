#pragma once

// **Words to a flight plan** (REQUIREMENTS.md section 5, the third layer):
// "take off, climb to 3,000 ft and orbit the CBD", said of an aeroplane
// standing at an airport, becomes a plan the navigator flies.
//
// **The model plans; the controllers fly.** What it answers is read as a
// flight plan (sim/navigator.hpp) and checked before it is used, and a plan
// that fails is refused - told back to the model, which may try again, up to
// `most_attempts` answers in all:
//   - it must read as a plan, for this aircraft;
//   - it must take off, from one of the airport's runways as they were given
//     to it, line for line - where a runway is comes from OurAirports' data
//     (world/runways.hpp), never from a model's memory;
//   - every airspeed between the aircraft's approach speed and a fifth over
//     its cruise, every height at least 500 ft above the runway, and every
//     waypoint within 200 km of it;
//   - no orbit tighter than the aircraft turns at its airspeed, which the
//     plan's own reading refuses (sim::least_orbit_radius_m).
// Where places are - "the CBD" - is the model's to know. That is what it is
// for.

#include "copilot/provider.hpp"
#include "sim/navigator.hpp"
#include "world/runways.hpp"

#include <string>
#include <vector>

namespace glideslope::copilot {

struct PlanRequest {
    std::string command;       // what the pilot said
    std::string aircraft;      // its catalogue id, "c172p"
    std::string aircraft_name; // "Cessna 172P Skyhawk"
    double approach_kts = 0.0; // its reference speed on the approach
    double climb_kts = 0.0;    // its best climb speed
    double cruise_kts = 0.0;   // a comfortable cruise
    std::string airport;       // where it stands, "YSSY"
    std::vector<world::RunwayEnd> runways; // that airport's
};

inline constexpr int most_attempts = 3;

struct Planned {
    std::string text; // the plan as the model wrote it, less any fences
    sim::FlightPlan plan;
    int attempts = 0;
    // Why each answer before the last was refused.
    std::vector<std::string> refused;
};

// What the model is told: how a plan is written and what it must hold.
std::string planning_instructions();
// And what is asked of it: the aircraft, where it stands, and the command.
std::string planning_request(const PlanRequest& request);
// A runway end as a plan's `runway` line.
std::string runway_line(const world::RunwayEnd& end);

// **Whether a plan may take off from `end`**: only where the file gives its
// elevation, which every height in the plan is checked against. An end with
// none is not offered to the model, and a plan naming it is refused.
bool plannable(const world::RunwayEnd& end);

// Why `plan` is not one this request may fly, or empty if it may.
std::string refusal(const PlanRequest& request, const sim::FlightPlan& plan);

// Asks `provider` for a plan, and checks it. Throws ProviderError when the
// provider fails, or when every answer is refused, saying why each was.
Planned plan_from_words(Provider& provider, const PlanRequest& request);

} // namespace glideslope::copilot
