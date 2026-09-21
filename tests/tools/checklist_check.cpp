// glideslope_checklist_check - reads the checklist off a frame and holds it
// to what the flight said it drew.
//
//   glideslope_checklist_check FRAME.bmp EXPECTED.txt
//
// FRAME is what `glideslope --shot` wrote; EXPECTED holds the lines the same
// run printed as "checklist: ...", one a line, with the prefix taken off.
// The block is read back glyph by glyph (gfx::read_text) from where
// gfx::checklist_layout puts it, and every line must be what was expected, in
// order, with nothing below them.
//
// **The marks are what this is for.** The first line is the phase and how
// much of it is done; each line after begins "X " for a ticked item or "- "
// for one still to do, so a frame that shows an item ticked when the flight
// has not ticked it, or the other way about, fails here.
//
// Exits 0 if the frame matches, 1 with the reason if not, 2 on bad arguments.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"

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
    const auto layout =
        glideslope::gfx::checklist_layout(frame.width, frame.height, expected.size());
    // One line more than expected, which must be empty: a checklist with
    // something under it is a checklist that has drawn more than it said.
    const auto shown = glideslope::gfx::read_text(
        frame, layout, expected.size() + 1,
        glideslope::gfx::checklist_columns(frame.width));

    std::size_t ticked = 0;
    for (std::size_t i = 0; i < shown.size(); ++i) {
        const std::string want = i < expected.size() ? expected[i] : "";
        std::printf("checklist line %zu: \"%s\"\n", i + 1, shown[i].c_str());
        if (shown[i] != want) {
            fail("line " + std::to_string(i + 1) + " reads \"" + shown[i] +
                 "\", not \"" + want + "\"");
        }
        if (i > 0 && !want.empty()) {
            if (want.rfind("X ", 0) != 0 && want.rfind("- ", 0) != 0) {
                fail("line " + std::to_string(i + 1) + " reads \"" + want +
                     "\", which begins with neither a tick nor a dash");
            }
            ticked += want.rfind("X ", 0) == 0 ? std::size_t{1} : std::size_t{0};
        }
    }
    // The count on the first line must be the marks below it, or the screen
    // is telling the pilot two different things at once.
    const std::string& head = expected[0];
    const std::size_t slash = head.rfind('/');
    const std::size_t space = head.rfind(' ', slash);
    if (slash == std::string::npos || space == std::string::npos) {
        fail("the first line \"" + head + "\" does not say how much is done");
    }
    const std::string said = head.substr(space + 1, slash - space - 1);
    if (said != std::to_string(ticked)) {
        fail("the checklist says " + said + " done but " + std::to_string(ticked) +
             " items are ticked");
    }
    std::printf("the checklist on the frame is the %s, %zu of its items ticked\n",
                head.c_str(), ticked);
    return 0;
}
