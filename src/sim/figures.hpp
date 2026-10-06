#pragma once

// Published figures: what an aircraft's handbook says it does, and flights that
// check the flight model does it.
//
// **A figure may instead be measured from the model**, for an aeroplane whose
// handbook is not public and publishes nothing - ten of the sixteen in the
// roster. Such a figure carries `from="measured"`, and what it asserts is that
// the model still does what it did when the number was taken, not that the
// aeroplane does it. `docs/ASSETS.md` records which aeroplanes those are.
//
// An aircraft's figures file (assets/figures/<model>.xml) names each figure, the
// range its measurement must land in, where the number comes from, the
// conditions it was measured in and the loading it was measured at. Each
// figure's flight - its `flight` attribute, or its name if it has none - picks
// a flight in figures.cpp that flies those conditions with a simple test pilot
// and measures the one number.

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "sim/aircraft.hpp"
#include "sim/plan.hpp"

namespace glideslope::sim {

struct FigureSpec {
    std::string name;   // unique in its file
    std::string flight; // the flight that measures it
    std::string loading; // the loading it was measured at, by name
    std::string unit;
    std::string source;     // the file's words for where the number is from
    // **Where the number came from.** `from="published"`, the default, means a
    // handbook says it and the flight checks the model against the handbook.
    // `from="measured"` means no handbook gives it and the number is what this
    // project's own flight model does, measured once and written down - so the
    // flight is not checking the model against the world, it is checking that
    // the model still does what it did. The two are not the same claim and a
    // figure says which it is making.
    bool measured = false;
    double published = 0.0; // the handbook's number, or the middle of its range
    double low = 0.0;       // the measurement must land in [low, high]
    double high = 0.0;
    std::map<std::string, double> conditions; // speeds, flap settings, altitudes
};

// What was on board for a figure, and what it all weighed.
struct FigureLoading {
    double total_lbs = 0.0;
    Loading loading;
};

struct PublishedFigures {
    std::string model;
    std::string source;
    double flaps_full_deg = 0.0; // the flaps' travel at a command of 1; 0, none
    // The speedbrake lever the aeroplane is flown down an approach with, 0 to
    // 1: 0 for all but one that has nothing else to slow it - a B-2A, with
    // no flap, lands with its drag rudders open.
    double approach_speedbrake = 0.0;
    // The sink, feet a minute, the approach autopilot's flare brings the
    // wheels to the runway at (`touchdown_fpm`); 0 where the file gives
    // none, and `approach_speeds` then uses a light aeroplane's forty.
    double touchdown_fpm = 0.0;
    // The file's first loading, the one a figure naming none is flown at.
    double total_lbs = 0.0;
    Loading loading;
    std::map<std::string, FigureLoading> loadings; // every loading, by name
    std::string first_loading;                     // the first's name
    std::vector<FigureSpec> figures;
    // **The final approach speed its flight manual gives**, KCAS, where the
    // file has an `<approach kcas="..." loading="...">`: what
    // `approach_speeds` flies the approach at in place of 1.3 times the
    // stall. A fighter's manual gives the approach by angle of attack and
    // weight, not as a margin over a stall it does not publish. 0 where the
    // file gives none; `approach_loading` names the weight it is for.
    double approach_kcas = 0.0;
    std::string approach_loading;
    // **The lift-off speed worked from its flight manual's**, KCAS, where the file
    // has a `<takeoff kcas="..." loading="...">`: what `departure_speeds`
    // takes off at, in place of 1.15 times a stall that, at the F-15C's 32
    // degrees of alpha, cannot be reached on a runway. 0 where it gives none.
    double takeoff_kcas = 0.0;
    std::string takeoff_loading;
    // **The slowest and fastest a plan may fly it**, KCAS:
    // `<plan_speeds slowest_kcas="..." fastest_kcas="...">`, which every file
    // gives. Measured, not worked out (`glideslope_cli plan-speeds`,
    // sim/orbit_trial.hpp): speeds at which the autopilot holds height and
    // speed round the tightest orbit a plan may ask, clean, both ways round,
    // in calm air and in wind. The approach speed is a flaps-down figure and
    // a plan is flown clean, so the slowest is the approach speed only where
    // that is held, and never below it.
    double plan_slowest_kcas = 0.0;
    double plan_fastest_kcas = 0.0;
    // **Take-off speeds measured from its model, not published**:
    // `<takeoff_speeds rotate_kcas="..." climb_kcas="..." flaps_deg="..."
    // weight_lbs="...">`, for an aircraft whose published figures give no
    // speed to rotate or climb away at (the 747-400, the F-22A). Found by
    // `glideslope_cli takeoff-speeds` (sim/takeoff_trial.hpp) at the weight
    // its model flies a plan at; 0 where the file gives none.
    double measured_rotate_kcas = 0.0;
    double measured_climb_kcas = 0.0;
    double measured_takeoff_flaps_deg = 0.0;
    double measured_takeoff_lbs = 0.0;
};

struct FigureResult {
    const FigureSpec* spec = nullptr;
    double measured = 0.0;

    bool passed() const {
        return measured >= spec->low && measured <= spec->high;
    }
};

// The slowest and fastest a plan may fly `model`, KCAS: its figures file's
// `<plan_speeds>`, read from `data`/figures. Throws as
// `read_published_figures` does.
PlanSpeeds plan_speeds(const std::filesystem::path& data, const std::string& model);

// **A plan file is held to the aircraft it flies, and its speeds, as it is
// read**, as a model's plan is (copilot/planner.cpp): throws FlightPlanError
// for a plan for another aircraft than `flown` (a catalogue id) - a flight
// flies the aircraft it was given, not the plan's - and for a start or a
// waypoint slower or faster than its aircraft's `<plan_speeds>`, naming it.
// Its aircraft is looked up in `data`'s catalogue, and CatalogueError is
// thrown for one it does not hold. The server, the client and
// `glideslope_cli fly-plan` each call it on the plan file they read.
void refuse_what_it_cannot_fly(const std::filesystem::path& data, const FlightPlan& plan,
                               const std::string& flown);

// Reads an aircraft's figures file. Throws std::runtime_error if it cannot be
// read, names a flight that does not exist or a loading it does not have, or
// leaves out a range.
PublishedFigures read_published_figures(const std::filesystem::path& file);

// The name of every flight a figure can be measured by.
std::vector<std::string> known_figures();

// Flies the flight for `spec` with the aircraft loaded as `figures` says, and
// measures its number. `jsbsim_root` is where the model's files are. Throws
// std::runtime_error if the flight could not be flown as specified - the loading
// does not come to the stated weight, say, or the throttle ran out.
FigureResult fly_figure(const std::filesystem::path& jsbsim_root,
                        const PublishedFigures& figures, const FigureSpec& spec);

} // namespace glideslope::sim
