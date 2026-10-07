#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/crash.hpp"
#include "sim/controller.hpp"
#include "sim/departure.hpp"
#include "sim/figures.hpp"
#include "sim/navigator.hpp"
#include "sim/plan.hpp"
#include "sim/fixed_step.hpp"
#include "sim/terrain.hpp"
#include "net/protocol.hpp"
#include "world/dem.hpp"
#include "world/digest.hpp"
#include "world/download.hpp"
#include "world/geodesy.hpp"
#include "world/geoid.hpp"
#include "world/runway_ground.hpp"
#include "world/runways.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <map>
#include <stdexcept>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::world::CollisionGround;
using glideslope::world::RunwayStrip;
using glideslope::world::RunwaySurfaces;

namespace {

constexpr double feet_per_metre = 3.280839895013123;
// **How far one runway may pull another's surface off its line**, anywhere in
// the world, by the measurement below (the owner's decision of 2026-10-07,
// REQUIREMENTS.md section 9): off the overlaps - wherever no other runway's
// pavement, or the `runway_overlap_band_m` round it, covers the place - 0.1 m.
// On the overlaps, where two lines sloping differently cannot agree over an
// area and the ground is their mean, what it is is measured and held: under
// rules 2 the worst 0.61 m, 12 runways over 0.3 m and 262 over 0.1 m; under
// rules 1, everywhere, they were 0.68 m, 38 and 572.
constexpr double off_overlap_pull_bound_m = 0.1;
constexpr double overlap_pull_bound_m = 0.62;
constexpr std::ptrdiff_t overlap_over_0_3_m = 12;
constexpr std::ptrdiff_t overlap_over_0_1_m = 262;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// **The ground as every program has it**: the DEM's tiles and the geoid from
// the downloads directory, fetched there if they are not yet, and the runway
// strips the build carries.
struct Ground {
    std::unique_ptr<glideslope::world::DemCoverage> coverage;
    std::unique_ptr<glideslope::world::DownloadedTiles> tiles;
    std::unique_ptr<glideslope::world::Geoid> geoid;
    std::shared_ptr<glideslope::world::Dem> dem;
    std::shared_ptr<const RunwaySurfaces> runways;
    std::shared_ptr<CollisionGround> collision;
};

Ground open_ground() {
    const std::filesystem::path source(GLIDESLOPE_TEST_SOURCE_DIR);
    const std::filesystem::path downloads(GLIDESLOPE_TEST_DOWNLOADS_DIR);
    std::ifstream coverage_file(source / "../assets/dem/coverage.txt", std::ios::binary);
    Ground g;
    g.coverage = std::make_unique<glideslope::world::DemCoverage>(
        std::string(std::istreambuf_iterator<char>(coverage_file), {}));
    const glideslope::world::Fetch fetch = glideslope::world::http_fetch();
    g.tiles = std::make_unique<glideslope::world::DownloadedTiles>(downloads, fetch);
    try {
        g.geoid = std::make_unique<glideslope::world::Geoid>(
            glideslope::world::egm2008_geoid(downloads, fetch));
    } catch (const std::exception& e) {
        if (!glideslope::test::network_required()) {
            glideslope::test::skip(std::string("the geoid could not be had: ") + e.what());
        }
        throw;
    }
    g.runways = glideslope::world::runway_surfaces(data());
    g.dem = std::make_shared<glideslope::world::Dem>(*g.coverage, *g.tiles, g.geoid.get());
    g.collision = std::make_shared<CollisionGround>(g.dem, g.runways);
    return g;
}

std::size_t runway_named(const RunwaySurfaces& runways, const std::string& airport,
                         const std::string& le, const std::string& he) {
    for (std::size_t i = 0; i < runways.size(); ++i) {
        const RunwayStrip& s = runways.at(i).strip;
        if (s.airport == airport && s.le_ident == le && s.he_ident == he) {
            return i;
        }
    }
    glideslope::test::fail("the runways file has no " + airport + " " + le + "/" + he);
}

// **A DEM of nothing but sea**: no cell has a tile, so every height is the
// sea's, zero, and nothing is fetched. Where synthetic runways are laid.
std::shared_ptr<glideslope::world::Dem> sea() {
    struct Sea {
        glideslope::world::DemCoverage coverage;
        glideslope::world::DirectoryTiles tiles;
        glideslope::world::Dem dem;
        static std::string text() {
            std::string t;
            for (int lat = 89; lat >= -90; --lat) {
                const int degrees = lat >= 0 ? lat : -lat;
                t += (lat >= 0 ? "N" : "S") + std::string(degrees < 10 ? "0" : "") +
                     std::to_string(degrees) + " " + std::string(360, '0') + "\n";
            }
            return t;
        }
        Sea()
            : coverage(text()), tiles(std::filesystem::temp_directory_path()),
              dem(coverage, tiles, nullptr) {}
    };
    const auto sea = std::make_shared<Sea>();
    return std::shared_ptr<glideslope::world::Dem>(sea, &sea->dem);
}

// A runway `length_m` long from (`latitude_deg`, `longitude_deg`) along true
// `heading_deg`, on a sphere, its ends at the elevations given, 45 m wide.
RunwayStrip laid(const std::string& name, double latitude_deg, double longitude_deg,
                 double heading_deg, double length_m, double le_ft, double he_ft) {
    constexpr double radians = 3.14159265358979323846 / 180.0;
    const double d = length_m / 6371008.8;
    const double p1 = latitude_deg * radians;
    const double h = heading_deg * radians;
    const double p2 =
        std::asin(std::sin(p1) * std::cos(d) + std::cos(p1) * std::sin(d) * std::cos(h));
    const double l2 = longitude_deg * radians +
                      std::atan2(std::sin(h) * std::sin(d) * std::cos(p1),
                                 std::cos(d) - std::sin(p1) * std::sin(p2));
    RunwayStrip s;
    s.airport = name;
    s.le_ident = "LE";
    s.he_ident = "HE";
    s.le_latitude_deg = latitude_deg;
    s.le_longitude_deg = longitude_deg;
    s.he_latitude_deg = p2 / radians;
    s.he_longitude_deg = std::remainder(l2 / radians, 360.0);
    s.le_elevation_ft = le_ft;
    s.he_elevation_ft = he_ft;
    s.width_m = 45.0;
    return s;
}

// A place `east_m` east and `north_m` north of (`latitude_deg`,
// `longitude_deg`), on a sphere.
std::pair<double, double> offset(double latitude_deg, double longitude_deg, double east_m,
                                 double north_m) {
    const RunwayStrip s = laid("", latitude_deg, longitude_deg,
                               std::atan2(east_m, north_m) * 180.0 / 3.14159265358979323846,
                               std::hypot(east_m, north_m), 0.0, 0.0);
    return {s.he_latitude_deg, s.he_longitude_deg};
}

// A place along runway `i`'s centreline - `t` 0 at its le end, 1 at its he
// end - and `across_m` to the right of it, looking from le to he.
glideslope::world::Geodetic on_runway(const RunwaySurfaces& runways, std::size_t i, double t,
                                      double across_m) {
    const RunwayStrip& s = runways.at(i).strip;
    const glideslope::world::Ecef le = glideslope::world::to_ecef({s.le_latitude_deg, s.le_longitude_deg, 0.0});
    const glideslope::world::Ecef he = glideslope::world::to_ecef({s.he_latitude_deg, s.he_longitude_deg, 0.0});
    glideslope::world::Geodetic g = glideslope::world::to_geodetic(
        {le.x + t * (he.x - le.x), le.y + t * (he.y - le.y), le.z + t * (he.z - le.z)});
    if (across_m != 0.0) {
        const RunwaySurfaces::Runway& r = runways.at(i);
        // Right of the direction (along_x, along_y) in the east-north frame.
        const double east = r.along_y * across_m;
        const double north = -r.along_x * across_m;
        const glideslope::world::Ecef at = glideslope::world::to_ecef(g);
        g = glideslope::world::to_geodetic({at.x + r.east[0] * east + r.north[0] * north,
                                            at.y + r.east[1] * east + r.north[1] * north,
                                            at.z + r.east[2] * east + r.north[2] * north});
    }
    return g;
}

// **What a runway's ground does along its centreline**, every metre end to
// end: how far it strays from the straight line between its ends' heights, and
// the largest change of slope from one 30 m stretch to the next - what pitches
// an aircraft rolling along it - in percent.
struct Profile {
    double worst_off_line_m = 0.0;
    double worst_slope_change_pc = 0.0;
    double where_m = 0.0; // along it, from the le end, of the worst slope change
};

template <typename Height>
Profile profile(const RunwaySurfaces& runways, std::size_t i, Height height) {
    const double length_m = runways.at(i).length_m;
    const auto n = static_cast<std::size_t>(std::floor(length_m));
    std::vector<double> h(n + 1);
    for (std::size_t k = 0; k <= n; ++k) {
        const glideslope::world::Geodetic g =
            on_runway(runways, i, static_cast<double>(k) / length_m, 0.0);
        h[k] = height(g.latitude_deg, g.longitude_deg);
    }
    Profile p;
    const double rise = (h[n] - h[0]) / static_cast<double>(n);
    for (std::size_t k = 0; k <= n; ++k) {
        p.worst_off_line_m =
            std::max(p.worst_off_line_m, std::abs(h[k] - (h[0] + rise * static_cast<double>(k))));
    }
    constexpr std::size_t stretch = 30;
    for (std::size_t k = stretch; k + stretch <= n; ++k) {
        const double before = (h[k] - h[k - stretch]) / static_cast<double>(stretch);
        const double after = (h[k + stretch] - h[k]) / static_cast<double>(stretch);
        const double change = std::abs(after - before) * 100.0;
        if (change > p.worst_slope_change_pc) {
            p.worst_slope_change_pc = change;
            p.where_m = static_cast<double>(k);
        }
    }
    return p;
}

struct Reference {
    const char* airport;
    const char* le;
    const char* he;
};

// **The reference runways**: Sydney's three, where the DEM's bumps were found;
// Denver's longest, a high airfield; Heathrow's northern, between buildings;
// Boston's 15R/33L, on reclaimed land at the water's edge; and Courchevel's,
// which climbs 18% to its top.
const std::vector<Reference>& references() {
    static const std::vector<Reference> r{
        {"YSSY", "16R", "34L"}, {"YSSY", "16L", "34R"}, {"YSSY", "07", "25"},
        {"KDEN", "16R", "34L"}, {"EGLL", "09L", "27R"}, {"KBOS", "15R", "33L"},
        {"LFLJ", "04", "22"},
    };
    return r;
}

} // namespace

