// checklist_horizon_check - the horizon, put across every checklist row on
// purpose, leaves the checklist read whole, and shows through its panel.
//
//   glideslope_checklist_horizon_check
//
// The checklist is drawn down the top right, from half the width, and the
// horizon reaches two-thirds across: pitched and banked the right way, it
// crosses any of the checklist's rows, and a stroke of it through a row reads
// as "?" where glideslope_checklist_check compares the lines whole. So the
// checklist is drawn over a panel that darkens what is behind it by half
// (gfx::checklist_panel), the horizon included. This holds that, in two parts.
//
// **The panel, at every size from 1x1 to 4096x4096, for 1 to 32 lines** -
// past the nine items and heading of the longest phase any aircraft has: it
// covers every pixel gfx::read_text looks at for the checklist's lines and
// the empty one below them that the check reads; and on a frame at least
// gfx::hud_narrowest_clear_width wide it starts right of the HUD's text, so
// never dims it. Narrower, where the HUD's own text can reach the middle of
// the frame, the sizes where it would are counted.
//
// **Frames**: a checklist of nine items - some ticked, one too long for its
// line - on landscape, square and portrait frames, the narrowest the HUD's
// text is clear at and one narrower. For each of its rows, the empty one
// below included, it pitches and banks the aircraft so that the horizon
// crosses the middle of the row's height at the checklist's first column, at
// the end of the horizon or the checklist, whichever comes first, and half way
// between, banked 30, 60 and 75 degrees either way, and level. It paints the
// HUD's mesh (gfx::hud_mesh) over a sky and requires:
//
//   - that the horizon's centre line does cross the row, inside the
//     checklist's columns: the frame is the one it says it is;
//   - the horizon whole: every pixel under its centre line that is on the
//     frame and above the credits' strip in the HUD's colour - or, under the
//     checklist's panel, in that colour dimmed by half, or the HUD's colour
//     where a letter is drawn over it - but for the last pixel at each end,
//     which the line's square end may leave uncovered; and
//   - the checklist judged as glideslope_checklist_check judges a shot
//     (checklist_judge.hpp), against the lines gfx::checklist_lines says are
//     drawn, which is what the flight prints.
//
// Banked 90 degrees the horizon stands upright at the middle of the frame,
// left of or on the checklist's first column, crossing no row inside it; so
// the banks stop at 75.
//
// Every count is held to the number there should be. Exits 0 if all is well,
// 1 with the first thing wrong and how many frames were misread.

#include "canvas.hpp"
#include "checklist_judge.hpp"
#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "world/dem.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <future>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using glideslope::gfx::Frame;

struct Wrong : std::runtime_error {
    using std::runtime_error::runtime_error;
};
[[noreturn]] void fail(const std::string& why) {
    throw Wrong(why);
}

// Whether the pixel is `colour` times `dim`, to within 2 of 255.
bool coloured(const Frame& frame, int x, int y, float dim) {
    const auto at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) +
                     static_cast<std::size_t>(x)) *
                    4;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto want = std::lround(glideslope::gfx::hud_colour[i] * dim * 255.0f);
        if (std::abs(static_cast<long>(frame.rgba[at + i]) - want) > 2) {
            return false;
        }
    }
    return true;
}

constexpr int most_size = 4096;
constexpr std::size_t most_lines = 32;

// The panel at every size and line count, walked a range of widths at a time.
struct PanelCounts {
    std::size_t layouts = 0;
    std::size_t covered = 0;
    std::size_t clear_of_hud = 0;
    std::size_t wide = 0;
    std::size_t over_hud_where_narrow = 0;
};

