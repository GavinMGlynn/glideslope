#pragma once

// **The copilot flies with you** (REQUIREMENTS.md section 5, the third layer
// kept in the loop): a language model told, every so often and whenever
// something happens, where the aircraft is and what it is doing, and asked
// whether the plan should change - pilot in command of the autopilot, as a
// pilot is.
//
// **The model plans; the controllers fly.** What it may answer is `keep`, or
// a new route: waypoints and orbits, flown from where the aircraft is, and
// `glide AIRSPEED_KT` - the route flown at that airspeed, held by the vertical
// speed, for an engine that has stopped (sim::Controller::set_glide). Nothing else: its answer is read as a flight plan
// (sim/plan.hpp) and checked, and one that fails is told back to it, up to
// `most_attempts` answers in all, as the planner's are (copilot/planner.hpp).
// A change is checked against the flight as the model was told it:
//   - every waypoint within 200 km of the aircraft, every airspeed between
//     its approach speed and a fifth over its cruise, and every height at
//     least 500 ft above both the sea and the ground beneath the aircraft;
//   - `glide` only with the engine stopped, at an airspeed from the approach
//     speed to the best climb; and with the engine stopped, only a glide. A
//     glide flies neither its waypoints' heights nor their airspeeds, so
//     neither is checked, but its orbits must be wide enough for its airspeed.
//
// **It never slows the step.** A question is asked on a thread of its own,
// and the answer, checked there, is picked up by whoever steps the
// simulation between two steps, when it has come: `ask` and `answered` never
// wait on the model. A question outstanding is not asked again.
//
// **Opt-in, with the player's key**: a Copilot is made from a Provider, which
// is refused without a key (copilot/provider.hpp) - never faked.

#include "copilot/provider.hpp"
#include "sim/plan.hpp"
#include "world/runways.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::copilot {

// **What a route may hold to be sent to a server** (`COPILOT_ROUTE`,
// net/messages.hpp, which this cannot see and which holds these same limits -
// frontend/players_copilot.cpp asserts it): so many waypoints, each named in
// so many letters, digits and underscores, round an orbit so many times.
// An answer past them is refused back to the model, not sent to be dropped.
inline constexpr std::size_t most_route_waypoints = 12;
inline constexpr std::size_t most_waypoint_name_bytes = 32;
inline constexpr int most_orbit_turns = 255;

// What the copilot is told once: the aircraft, and what the pilot asked.
struct Brief {
    std::string aircraft;      // its catalogue id, "c172p"
    std::string aircraft_name; // "Cessna 172P Skyhawk"
    // In whole knots, as PlanRequest's (planner.hpp); 0 for an aircraft that
    // has none - the 747-400 and the F-22A publish no stall speed - and is
    // then told none.
    double approach_kts = 0.0;
    // The slowest and fastest a route may fly it under power
    // (sim::plan_speeds), measured: a route is flown clean, and the approach
    // speed is a flaps-down figure. Where not given (0), the approach speed
    // and a fifth over the cruise; the slowest is never below the approach.
    double slowest_kts = 0.0;
    double fastest_kts = 0.0;
    double climb_kts = 0.0;
    double cruise_kts = 0.0;
    // The slowest a glide may fly it (sim::glide_slowest_kts), measured; 0
    // where not given.
    double glide_slowest_kts = 0.0;
    std::string task; // what the pilot said, "follow the coast north to Palm Beach"
};

// The slowest a route may fly the aircraft under power: its slowest, or its
// approach speed where that is more.
inline double slowest_routed_kts(const Brief& b) {
    return b.slowest_kts > b.approach_kts ? b.slowest_kts : b.approach_kts;
}

// The fastest a route may fly it: its fastest, or a fifth over its cruise
// where none is given.
inline double fastest_routed_kts(const Brief& b) {
    return b.fastest_kts > 0.0 ? b.fastest_kts : b.cruise_kts * 1.2;
}

// **The airspeeds a glide may be flown at**: from the slowest a route may fly
// it - its approach speed, or the slowest it holds its tightest orbit at
// clean where that is more - or the slowest it glides at without stalling
// where that is more still, to its best climb, or that slowest where the
// best climb is less. A glide is flown clean, and a jet's approach speed is a
// flaps-down figure: glided round its tightest orbit clean at it from 15,000
// ft, the 737-300 at 137 kt went past its lift's peak to 59 degrees of alpha,
// and stalled at every speed up to 162. Nor is the slowest a route may fly
// always enough: from 30,000 ft, engines stopped, the 747-400, 787-8, A320,
// F-15C and Mosquito stalled gliding there, and each figures file gives the
// slowest it glides at, measured (`glideslope_cli glide-speeds`).
struct GlideSpeeds {
    double slowest_kts = 0.0;
    double fastest_kts = 0.0;
};
inline GlideSpeeds glide_speeds(const Brief& b) {
    const double routed = slowest_routed_kts(b);
    const double slowest = b.glide_slowest_kts > routed ? b.glide_slowest_kts : routed;
    return {slowest, b.climb_kts > slowest ? b.climb_kts : slowest};
}

