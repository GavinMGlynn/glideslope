#include "harness.hpp"
#include "tiff_writer.hpp"

#include "world/dem.hpp"
#include "world/geoid.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::Dem;
using glideslope::world::DemCell;
using glideslope::world::DemCoverage;
using glideslope::world::DemDataset;
using glideslope::world::DemError;
using glideslope::world::DemTiles;
using glideslope::world::MemorySource;

namespace {

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    check(static_cast<bool>(in), "can read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), {});
}

// Coverage text with the given cells marked, the rest sea.
std::string coverage_text(const std::map<std::pair<int, int>, char>& cells) {
    std::string text = "# a test coverage\n";
    for (int lat = 89; lat >= -90; --lat) {
        const int degrees = std::abs(lat);
        text += lat >= 0 ? 'N' : 'S';
        text += (degrees < 10 ? "0" : "") + std::to_string(degrees) + " ";
        for (int lon = -180; lon < 180; ++lon) {
            const auto it = cells.find({lat, lon});
            text += it == cells.end() ? '0' : it->second;
        }
        text += '\n';
    }
    return text;
}

// A small tile for a cell: `rows` by `columns` samples, spaced to fill the
// degree, each the height `height` gives at its latitude and longitude.
std::vector<std::uint8_t>
make_tile(DemCell cell, std::uint32_t rows, std::uint32_t columns,
          const std::function<double(double, double)>& height) {
    glideslope::test::tiff::Spec spec;
    spec.width = columns;
    spec.height = rows;
    spec.block = 3; // several blocks, so edges between them are crossed
    spec.deflate = true;
    spec.float_predictor = true;
    spec.overview = false;
    spec.latitude_step = 1.0 / rows;
    spec.longitude_step = 1.0 / columns;
    spec.origin_latitude = cell.latitude + 1;
    spec.origin_longitude = cell.longitude;
    std::vector<float> values;
    for (std::uint32_t r = 0; r < rows; ++r) {
        for (std::uint32_t c = 0; c < columns; ++c) {
            values.push_back(static_cast<float>(
                height(spec.origin_latitude - r * spec.latitude_step,
                       spec.origin_longitude + c * spec.longitude_step)));
        }
    }
    return glideslope::test::tiff::write_tiff(spec, values, {});
}

// A small water body mask for a cell, as make_tile's grid: each sample's value
// `value` gives from its row and column.
std::vector<std::uint8_t>
make_mask(DemCell cell, std::uint32_t rows, std::uint32_t columns,
          const std::function<int(std::uint32_t, std::uint32_t)>& value) {
    glideslope::test::tiff::Spec spec;
    spec.bits = 8;
    spec.width = columns;
    spec.height = rows;
    spec.block = 3;
    spec.deflate = true;
    spec.differencing = true;
    spec.overview = false;
    spec.latitude_step = 1.0 / rows;
    spec.longitude_step = 1.0 / columns;
    spec.origin_latitude = cell.latitude + 1;
    spec.origin_longitude = cell.longitude;
    std::vector<float> values;
    for (std::uint32_t r = 0; r < rows; ++r) {
        for (std::uint32_t c = 0; c < columns; ++c) {
            values.push_back(static_cast<float>(value(r, c)));
        }
    }
    return glideslope::test::tiff::write_tiff(spec, values, {});
}

class MemoryTiles : public DemTiles {
public:
    std::map<std::pair<int, int>, std::vector<std::uint8_t>> tiles;
    std::map<std::pair<int, int>, std::vector<std::uint8_t>> masks;
    std::set<std::pair<int, int>> opened;

    std::shared_ptr<const glideslope::world::ByteSource> open(DemDataset,
                                                              DemCell cell) override {
        const auto it = tiles.find({cell.latitude, cell.longitude});
        if (it == tiles.end()) {
            throw DemError("no test tile for " + std::to_string(cell.latitude) + ", " +
                           std::to_string(cell.longitude));
        }
        opened.insert({cell.latitude, cell.longitude});
        return std::make_shared<MemorySource>(it->second);
    }

