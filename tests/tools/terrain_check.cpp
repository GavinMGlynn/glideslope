// glideslope_terrain_check - holds a frame of the terrain to the DEM itself.
//
//   glideslope_terrain_check FRAME.bmp REFERENCE.bmp DIFFERENCE.bmp
//                            LAT,LON,HEIGHT LAT,LON,HEIGHT CACHE DATA [imagery]
//
// FRAME is what `glideslope --screen terrain --at EYE --toward TARGET --shot`
// wrote. This makes the reference frame of the same view without Cesium Native,
// the tiles, the meshes or the GPU: a ray from the eye through every pixel's
// centre, marched through the Copernicus DEM - the tiles and geoid in CACHE,
// the coverage in DATA - over the same region, the whole-degree cell the eye is
// in, until it meets the ground. The pixel is the sky's colour if it meets
// none; if it does, the terrain's colour there (gfx/terrain_colour.hpp), from
// the DEM's height and its slope over 30 m - or, with `imagery`, that slope's
// light times the open imagery at that place, fetched from the imagery service
// at the level whose pixels are the size the frame's pixel covers there.
//
// The DEM's notice, and with `imagery` the imagery's credit, must be along the
// bottom of the frame, as gfx::credit_lines draws them; those rows are left out
// of what is compared. Above them, the two must agree within the tolerances
// below, which are stated in PROJECT_STATUS.md: where the skyline is, whether
// each pixel is ground or sky, and the colour of the ground - and with
// imagery, the frame must match the reference better where it is than moved
// four pixels any way, so the imagery is where it belongs. REFERENCE is
// written, and DIFFERENCE, the difference four times over, for looking at.
// Exits 0 if they agree, 1 with the numbers if not, 2 on bad arguments.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "gfx/scene.hpp"
#include "gfx/terrain_colour.hpp"
#include "gfx/terrain_tiles.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/geodesy.hpp"
#include "world/geoid.hpp"

#include <CesiumImage/ImageDecoder.h>
#include <CesiumImage/Ktx2TranscodeTargets.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace {

struct Tolerances {
    double mean_skyline;    // pixels out, over the columns
    int skyline;            // pixels out, in any column
    double agreement;       // of pixels, ground or sky alike
    double mean_difference; // of 255, over ground in both
    int p95_difference;     // of 255, the largest channel's
};

// The tolerances, set from what was measured (PROJECT_STATUS.md), over the rows
// above the credits. Tinted by height, the frame as drawn is 0.12 pixels out
// along its skyline, agrees on 99.94% of pixels, and differs in colour by 4.76
// on average and 23 at the 95th percentile. Drawing coarser tiles than the
// view needs, or the terrain 20 m too high, each broke at least two of these:
// the skyline 0.30 to 0.48 pixels out, the colour 6.1 to 7.8 on average.
constexpr Tolerances tinted{0.25, 2, 0.995, 5.5, 28};
// With imagery, the geometry is the same and the colour is the imagery's, whose
// level the renderer and the reference choose separately.
constexpr Tolerances imaged{0.35, 3, 0.995, 3.0, 12};

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

// Where a ray meets the ground: the place, the slope there as a unit normal,
// and how far along the ray.
struct Hit {
    double latitude_deg;
    double longitude_deg;
    Ecef normal;
    double distance_m;
};

// Where a ray from `eye` along unit `d` meets the ground first within `region`,
// if it does. `pixel` is the angle a pixel spans.
std::optional<Hit> cast(Ground& ground, const Region& region, const Ecef& eye,
                        const Ecef& d, double pixel) {
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
            return Hit{lat, lon,
                       Ecef{-sl * ne - sp * cl * nn + cp * cl * nu,
                            cl * ne - sp * sl * nn + cp * sl * nu, cp * nn + sp * nu},
                       high};
        }
        previous = t;
        // Half the height above the ground can be flown without passing through
        // any slope under 63 degrees; and no step need be finer than a
        // quarter of what a pixel spans there.
        t += std::clamp(std::max(0.5 * above, 0.25 * t * pixel), 1.0, 1000.0);
    }
    return std::nullopt;
}

