// hud_horizon_check - the horizon, put level with every HUD row on purpose, is
// drawn whole and leaves the HUD read and judged as a shot is.
//
//   glideslope_hud_horizon_check
//
// The HUD tests fly in the live weather, so where the horizon lies at the
// shot's tick is the day's wind's to decide. Three of their checks compare
// lines whole, and a stroke of the horizon through a row reads as "?". So the
// HUD's text is drawn no larger than keeps it left of the horizon's reach
// (gfx::hud_layout), and the horizon is drawn whole. This holds both, in two
// parts.
//
// **The layout, at every size from 1x1 to 4096x4096**: the text is clear of
// the horizon exactly where the frame is at least
// gfx::hud_narrowest_clear_width wide; where it is, the text block ends a
// pixel short of the furthest left the horizon reaches; and the scale is the
// largest that does so, up to a pixel for every 240 of the smaller side.
// Larger frames are left out: the scale grows with the frame, and 4096 is
// past every size the tests shoot and most displays.
//
// **Frames**: for every case the HUD's lines can be in - the pilot flying,
// the AI holding, the AI flying to a waypoint; with and without the Mach
// number and flight level; the gear fixed, up and down - on landscape, square
// and portrait frames, the narrowest the text is clear at and two narrower,
// and for each HUD row of that case, it pitches and banks the aircraft so
// that the horizon meets the middle of the row's height at its left end, its
// middle and its right end, banked 30 and 90 degrees either way, and level.
// It paints the HUD's mesh (gfx::hud_mesh) over a sky and requires:
//
//   - the horizon whole: every pixel under its centre line that is on the
//     frame and above the credits' strip, which darkens it, in the HUD's
//     colour, but for the last pixel at each end, which the line's square
//     end may leave uncovered; and
//   - on a frame wide enough, the HUD judged as glideslope_hud_check judges a
//     shot (hud_judge.hpp), with the line of who is flying the words
//     frame_hud.cmake looks for.
//
// On a frame too narrow for the text to be clear, the horizon is still whole,
// and crosses the text where it runs through it. The HUD is judged there only when
// the horizon misses the text block; the frames where it does not are
// counted, and there must be some, since that is what "too narrow" says.
//
// Every count is held to the number there should be. Exits 0 if all is well,
// 1 with the first thing wrong.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "canvas.hpp"
#include "hud_judge.hpp"
#include "world/dem.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <future>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using glideslope::gfx::Frame;

// What is wrong, thrown: the sizes are walked on threads, and what one of
// them throws is thrown again where main collects it.
struct Wrong : std::runtime_error {
    using std::runtime_error::runtime_error;
};
[[noreturn]] void fail(const std::string& why) {
    throw Wrong(why);
}

using glideslope::test::Canvas;

bool hud_coloured(const Frame& frame, int x, int y) {
    const auto at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) +
                     static_cast<std::size_t>(x)) *
                    4;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto want = std::lround(glideslope::gfx::hud_colour[i] * 255.0f);
        if (std::abs(static_cast<long>(frame.rgba[at + i]) - want) > 2) {
            return false;
        }
    }
    return true;
}