    std::shared_ptr<const glideslope::world::ByteSource>
    open_water_mask(DemDataset, DemCell cell) override {
        const auto it = masks.find({cell.latitude, cell.longitude});
        if (it == masks.end()) {
            throw DemError("no test mask for " + std::to_string(cell.latitude) + ", " +
                           std::to_string(cell.longitude));
        }
        return std::make_shared<MemorySource>(it->second);
    }
};

bool near(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) <= tolerance;
}

} // namespace

GLIDESLOPE_TEST(the_dem_coverage_file_marks_every_cell_the_tile_lists_have) {
    const DemCoverage coverage(
        read_text(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                  "../assets/dem/coverage.txt"));
    check(coverage.count(DemDataset::glo30) == 26450, "26450 cells at 30 m");
    check(coverage.count(DemDataset::glo90) == 25, "25 cells at 90 m only");
    check(coverage.count(DemDataset::none) == 64800 - 26475, "the rest are sea");
    check(coverage.at({-34, 151}) == DemDataset::glo30, "Sydney's tile is there");
    check(coverage.at({-34, 152}) == DemDataset::none,
          "the Tasman Sea east of it is not");
    check(coverage.at({39, 45}) == DemDataset::glo90, "Armenia has only a 90 m tile");
    check(coverage.at({0, 6}) == DemDataset::glo30, "the first tile in the list");

    bool refused = false;
    try {
        DemCoverage broken("N89 0\n");
    } catch (const DemError&) {
        refused = true;
    }
    check(refused, "a malformed coverage is refused");
}

GLIDESLOPE_TEST(dem_tiles_are_named_and_found_as_the_buckets_name_them) {
    using glideslope::world::dem_tile_name;
    using glideslope::world::dem_tile_url;
    check(dem_tile_name(DemDataset::glo30, {-34, 151}) ==
              "Copernicus_DSM_COG_10_S34_00_E151_00_DEM",
          "south and east");
    check(dem_tile_name(DemDataset::glo30, {0, -1}) ==
              "Copernicus_DSM_COG_10_N00_00_W001_00_DEM",
          "the equator and just west of Greenwich");
    check(dem_tile_name(DemDataset::glo90, {-90, -180}) ==
              "Copernicus_DSM_COG_30_S90_00_W180_00_DEM",
          "the south-west corner of the world, at 90 m");
    check(dem_tile_name(DemDataset::glo30, {89, 179}) ==
              "Copernicus_DSM_COG_10_N89_00_E179_00_DEM",
          "the north-east corner");
    check(dem_tile_url(DemDataset::glo30, {-34, 151}) ==
              "https://copernicus-dem-30m.s3.amazonaws.com/"
              "Copernicus_DSM_COG_10_S34_00_E151_00_DEM/"
              "Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif",
          "the URL is the bucket's");
    check(dem_tile_url(DemDataset::glo90, {39, 45})
              .starts_with("https://copernicus-dem-90m."),
          "90 m tiles come from the 90 m bucket");
    check(glideslope::world::dem_water_mask_name(DemDataset::glo90, {-90, -180}) ==
              "Copernicus_DSM_COG_30_S90_00_W180_00_WBM",
          "a mask is named for its tile");
    check(glideslope::world::dem_water_mask_url(DemDataset::glo30, {-34, 151}) ==
              "https://copernicus-dem-30m.s3.amazonaws.com/"
              "Copernicus_DSM_COG_10_S34_00_E151_00_DEM/AUXFILES/"
              "Copernicus_DSM_COG_10_S34_00_E151_00_WBM.tif",
          "a mask's URL is beside its tile's, in AUXFILES");
}

