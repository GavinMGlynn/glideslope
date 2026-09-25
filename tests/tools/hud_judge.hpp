#pragma once

// hud_judge.hpp - the HUD read back out of a frame and held to the flight's
// state at a tick: what glideslope_hud_check judges a shot by, and what
// glideslope_hud_horizon_check judges frames built on purpose by.
//
// judge_hud reads the HUD (gfx::read_text) and requires each number on it to
// be the state's, rounded as the HUD rounds: within half its last digit - the
// Mach number and flight level among them where they apply, and not shown
// where they do not; who is flying, and the controls; nothing more of the
// HUD's; and the credits along the bottom, `credits`, drawn as
// gfx::credit_lines draws them, with nothing below them. What it read is
// printed to `out`. It returns the line saying who is flying, and throws
// HudWrong with the reason if anything is not so.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace glideslope::test {

struct HudWrong : std::runtime_error {
    using std::runtime_error::runtime_error;
};

namespace hud_judge_detail {

[[noreturn]] inline void fail(const std::string& why) {
    throw HudWrong(why);
}

// The words of a HUD line.
inline std::vector<std::string> words_of(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> out;
    for (std::string w; in >> w;) {
        out.push_back(w);
    }
    return out;
}

inline double number(const std::string& text, const std::string& line) {
    char* end = nullptr;
    const double v = std::strtod(text.c_str(), &end);
    if (text.empty() || *end != '\0') {
        fail("the HUD line \"" + line + "\" holds \"" + text +
             "\" where a number should be");
    }
    return v;
}

} // namespace hud_judge_detail

