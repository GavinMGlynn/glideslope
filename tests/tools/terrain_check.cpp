// glideslope_terrain_check - holds a frame of the terrain to the DEM itself.
//
//   glideslope_terrain_check FRAME.bmp REFERENCE.bmp DIFFERENCE.bmp
//                            LAT,LON,HEIGHT LAT,LON,HEIGHT CACHE DATA
//
// FRAME is what `glideslope --screen terrain --at EYE --toward TARGET --shot`
// wrote. This makes the reference frame of the same view without Cesium Native,
// the tiles, the meshes or the GPU: a ray from the eye through every pixel's
// centre, marched through the Copernicus DEM - the tiles and geoid in CACHE,
// the coverage in DATA - over the same region, the whole-degree cell the eye is
// in, until it meets the ground; the pixel is the terrain's colour there
// (gfx/terrain_colour.hpp), from the DEM's height and its slope over 30 m, or
// the sky's.
//
// The Copernicus DEM's notice must be along the bottom of the frame, as
// gfx::credit_lines draws it; those rows are left out of what is compared.
// Above them, the two must agree within the tolerances below, which are stated
// in PROJECT_STATUS.md: where the skyline is, whether each pixel is ground or
// sky, and the colour of the ground. REFERENCE is written, and DIFFERENCE, the
// difference four times over, for looking at. Exits 0 if they agree, 1 with the numbers
// if not, 2 on bad arguments.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "gfx/scene.hpp"
#include "gfx/terrain_colour.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/geodesy.hpp"
#include "world/geoid.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace {

// The tolerances, set from what was measured (PROJECT_STATUS.md), over the rows
// above the notice. The frame as drawn is 0.12 pixels out along its skyline,
// agrees on 99.94% of pixels, and differs in colour by 4.76 on average and 23
// at the 95th percentile. Drawing coarser tiles than the view needs, or the
// terrain 20 m too high, each broke at least two of these: the skyline 0.30 to
// 0.48 pixels out, the colour 6.1 to 7.8 on average.
constexpr double most_mean_skyline = 0.25;   // pixels, over the columns
constexpr int most_skyline = 2;              // pixels, in any column
constexpr double least_agreement = 0.995;    // of pixels, ground or sky alike
constexpr double most_mean_difference = 5.5; // of 255, over ground in both
constexpr int most_p95_difference = 28;      // of 255, the largest channel's

constexpr double radians = 3.14159265358979323846 / 180.0;

using glideslope::world::Ecef;
using glideslope::world::Geodetic;

[[noreturn]] void fail(const std::string& why, int status = 1) {
    std::fprintf(stderr, "glideslope_terrain_check: %s\n", why.c_str());
    std::exit(status);
}

glideslope::gfx::Frame load(const char* path) {
    SDL_Surface* loaded = SDL_LoadBMP(path);
    if (loaded == nullptr) {
        fail(std::string("cannot read ") + path + ": " + SDL_GetError());
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == nullptr) {
        fail(std::string("cannot convert ") + path + ": " + SDL_GetError());
    }
    glideslope::gfx::Frame frame;
    frame.width = rgba->w;
    frame.height = rgba->h;
    frame.rgba.resize(static_cast<std::size_t>(rgba->w * rgba->h) * 4);
    for (int y = 0; y < rgba->h; ++y) {
        const auto* row =
            static_cast<const std::uint8_t*>(rgba->pixels) + y * rgba->pitch;
        std::copy(row, row + rgba->w * 4,
                  frame.rgba.begin() + static_cast<long>(y) * rgba->w * 4);
    }
    SDL_DestroySurface(rgba);
    return frame;
}

Geodetic parse(const char* text) {
    Geodetic g;
    if (std::sscanf(text, "%lf,%lf,%lf", &g.latitude_deg, &g.longitude_deg,
                    &g.height_m) != 3) {
        fail(std::string("not LAT,LON,HEIGHT: ") + text, 2);
    }
    return g;
}

