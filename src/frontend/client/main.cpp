// glideslope - the simulator.
//
// It opens a window, or renders headless, on one of its screens: the flight -
// the Cessna over the DEM, seen from its cockpit, with the HUD - or a test
// scene. Terrain is not drawn yet.
//
//   glideslope [--headless] [--gpu-driver NAME] [--size WxH]
//              [--screen flight|sky|origin|depth]
//              [--at LAT,LON,HEIGHT | --at-ecef X,Y,Z] [--weather STATION]
//              [--shot FILE] [--shot-at TICK] [--trace]
//
// Test flags. --shot writes the frame drawn at simulation tick --shot-at
// (default 2) as a BMP and exits; while shooting, every frame advances exactly
// two ticks - a sixtieth of a second - whatever the clock says, so the same
// command draws the same frame on every machine. --trace prints the flight's
// state after every tick.

#include "flight.hpp"
#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "platform/input.hpp"
#include "platform/paths.hpp"
#include "scenes.hpp"
#include "sim/fixed_step.hpp"
#include "sim/version.hpp"
#include "world/geodesy.hpp"
#include "world/weather.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
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
    std::int64_t shot_at = 2;
    bool trace = false;
    int width = 1280;
    int height = 720;
    std::string screen = "flight";
    std::optional<glideslope::world::Geodetic> at;
    std::optional<glideslope::world::Ecef> at_ecef;
    std::string weather_station;
};

