#pragma once

// The head-up display: airspeed, altitude, heading, vertical speed and
// attitude, as text, with a horizon line.
//
// **Text is drawn from a 5-by-7 pixel font, each font pixel a square of whole
// screen pixels**, so a frame can be read back exactly: read_text() decodes what
// text_mesh() drew, which is how a test holds the numbers on screen to the
// simulation's state. The font is this project's own.

#include "gfx/renderer.hpp"
#include "gfx/scene.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace glideslope::gfx {

struct HudReadings {
    double airspeed_kts = 0.0; // calibrated
    double altitude_ft = 0.0;  // above sea level
    double heading_deg = 0.0;  // true
    double vertical_speed_fpm = 0.0;
    double pitch_deg = 0.0;
    double roll_deg = 0.0;
    // Whose data is on screen, shown along the bottom in capitals; empty for
    // nothing.
    std::string credit;
};

// The HUD's lines, top to bottom:
//   SPD  102 KT
//   ALT   3012 FT
//   HDG 164
//   VS   -120 FPM
//   PITCH  +2.4
//   BANK   -5.0
// Speeds, altitudes, headings and vertical speeds to the nearest whole unit;
// pitch and bank to a tenth of a degree.
std::vector<std::string> hud_lines(const HudReadings& readings);

// Where text goes on a frame of `width` by `height` pixels: every font pixel a
// `scale`-pixel square, a character cell six font pixels wide and ten high,
// starting two cells in from the top left.
struct TextLayout {
    int scale = 1;
    int left = 0; // pixels
    int top = 0;
    int cell_width() const {
        return 6 * scale;
    }
    int cell_height() const {
        return 10 * scale;
    }
};

TextLayout hud_layout(int width, int height);

// Where the credit goes: the same text, on one line two cells in from the
// bottom left.
TextLayout credit_layout(int width, int height);

// The glyph for `c`: seven rows, the top first, each five bits with the
// leftmost pixel the highest. Null for a character the font lacks.
const std::array<std::uint8_t, 7>* glyph(char c);

// Every character the font has.
const std::string& font_characters();

inline constexpr std::array<float, 4> hud_colour{0.2f, 1.0f, 0.4f, 1.0f};

// The HUD for `readings` on a frame of `width` by `height`: its text, the
// credit, and a horizon line across the middle, pitched and banked with the aircraft.
// In clip space, drawn over everything else.
Mesh hud_mesh(const HudReadings& readings, int width, int height);

// Text read back from a frame drawn with `layout`: `lines` lines of `columns`
// characters each, in the HUD's colour. A cell that is not a glyph of the font
// reads as '?'. Trailing spaces are removed.
std::vector<std::string> read_text(const Frame& frame, const TextLayout& layout,
                                   std::size_t lines, std::size_t columns);

} // namespace glideslope::gfx
