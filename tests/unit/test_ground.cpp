#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/terrain.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/geoid.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

using glideslope::sim::Aircraft;
using glideslope::sim::AircraftState;
using glideslope::sim::Controls;
using glideslope::sim::FunctionTerrain;
using glideslope::sim::InitialConditions;
using glideslope::sim::Terrain;
using glideslope::test::check;

namespace {

constexpr double feet_per_metre = 1.0 / 0.3048;
constexpr double degrees = 180.0 / 3.14159265358979323846;
// Nose wheel to main wheels, from the model's gear locations (c172p.xml:
// x -6.8 in and 58.2 in).
constexpr double wheelbase_ft = 65.0 / 12.0;

struct Rest {
    AircraftState state;
    // Above the ellipsoid along its normal, as DEM heights are; JSBSim's
    // altitude_ft is along the Earth's radius, which at Denver's height is
    // 0.03 ft different.
    double geodetic_altitude_ft = 0.0;
    double nose_compression_ft = 0.0;
    double main_compression_ft = 0.0; // the mean of the two
    bool all_wheels_down = false;
    double ground_speed_kts = 0.0;
};

// Sets the Cessna down - engine idling, brakes on - and lets it settle for
// twenty seconds.
Rest set_down(std::shared_ptr<Terrain> terrain, double elevation_ft, double latitude,
              double longitude, double heading) {
    Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    if (terrain) {
        a.set_terrain(terrain);
    }
    InitialConditions ic;
    ic.latitude_deg = latitude;
    ic.longitude_deg = longitude;
    ic.heading_deg = heading;
    ic.terrain_elevation_ft = elevation_ft;
    // At the surface itself: initialize() raises the aircraft until its
    // lowest wheel touches. It once left the wheels four feet under it, and
    // the struts threw the aircraft into the air - harmlessly on level
    // ground, and end over end facing down a slope.
    ic.altitude_ft = elevation_ft;
    a.initialize(ic);
    Controls c;
    c.left_brake = c.right_brake = 1.0;
    a.set_controls(c);
    for (int i = 0; i < 20 * 120; ++i) {
        a.step();
    }
    Rest r;
    r.state = a.state();
    r.geodetic_altitude_ft = a.property("position/geod-alt-ft");
    r.nose_compression_ft = a.property("gear/unit[0]/compression-ft");
    r.main_compression_ft = (a.property("gear/unit[1]/compression-ft") +
                             a.property("gear/unit[2]/compression-ft")) /
                            2;
    r.all_wheels_down = a.property("gear/unit[0]/WOW") > 0.5 &&
                        a.property("gear/unit[1]/WOW") > 0.5 &&
                        a.property("gear/unit[2]/WOW") > 0.5;
    r.ground_speed_kts = a.property("velocities/vg-fps") * 0.592484;
    return r;
}

bool at_rest(const Rest& r) {
    return r.all_wheels_down && r.ground_speed_kts < 0.01 &&
           std::abs(r.state.climb_rate_fpm) < 0.01;
}

} // namespace

GLIDESLOPE_TEST(level_terrain_holds_an_aircraft_exactly_as_jsbsims_own_ground_does) {
    // At sea level and at Denver's height: the terrain callback, level, must be
    // JSBSim's ground - the same rest, to the last digit it reports.
    for (const double metres : {0.0, 1656.0}) {
        const double feet = metres * feet_per_metre;
        const Rest own = set_down(nullptr, feet, 39.9, -104.7, 0.0);
        const Rest ours = set_down(
            std::make_shared<FunctionTerrain>([=](double, double) { return metres; }),
            feet, 39.9, -104.7, 0.0);
        const std::string where = "at " + std::to_string(metres) + " m";
        check(at_rest(own) && at_rest(ours),
              where + ": both at rest on all three wheels");
        check(std::abs(ours.state.altitude_ft - own.state.altitude_ft) < 1e-6 &&
                  std::abs(ours.state.pitch_deg - own.state.pitch_deg) < 1e-6 &&
                  std::abs(ours.state.roll_deg - own.state.roll_deg) < 1e-6 &&
                  std::abs(ours.nose_compression_ft - own.nose_compression_ft) < 1e-6,
              where + ": the same height, attitude and strut compression");
        check(std::abs(ours.state.height_above_ground_ft -
                       own.state.height_above_ground_ft) < 1e-6,
              where + ": the same height above the ground");
        check(std::abs(ours.state.terrain_elevation_ft - feet) < 1e-6,
              where + ": JSBSim sees the terrain's elevation");
    }
}

