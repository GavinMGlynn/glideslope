#pragma once

// The flight screen: one aircraft, standing on or flying over the DEM, seen
// from its cockpit, with the HUD.

#include "gfx/aircraft.hpp"
#include "gfx/hud.hpp"
#include "gfx/scene.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/checklist_run.hpp"
#include "sim/controller.hpp"
#include "sim/navigator.hpp"
#include "gfx/sky.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/geoid.hpp"
#include "world/weather.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::client {

struct FlightStart {
    // The aircraft, by its id in the catalogue (sim/catalogue.hpp).
    std::string aircraft = "c172p";
    double latitude_deg = -33.9461; // over Sydney airport
    double longitude_deg = 151.1772;
    double height_m = 1000.0; // above the WGS84 ellipsoid
    double heading_deg = 160.0;
    // Calibrated; by default, the catalogue's for the aircraft.
    std::optional<double> airspeed_kts;
    // Standing on the ground at the latitude and longitude, its wheels down,
    // rather than flying - the height is then the ground's.
    bool on_ground = false;
    // The airfield - its ICAO code - whose reported weather the flight is flown
    // in; empty for the standard atmosphere with no wind.
    std::string weather_station;
    // Microbursts put into that weather, for as long as each lasts.
    std::vector<world::Microburst> microbursts;
    // Flown by the AI from the first step: holding what the aircraft is doing,
    // or flying `plan` - from the plan's start, if it has one, rather than the
    // position above. A plan's altitudes are above sea level.
    bool autopilot = false;
    std::optional<sim::FlightPlan> plan;
};

inline constexpr double weather_refresh_seconds = 15 * 60.0;
inline constexpr double weather_blend_seconds = 5 * 60.0;

class Flight {
public:
    // Loads the aircraft `start` names, from the catalogue in `data`, stands
    // it on the DEM - tiles and the geoid
    // from `cache`, fetched there when missing - and starts it at `start`, in
    // the weather reported now at its station if it names one. Throws if any of
    // that cannot be had.
    //
    // The weather is fetched again every weather_refresh_seconds of the flight,
    // off the simulation's thread, and blended in over weather_blend_seconds; a
    // fetch that fails is reported and the weather kept. Its air rises and
    // sinks over the DEM's terrain 32 km either way along the wind, whose tiles
    // are fetched, as those under the aircraft are, when first needed.
    Flight(const std::filesystem::path& data, const std::filesystem::path& cache,
           const FlightStart& start);

    // The aircraft flown, from the catalogue.
    // Whether it started --on-ground afloat, a seaplane on water.
    bool afloat_at_start() const {
        return afloat_at_start_;
    }

    const sim::CatalogueEntry& aircraft() const {
        return aircraft_entry_;
    }

    // One step, with the pilot's controls - which fly the aircraft unless the
    // AI does. Each waypoint of a plan the AI passes is printed as it is
    // passed: how close it came, and at what altitude.
    void step(const sim::Controls& controls);

    // Hands the aircraft to the AI - flying what is left of the plan, if any
    // is - or back to the pilot (sim/controller.hpp).
    void swap_pilot();
    bool ai_flying() const {
        return controller_ && controller_->flying() == sim::Controller::Flying::ai;
    }

    std::int64_t tick() const {
        return tick_;
    }
    sim::AircraftState state() const {
        return aircraft_->state();
    }

    // The camera for a view of the aircraft: the cockpit looks out along the
    // nose from the pilot's eye, and the others stand off around it.
    // `orbit_rad` is where the orbit has got to, and only it uses that.
    gfx::Camera camera(gfx::View view, double orbit_rad = 0.0) const;

    // The aircraft's visual model, or null where it ships none - FlightGear
    // has no Learjet 35A, and docs/ASSETS.md says so.
    const gfx::Model* model() const {
        return model_ ? &*model_ : nullptr;
    }

