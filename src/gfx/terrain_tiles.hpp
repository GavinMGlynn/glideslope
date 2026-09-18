#pragma once

// Terrain drawn through Cesium Native.
//
// **Cesium Native chooses; this draws.** The terrain is a Cesium Native tileset:
// it decides which tiles a view needs - coarse far away, fine close by - loads
// them on worker threads, and keeps a cache. Its tiles are glTF models, and
// this is the glue that turns each into the renderer's meshes, uploads them on
// the main thread, and frees them when Cesium Native drops the tile.
//
// **The open-data terrain is made here, not fetched.** Cesium Native streams
// quantized-mesh terrain and 3D Tiles, and nothing serves the Copernicus DEM as
// either; so its tiles come from a loader that builds each one from the DEM
// (world/terrain_mesh.hpp): a quadtree over a region, every tile the same grid
// of cells, down to the DEM's own spacing. A tile's geometric error is half its
// cell size. It is coloured by height and slope (gfx/terrain_colour.hpp) until
// there is imagery.

#include "gfx/renderer.hpp"
#include "gfx/scene.hpp"
#include "world/terrain_mesh.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace glideslope::gfx {

struct TerrainOptions {
    // Where there is terrain.
    world::GeoRectangle region;
    // How many pixels a tile's shape may be out on screen before finer tiles
    // are drawn in its place.
    double maximum_screen_space_error = 4.0;
    // Cesium Native's worker threads.
    int worker_threads = 4;
};

inline constexpr int terrain_tile_cells = 32;

// The finest spacing the terrain is built at, in degrees of latitude: the
// DEM's.
inline constexpr double terrain_finest_spacing_deg = 1.0 / 3600.0;

class TerrainTiles {
public:
    // Terrain over `options.region`, drawn by `renderer`, with heights from
    // `heights` - called on worker threads, never two calls at once.
    TerrainTiles(Renderer& renderer, const TerrainOptions& options,
                 world::HeightSource heights);
    ~TerrainTiles();

    TerrainTiles(const TerrainTiles&) = delete;
    TerrainTiles& operator=(const TerrainTiles&) = delete;

    // The tiles `camera` needs on a frame `width` by `height` pixels, to draw.
    // Loads what it can without waiting - or, when `complete`, every tile the
    // view needs, waiting for them all, so that the same view draws the same
    // terrain every time.
    std::vector<Draw> update(const Camera& camera, int width, int height,
                             bool complete);

    struct Counts {
        std::size_t drawn = 0;   // tiles in the last update
        std::size_t loaded = 0;  // tiles Cesium Native holds loaded
        std::size_t deepest = 0; // the deepest level drawn
        std::size_t failed = 0;  // tiles that could not be made, ever
        std::size_t skipped = 0; // glTF primitives that could not be drawn, ever
    };
    Counts counts() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glideslope::gfx