GLIDESLOPE_TEST(an_aircraft_set_down_on_a_slope_rests_tilted_by_the_slope) {
    // Rising 10% to the north. Facing up it and down it, the aircraft must come
    // to rest on all three wheels, its centre of gravity the same height above
    // the slope as above level ground, and pitched by the slope's angle - plus
    // what the struts add, which is measured, not guessed: the nose strut
    // extends as weight moves aft, the mains compress, over the wheelbase.
    constexpr double latitude = 45.0;
    constexpr double longitude = 6.0;
    constexpr double base_m = 1000.0;
    constexpr double grade = 0.10;
    const auto slope = std::make_shared<FunctionTerrain>([](double lat, double) {
        return base_m + grade * (lat - latitude) / degrees * 6378137.0;
    });
    const Rest level = set_down(
        std::make_shared<FunctionTerrain>([](double, double) { return base_m; }),
        base_m * feet_per_metre, latitude, longitude, 0.0);
    check(at_rest(level), "at rest on level ground");

    for (const double heading : {0.0, 180.0}) {
        const Rest r =
            set_down(slope, base_m * feet_per_metre, latitude, longitude, heading);
        const std::string facing = heading == 0.0 ? "facing uphill" : "facing downhill";
        check(at_rest(r), facing + ": at rest on all three wheels");
        check(std::abs(r.state.height_above_ground_ft -
                       level.state.height_above_ground_ft) < 0.1,
              facing + ": the centre of gravity " +
                  std::to_string(r.state.height_above_ground_ft) +
                  " ft above the slope, " +
                  std::to_string(level.state.height_above_ground_ft) +
                  " ft above level ground");
        const double slope_deg =
            std::atan(grade) * degrees * (heading == 0.0 ? 1.0 : -1.0);
        const double struts_deg =
            std::atan(((level.nose_compression_ft - r.nose_compression_ft) +
                       (r.main_compression_ft - level.main_compression_ft)) /
                      wheelbase_ft) *
            degrees;
        const double expected = level.state.pitch_deg + slope_deg + struts_deg;
        check(std::abs(r.state.pitch_deg - expected) < 0.1,
              facing + ": pitched " + std::to_string(r.state.pitch_deg) +
                  " degrees, expected " + std::to_string(expected) + " (level " +
                  std::to_string(level.state.pitch_deg) + ", slope " +
                  std::to_string(slope_deg) + ", struts " + std::to_string(struts_deg) +
                  ")");
    }
}

GLIDESLOPE_TEST(
    an_aircraft_with_its_brakes_off_rolls_down_a_slope_and_stays_on_level_ground) {
    // The slope's pull comes through the ground's normal: straight up, the
    // ground would hold the aircraft wherever it stood. Engine stopped, brakes
    // off, facing down a 10% slope, it must roll away; on level ground it must
    // not.
    constexpr double latitude = 45.0;
    const auto roll = [](std::shared_ptr<Terrain> terrain) {
        Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "c172p");
        a.set_terrain(terrain);
        InitialConditions ic;
        ic.latitude_deg = latitude;
        ic.longitude_deg = 6.0;
        ic.heading_deg = 180.0;
        ic.altitude_ft = terrain->height_m(latitude, 6.0) * feet_per_metre;
        ic.engine_running = false;
        a.initialize(ic);
        Controls c;
        c.mixture = 0.0;
        a.set_controls(c);
        for (int i = 0; i < 10 * 120; ++i) {
            a.step();
        }
        return a.property("velocities/vg-fps") * 0.592484;
    };
    const double on_slope =
        roll(std::make_shared<FunctionTerrain>([](double lat, double) {
            return 1000.0 + 0.10 * (lat - latitude) / degrees * 6378137.0;
        }));
    const double on_level =
        roll(std::make_shared<FunctionTerrain>([](double, double) { return 1000.0; }));
    check(on_slope > 5.0,
          "down the slope at " + std::to_string(on_slope) + " kt after ten seconds");
    check(on_level < 0.1, "still on level ground: " + std::to_string(on_level) + " kt");
}

