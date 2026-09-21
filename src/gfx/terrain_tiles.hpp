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
// cell size. It is coloured by height and slope (gfx/terrain_colour.hpp), or
// draped with imagery - Cesium Native's raster overlays, each imagery tile a
// texture on the GPU - lit by the slope.

#include "gfx/renderer.hpp"
#include "gfx/scene.hpp"
#include "world/terrain_mesh.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::gfx {

// Puts Cesium Native's log on standard error.
//
// Cesium Native logs through spdlog, whose default logger writes to standard
// *output* - the stream the client's own output goes to. A log line then
// lands in the middle of one of ours: a `--trace` line has been cut in half
// by "[error] [SqliteCache.cpp] database is locked", which happens when tests
// run at once and share one cache, and the test reading that line saw half a
// number. Standard output is what the program says; standard error is what
// goes wrong with it, and that is where this puts the log.
//
// It also quiets the log to warnings and worse: Cesium Native says at info
// level which URLs it fetched, and a Cesium ion URL carries the user's own
// token, which would then be in whatever kept the output - a CI log included.
//
// Call it before anything that can log - the client does, first thing. It may
// be called more than once.
void log_to_standard_error();

// Imagery to drape on the terrain: a Web Map Tile Service in latitude and
// longitude, its tiles fetched as the view needs them.
struct Imagery {
    std::string url; // a template: {Layer}, {Style}, {TileMatrixSet} and so on
    std::string layer;
    std::string style;
    std::string tile_matrix_set;
    std::string format;         // "image/jpeg"
    unsigned maximum_level = 0; // the finest level worth fetching
    std::string credit;         // shown wherever the imagery is
};

// The open imagery (REQUIREMENTS.md, section 9): EOX's Sentinel-2 cloudless
// mosaic of 2016, 10 m a pixel, under CC BY 4.0. See docs/ASSETS.md.
Imagery open_imagery();

// Where the terrain drawn comes from. The ground the aircraft meets is never
// any of these: collision terrain is always the open DEM, so that the server
// and every client agree on where the ground is (CLAUDE.md). A visual provider
// may disagree with it, and by how much is measured, not assumed.
enum class Provider {
    open,   // the Copernicus DEM with the open imagery: no key, always there
    ion,    // Cesium ion's world terrain, with the user's own token
    google, // Google's Photorealistic 3D Tiles, through ion or a Google key
};

// Every provider, in the order --terrain lists them.
const std::vector<Provider>& every_provider();
std::string_view name_of(Provider provider);
std::optional<Provider> provider_named(std::string_view name);
std::string provider_names();

// Why a provider cannot be used now, or empty if it can: a provider needing a
// key the user has not given says so rather than failing.
std::string why_not(Provider provider, const std::string& ion_token,
                    const std::string& google_key);

struct TerrainOptions {
    // Where the terrain comes from, and the user's own keys for it. A key is
    // never in the repository: platform/paths.hpp reads it at run time.
    Provider provider = Provider::open;
    std::string ion_token;
    std::string google_key;
    // Where there is terrain.
    world::GeoRectangle region;
    // Imagery draped on it, lit by the terrain's slope; without, the terrain
    // is tinted by height instead.
    std::optional<Imagery> imagery;
    // Where Cesium Native keeps what it fetches between runs, as the fetched
    // data's own caching headers allow; empty for nowhere.
    std::filesystem::path cache_file;
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
        // With imagery, tiles drawn in the last update with none on them yet.
        std::size_t without_imagery = 0;
    };
    Counts counts() const;

    // What must be on screen wherever this terrain is drawn: the provider's
    // attribution, in plain text. Cesium Native collects it as each tile
    // loads - Cesium ion's and Google's terms are met by showing it - and the
    // open provider's notices are its data's own. It grows as tiles arrive,
    // so it is asked for every frame.
    std::vector<std::string> credits() const;

    // Which provider this is drawing.
    Provider provider() const;

    // The height of the drawn surface above the ellipsoid at each place, in
    // metres, or nothing where the provider has no surface there.
    //
    // **This is the terrain that is seen, not the terrain that is flown.**
    // What an aircraft meets is always the open DEM, so that a server and
    // every client agree on where the ground is; a visual provider may put
    // its surface somewhere else, and how far is what this measures. It
    // loads whatever tiles it needs to answer, so it is slow and is not for
    // a frame.
    std::vector<std::optional<double>> heights_at(
        const std::vector<world::Geodetic>& places);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glideslope::gfx