PanelCounts check_panel_widths(int width_from, int width_to) {
    PanelCounts n;
    for (int w = width_from; w <= width_to; ++w) {
        const auto columns = static_cast<int>(glideslope::gfx::checklist_columns(w));
        for (int h = 1; h <= most_size; ++h) {
            const auto hud = glideslope::gfx::hud_layout(w, h);
            const double hud_right =
                hud.left + static_cast<double>(glideslope::gfx::hud_columns) * hud.cell_width();
            for (std::size_t lines = 1; lines <= most_lines; ++lines) {
                ++n.layouts;
                const auto layout = glideslope::gfx::checklist_layout(w, h, lines);
                const auto panel = glideslope::gfx::checklist_panel(w, h, lines);
                // read_text looks at the middle of each font pixel: at scale
                // one, columns 0 to 4 and rows 0 to 6 of each cell, for the
                // lines and the one below them.
                const double read_left = layout.left;
                const double read_right =
                    layout.left + (columns - 1) * layout.cell_width() + 5.0 * layout.scale;
                const double read_top = layout.top;
                const double read_bottom = layout.top +
                                           static_cast<double>(lines) * layout.cell_height() +
                                           7.0 * layout.scale;
                if (layout.scale != 1 || panel.left > read_left || panel.right < read_right ||
                    panel.top > read_top || panel.bottom < read_bottom) {
                    fail(std::to_string(w) + "x" + std::to_string(h) + ", " +
                         std::to_string(lines) + " lines: the panel [" +
                         std::to_string(panel.left) + ", " + std::to_string(panel.right) +
                         ") by [" + std::to_string(panel.top) + ", " +
                         std::to_string(panel.bottom) + ") does not cover what is read, [" +
                         std::to_string(read_left) + ", " + std::to_string(read_right) +
                         ") by [" + std::to_string(read_top) + ", " +
                         std::to_string(read_bottom) + ")");
                }
                ++n.covered;
                const bool clear = panel.left >= hud_right;
                if (w >= glideslope::gfx::hud_narrowest_clear_width) {
                    ++n.wide;
                    if (!clear) {
                        fail(std::to_string(w) + "x" + std::to_string(h) +
                             ": the checklist's panel starts at " + std::to_string(panel.left) +
                             ", over the HUD's text, which ends at " +
                             std::to_string(hud_right));
                    }
                    ++n.clear_of_hud;
                } else if (!clear) {
                    ++n.over_hud_where_narrow;
                }
            }
        }
    }
    return n;
}

void check_panels() {
    std::vector<std::future<PanelCounts>> walking;
    constexpr int parts = 4;
    for (int p = 0; p < parts; ++p) {
        walking.push_back(std::async(std::launch::async, check_panel_widths,
                                     p * most_size / parts + 1, (p + 1) * most_size / parts));
    }
    PanelCounts all;
    for (auto& part : walking) {
        const PanelCounts n = part.get();
        all.layouts += n.layouts;
        all.covered += n.covered;
        all.clear_of_hud += n.clear_of_hud;
        all.wide += n.wide;
        all.over_hud_where_narrow += n.over_hud_where_narrow;
    }
    const auto sizes = static_cast<std::size_t>(most_size) * static_cast<std::size_t>(most_size);
    const std::size_t wide = static_cast<std::size_t>(most_size) *
                             static_cast<std::size_t>(
                                 most_size - glideslope::gfx::hud_narrowest_clear_width + 1) *
                             most_lines;
    std::printf("the checklist's panel at %zu sizes, 1x1 to %dx%d, for 1 to %zu lines: %zu "
                "layouts, what is read covered in %zu; clear of the HUD's text in %zu of the %zu at "
                "least %d wide, and over it in %zu of the narrower\n",
                sizes, most_size, most_size, most_lines, all.layouts, all.covered,
                all.clear_of_hud, all.wide, glideslope::gfx::hud_narrowest_clear_width,
                all.over_hud_where_narrow);
    if (all.layouts != sizes * most_lines || all.covered != all.layouts || all.wide != wide ||
        all.clear_of_hud != wide) {
        fail("the layouts walked were " + std::to_string(all.layouts) + ", " +
             std::to_string(all.covered) + " covered, " + std::to_string(all.clear_of_hud) +
             " clear of the HUD of " + std::to_string(all.wide) + " wide, not " +
             std::to_string(sizes * most_lines) + " and " + std::to_string(wide));
    }
}

struct Counts {
    std::size_t frames = 0;
    std::size_t crossing = 0;
    std::size_t whole = 0;
    std::size_t read = 0;
    std::size_t expected_frames = 0;
    std::string first_misread;
};

} // namespace

int run();

int main() {
    try {
        return run();
    } catch (const Wrong& e) {
        std::fprintf(stderr, "glideslope_checklist_horizon_check: %s\n", e.what());
        return 1;
    }
}