GLIDESLOPE_TEST(
    an_aircraft_set_down_on_the_dem_rests_on_its_surface_at_sea_level_a_high_airfield_and_a_slope) {
    // On the real terrain: Boston Logan's runway 33L, 4.8 m above the sea;
    // Denver's 34L, at 1,624 m; and halfway up Courchevel's runway, whose
    // ground rises some 18% towards its top.
    const std::filesystem::path source(GLIDESLOPE_TEST_SOURCE_DIR);
    const std::filesystem::path downloads(GLIDESLOPE_TEST_DOWNLOADS_DIR);
    std::ifstream coverage_file(source / "../assets/dem/coverage.txt",
                                std::ios::binary);
    const glideslope::world::DemCoverage coverage(
        std::string(std::istreambuf_iterator<char>(coverage_file), {}));
    const glideslope::world::Fetch fetch = glideslope::world::http_fetch();
    glideslope::world::DownloadedTiles tiles(downloads, fetch);
    std::shared_ptr<glideslope::world::Geoid> geoid;
    try {
        geoid = std::make_shared<glideslope::world::Geoid>(
            glideslope::world::egm2008_geoid(downloads, fetch));
    } catch (const glideslope::world::DemError& e) {
        if (!glideslope::test::network_required()) {
            glideslope::test::skip(std::string("the geoid could not be had: ") +
                                   e.what());
        }
        throw;
    }
    auto dem = std::make_shared<glideslope::world::Dem>(coverage, tiles, geoid.get());
    const auto terrain =
        std::make_shared<FunctionTerrain>([dem](double lat, double lon) {
            return dem->height_above_ellipsoid(lat, lon);
        });

    struct Place {
        const char* name;
        double latitude;
        double longitude;
        double heading;
    };
    for (const Place& p : {Place{"Boston 33L", 42.35465078, -70.99158605, 330.0},
                           Place{"Denver 34L", 39.85188683, -104.69658795, 0.5},
                           Place{"Courchevel", 45.39772, 6.63538, 222.0}}) {
        const double ground_m = dem->height_above_ellipsoid(p.latitude, p.longitude);
        const Rest r = set_down(terrain, ground_m * feet_per_metre, p.latitude,
                                p.longitude, p.heading);
        const std::string name = p.name;
        check(at_rest(r), name + ": at rest on all three wheels");
        const double below_ft =
            dem->height_above_ellipsoid(r.state.latitude_deg, r.state.longitude_deg) *
            feet_per_metre;
        check(std::abs((r.geodetic_altitude_ft - below_ft) -
                       r.state.height_above_ground_ft) < 0.01,
              name +
                  ": the ground JSBSim stands on is the DEM's, where it came to rest");
        check(r.state.height_above_ground_ft > 4.0 &&
                  r.state.height_above_ground_ft < 5.0,
              name + ": its centre of gravity " +
                  std::to_string(r.state.height_above_ground_ft) +
                  " ft above the DEM, as on its wheels");

        if (name == "Courchevel") {
            // Tilted by the DEM's own slope under it, along its heading.
            const Rest level =
                set_down(std::make_shared<FunctionTerrain>(
                             [=](double, double) { return ground_m; }),
                         ground_m * feet_per_metre, p.latitude, p.longitude, p.heading);
            const double heading = r.state.heading_deg / degrees;
            const double lat = r.state.latitude_deg;
            const double lon = r.state.longitude_deg;
            constexpr double half_m = 2.7; // half the wheelbase
            const double d_lat = half_m * std::cos(heading) / 6378137.0 * degrees;
            const double d_lon = half_m * std::sin(heading) /
                                 (6378137.0 * std::cos(lat / degrees)) * degrees;
            const double rise = dem->height_above_ellipsoid(lat + d_lat, lon + d_lon) -
                                dem->height_above_ellipsoid(lat - d_lat, lon - d_lon);
            const double slope_deg = std::atan(rise / (2 * half_m)) * degrees;
            const double struts_deg =
                std::atan(((level.nose_compression_ft - r.nose_compression_ft) +
                           (r.main_compression_ft - level.main_compression_ft)) /
                          wheelbase_ft) *
                degrees;
            const double expected = level.state.pitch_deg + slope_deg + struts_deg;
            check(slope_deg > 5.0,
                  "Courchevel's slope here is steep: " + std::to_string(slope_deg));
            check(std::abs(r.state.pitch_deg - expected) < 0.5,
                  "Courchevel: pitched " + std::to_string(r.state.pitch_deg) +
                      " degrees, expected " + std::to_string(expected) + " (slope " +
                      std::to_string(slope_deg) + ")");
        }
    }
}