GLIDESLOPE_TEST(a_runway_strips_file_is_read_and_refused_where_it_is_wrong) {
    const std::string header =
        "# a comment\n"
        "airport,le_ident,he_ident,le_latitude_deg,le_longitude_deg,le_elevation_ft,"
        "he_latitude_deg,he_longitude_deg,he_elevation_ft,width_m\n";
    const std::vector<RunwayStrip> strips = glideslope::world::read_runway_strips(
        header + "YSSY,16R,34L,-33.929401,151.171997,8,-33.964298,151.180999,14,45.1\n"
                 "ONE,09,27,0,10,,0,10.009,,\r\n");
    check(strips.size() == 2, "two strips: " + std::to_string(strips.size()));
    check(strips[0].airport == "YSSY" && strips[0].le_ident == "16R" &&
              strips[0].he_ident == "34L" && strips[0].le_latitude_deg == -33.929401 &&
              strips[0].le_longitude_deg == 151.171997 && strips[0].le_elevation_ft == 8.0 &&
              strips[0].he_latitude_deg == -33.964298 &&
              strips[0].he_longitude_deg == 151.180999 && strips[0].he_elevation_ft == 14.0 &&
              strips[0].width_m == 45.1,
          "Sydney's 16R/34L, every field");
    check(std::isnan(strips[1].le_elevation_ft) && std::isnan(strips[1].he_elevation_ft) &&
              std::isnan(strips[1].width_m),
          "empty elevations and width are not given");
    const auto refused = [&](const std::string& text, const std::string& says) {
        try {
            (void)glideslope::world::read_runway_strips(text);
        } catch (const glideslope::world::RunwayError& e) {
            return std::string(e.what()).find(says) != std::string::npos;
        }
        return false;
    };
    check(refused("", "no header"), "an empty file");
    check(refused("airport,le\n", "not the header"), "another file's header");
    check(refused(header + "X,1,2,0,0,,0,1\n", "ten fields"), "a line short of fields");
    check(refused(header + "X,1,2,north,0,,0,1,,\n", "is not a number"), "a place not a number");
    check(refused(header + "X,1,2,91,0,,0,1,,\n", "off the Earth"), "a latitude past the pole");
}

// **The ground's identity travels with the protocol's version.** A server
// and a client on different ground refuse each other at the envelope, because
// any change to the strips or to the rules moves the version
// (net/protocol.hpp); this is what holds the three together. If it fails, the
// ground has changed: move `protocol_version`, and record the new three here
// and in docs/TRANSPORT.md.
GLIDESLOPE_TEST(the_protocol_version_moves_with_the_collision_ground) {
    std::ifstream in(data() / "runways" / "strips.csv", std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(in), {}};
    check(!text.empty(), "the build carries its runway strips");
    const std::string sha = glideslope::world::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
    std::printf("protocol version %d, ground rules %d, strips SHA-256 %s\n",
                glideslope::net::protocol_version, glideslope::world::collision_ground_rules,
                sha.c_str());
    check(glideslope::net::protocol_version == 9 &&
              glideslope::world::collision_ground_rules == 2 &&
              sha == "6c1ba3c3e6dc3bf19a6b0a402a00734b4d886b95e40c1097f1c69a301576898f",
          "the collision ground is the one protocol version 5 was moved for, and "
          "versions 6 to 9 kept: rules 2 on the strips of 2026-10-01");
    // **And the client written from the document speaks it**: its own
    // version constant, which a move of the version must move too.
    std::ifstream doc_client(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) / "doc_client" /
                                 "doc_client.cpp",
                             std::ios::binary);
    const std::string client{std::istreambuf_iterator<char>(doc_client), {}};
    const std::string constant = "constexpr std::uint8_t kVersion = 0x0" +
                                 std::to_string(glideslope::net::protocol_version) + ";";
    check(client.find(constant) != std::string::npos,
          "the client written from TRANSPORT.md says " + constant);
}

GLIDESLOPE_TEST(the_collision_ground_is_the_same_whatever_order_the_runways_come_in) {
    // Sydney's runways and Boston's - where 15R/33L crosses three others, so
    // that four runways' lines are moved together, and runways' surfaces
    // overlap - read in reverse and shuffled, give the same ground to the
    // last bit, everywhere they reach.
    const Ground g = open_ground();
    std::vector<RunwayStrip> strips;
    for (std::size_t i = 0; i < g.runways->size(); ++i) {
        const std::string& airport = g.runways->at(i).strip.airport;
        if (airport == "YSSY" || airport == "KBOS") {
            strips.push_back(g.runways->at(i).strip);
        }
    }
    check(strips.size() == 9, "Sydney has three runways and Boston six: " +
                                  std::to_string(strips.size()));
    std::vector<RunwayStrip> shuffled = strips;
    std::mt19937 random(7);
    std::shuffle(shuffled.begin(), shuffled.end(), random);
    std::reverse(strips.begin(), strips.end());
    CollisionGround one(g.dem, std::make_shared<const RunwaySurfaces>(strips));
    CollisionGround other(g.dem, std::make_shared<const RunwaySurfaces>(shuffled));
    struct Area {
        double south, west, north, east;
    };
    int compared = 0;
    int overlapping = 0;
    // Every 20 m over each airport: crossings, shoulders, ends and grass.
    for (const Area& a : {Area{-33.975, 151.160, -33.925, 151.200},
                          Area{42.345, -71.030, 42.382, -70.985}}) {
        const double d_lon = 0.00018 / std::cos(a.south * 3.14159265358979323846 / 180.0);
        for (double lat = a.south; lat <= a.north; lat += 0.00018) {
            for (double lon = a.west; lon <= a.east; lon += d_lon) {
                check(one.height_above_geoid(lat, lon) == other.height_above_geoid(lat, lon),
                      "the same ground at " + std::to_string(lat) + ", " +
                          std::to_string(lon));
                int reaching = 0;
                for (const std::uint32_t j : one.runways().reaching(lat, lon)) {
                    reaching += one.runways().place(j, lat, lon).weight > 0.0 ? 1 : 0;
                }
                overlapping += reaching >= 2 ? 1 : 0;
                ++compared;
            }
        }
    }
    std::printf("compared %d places, %d of them where two runways or more reach\n", compared,
                overlapping);
    check(overlapping > 0, "some places compared are reached by two runways or more");
}

