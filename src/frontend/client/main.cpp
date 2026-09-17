// glideslope - the simulator.
//
// Today it opens a window, or renders headless, and draws one of the scenes
// built into it; the world, the aircraft and the controls arrive through
// Phase 2.
//
//   glideslope [--headless] [--gpu-driver NAME] [--shot FILE] [--shot-at FRAME]
//              [--size WxH] [--scene NAME] [--at LAT,LON,HEIGHT | --at-ecef X,Y,Z]
//
// --shot writes the frame numbered --shot-at (default 1) as a BMP and exits,
// which is how CI and the tests see what the renderer really drew.

#include "gfx/renderer.hpp"
#include "scenes.hpp"
#include "sim/version.hpp"
#include "world/geodesy.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Options {
    bool headless = false;
    std::string driver;
    std::string shot;
    long shot_at = 1;
    int width = 1280;
    int height = 720;
    std::string scene = "sky";
    glideslope::world::Ecef at;
};

void usage(std::FILE* out) {
    std::fputs("usage: glideslope [--headless] [--gpu-driver vulkan|direct3d12|metal]\n"
               "                  [--shot FILE] [--shot-at FRAME] [--size WxH]\n"
               "                  [--scene sky|origin|depth]\n"
               "                  [--at LAT,LON,HEIGHT | --at-ecef X,Y,Z]\n"
               "       glideslope --version | --help\n"
               "\n"
               "  --at puts the scene at a latitude and longitude in degrees and a\n"
               "  height in metres above the WGS84 ellipsoid; --at-ecef at an\n"
               "  Earth-centred, Earth-fixed position in metres. The default is the\n"
               "  Earth's centre.\n",
               out);
}

std::optional<long> parse_integer(std::string_view text) {
    long value = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::array<int, 2>> parse_size(std::string_view text) {
    const auto x = text.find('x');
    if (x == std::string_view::npos) {
        return std::nullopt;
    }
    const auto w = parse_integer(text.substr(0, x));
    const auto h = parse_integer(text.substr(x + 1));
    if (!w || !h || *w <= 0 || *h <= 0 || *w > 16384 || *h > 16384) {
        return std::nullopt;
    }
    return std::array<int, 2>{static_cast<int>(*w), static_cast<int>(*h)};
}

// Three comma-separated numbers.
std::optional<std::array<double, 3>> parse_triple(std::string_view text) {
    std::array<double, 3> values{};
    const std::string copy(text);
    const char* at = copy.c_str();
    for (std::size_t i = 0; i < 3; ++i) {
        char* end = nullptr;
        values[i] = std::strtod(at, &end);
        if (end == at || *end != (i < 2 ? ',' : '\0')) {
            return std::nullopt;
        }
        at = end + 1;
    }
    return values;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    Options o;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view a = args[i];
        const bool has_value = i + 1 < args.size();
        bool ok = true;
        if (a == "--version") {
            const std::string_view v = glideslope::sim::version();
            std::printf("glideslope %.*s\n", static_cast<int>(v.size()), v.data());
            return 0;
        } else if (a == "--help") {
            usage(stdout);
            return 0;
        } else if (a == "--headless") {
            o.headless = true;
        } else if (a == "--gpu-driver" && has_value) {
            o.driver = std::string(args[++i]);
        } else if (a == "--shot" && has_value) {
            o.shot = std::string(args[++i]);
        } else if (a == "--shot-at" && has_value) {
            const auto frame = parse_integer(args[++i]);
            ok = frame && *frame >= 1;
            o.shot_at = frame.value_or(0);
        } else if (a == "--size" && has_value) {
            const auto size = parse_size(args[++i]);
            ok = size.has_value();
            if (size) {
                o.width = (*size)[0];
                o.height = (*size)[1];
            }
        } else if (a == "--scene" && has_value) {
            o.scene = std::string(args[++i]);
        } else if (a == "--at" && has_value) {
            const auto g = parse_triple(args[++i]);
            ok = g && (*g)[0] >= -90.0 && (*g)[0] <= 90.0 && (*g)[1] >= -180.0 &&
                 (*g)[1] <= 180.0;
            if (ok) {
                o.at = glideslope::world::to_ecef({(*g)[0], (*g)[1], (*g)[2]});
            }
        } else if (a == "--at-ecef" && has_value) {
            const auto e = parse_triple(args[++i]);
            ok = e.has_value();
            if (ok) {
                o.at = {(*e)[0], (*e)[1], (*e)[2]};
            }
        } else {
            ok = false;
        }
        if (!ok) {
            usage(stderr);
            return 2;
        }
    }
    if (o.headless && o.shot.empty()) {
        std::fputs("glideslope: --headless needs --shot, or it has nothing to show\n",
                   stderr);
        return 2;
    }

    // Headless is no window. Elsewhere that is SDL's offscreen video driver, which
    // needs no display; SDL's Metal backend will only start on a video driver
    // that can make a Metal view, which on macOS is Cocoa's, so there headless
    // keeps the native driver and simply opens no window.
#ifndef __APPLE__
    if (o.headless) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    }
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "glideslope: SDL did not start: %s\n", SDL_GetError());
        return 1;
    }

    int status = 0;
    SDL_Window* window = nullptr;
    try {
        const glideslope::client::Scene scene =
            glideslope::client::make_scene(o.scene, o.at);
        if (!o.headless) {
            window =
                SDL_CreateWindow("glideslope", o.width, o.height, SDL_WINDOW_RESIZABLE);
            if (window == nullptr) {
                throw std::runtime_error(std::string("no window: ") + SDL_GetError());
            }
        }
        glideslope::gfx::Renderer renderer(o.driver, window, o.width, o.height);
        std::printf("glideslope: GPU driver %s\n", renderer.driver().c_str());
        std::vector<glideslope::gfx::Draw> draws = scene.draws;
        for (glideslope::gfx::Draw& draw : draws) {
            draw.mesh = renderer.add_mesh(scene.meshes.at(draw.mesh));
        }

        bool running = true;
        for (long frame = 1; running; ++frame) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }
            renderer.render(scene.camera, draws);
            if (!o.shot.empty() && frame == o.shot_at) {
                glideslope::gfx::save_bmp(renderer.capture(), o.shot);
                std::printf("glideslope: wrote frame %ld to %s\n", frame,
                            o.shot.c_str());
                if (window != nullptr) {
                    std::printf("glideslope: presented %ld of %ld frames to the "
                                "window\n",
                                renderer.presented(), frame);
                }
                running = false;
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glideslope: %s\n", e.what());
        status = 1;
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return status;
}
