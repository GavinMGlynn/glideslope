// glideslope - the simulator.
//
// Today it opens a window, or renders headless, and clears the sky; the world,
// the aircraft and the controls arrive through Phase 2.
//
//   glideslope [--headless] [--gpu-driver NAME] [--shot FILE] [--shot-at FRAME]
//              [--size WxH]
//
// --shot writes the frame numbered --shot-at (default 1) as a BMP and exits,
// which is how CI and the tests see what the renderer really drew.

#include "gfx/renderer.hpp"
#include "sim/version.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <exception>
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
};

void usage(std::FILE* out) {
    std::fputs("usage: glideslope [--headless] [--gpu-driver vulkan|direct3d12|metal]\n"
               "                  [--shot FILE] [--shot-at FRAME] [--size WxH]\n"
               "       glideslope --version | --help\n",
               out);
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    Options o;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view a = args[i];
        const bool has_value = i + 1 < args.size();
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
            o.shot_at = std::stol(std::string(args[++i]));
        } else if (a == "--size" && has_value &&
                   std::sscanf(std::string(args[++i]).c_str(), "%dx%d", &o.width,
                               &o.height) == 2 &&
                   o.width > 0 && o.height > 0) {
        } else {
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
        if (!o.headless) {
            window =
                SDL_CreateWindow("glideslope", o.width, o.height, SDL_WINDOW_RESIZABLE);
            if (window == nullptr) {
                throw std::runtime_error(std::string("no window: ") + SDL_GetError());
            }
        }
        glideslope::gfx::Renderer renderer(o.driver, window, o.width, o.height);
        std::printf("glideslope: GPU driver %s\n", renderer.driver().c_str());

        bool running = true;
        for (long frame = 1; running; ++frame) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }
            renderer.render();
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