// **The ground does not depend on which runway is asked for first.** A tied
// group is solved whole the first time any of its runways is asked for; two
// grounds over the same runways, one asked for them first to last and the
// other last to first, give every runway the same line and every place the
// same height, to the last bit - over Sydney and Boston, where groups of two
// and four are tied.
GLIDESLOPE_TEST(the_collision_ground_is_the_same_whichever_runway_is_asked_for_first) {
    const Ground g = open_ground();
    std::vector<RunwayStrip> strips;
    for (std::size_t i = 0; i < g.runways->size(); ++i) {
        const std::string& airport = g.runways->at(i).strip.airport;
        if (airport == "YSSY" || airport == "KBOS") {
            strips.push_back(g.runways->at(i).strip);
        }
    }
    const auto runways = std::make_shared<const RunwaySurfaces>(strips);
    CollisionGround forward(g.dem, runways);
    CollisionGround backward(g.dem, runways);
    for (std::size_t i = 0; i < runways->size(); ++i) {
        (void)forward.surface(i);
        (void)backward.surface(runways->size() - 1 - i);
    }
    std::size_t tied = 0;
    for (std::size_t i = 0; i < runways->size(); ++i) {
        const CollisionGround::Surface a = forward.surface(i);
        const CollisionGround::Surface b = backward.surface(i);
        check(a.le_m == b.le_m && a.he_m == b.he_m && a.from_file == b.from_file,
              runways->at(i).strip.airport + " " + runways->at(i).strip.le_ident +
                  ": the same line asked for first or last");
        tied += runways->at(i).ties.empty() ? 0U : 1U;
    }
    check(tied >= 6, "tied runways among them: " + std::to_string(tied));
    int compared = 0;
    for (const double lat0 : {-33.95, 42.365}) {
        const double lon0 = lat0 < 0.0 ? 151.18 : -71.01;
        for (int a = -40; a <= 40; ++a) {
            for (int b = -40; b <= 40; ++b) {
                const double lat = lat0 + a * 0.0003;
                const double lon = lon0 + b * 0.0004;
                check(forward.height_above_geoid(lat, lon) == backward.height_above_geoid(lat, lon),
                      "the same ground at " + std::to_string(lat) + ", " + std::to_string(lon));
                ++compared;
            }
        }
    }
    check(compared == 2 * 81 * 81, "every place compared");
}

GLIDESLOPE_TEST(the_collision_ground_under_a_runway_is_its_own_line_and_past_its_shoulder_the_dem) {
    const Ground g = open_ground();
    // **The file's elevations are used**, not fallen back from: Sydney's 16R
    // is the file's 8 ft and 34L its 14 ft, to the bit; Courchevel's, whose
    // 04 end is 11.7 m from the fit, is the fit.
    {
        const CollisionGround::Surface yssy =
            g.collision->own_line(runway_named(*g.runways, "YSSY", "16R", "34L"));
        check(yssy.from_file && yssy.le_m == 8 * 0.3048 && yssy.he_m == 14 * 0.3048,
              "Sydney 16R/34L's own line is the file's 8 ft and 14 ft: " +
                  std::to_string(yssy.le_m) + " m and " + std::to_string(yssy.he_m) + " m");
        check(!g.collision->own_line(runway_named(*g.runways, "LFLJ", "04", "22")).from_file,
              "Courchevel's own line is the fit to the DEM");
        std::size_t both = 0;
        for (std::size_t i = 0; i < g.runways->size(); ++i) {
            const RunwayStrip& s = g.runways->at(i).strip;
            both += std::isfinite(s.le_elevation_ft) && std::isfinite(s.he_elevation_ft) ? 1U : 0U;
        }
        std::printf("%zu runways; %zu give both ends' elevations, %zu do not and are fitted\n",
                    g.runways->size(), both, g.runways->size() - both);
    }
    for (const Reference& ref : references()) {
        const std::size_t i = runway_named(*g.runways, ref.airport, ref.le, ref.he);
        const RunwaySurfaces::Runway& r = g.runways->at(i);
        const CollisionGround::Surface s = g.collision->surface(i);
        const CollisionGround::Surface own = g.collision->own_line(i);
        const std::string name = std::string(ref.airport) + " " + ref.le + "/" + ref.he;
        std::printf("%s: %.0f m by %.0f m, %.3f m to %.3f m above sea level, through %s at "
                    "%.3f m and %.3f m, tied %zu time%s\n",
                    name.c_str(), r.length_m, 2 * r.half_width_m, s.le_m, s.he_m,
                    own.from_file ? "the file's elevations" : "the fit to the DEM", own.le_m,
                    own.he_m, r.ties.size(), r.ties.size() == 1 ? "" : "s");
        // Where it is tied to another runway, the two lines meet.
        for (const RunwaySurfaces::Tie& c : r.ties) {
            const CollisionGround::Surface o = g.collision->surface(c.other);
            const double here = s.le_m + c.t * (s.he_m - s.le_m);
            const double there = o.le_m + c.other_t * (o.he_m - o.le_m);
            std::printf("  meets %s %s/%s %.0f m along, at %.4f m and %.4f m\n",
                        g.runways->at(c.other).strip.airport.c_str(),
                        g.runways->at(c.other).strip.le_ident.c_str(),
                        g.runways->at(c.other).strip.he_ident.c_str(), c.t * r.length_m, here,
                        there);
            check(std::abs(here - there) < 0.001,
                  name + ": its line meets the line of the runway it is tied to");
        }
        // Each station is checked, or counted as another runway's to share.
        int stations = 0;
        int checked = 0;
        for (double t = 0.0; t <= 1.0; t += 1.0 / 64.0) {
            const double line_m = s.le_m + t * (s.he_m - s.le_m);
            // On it, level across it - away from any runway crossing it,
            // where the ground is the two's mean.
            for (const double across : {-0.95, 0.0, 0.95}) {
                ++stations;
                const glideslope::world::Geodetic p =
                    on_runway(*g.runways, i, t, across * r.half_width_m);
                bool crossed = false;
                for (const std::uint32_t j : g.runways->reaching(p.latitude_deg, p.longitude_deg)) {
                    crossed = crossed ||
                              (j != i && g.runways->place(j, p.latitude_deg, p.longitude_deg)
                                                 .weight > 0.0);
                }
                if (crossed) {
                    continue;
                }
                ++checked;
                const double on = g.collision->height_above_geoid(p.latitude_deg, p.longitude_deg);
                check(std::abs(on - line_m) < 1e-6,
                      name + ": on the runway at " + std::to_string(t) + " along and " +
                          std::to_string(across) + " across, " + std::to_string(on) +
                          " m where its line is " + std::to_string(line_m));
            }
            // Past its shoulder, either side: the DEM's, exactly.
            for (const double side : {-1.0, 1.0}) {
                const glideslope::world::Geodetic p = on_runway(
                    *g.runways, i, t, side * (r.half_width_m + glideslope::world::runway_shoulder_m + 0.5));
                bool reached = false;
                for (const std::uint32_t j : g.runways->reaching(p.latitude_deg, p.longitude_deg)) {
                    reached = reached ||
                              (j != i &&
                               g.runways->place(j, p.latitude_deg, p.longitude_deg).weight > 0.0);
                }
                if (reached) {
                    continue; // on another runway or its shoulder
                }
                check(g.collision->height_above_geoid(p.latitude_deg, p.longitude_deg) ==
                          g.dem->height_above_geoid(p.latitude_deg, p.longitude_deg),
                      name + ": past its shoulder, the DEM's height");
            }
        }
        std::printf("  %d of %d stations on it checked; the rest share it with another runway\n",
                    checked, stations);
        check(stations == 65 * 3 && checked * 2 >= stations,
              name + ": " + std::to_string(checked) + " of " + std::to_string(stations) +
                  " stations checked, where at least half were to be");
        // Past each end's shoulder, on the centreline: the DEM's.
        for (const double t : {-(glideslope::world::runway_shoulder_m + 0.5) / r.length_m,
                               1.0 + (glideslope::world::runway_shoulder_m + 0.5) / r.length_m}) {
            const glideslope::world::Geodetic p = on_runway(*g.runways, i, t, 0.0);
            check(g.collision->height_above_geoid(p.latitude_deg, p.longitude_deg) ==
                      g.dem->height_above_geoid(p.latitude_deg, p.longitude_deg),
                  name + ": past an end's shoulder, the DEM's height");
        }
    }
}