GLIDESLOPE_TEST(the_ground_under_an_aircraft_is_water_or_land_where_the_dem_water_mask_says) {
    // The pinned tile south of Sydney and its water body mask, read from the
    // downloads directory as a directory of tiles.
    const std::filesystem::path source(GLIDESLOPE_TEST_SOURCE_DIR);
    const std::filesystem::path downloads(GLIDESLOPE_TEST_DOWNLOADS_DIR);
    for (const char* name : {"Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif",
                             "Copernicus_DSM_COG_10_S34_00_E151_00_WBM.tif"}) {
        if (!std::filesystem::exists(downloads / name)) {
            glideslope::test::skip(std::string(name) + " was not fetched");
        }
    }
    std::ifstream coverage_file(source / "../assets/dem/coverage.txt", std::ios::binary);
    const glideslope::world::DemCoverage coverage(
        std::string(std::istreambuf_iterator<char>(coverage_file), {}));
    glideslope::world::DirectoryTiles tiles(downloads);
    auto dem = std::make_shared<glideslope::world::Dem>(coverage, tiles, nullptr);
    const auto terrain = std::make_shared<FunctionTerrain>(
        [dem](double lat, double lon) { return dem->height_above_geoid(lat, lon); },
        [dem](double lat, double lon) {
            return dem->water(lat, lon) != glideslope::world::Water::none;
        });

    using glideslope::world::Water;
    // Each at least a kilometre from a shore. The mask calls the harbours and
    // bays, where rivers meet the sea, river.
    struct Place {
        const char* name;
        double latitude;
        double longitude;
        Water water;
    };
    for (const Place& p :
         {Place{"the Tasman Sea, 8 km off Bondi", -33.90, 151.35, Water::ocean},
          Place{"the Tasman Sea, 2 km off Maroubra", -33.95, 151.28, Water::ocean},
          Place{"Maroubra, 1.5 km inland", -33.95, 151.24, Water::none},
          Place{"Lake Macquarie", -33.08, 151.60, Water::lake},
          Place{"Tuggerah Lake", -33.32, 151.49, Water::lake},
          Place{"Sydney Harbour, off Bradleys Head", -33.851, 151.255, Water::river},
          Place{"Botany Bay", -33.99, 151.19, Water::river},
          Place{"Broken Bay, the Hawkesbury's mouth", -33.57, 151.30, Water::river},
          Place{"Sydney airport, beside runway 16R", -33.9461, 151.1772, Water::none},
          Place{"Hyde Park, Sydney", -33.873, 151.211, Water::none},
          Place{"Parramatta", -33.815, 151.003, Water::none}}) {
        const std::string name = p.name;
        check(dem->water(p.latitude, p.longitude) == p.water,
              name + ": the mask says what it is");
        // Flying 1,000 ft over it: the ground JSBSim is given below is water
        // or land as the mask says.
        Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "c172p");
        a.set_terrain(terrain);
        InitialConditions ic;
        ic.latitude_deg = p.latitude;
        ic.longitude_deg = p.longitude;
        ic.altitude_ft = dem->height_above_geoid(p.latitude, p.longitude) * feet_per_metre +
                         1000.0;
        ic.airspeed_kts = 100.0;
        a.initialize(ic);
        a.step();
        const AircraftState s = a.state();
        check(s.on_water == (p.water != Water::none),
              name + ": the ground under the aircraft is " +
                  (s.on_water ? "water" : "land"));
        check(!s.ditched, name + ": in the air, nothing has ditched");
    }
}
