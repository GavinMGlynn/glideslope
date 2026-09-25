// glideslope_checklist_check - reads the checklist off a frame and holds it
// to what the flight said it drew.
//
//   glideslope_checklist_check FRAME.bmp EXPECTED.txt
//
// FRAME is what `glideslope --shot` wrote; EXPECTED holds the lines the same
// run printed as "checklist: ...", one a line, with the prefix taken off.
// The block is read back glyph by glyph (gfx::read_text) from where
// gfx::checklist_layout puts it, and every line must be what was expected, in
// order, with nothing below them - judged by checklist_judge.hpp, which
// glideslope_checklist_horizon_check judges its frames by too.
//
// **The marks are what this is for.** The first line is the phase and how
// much of it is done; each line after begins "X " for a ticked item or "- "
// for one still to do, so a frame that shows an item ticked when the flight
// has not ticked it, or the other way about, fails here.
//
// Exits 0 if the frame matches, 1 with the reason if not, 2 on bad arguments.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "checklist_judge.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

[[noreturn]] void fail(const std::string& why) {
    std::fprintf(stderr, "glideslope_checklist_check: %s\n", why.c_str());
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

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fputs("usage: glideslope_checklist_check FRAME.bmp EXPECTED.txt\n", stderr);
        return 2;
    }
    std::vector<std::string> expected;
    {
        std::ifstream in(argv[2]);
        if (!in) {
            fail(std::string("cannot read ") + argv[2]);
        }
        for (std::string line; std::getline(in, line);) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }
            if (!line.empty()) {
                expected.push_back(line);
            }
        }
    }
    if (expected.empty()) {
        fail("the flight said it drew no checklist at all");
    }

    const glideslope::gfx::Frame frame = load(argv[1]);
    std::size_t ticked = 0;
    try {
        ticked = glideslope::test::judge_checklist(frame, expected, stdout);
    } catch (const glideslope::test::ChecklistWrong& e) {
        fail(e.what());
    }
    const std::string& head = expected[0];
    std::printf("the checklist on the frame is the %s, %zu of its items ticked\n",
                head.c_str(), ticked);
    return 0;
}