void usage(std::FILE* out) {
    std::fputs(
        "usage: glideslope [--headless] [--gpu-driver vulkan|direct3d12|metal]\n"
        "                  [--size WxH] [--screen flight|sky|origin|depth]\n"
        "                  [--at LAT,LON,HEIGHT | --at-ecef X,Y,Z]\n"
        "                  [--weather STATION]\n"
        "                  [--shot FILE] [--shot-at TICK] [--trace]\n"
        "       glideslope --version | --help\n"
        "\n"
        "  --screen      what to show: the flight (the default), or a test scene\n"
        "  --at          where: a latitude and longitude in degrees and a height\n"
        "                in metres above the WGS84 ellipsoid - the flight starts\n"
        "                there, a scene is built there; --at-ecef in Earth-centred\n"
        "                metres, for scenes\n"
        "  --weather     fly in the weather reported now at an airfield, by its\n"
        "                ICAO code - its METAR, and Open-Meteo's winds aloft\n"
        "  --shot        write the frame at tick --shot-at (default 2) and exit;\n"
        "                each frame is then two ticks, whatever the clock says\n"
        "  --trace       print the flight's state after every tick\n",
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

// The keyboard, beside any flight controller: arrows for the elevator and
// ailerons, Z and X for the rudder, Page Up and Page Down for the throttle, B
// for the brakes. A key moves its control while held, and lets it go when
// released, so that a stick left alone is not overridden every frame.
class Keyboard {
public:
    void apply(glideslope::sim::Controls& c, double seconds) {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        const auto axis = [&](double& control, SDL_Scancode minus, SDL_Scancode plus,
                              bool& was) {
            const double v = (keys[plus] ? 0.5 : 0.0) - (keys[minus] ? 0.5 : 0.0);
            const bool held = keys[plus] || keys[minus];
            if (held || was) {
                control = v;
            }
            was = held;
        };
        axis(c.elevator, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, elevator_);
        axis(c.aileron, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, aileron_);
        axis(c.rudder, SDL_SCANCODE_Z, SDL_SCANCODE_X, rudder_);
        const double throttle = (keys[SDL_SCANCODE_PAGEUP] ? 1.0 : 0.0) -
                                (keys[SDL_SCANCODE_PAGEDOWN] ? 1.0 : 0.0);
        c.throttle = std::clamp(c.throttle + 0.5 * seconds * throttle, 0.0, 1.0);
        if (keys[SDL_SCANCODE_B] || brakes_) {
            c.left_brake = c.right_brake = keys[SDL_SCANCODE_B] ? 1.0 : 0.0;
        }
        brakes_ = keys[SDL_SCANCODE_B];
    }

private:
    bool elevator_ = false;
    bool aileron_ = false;
    bool rudder_ = false;
    bool brakes_ = false;
};

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
        } else if (a == "--trace") {
            o.trace = true;
        } else if (a == "--gpu-driver" && has_value) {
            o.driver = std::string(args[++i]);
        } else if (a == "--shot" && has_value) {
            o.shot = std::string(args[++i]);
        } else if (a == "--shot-at" && has_value) {
            const auto tick = parse_integer(args[++i]);
            ok = tick && *tick >= 0;
            o.shot_at = tick.value_or(0);
        } else if (a == "--size" && has_value) {
            const auto size = parse_size(args[++i]);
            ok = size.has_value();
            if (size) {
                o.width = (*size)[0];
                o.height = (*size)[1];
            }
        } else if (a == "--screen" && has_value) {
            o.screen = std::string(args[++i]);
            ok = o.screen == "flight" || o.screen == "sky" || o.screen == "origin" ||
                 o.screen == "depth";
        } else if (a == "--at" && has_value) {
            const auto g = parse_triple(args[++i]);
            ok = g && (*g)[0] >= -90.0 && (*g)[0] <= 90.0 && (*g)[1] >= -180.0 &&
                 (*g)[1] <= 180.0;
            if (ok) {
                o.at = glideslope::world::Geodetic{(*g)[0], (*g)[1], (*g)[2]};
            }
        } else if (a == "--weather" && has_value) {
            o.weather_station = std::string(args[++i]);
            ok = o.weather_station.size() == 4 &&
                 std::all_of(
                     o.weather_station.begin(), o.weather_station.end(), [](char c) {
                         return std::isalnum(static_cast<unsigned char>(c)) != 0;
                     });
        } else if (a == "--at-ecef" && has_value) {
            const auto e = parse_triple(args[++i]);
            ok = e.has_value();
            if (ok) {
                o.at_ecef = glideslope::world::Ecef{(*e)[0], (*e)[1], (*e)[2]};
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
    if (o.screen != "flight" && !o.weather_station.empty()) {
        std::fputs("glideslope: only the flight has --weather\n", stderr);
        return 2;
    }
    if (o.screen == "flight" && o.at_ecef) {
        std::fputs(
            "glideslope: the flight starts --at a latitude, longitude and height\n",
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
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        std::fprintf(stderr, "glideslope: SDL did not start: %s\n", SDL_GetError());
        return 1;
    }

    int status = 0;
    SDL_Window* window = nullptr;
    try {
        std::unique_ptr<glideslope::client::Flight> flight;
        glideslope::client::Scene scene;
        if (o.screen == "flight") {
            glideslope::client::FlightStart start;
            if (o.at) {
                start.latitude_deg = o.at->latitude_deg;
                start.longitude_deg = o.at->longitude_deg;
                start.height_m = o.at->height_m;
            }
            start.weather_station = o.weather_station;
            flight = std::make_unique<glideslope::client::Flight>(
                glideslope::platform::data_directory(),
                glideslope::platform::cache_directory(), start);
            if (!start.weather_station.empty()) {
                std::printf("glideslope: flying in the weather at %s: METAR from "
                            "aviationweather.gov; %s (https://open-meteo.com/), CC BY "
                            "4.0\n",
                            start.weather_station.c_str(),
                            glideslope::world::open_meteo_credit);
            }
        } else {
            const glideslope::world::Ecef at =
                o.at_ecef ? *o.at_ecef
                          : (o.at ? glideslope::world::to_ecef(*o.at)
                                  : glideslope::world::Ecef{});
            scene = glideslope::client::make_scene(o.screen, at);
        }

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

        const bool shooting = !o.shot.empty();
        glideslope::sim::Controls controls;
        controls.throttle = 0.65;
        const std::filesystem::path bindings_path =
            glideslope::platform::data_directory() / "input" / "bindings.txt";
        std::ifstream bindings_file(bindings_path, std::ios::binary);
        if (!bindings_file) {
            throw std::runtime_error("cannot read " + bindings_path.string());
        }
        glideslope::platform::ControlMapper mapper(glideslope::platform::parse_bindings(
            std::string(std::istreambuf_iterator<char>(bindings_file), {})));
        glideslope::platform::Joysticks joysticks;
        Keyboard keys;
        glideslope::sim::FixedStep clock;
        auto last = std::chrono::steady_clock::now();
        std::int64_t ticks = 0;
        long frames = 0;
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }
            std::int64_t due = 0;
            if (shooting) {
                due = std::min<std::int64_t>(2, o.shot_at - ticks);
            } else {
                const auto now = std::chrono::steady_clock::now();
                due = std::min<std::int64_t>(clock.advance(now - last), 24);
                last = now;
                keys.apply(controls,
                           static_cast<double>(due) /
                               static_cast<double>(glideslope::sim::steps_per_second));
            }
            mapper.apply(joysticks.read(), controls);
            for (std::int64_t i = 0; i < due; ++i) {
                if (flight) {
                    flight->step(controls);
                    if (o.trace) {
                        std::printf("%s\n", flight->trace().c_str());
                    }
                }
                ++ticks;
            }

            if (flight) {
                const glideslope::gfx::Mesh hud =
                    glideslope::gfx::hud_mesh(flight->hud(), o.width, o.height);
                renderer.render(flight->camera(), {}, &hud);
            } else {
                renderer.render(scene.camera, draws);
            }
            ++frames;

            if (shooting && ticks >= o.shot_at) {
                glideslope::gfx::save_bmp(renderer.capture(), o.shot);
                std::printf("glideslope: wrote tick %lld, frame %ld, to %s\n",
                            static_cast<long long>(ticks), frames, o.shot.c_str());
                if (window != nullptr) {
                    std::printf("glideslope: presented %ld of %ld frames to the "
                                "window\n",
                                renderer.presented(), frames);
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
