// glideslope_lesson_check - reads the lessons screen off a frame and holds it
// to what the client said it drew.
//
//   glideslope_lesson_check FRAME.bmp EXPECTED.txt
//
// FRAME is what `glideslope --shot-lesson` wrote for one state of the lessons
// screen, and EXPECTED the lines it wrote beside it (gfx::lesson_lines), one
// a line. The screen is read back glyph by glyph (gfx::read_text) from where
// the checklist goes (gfx::checklist_layout), one line more than expected,
// and every line must be what was expected, in order, with nothing below
// them. An EXPECTED with no lines is the screen closed: then the first
// three lines there must read empty.
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
    std::fprintf(stderr, "glideslope_lesson_check: %s\n", why.c_str());
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
        const auto* row = static_cast<const std::uint8_t*>(rgba->pixels) + y * rgba->pitch;
        std::copy(row, row + rgba->w * 4, frame.rgba.begin() + static_cast<long>(y) * rgba->w * 4);
    }
    SDL_DestroySurface(rgba);
    return frame;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fputs("usage: glideslope_lesson_check FRAME.bmp EXPECTED.txt\n", stderr);
        return 2;
    }
    std::vector<std::string> expected;
    {
        std::ifstream in(argv[2], std::ios::binary);
        if (!in) {
            fail(std::string("cannot read ") + argv[2]);
        }
        for (std::string line; std::getline(in, line);) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }
            expected.push_back(line);
        }
    }
    const glideslope::gfx::Frame frame = load(argv[1]);
    const std::size_t rows = expected.empty() ? 3 : expected.size() + 1;
    const auto layout = glideslope::gfx::checklist_layout(frame.width, frame.height, rows);
    const auto shown = glideslope::gfx::read_text(frame, layout, rows,
                                                  glideslope::gfx::checklist_columns(frame.width));
    for (std::size_t i = 0; i < shown.size(); ++i) {
        const std::string want = i < expected.size() ? expected[i] : "";
        std::printf("lesson line %zu: \"%s\"\n", i + 1, shown[i].c_str());
        if (shown[i] != want) {
            fail("line " + std::to_string(i + 1) + " reads \"" + shown[i] + "\", not \"" + want +
                 "\"");
        }
    }
    std::printf("the lessons screen on the frame is the one drawn, %zu lines\n",
                expected.size());
    return 0;
}