// **What the collision ground under a runway may do**, measured along every
// reference runway's centreline against the DEM's own: how far it strays from
// the straight line between its ends, and how sharply its slope changes over
// 30 m. On the DEM, Sydney's 16R/34L strays by metres and its slope changes by
// several percent within a few cells; the collision ground is the runway's own
// line, and strays from it only where another runway crosses it, by half the
// two's difference in height there, blended in over a shoulder.
GLIDESLOPE_TEST(reference_runways_roll_with_no_bump_beyond_a_bound) {
    const Ground g = open_ground();
    constexpr double off_line_bound_m = 0.05;
    constexpr double slope_change_bound_pc = 0.25;
    std::size_t measured = 0;
    for (const Reference& ref : references()) {
        const std::size_t i = runway_named(*g.runways, ref.airport, ref.le, ref.he);
        const std::string name = std::string(ref.airport) + " " + ref.le + "/" + ref.he;
        const Profile dem = profile(*g.runways, i, [&](double lat, double lon) {
            return g.dem->height_above_geoid(lat, lon);
        });
        const Profile flown = profile(*g.runways, i, [&](double lat, double lon) {
            return g.collision->height_above_geoid(lat, lon);
        });
        std::printf("%s: the DEM strays %.2f m from its line and changes slope by %.2f%% "
                    "(%.0f m along); the collision ground %.3f m and %.3f%% (%.0f m along)\n",
                    name.c_str(), dem.worst_off_line_m, dem.worst_slope_change_pc, dem.where_m,
                    flown.worst_off_line_m, flown.worst_slope_change_pc, flown.where_m);
        check(flown.worst_off_line_m <= off_line_bound_m,
              name + ": the collision ground strays " + std::to_string(flown.worst_off_line_m) +
                  " m from the runway's line");
        check(flown.worst_slope_change_pc <= slope_change_bound_pc,
              name + ": the collision ground's slope changes by " +
                  std::to_string(flown.worst_slope_change_pc) + "% over 30 m");
        ++measured;
    }
    check(measured == references().size(), "every reference runway measured");
}

