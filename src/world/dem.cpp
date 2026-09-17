#include "world/dem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace glideslope::world {

namespace {

constexpr std::int64_t units_per_degree = 7200; // half-arc-seconds

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) {
        --q;
    }
    return q;
}

// A step in degrees as whole half-arc-seconds, if it is one.
std::int64_t step_units(double degrees, const std::string& what) {
    const double units = degrees * static_cast<double>(units_per_degree);
    const double rounded = std::round(units);
    if (rounded < 1.0 || std::abs(units - rounded) > 1e-6) {
        throw DemError(what + ": a sample spacing of " + std::to_string(degrees) +
                       " degrees is not a whole number of half-arc-seconds");
    }
    return static_cast<std::int64_t>(rounded);
}

// The longitude spacing Copernicus uses in a cell's band of latitude, in
// multiples of the latitude spacing, from the bucket's readme - for cells with
// no tile, whose grid only needs to be one the sea could have.
std::int64_t band_multiple_x2(int cell_latitude) {
    const int nearest_equator = cell_latitude >= 0 ? cell_latitude : -cell_latitude - 1;
    if (nearest_equator < 50) {
        return 2;
    }
    if (nearest_equator < 60) {
        return 3;
    }
    if (nearest_equator < 70) {
        return 4;
    }
    if (nearest_equator < 80) {
        return 6;
    }
    if (nearest_equator < 85) {
        return 10;
    }
    return 20;
}

constexpr std::size_t max_tiles = 16;
constexpr std::size_t max_blocks = 64;

} // namespace

DemCoverage::DemCoverage(std::string_view text) : cells_(180 * 360, 0) {
    int rows = 0;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (rows >= 180) {
            throw DemError("the DEM coverage has more than 180 rows");
        }
        const int latitude = 89 - rows;
        const int degrees = latitude >= 0 ? latitude : -latitude;
        const std::string name = (latitude >= 0 ? "N" : "S") +
                                 std::string(degrees < 10 ? "0" : "") +
                                 std::to_string(degrees);
        if (line.size() != 4 + 360 || line.substr(0, 3) != name || line[3] != ' ') {
            throw DemError("the DEM coverage's row for " + name + " is malformed");
        }
        for (std::size_t c = 0; c < 360; ++c) {
            const char v = line[4 + c];
            if (v != '0' && v != '1' && v != '2') {
                throw DemError("the DEM coverage's row for " + name + " holds '" + v +
                               "'");
            }
            cells_[static_cast<std::size_t>(rows) * 360 + c] =
                static_cast<std::uint8_t>(v - '0');
        }
        ++rows;
    }
    if (rows != 180) {
        throw DemError("the DEM coverage has " + std::to_string(rows) +
                       " rows, not 180");
    }
}

DemDataset DemCoverage::at(DemCell cell) const {
    if (cell.latitude < -90 || cell.latitude > 89 || cell.longitude < -180 ||
        cell.longitude > 179) {
        throw DemError("no cell at " + std::to_string(cell.latitude) + ", " +
                       std::to_string(cell.longitude));
    }
    const auto row = static_cast<std::size_t>(89 - cell.latitude);
    const auto column = static_cast<std::size_t>(cell.longitude + 180);
    switch (cells_[row * 360 + column]) {
    case 2: return DemDataset::glo30;
    case 1: return DemDataset::glo90;
    default: return DemDataset::none;
    }
}

int DemCoverage::count(DemDataset dataset) const {
    const std::uint8_t v = dataset == DemDataset::glo30   ? 2
                           : dataset == DemDataset::glo90 ? 1
                                                          : 0;
    return static_cast<int>(std::count(cells_.begin(), cells_.end(), v));
}

std::string dem_tile_name(DemDataset dataset, DemCell cell) {
    if (dataset == DemDataset::none) {
        throw DemError("the sea has no tile");
    }
    char name[64];
    std::snprintf(name, sizeof name, "Copernicus_DSM_COG_%s_%c%02d_00_%c%03d_00_DEM",
                  dataset == DemDataset::glo30 ? "10" : "30",
                  cell.latitude >= 0 ? 'N' : 'S', std::abs(cell.latitude),
                  cell.longitude >= 0 ? 'E' : 'W', std::abs(cell.longitude));
    return name;
}

std::string dem_tile_url(DemDataset dataset, DemCell cell) {
    const std::string name = dem_tile_name(dataset, cell);
    return std::string("https://copernicus-dem-") +
           (dataset == DemDataset::glo30 ? "30m" : "90m") + ".s3.amazonaws.com/" +
           name + "/" + name + ".tif";
}

