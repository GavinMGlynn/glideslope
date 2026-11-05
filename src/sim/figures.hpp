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
#include <optional>
#include <string>
#include <utility>
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

// **One weight's centre-of-gravity limits**, inches along her model's own x
// axis (JSBSim's structural frame, aft positive), forward and aft.
struct CgLimit {
    double lbs = 0.0;
    double forward_in = 0.0;
    double aft_in = 0.0;
};

// **Her weight and centre-of-gravity envelope, as her books publish it**
// (`<cg_limits>`): its limits by weight, lightest first, the lightest the
// envelope is drawn to and the heaviest her greatest take-off weight, read
// in her source's units - inches from its datum, put on her model's frame by
// `offset_in`, or per cent of her mean chord from its leading edge
// (`lemac_in`, `mac_in`, on her model's frame) - and held here in her
// model's inches. Or none, and `unpublished` says why: an envelope no public
// document gives.
struct CgLimits {
    std::vector<CgLimit> limits;
    std::string unpublished;
    std::string source;
};

// **Whether `lbs` at `cg_in`, her model's inches, lies inside `limits`**:
// at a weight from the lightest to the heaviest given, between the forward
// and aft limits taken straight between the weights either side. False for
// an envelope unpublished, and for any weight outside it.
bool within_cg_limits(const CgLimits& limits, double lbs, double cg_in);

// What was on board for a figure, and what it all weighed.
struct FigureLoading {
    double total_lbs = 0.0;
    Loading loading;
};

