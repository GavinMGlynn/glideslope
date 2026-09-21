#include "gfx/hud.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>

namespace glideslope::gfx {

namespace {

// Drawn here, a row of five pixels at a time: '#' is lit.
const std::map<char, std::array<const char*, 7>>& font_source() {
    static const std::map<char, std::array<const char*, 7>> font{
        {' ', {"     ", "     ", "     ", "     ", "     ", "     ", "     "}},
        {'(', {"   # ", "  #  ", " #   ", " #   ", " #   ", "  #  ", "   # "}},
        {')', {" #   ", "  #  ", "   # ", "   # ", "   # ", "  #  ", " #   "}},
        {'+', {"     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     "}},
        {',', {"     ", "     ", "     ", "     ", " ##  ", "  #  ", " #   "}},
        {'-', {"     ", "     ", "     ", "#####", "     ", "     ", "     "}},
        {'.', {"     ", "     ", "     ", "     ", "     ", " ##  ", " ##  "}},
        {'/', {"    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    "}},
        {'0', {" ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "}},
        {'1', {"  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "}},
        {'2', {" ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"}},
        {'3', {"#####", "   # ", "  #  ", "   # ", "    #", "#   #", " ### "}},
        {'4', {"   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # "}},
        {'5', {"#####", "#    ", "#### ", "    #", "    #", "#   #", " ### "}},
        {'6', {"  ## ", " #   ", "#    ", "#### ", "#   #", "#   #", " ### "}},
        {'7', {"#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   "}},
        {'8', {" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "}},
        {'9', {" ### ", "#   #", "#   #", " ####", "    #", "   # ", " ##  "}},
        {':', {"     ", " ##  ", " ##  ", "     ", " ##  ", " ##  ", "     "}},
        {';', {"     ", " ##  ", " ##  ", "     ", " ##  ", "  #  ", " #   "}},
        {'A', {" ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"}},
        {'B', {"#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "}},
        {'C', {" ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### "}},
        {'D', {"###  ", "#  # ", "#   #", "#   #", "#   #", "#  # ", "###  "}},
        {'E', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####"}},
        {'F', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "}},
        {'G', {" ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ####"}},
        {'H', {"#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"}},
        {'I', {" ### ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "}},
        {'J', {"  ###", "   # ", "   # ", "   # ", "   # ", "#  # ", " ##  "}},
        {'K', {"#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #"}},
        {'L', {"#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"}},
        {'M', {"#   #", "## ##", "# # #", "# # #", "#   #", "#   #", "#   #"}},
        {'N', {"#   #", "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #"}},
        {'O', {" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
        {'P', {"#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "}},
        {'Q', {" ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #"}},
        {'R', {"#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #"}},
        {'S', {" ####", "#    ", "#    ", " ### ", "    #", "    #", "#### "}},
        {'T', {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "}},
        {'U', {"#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
        {'V', {"#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  "}},
        {'W', {"#   #", "#   #", "#   #", "# # #", "# # #", "# # #", " # # "}},
        {'X', {"#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #"}},
        {'Y', {"#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  "}},
        {'Z', {"#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####"}},
    };
    return font;
}

const std::map<char, std::array<std::uint8_t, 7>>& font() {
    static const std::map<char, std::array<std::uint8_t, 7>> bits = [] {
        std::map<char, std::array<std::uint8_t, 7>> out;
        for (const auto& [c, rows] : font_source()) {
            std::array<std::uint8_t, 7> g{};
            for (std::size_t r = 0; r < 7; ++r) {
                for (int x = 0; x < 5; ++x) {
                    if (rows[r][x] == '#') {
                        g[r] = static_cast<std::uint8_t>(g[r] | (0x10 >> x));
                    }
                }
            }
            out[c] = g;
        }
        return out;
    }();
    return bits;
}

// A quad of screen pixels [x0, x1) by [y0, y1), in clip space.
void add_rect(Mesh& mesh, double x0, double y0, double x1, double y1, int width,
              int height, const std::array<float, 4>& colour) {
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto ndc_x = [&](double x) {
        return static_cast<float>(2.0 * x / width - 1.0);
    };
    const auto ndc_y = [&](double y) {
        return static_cast<float>(1.0 - 2.0 * y / height);
    };
    for (const auto& [x, y] :
         {std::pair{x0, y0}, std::pair{x1, y0}, std::pair{x1, y1}, std::pair{x0, y1}}) {
        mesh.vertices.push_back({{ndc_x(x), ndc_y(y), 0.5f}, colour});
    }
    for (const std::uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) {
        mesh.indices.push_back(first + i);
    }
}

// A quad along a line from (x0, y0) to (x1, y1), `thickness` pixels across.
void add_line(Mesh& mesh, double x0, double y0, double x1, double y1, double thickness,
              int width, int height, const std::array<float, 4>& colour) {
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length == 0.0) {
        return;
    }
    const double nx = -dy / length * thickness / 2;
    const double ny = dx / length * thickness / 2;
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto ndc_x = [&](double x) {
        return static_cast<float>(2.0 * x / width - 1.0);
    };
    const auto ndc_y = [&](double y) {
        return static_cast<float>(1.0 - 2.0 * y / height);
    };
    for (const auto& [x, y] :
         {std::pair{x0 + nx, y0 + ny}, std::pair{x1 + nx, y1 + ny},
          std::pair{x1 - nx, y1 - ny}, std::pair{x0 - nx, y0 - ny}}) {
        mesh.vertices.push_back({{ndc_x(x), ndc_y(y), 0.5f}, colour});
    }
    for (const std::uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) {
        mesh.indices.push_back(first + i);
    }
}

long nearest(double v) {
    return std::lround(v);
}

} // namespace

std::vector<std::string> hud_lines(const HudReadings& r) {
    char buffer[64];
    std::vector<std::string> lines;
    std::snprintf(buffer, sizeof buffer, "SPD %4ld KT", nearest(r.airspeed_kts));
    lines.emplace_back(buffer);
    std::snprintf(buffer, sizeof buffer, "ALT %6ld FT", nearest(r.altitude_ft));
    lines.emplace_back(buffer);
    long heading = nearest(r.heading_deg) % 360;
    if (heading < 0) {
        heading += 360;
    }
    std::snprintf(buffer, sizeof buffer, "HDG %03ld", heading);
    lines.emplace_back(buffer);
    std::snprintf(buffer, sizeof buffer, "VS  %+5ld FPM",
                  nearest(r.vertical_speed_fpm));
    lines.emplace_back(buffer);
    std::snprintf(buffer, sizeof buffer, "PITCH %+5.1f", r.pitch_deg);
    lines.emplace_back(buffer);
    std::snprintf(buffer, sizeof buffer, "BANK  %+5.1f", r.roll_deg);
    lines.emplace_back(buffer);
    if (r.mach >= hud_mach_from) {
        std::snprintf(buffer, sizeof buffer, "MACH %4.2f", r.mach);
        lines.emplace_back(buffer);
    }
    if (r.pressure_altitude_ft >= hud_flight_level_from_ft) {
        std::snprintf(buffer, sizeof buffer, "FL %03ld", nearest(r.pressure_altitude_ft / 100.0));
        lines.emplace_back(buffer);
    }
    if (!r.autopilot.empty()) {
        // The font has no underscore: names written with them read as words.
        std::string shown = "AP  " + r.autopilot;
        std::replace(shown.begin(), shown.end(), '_', ' ');
        lines.push_back(shown);
    }
    return lines;
}

TextLayout hud_layout(int width, int height) {
    TextLayout layout;
    layout.scale = std::max(1, std::min(width, height) / 240);
    layout.left = 2 * layout.cell_width();
    layout.top = 2 * layout.cell_height();
    return layout;
}

std::size_t credit_columns(int width) {
    // One screen pixel a font pixel: six pixels a character, and two
    // characters' margin each side.
    return static_cast<std::size_t>(std::max(1, width / 6 - 4));
}

std::vector<std::string> credit_lines(const std::vector<std::string>& credits,
                                      int width) {
    const std::size_t columns = credit_columns(width);
    std::vector<std::string> lines;
    for (const std::string& credit : credits) {
        std::string text;
        for (std::size_t i = 0; i < credit.size(); ++i) {
            if (credit.compare(i, 2, "\xc2\xa9") == 0) {
                text += "(C)";
                ++i;
            } else {
                text.push_back(static_cast<char>(
                    std::toupper(static_cast<unsigned char>(credit[i]))));
            }
        }
        std::string line;
        std::size_t at = 0;
        while (at < text.size()) {
            std::size_t end = text.find(' ', at);
            if (end == std::string::npos) {
                end = text.size();
            }
            const std::string word = text.substr(at, end - at);
            if (!line.empty() && line.size() + 1 + word.size() > columns) {
                lines.push_back(line);
                line.clear();
            }
            line += (line.empty() ? "" : " ") + word;
            // A word longer than a line is cut.
            while (line.size() > columns) {
                lines.push_back(line.substr(0, columns));
                line.erase(0, columns);
            }
            at = end + 1;
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::size_t checklist_columns(int width) {
    // The right-hand half of the frame, with two characters' margin each
    // side, at one screen pixel a font pixel: six pixels a character.
    return static_cast<std::size_t>(std::max(1, width / 2 / 6 - 2));
}

std::vector<std::string> checklist_lines(const ChecklistOnScreen& showing, int width) {
    if (showing.phase.empty()) {
        return {};
    }
    const std::size_t columns = checklist_columns(width);
    // Capitals, and nothing the font has not got: a comma would read as a
    // question mark, which is worse than losing it.
    const auto spellable = [](const std::string& from) {
        std::string out;
        for (const char c : from) {
            const char up = static_cast<char>(
                std::toupper(static_cast<unsigned char>(c)));
            if (glyph(up) != nullptr) {
                out.push_back(up);
            }
        }
        return out;
    };
    std::size_t done = 0;
    for (const auto& [ticked, text] : showing.items) {
        done += ticked ? 1 : 0;
    }
    std::vector<std::string> lines;
    lines.push_back(spellable(showing.phase) + " " + std::to_string(done) + "/" +
                    std::to_string(showing.items.size()));
    for (const auto& [ticked, text] : showing.items) {
        lines.push_back(std::string(ticked ? "X " : "- ") + spellable(text));
    }
    for (std::string& line : lines) {
        if (line.size() > columns) {
            line.resize(columns);
        }
    }
    return lines;
}

TextLayout checklist_layout(int width, int height, std::size_t lines) {
    (void)height;
    (void)lines;
    TextLayout layout;
    layout.scale = 1;
    // Its right-hand edge two characters in from the frame's.
    layout.left = width - static_cast<int>(checklist_columns(width) + 2) *
                              layout.cell_width();
    layout.top = 2 * layout.cell_height();
    return layout;
}

TextLayout credit_layout(int width, int height, std::size_t lines) {
    (void)width;
    TextLayout layout;
    layout.scale = 1;
    layout.left = 2 * layout.cell_width();
    layout.top = height - static_cast<int>(lines + 1) * layout.cell_height();
    return layout;
}

const std::array<std::uint8_t, 7>* glyph(char c) {
    const auto it = font().find(c);
    return it == font().end() ? nullptr : &it->second;
}

const std::string& font_characters() {
    static const std::string all = [] {
        std::string s;
        for (const auto& [c, g] : font()) {
            s.push_back(c);
        }
        return s;
    }();
    return all;
}

namespace {

void add_text(Mesh& mesh, const std::vector<std::string>& lines,
              const TextLayout& layout, int width, int height) {
    for (std::size_t l = 0; l < lines.size(); ++l) {
        for (std::size_t i = 0; i < lines[l].size(); ++i) {
            const auto* g = glyph(lines[l][i]);
            if (g == nullptr) {
                continue;
            }
            const int x0 = layout.left + static_cast<int>(i) * layout.cell_width();
            const int y0 = layout.top + static_cast<int>(l) * layout.cell_height();
            for (int r = 0; r < 7; ++r) {
                for (int x = 0; x < 5; ++x) {
                    if (((*g)[static_cast<std::size_t>(r)] & (0x10 >> x)) != 0) {
                        add_rect(mesh, x0 + x * layout.scale, y0 + r * layout.scale,
                                 x0 + (x + 1) * layout.scale,
                                 y0 + (r + 1) * layout.scale, width, height,
                                 hud_colour);
                    }
                }
            }
        }
    }
}

} // namespace

// The credits, over a strip that darkens what is behind them by half - the
// HUD's horizon line included - so they can be read over anything.
void add_credits(Mesh& mesh, const std::vector<std::string>& credits, int width,
                 int height) {
    if (credits.empty()) {
        return;
    }
    const TextLayout layout = credit_layout(width, height, credits.size());
    add_rect(mesh, 0.0, layout.top, width, height, width, height,
             {0.0f, 0.0f, 0.0f, 0.5f});
    add_text(mesh, credits, layout, width, height);
}

Mesh hud_mesh(const HudReadings& readings, int width, int height) {
    Mesh mesh;
    const TextLayout layout = hud_layout(width, height);
    add_text(mesh, hud_lines(readings), layout, width, height);

    // The horizon: across the middle third, moved down the screen as the nose
    // rises - a degree of pitch a hundredth of the height - and turned against
    // the bank.
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    const double cx = width / 2.0;
    const double cy = height / 2.0 + readings.pitch_deg * height / 100.0;
    const double half = width / 6.0;
    const double angle = readings.roll_deg / degrees;
    add_line(mesh, cx - half * std::cos(angle), cy + half * std::sin(angle),
             cx + half * std::cos(angle), cy - half * std::sin(angle),
             std::max(2.0, layout.scale * 1.0), width, height, hud_colour);
    // The aircraft's own reference, fixed at the centre.
    add_rect(mesh, cx - 3 * layout.scale, height / 2.0 - layout.scale,
             cx + 3 * layout.scale, height / 2.0 + layout.scale, width, height,
             hud_colour);
    const std::vector<std::string> checklist =
        checklist_lines(readings.checklist, width);
    if (!checklist.empty()) {
        add_text(mesh, checklist, checklist_layout(width, height, checklist.size()),
                 width, height);
    }
    // Last, over the horizon wherever it runs.
    add_credits(mesh, credit_lines(readings.credits, width), width, height);
    return mesh;
}

Mesh credits_mesh(const std::vector<std::string>& credits, int width, int height) {
    Mesh mesh;
    add_credits(mesh, credit_lines(credits, width), width, height);
    return mesh;
}

std::vector<std::string> read_text(const Frame& frame, const TextLayout& layout,
                                   std::size_t lines, std::size_t columns) {
    const auto lit = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
            return false;
        }
        const auto at = static_cast<std::size_t>(y * frame.width + x) * 4;
        const auto near = [](std::uint8_t v, float c) {
            return std::abs(static_cast<int>(v) -
                            static_cast<int>(std::lround(c * 255.0f))) <= 2;
        };
        return near(frame.rgba[at], hud_colour[0]) &&
               near(frame.rgba[at + 1], hud_colour[1]) &&
               near(frame.rgba[at + 2], hud_colour[2]);
    };
    std::vector<std::string> out;
    for (std::size_t l = 0; l < lines; ++l) {
        std::string line;
        for (std::size_t i = 0; i < columns; ++i) {
            const int x0 = layout.left + static_cast<int>(i) * layout.cell_width();
            const int y0 = layout.top + static_cast<int>(l) * layout.cell_height();
            std::array<std::uint8_t, 7> seen{};
            for (int r = 0; r < 7; ++r) {
                for (int x = 0; x < 5; ++x) {
                    // The centre of the font pixel's square.
                    if (lit(x0 + x * layout.scale + layout.scale / 2,
                            y0 + r * layout.scale + layout.scale / 2)) {
                        seen[static_cast<std::size_t>(r)] = static_cast<std::uint8_t>(
                            seen[static_cast<std::size_t>(r)] | (0x10 >> x));
                    }
                }
            }
            char found = '?';
            for (const auto& [c, g] : font()) {
                if (g == seen) {
                    found = c;
                    break;
                }
            }
            line.push_back(found);
        }
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        out.push_back(line);
    }
    return out;
}

} // namespace glideslope::gfx