// The open imagery's tiles, fetched once into CACHE and decoded, shared by the
// threads. Its tile matrix set is latitude and longitude: at level L, 2^(L+1)
// tiles across and 2^L down from 90 N, 180 W, each 256 pixels square.
class ImageryTiles {
public:
    explicit ImageryTiles(std::filesystem::path cache) : cache_(std::move(cache)) {}

    // The imagery's colour at a place, at `level`, bilinear between pixels.
    std::array<double, 3> sample(double latitude_deg, double longitude_deg,
                                 unsigned level) {
        const double tile_deg = 180.0 / std::ldexp(1.0, static_cast<int>(level));
        const double fx = (longitude_deg + 180.0) / tile_deg;
        const double fy = (90.0 - latitude_deg) / tile_deg;
        const auto col = static_cast<long>(std::floor(fx));
        const auto row = static_cast<long>(std::floor(fy));
        const Tile& tile = get(level, row, col);
        const double px = std::clamp((fx - static_cast<double>(col)) * 256.0 - 0.5, 0.0,
                                     static_cast<double>(tile.width - 1));
        const double py = std::clamp((fy - static_cast<double>(row)) * 256.0 - 0.5, 0.0,
                                     static_cast<double>(tile.height - 1));
        const int x0 = static_cast<int>(px);
        const int y0 = static_cast<int>(py);
        const int x1 = std::min(x0 + 1, tile.width - 1);
        const int y1 = std::min(y0 + 1, tile.height - 1);
        const double ax = px - x0;
        const double ay = py - y0;
        std::array<double, 3> out{};
        for (std::size_t c = 0; c < 3; ++c) {
            const auto v = [&](int x, int y) {
                return static_cast<double>(
                    tile.rgb[(static_cast<std::size_t>(y) *
                                  static_cast<std::size_t>(tile.width) +
                              static_cast<std::size_t>(x)) *
                                 3 +
                             c]);
            };
            out[c] = (v(x0, y0) * (1 - ax) + v(x1, y0) * ax) * (1 - ay) +
                     (v(x0, y1) * (1 - ax) + v(x1, y1) * ax) * ay;
        }
        return out;
    }

private:
    struct Tile {
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> rgb;
    };

    const Tile& get(unsigned level, long row, long col) {
        const std::lock_guard lock(mutex_);
        const auto key = std::make_tuple(level, row, col);
        const auto found = tiles_.find(key);
        if (found != tiles_.end()) {
            return *found->second;
        }
        const glideslope::gfx::Imagery imagery = glideslope::gfx::open_imagery();
        const std::filesystem::path path = cache_ / "imagery-check" / imagery.layer /
                                           std::to_string(level) / std::to_string(row) /
                                           (std::to_string(col) + ".jpg");
        std::vector<std::byte> bytes;
        if (std::ifstream in{path, std::ios::binary}) {
            const std::string text(std::istreambuf_iterator<char>(in), {});
            bytes.resize(text.size());
            std::memcpy(bytes.data(), text.data(), text.size());
        } else {
            std::string url = imagery.url;
            for (const auto& [name, value] :
                 {std::pair<std::string, std::string>{"{Layer}", imagery.layer},
                  {"{Style}", imagery.style},
                  {"{TileMatrixSet}", imagery.tile_matrix_set},
                  {"{TileMatrix}", std::to_string(level)},
                  {"{TileRow}", std::to_string(row)},
                  {"{TileCol}", std::to_string(col)}}) {
                url.replace(url.find(name), name.size(), value);
            }
            const auto response = glideslope::world::fetch_with_retries(
                glideslope::world::http_fetch(), url);
            if (response.status != 200) {
                fail(url + " answered " + std::to_string(response.status));
            }
            bytes.resize(response.body.size());
            std::memcpy(bytes.data(), response.body.data(), response.body.size());
            std::filesystem::create_directories(path.parent_path());
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(response.body.data()),
                      static_cast<std::streamsize>(response.body.size()));
        }
        const auto decoded = CesiumImage::ImageDecoder::readImage(
            bytes, CesiumImage::Ktx2TranscodeTargets{});
        if (!decoded.pImage || decoded.pImage->bytesPerChannel != 1 ||
            decoded.pImage->channels < 3) {
            fail("an imagery tile would not decode: " + path.string());
        }
        const auto& image = *decoded.pImage;
        auto tile = std::make_unique<Tile>();
        tile->width = image.width;
        tile->height = image.height;
        const auto count = static_cast<std::size_t>(image.width) *
                           static_cast<std::size_t>(image.height);
        tile->rgb.resize(count * 3);
        const auto channels = static_cast<std::size_t>(image.channels);
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t c = 0; c < 3; ++c) {
                tile->rgb[i * 3 + c] =
                    static_cast<std::uint8_t>(image.pixelData[i * channels + c]);
            }
        }
        return *tiles_.emplace(key, std::move(tile)).first->second;
    }

    std::filesystem::path cache_;
    std::mutex mutex_;
    std::map<std::tuple<unsigned, long, long>, std::unique_ptr<Tile>> tiles_;
};

} // namespace

