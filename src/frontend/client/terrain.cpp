#include "terrain.hpp"

#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/geoid.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>

namespace glideslope::client {

world::GeoRectangle cells_around(double latitude_deg, double longitude_deg,
                                 int radius) {
    // A whole-degree latitude belongs to the cell below it, as in the DEM.
    const double south = std::ceil(latitude_deg) - 1.0;
    const double west = std::floor(longitude_deg);
    return {std::max(-90.0, south - radius), west - radius,
            std::min(90.0, south + 1.0 + radius), west + 1.0 + radius};
}

std::unique_ptr<gfx::TerrainTiles> open_terrain(gfx::Renderer& renderer,
                                                const std::filesystem::path& data,
                                                const std::filesystem::path& cache,
                                                const world::GeoRectangle& region) {
    struct Ground {
        world::DemCoverage coverage;
        world::DownloadedTiles tiles;
        world::Geoid geoid;
        world::Dem dem;
        Ground(std::string coverage_text, const std::filesystem::path& cache,
               const world::Fetch& fetch)
            : coverage(coverage_text), tiles(cache, fetch),
              geoid(world::egm2008_geoid(cache, fetch)), dem(coverage, tiles, &geoid) {}
    };
    const std::filesystem::path coverage_path = data / "dem" / "coverage.txt";
    std::ifstream coverage_file(coverage_path, std::ios::binary);
    if (!coverage_file) {
        throw std::runtime_error("cannot read " + coverage_path.string());
    }
    auto ground = std::make_shared<Ground>(
        std::string(std::istreambuf_iterator<char>(coverage_file), {}), cache,
        world::http_fetch());

    gfx::TerrainOptions options;
    options.region = region;
    options.worker_threads =
        static_cast<int>(std::clamp(std::thread::hardware_concurrency(), 2u, 8u));
    return std::make_unique<gfx::TerrainTiles>(
        renderer, options, [ground](double latitude_deg, double longitude_deg) {
            return world::GroundHeight{
                ground->dem.height_above_geoid(latitude_deg, longitude_deg),
                ground->geoid.undulation(latitude_deg, longitude_deg)};
        });
}

} // namespace glideslope::client