std::array<std::uint8_t, 3> bytes(const std::array<float, 4>& c) {
    const auto b = [](float v) {
        return static_cast<std::uint8_t>(
            std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    };
    return {b(c[0]), b(c[1]), b(c[2])};
}

struct Region {
    double south;
    double west;
    double north;
    double east;
    bool holds(const Geodetic& g) const {
        return g.latitude_deg >= south && g.latitude_deg <= north &&
               g.longitude_deg >= west && g.longitude_deg <= east;
    }
};

// One thread's DEM: the DEM is not thread-safe, and each keeps its own cache.
struct Ground {
    glideslope::world::DownloadedTiles tiles;
    glideslope::world::Dem dem;
    Ground(const glideslope::world::DemCoverage& coverage,
           const glideslope::world::Geoid& geoid, const std::filesystem::path& cache)
        : tiles(cache, glideslope::world::http_fetch()), dem(coverage, tiles, &geoid) {}
};

// The colour of the ground a ray from `eye` along unit `d` meets first within
// `region`, or nothing if it meets none. `pixel` is the angle a pixel spans.
std::optional<std::array<std::uint8_t, 3>> cast(Ground& ground, const Region& region,
                                                const Ecef& eye, const Ecef& d,
                                                double pixel) {
    const auto at = [&](double t) {
        return glideslope::world::to_geodetic(
            {eye.x + d.x * t, eye.y + d.y * t, eye.z + d.z * t});
    };
    const auto clearance = [&](const Geodetic& g) {
        return g.height_m -
               ground.dem.height_above_ellipsoid(g.latitude_deg, g.longitude_deg);
    };
    double previous = 0.0;
    double t = 1.0; // the renderer's near plane on the terrain screen
    for (int steps = 0; steps < 200000; ++steps) {
        const Geodetic g = at(t);
        if (!region.holds(g) || g.height_m > 9000.0) {
            // Out of the region, or above any ground: rays go straight, and
            // over a region this size neither comes back.
            return std::nullopt;
        }
        const double above = clearance(g);
        if (above <= 0.0) {
            // Between the last point above the ground and this one below it.
            double low = previous;
            double high = t;
            for (int i = 0; i < 30; ++i) {
                const double mid = (low + high) / 2;
                (clearance(at(mid)) > 0.0 ? low : high) = mid;
            }
            const Geodetic hit = at(high);
            const double lat = hit.latitude_deg;
            const double lon = hit.longitude_deg;
            // The slope over 30 m each way, as a normal in east, north and up.
            const double de = 30.0 / (glideslope::world::Wgs84::a *
                                      std::cos(lat * radians) * radians);
            const double dn = 30.0 / (glideslope::world::Wgs84::a * radians);
            const double east = (ground.dem.height_above_ellipsoid(lat, lon + de) -
                                 ground.dem.height_above_ellipsoid(lat, lon - de)) /
                                60.0;
            const double north = (ground.dem.height_above_ellipsoid(lat + dn, lon) -
                                  ground.dem.height_above_ellipsoid(lat - dn, lon)) /
                                 60.0;
            const double length = std::sqrt(east * east + north * north + 1.0);
            const double ne = -east / length;
            const double nn = -north / length;
            const double nu = 1.0 / length;
            const double sp = std::sin(lat * radians);
            const double cp = std::cos(lat * radians);
            const double sl = std::sin(lon * radians);
            const double cl = std::cos(lon * radians);
            const Ecef normal{-sl * ne - sp * cl * nn + cp * cl * nu,
                              cl * ne - sp * sl * nn + cp * sl * nu, cp * nn + sp * nu};
            return bytes(glideslope::gfx::terrain_colour(
                ground.dem.height_above_geoid(lat, lon), normal,
                glideslope::gfx::up_at(lat, lon)));
        }
        previous = t;
        // Half the height above the ground can be flown without passing through
        // any slope under 63 degrees; and no step need be finer than a
        // quarter of what a pixel spans there.
        t += std::clamp(std::max(0.5 * above, 0.25 * t * pixel), 1.0, 1000.0);
    }
    return std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 8) {
        std::fputs(
            "usage: glideslope_terrain_check FRAME.bmp REFERENCE.bmp DIFFERENCE.bmp "
            "EYE TARGET CACHE DATA\n",
            stderr);
        return 2;
    }
    const glideslope::gfx::Frame frame = load(argv[1]);
    const Geodetic eye = parse(argv[4]);
    const Geodetic target = parse(argv[5]);
    const std::filesystem::path cache = argv[6];
    const std::filesystem::path data = argv[7];

    std::ifstream coverage_file(data / "dem" / "coverage.txt", std::ios::binary);
    if (!coverage_file) {
        fail("cannot read the coverage in " + data.string(), 2);
    }
    const glideslope::world::DemCoverage coverage(
        std::string(std::istreambuf_iterator<char>(coverage_file), {}));
    const glideslope::world::Geoid geoid =
        glideslope::world::egm2008_geoid(cache, glideslope::world::http_fetch());
    // The cell the eye is in, as the client's terrain screen draws.
    const double south = std::ceil(eye.latitude_deg) - 1.0;
    const double west = std::floor(eye.longitude_deg);
    const Region region{south, west, south + 1.0, west + 1.0};

    // The same camera the client makes.
    glideslope::gfx::Camera camera = glideslope::gfx::look_at(
        glideslope::world::to_ecef(eye), glideslope::world::to_ecef(target));
    const auto& axes = camera.world_from_camera.m;
    const double tan_half = std::tan(camera.vertical_fov_rad / 2.0);
    const double aspect = static_cast<double>(frame.width) / frame.height;
    const double pixel = camera.vertical_fov_rad / frame.height;

    glideslope::gfx::Frame reference;
    reference.width = frame.width;
    reference.height = frame.height;
    reference.rgba.assign(frame.rgba.size(), 255);
    std::vector<std::uint8_t> ground_here(static_cast<std::size_t>(frame.width) *
                                          static_cast<std::size_t>(frame.height));
    const auto sky = bytes(
        {glideslope::gfx::sky.r, glideslope::gfx::sky.g, glideslope::gfx::sky.b, 1.0f});
    std::atomic<int> next_row{0};
    const auto work = [&] {
        Ground ground(coverage, geoid, cache);
        for (int y = next_row++; y < frame.height; y = next_row++) {
            for (int x = 0; x < frame.width; ++x) {
                // The pixel's centre, from the top left, in the camera's frame:
                // right, up, and forward along its negative back axis.
                const double cx =
                    (2.0 * (x + 0.5) / frame.width - 1.0) * tan_half * aspect;
                const double cy = (1.0 - 2.0 * (y + 0.5) / frame.height) * tan_half;
                Ecef d{axes[0] * cx + axes[3] * cy - axes[6],
                       axes[1] * cx + axes[4] * cy - axes[7],
                       axes[2] * cx + axes[5] * cy - axes[8]};
                const double length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
                d = {d.x / length, d.y / length, d.z / length};
                const auto hit = cast(ground, region, camera.position, d, pixel);
                const auto at = static_cast<std::size_t>(y * frame.width + x);
                const auto& rgb = hit ? *hit : sky;
                std::copy(rgb.begin(), rgb.end(),
                          reference.rgba.begin() + static_cast<std::ptrdiff_t>(at * 4));
                ground_here[at] = hit ? 1 : 0;
            }
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(work);
    }
    for (std::thread& t : threads) {
        t.join();
    }
    glideslope::gfx::save_bmp(reference, argv[2]);

    // The notice, read back; the rows it is on are not compared.
    const auto notice = glideslope::gfx::credit_lines(
        {glideslope::world::copernicus_dem_notice}, frame.width);
    const auto notice_layout =
        glideslope::gfx::credit_layout(frame.width, frame.height, notice.size());
    const auto shown =
        glideslope::gfx::read_text(frame, notice_layout, notice.size(),
                                   glideslope::gfx::credit_columns(frame.width));
    for (std::size_t i = 0; i < notice.size(); ++i) {
        std::printf("notice: %s\n", shown[i].c_str());
        if (shown[i] != notice[i]) {
            fail("the DEM's notice, line " + std::to_string(i + 1) + ", reads \"" +
                 shown[i] + "\", not \"" + notice[i] + "\"");
        }
    }
    const int rows = notice_layout.top;
    const std::size_t compared = static_cast<std::size_t>(rows * frame.width);

    // Compare.
    glideslope::gfx::Frame difference = reference;
    std::size_t agree = 0;
    std::size_t reference_ground = 0;
    std::size_t both_ground = 0;
    double total_difference = 0.0;
    std::vector<int> largest;
    for (std::size_t i = 0; i < compared; ++i) {
        const auto* f = &frame.rgba[i * 4];
        const auto* r = &reference.rgba[i * 4];
        const bool frame_sky = std::abs(f[0] - sky[0]) <= 1 &&
                               std::abs(f[1] - sky[1]) <= 1 &&
                               std::abs(f[2] - sky[2]) <= 1;
        const bool frame_ground = !frame_sky;
        reference_ground += ground_here[i];
        agree += (frame_ground == (ground_here[i] != 0)) ? 1u : 0u;
        int worst = 0;
        for (std::size_t c = 0; c < 3; ++c) {
            const int delta = std::abs(static_cast<int>(f[c]) - static_cast<int>(r[c]));
            worst = std::max(worst, delta);
            difference.rgba[i * 4 + c] =
                static_cast<std::uint8_t>(std::min(255, delta * 4));
            if (frame_ground && ground_here[i] != 0) {
                total_difference += delta;
            }
        }
        if (frame_ground && ground_here[i] != 0) {
            ++both_ground;
            largest.push_back(worst);
        }
    }
    glideslope::gfx::save_bmp(difference, argv[3]);

    // The skyline: in each column, the first row from the top that is ground.
    const auto skyline = [&](const auto& is_ground) {
        std::vector<int> line(static_cast<std::size_t>(frame.width), rows);
        for (int x = 0; x < frame.width; ++x) {
            for (int y = 0; y < rows; ++y) {
                if (is_ground(static_cast<std::size_t>(y * frame.width + x))) {
                    line[static_cast<std::size_t>(x)] = y;
                    break;
                }
            }
        }
        return line;
    };
    const std::vector<int> drawn_skyline = skyline([&](std::size_t i) {
        const auto* f = &frame.rgba[i * 4];
        return !(std::abs(f[0] - sky[0]) <= 1 && std::abs(f[1] - sky[1]) <= 1 &&
                 std::abs(f[2] - sky[2]) <= 1);
    });
    const std::vector<int> cast_skyline =
        skyline([&](std::size_t i) { return ground_here[i] != 0; });
    double skyline_total = 0.0;
    int skyline_worst = 0;
    for (std::size_t x = 0; x < drawn_skyline.size(); ++x) {
        const int off = std::abs(drawn_skyline[x] - cast_skyline[x]);
        skyline_total += off;
        skyline_worst = std::max(skyline_worst, off);
    }
    const double skyline_mean =
        skyline_total / static_cast<double>(drawn_skyline.size());
    std::printf(
        "the skyline: %.2f pixels out on average (at most %.2f), %d in the worst "
        "column (at most %d)\n",
        skyline_mean, most_mean_skyline, skyline_worst, most_skyline);

    const double pixels = static_cast<double>(compared);
    const double agreement = static_cast<double>(agree) / pixels;
    const double ground_share = static_cast<double>(reference_ground) / pixels;
    const double mean =
        both_ground == 0 ? 255.0
                         : total_difference / (3.0 * static_cast<double>(both_ground));
    std::sort(largest.begin(), largest.end());
    const int p95 = largest.empty() ? 255 : largest[largest.size() * 95 / 100];
    std::printf("ground in %.1f%% of the reference; ground or sky alike in %.2f%% of "
                "pixels (at least %.1f%%)\n",
                100.0 * ground_share, 100.0 * agreement, 100.0 * least_agreement);
    std::printf(
        "where both are ground: mean difference %.2f of 255 (at most %.1f), 95th "
        "percentile of the largest channel %d (at most %d)\n",
        mean, most_mean_difference, p95, most_p95_difference);
    if (ground_share < 0.1 || ground_share > 0.9) {
        fail("the view is not a test of anything: ground in " +
             std::to_string(100.0 * ground_share) + "% of it");
    }
    if (skyline_mean > most_mean_skyline || skyline_worst > most_skyline ||
        agreement < least_agreement || mean > most_mean_difference ||
        p95 > most_p95_difference) {
        fail("the frame does not match the DEM ray-cast from the same eye");
    }
    std::printf("the frame matches the DEM ray-cast from the same eye\n");
    return 0;
}