int main(int argc, char** argv) {
    const bool with_imagery = argc == 9 && std::string(argv[8]) == "imagery";
    if (argc != 8 && !with_imagery) {
        std::fputs(
            "usage: glideslope_terrain_check FRAME.bmp REFERENCE.bmp DIFFERENCE.bmp "
            "EYE TARGET CACHE DATA [imagery]\n",
            stderr);
        return 2;
    }
    const Tolerances& tolerances = with_imagery ? imaged : tinted;
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
    ImageryTiles imagery(cache);
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
                std::array<std::uint8_t, 3> rgb = sky;
                if (hit) {
                    const Ecef up =
                        glideslope::gfx::up_at(hit->latitude_deg, hit->longitude_deg);
                    if (with_imagery) {
                        // The level whose pixels are the ground this pixel
                        // covers there: 180 / 2^L / 256 degrees, 78 km / 2^L.
                        const double covered = hit->distance_m * pixel;
                        const double level = std::round(std::log2(78271.5 / covered));
                        const auto chosen = static_cast<unsigned>(std::clamp(
                            level, 0.0,
                            static_cast<double>(
                                glideslope::gfx::open_imagery().maximum_level)));
                        const auto image = imagery.sample(hit->latitude_deg,
                                                          hit->longitude_deg, chosen);
                        const double light =
                            glideslope::gfx::terrain_light(hit->normal, up);
                        for (std::size_t c = 0; c < 3; ++c) {
                            rgb[c] = static_cast<std::uint8_t>(
                                std::lround(std::clamp(image[c] * light, 0.0, 255.0)));
                        }
                    } else {
                        rgb = bytes(glideslope::gfx::terrain_colour(
                            ground.dem.height_above_geoid(hit->latitude_deg,
                                                          hit->longitude_deg),
                            hit->normal, up));
                    }
                }
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
    std::vector<std::string> credits{glideslope::world::copernicus_dem_notice};
    if (with_imagery) {
        credits.push_back(glideslope::gfx::open_imagery().credit);
    }
    const auto notice = glideslope::gfx::credit_lines(credits, frame.width);
    const auto notice_layout =
        glideslope::gfx::credit_layout(frame.width, frame.height, notice.size());
    const auto shown =
        glideslope::gfx::read_text(frame, notice_layout, notice.size(),
                                   glideslope::gfx::credit_columns(frame.width));
    for (std::size_t i = 0; i < notice.size(); ++i) {
        std::printf("notice: %s\n", shown[i].c_str());
        if (shown[i] != notice[i]) {
            fail("the credits, line " + std::to_string(i + 1) + ", read \"" + shown[i] +
                 "\", not \"" + notice[i] + "\"");
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
        skyline_mean, tolerances.mean_skyline, skyline_worst, tolerances.skyline);

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
                100.0 * ground_share, 100.0 * agreement, 100.0 * tolerances.agreement);
    std::printf(
        "where both are ground: mean difference %.2f of 255 (at most %.1f), 95th "
        "percentile of the largest channel %d (at most %d)\n",
        mean, tolerances.mean_difference, p95, tolerances.p95_difference);
    // With imagery: whether it is where it belongs. Both frames are blurred by a
    // 4-pixel box, and the drawn one moved up to 3 pixels each way over the
    // reference; where the ground matches best must be within a pixel of where
    // it was drawn. A pixel's error there is a few metres at this range.
    bool aligned = true;
    if (with_imagery) {
        constexpr int box = 4;
        constexpr int reach = 3;
        const int w = frame.width;
        const auto blurred = [&](const glideslope::gfx::Frame& f) {
            // Summed-area tables, one per channel, and the ground's likewise.
            const auto stride = static_cast<std::size_t>(w + 1);
            std::vector<double> sums(stride * static_cast<std::size_t>(rows + 1) * 4,
                                     0.0);
            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < w; ++x) {
                    const auto i = static_cast<std::size_t>(y * w + x);
                    for (std::size_t c = 0; c < 4; ++c) {
                        const double v = c < 3 ? static_cast<double>(f.rgba[i * 4 + c])
                                               : static_cast<double>(ground_here[i]);
                        const auto at = [&](int yy, int xx) -> double& {
                            return sums[(static_cast<std::size_t>(yy) * stride +
                                         static_cast<std::size_t>(xx)) *
                                            4 +
                                        c];
                        };
                        at(y + 1, x + 1) = v + at(y, x + 1) + at(y + 1, x) - at(y, x);
                    }
                }
            }
            return sums;
        };
        const auto frame_sums = blurred(frame);
        const auto reference_sums = blurred(reference);
        const auto box_mean = [&](const std::vector<double>& sums, int x, int y,
                                  std::size_t c) {
            const auto stride = static_cast<std::size_t>(w + 1);
            const auto at = [&](int yy, int xx) {
                return sums[(static_cast<std::size_t>(yy) * stride +
                             static_cast<std::size_t>(xx)) *
                                4 +
                            c];
            };
            return (at(y + box, x + box) - at(y, x + box) - at(y + box, x) + at(y, x)) /
                   (box * box);
        };
        const auto out_by = [&](int dx, int dy) {
            double total = 0.0;
            long count = 0;
            for (int y = reach; y + box + reach <= rows; ++y) {
                for (int x = reach; x + box + reach <= w; ++x) {
                    // Only where the box is ground throughout, in both.
                    if (box_mean(reference_sums, x, y, 3) < 1.0 ||
                        box_mean(frame_sums, x + dx, y + dy, 3) < 1.0) {
                        continue;
                    }
                    for (std::size_t c = 0; c < 3; ++c) {
                        total += std::abs(box_mean(frame_sums, x + dx, y + dy, c) -
                                          box_mean(reference_sums, x, y, c));
                    }
                    ++count;
                }
            }
            return count == 0 ? 255.0 : total / (3.0 * static_cast<double>(count));
        };
        int best_x = 0;
        int best_y = 0;
        double best = out_by(0, 0);
        const double here = best;
        for (int dy = -reach; dy <= reach; ++dy) {
            for (int dx = -reach; dx <= reach; ++dx) {
                const double d = out_by(dx, dy);
                if (d < best) {
                    best = d;
                    best_x = dx;
                    best_y = dy;
                }
            }
        }
        std::printf(
            "the imagery, blurred 4 pixels: %.2f out where it is; the best match "
            "%.2f, %d pixels across and %d down (at most 1 each)\n",
            here, best, best_x, best_y);
        aligned = std::abs(best_x) <= 1 && std::abs(best_y) <= 1;
    }

    if (ground_share < 0.1 || ground_share > 0.9) {
        fail("the view is not a test of anything: ground in " +
             std::to_string(100.0 * ground_share) + "% of it");
    }
    if (skyline_mean > tolerances.mean_skyline || skyline_worst > tolerances.skyline ||
        agreement < tolerances.agreement || mean > tolerances.mean_difference ||
        p95 > tolerances.p95_difference || !aligned) {
        fail(std::string("the frame does not match the ") +
             (with_imagery ? "DEM and imagery" : "DEM") +
             " ray-cast from the same eye");
    }
    std::printf("the frame matches the %s ray-cast from the same eye\n",
                with_imagery ? "DEM and imagery" : "DEM");
    return 0;
}