inline std::string judge_hud(const gfx::Frame& frame, const std::map<std::string, double>& state,
                             long long tick, const std::vector<std::string>& credits,
                             std::FILE* out) {
    using hud_judge_detail::fail;
    using hud_judge_detail::number;
    using hud_judge_detail::words_of;
    const auto layout = gfx::hud_layout(frame.width, frame.height);
    const auto lines = gfx::read_text(frame, layout, 16, gfx::hud_columns);
    for (const auto& l : lines) {
        std::fprintf(out, "read: %s\n", l.c_str());
    }

    struct Field {
        std::string label;
        std::string unit; // empty for none
        std::string key;
        double half_step;
        bool angle;
    };
    const std::vector<Field> fields{
        {"SPD", "KT", "kcas", 0.5, false},   {"ALT", "FT", "alt_ft", 0.5, false},
        {"HDG", "", "heading", 0.5, true},   {"VS", "FPM", "vs_fpm", 0.5, false},
        {"PITCH", "", "pitch", 0.05, false}, {"BANK", "", "roll", 0.05, false}};
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const Field& f = fields[i];
        const auto words = words_of(lines[i]);
        const std::size_t expected_words = f.unit.empty() ? 2 : 3;
        if (words.size() != expected_words || words[0] != f.label ||
            (!f.unit.empty() && words[2] != f.unit)) {
            fail("line " + std::to_string(i + 1) + " reads \"" + lines[i] + "\", not " +
                 f.label + " and a number" + (f.unit.empty() ? "" : " in " + f.unit));
        }
        const double shown = number(words[1], lines[i]);
        double actual = state.at(f.key);
        double difference = std::abs(shown - actual);
        if (f.angle) {
            difference = std::abs(std::remainder(shown - actual, 360.0));
        }
        std::fprintf(out, "%-6s shown %10.2f, state %10.4f\n", f.label.c_str(), shown,
                    actual);
        if (difference > f.half_step + 1e-6) {
            fail(f.label + " shows " + words[1] + " at tick " + std::to_string(tick) +
                 " when the state is " + std::to_string(actual));
        }
    }

    // The Mach number and the flight level, each where it applies and only
    // there; then nothing, or the autopilot's line.
    std::size_t next = fields.size();
    const auto optional_line = [&](bool applies, const std::string& label, double actual,
                                   double scale, double half_step) {
        const auto words = words_of(lines[next]);
        const bool shown = !words.empty() && words[0] == label;
        if (applies != shown) {
            fail("line " + std::to_string(next + 1) + " reads \"" + lines[next] + "\": " +
                 label + (applies ? " should be shown" : " should not be shown") +
                 " at tick " + std::to_string(tick));
        }
        if (!shown) {
            return;
        }
        if (words.size() != 2) {
            fail("line " + std::to_string(next + 1) + " reads \"" + lines[next] + "\", not " +
                 label + " and a number");
        }
        const double value = number(words[1], lines[next]) * scale;
        std::fprintf(out, "%-6s shown %10.2f, state %10.4f\n", label.c_str(), value, actual);
        if (std::abs(value - actual) > half_step + 1e-6) {
            fail(label + " shows " + words[1] + " at tick " + std::to_string(tick) +
                 " when the state is " + std::to_string(actual));
        }
        ++next;
    };
    optional_line(state.at("mach") >= gfx::hud_mach_from, "MACH", state.at("mach"),
                  1.0, 0.005);
    optional_line(state.at("pa_ft") >= gfx::hud_flight_level_from_ft, "FL",
                  state.at("pa_ft"), 100.0, 50.0);
    // **Who is flying**, always: the pilot, or the AI and what it is doing.
    const bool ai = state.at("ai") >= 0.5;
    if (ai ? lines[next].rfind("FLYING AI", 0) != 0 : lines[next] != "FLYING PILOT") {
        fail("line " + std::to_string(next + 1) + " reads \"" + lines[next] + "\", when " +
             (ai ? "the AI" : "the pilot") + " is flying at tick " + std::to_string(tick));
    }
    std::fprintf(out, "%s\n", lines[next].c_str());
    const std::string flying = lines[next];
    ++next;

    // **And the controls**, each where the flight model has it, to the
    // hundredth the HUD shows.
    const auto control = [&](const std::string& label, std::size_t word, const char* key) {
        const auto words = words_of(lines[next]);
        if (words.empty() || words[0] != label || words.size() <= word) {
            fail("line " + std::to_string(next + 1) + " reads \"" + lines[next] + "\", not " +
                 label);
        }
        const double shown = number(words[word], lines[next]);
        const double actual = state.at(key);
        std::fprintf(out, "%-8s %s shown %+.2f, state %+.4f\n", label.c_str(), key, shown, actual);
        if (std::abs(shown - actual) > 0.005 + 1e-6) {
            fail(label + " shows " + words[word] + " for " + key + " at tick " +
                 std::to_string(tick) + " when the state is " + std::to_string(actual));
        }
    };
    control("STICK", 1, "aileron");
    control("STICK", 2, "elevator");
    ++next;
    control("RUDDER", 1, "rudder");
    ++next;
    control("THROTTLE", 1, "throttle");
    ++next;
    control("FLAPS", 1, "flaps");
    ++next;
    if (state.at("gear") >= 0.0) {
        const std::string want = state.at("gear") >= 0.5 ? "GEAR DOWN" : "GEAR UP";
        if (lines[next] != want) {
            fail("line " + std::to_string(next + 1) + " reads \"" + lines[next] + "\", not " +
                 want);
        }
        ++next;
    }
    // **Nothing more of the HUD's**: every HUD line begins at its margin, in
    // the first column. What begins further in is the scene behind the text -
    // the horizon line, which crosses these rows below the HUD's own (it is
    // kept out of the HUD's, gfx::hud_text_block) when the flight, flown in
    // the live weather, is banked as it was on 2026-09-25 and read as " ?" -
    // and is not the HUD's to be judged here.
    if (!lines[next].empty() && lines[next][0] != ' ') {
        fail("line " + std::to_string(next + 1) + " reads \"" + lines[next] +
             "\", which the HUD should not show");
    }

    // The credits, and one line more below them, which must be empty.
    const auto expected = gfx::credit_lines(credits, frame.width);
    auto shown = gfx::read_text(
        frame,
        gfx::credit_layout(frame.width, frame.height, expected.size()),
        expected.size() + 1, gfx::credit_columns(frame.width));
    for (std::size_t i = 0; i < shown.size(); ++i) {
        const std::string want = i < expected.size() ? expected[i] : "";
        std::fprintf(out, "credit: %s\n", shown[i].c_str());
        if (shown[i] != want) {
            fail("credit line " + std::to_string(i + 1) + " reads \"" + shown[i] +
                 "\", not \"" + want + "\"");
        }
    }
    return flying;
}

} // namespace glideslope::test
