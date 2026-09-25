#include "harness.hpp"

#include "world/runways.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::world::read_runways;
using glideslope::world::RunwayEnd;

namespace {

const std::string header =
    "\"id\",\"airport_ref\",\"airport_ident\",\"length_ft\",\"width_ft\",\"surface\","
    "\"lighted\",\"closed\",\"le_ident\",\"le_latitude_deg\",\"le_longitude_deg\","
    "\"le_elevation_ft\",\"le_heading_degT\",\"le_displaced_threshold_ft\",\"he_ident\","
    "\"he_latitude_deg\",\"he_longitude_deg\",\"he_elevation_ft\",\"he_heading_degT\","
    "\"he_displaced_threshold_ft\"\n";

bool refused(const std::string& text, const std::string& says) {
    try {
        (void)read_runways(text);
    } catch (const glideslope::world::RunwayError& e) {
        return std::string(e.what()).find(says) != std::string::npos;
    }
    return false;
}

} // namespace

GLIDESLOPE_TEST(a_runways_file_gives_each_open_runway_end_with_a_place_and_a_heading) {
    // One of each kind of line, as OurAirports writes them.
    const std::string csv =
        header +
        // Both ends, one with no elevation, and a surface quoted with a comma.
        "233267,27145,\"YSSY\",12999,148,\"ASP, grooved\",1,0,\"16R\",-33.9294,151.172,8,"
        "168,279,\"34L\",-33.9643,151.181,,348,\r\n"
        // A helipad: no runway to take off along.
        "269408,6523,\"00A\",80,80,\"ASPH-G\",1,0,\"H1\",-33.1,151.1,,90,,,,,,,\n"
        // Closed.
        "1,2,\"XXXX\",5000,100,\"ASP\",0,1,\"09\",-33.2,151.2,10,90,,\"27\",-33.2,151.25,10,"
        "270,\n"
        // No place for either end.
        "255155,6524,\"00AK\",2500,40,\"GRVL\",0,0,\"N\",,,,,,\"S\",,,,,\n"
        // A place and no heading at one end.
        "3,4,\"YYYY\",3000,60,\"GRS\",0,0,\"18\",-34.0,150.0,300,,,\"36\",-34.01,150.0,300,"
        "0,\n";
    const std::vector<RunwayEnd> ends = read_runways(csv);
    check(ends.size() == 3, "three ends kept of the ten: " + std::to_string(ends.size()));
    check(ends[0].airport == "YSSY" && ends[0].ident == "16R" &&
              ends[0].latitude_deg == -33.9294 && ends[0].longitude_deg == 151.172 &&
              ends[0].elevation_ft == 8.0 && ends[0].heading_deg == 168.0 &&
              std::abs(ends[0].length_m - 12999 * 0.3048) < 1e-9 &&
              ends[0].surface == "ASP, grooved",
          "Sydney's 16R, every field");
    check(ends[1].ident == "34L" && std::isnan(ends[1].elevation_ft) &&
              ends[1].heading_deg == 348.0,
          "its other end, 34L, with no elevation given");
    check(ends[2].airport == "YYYY" && ends[2].ident == "36" && ends[2].heading_deg == 0.0,
          "of a runway with one end's heading missing, the other end");
    check(glideslope::world::runways_at(ends, "YSSY").size() == 2 &&
              glideslope::world::runways_at(ends, "XXXX").empty(),
          "looked up by airport");
    const glideslope::sim::Runway r = glideslope::world::as_runway(ends[1], 21.0);
    check(r.name == "YSSY 34L" && r.threshold_lat_deg == -33.9643 &&
              r.elevation_ft == 21.0 && r.heading_deg == 348.0,
          "as the take-off autopilot has a runway, at the ground's height given");

    check(refused("", "empty"), "an empty file");
    check(refused("\"id\",\"airport_ident\"\n", "no column"), "a file that is not runways");
}

GLIDESLOPE_TEST(the_pinned_runways_file_holds_sydneys_six_runway_ends_where_they_are) {
    const std::filesystem::path path =
        std::filesystem::path(GLIDESLOPE_TEST_DOWNLOADS_DIR) / "ourairports-runways.csv";
    if (!std::filesystem::exists(path)) {
        glideslope::test::skip(path.string() + " was not fetched");
    }
    std::ifstream in(path, std::ios::binary);
    const std::vector<RunwayEnd> all =
        read_runways(std::string(std::istreambuf_iterator<char>(in), {}));
    std::fprintf(stderr, "%zu runway ends\n", all.size());
    // Counted from the pinned file.
    check(all.size() == 27116,
          "every usable end in the pinned file: " + std::to_string(all.size()));
    const std::vector<RunwayEnd> sydney = glideslope::world::runways_at(all, "YSSY");
    std::vector<std::string> idents;
    for (const RunwayEnd& r : sydney) {
        idents.push_back(r.ident);
    }
    check(idents == std::vector<std::string>{"07", "25", "16L", "34R", "16R", "34L"},
          "Sydney's six ends, in the file's order");
    // 34L as the pinned file has it, which is what this pins; and every end
    // within 4 km - its longest runway's length - of the airport's reference
    // point, -33.946111 151.177222 (as metar-taf.com and SkyVector list it),
    // which is what says the file's places are Sydney's.
    const RunwayEnd& l34 = sydney[5];
    check(l34.latitude_deg == -33.964298248291016 && l34.longitude_deg == 151.18099975585938 &&
              l34.heading_deg == 348.0 && std::abs(l34.length_m - 12999 * 0.3048) < 1e-9,
          "34L where the pinned file has it, facing 348 true, 3.96 km long");
    for (const RunwayEnd& r : sydney) {
        const double north_m = (r.latitude_deg - -33.946111) * 111320.0;
        const double east_m =
            (r.longitude_deg - 151.177222) * 111320.0 * std::cos(33.946111 * 3.14159265358979 / 180.0);
        check(std::hypot(north_m, east_m) < 4000.0,
              r.ident + " within 4 km of Sydney's reference point: " +
                  std::to_string(std::hypot(north_m, east_m)) + " m");
    }
}