// **Every aeroplane takes off from Sydney's 16R on the DEM**: stood on its end
// as OurAirports places it, at the collision ground's height there, and flown
// by the take-off autopilot as `glideslope_cli fly-plan` flies it, until it
// hands over - with nothing wrecked on the way. Every landplane the data holds.
// **One that could not be taken off would be rolled instead**, as the
// take-off autopilot rolls - full power, held on the centreline, the stick
// where it sits - through 16R's first 2,000 m, with nothing wrecked and the
// nose within 1.5 degrees of where it stood: the 747-400 and F-22A were, until
// they were given take-off speeds measured from their models (2026-10-06).
// None is now. Left out, and named: a flying boat, which has no wheels to
// roll on.
//
// **Flown in five groups**, one test each, so that no CI shard waits on all of
// them: flown to the orbit, the light aeroplanes take eight to ten minutes of
// simulated time each, and the thirteen together took 333 s in linux-debug.
// Every group checks that the groups and the flying boat are the catalogue,
// each aircraft once.
namespace {

const std::map<std::string, std::vector<std::string>>& off_16r_groups() {
    static const std::map<std::string, std::vector<std::string>> groups{
        {"airliners", {"737-300", "747-400", "787-8", "a320", "a380"}},
        {"military", {"b2", "f15c", "f22", "f35b"}},
        {"learjet and mosquito", {"learjet35a", "mosquito-fb6"}},
        {"cessnas", {"c172p", "c182"}},
        {"cub and cherokee", {"j3cub", "pa28"}},
    };
    return groups;
}

void off_16r_to_the_orbit(const std::string& group) {
    const std::vector<std::string>& ids = off_16r_groups().at(group);
    const Ground g = open_ground();
    const std::vector<glideslope::world::RunwayEnd> ends = glideslope::world::runways_at(
        glideslope::world::world_runways(GLIDESLOPE_TEST_DOWNLOADS_DIR,
                                         glideslope::world::http_fetch()),
        "YSSY");
    const auto end = std::find_if(ends.begin(), ends.end(), [](const auto& e) {
        return e.ident == "16R";
    });
    check(end != ends.end(), "the runways file has Sydney's 16R");
    const std::shared_ptr<CollisionGround> ground = g.collision;
    const double runway_ft =
        ground->height_above_ellipsoid(end->latitude_deg, end->longitude_deg) * feet_per_metre;
    const glideslope::sim::Runway runway = glideslope::world::as_runway(*end, runway_ft);

    const std::vector<glideslope::sim::CatalogueEntry> catalogue =
        glideslope::sim::read_catalogue(data());
    std::size_t flown = 0;
    std::size_t rolled = 0;
    std::vector<std::string> left_out;
    std::string failures;
    // Rolled and not taken off, each with its reason; the test fails if any
    // other cannot be flown, or if one of these can.
    // None since 2026-10-06, when the 747-400 and the F-22A were given
    // take-off speeds measured from their models (`<takeoff_speeds>`).
    const std::map<std::string, std::string> cannot_be_flown{};
    const glideslope::sim::FlightPlan cbd_orbit = [] {
        std::ifstream in(data() / "plans" / "sydney-cbd-orbit.plan", std::ios::binary);
        check(static_cast<bool>(in), "the data has the sydney-cbd-orbit plan");
        return glideslope::sim::parse_flight_plan(
            std::string(std::istreambuf_iterator<char>(in), {}));
    }();
    // **The groups are the catalogue**: every landplane in exactly one, and
    // the flying boats in none.
    std::map<std::string, int> grouped;
    for (const auto& [name, members] : off_16r_groups()) {
        for (const std::string& id : members) {
            ++grouped[id];
        }
    }
    std::size_t landplanes = 0;
    for (const glideslope::sim::CatalogueEntry& e : catalogue) {
        if (e.seaplane) {
            check(grouped.count(e.id) == 0, e.id + " is a flying boat, and in a 16R group");
            continue;
        }
        ++landplanes;
        check(grouped.count(e.id) != 0 && grouped.at(e.id) == 1,
              e.id + " is in one 16R group, and only one");
    }
    check(grouped.size() == landplanes, "every aircraft in a 16R group is in the catalogue");
    for (const glideslope::sim::CatalogueEntry& e : catalogue) {
        if (std::find(ids.begin(), ids.end(), e.id) == ids.end()) {
            continue;
        }
        std::optional<glideslope::sim::DepartureSpeeds> speeds;
        bool roll_only = false;
        try {
            speeds = glideslope::sim::departure_speeds(data(), e.model);
        } catch (const std::runtime_error& why) {
            check(cannot_be_flown.contains(e.id),
                  e.id + " cannot be taken off and is not named as such: " + why.what());
            // Never rotated: the roll is all that is flown.
            speeds = glideslope::sim::DepartureSpeeds{};
            speeds->rotate_kts = 1e9;
            roll_only = true;
        }
        check(roll_only == cannot_be_flown.contains(e.id),
              e.id + " is named as one that cannot be taken off, and can be");
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [ground](double lat, double lon) { return ground->height_above_ellipsoid(lat, lon); },
            [ground](double lat, double lon) {
                return ground->water(lat, lon) != glideslope::world::Water::none;
            }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = runway.threshold_lat_deg;
        ic.longitude_deg = runway.threshold_lon_deg;
        ic.altitude_ft = runway.elevation_ft;
        ic.terrain_elevation_ft = runway.elevation_ft;
        ic.heading_deg = runway.heading_deg;
        ic.airspeed_kts = 0.0;
        ic.engine_running = true;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        glideslope::sim::GroundJudge judge(false);
        std::optional<std::string> wrecked;
        double worst_pitch_on_the_ground_deg = 0.0;
        const double standing_pitch_deg = aircraft.state().pitch_deg;
        double worst_nose_change_deg = 0.0;
        if (roll_only) {
            glideslope::sim::Departure departure(aircraft, runway, *speeds);
            for (std::int64_t step = 0; step < 300 * glideslope::sim::steps_per_second &&
                                        departure.along_m() < 2000.0;
                 ++step) {
                aircraft.set_controls(departure.fly());
                aircraft.step();
                if ((wrecked = judge.judge(aircraft))) {
                    break;
                }
                worst_nose_change_deg = std::max(
                    worst_nose_change_deg, std::abs(aircraft.state().pitch_deg - standing_pitch_deg));
            }
            const double kts = aircraft.state().airspeed_kts;
            std::printf("%s: rolled %.0f m to %.0f kt, %s, the nose within %.1f degrees of "
                        "where it stood\n",
                        e.id.c_str(), departure.along_m(), kts,
                        wrecked ? ("wrecked: " + *wrecked).c_str() : "nothing wrecked",
                        worst_nose_change_deg);
            std::fflush(stdout);
            if (wrecked || departure.along_m() < 2000.0 || worst_nose_change_deg > 1.5) {
                failures += "\n  " + e.id + " was not rolled through 2,000 m with its nose down";
            }
            left_out.push_back(e.id + " rolled, not taken off (" + cannot_be_flown.at(e.id) + ")");
            ++rolled;
            continue;
        }
        // **Taken off, then flown on by the plan to its orbit** - the
        // sydney-cbd-orbit plan, written for the C172P, made this aircraft's:
        // its speeds brought within the ones a plan may fly it at, and its
        // orbit widened to the tightest it may fly at that speed, as a plan
        // for it would have to be. No function of the product makes a plan
        // another aircraft's, so this reads the floor and ceiling from
        // `sim::plan_speeds` and the radius from `sim::least_orbit_radius_m`,
        // and `sim::refuse_what_it_cannot_fly` - what refuses a plan file -
        // checks the result. Flown at the plan's 80 and 90 knots, as it was
        // before 2026-10-02's refusal of such plans, the Learjet 35A and the
        // Mosquito stalled 80 to 90 s in and came down in Botany Bay.
        glideslope::sim::FlightPlan plan = cbd_orbit;
        plan.aircraft = e.id;
        plan.takeoff->runway = runway;
        const glideslope::sim::PlanSpeeds may = glideslope::sim::plan_speeds(data(), e.model);
        for (glideslope::sim::Waypoint& w : plan.waypoints) {
            w.airspeed_kts = std::clamp(w.airspeed_kts, std::ceil(may.slowest_kts),
                                        std::floor(may.fastest_kts));
            if (w.orbit) {
                w.orbit->radius_m = std::max(
                    w.orbit->radius_m, std::ceil(glideslope::sim::least_orbit_radius_m(w.airspeed_kts)));
            }
        }
        glideslope::sim::refuse_what_it_cannot_fly(data(), plan, e.id);
        for (glideslope::sim::Waypoint& w : plan.waypoints) {
            w.altitude_ft += g.geoid->undulation(w.latitude_deg, w.longitude_deg) * feet_per_metre;
        }
        const glideslope::sim::Waypoint orbit = plan.waypoints.back();
        glideslope::sim::Controller controller(aircraft, glideslope::sim::Controls{});
        controller.to_ai_flying(plan, *speeds);
        double unstuck_along_m = 0.0;
        double handed_over_s = -1.0;
        double at_orbit_s = -1.0;
        const auto seconds = [](std::int64_t steps) {
            return static_cast<double>(steps) / static_cast<double>(glideslope::sim::steps_per_second);
        };
        // At most twenty minutes, which the slowest, the J-3 Cub, needs not
        // half of.
        for (std::int64_t step = 0; step < 1200 * glideslope::sim::steps_per_second; ++step) {
            aircraft.set_controls(controller.fly());
            aircraft.step();
            if ((wrecked = judge.judge(aircraft))) {
                break;
            }
            if (const glideslope::sim::Departure* d = controller.departure()) {
                unstuck_along_m = d->unstuck_along_m();
                if (d->stage() == glideslope::sim::Departure::Stage::roll) {
                    worst_pitch_on_the_ground_deg = std::max(worst_pitch_on_the_ground_deg,
                                                             std::abs(aircraft.state().pitch_deg));
                }
                continue;
            }
            if (handed_over_s < 0.0) {
                handed_over_s = seconds(step);
            }
            // **At its orbit**: on the orbit's leg, and within a tenth of
            // its radius of its circle.
            const glideslope::sim::Navigator* navigator = controller.navigator();
            if (navigator != nullptr && navigator->next() + 1 == plan.waypoints.size()) {
                const double from_centre_m = glideslope::sim::distance_m(
                    aircraft.property("position/lat-geod-deg"),
                    aircraft.property("position/long-gc-deg"), orbit.latitude_deg,
                    orbit.longitude_deg);
                if (std::abs(from_centre_m - orbit.orbit->radius_m) < 0.1 * orbit.orbit->radius_m) {
                    at_orbit_s = seconds(step);
                    break;
                }
            }
        }
        std::printf("%s: %s; unstuck %.0f m along, pitched at most %.1f degrees on its roll; "
                    "handed over %.0f s in; at its orbit, %.0f m round at %.0f kt, %.0f s in\n",
                    e.id.c_str(), wrecked ? ("wrecked: " + *wrecked).c_str() : "nothing wrecked",
                    unstuck_along_m, worst_pitch_on_the_ground_deg, handed_over_s,
                    orbit.orbit->radius_m, orbit.airspeed_kts, at_orbit_s);
        std::fflush(stdout);
        if (wrecked) {
            failures += "\n  " + e.id + " was wrecked: " + *wrecked;
        } else if (handed_over_s < 0.0) {
            failures += "\n  " + e.id + " did not take off";
        } else if (at_orbit_s < 0.0) {
            failures += "\n  " + e.id + " did not reach its orbit";
        }
        ++flown;
    }
    for (const std::string& why : left_out) {
        std::printf("left out: %s\n", why.c_str());
    }
    check(flown + rolled == ids.size() && left_out.size() == rolled,
          "every aircraft of the " + group + " flown or rolled: " + std::to_string(flown) +
              " flown, " + std::to_string(rolled) + " rolled, of " +
              std::to_string(ids.size()));
    check(failures.empty(), "every aeroplane took off from 16R and flew its plan to its orbit:" + failures);
}

} // namespace

GLIDESLOPE_TEST(the_airliners_roll_down_sydneys_16r_on_the_dem_and_those_that_can_climb_away_fly_their_plan_to_its_orbit) {
    off_16r_to_the_orbit("airliners");
}

GLIDESLOPE_TEST(the_military_jets_roll_down_sydneys_16r_on_the_dem_and_those_that_can_climb_away_fly_their_plan_to_its_orbit) {
    off_16r_to_the_orbit("military");
}

GLIDESLOPE_TEST(the_learjet_and_the_mosquito_take_off_from_sydneys_16r_on_the_dem_and_fly_their_plan_to_its_orbit) {
    off_16r_to_the_orbit("learjet and mosquito");
}

GLIDESLOPE_TEST(the_cessnas_take_off_from_sydneys_16r_on_the_dem_and_fly_their_plan_to_its_orbit) {
    off_16r_to_the_orbit("cessnas");
}

GLIDESLOPE_TEST(the_cub_and_the_cherokee_take_off_from_sydneys_16r_on_the_dem_and_fly_their_plan_to_its_orbit) {
    off_16r_to_the_orbit("cub and cherokee");
}

