#pragma once

// The flight screen: one aircraft, standing on or flying over the DEM, seen
// from its cockpit, with the HUD.

#include "gfx/hud.hpp"
#include "gfx/scene.hpp"
#include "sim/aircraft.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/geoid.hpp"
#include "world/weather.hpp"

#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace glideslope::client {

struct FlightStart {
    double latitude_deg = -33.9461; // over Sydney airport
    double longitude_deg = 151.1772;
    double height_m = 1000.0; // above the WGS84 ellipsoid
    double heading_deg = 160.0;
    double airspeed_kts = 100.0;
    // The airfield - its ICAO code - whose reported weather the flight is flown
    // in; empty for the standard atmosphere with no wind.
    std::string weather_station;
    // Microbursts put into that weather, for as long as each lasts.
    std::vector<world::Microburst> microbursts;
};

inline constexpr double weather_refresh_seconds = 15 * 60.0;
inline constexpr double weather_blend_seconds = 5 * 60.0;

class Flight {
public:
    // Loads the Cessna from `data`, stands it on the DEM - tiles and the geoid
    // from `cache`, fetched there when missing - and starts it at `start`, in
    // the weather reported now at its station if it names one. Throws if any of
    // that cannot be had.
    //
    // The weather is fetched again every weather_refresh_seconds of the flight,
    // off the simulation's thread, and blended in over weather_blend_seconds; a
    // fetch that fails is reported and the weather kept.
    Flight(const std::filesystem::path& data, const std::filesystem::path& cache,
           const FlightStart& start);

    void step(const sim::Controls& controls);

    std::int64_t tick() const {
        return tick_;
    }
    sim::AircraftState state() const {
        return aircraft_->state();
    }

    // The camera at the pilot's eye, looking along the aircraft's nose.
    gfx::Camera camera() const;

    gfx::HudReadings hud() const;

    // One line of the state at this tick, for --trace.
    std::string trace() const;

private:
    void refresh_weather();

    world::Fetch fetch_;
    std::unique_ptr<world::DemCoverage> coverage_;
    std::unique_ptr<world::DemTiles> tiles_;
    std::unique_ptr<world::Geoid> geoid_;
    std::shared_ptr<world::Dem> dem_;
    std::unique_ptr<sim::Aircraft> aircraft_;
    std::int64_t tick_ = 0;
    std::string weather_station_;
    std::vector<world::Microburst> microbursts_;
    std::shared_ptr<world::ReportedWeather> weather_;
    double weather_fetched_at_s_ = 0.0;
    // Last, so that a fetch still under way is waited for before anything it
    // could reach is destroyed.
    std::future<world::WeatherReport> next_weather_;
};

} // namespace glideslope::client