// The layout at every size from 1x1 to `most` square.
void check_layouts(int most) {
    std::size_t sizes = 0;
    std::size_t clear = 0;
    for (int w = 1; w <= most; ++w) {
        for (int h = 1; h <= most; ++h) {
            ++sizes;
            const auto layout = glideslope::gfx::hud_layout(w, h);
            const bool is_clear = glideslope::gfx::hud_text_clear_of_horizon(w, h);
            const auto where = std::to_string(w) + "x" + std::to_string(h);
            if (is_clear != (w >= glideslope::gfx::hud_narrowest_clear_width)) {
                fail(where + ": the text is " + (is_clear ? "" : "not ") +
                     "clear of the horizon, and the narrowest clear is " +
                     std::to_string(glideslope::gfx::hud_narrowest_clear_width));
            }
            const auto reach = [&](int scale) {
                return w / 3.0 - std::max(2.0, scale * 1.0) / 2.0;
            };
            const auto right = [&](int scale) {
                return 2.0 * 6 * scale + static_cast<double>(glideslope::gfx::hud_columns) * 6 * scale;
            };
            const int most_scale = std::max(1, std::min(w, h) / 240);
            if (layout.scale < 1 || layout.scale > most_scale) {
                fail(where + ": scale " + std::to_string(layout.scale));
            }
            if (is_clear) {
                ++clear;
                if (right(layout.scale) + 1.0 > reach(layout.scale)) {
                    fail(where + ": the text ends at " + std::to_string(right(layout.scale)) +
                         " and the horizon reaches " + std::to_string(reach(layout.scale)));
                }
            }
            // The largest that keeps clear: one larger would not, or is past
            // a pixel for every 240.
            const int next = layout.scale + 1;
            if (next <= most_scale && right(next) + 1.0 <= reach(next)) {
                fail(where + ": scale " + std::to_string(layout.scale) + " when " +
                     std::to_string(next) + " would be clear");
            }
        }
    }
    const auto expected_clear = static_cast<std::size_t>(most) *
                                static_cast<std::size_t>(
                                    most - glideslope::gfx::hud_narrowest_clear_width + 1);
    if (sizes != static_cast<std::size_t>(most) * static_cast<std::size_t>(most) ||
        clear != expected_clear) {
        fail("the layouts walked were " + std::to_string(sizes) + ", " + std::to_string(clear) +
             " clear, not " + std::to_string(most * most) + " and " +
             std::to_string(expected_clear));
    }
    std::printf("the layout at %zu sizes, 1x1 to %dx%d: clear of the horizon at the %zu at "
                "least %d wide, and nowhere else\n",
                sizes, most, most, clear, glideslope::gfx::hud_narrowest_clear_width);
}

struct Flying {
    const char* what;
    bool ai;
    std::string autopilot;
    std::string said; // the line, as frame_hud.cmake looks for it
};

struct Regime {
    const char* what;
    double mach;
    double pressure_altitude_ft;
};

struct Gear {
    const char* what;
    std::optional<double> position;
};

} // namespace

int run();

int main() {
    try {
        return run();
    } catch (const Wrong& e) {
        std::fprintf(stderr, "glideslope_hud_horizon_check: %s\n", e.what());
        return 1;
    }
}

