#include "harness.hpp"

#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/geoid.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::test::fail;

namespace {

struct Surveyed {
    std::string kind;
    std::string name;
    double latitude = 0.0;
    double longitude = 0.0;
    double height = 0.0;
    double dem = 0.0; // what the DEM gives there, above the geoid
};

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    check(static_cast<bool>(in), "can read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), {});
}

// Every point of tests/data/dem/surveyed.txt with the DEM's height there,
// fetching the tiles it needs. Skipped without the network, unless required.
std::vector<Surveyed> measure() {
    const std::filesystem::path source(GLIDESLOPE_TEST_SOURCE_DIR);
    const std::filesystem::path downloads(GLIDESLOPE_TEST_DOWNLOADS_DIR);
    std::vector<Surveyed> points;
    std::istringstream lines(read_text(source / "data/dem/surveyed.txt"));
    for (std::string line; std::getline(lines, line);) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::istringstream fields(line);
        Surveyed s;
        if (!(fields >> s.kind >> s.name >> s.latitude >> s.longitude >> s.height)) {
            fail("a malformed line in surveyed.txt: " + line);
        }
        points.push_back(s);
    }

    const glideslope::world::DemCoverage coverage(
        read_text(source / "../assets/dem/coverage.txt"));
    const glideslope::world::Fetch fetch = glideslope::world::http_fetch();
    glideslope::world::DownloadedTiles tiles(downloads, fetch);
    try {
        const glideslope::world::Geoid geoid =
            glideslope::world::egm2008_geoid(downloads, fetch);
        glideslope::world::Dem dem(coverage, tiles, &geoid);
        for (Surveyed& s : points) {
            s.dem = dem.height_above_geoid(s.latitude, s.longitude);
        }
    } catch (const glideslope::world::DemError& e) {
        if (!glideslope::test::network_required()) {
            glideslope::test::skip(std::string("the tiles could not be had: ") +
                                   e.what());
        }
        throw;
    }
    std::printf("%-10s %-20s %10s %10s %8s\n", "kind", "name", "surveyed", "DEM",
                "DEM-surv");
    for (const Surveyed& s : points) {
        std::printf("%-10s %-20s %10.2f %10.2f %+8.2f\n", s.kind.c_str(),
                    s.name.c_str(), s.height, s.dem, s.dem - s.height);
    }
    return points;
}

} // namespace

GLIDESLOPE_TEST(
    the_dem_meets_surveyed_airfields_and_coastlines_within_its_stated_accuracy) {
    // The Copernicus DEM's stated absolute vertical accuracy: under 4 m, as a
    // 90% linear error (the product handbook; docs/ASSETS.md). Every airfield
    // and coastline here is held to it, not 90% of them.
    int airfields = 0;
    int water = 0;
    for (const Surveyed& s : measure()) {
        if (s.kind == "summit") {
            continue;
        }
        check(std::abs(s.dem - s.height) <= 4.0,
              s.name + ": the DEM gives " + std::to_string(s.dem) + " m, surveyed " +
                  std::to_string(s.height) + " m");
        airfields += s.kind == "airfield" ? 1 : 0;
        water += s.kind == "water" ? 1 : 0;
    }
    check(airfields == 12 && water == 5,
          "12 runway ends and 5 coastal waters were measured");
}

GLIDESLOPE_TEST(the_dem_holds_summits_below_their_surveyed_heights_by_its_resolution) {
    // A summit is a point, and a 30 m grid of a radar surface does not sample
    // it: the DEM's highest value near each of these lies 5 to 21 m below the
    // survey, and the height interpolated at the station itself 8 to 35 m
    // below (docs/PROJECT_STATUS.md). This pins that: never above the survey by
    // more than the stated accuracy, and never further below than 40 m, so a
    // change that moves summits is seen.
    int summits = 0;
    for (const Surveyed& s : measure()) {
        if (s.kind != "summit") {
            continue;
        }
        const double error = s.dem - s.height;
        check(error <= 4.0 && error >= -40.0,
              s.name + ": the DEM gives " + std::to_string(s.dem) + " m, surveyed " +
                  std::to_string(s.height) + " m");
        ++summits;
    }
    check(summits == 5, "five summits were measured");
}
