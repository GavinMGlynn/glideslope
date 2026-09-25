// hud_horizon_check - a frame with the horizon drawn across every HUD row,
// built on purpose, is read and judged as a shot is.
//
//   glideslope_hud_horizon_check
//
// The HUD tests fly in the live weather, so whether the horizon crosses the
// HUD's rows at the shot's tick, and which, is the day's wind's to decide:
// three of the checks compare lines whole, and a stroke of the horizon through
// a row reads as "?". This builds the crossing instead. For every case the
// HUD's lines can be in - the pilot flying, the AI holding, the AI flying to a
// waypoint; with and without the Mach number and flight level; the gear fixed,
// up and down - on each frame size the frame tests shoot, and for each HUD row
// of that case, level and banked 20 degrees either way, it pitches and banks
// the aircraft so that the horizon passes through the middle of the row
// inside the text block (gfx::hud_text_block), paints the HUD's mesh
// (gfx::hud_mesh) into a frame over a sky, and judges it as
// glideslope_hud_check judges a shot (hud_judge.hpp), holding the line of who
// is flying to the words frame_hud.cmake looks for. That the horizon does
// cross the row is worked out from its own line (gfx::hud_horizon), and every
// row of every case must be crossed: the count is held to the number of rows.
// Exits 0 if all is well, 1 with the first frame wrong.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "hud_judge.hpp"
#include "world/dem.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

using glideslope::gfx::Frame;
using glideslope::gfx::Mesh;

// What the frames are cleared to before the HUD: a sky, nothing like the
// HUD's colour.
constexpr std::array<float, 4> sky{0.40f, 0.60f, 0.90f, 1.0f};

// The mesh painted as the GPU draws the HUD: each triangle fills the pixels
// whose centres it covers, blended over what is there by its alpha.
Frame paint(const Mesh& mesh, int width, int height) {
    Frame frame;
    frame.width = width;
    frame.height = height;
    std::vector<std::array<float, 3>> colour(static_cast<std::size_t>(width * height),
                                             {sky[0], sky[1], sky[2]});
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        std::array<std::array<double, 2>, 3> p{};
        for (std::size_t k = 0; k < 3; ++k) {
            const auto& v = mesh.vertices[mesh.indices[t + k]].position;
            p[k] = {(static_cast<double>(v[0]) + 1.0) / 2.0 * width,
                    (1.0 - static_cast<double>(v[1])) / 2.0 * height};
        }
        const auto& c = mesh.vertices[mesh.indices[t]].colour;
        const auto edge = [](const std::array<double, 2>& a, const std::array<double, 2>& b,
                             double x, double y) {
            return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
        };
        const double area = edge(p[0], p[1], p[2][0], p[2][1]);
        if (area == 0.0) {
            continue;
        }
        const int x_from = std::max(0, static_cast<int>(std::floor(
                                           std::min({p[0][0], p[1][0], p[2][0]}))));
        const int x_to = std::min(width - 1, static_cast<int>(std::ceil(
                                                 std::max({p[0][0], p[1][0], p[2][0]}))));
        const int y_from = std::max(0, static_cast<int>(std::floor(
                                           std::min({p[0][1], p[1][1], p[2][1]}))));
        const int y_to = std::min(height - 1, static_cast<int>(std::ceil(
                                                  std::max({p[0][1], p[1][1], p[2][1]}))));
        for (int y = y_from; y <= y_to; ++y) {
            for (int x = x_from; x <= x_to; ++x) {
                const double cx = x + 0.5;
                const double cy = y + 0.5;
                const double a = edge(p[1], p[2], cx, cy) * area;
                const double b = edge(p[2], p[0], cx, cy) * area;
                const double d = edge(p[0], p[1], cx, cy) * area;
                if (a < 0.0 || b < 0.0 || d < 0.0) {
                    continue;
                }
                auto& out = colour[static_cast<std::size_t>(y * width + x)];
                for (std::size_t i = 0; i < 3; ++i) {
                    out[i] = c[i] * c[3] + out[i] * (1.0f - c[3]);
                }
            }
        }
    }
    frame.rgba.resize(colour.size() * 4);
    for (std::size_t i = 0; i < colour.size(); ++i) {
        for (std::size_t k = 0; k < 3; ++k) {
            frame.rgba[i * 4 + k] = static_cast<std::uint8_t>(
                std::lround(std::clamp(colour[i][k], 0.0f, 1.0f) * 255.0f));
        }
        frame.rgba[i * 4 + 3] = 255;
    }
    return frame;
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

