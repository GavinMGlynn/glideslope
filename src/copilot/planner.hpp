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
//     plan's own reading refuses (sim::least_orbit_radius_m);
//   - `land`, last, only on one of the runways it was told it may land on,
//     by an aircraft with an approach speed, and after no orbit flown for
//     ever: the checks a copilot's route's landing is held to
//     (copilot::landing_refusal). After its last waypoint the AI flies the
//     final approach and lands (sim/plan.hpp).
// Where places are - "the CBD" - is the model's to know. That is what it is
// for.

#include "copilot/provider.hpp"
#include "sim/plan.hpp"
#include "world/runways.hpp"

#include <string>
#include <vector>

namespace glideslope::copilot {

struct PlanRequest {
    std::string command;       // what the pilot said
    std::string aircraft;      // its catalogue id, "c172p"
    std::string aircraft_name; // "Cessna 172P Skyhawk"
    // Its reference speed on the approach, in whole knots - 0 for one that
    // has none, which is told none - as everything
    // said to the model and asked of a plan is: rounded once, where the
    // request is filled, so that the radius it is told and the one the plan
    // reader holds it to are for the same speed.
    double approach_kts = 0.0;
    // **The slowest and fastest a plan may fly it** (sim::plan_speeds),
    // measured: a plan is flown clean, and the approach speed is a
    // flaps-down figure. Where not given (0), the approach speed and a fifth
    // over the cruise; the slowest is never below the approach speed.
    double slowest_kts = 0.0;
    double fastest_kts = 0.0;
    double climb_kts = 0.0;    // its best climb speed
    double cruise_kts = 0.0;   // a comfortable cruise
    // The runway it needs to land, metres, as the copilot's Brief has it: 0
    // where nothing published gives one.
    double landing_need_m = 0.0;
    std::string airport;       // where it stands, "YSSY"
    std::vector<world::RunwayEnd> runways; // that airport's
    // **Where it may land**: runway ends near the airport - its own among
    // them - nearest first, any one of which the plan may end on (`land`).
    // None, and it is told of none, and a plan that lands is refused.
    std::vector<world::RunwayEnd> fields;
};

inline constexpr int most_attempts = 3;

// The slowest a plan may fly the aircraft: its slowest, or its approach
// speed where that is more.
inline double slowest_planned_kts(const PlanRequest& r) {
    return r.slowest_kts > r.approach_kts ? r.slowest_kts : r.approach_kts;
}

// The fastest a plan may fly it: its fastest, or a fifth over its cruise
// where none is given.
inline double fastest_planned_kts(const PlanRequest& r) {
    return r.fastest_kts > 0.0 ? r.fastest_kts : r.cruise_kts * 1.2;
}

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
// The answer less any Markdown fence a model puts round it.
std::string unfenced(const std::string& answer);

// A runway end as a plan's `runway` line.
std::string runway_line(const world::RunwayEnd& end);

// **Whether a plan may take off from `end`**: only where the file gives its
// elevation, which every height in the plan is checked against. An end with
// none is not offered to the model, and a plan naming it is refused.
bool plannable(const world::RunwayEnd& end);

// Why `plan` is not one this request may fly, or empty if it may.
std::string refusal(const PlanRequest& request, const sim::FlightPlan& plan);

// **A task to plan**, as a server gives it to the model planning an AI
// aircraft: which aircraft, the airport it stands at, and the words. Read
// from a file (content is data), one `KEY value` a line, `#` a comment:
//
//   aircraft c172p
//   airport YSSY
//   task take off, climb to 3,000 ft and orbit the CBD
struct Task {
    std::string aircraft;
    std::string airport;
    std::string command;
};

// Throws ProviderError for a line it does not know, a key given twice, a key
// followed by a tab rather than a space, or a task missing any of the three.
Task parse_task(const std::string& text);

// Asks `provider` for a plan, and checks it. Throws ProviderError when the
// provider fails, or when every answer is refused, saying why each was.
Planned plan_from_words(Provider& provider, const PlanRequest& request);

} // namespace glideslope::copilot