DirectoryTiles::DirectoryTiles(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

std::shared_ptr<const ByteSource> DirectoryTiles::open(DemDataset dataset,
                                                       DemCell cell) {
    const std::filesystem::path path =
        directory_ / (dem_tile_name(dataset, cell) + ".tif");
    if (!std::filesystem::exists(path)) {
        throw DemError(path.string() + " is not there");
    }
    try {
        return std::make_shared<FileSource>(path);
    } catch (const ByteSourceError& e) {
        throw DemError(e.what());
    }
}

Dem::Dem(const DemCoverage& coverage, DemTiles& tiles, const Geoid* geoid)
    : coverage_(coverage), tiles_(tiles), geoid_(geoid) {}

const Dem::Tile& Dem::tile(DemCell cell) {
    const std::pair key{cell.latitude, cell.longitude};
    if (const auto it = tile_cache_.find(key); it != tile_cache_.end()) {
        tile_order_.remove(key);
        tile_order_.push_front(key);
        return it->second;
    }
    Tile t;
    t.dataset = coverage_.at(cell);
    if (t.dataset == DemDataset::none) {
        // The sea: a grid of zeros the size the band's tiles would have.
        t.latitude_step = 2;
        t.longitude_step = band_multiple_x2(cell.latitude);
        t.rows = units_per_degree / t.latitude_step;
        t.columns = units_per_degree / t.longitude_step;
    } else {
        const std::string name = dem_tile_name(t.dataset, cell);
        t.bytes = tiles_.open(t.dataset, cell);
        try {
            t.tiff = read_geotiff(*t.bytes);
        } catch (const GeoTiffError& e) {
            throw DemError(name + ": " + e.what());
        }
        t.latitude_step = step_units(t.tiff.latitude_step_deg, name);
        t.longitude_step = step_units(t.tiff.longitude_step_deg, name);
        t.rows = t.tiff.images[0].height;
        t.columns = t.tiff.images[0].width;
        // The tile must be the cell, sample for sample: its first sample at the
        // cell's north-west corner, and exactly a degree of samples each way.
        const double north = cell.latitude + 1;
        const double west = cell.longitude;
        if (std::abs(t.tiff.origin_latitude_deg - north) > 1e-9 ||
            std::abs(t.tiff.origin_longitude_deg - west) > 1e-9 ||
            t.rows * t.latitude_step != units_per_degree ||
            t.columns * t.longitude_step != units_per_degree) {
            throw DemError(name + " does not cover its cell sample for sample");
        }
    }
    if (tile_cache_.size() >= max_tiles) {
        const auto oldest = tile_order_.back();
        tile_order_.pop_back();
        tile_cache_.erase(oldest);
        for (auto it = block_cache_.begin(); it != block_cache_.end();) {
            if (it->first.latitude == oldest.first &&
                it->first.longitude == oldest.second) {
                block_order_.remove(it->first);
                it = block_cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
    tile_order_.push_front(key);
    return tile_cache_.emplace(key, std::move(t)).first->second;
}

float Dem::stored_sample(const Tile& t, DemCell cell, std::int64_t row,
                         std::int64_t column) {
    if (t.dataset == DemDataset::none) {
        return 0.0f;
    }
    const RasterImage& image = t.tiff.images[0];
    const auto across = static_cast<std::uint32_t>(column / image.block_width);
    const auto down = static_cast<std::uint32_t>(row / image.block_height);
    const BlockKey key{cell.latitude, cell.longitude,
                       down * image.blocks_across() + across};
    auto it = block_cache_.find(key);
    if (it == block_cache_.end()) {
        std::vector<float> block;
        try {
            block = read_block(*t.bytes, image, across, down);
        } catch (const GeoTiffError& e) {
            throw DemError(dem_tile_name(t.dataset, cell) + ": " + e.what());
        }
        if (block_cache_.size() >= max_blocks) {
            block_cache_.erase(block_order_.back());
            block_order_.pop_back();
        }
        it = block_cache_.emplace(key, std::move(block)).first;
    } else {
        block_order_.remove(key);
    }
    block_order_.push_front(key);
    const float value =
        it->second[static_cast<std::size_t>(row % image.block_height) *
                       image.block_width +
                   static_cast<std::size_t>(column % image.block_width)];
    if (t.tiff.nodata && value == *t.tiff.nodata) {
        return 0.0f;
    }
    return value;
}

double Dem::sample(DemCell cell, std::int64_t row, std::int64_t column, int depth) {
    const Tile& t = tile(cell);
    if (row < t.rows && column < t.columns) {
        return stored_sample(t, cell, row, column);
    }
    // Past the south or east edge: the sample is the next tile's.
    const std::int64_t latitude =
        (cell.latitude + 1) * units_per_degree - row * t.latitude_step;
    const std::int64_t longitude =
        cell.longitude * units_per_degree + column * t.longitude_step;
    return at_units(latitude, longitude, depth + 1);
}

double Dem::at_units(std::int64_t latitude, std::int64_t longitude, int depth) {
    if (depth > 4) {
        throw DemError("the DEM's tiles do not meet"); // a grid this reader cannot join
    }
    const std::int64_t turn = 360 * units_per_degree;
    longitude = ((longitude + 180 * units_per_degree) % turn + turn) % turn -
                180 * units_per_degree;
    latitude = std::clamp(latitude, -90 * units_per_degree, 90 * units_per_degree);
    DemCell cell;
    // A tile holds its north edge and not its south one, so a latitude exactly
    // on a whole degree is the northern row of the tile below it.
    cell.latitude = static_cast<int>(
        std::clamp<std::int64_t>(floor_div(latitude - 1, units_per_degree), -90, 89));
    cell.longitude = static_cast<int>(floor_div(longitude, units_per_degree));
    const Tile& t = tile(cell);
    const auto row =
        static_cast<double>((cell.latitude + 1) * units_per_degree - latitude) /
        static_cast<double>(t.latitude_step);
    const auto column =
        static_cast<double>(longitude - cell.longitude * units_per_degree) /
        static_cast<double>(t.longitude_step);
    return interpolate(cell, row, column, depth);
}

double Dem::interpolate(DemCell cell, double row, double column, int depth) {
    const Tile& t = tile(cell);
    double r0 = std::floor(row);
    const double c0 = std::floor(column);
    double dr = row - r0;
    const double dc = column - c0;
    // Below the last row of the southernmost tiles there is no tile: the South
    // Pole takes that row's heights.
    if (cell.latitude == -90 && r0 >= static_cast<double>(t.rows - 1)) {
        r0 = static_cast<double>(t.rows - 1);
        dr = 0.0;
    }
    const std::array<std::pair<double, std::array<double, 2>>, 4> corners{{
        {(1.0 - dr) * (1.0 - dc), {r0, c0}},
        {(1.0 - dr) * dc, {r0, c0 + 1.0}},
        {dr * (1.0 - dc), {r0 + 1.0, c0}},
        {dr * dc, {r0 + 1.0, c0 + 1.0}},
    }};
    double height = 0.0;
    for (const auto& [weight, at] : corners) {
        // A sample with no weight is not looked at: it may be in a tile that
        // is not needed.
        if (weight != 0.0) {
            height += weight * sample(cell, static_cast<std::int64_t>(at[0]),
                                      static_cast<std::int64_t>(at[1]), depth);
        }
    }
    return height;
}

double Dem::height_above_geoid(double latitude_deg, double longitude_deg) {
    if (!std::isfinite(latitude_deg) || !std::isfinite(longitude_deg)) {
        throw DemError("no height at a position that is not a number");
    }
    const double latitude = std::clamp(latitude_deg, -90.0, 90.0);
    double longitude = std::fmod(longitude_deg + 180.0, 360.0);
    if (longitude < 0.0) {
        longitude += 360.0;
    }
    longitude -= 180.0;
    DemCell cell;
    // As in at_units: on a whole degree, the tile below.
    cell.latitude = std::clamp(static_cast<int>(std::ceil(latitude)) - 1, -90, 89);
    cell.longitude = std::min(static_cast<int>(std::floor(longitude)), 179);
    const Tile& t = tile(cell);
    const double row = (static_cast<double>(cell.latitude + 1) - latitude) *
                       static_cast<double>(units_per_degree) /
                       static_cast<double>(t.latitude_step);
    const double column = (longitude - static_cast<double>(cell.longitude)) *
                          static_cast<double>(units_per_degree) /
                          static_cast<double>(t.longitude_step);
    return interpolate(cell, row, column, 0);
}

double Dem::height_above_ellipsoid(double latitude_deg, double longitude_deg) {
    if (geoid_ == nullptr) {
        throw DemError("heights above the ellipsoid need the geoid");
    }
    return height_above_geoid(latitude_deg, longitude_deg) +
           geoid_->undulation(latitude_deg, longitude_deg);
}

} // namespace glideslope::world