int run() {
    check_panels();

    // Nine items, the most any phase of any aircraft's has: some ticked, one
    // too long for its line at every size here.
    glideslope::gfx::ChecklistOnScreen showing;
    showing.phase = "take-off";
    showing.items = {
        {true, "Cabin doors closed and locked, and the flight controls free and correct, "
               "checked against the handbook's list item by item before the run-up, and "
               "the fuel selector on both tanks and the mixture rich below three thousand "
               "feet"},
        {true, "Run-up at 1,700: magneto drop within 125, and 50 between them"},
        {false, "Wing flaps nought to ten degrees"},
        {true, "Mixture rich"},
        {false, "Elevator trim set for take-off"},
        {false, "Throttle full open"},
        {true, "Lift the nose wheel at 55 knots"},
        {false, "Climb at 70 to 80 knots"},
        {false, "Flaps up"}};
    const std::vector<std::string> credits{glideslope::world::copernicus_dem_notice,
                                           glideslope::world::open_meteo_credit};
    // Landscape, square and portrait; the narrowest the HUD's text is clear
    // at, and one narrower.
    const std::vector<std::pair<int, int>> sizes{{640, 480},   {1280, 720}, {1920, 1080},
                                                 {800, 800},   {600, 1000}, {1080, 1920},
                                                 {474, 800},   {360, 640}};
    const std::vector<double> banks{-75.0, -60.0, -30.0, 0.0, 30.0, 60.0, 75.0};
    // Where along the checklist's part of the horizon it crosses the row: at
    // the first column, the far end, and half way. Level, the three are one
    // frame, and only one is shot.
    const std::vector<double> along{0.0, 0.5, 1.0};
    constexpr double degrees = 180.0 / 3.14159265358979323846;

    const auto walk = [&](int width, int height) {
        Counts n;
        glideslope::test::Canvas canvas(width, height);
        glideslope::gfx::HudReadings r;
        r.airspeed_kts = 102.4;
        r.altitude_ft = 3012.2;
        r.heading_deg = 164.2;
        r.vertical_speed_fpm = -120.3;
        r.credits = credits;
        r.checklist = showing;
        // As frame_checklist.cmake takes them from what the flight prints:
        // trailing spaces - a line cut at a space - taken off.
        std::vector<std::string> expected = glideslope::gfx::checklist_lines(showing, width);
        for (std::string& line : expected) {
            while (!line.empty() && line.back() == ' ') {
                line.pop_back();
            }
        }
        if (expected.size() != 1 + showing.items.size() ||
            glideslope::gfx::checklist_lines(showing, width)[1].size() !=
                glideslope::gfx::checklist_columns(width)) {
            fail(std::to_string(width) + " wide: the checklist is " +
                 std::to_string(expected.size()) + " lines, its first item " +
                 std::to_string(expected[1].size()) + " long, not 10 lines and " +
                 std::to_string(glideslope::gfx::checklist_columns(width)) +
                 ", the first cut to fit");
        }
        const auto layout = glideslope::gfx::checklist_layout(width, height, expected.size());
        const auto panel = glideslope::gfx::checklist_panel(width, height, expected.size());
        const double columns_right =
            layout.left +
            static_cast<double>(glideslope::gfx::checklist_columns(width)) * layout.cell_width();
        const double strip_top =
            glideslope::gfx::credit_layout(width, height,
                                           glideslope::gfx::credit_lines(credits, width).size())
                .top;
        // Every line, and the empty one below that the check reads.
        const std::size_t rows = expected.size() + 1;
        n.expected_frames = rows * ((banks.size() - 1) * along.size() + 1);
        const double cx = width / 2.0;
        const double half = width / 6.0;
        for (std::size_t row = 0; row < rows; ++row) {
            const double row_top = layout.top + static_cast<double>(row) * layout.cell_height();
            const double y = row_top + 3.5 * layout.scale;
            for (const double bank : banks) {
                const double cosb = std::cos(bank / degrees);
                const double sinb = std::sin(bank / degrees);
                // The first column's glyphs' middle; and the horizon's end,
                // a pixel in, or the checklist's last column, whichever
                // comes first.
                const double first = layout.left + 2.5 * layout.scale;
                const double last = std::min(cx + half * cosb - 1.0, columns_right - 1.0);
                if (last < first) {
                    fail(std::to_string(width) + "x" + std::to_string(height) + ", bank " +
                         std::to_string(bank) + ": the horizon does not reach the checklist");
                }
                for (const double a : along) {
                    if (bank == 0.0 && a != 0.0) {
                        continue;
                    }
                    ++n.frames;
                    char name[256];
                    std::snprintf(name, sizeof name, "%dx%d, row %zu, bank %+.0f, %.1f along",
                                  width, height, row + 1, bank, a);
                    // The horizon through (x, y), banked.
                    const double x = first + (last - first) * a;
                    const double s = (x - cx) / (half * cosb);
                    const double centre_y = y + s * half * sinb;
                    r.roll_deg = bank;
                    r.pitch_deg = (centre_y - height / 2.0) * 100.0 / height;

                    const Frame& frame = canvas.paint(glideslope::gfx::hud_mesh(r, width, height));

                    const auto h = glideslope::gfx::hud_horizon(r, width, height);
                    const double length = std::hypot(h.x1 - h.x0, h.y1 - h.y0);
                    const auto steps = static_cast<int>(std::ceil(length * 2.0));
                    int on_frame = 0;
                    int lit = 0;
                    bool crosses = false;
                    for (int i = 0; i <= steps; ++i) {
                        const double t = static_cast<double>(i) / steps;
                        const double px = h.x0 + (h.x1 - h.x0) * t;
                        const double py = h.y0 + (h.y1 - h.y0) * t;
                        if (px >= layout.left && px < columns_right && py >= row_top &&
                            py < row_top + 7.0 * layout.scale) {
                            crosses = true;
                        }
                        if (t * length < 1.0 || (1.0 - t) * length < 1.0) {
                            continue;
                        }
                        if (px < 0.0 || py < 0.0 || px >= width || py >= strip_top) {
                            continue;
                        }
                        ++on_frame;
                        const int ix = static_cast<int>(px);
                        const int iy = static_cast<int>(py);
                        const bool under_panel = ix >= panel.left && ix < panel.right &&
                                                 iy >= panel.top && iy < panel.bottom;
                        const bool shows = coloured(frame, ix, iy, 1.0f) ||
                                           (under_panel && coloured(frame, ix, iy, 0.5f));
                        lit += shows ? 1 : 0;
                    }
                    if (!crosses) {
                        fail(std::string(name) + ": the horizon does not cross the row");
                    }
                    ++n.crossing;
                    if (on_frame == 0 || lit != on_frame) {
                        fail(std::string(name) + ": the horizon is not whole: " +
                             std::to_string(lit) + " of " + std::to_string(on_frame) +
                             " points on the frame lit");
                    }
                    ++n.whole;
                    try {
                        glideslope::test::judge_checklist(frame, expected, nullptr);
                        ++n.read;
                    } catch (const glideslope::test::ChecklistWrong& e) {
                        if (n.first_misread.empty()) {
                            n.first_misread = std::string(name) + ": " + e.what();
                        }
                    }
                }
            }
        }
        return n;
    };

    std::vector<std::future<Counts>> walking;
    for (const auto& [width, height] : sizes) {
        walking.push_back(std::async(std::launch::async, walk, width, height));
    }
    Counts all;
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        const Counts n = walking[i].get();
        std::printf("%dx%d: %zu frames, the horizon across a checklist row in each; the "
                    "checklist read whole in %zu\n",
                    sizes[i].first, sizes[i].second, n.frames, n.read);
        all.frames += n.frames;
        all.crossing += n.crossing;
        all.whole += n.whole;
        all.read += n.read;
        all.expected_frames += n.expected_frames;
        if (all.first_misread.empty()) {
            all.first_misread = n.first_misread;
        }
    }
    std::printf("%zu frames of %zu, the horizon across a checklist row in %zu and whole in "
                "%zu; the checklist read whole in %zu\n",
                all.frames, all.expected_frames, all.crossing, all.whole, all.read);
    if (!all.first_misread.empty()) {
        fail(std::to_string(all.frames - all.read) + " of " + std::to_string(all.frames) +
             " frames misread the checklist; the first, " + all.first_misread);
    }
    if (all.frames != all.expected_frames || all.crossing != all.frames ||
        all.whole != all.frames || all.read != all.frames) {
        fail("counts wrong");
    }
    return 0;
}