GLIDESLOPE_TEST(the_dem_surface_is_continuous_across_tiles_resolutions_and_bands) {
    // A plane: bilinear interpolation of a linear height is exact, so every
    // height asked for is known, wherever its four samples come from.
    const auto plane = [](double lat, double lon) {
        return 100.0 + 20.0 * lat + 7.0 * lon;
    };
    std::map<std::pair<int, int>, char> cells;
    MemoryTiles tiles;
    for (int lat = 48; lat <= 52; ++lat) {
        for (int lon = 9; lon <= 13; ++lon) {
            cells[{lat, lon}] = '2';
            // North of 50, samples further apart in longitude, as Copernicus
            // spaces them; one cell coarser still, as a 90 m tile among 30 m.
            const std::uint32_t columns = lat >= 50 ? 3 : 4;
            const std::uint32_t rows = (lat == 50 && lon == 11) ? 2 : 4;
            tiles.tiles[{lat, lon}] = make_tile(
                {lat, lon}, rows, lat == 50 && lon == 11 ? 2 : columns, plane);
        }
    }
    const DemCoverage coverage(coverage_text(cells));
    Dem dem(coverage, tiles, nullptr);

    int checked = 0;
    for (double lat = 49.0; lat < 51.0; lat += 0.0625 + 1.0 / 7919.0) {
        for (double lon = 10.0; lon < 12.0; lon += 0.0625 + 1.0 / 7907.0) {
            const double got = dem.height_above_geoid(lat, lon);
            if (!near(got, plane(lat, lon), 1e-3)) {
                fail("at " + std::to_string(lat) + ", " + std::to_string(lon) + ": " +
                     std::to_string(got) + " rather than " +
                     std::to_string(plane(lat, lon)));
            }
            ++checked;
        }
    }
    // Exactly on the edges and corners where tiles meet.
    for (const double lat : {49.0, 50.0, 51.0}) {
        for (const double lon : {10.0, 11.0, 12.0}) {
            check(near(dem.height_above_geoid(lat, lon), plane(lat, lon), 1e-3),
                  "exactly on a tile corner");
            ++checked;
        }
    }
    check(checked > 1000, "over a thousand heights were checked");
}

GLIDESLOPE_TEST(
    the_dem_surface_is_continuous_across_the_antimeridian_and_to_the_poles) {
    const auto by_latitude = [](double lat, double) { return 50.0 + 10.0 * lat; };
    // Land from 2 S to 1 N either side of the antimeridian, so every sample
    // around the latitudes asked for is land.
    std::map<std::pair<int, int>, char> cells{
        {{0, 179}, '2'},  {{0, -180}, '2'},  {{-1, 179}, '2'}, {{-1, -180}, '2'},
        {{-2, 179}, '2'}, {{-2, -180}, '2'}, {{89, 0}, '2'},   {{89, 1}, '2'},
        {{-90, 0}, '2'},  {{-90, 1}, '2'}};
    MemoryTiles tiles;
    for (const auto& [cell, kind] : cells) {
        const bool pole = cell.first == 89 || cell.first == -90;
        tiles.tiles[cell] =
            make_tile({cell.first, cell.second}, 4, 4,
                      pole ? [](double lat, double) { return lat > 0 ? 5.0 : 7.0; }
                           : std::function<double(double, double)>(by_latitude));
    }
    const DemCoverage coverage(coverage_text(cells));
    Dem dem(coverage, tiles, nullptr);

    for (const double lon :
         {179.5, 179.9, 179.999999, 180.0, -180.0, -179.9, 540.0, -540.0}) {
        for (const double lat : {-0.9, -0.5, 0.0, 0.3, 0.99}) {
            check(near(dem.height_above_geoid(lat, lon), by_latitude(lat, lon), 1e-3),
                  "at " + std::to_string(lat) + ", " + std::to_string(lon));
        }
    }
    check(near(dem.height_above_geoid(90.0, 0.5), 5.0), "the North Pole");
    check(near(dem.height_above_geoid(89.9, 1.5), 5.0), "near the North Pole");
    check(near(dem.height_above_geoid(-90.0, 0.5), 7.0), "the South Pole");
    check(near(dem.height_above_geoid(-89.99, 1.2), 7.0), "near the South Pole");
    check(near(dem.height_above_geoid(-95.0, 0.5), 7.0),
          "past the South Pole is clamped to it");
}

