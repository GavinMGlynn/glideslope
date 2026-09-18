#pragma once

// The open-data terrain the client draws: the Copernicus DEM over a region of
// whole-degree cells, through Cesium Native.

#include "gfx/renderer.hpp"
#include "gfx/terrain_tiles.hpp"
#include "world/terrain_mesh.hpp"

#include <filesystem>
#include <memory>

namespace glideslope::client {

// The whole-degree cells within `radius` cells of the one holding a latitude
// and longitude, as one rectangle.
world::GeoRectangle cells_around(double latitude_deg, double longitude_deg, int radius);

// Terrain over `region`, drawn by `renderer`, from the DEM's tiles and the geoid
// in `cache` - fetched there when missing - with the coverage in `data`. Its
// DEM is its own, not shared with the simulation's: tiles are made on worker
// threads.
std::unique_ptr<gfx::TerrainTiles> open_terrain(gfx::Renderer& renderer,
                                                const std::filesystem::path& data,
                                                const std::filesystem::path& cache,
                                                const world::GeoRectangle& region);

} // namespace glideslope::client
