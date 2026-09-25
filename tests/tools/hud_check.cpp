// glideslope_hud_check - reads the HUD out of a frame and holds it to the
// flight's state.
//
//   glideslope_hud_check FRAME.bmp TRACE.txt TICK [dem] [imagery] [weather]
//
// FRAME is what `glideslope --shot` wrote at TICK; TRACE is what `--trace`
// printed on the way there. The trace must hold every tick from 1 to TICK, in
// order. The HUD is read back glyph by glyph (gfx::read_text), and each number
// on it must be the state at TICK, rounded as the HUD rounds: within half its
// last digit - the Mach number and flight level among them where they apply,
// and not shown where they do not. The credits along the bottom must be those named, in order - the
// Copernicus DEM's notice, the imagery's credit, Open-Meteo's credit - drawn as
// gfx::credit_lines draws them, and there must be nothing below them. Exits 0 if so, 1
// with the reason if not, 2 on bad arguments.

#include "gfx/hud.hpp"
#include "hud_judge.hpp"
#include "gfx/renderer.hpp"
#include "gfx/terrain_tiles.hpp"
#include "world/dem.hpp"
#include "world/weather.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[noreturn]] void fail(const std::string& why) {
    std::fprintf(stderr, "glideslope_hud_check: %s\n", why.c_str());
    std::exit(1);
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

// The trace line for every tick, by tick.
std::map<long long, std::map<std::string, double>> read_trace(const char* path) {
    std::ifstream in(path);
    if (!in) {
        fail(std::string("cannot read ") + path);
    }
    std::map<long long, std::map<std::string, double>> ticks;
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("trace ", 0) != 0) {
            continue;
        }
        std::istringstream words(line.substr(6));
        std::map<std::string, double> values;
        std::string key;
        double value = 0.0;
        while (words >> key >> value) {
            values[key] = value;
        }
        ticks[static_cast<long long>(values["tick"])] = values;
    }
    return ticks;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> credits;
    for (int i = 4; i < argc; ++i) {
        const std::string name = argv[i];
        if (name == "dem") {
            credits.emplace_back(glideslope::world::copernicus_dem_notice);
        } else if (name == "imagery") {
            credits.emplace_back(glideslope::gfx::open_imagery().credit);
        } else if (name == "weather") {
            credits.emplace_back(glideslope::world::open_meteo_credit);
        } else {
            argc = 0;
        }
    }
    if (argc < 4) {
        std::fputs(
            "usage: glideslope_hud_check FRAME.bmp TRACE.txt TICK [dem] [imagery] "
            "[weather]\n",
            stderr);
        return 2;
    }
    const long long tick = std::atoll(argv[3]);
    const auto trace = read_trace(argv[2]);
    if (trace.size() != static_cast<std::size_t>(tick) || trace.begin()->first != 1 ||
        trace.rbegin()->first != tick) {
        fail("the trace holds " + std::to_string(trace.size()) + " ticks, not 1 to " +
             std::to_string(tick));
    }
    const auto& state = trace.at(tick);

    const glideslope::gfx::Frame frame = load(argv[1]);
    try {
        (void)glideslope::test::judge_hud(frame, state, tick, credits, stdout);
    } catch (const glideslope::test::HudWrong& e) {
        fail(e.what());
    }
    std::printf("the HUD at tick %lld matches the state\n", tick);
    return 0;
}
