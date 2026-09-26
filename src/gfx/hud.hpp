#pragma once

// The head-up display: airspeed, altitude, heading, vertical speed and
// attitude, as text, with a horizon line; and for a fast aircraft high up, its
// Mach number and flight level.
//
// **Text is drawn from a 5-by-7 pixel font, each font pixel a square of whole
// screen pixels**, so a frame can be read back exactly: read_text() decodes what
// text_mesh() drew, which is how a test holds the numbers on screen to the
// simulation's state. The font is this project's own.

#include "gfx/renderer.hpp"
#include "gfx/scene.hpp"

#include <optional>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace glideslope::gfx {

// **A checklist on screen**: which phase's it is, and each of its items with
// whether it has been ticked. Empty phase means none is showing.
struct ChecklistOnScreen {
    std::string phase; // as the data spells it, "take-off"
    std::vector<std::pair<bool, std::string>> items; // ticked, and its words
};

// **Where the controls are**, as the flight model has them: the stick's
// aileron (right positive) and elevator (back positive), the rudder (right
// positive), the throttle and the flaps from nought to one, and the gear -
// down at one - where it retracts.
struct ControlsShown {
    double aileron = 0.0;
    double elevator = 0.0;
    double rudder = 0.0;
    double throttle = 0.0;
    double flaps = 0.0;
    std::optional<double> gear; // none: it does not retract
};

struct HudReadings {
    double airspeed_kts = 0.0; // calibrated - or the ground speed, where that is all there is
    // Shown as `GS`, not `SPD`: another aircraft's, ridden along in, whose
    // airspeed is not sent.
    bool ground_speed = false;
    double altitude_ft = 0.0;  // above sea level
    double heading_deg = 0.0;  // true
    double vertical_speed_fpm = 0.0;
    double pitch_deg = 0.0;
    double roll_deg = 0.0;
    double mach = 0.0;
    double pressure_altitude_ft = 0.0; // the standard atmosphere's for the pressure
    // What the AI is flying - "HOLD", or "NAV" and the waypoint it is flying
    // to - or empty when the pilot flies.
    std::string autopilot;
    // Whether the AI pilot has the aircraft: the HUD says who is flying, always.
    bool ai_flying = false;
    // The controls, when there is an aircraft to read them from.
    std::optional<ControlsShown> controls;
    // Whose data is on screen - each a credit its source asks for - shown
    // along the bottom.
    std::vector<std::string> credits;
    // The checklist being worked through, if one is showing.
    ChecklistOnScreen checklist;
};

// The HUD's lines, top to bottom:
//   SPD  102 KT
//   ALT   3012 FT
//   HDG 164
//   VS   -120 FPM
//   PITCH  +2.4
//   BANK   -5.0
//   MACH 0.82                  from hud_mach_from, and only then
//   FL 350                     from hud_flight_level_from_ft, and only then
//   FLYING PILOT               who has the aircraft, always: PILOT, or AI
//   FLYING AI NAV THE HEADS    and what the AI is flying - HOLD, or NAV and
//                              the waypoint it is flying to
//   STICK +0.30 -0.05          aileron and elevator, from -1 to 1
//   RUDDER +0.00
//   THROTTLE 0.70              from 0 to 1
//   FLAPS 0.33                 from 0 to 1
//   GEAR DOWN                  or UP, and only where it retracts
// Speeds, altitudes, headings and vertical speeds to the nearest whole unit;
// pitch and bank to a tenth of a degree; the Mach number to a hundredth; the
// flight level to the nearest hundred feet of pressure altitude.
std::vector<std::string> hud_lines(const HudReadings& readings);

// Where the Mach number and flight level apply: from Mach 0.40, where
// airliners' displays show it, and from 18,000 ft of pressure altitude, the
// United States' transition altitude, above which altitudes are flight levels.
inline constexpr double hud_mach_from = 0.40;
inline constexpr double hud_flight_level_from_ft = 18000.0;

// Where text goes on a frame of `width` by `height` pixels: every font pixel a
// `scale`-pixel square, a character cell six font pixels wide and ten high,
// starting two cells in from the top left. For the HUD (hud_layout) the scale
// is a pixel for every 240 of the frame's smaller side, but no larger than
// keeps the text block clear of the horizon (hud_text_clear_of_horizon), and
// never less than one.
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

// A box of screen pixels, its right and bottom edges exclusive.
struct PixelBox {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0; // exclusive
    double bottom = 0.0;
};

// **Credits** - the notices data sources ask to be shown with their data - are
// drawn small along the bottom left: one screen pixel a font pixel, each
// credit wrapped at its spaces to the frame's width, in capitals, with a
// copyright sign as "(C)", the font having no other.

// How many characters a line of credits holds on a frame `width` wide.
std::size_t credit_columns(int width);