int run() {
    check_layouts(4096);

    const std::vector<Flying> flyings{
        {"the pilot flying", false, "", "FLYING PILOT"},
        {"the AI holding", true, "HOLD", "FLYING AI HOLD"},
        {"the AI flying to a waypoint", true, "NAV THE_HEADS", "FLYING AI NAV THE HEADS"}};
    const std::vector<Regime> regimes{{"slow and low", 0.15, 3000.0},
                                      {"with Mach and flight level", 0.82, 35000.0}};
    const std::vector<Gear> gears{{"gear fixed", std::nullopt}, {"gear up", 0.0},
                                  {"gear down", 1.0}};
    // Landscape, square and portrait; the narrowest clear, and four too
    // narrow.
    const std::vector<std::pair<int, int>> sizes{{640, 480},  {1280, 720}, {800, 800},
                                                 {600, 1000}, {1080, 1920}, {474, 800},
                                                 {473, 600},  {360, 640}};
    const std::vector<double> banks{-90.0, -30.0, 0.0, 30.0, 90.0};
    // Where along the horizon it meets the row: its left end, its middle and
    // its right end, as fractions of its half-length from the middle. Level,
    // the three are one frame, and only the middle is shot.
    const std::vector<double> along{-1.0, 0.0, 1.0};
    const std::vector<std::string> credits{glideslope::world::copernicus_dem_notice,
                                           glideslope::world::open_meteo_credit};
    constexpr double degrees = 180.0 / 3.14159265358979323846;

    // One size's frames, counted. Each size is walked on a thread of its own:
    // they share nothing but what they read, and the file judge_hud writes to.
    struct Counts {
        std::size_t frames = 0;
        std::size_t whole = 0;
        std::size_t judged = 0;
        std::size_t crossed_where_narrow = 0;
        std::size_t expected_frames = 0;
        std::size_t frames_clear = 0;
        std::size_t judged_clear = 0;
        bool clear = false;
    };
    const auto walk = [&](int width, int height) {
        Counts n;
        std::size_t& frames = n.frames;
        std::size_t& whole = n.whole;
        std::size_t& judged = n.judged;
        std::size_t& crossed_where_narrow = n.crossed_where_narrow;
        std::size_t& expected_frames = n.expected_frames;
        std::size_t& frames_clear = n.frames_clear;
        std::size_t& judged_clear = n.judged_clear;
        Canvas canvas(width, height);
        const bool clear = glideslope::gfx::hud_text_clear_of_horizon(width, height);
        n.clear = clear;
        for (const Flying& flying : flyings) {
            for (const Regime& regime : regimes) {
                for (const Gear& gear : gears) {
                    glideslope::gfx::HudReadings r;
                    r.airspeed_kts = 102.4;
                    r.altitude_ft = 3012.2;
                    r.heading_deg = 164.2;
                    r.vertical_speed_fpm = -120.3;
                    r.mach = regime.mach;
                    r.pressure_altitude_ft = regime.pressure_altitude_ft;
                    r.ai_flying = flying.ai;
                    r.autopilot = flying.autopilot;
                    glideslope::gfx::ControlsShown controls;
                    controls.aileron = 0.3;
                    controls.elevator = -0.05;
                    controls.rudder = 0.0;
                    controls.throttle = 0.7;
                    controls.flaps = 0.33;
                    controls.gear = gear.position;
                    r.controls = controls;
                    r.credits = credits;
                    const auto layout = glideslope::gfx::hud_layout(width, height);
                    const std::size_t lines = glideslope::gfx::hud_lines(r).size();
                    // The six of the flight, Mach and flight level where they
                    // apply, who is flying, four controls, and the gear where
                    // it retracts.
                    const std::size_t rows_expected = 6u + (regime.mach >= 0.4 ? 2u : 0u) + 1u + 4u +
                                                      (gear.position ? 1u : 0u);
                    if (lines != rows_expected) {
                        fail(std::string(flying.what) + ", " + regime.what + ", " + gear.what +
                             ": " + std::to_string(lines) + " HUD rows, not " +
                             std::to_string(rows_expected));
                    }
                    expected_frames += lines * ((banks.size() - 1) * along.size() + 1);
                    const auto block = glideslope::gfx::hud_text_block(r, width, height);
                    for (std::size_t row = 0; row < lines; ++row) {
                        for (const double bank : banks) {
                            for (const double s : along) {
                                if (bank == 0.0 && s != 0.0) {
                                    continue;
                                }
                                ++frames;
                                frames_clear += clear ? 1 : 0;
                                char name[256];
                                std::snprintf(name, sizeof name,
                                              "%dx%d, %s, %s, %s, row %zu, bank %+.0f, %+.2f "
                                              "along",
                                              width, height, flying.what, regime.what,
                                              gear.what, row + 1, bank, s);
                                // The point `s` of the way from the middle
                                // to the right-hand end, at the middle of the
                                // row's glyphs.
                                const double y = layout.top +
                                                 static_cast<double>(row) * layout.cell_height() +
                                                 3.5 * layout.scale;
                                const double half = width / 6.0;
                                const double cy = y + s * half * std::sin(bank / degrees);
                                r.roll_deg = bank;
                                r.pitch_deg = (cy - height / 2.0) * 100.0 / height;

                                const Frame& frame =
                                    canvas.paint(glideslope::gfx::hud_mesh(r, width, height));

                                // **Whole**: every pixel under the centre line,
                                // on the frame and above the credits' strip.
                                const auto h = glideslope::gfx::hud_horizon(r, width, height);
                                const double length = std::hypot(h.x1 - h.x0, h.y1 - h.y0);
                                const auto steps = static_cast<int>(std::ceil(length * 2.0));
                                const double strip_top = glideslope::gfx::credit_layout(
                                                             width, height,
                                                             glideslope::gfx::credit_lines(
                                                                 credits, width)
                                                                 .size())
                                                             .top;
                                int on_frame = 0;
                                int lit = 0;
                                bool meets_text = false;
                                for (int i = 0; i <= steps; ++i) {
                                    const double t = static_cast<double>(i) / steps;
                                    const double px = h.x0 + (h.x1 - h.x0) * t;
                                    const double py = h.y0 + (h.y1 - h.y0) * t;
                                    // Within half its thickness of the
                                    // block: its pixels could land there.
                                    if (px >= block.left - h.thickness / 2 &&
                                        px < block.right + h.thickness / 2 &&
                                        py >= block.top - h.thickness / 2 &&
                                        py < block.bottom + h.thickness / 2) {
                                        meets_text = true;
                                    }
                                    // The pixel a point lies in has its
                                    // centre within 0.71 of it, so under a
                                    // line two or more across - except
                                    // within a pixel of the line's ends.
                                    if (t * length < 1.0 || (1.0 - t) * length < 1.0) {
                                        continue;
                                    }
                                    if (px < 0.0 || py < 0.0 || px >= width || py >= strip_top) {
                                        continue;
                                    }
                                    ++on_frame;
                                    lit += hud_coloured(frame, static_cast<int>(px),
                                                        static_cast<int>(py))
                                               ? 1
                                               : 0;
                                }
                                if (on_frame == 0 || lit != on_frame) {
                                    fail(std::string(name) + ": the horizon is not whole: " +
                                         std::to_string(lit) + " of " +
                                         std::to_string(on_frame) + " points on the frame lit, " +
                                         std::to_string(steps + 1) + " along it");
                                }
                                ++whole;

                                if (clear && meets_text) {
                                    fail(std::string(name) +
                                         ": the horizon reaches the text where it should be "
                                         "clear");
                                }
                                if (!clear && meets_text) {
                                    ++crossed_where_narrow;
                                    continue;
                                }
                                const std::map<std::string, double> state{
                                    {"kcas", r.airspeed_kts},
                                    {"alt_ft", r.altitude_ft},
                                    {"heading", r.heading_deg},
                                    {"vs_fpm", r.vertical_speed_fpm},
                                    {"pitch", r.pitch_deg},
                                    {"roll", r.roll_deg},
                                    {"mach", r.mach},
                                    {"pa_ft", r.pressure_altitude_ft},
                                    {"ai", r.ai_flying ? 1.0 : 0.0},
                                    {"aileron", controls.aileron},
                                    {"elevator", controls.elevator},
                                    {"rudder", controls.rudder},
                                    {"throttle", controls.throttle},
                                    {"flaps", controls.flaps},
                                    {"gear", gear.position ? *gear.position : -1.0}};
                                std::string said;
                                try {
                                    said = glideslope::test::judge_hud(
                                        frame, state, 1, credits, nullptr);
                                } catch (const glideslope::test::HudWrong& e) {
                                    fail(std::string(name) + ": " + e.what());
                                }
                                if (said != flying.said) {
                                    fail(std::string(name) + ": the HUD says \"" + said +
                                         "\", not \"" + flying.said + "\"");
                                }
                                ++judged;
                                judged_clear += clear ? 1 : 0;
                            }
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
        std::printf("%dx%d, text %s: %zu frames, the horizon whole in each\n", sizes[i].first,
                    sizes[i].second, n.clear ? "clear of the horizon" : "too wide to be clear",
                    n.frames);
        all.frames += n.frames;
        all.whole += n.whole;
        all.judged += n.judged;
        all.crossed_where_narrow += n.crossed_where_narrow;
        all.expected_frames += n.expected_frames;
        all.frames_clear += n.frames_clear;
        all.judged_clear += n.judged_clear;
    }
    const std::size_t frames = all.frames;
    const std::size_t whole = all.whole;
    const std::size_t judged = all.judged;
    const std::size_t crossed_where_narrow = all.crossed_where_narrow;
    const std::size_t expected_frames = all.expected_frames;
    const std::size_t frames_clear = all.frames_clear;
    const std::size_t judged_clear = all.judged_clear;
    std::printf("%zu frames, the horizon whole in %zu; the HUD judged whole in %zu of the %zu "
                "where the text is clear; where the frame is too narrow for that, %zu judged "
                "and %zu with the horizon over the text\n",
                frames, whole, judged_clear, frames_clear, judged - judged_clear,
                crossed_where_narrow);
    if (frames != expected_frames || whole != frames || judged_clear != frames_clear ||
        judged + crossed_where_narrow != frames || crossed_where_narrow == 0) {
        fail("counts wrong: " + std::to_string(frames) + " frames of " +
             std::to_string(expected_frames) + ", " + std::to_string(whole) + " whole, " +
             std::to_string(judged) + " judged, " + std::to_string(crossed_where_narrow) +
             " crossed where too narrow");
    }
    return 0;
}