namespace {

// **How far the ground under runway `i` is pulled off its own line** by
// others reaching it, at every 10 m along it and three places across: the
// worst, in metres, and how many places another runway reached.
struct Pull {
    double worst_m = 0.0;
    double where_t = 0.0;
    int reached = 0;
    // Of those, where another runway's pavement overlaps this one's - the
    // other's rectangle covers the place - and the worst there; `worst_m` is
    // the worst everywhere else.
    int overlapped = 0;
    double overlap_worst_m = 0.0;
};

Pull pull_on(CollisionGround& ground, std::size_t i, double step_m = 10.0) {
    const RunwaySurfaces& runways = ground.runways();
    const RunwaySurfaces::Runway& r = runways.at(i);
    const CollisionGround::Surface s = ground.surface(i);
    Pull p;
    const auto steps = static_cast<int>(std::ceil(r.length_m / step_m));
    for (int k = 0; k <= steps; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(steps);
        for (const double across : {-0.9, 0.0, 0.9}) {
            const glideslope::world::Geodetic g = on_runway(runways, i, t, across * r.half_width_m);
            bool others = false;
            bool overlap = false;
            for (const std::uint32_t j : runways.reaching(g.latitude_deg, g.longitude_deg)) {
                if (j == i) {
                    continue;
                }
                const RunwaySurfaces::Placed o = runways.place(j, g.latitude_deg, g.longitude_deg);
                others = others || o.weight > 0.0;
                overlap = overlap || o.outside_m < glideslope::world::runway_overlap_band_m;
            }
            if (!others) {
                continue;
            }
            ++p.reached;
            const double off = std::abs(ground.height_above_geoid(g.latitude_deg, g.longitude_deg) -
                                        (s.le_m + t * (s.he_m - s.le_m)));
            if (overlap) {
                ++p.overlapped;
                p.overlap_worst_m = std::max(p.overlap_worst_m, off);
                continue;
            }
            if (off > p.worst_m) {
                p.worst_m = off;
                p.where_t = t;
            }
        }
    }
    return p;
}

} // namespace

// **Runways that meet without crossing are made to meet too.** Laid on the
// sea, 45 m wide, each pair at ends 20 ft apart and sloping differently: a V
// (two runways from one end), a T (one ending at the other's edge), two end to
// end with 10 m between, and two side by side 80 m apart, centreline to
// centreline. Untied, each would pull the other's surface by up to half their
// difference - three metres - where they overlap; tied, the lines meet where
// the runways do and the pull is what is left of their slopes' difference.
GLIDESLOPE_TEST(runways_that_meet_in_a_v_a_t_end_to_end_or_side_by_side_are_made_to_meet) {
    const double lat = 0.5;
    const double lon = -150.0;
    struct Layout {
        const char* name;
        std::vector<RunwayStrip> strips;
    };
    const auto at = [&](double east_m, double north_m) { return offset(lat, lon, east_m, north_m); };
    const auto [t_lat, t_lon] = at(1000.0, 22.5);
    const auto [e_lat, e_lon] = at(2010.0, 0.0);
    const auto [s_lat, s_lon] = at(300.0, 80.0);
    const std::vector<Layout> layouts{
        {"a V", {laid("A", lat, lon, 90.0, 2000.0, 10.0, 40.0),
                 laid("B", lat, lon, 45.0, 2000.0, 30.0, 90.0)}},
        {"a T", {laid("A", lat, lon, 90.0, 2000.0, 10.0, 40.0),
                 laid("B", t_lat, t_lon, 0.0, 1500.0, 30.0, 0.0)}},
        {"end to end", {laid("A", lat, lon, 90.0, 2000.0, 10.0, 40.0),
                        laid("B", e_lat, e_lon, 90.0, 1500.0, 60.0, 70.0)}},
        {"side by side", {laid("A", lat, lon, 90.0, 2000.0, 10.0, 40.0),
                          laid("B", s_lat, s_lon, 90.0, 1500.0, 30.0, 20.0)}},
    };
    constexpr double pull_bound_m = 0.3;
    for (const Layout& l : layouts) {
        CollisionGround ground(sea(), std::make_shared<const RunwaySurfaces>(l.strips),
                               std::numeric_limits<double>::infinity());
        const RunwaySurfaces& runways = ground.runways();
        check(runways.size() == 2, std::string(l.name) + ": two runways");
        check(!runways.at(0).ties.empty(), std::string(l.name) + ": tied");
        for (std::size_t i = 0; i < 2; ++i) {
            const CollisionGround::Surface s = ground.surface(i);
            for (const RunwaySurfaces::Tie& c : runways.at(i).ties) {
                const CollisionGround::Surface o = ground.surface(c.other);
                check(std::abs((s.le_m + c.t * (s.he_m - s.le_m)) -
                               (o.le_m + c.other_t * (o.he_m - o.le_m))) < 0.001,
                      std::string(l.name) + ": the lines meet where they are tied");
            }
            const Pull p = pull_on(ground, i, 1.0);
            const CollisionGround::Surface own = ground.own_line(i);
            std::printf("%s, runway %s: tied %zu time%s, moved %.2f m and %.2f m at its ends; "
                        "pulled %.3f m at worst (%.0f%% along), at %d places another reaches\n",
                        l.name, runways.at(i).strip.airport.c_str(), runways.at(i).ties.size(),
                        runways.at(i).ties.size() == 1 ? "" : "s", s.le_m - own.le_m,
                        s.he_m - own.he_m, p.worst_m, p.where_t * 100.0, p.reached);
            check(p.reached > 0, std::string(l.name) + ": the other runway reaches this one");
            check(p.worst_m <= pull_bound_m,
                  std::string(l.name) + ": pulled " + std::to_string(p.worst_m) +
                      " m off its line");
        }
    }
}

// **One runway's shoulder does not pull another's pavement** (rules 2,
// 2026-10-06). On the sea, 45 m wide: A east for 2,000 m from 10 ft to 40 ft,
// and B from 80 m north of A's first end, 5 degrees north of east, for 1,500 m
// from 30 ft down to 0 - tied at B's first end, which is within reach of A,
// and parting from A's line away from it. Their pavements are 35 m apart
// there, so B's shoulder lies over A's pavement and A's over B's for the
// first hundred metres or so, where their lines part. Under rules 1 each
// shoulder pulled the other's pavement, A's by 0.027 m at worst; now,
// on a runway's pavement, a runway's surface further than
// `runway_overlap_band_m` from its own rectangle weighs nothing, and each is
// exactly its own line.
GLIDESLOPE_TEST(a_runways_shoulder_does_not_pull_the_pavement_of_another_beside_it) {
    const auto [b_lat, b_lon] = offset(0.5, -150.0, 0.0, 80.0);
    const std::vector<RunwayStrip> strips{laid("A", 0.5, -150.0, 90.0, 2000.0, 10.0, 40.0),
                                          laid("B", b_lat, b_lon, 85.0, 1500.0, 30.0, 0.0)};
    CollisionGround ground(sea(), std::make_shared<const RunwaySurfaces>(strips),
                           std::numeric_limits<double>::infinity());
    const RunwaySurfaces& runways = ground.runways();
    check(runways.size() == 2 && !runways.at(0).ties.empty(), "two runways, tied");
    for (std::size_t i = 0; i < 2; ++i) {
        const Pull p = pull_on(ground, i, 1.0);
        std::printf("runway %s: pulled %.6f m at worst (%.0f%% along), at %d places the other's "
                    "shoulder reaches\n",
                    runways.at(i).strip.airport.c_str(), p.worst_m, p.where_t * 100.0, p.reached);
        check(p.reached >= 100, runways.at(i).strip.airport +
                                    ": the other's shoulder reaches its pavement at 100 places "
                                    "or more: " + std::to_string(p.reached));
        check(p.worst_m <= 1e-6, runways.at(i).strip.airport + ": pulled " +
                                     std::to_string(p.worst_m) + " m off its line");
    }
}