// What it is told each time it is asked: the flight as it is now.
struct Situation {
    double seconds = 0.0; // since the copilot was engaged
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double altitude_ft = 0.0; // above sea level
    double ground_ft = 0.0;   // the ground beneath, above sea level
    double heading_deg = 0.0; // true
    double airspeed_kts = 0.0;
    double vertical_speed_fpm = 0.0;
    bool engine_running = true;
    std::optional<double> gliding_kts; // the glide being flown, if one is
    // The waypoints still to fly, the one being flown to first; none when
    // the autopilot only holds what the aircraft is doing.
    std::vector<sim::Waypoint> route;
    // Runways nearby, nearest first: where it may land.
    std::vector<world::RunwayEnd> fields;
    // Why it is asked now: "the pilot has asked", "the engine has stopped",
    // "a routine look".
    std::string event;
};

// What the copilot decided: to keep what is flown, or to fly `plan` - its
// waypoints, from where the aircraft is - gliding at `glide_kts` if set.
struct Change {
    bool keep = true;
    sim::FlightPlan plan;
    std::optional<double> glide_kts;
    std::string text; // the answer as the model wrote it, less any fences
    int attempts = 0;
    std::vector<std::string> refused; // why each answer before the last was
};

// What the model is told: what it is for, and how it answers.
std::string copilot_instructions();
// What it is asked each time.
std::string situation_text(const Brief& brief, const Situation& now);

// The answer read as a change, or throws sim::FlightPlanError saying why it
// cannot be read.
Change read_change(const Brief& brief, const Situation& now, const std::string& answer);
// Why `change` may not be flown now, or empty if it may.
std::string change_refusal(const Brief& brief, const Situation& now, const Change& change);

// Asks `provider` what to do now, and checks what it says: waits for the
// model, so it is for a thread of its own. Throws ProviderError when the
// provider fails, or when every answer is refused, saying why each was.
// `given_up`, when it becomes true, ends it between answers with a
// ProviderError.
Change decide(Provider& provider, const Brief& brief, const Situation& now,
              const std::atomic<bool>* given_up = nullptr);

class Copilot {
public:
    Copilot(std::unique_ptr<Provider> provider, Brief brief);
    // **Gives up a question still outstanding, and waits for that**: the
    // model's request is abandoned at once and no further answer is asked
    // for, so a player quitting is not held up by a model thinking.
    ~Copilot();
    Copilot(const Copilot&) = delete;
    Copilot& operator=(const Copilot&) = delete;

    const Brief& brief() const {
        return brief_;
    }
    Provider& provider() {
        return *provider_;
    }

    // Asks, on a thread of its own, what to do now. Returns at once; false,
    // asking nothing, while an earlier question is still outstanding.
    bool ask(Situation now);
    // Or with what it is told worked out there too, on its own thread, by
    // `told` - for a situation whose ground has to be looked up, which is
    // not for the thread that steps.
    bool ask(std::function<Situation()> told);
    // Whether a question is outstanding: asked, and its answer not yet
    // taken by `answered`.
    bool asking() const {
        return pending_.valid();
    }
    // **Gives up the question outstanding without waiting for it**: its
    // model's request is abandoned and no further answer asked for. What
    // is still running is waited for by the destructor - so whoever must
    // not wait keeps this until `settled`.
    void give_up() {
        given_up_ = true;
    }
    // Whether nothing it asked is still running: destroying it now waits
    // for nothing.
    bool settled() const {
        return !pending_.valid() ||
               pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    // The answer to the question outstanding, once and only once it has
    // come; nothing while it has not. Never waits. Throws ProviderError from
    // the question, taking it, when the model could not be asked or every
    // answer was refused.
    std::optional<Change> answered();

private:
    std::unique_ptr<Provider> provider_;
    Brief brief_;
    std::future<Change> pending_;
    std::atomic<bool> given_up_{false};
};

} // namespace glideslope::copilot