struct PublishedFigures {
    std::string model;
    std::string source;
    double flaps_full_deg = 0.0; // the flaps' travel at a command of 1; 0, none
    // **The flap the stall recovery raises the flaps to**, degrees
    // (`go_around_flaps_deg`), at a hand's pace and never lower
    // (sim/autopilot.cpp): a climb-out or go-around setting from the
    // aeroplane's handbooks, which the file cites. None: the recovery leaves
    // the flaps where they are.
    std::optional<double> go_around_flaps_deg;
    // **Where her own stall warning begins**, knots above her stall
    // (`stall_warning_kts`): the buffet or the horn her handbooks give, which
    // the AI's notice and her plan's slowest keep, where it is more than the
    // rule's 5 knots or 5% (sim::stall_warning_kts). 0, none given.
    double stall_warning_kts = 0.0;
    // The speedbrake lever the aeroplane is flown down an approach with, 0 to
    // 1: 0 for all but one that has nothing else to slow it - a B-2A, with
    // no flap, lands with its drag rudders open.
    double approach_speedbrake = 0.0;
    // The sink, feet a minute, the approach autopilot's flare brings the
    // wheels to the runway at (`touchdown_fpm`); 0 where the file gives
    // none, and `approach_speeds` then uses a light aeroplane's forty.
    double touchdown_fpm = 0.0;
    // **The rate the take-off autopilot holds her rotation to on her
    // wheels**, degrees of pitch a second (`rotation_degps`, sim::Departure):
    // her flight manual's or her class's, which the file cites. 0 where the
    // file gives none, and her rotation is then not held.
    double rotation_degps = 0.0;
    // **The stick the rotation's rate loop asks for each degree a second
    // she is off that rate** (`rotation_stick_per_degps`, sim::Departure):
    // her elevator's authority over her pitch rate on her wheels, so that
    // the loop neither lags nor rings. 0 where the file gives none, and the
    // loop's own default (0.3, the airliners') is used.
    double rotation_stick_per_degps = 0.0;
    // **Her elevator's pitching moment coefficient at full travel**, nose up
    // or down (`elevator_full_cm`), from her flight model's own table: with
    // her dynamic pressure, wing and inertia, the pitch acceleration a full
    // stick gives her, which the lander's hold of a hull's trim on the step
    // is damped by (sim::Lander). 0 where the file gives none.
    double elevator_full_cm = 0.0;
    // **Where the roll-out's stick starts back**, a fraction of her reference
    // speed (`stick_back_from_vref`): her tail held up, or her nose wheel
    // light, from there down to half of it, where the stick is fully back
    // (sim::Lander). 0.9 where the file gives none.
    double stick_back_from_vref = 0.9;
    // **Her pedals command a yaw rate**, through her own flight controls
    // (`yaw_rate_pedal_per_degps`): the pedal the lander's approach yaw
    // damper asks for each degree a second of yaw her bank does not account
    // for (sim::Lander). 0 where the file gives none: her pedals move her
    // rudder, and the lander's own 0.05 is flown.
    double yaw_rate_pedal_per_degps = 0.0;
    // **What her manufacturer adds to her approach speed in gusts**: the
    // share of the gust factor (`gust_additive_share`) and the most it
    // comes to (`most_gust_additive_kts`), which her file cites. 0 where the
    // file gives none, and the FAA's half of it, up to 15 kt, is flown
    // (sim::Lander::approach_kts).
    double gust_additive_share = 0.0;
    double most_gust_additive_kts = 0.0;
    // **Her aileron for each degree of bank off what is wanted, in gusts**
    // (`gusty_aileron_per_deg`), where her file gives one: a wide wing rocked
    // by turbulence's rolling gusts. 0 where the file gives none, and the
    // lander's own 0.035 is flown (sim::Lander).
    double gusty_aileron_per_deg = 0.0;
    // **The nose-up rate her gusty flare lets through while she closes with
    // the runway faster than it wants**, degrees a second
    // (`gusty_closing_nose_degps`), where her file gives one. 0 where it gives
    // none: a jet keeps the degree a second, a propeller aeroplane her own
    // (sim::Lander).
    double gusty_closing_nose_degps = 0.0;
    // **The height over her wheels her maker has her flare's power closed
    // at** (`gusty_retard_ft`), feet, where her file gives one: in gusts too,
    // where the lander otherwise holds it to the touch. 0 where it gives
    // none (sim::Lander).
    double gusty_retard_ft = 0.0;
    // **How far under her path's attitude her nose may be put for the path
    // in the last of a gusty approach** (`gusty_most_nose_down_deg`),
    // degrees, where her file gives it. 0 where it gives none: as far as
    // the path asks (sim::Lander).
    double gusty_most_nose_down_deg = 0.0;
    // **The speed her approach's pitch gains are right at**
    // (`pitch_gains_kcas`), knots, where her file gives one: flown slower,
    // her elevator law is scaled up by the dynamic pressure she has lost,
    // (this over her approach speed) squared (sim::Lander). 0 where it gives
    // none: the gains are flown as they are at every speed.
    double pitch_gains_kcas = 0.0;
    // **How far ahead her calm flare judges her sink** (`flare_lead_s`),
    // seconds, where her file gives one. 0 where it gives none: the
    // lander's own three tenths (sim::Lander).
    double flare_lead_s = 0.0;
    // **Her stick on the ground commands a pitch rate**
    // (`roll_out_stick_per_degps`): the stick the roll-out's nose law asks
    // for each degree a second she is off the rate it wants (sim::Lander,
    // `lower_the_nose`). 0 where the file gives none: the attitude law.
    double roll_out_stick_per_degps = 0.0;
    // **Her rudder on the roll for each degree her track is off the one
    // wanted** (`roll_out_track_rudder_per_deg`), where her file gives one:
    // the roll-out's steering by her track (sim::Lander). 0 where it gives
    // none, and the lander's own 0.5 is flown.
    double roll_out_track_rudder_per_deg = 0.0;
    // **The share of the roll-out's yaw-rate damping she is given**
    // (`roll_out_yaw_damping_share`), where her file gives one (sim::Lander).
    // 0 where it gives none, and all of it is flown.
    double roll_out_yaw_damping_share = 0.0;
    // **How a tail-wheel aeroplane is landed** (`lands`): "three_point", held
    // off until she touches at the attitude she stands at (sim::Lander), or
    // "wheels", the default, flown on to her main wheels.
    bool three_point = false;
    // **The crosswind her handbook says she was demonstrated in**, knots
    // (`demonstrated_crosswind_kts`), where her file gives one: the most her
    // learnt landing's gate admits across, if under training's
    // (sim::learnt_gate_most_crosswind_kts). 0 where it gives none.
    double demonstrated_crosswind_kts = 0.0;
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
    // **The flap that approach speed is for**, degrees, where the
    // `<approach>` gives one (`flaps_deg`); -1 where it does not. An
    // aeroplane that publishes an approach speed and no stall speed - the
    // 747-400 - is landed with it.
    double approach_flaps_deg = -1.0;
    // **The least speed each flap may be flown at as it comes up after a
    // take-off**, knots over her approach speed for her weight - her
    // `<approach>`, or the one worked from her landing stall - by flap
    // degrees: `<flap_manoeuvre_speeds>`, where the file gives them (the
    // 747-400's, over her Vref30; the A380's, Airbus's F and S speeds over
    // the approach speed worked from her stall). Empty where it gives none: her flaps then
    // come up as every aeroplane's do (sim::Departure).
    std::vector<std::pair<double, double>> flap_manoeuvre_over_approach_kts;
    // **The lift-off speed worked from its flight manual's**, KCAS, where the file
    // has a `<takeoff kcas="..." loading="...">`: what `departure_speeds`
    // takes off at, in place of 1.15 times a stall that, at the F-15C's 40
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
    // **And the weight they were measured at**, pounds: `weight_lbs`, her
    // model's own - the trial loads nothing, and a server flies her at it.
    // Heavier, her slowest is raised for her weight (sim::for_weight).
    double plan_weight_lbs = 0.0;
    // **The slowest a glide may be flown at**, KCAS: `<glide_speeds
    // slowest_kcas="...">`, which every file gives. Measured, not worked out
    // (`glideslope_cli glide-speeds`, sim::glide_tightest_orbit): the slowest,
    // from the slowest a route may fly it up in 5 kt steps, at which it
    // glides round its tightest orbit from 30,000 ft, its engines stopped,
    // both ways round, without stalling.
    double glide_slowest_kcas = 0.0;
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
    // **The runway it needs to land**, metres: `<landing_distance ft="..."
    // basis="..." factor="...">`, its published landing distance times the
    // margin the file states for its class - a light aeroplane's handbook
    // distance over a 50 ft obstacle, or a transport's landing field length,
    // which is already the demonstrated distance over 0.6 (14 CFR
    // 121.195(b)) - or, where nothing published gives one, basis "measured":
    // the runway her model used, threshold to stop, landed by the approach
    // autopilot (`glideslope_cli land`), times its class's margin. 0 where the
    // file says, by `<no_landing_distance>`, why none applies: then no runway
    // is refused it for length. Every file says one or the other, once.
    double landing_need_m = 0.0;
    std::string landing_basis; // "over_50_ft_obstacle", "field_length", "measured"; empty: none
    // **The landing distance itself, unfactored**, metres: `<landing_distance>`'s
    // figure with no margin - over a 50 ft obstacle, or measured, as given;
    // a landing field length times 0.6, the demonstrated distance it was
    // made from (14 CFR 121.195(b)). What AC 91-79B's landing distance
    // factors multiply (world::landing_distance_factor). 0 where none.
    double landing_distance_m = 0.0;
    // **The runway it needs to land on a wet runway, from its own figure**,
    // metres: `<wet_landing_distance ft="..." basis="..." factor="...">`, read
    // as `<landing_distance>` is, where its manual publishes one. 0 where it
    // does not: then the wet need is the dry one times 14 CFR 121.195(d)'s
    // 1.15 (`wet_landing_need_m`, below).
    double own_wet_landing_need_m = 0.0;
    // **Her weight and centre-of-gravity envelope** (`<cg_limits>`, above),
    // or why none is published: every file says one or the other, once.
    CgLimits cg_limits;
};