// The credits as the lines drawn, for a frame `width` wide.
std::vector<std::string> credit_lines(const std::vector<std::string>& credits,
                                      int width);

// Where `lines` lines of credits go on a frame, ending a line above its bottom.
TextLayout credit_layout(int width, int height, std::size_t lines);

// The credits alone, in clip space, for a screen with no HUD. Credits are
// drawn over a strip across the frame's bottom, from the layout's top down,
// that darkens what is behind them by half, so they can be read over anything.
Mesh credits_mesh(const std::vector<std::string>& credits, int width, int height);

// **The checklist** is drawn down the top right, small as the credits are -
// one screen pixel a font pixel - so a long item reads without crowding the
// numbers down the left. Its first line is the phase and how much of it is
// done; then one line an item, "X" for ticked and "-" for still to do. An
// item too long for the line is cut rather than wrapped, so that every line
// but the first begins with a mark and the two can never be told apart.
//
// The font has capitals, digits and a little punctuation and no more, so the
// words are put in capitals and anything the font lacks - a comma - is
// dropped.

// How many characters a line of the checklist holds on a frame `width` wide.
std::size_t checklist_columns(int width);

// The checklist as the lines drawn, for a frame `width` wide. Empty when
// nothing is showing.
std::vector<std::string> checklist_lines(const ChecklistOnScreen& showing, int width);

// Where `lines` lines of checklist go on a frame: the right-hand side, two
// cells down from the top, as the HUD's own text starts.
TextLayout checklist_layout(int width, int height, std::size_t lines);

// **The checklist's panel**: the checklist is drawn over a panel that darkens
// what is behind it by half - the horizon included - as the credits' strip
// does, so the checklist reads whole wherever the horizon runs. The horizon
// is not cut for it: it shows through the panel, dimmed. The checklist sits
// in the frame's right half and the horizon reaches two-thirds across, so at
// the right pitch and bank it crosses any of the checklist's rows; keeping
// the checklist right of the horizon instead, as the HUD's text is kept left
// of it, would cut every item at a third of the width.
//
// The panel is a cell wider than the checklist's `checklist_columns` each
// side, a line above its first line and a line below its last - an empty line
// that reads as one, so a reader can tell that nothing is drawn under it.
PixelBox checklist_panel(int width, int height, std::size_t lines);

// The glyph for `c`: seven rows, the top first, each five bits with the
// leftmost pixel the highest. Null for a character the font lacks.
const std::array<std::uint8_t, 7>* glyph(char c);

// Every character the font has.
const std::string& font_characters();

inline constexpr std::array<float, 4> hud_colour{0.2f, 1.0f, 0.4f, 1.0f};

// **The HUD's text block**: its lines, top to bottom, each hud_columns cells
// wide, in screen pixels. hud_lines cuts any line longer than that - only a
// waypoint's name can make one.
inline constexpr std::size_t hud_columns = 24;
PixelBox hud_text_block(const HudReadings& readings, int width, int height);

// **The horizon line**: its centre from (x0, y0) to (x1, y1), in screen
// pixels, `thickness` across. Across the middle third of the frame, moved
// down the screen as the nose rises - a degree of pitch a hundredth of the
// height - and turned against the bank. It is drawn whole, always: it is the
// instrument.
struct HorizonLine {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    double thickness = 0.0;
};
HorizonLine hud_horizon(const HudReadings& readings, int width, int height);

// **Whether the HUD's text is clear of the horizon on a frame this size**:
// whether the text block ends, with a pixel to spare, left of the furthest
// left any pixel of the horizon can reach, at any pitch and bank - a third of
// the width less half the line's thickness. hud_layout makes it so wherever
// it can, by drawing the text no larger than fits; on a frame too narrow for
// even the smallest text to fit - under hud_narrowest_clear_width - it cannot,
// and there the horizon, drawn whole, can cross the right-hand end of the
// text's rows.
bool hud_text_clear_of_horizon(int width, int height);
inline constexpr int hud_narrowest_clear_width = 474;

// The HUD for `readings` on a frame of `width` by `height`: the horizon line,
// whole; the checklist's panel over it; the aircraft's mark, the HUD's text
// and the checklist over the panel, which dims none of them; and the credits
// over their strip, last - both panel and strip over the line wherever it
// runs. In clip space, drawn over everything else.
Mesh hud_mesh(const HudReadings& readings, int width, int height);

// Text read back from a frame drawn with `layout`: `lines` lines of `columns`
// characters each, in the HUD's colour. A cell that is not a glyph of the font
// reads as '?'. Trailing spaces are removed.
std::vector<std::string> read_text(const Frame& frame, const TextLayout& layout,
                                   std::size_t lines, std::size_t columns);

} // namespace glideslope::gfx
