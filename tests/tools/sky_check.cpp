// glideslope_sky_check - what a frame of the weather shows.
//
//   glideslope_sky_check FRAME.bmp cloud|sky|whiteout|ground
//   glideslope_sky_check FRAME.bmp rain DRY.bmp
//
// FRAME is what `glideslope --screen terrain --imagery off --metar ... --shot`
// wrote; the rows the DEM's notice is drawn on, along its bottom, are left
// out. Every other pixel is judged by its colour alone:
//
//   cloud     at least 95% of them grey - no channel more than 20 from another
//             - as a cloud's base, and the haze, are;
//   sky       at least 95% of them blue - blue at least 60 above red - as a
//             clear sky is;
//   whiteout  at least 99% of them within 4 of the frame's median in every
//             channel, and that grey: inside cloud, nothing is seen;
//   ground    at least 30% of them greener than blue by 10 or more, as the
//             ground tinted by height below 1,000 m is: it is seen;
//   rain      at least 0.3% and at most 30% of them differ from DRY, the same
//             view with nothing falling, by more than 10 in a channel, and
//             most of those are lighter: the streaks.
//
// Exits 0 if the frame shows it, 1 with the numbers if not, 2 on bad
// arguments.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "world/dem.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

[[noreturn]] void fail(const std::string& why, int status = 1) {
    std::fprintf(stderr, "glideslope_sky_check: %s\n", why.c_str());
    std::exit(status);
}

glideslope::gfx::Frame load(const char* path) {
    SDL_Surface* loaded = SDL_LoadBMP(path);
    if (loaded == nullptr) {
        fail(std::string("cannot read ") + path + ": " + SDL_GetError(), 2);
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == nullptr) {
        fail(std::string("cannot convert ") + path + ": " + SDL_GetError(), 2);
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

bool grey(const std::uint8_t* p) {
    const int r = p[0];
    const int g = p[1];
    const int b = p[2];
    return std::max({std::abs(r - g), std::abs(g - b), std::abs(r - b)}) <= 20;
}

} // namespace

int main(int argc, char** argv) {
    const std::string kind = argc >= 3 ? argv[2] : "";
    const bool rain = kind == "rain" && argc == 4;
    if (!rain && !(argc == 3 && (kind == "cloud" || kind == "sky" ||
                                 kind == "whiteout" || kind == "ground"))) {
        std::fputs("usage: glideslope_sky_check FRAME.bmp cloud|sky|whiteout|ground\n"
                   "       glideslope_sky_check FRAME.bmp rain DRY.bmp\n",
                   stderr);
        return 2;
    }
    const glideslope::gfx::Frame frame = load(argv[1]);

    // Above the DEM's notice.
    const auto notice = glideslope::gfx::credit_lines(
        {glideslope::world::copernicus_dem_notice}, frame.width);
    const int rows =
        glideslope::gfx::credit_layout(frame.width, frame.height, notice.size()).top;
    const auto pixels = static_cast<std::size_t>(rows * frame.width);
    if (pixels == 0) {
        fail("the notice fills the frame");
    }
    const auto share = [&](auto&& is) {
        std::size_t n = 0;
        for (std::size_t i = 0; i < pixels; ++i) {
            n += is(&frame.rgba[i * 4]) ? 1u : 0u;
        }
        return static_cast<double>(n) / static_cast<double>(pixels);
    };

    if (kind == "cloud") {
        const double s = share(grey);
        std::printf("grey, as cloud: %.2f%% of the frame (at least 95%%)\n", 100.0 * s);
        if (s < 0.95) {
            fail("the frame is not cloud");
        }
    } else if (kind == "sky") {
        const double s = share([](const std::uint8_t* p) { return p[2] - p[0] >= 60; });
        std::printf("blue, as sky: %.2f%% of the frame (at least 95%%)\n", 100.0 * s);
        if (s < 0.95) {
            fail("the frame is not clear sky");
        }
    } else if (kind == "ground") {
        const double s = share([](const std::uint8_t* p) { return p[1] - p[2] >= 10; });
        std::printf("green, as the ground: %.2f%% of the frame (at least 30%%)\n",
                    100.0 * s);
        if (s < 0.30) {
            fail("the frame does not show the ground");
        }
    } else if (kind == "whiteout") {
        std::array<std::uint8_t, 3> median{};
        for (std::size_t c = 0; c < 3; ++c) {
            std::vector<std::uint8_t> values(pixels);
            for (std::size_t i = 0; i < pixels; ++i) {
                values[i] = frame.rgba[i * 4 + c];
            }
            std::nth_element(values.begin(),
                             values.begin() + static_cast<long>(pixels / 2),
                             values.end());
            median[c] = values[pixels / 2];
        }
        const double s = share([&](const std::uint8_t* p) {
            for (std::size_t c = 0; c < 3; ++c) {
                if (std::abs(p[c] - median[c]) > 4) {
                    return false;
                }
            }
            return true;
        });
        std::printf("within 4 of the median %d,%d,%d: %.2f%% of the frame (at least "
                    "99%%)\n",
                    median[0], median[1], median[2], 100.0 * s);
        if (s < 0.99 || !grey(median.data())) {
            fail("the frame is not a whiteout");
        }
    } else {
        const glideslope::gfx::Frame dry = load(argv[3]);
        if (dry.width != frame.width || dry.height != frame.height) {
            fail("the frames are not the same size");
        }
        std::size_t differ = 0;
        std::size_t lighter = 0;
        for (std::size_t i = 0; i < pixels; ++i) {
            int most = 0;
            int sum = 0;
            for (std::size_t c = 0; c < 3; ++c) {
                const int d = frame.rgba[i * 4 + c] - dry.rgba[i * 4 + c];
                most = std::max(most, std::abs(d));
                sum += d;
            }
            if (most > 10) {
                ++differ;
                lighter += sum > 0 ? 1u : 0u;
            }
        }
        const double s = static_cast<double>(differ) / static_cast<double>(pixels);
        const double light =
            differ == 0 ? 0.0
                        : static_cast<double>(lighter) / static_cast<double>(differ);
        std::printf(
            "differing from the dry frame: %.2f%% of it (0.3%% to 30%%), %.0f%% of "
            "those lighter (over half)\n",
            100.0 * s, 100.0 * light);
        if (s < 0.003 || s > 0.3 || light <= 0.5) {
            fail("the frame does not show rain falling");
        }
    }
    return 0;
}