struct FigureResult {
    const FigureSpec* spec = nullptr;
    double measured = 0.0;

    bool passed() const {
        return measured >= spec->low && measured <= spec->high;
    }
};

// The slowest and fastest a plan may fly `model`, KCAS, and the weight they
// were measured at: its figures file's `<plan_speeds>`, read from
// `data`/figures. For another weight, sim::for_weight. Throws as
// `read_published_figures` does.
PlanSpeeds plan_speeds(const std::filesystem::path& data, const std::string& model);

// The slowest a glide may fly `model`, KCAS: its figures file's
// `<glide_speeds>`. Throws as `read_published_figures` does.
double glide_slowest_kts(const std::filesystem::path& data, const std::string& model);

// The runway `model` needs to land, metres (`PublishedFigures::landing_need_m`),
// or 0 where nothing published gives it. Throws as `read_published_figures`
// does.
double landing_need_m(const std::filesystem::path& data, const std::string& model);

// `model`'s unfactored dry landing distance, metres
// (`PublishedFigures::landing_distance_m`), or 0 where nothing published gives
// it. Throws as `read_published_figures` does.
double landing_distance_m(const std::filesystem::path& data, const std::string& model);

// The runway `model` needs to land on a wet runway, metres: her figures'
// own wet landing distance where they give one, and otherwise her dry need
// times sim::wet_landing_factor (0 where she has no dry need either). Throws
// as `read_published_figures` does.
double wet_landing_need_m(const std::filesystem::path& data, const std::string& model);

// **A plan file is held to the aircraft it flies, and its speeds, as it is
// read**, as a model's plan is (copilot/planner.cpp): throws FlightPlanError
// for a plan for another aircraft than `flown` (a catalogue id) - a flight
// flies the aircraft it was given, not the plan's - and for a start or a
// waypoint slower or faster than its aircraft's `<plan_speeds>`, naming it,
// and for a `loading` her figures file does not name, naming it and those
// it does.
// Its aircraft is looked up in `data`'s catalogue, and CatalogueError is
// thrown for one it does not hold. The server, the client and
// `glideslope_cli fly-plan` each call it on the plan file they read.
void refuse_what_it_cannot_fly(const std::filesystem::path& data, const FlightPlan& plan,
                               const std::string& flown);

// **A loading `model`'s figures file names, by `name`**: what is on board
// and what it all weighs - what a server gives a player who asks for it, and
// what her prediction is loaded as (net::AskedAircraft). None where she has
// no figures file, or it names no such loading. Throws as
// `read_published_figures` does for a file that is there and does not read.
std::optional<FigureLoading> named_loading(const std::filesystem::path& data,
                                           const std::string& model, const std::string& name);

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
