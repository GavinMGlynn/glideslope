// glideslope_help_check - the controls' help on a frame is what the client
// said it drew, and the help's columns are laid out inside the frame.
//
//   glideslope_help_check FRAME.bmp SAID.txt
//       reads each column back off the frame (gfx::read_text) and holds every
//       line to what the client said it drew there. SAID.txt is the client's
//       "help column LEFT TOP LINES" lines, each followed by its LINES lines.
//   glideslope_help_check layout
//       lays out help of 43 lines (as many as the committed bindings give) on
//       every frame size from 160x120 to 1920x1080 in steps, with and without
//       credits, and holds each to the rules gfx/hud.hpp states: inside the
//       frame and above the bottom, in order, every line drawn or the last
//       one drawn saying what is cut - or nothing, only where even that line
//       cannot fit.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[noreturn]] void fail(const std::string& why) {
    std::fprintf(stderr, "glideslope_help_check: %s\n", why.c_str());
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

int check_frame(const char* frame_path, const char* said_path) {
    std::ifstream in(said_path);
    if (!in) {
        fail(std::string("cannot read ") + said_path);
    }
    const glideslope::gfx::Frame frame = load(frame_path);
    std::size_t columns = 0;
    std::size_t lines = 0;
    for (std::string head; std::getline(in, head);) {
        if (head.rfind("help column ", 0) != 0) {
            continue;
        }
        std::istringstream words(head);
        std::string help;
        std::string column;
        glideslope::gfx::TextLayout layout;
        layout.scale = 1;
        std::size_t count = 0;
        if (!(words >> help >> column >> layout.left >> layout.top >> count) ||
            help != "help" || column != "column") {
            fail("not a column's heading: " + head);
        }
        std::vector<std::string> expected;
        std::size_t widest = 0;
        for (std::size_t i = 0; i < count; ++i) {
            std::string line;
            if (!std::getline(in, line) || line.rfind("help: ", 0) != 0) {
                fail("a column said fewer lines than it counted");
            }
            line.erase(0, 6);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }
            widest = std::max(widest, line.size());
            expected.push_back(line);
        }
        const std::vector<std::string> read =
            glideslope::gfx::read_text(frame, layout, count, widest);
        for (std::size_t i = 0; i < count; ++i) {
            if (read[i] != expected[i]) {
                fail("column " + std::to_string(columns) + " line " + std::to_string(i) +
                     " reads '" + read[i] + "', not '" + expected[i] + "'");
            }
        }
        ++columns;
        lines += count;
    }
    if (lines == 0) {
        fail("the client said it drew no help");
    }
    std::printf("read back %zu lines in %zu columns, every one as the client drew it\n", lines,
                columns);
    return 0;
}

int check_layout() {
    // Lines as long as the committed bindings' longest and as many.
    std::vector<std::string> help;
    for (int i = 0; i < 43; ++i) {
        help.push_back(std::string(static_cast<std::size_t>(20 + (i * 7) % 60), 'X') +
                       std::to_string(i));
    }
    std::size_t sizes = 0;
    std::size_t whole = 0;
    std::size_t cut = 0;
    std::size_t nothing = 0;
    for (int width = 160; width <= 1920; width += 80) {
        for (int height = 120; height <= 1080; height += 60) {
            for (const bool credits : {false, true}) {
                const std::vector<std::string> notices =
                    credits ? std::vector<std::string>{"A NOTICE A DATA SOURCE ASKS FOR, LONG "
                                                       "ENOUGH TO WRAP ON A NARROW FRAME"}
                            : std::vector<std::string>{};
                const int bottom = glideslope::gfx::help_bottom(notices, width, height);
                const auto columns = glideslope::gfx::help_columns(help, width, bottom);
                const std::string where = std::to_string(width) + "x" + std::to_string(height) +
                                          (credits ? " with credits" : "");
                std::size_t drawn = 0;
                int right_before = 0;
                for (const auto& c : columns) {
                    const int right =
                        c.layout.left + static_cast<int>(c.columns) * c.layout.cell_width();
                    const int foot = c.layout.top +
                                     static_cast<int>(c.lines.size()) * c.layout.cell_height();
                    if (c.layout.left < right_before || right > width || foot > bottom - c.layout.cell_height()) {
                        fail(where + ": a column is outside the frame, less than a line above the bottom or "
                                     "over the one before");
                    }
                    right_before = right;
                    drawn += c.lines.size();
                }
                if (columns.empty()) {
                    // Only where even the line saying so cannot fit.
                    if (20 + 6 * static_cast<int>(std::string(glideslope::gfx::help_cut_line)
                                                      .size()) <= width - 6) {
                        fail(where + ": nothing drawn, though the cut line would fit");
                    }
                    ++sizes;
                    ++nothing;
                    continue;
                }
                const bool said_cut = columns.back().lines.back() == glideslope::gfx::help_cut_line;
                std::size_t in_order = 0;
                for (const auto& c : columns) {
                    for (const std::string& line : c.lines) {
                        if (in_order < help.size() && line == help[in_order]) {
                            ++in_order;
                        }
                    }
                }
                if (said_cut ? in_order != drawn - 1 : (drawn != help.size() || in_order != drawn)) {
                    fail(where + ": " + std::to_string(drawn) + " lines drawn, " +
                         std::to_string(in_order) + " of them the help's in order, " +
                         (said_cut ? "and it says it is cut" : "and it does not say it is cut"));
                }
                ++sizes;
                (said_cut ? cut : whole) += 1;
            }
        }
    }
    const std::size_t expected = 23 * 17 * 2;
    std::printf("%zu frame sizes of %zu laid out: %zu whole, %zu cut and saying so, %zu too "
                "narrow for anything\n",
                sizes, expected, whole, cut, nothing);
    if (sizes != expected || whole == 0 || cut == 0 || whole + cut + nothing != sizes) {
        fail("not every size was laid out, or none was whole, or none was cut");
    }
    const auto at_default = glideslope::gfx::help_columns(
        help, 1280, glideslope::gfx::help_bottom({"A NOTICE"}, 1280, 720));
    if (at_default.back().lines.back() == glideslope::gfx::help_cut_line) {
        fail("at the client's own 1280x720 the help is cut");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "layout") {
        return check_layout();
    }
    if (argc != 3) {
        std::fputs("usage: glideslope_help_check FRAME.bmp SAID.txt | layout\n", stderr);
        return 2;
    }
    return check_frame(argv[1], argv[2]);
}