    // Where that model goes: its origin - the flight model's visual reference
    // point moved by the alignment in assets/models/alignment.txt - and the
    // body's axes there. Meaningless without a model.
    gfx::Placement model_placement() const;

    // How far the model reaches from its origin, in metres: what the outside
    // views stand off by. Zero without a model.
    double model_radius() const {
        return model_radius_;
    }

    // The unit vector towards the sun in the body frame, for lighting the
    // model. It turns as the aircraft does, so the mesh is made again when
    // this has moved far enough to see.
    world::Ecef sun_in_body() const;

    gfx::HudReadings hud() const;

    // **The checklist on screen.** The aircraft's own lists are loaded with
    // it; `show_checklist` says which phase's is on screen, and from then on
    // it ticks itself as the aeroplane flies. Nothing shows until it is
    // asked for.
    void show_checklist(sim::Phase phase);
    void hide_checklist();
    // Which phase is showing, or nothing.
    std::optional<sim::Phase> showing_checklist() const;

    // The weather report flown in now, or null without one; and where its
    // station is, for drawing its sky.
    const world::WeatherReport* weather_report() const;
    gfx::Station weather_station() const;

    // Simulation time, seconds.
    double time_s() const {
        return aircraft_->state().sim_time_s;
    }

    // The aircraft's altitude above sea level - the geoid - in feet. The
    // simulation's is above the WGS84 ellipsoid, on which the DEM's ground is
    // set: at Sydney the two are 72 ft apart.
    double sea_level_ft() const;

    // One line of the state at this tick, for --trace: its altitude above sea
    // level as alt_ft, and above the ellipsoid as ell_ft.
    std::string trace() const;

private:
    void refresh_weather();
    void report_navigation();

    // The body's axes in ECEF - forward, starboard and down - and the
    // aircraft's position, which JSBSim reports at its centre of gravity.
    struct Axes {
        world::Ecef forward;
        world::Ecef right;
        world::Ecef down;
        world::Ecef position;
    };
    Axes axes() const;
    // A structural point, in the body frame relative to the model's origin.
    std::array<double, 3> from_model_origin(const char* what) const;

    world::Fetch fetch_;
    std::unique_ptr<world::DemCoverage> coverage_;
    std::unique_ptr<world::DemTiles> tiles_;
    std::unique_ptr<world::Geoid> geoid_;
    std::shared_ptr<world::Dem> dem_;
    sim::CatalogueEntry aircraft_entry_;
    // The visual model, read once, and where it sits on this aeroplane.
    std::optional<gfx::Model> model_;
    gfx::ModelAlignment alignment_;
    double model_radius_ = 0.0;
    bool afloat_at_start_ = false;
    std::unique_ptr<sim::Aircraft> aircraft_;
    // Made at the first step, from the pilot's controls then.
    std::unique_ptr<sim::Controller> controller_;
    bool start_with_ai_ = false;
    // The plan, its altitudes above the ellipsoid as the aircraft's are, and
    // how far along it the AI has flown: the waypoints passed, the closest the
    // next has come, and the altitude there.
    std::optional<sim::FlightPlan> plan_;
    std::size_t passed_ = 0;
    std::size_t handed_from_ = 0; // the waypoint the AI's plan began with
    double closest_m_ = std::numeric_limits<double>::infinity();
    double altitude_there_ft_ = 0.0;
    bool plan_flown_ = false;
    std::int64_t tick_ = 0;
    // The aircraft's checklists, and the one being worked through. A run is
    // kept whether or not one is showing, so that turning to a phase shows
    // what the aeroplane has already done rather than an empty list.
    std::optional<sim::ChecklistRun> checklist_;
    bool checklist_showing_ = false;
    std::string weather_station_;
    std::vector<world::Microburst> microbursts_;
    std::shared_ptr<world::ReportedWeather> weather_;
    double weather_fetched_at_s_ = 0.0;
    // Last, so that a fetch still under way is waited for before anything it
    // could reach is destroyed.
    std::future<world::WeatherReport> next_weather_;
};

} // namespace glideslope::client