// **Where longitude wraps and near a pole** the ground is the same: a runway
// across the antimeridian on the equator, and one 10 km from the North Pole,
// each on its line along its length, either side of 180 degrees, and the sea's
// past its shoulder.
GLIDESLOPE_TEST(a_runway_across_the_antimeridian_or_near_a_pole_is_its_own_line) {
    const std::vector<RunwayStrip> strips{laid("DATELINE", 0.0, 179.99, 90.0, 2200.0, 3.0, 13.0),
                                          laid("POLE", 89.91, 30.0, 120.0, 3000.0, 6.0, 1.0)};
    CollisionGround ground(sea(), std::make_shared<const RunwaySurfaces>(strips));
    const RunwaySurfaces& runways = ground.runways();
    check(runways.size() == 2, "both laid");
    check(runways.at(0).strip.he_longitude_deg < 0.0,
          "the dateline runway's far end is west of 180: " +
              std::to_string(runways.at(0).strip.he_longitude_deg));
    for (std::size_t i = 0; i < runways.size(); ++i) {
        const CollisionGround::Surface s = ground.surface(i);
        check(s.from_file, runways.at(i).strip.airport + ": the file's elevations");
        int stations = 0;
        for (double t = 0.0; t <= 1.0; t += 1.0 / 128.0) {
            for (const double across : {-0.9, 0.0, 0.9}) {
                const glideslope::world::Geodetic p =
                    on_runway(runways, i, t, across * runways.at(i).half_width_m);
                const double on = ground.height_above_geoid(p.latitude_deg, p.longitude_deg);
                check(std::abs(on - (s.le_m + t * (s.he_m - s.le_m))) < 1e-6,
                      runways.at(i).strip.airport + ": on its line at " + std::to_string(t) +
                          " along, " + std::to_string(p.longitude_deg) + " E");
                ++stations;
            }
            const glideslope::world::Geodetic past = on_runway(
                runways, i, t, runways.at(i).half_width_m + glideslope::world::runway_shoulder_m + 0.5);
            check(ground.height_above_geoid(past.latitude_deg, past.longitude_deg) == 0.0,
                  runways.at(i).strip.airport + ": the sea's past its shoulder");
        }
        check(stations == 129 * 3, runways.at(i).strip.airport + ": every station checked");
    }
}

// **How far one runway pulls another's surface off its line, worldwide**:
// every runway in the strips that gives both its ends' elevations, believed
// as the file gives them, laid on the sea so that no fit to the DEM enters,
// and sampled every 20 m along it and three places across, wherever another
// runway with both elevations reaches it. Off the overlaps it is held to
// 0.1 m; on them, what remains is stated and bounded.
GLIDESLOPE_TEST(no_runway_in_the_world_is_pulled_off_its_line_beyond_a_bound) {
    std::vector<RunwayStrip> strips;
    for (const RunwayStrip& s : glideslope::world::read_runway_strips([] {
             std::ifstream in(data() / "runways" / "strips.csv", std::ios::binary);
             return std::string(std::istreambuf_iterator<char>(in), {});
         }())) {
        if (std::isfinite(s.le_elevation_ft) && std::isfinite(s.he_elevation_ft)) {
            strips.push_back(s);
        }
    }
    CollisionGround ground(sea(), std::make_shared<const RunwaySurfaces>(strips),
                           std::numeric_limits<double>::infinity());
    const RunwaySurfaces& runways = ground.runways();
    struct Worst {
        double m;
        std::size_t i;
    };
    std::vector<Worst> pulled;
    std::vector<Worst> overlaps;
    std::size_t reached = 0;
    std::size_t places = 0;
    std::size_t overlapped_places = 0;
    std::size_t moved_over_1m = 0;
    double most_moved_m = 0.0;
    std::size_t most_moved = 0;
    for (std::size_t i = 0; i < runways.size(); ++i) {
        // How far the ties moved its ends from the file's elevations.
        const CollisionGround::Surface s = ground.surface(i);
        const CollisionGround::Surface own = ground.own_line(i);
        const double moved = std::max(std::abs(s.le_m - own.le_m), std::abs(s.he_m - own.he_m));
        moved_over_1m += moved > 1.0 ? 1U : 0U;
        if (moved > most_moved_m) {
            most_moved_m = moved;
            most_moved = i;
        }
        const Pull p = pull_on(ground, i, 20.0);
        if (p.reached > 0) {
            ++reached;
            places += static_cast<std::size_t>(p.reached);
            overlapped_places += static_cast<std::size_t>(p.overlapped);
            pulled.push_back({p.worst_m, i});
            if (p.overlapped > 0) {
                overlaps.push_back({p.overlap_worst_m, i});
            }
        }
    }
    const auto worst_first = [](const Worst& a, const Worst& b) { return a.m > b.m; };
    std::sort(pulled.begin(), pulled.end(), worst_first);
    std::sort(overlaps.begin(), overlaps.end(), worst_first);
    const auto count_over = [](const std::vector<Worst>& list, double m) {
        return std::count_if(list.begin(), list.end(), [m](const Worst& w) { return w.m > m; });
    };
    const auto over = [&](double m) { return count_over(pulled, m); };
    const auto list = [&](const std::vector<Worst>& worst) {
        for (std::size_t k = 0; k < worst.size() && k < 8; ++k) {
            const RunwayStrip& s = runways.at(worst[k].i).strip;
            std::printf("  %.3f m  %s %s/%s\n", worst[k].m, s.airport.c_str(),
                        s.le_ident.c_str(), s.he_ident.c_str());
        }
    };
    std::printf("%zu runways with both elevations; %zu reached by another, at %zu places, "
                "%zu of them where another's pavement overlaps\n",
                runways.size(), reached, places, overlapped_places);
    std::printf("off the overlaps, pulled more than 0.1 m: %td, 0.3 m: %td, 1 m: %td\n",
                over(0.1), over(0.3), over(1.0));
    list(pulled);
    std::printf("on the overlaps (%zu runways), pulled more than 0.1 m: %td, 0.3 m: %td\n",
                overlaps.size(), count_over(overlaps, 0.1), count_over(overlaps, 0.3));
    list(overlaps);
    std::printf("the ties moved %zu runways' ends more than 1 m from the file's elevations; "
                "most, %s %s/%s, by %.2f m\n",
                moved_over_1m, runways.at(most_moved).strip.airport.c_str(),
                runways.at(most_moved).strip.le_ident.c_str(),
                runways.at(most_moved).strip.he_ident.c_str(), most_moved_m);
    // Every runway with both elevations, and every one another reaches: the
    // pinned strips' numbers, so that a measurement that walked less of the
    // world than it says fails.
    check(runways.size() == 11201 && reached == 3619,
          "11,201 runways measured, 3,619 of them reached by another: " +
              std::to_string(runways.size()) + " and " + std::to_string(reached));
    check(!pulled.empty() && pulled.front().m <= off_overlap_pull_bound_m,
          "off the overlaps, the worst pull is " +
              std::to_string(pulled.empty() ? 0.0 : pulled.front().m) + " m");
    check(overlapped_places > 0 && count_over(overlaps, 0.3) <= overlap_over_0_3_m &&
              count_over(overlaps, 0.1) <= overlap_over_0_1_m,
          "on the overlaps, pulled more than 0.3 m: " + std::to_string(count_over(overlaps, 0.3)) +
              ", more than 0.1 m: " + std::to_string(count_over(overlaps, 0.1)));
    check(!overlaps.empty() && overlaps.front().m <= overlap_pull_bound_m,
          "on the overlaps, the worst pull is " +
              std::to_string(overlaps.empty() ? 0.0 : overlaps.front().m) + " m");
}

