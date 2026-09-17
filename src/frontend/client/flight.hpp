#pragma once

// The flight screen: one aircraft, standing on or flying over the DEM, seen
// from its cockpit, with the HUD.

#include "gfx/hud.hpp"
#include "gfx/scene.hpp"
#include "sim/aircraft.hpp"
#include "world/dem.hpp"
#include "world/geoid.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace glideslope::client {

struct FlightStart {
    double latitude_deg = -33.9461; // over Sydney airport
    double longitude_deg = 151.1772;
    double height_m = 1000.0; // above the WGS84 ellipsoid
    double heading_deg = 160.0;
    double airspeed_kts = 100.0;
};

class Flight {
public:
    // Loads the Cessna from `data`, stands it on the DEM - tiles and the geoid
    // from `cache`, fetched there when missing - and starts it at `start`.
    // Throws if any of that cannot be had.
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
    std::unique_ptr<world::DemCoverage> coverage_;
    std::unique_ptr<world::DemTiles> tiles_;
    std::unique_ptr<world::Geoid> geoid_;
    std::shared_ptr<world::Dem> dem_;
    std::unique_ptr<sim::Aircraft> aircraft_;
    std::int64_t tick_ = 0;
};

} // namespace glideslope::client
