#pragma once

// Heights anywhere on Earth, from the Copernicus DEM.
//
// **Where the ground is.** The DEM is a grid of heights above the EGM2008 geoid
// at points one arc-second apart in latitude (GLO-30; three in GLO-90) and
// further apart in longitude towards the poles, in 1-degree tiles. A height
// between points is interpolated bilinearly from the four around it, which
// near a tile's south or east edge come from the next tile - whose spacing in
// longitude can differ (at 50 degrees, and wherever a 90 m tile meets a 30 m
// one), and then that tile's own row or column is interpolated to the point
// needed. So the surface is continuous everywhere: across tiles, across the
// antimeridian, and onto the sea, which the DEM has no tiles for and whose
// height above the geoid is zero.
//
// Positions on the grid are kept in whole half-arc-seconds, in which every
// spacing either dataset uses is a whole number, so the edge of one tile meets
// the next exactly rather than to within a rounding error.
//
// **Which tiles exist** is `assets/dem/coverage.txt`: 30 m where there is a
// tile, 90 m where only that exists (25 cells around Armenia and Azerbaijan),
// or neither. Where the tiles come from - a directory, a download - is a
// DemTiles.

#include "world/byte_source.hpp"
#include "world/geoid.hpp"
#include "world/geotiff.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::world {

struct DemError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class DemDataset { none, glo30, glo90 };

// A 1-degree cell, named by its south-west corner as the tiles are: latitude
// -90 to 89, longitude -180 to 179.
struct DemCell {
    int latitude = 0;
    int longitude = 0;
};

class DemCoverage {
public:
    // Parses assets/dem/coverage.txt. Throws DemError if it is malformed.
    explicit DemCoverage(std::string_view text);

    DemDataset at(DemCell cell) const;

    // Cells of each kind, for knowing the file was read whole.
    int count(DemDataset dataset) const;

private:
    std::vector<std::uint8_t> cells_; // 180 rows, north first; 360 columns from 180 W
};

// The notice the Copernicus DEM's licence asks for wherever its data, adapted
// as terrain or heights are, are shown - its Article 6(b); see docs/ASSETS.md.
inline constexpr const char* copernicus_dem_notice =
    "Produced using Copernicus WorldDEM-30 \xc2\xa9 DLR e.V. 2010-2014 and \xc2\xa9 "
    "Airbus "
    "Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union "
    "and "
    "ESA; all rights reserved";

// "Copernicus_DSM_COG_10_S34_00_E151_00_DEM", for GLO-30.
std::string dem_tile_name(DemDataset dataset, DemCell cell);

// Where the public bucket keeps a tile.
std::string dem_tile_url(DemDataset dataset, DemCell cell);

// Where tiles come from.
class DemTiles {
public:
    virtual ~DemTiles() = default;
    // The tile's bytes. Throws DemError if it cannot be had.
    virtual std::shared_ptr<const ByteSource> open(DemDataset dataset,
                                                   DemCell cell) = 0;
};

// Tiles already in a directory, named <tile name>.tif.
class DirectoryTiles : public DemTiles {
public:
    explicit DirectoryTiles(std::filesystem::path directory);
    std::shared_ptr<const ByteSource> open(DemDataset dataset, DemCell cell) override;

private:
    std::filesystem::path directory_;
};

class Dem {
public:
    // `geoid` may be null, and then only heights above the geoid are given.
    Dem(const DemCoverage& coverage, DemTiles& tiles, const Geoid* geoid);

    // The DEM's height above the geoid - above sea level - in metres.
    double height_above_geoid(double latitude_deg, double longitude_deg);

    // The same place's height above the WGS84 ellipsoid: what a position is
    // kept in. Throws DemError without a geoid.
    double height_above_ellipsoid(double latitude_deg, double longitude_deg);

private:
    struct Tile {
        DemDataset dataset = DemDataset::none;
        std::shared_ptr<const ByteSource> bytes;
        GeoTiff tiff;
        // The grid, in half-arc-seconds between samples.
        std::int64_t latitude_step = 0;
        std::int64_t longitude_step = 0;
        std::int64_t rows = 0;
        std::int64_t columns = 0;
    };

    const Tile& tile(DemCell cell);
    float stored_sample(const Tile& tile, DemCell cell, std::int64_t row,
                        std::int64_t column);
    double sample(DemCell cell, std::int64_t row, std::int64_t column, int depth);
    double at_units(std::int64_t latitude, std::int64_t longitude, int depth);
    double interpolate(DemCell cell, double row, double column, int depth);

    const DemCoverage& coverage_;
    DemTiles& tiles_;
    const Geoid* geoid_;

    std::map<std::pair<int, int>, Tile> tile_cache_;
    std::list<std::pair<int, int>> tile_order_; // most recently used first

    struct BlockKey {
        int latitude;
        int longitude;
        std::uint32_t index;
        auto operator<=>(const BlockKey&) const = default;
    };
    std::map<BlockKey, std::vector<float>> block_cache_;
    std::list<BlockKey> block_order_;
};

} // namespace glideslope::world