GLIDESLOPE_TEST(the_dem_says_each_place_is_the_water_of_its_nearest_mask_sample) {
    using glideslope::world::Water;
    // Each sample's value from its row and column, so which sample was read
    // is known from what it says.
    const auto by_sample = [](std::uint32_t r, std::uint32_t c) {
        return static_cast<int>((r + 2 * c) % 4);
    };
    std::map<std::pair<int, int>, char> cells{
        {{10, 20}, '2'}, {{9, 20}, '2'}, {{10, 21}, '1'}};
    MemoryTiles tiles;
    tiles.masks[{10, 20}] = make_mask({10, 20}, 4, 4, by_sample);
    tiles.masks[{9, 20}] = make_mask({9, 20}, 4, 4, [](auto, auto c) {
        return c == 2 ? 3 : 0;
    });
    // A 90 m tile, coarser.
    tiles.masks[{10, 21}] = make_mask({10, 21}, 2, 2, [](auto r, auto) {
        return r == 0 ? 2 : 1;
    });
    const DemCoverage coverage(coverage_text(cells));
    Dem dem(coverage, tiles, nullptr);

    const auto expect = [](std::uint32_t r, std::uint32_t c) {
        return static_cast<Water>((r + 2 * c) % 4);
    };
    int checked = 0;
    for (std::uint32_t r = 0; r < 4; ++r) {
        for (std::uint32_t c = 0; c < 4; ++c) {
            const double lat = 11.0 - r * 0.25;
            const double lon = 20.0 + c * 0.25;
            // On the sample, and anywhere nearer it than any other.
            for (const double dlat : {0.0, 0.12, -0.12}) {
                for (const double dlon : {0.0, 0.12, -0.12}) {
                    if ((r == 0 && dlat > 0) || (c == 0 && dlon < 0)) {
                        continue; // nearer another tile's sample
                    }
                    check(dem.water(lat + dlat, lon + dlon) == expect(r, c),
                          "at " + std::to_string(lat + dlat) + ", " +
                              std::to_string(lon + dlon) + ", the sample in row " +
                              std::to_string(r) + " and column " + std::to_string(c));
                    ++checked;
                }
            }
        }
    }
    check(checked == 16 * 9 - 4 * 3 - 4 * 3 + 1, "every sample was asked about");
    // Nearer the south or east edge than the last sample: the next tile's.
    check(dem.water(10.1, 20.5) == Water::river,
          "south of the last row, the tile below's first row");
    check(dem.water(10.1, 20.25) == Water::none, "and another column of it");
    check(dem.water(10.9, 20.9) == Water::lake,
          "east of the last column, the next tile's first column, at 90 m");
    check(dem.water(10.4, 20.9) == Water::ocean, "and its other row");
    check(dem.water(10.5, 25.0) == Water::ocean, "out at sea, where there is no tile");
    check(dem.water(10.5, 20.5 + 360.0) == expect(2, 2), "a turn of the world east");

    // A mask with a value the handbook does not give, or heights in its place,
    // is refused rather than read as some water.
    const auto refusal = [](const std::vector<std::uint8_t>& mask) {
        std::map<std::pair<int, int>, char> one{{{0, 0}, '2'}};
        MemoryTiles wrong;
        wrong.masks[{0, 0}] = mask;
        const DemCoverage one_coverage(coverage_text(one));
        Dem bad(one_coverage, wrong, nullptr);
        try {
            bad.water(0.5, 0.5);
        } catch (const DemError& e) {
            return std::string(e.what());
        }
        return std::string();
    };
    check(refusal(make_mask({0, 0}, 4, 4, [](auto, auto) { return 7; }))
                  .find("holds 7, which is no water body") != std::string::npos,
          "a value past river is refused");
    check(refusal(make_tile({0, 0}, 4, 4, [](double, double) { return 1.0; }))
                  .find("32-bit samples, not a mask") != std::string::npos,
          "heights are refused as a mask");
}