int main() {
    const std::vector<Flying> flyings{
        {"the pilot flying", false, "", "FLYING PILOT"},
        {"the AI holding", true, "HOLD", "FLYING AI HOLD"},
        {"the AI flying to a waypoint", true, "NAV THE_HEADS", "FLYING AI NAV THE HEADS"}};
    const std::vector<Regime> regimes{{"slow and low", 0.15, 3000.0},
                                      {"with Mach and flight level", 0.82, 35000.0}};
    const std::vector<Gear> gears{{"gear fixed", std::nullopt}, {"gear up", 0.0},
                                  {"gear down", 1.0}};
    const std::vector<std::pair<int, int>> sizes{{640, 480}, {1280, 720}};
    const std::vector<double> banks{-20.0, 0.0, 20.0};
    const std::vector<std::string> credits{glideslope::world::copernicus_dem_notice,
                                           glideslope::world::open_meteo_credit};
    constexpr double degrees = 180.0 / 3.14159265358979323846;

    std::size_t rows = 0;
    std::size_t crossed = 0;
    // What each judgement read goes here, not to the screen: a thousand
    // frames' worth.
    std::FILE* quiet = std::tmpfile();
    for (const auto& [width, height] : sizes) {
        for (const Flying& flying : flyings) {
            for (const Regime& regime : regimes) {
                for (const Gear& gear : gears) {
                    for (const double bank : banks) {
                        glideslope::gfx::HudReadings r;
                        r.airspeed_kts = 102.4;
                        r.altitude_ft = 3012.2;
                        r.heading_deg = 164.2;
                        r.vertical_speed_fpm = -120.3;
                        r.roll_deg = bank;
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
                        const auto block = glideslope::gfx::hud_text_block(r, width, height);
                        for (std::size_t row = 0; row < lines; ++row) {
                            ++rows;
                            char name[256];
                            std::snprintf(name, sizeof name,
                                          "%dx%d, %s, %s, %s, bank %+.0f, row %zu", width,
                                          height, flying.what, regime.what, gear.what, bank,
                                          row + 1);
                            // Through the middle of the row's glyphs, half a
                            // cell in from the block's right-hand end.
                            const double row_top =
                                layout.top + static_cast<double>(row) * layout.cell_height();
                            const double y = row_top + 3.5 * layout.scale;
                            const double x = block.right - 0.5 * layout.cell_width();
                            const double cy =
                                y + (x - width / 2.0) * std::tan(bank / degrees);
                            r.pitch_deg = (cy - height / 2.0) * 100.0 / height;

                            // That it crosses, from the horizon's own line:
                            // some point of it in the row's glyphs, inside
                            // the block.
                            const auto h = glideslope::gfx::hud_horizon(r, width, height);
                            bool crosses = false;
                            for (int i = 0; i <= 4000 && !crosses; ++i) {
                                const double s = i / 4000.0;
                                const double px = h.x0 + (h.x1 - h.x0) * s;
                                const double py = h.y0 + (h.y1 - h.y0) * s;
                                crosses = px >= block.left && px < block.right &&
                                          py >= row_top && py < row_top + 7.0 * layout.scale;
                            }
                            if (!crosses) {
                                std::fprintf(stderr,
                                             "glideslope_hud_horizon_check: %s: the horizon "
                                             "was built to cross the row and does not\n",
                                             name);
                                return 1;
                            }
                            ++crossed;

                            const Frame frame =
                                paint(glideslope::gfx::hud_mesh(r, width, height), width,
                                      height);
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
                                said = glideslope::test::judge_hud(frame, state, 1, credits,
                                                                   quiet != nullptr ? quiet
                                                                                    : stdout);
                            } catch (const glideslope::test::HudWrong& e) {
                                std::fprintf(stderr, "glideslope_hud_horizon_check: %s: %s\n",
                                             name, e.what());
                                return 1;
                            }
                            if (said != flying.said) {
                                std::fprintf(stderr,
                                             "glideslope_hud_horizon_check: %s: the HUD "
                                             "says \"%s\", not \"%s\"\n",
                                             name, said.c_str(), flying.said.c_str());
                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }
    // Every row of every case: the six of the flight, Mach and flight level in
    // half the cases, who is flying, four of the controls, and the gear in
    // two cases of three - on two sizes, at three banks.
    const std::size_t expected = sizes.size() * banks.size() * flyings.size() *
                                 (gears.size() * (6 + 1 + 4) * regimes.size() +
                                  gears.size() * 2 + regimes.size() * 2);
    if (quiet != nullptr) {
        std::fclose(quiet);
    }
    std::printf("the horizon crossed %zu HUD rows of %zu, each read and judged whole\n",
                crossed, expected);
    if (crossed != expected || rows != expected) {
        std::fprintf(stderr,
                     "glideslope_hud_horizon_check: %zu rows walked and %zu crossed, "
                     "not %zu\n",
                     rows, crossed, expected);
        return 1;
    }
    return 0;
}