// **A group its ties would move too far is made from the fits instead.** On
// the sea, whose fit is zero: a 2,000 m runway from +16 ft to -16 ft, and
// beside its middle, 80 m off, a 150 m one from -16 ft to +16 ft. Each end is
// within the 5 m tolerance of the fit, so each runway alone is believed; tied
// at the short one's ends, it would be moved 5.4 m - more than the tolerance -
// so the group is made from the fits, and lies at sea level.
GLIDESLOPE_TEST(a_group_its_ties_would_move_too_far_is_made_from_the_fits_instead) {
    const auto [b_lat, b_lon] = offset(0.5, -150.0, 1000.0, 80.0);
    const std::vector<RunwayStrip> strips{laid("A", 0.5, -150.0, 90.0, 2000.0, 16.0, -16.0),
                                          laid("B", b_lat, b_lon, 90.0, 150.0, -16.0, 16.0)};
    CollisionGround ground(sea(), std::make_shared<const RunwaySurfaces>(strips));
    for (std::size_t i = 0; i < 2; ++i) {
        const std::string name = ground.runways().at(i).strip.airport;
        check(ground.own_line(i).from_file, name + ": alone, its file's elevations");
        check(!ground.runways().at(i).ties.empty(), name + ": tied");
        const CollisionGround::Surface s = ground.surface(i);
        std::printf("%s: %.6f m to %.6f m\n", name.c_str(), s.le_m, s.he_m);
        check(std::abs(s.le_m) < 1e-9 && std::abs(s.he_m) < 1e-9,
              name + ": in its group, the fit to the DEM - the sea");
    }
}

// **The runway an aircraft is rolling on is the world's runway under her, the
// end she is rolling towards** (`runway_rolled_on`), for a landing flown by
// hand and handed to the AI on its roll. At Sydney, a C172P rolling at forty
// knots: down 16R and up it (34L), down 16L beside it, along 07 and back
// (25); where 07/25 crosses 16R/34L, the one she is rolling along of the two;
// across 16R, none; on the grass between the parallels, none; and 40 m off
// 16R's centreline towards 16L - off its pavement but on its shoulder, which
// the index reaches - none. **The crossing is built as a pair**: the place is
// on both runways' rectangles, so the one she rolls along must be chosen of
// the two; **and the parallels as a pair**: 16L's place is off 16R's
// rectangle, and the shoulder's on neither.
GLIDESLOPE_TEST(an_aircraft_rolling_on_a_runway_of_the_worlds_is_given_that_runway_towards_where_she_rolls_and_off_one_none) {
    const auto surfaces = glideslope::world::runway_surfaces(data());
    struct End {
        double lat = 0.0;
        double lon = 0.0;
    };
    std::map<std::string, End> ends;
    for (std::size_t i = 0; i < surfaces->size(); ++i) {
        const RunwayStrip& s = surfaces->at(i).strip;
        if (s.airport == "YSSY") {
            ends[s.le_ident] = {s.le_latitude_deg, s.le_longitude_deg};
            ends[s.he_ident] = {s.he_latitude_deg, s.he_longitude_deg};
        }
    }
    check(ends.size() == 6, "Sydney's three runways, six ends");
    const auto between = [](const End& a, const End& b, double t) {
        return End{a.lat + t * (b.lat - a.lat), a.lon + t * (b.lon - a.lon)};
    };
    const auto heading = [](const End& from, const End& to) {
        const double north = to.lat - from.lat;
        const double east = (to.lon - from.lon) * std::cos(from.lat * 3.14159265358979 / 180.0);
        return std::fmod(std::atan2(east, north) * 180.0 / 3.14159265358979 + 360.0, 360.0);
    };
    // Where 07/25 crosses 16R/34L, on the flat in degrees: near enough for
    // the middle of two 45 m runways.
    const End a = ends["16R"], b = ends["34L"], c = ends["07"], d = ends["25"];
    const double denominator =
        (b.lat - a.lat) * (d.lon - c.lon) - (b.lon - a.lon) * (d.lat - c.lat);
    const double t =
        ((c.lat - a.lat) * (d.lon - c.lon) - (c.lon - a.lon) * (d.lat - c.lat)) / denominator;
    const End crossing = between(a, b, t);
    std::size_t sixteen_right = surfaces->size();
    std::size_t oh_seven = surfaces->size();
    std::size_t sixteen_left = surfaces->size();
    for (std::size_t i = 0; i < surfaces->size(); ++i) {
        const RunwayStrip& r = surfaces->at(i).strip;
        if (r.airport == "YSSY") {
            (r.le_ident == "16R" ? sixteen_right
             : r.le_ident == "07" ? oh_seven
                                  : sixteen_left) = i;
        }
    }
    const auto on = [&](std::size_t i, const End& at) {
        return surfaces->place(i, at.lat, at.lon).outside_m <= 0.0;
    };
    check(on(sixteen_right, crossing) && on(oh_seven, crossing),
          "the crossing is on both 16R/34L's rectangle and 07/25's");
    struct Case {
        std::string where;
        End at;
        double heading_deg;
        std::string expected; // "" for none
    };
    const End middle_16r = between(a, b, 0.5);
    const End middle_16l = between(ends["16L"], ends["34R"], 0.5);
    // Forty metres from 16R's centreline square to it, towards 16L (east,
    // on its left looking down it).
    const double towards_16l = (heading(a, b) - 90.0) * 3.14159265358979 / 180.0;
    const End shoulder_16r{
        middle_16r.lat + 40.0 * std::cos(towards_16l) / 111132.0,
        middle_16r.lon + 40.0 * std::sin(towards_16l) /
                             (111320.0 * std::cos(middle_16r.lat * 3.14159265358979 / 180.0))};
    std::printf("  16R at 16L's middle %.1f m out, 16L at 16R's %.1f, the shoulder %.1f and %.1f, "
                "16L's middle %.1f\n",
                surfaces->place(sixteen_right, middle_16l.lat, middle_16l.lon).outside_m,
                surfaces->place(sixteen_left, middle_16r.lat, middle_16r.lon).outside_m,
                surfaces->place(sixteen_right, shoulder_16r.lat, shoulder_16r.lon).outside_m,
                surfaces->place(sixteen_left, shoulder_16r.lat, shoulder_16r.lon).outside_m,
                surfaces->place(sixteen_left, middle_16l.lat, middle_16l.lon).outside_m);
    check(!on(sixteen_right, middle_16l) && !on(sixteen_left, middle_16r) &&
              !on(sixteen_right, shoulder_16r) && !on(sixteen_left, shoulder_16r) &&
              on(sixteen_left, middle_16l),
          "the parallels are a pair: each place on its own rectangle and off the other's, "
          "and 16R's shoulder on neither");
    const std::vector<Case> cases{
        {"down 16R", between(a, b, 0.3), heading(a, b), "YSSY 16R"},
        {"up 34L", between(a, b, 0.7), heading(b, a), "YSSY 34L"},
        {"down 16L", middle_16l, heading(ends["16L"], ends["34R"]), "YSSY 16L"},
        {"along 07", between(c, d, 0.8), heading(c, d), "YSSY 07"},
        {"along 25", between(c, d, 0.8), heading(d, c), "YSSY 25"},
        {"down 16R at the crossing", crossing, heading(a, b), "YSSY 16R"},
        {"along 07 at the crossing", crossing, heading(c, d), "YSSY 07"},
        {"across 16R", middle_16r, std::fmod(heading(a, b) + 90.0, 360.0), ""},
        {"between the parallels", between(middle_16r, middle_16l, 0.5), heading(a, b), ""},
        {"on 16R's shoulder", shoulder_16r, heading(a, b), ""},
    };
    const auto ground = [](double, double) { return 6.0; };
    std::size_t walked = 0;
    for (const Case& k : cases) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", "c172p");
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 6.0; }, [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = k.at.lat;
        ic.longitude_deg = k.at.lon;
        ic.altitude_ft = 6.0 * feet_per_metre;
        ic.terrain_elevation_ft = ic.altitude_ft;
        ic.heading_deg = k.heading_deg;
        ic.airspeed_kts = 40.0;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        const auto found = glideslope::world::runway_rolled_on(*surfaces, aircraft, ground);
        const std::string got = found ? found->name : "";
        std::printf("  %-26s heading %5.1f: %s\n", k.where.c_str(), k.heading_deg,
                    found ? got.c_str() : "none");
        check(got == k.expected, k.where + ": given '" + got + "', not '" + k.expected + "'");
        if (found) {
            check(std::abs(found->elevation_ft - 6.0 * feet_per_metre) < 1e-9 &&
                      found->length_m > 1000.0,
                  k.where + ": the runway's elevation is the ground's at its threshold, and "
                            "its length the runway's");
        }
        ++walked;
    }
    check(walked == 10 && cases.size() == 10, "ten cases walked of ten");
}