GLIDESLOPE_TEST(the_dem_meets_the_sea_at_zero_between_the_last_sample_and_the_coast) {
    const auto flat = [](double, double) { return 40.0; };
    std::map<std::pair<int, int>, char> cells{{{10, 20}, '2'}};
    MemoryTiles tiles;
    tiles.tiles[{10, 20}] = make_tile({10, 20}, 4, 4, flat);
    const DemCoverage coverage(coverage_text(cells));
    Dem dem(coverage, tiles, nullptr);

    check(near(dem.height_above_geoid(10.5, 20.5), 40.0), "inland");
    check(near(dem.height_above_geoid(10.5, 20.875), 20.0),
          "halfway from the last column to the sea to the east");
    check(near(dem.height_above_geoid(10.5, 21.0), 0.0), "at the coast, the sea");
    check(near(dem.height_above_geoid(10.5, 25.0), 0.0), "out at sea");
    check(near(dem.height_above_geoid(10.125, 20.5), 20.0),
          "halfway from the last row to the sea to the south");
    check(tiles.opened == std::set<std::pair<int, int>>{{10, 20}},
          "only the land tile was opened");

    // A tile that does not fill its cell is refused rather than misplaced.
    std::map<std::pair<int, int>, char> bad{{{0, 0}, '2'}};
    MemoryTiles wrong;
    wrong.tiles[{0, 0}] = make_tile({1, 0}, 4, 4, flat);
    const DemCoverage bad_coverage(coverage_text(bad));
    Dem bad_dem(bad_coverage, wrong, nullptr);
    bool refused = false;
    try {
        bad_dem.height_above_geoid(0.5, 0.5);
    } catch (const DemError& e) {
        refused =
            std::string(e.what()).find("does not cover its cell") != std::string::npos;
    }
    check(refused, "a tile for the wrong cell is refused");
}

GLIDESLOPE_TEST(
    the_dem_gives_the_sydney_tiles_heights_at_its_samples_and_between_them) {
    const std::filesystem::path downloads(GLIDESLOPE_TEST_DOWNLOADS_DIR);
    const std::filesystem::path tile =
        downloads / "Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif";
    const std::filesystem::path geoid_zip = downloads / "egm2008-5.zip";
    if (!std::filesystem::exists(tile) || !std::filesystem::exists(geoid_zip)) {
        glideslope::test::skip("the Sydney tile or the geoid was not fetched");
    }
    const DemCoverage coverage(
        read_text(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                  "../assets/dem/coverage.txt"));
    glideslope::world::DirectoryTiles tiles(downloads);
    const glideslope::world::Geoid geoid =
        glideslope::world::load_geoid_zip(glideslope::world::FileSource(geoid_zip));
    Dem dem(coverage, tiles, &geoid);

    const auto at = [](double column, double row) {
        return std::pair{-33.0 - row / 3600.0, 151.0 + column / 3600.0};
    };
    // The independent decoder's values (test_geotiff.cpp) at their samples.
    const std::vector<std::tuple<double, double, double>> samples{
        {0, 0, 274.5896911621094},       {1023, 0, 199.3792266845703},
        {1024, 0, 195.0670623779297},    {100, 1500, 1.8994510173797607},
        {1500, 500, 27.805686950683594}, {900, 2400, 119.47073364257812},
        {1024, 1024, 76.25465393066406}};
    for (const auto& [column, row, height] : samples) {
        const auto [lat, lon] = at(column, row);
        check(near(dem.height_above_geoid(lat, lon), height, 1e-3),
              "sample (" + std::to_string(column) + ", " + std::to_string(row) + ")");
    }
    const auto [lat, lon] = at(1023.5, 0);
    check(near(dem.height_above_geoid(lat, lon),
               (199.3792266845703 + 195.0670623779297) / 2, 1e-3),
          "halfway between two samples in different internal tiles");
    check(near(dem.height_above_ellipsoid(lat, lon),
               dem.height_above_geoid(lat, lon) + geoid.undulation(lat, lon), 1e-9),
          "above the ellipsoid is above the geoid plus the undulation");
    check(near(dem.height_above_geoid(-33.5, 152.5), 0.0),
          "the Tasman Sea, where there is no tile");
}
