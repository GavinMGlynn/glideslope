// glideslope_hud_check - reads the HUD out of a frame and holds it to the
// flight's state.
//
//   glideslope_hud_check FRAME.bmp TRACE.txt TICK
//
// FRAME is what `glideslope --shot` wrote at TICK; TRACE is what `--trace`
// printed on the way there. The trace must hold every tick from 1 to TICK, in
// order. The HUD is read back glyph by glyph (gfx::read_text), and each number
// on it must be the state at TICK, rounded as the HUD rounds: within half its
// last digit. Exits 0 if so, 1 with the reason if not, 2 on bad arguments.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"

#include <SDL3/SDL.h>

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

// The words of a HUD line.
std::vector<std::string> words_of(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> out;
    for (std::string w; in >> w;) {
        out.push_back(w);
    }
    return out;
}

double number(const std::string& text, const std::string& line) {
    char* end = nullptr;
    const double v = std::strtod(text.c_str(), &end);
    if (text.empty() || *end != '\0') {
        fail("the HUD line \"" + line + "\" holds \"" + text +
             "\" where a number should be");
    }
    return v;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fputs("usage: glideslope_hud_check FRAME.bmp TRACE.txt TICK\n", stderr);
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
    const auto layout = glideslope::gfx::hud_layout(frame.width, frame.height);
    const auto lines = glideslope::gfx::read_text(frame, layout, 6, 16);
    for (const auto& l : lines) {
        std::printf("read: %s\n", l.c_str());
    }

    struct Field {
        std::string label;
        std::string unit; // empty for none
        std::string key;
        double half_step;
        bool angle;
    };
    const std::vector<Field> fields{
        {"SPD", "KT", "kcas", 0.5, false},   {"ALT", "FT", "alt_ft", 0.5, false},
        {"HDG", "", "heading", 0.5, true},   {"VS", "FPM", "vs_fpm", 0.5, false},
        {"PITCH", "", "pitch", 0.05, false}, {"BANK", "", "roll", 0.05, false}};
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const Field& f = fields[i];
        const auto words = words_of(lines[i]);
        const std::size_t expected_words = f.unit.empty() ? 2 : 3;
        if (words.size() != expected_words || words[0] != f.label ||
            (!f.unit.empty() && words[2] != f.unit)) {
            fail("line " + std::to_string(i + 1) + " reads \"" + lines[i] + "\", not " +
                 f.label + " and a number" + (f.unit.empty() ? "" : " in " + f.unit));
        }
        const double shown = number(words[1], lines[i]);
        double actual = state.at(f.key);
        double difference = std::abs(shown - actual);
        if (f.angle) {
            difference = std::abs(std::remainder(shown - actual, 360.0));
        }
        std::printf("%-6s shown %10.2f, state %10.4f\n", f.label.c_str(), shown,
                    actual);
        if (difference > f.half_step + 1e-6) {
            fail(f.label + " shows " + words[1] + " at tick " + std::to_string(tick) +
                 " when the state is " + std::to_string(actual));
        }
    }
    std::printf("the HUD at tick %lld matches the state\n", tick);
    return 0;
}
