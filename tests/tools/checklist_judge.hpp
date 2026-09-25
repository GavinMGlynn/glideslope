#pragma once

// checklist_judge.hpp - the checklist read back out of a frame and held to the
// lines the flight said it drew: what glideslope_checklist_check judges a shot
// by, and what glideslope_checklist_horizon_check judges frames built on
// purpose by.
//
// judge_checklist reads the block back glyph by glyph (gfx::read_text) from
// where gfx::checklist_layout puts it, one line more than expected, and
// requires every line to be what was expected, in order, with nothing below
// them. The marks are what this is for: the first line is the phase and how
// much of it is done; each line after begins "X " for a ticked item or "- "
// for one still to do, and the count on the first line must be the ticks
// below it. What it read is printed to `out`, unless it is null - a test
// judging thousands of frames prints nothing of each. It returns how many
// items are ticked, and throws ChecklistWrong with the reason if anything is
// not so.

#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace glideslope::test {

struct ChecklistWrong : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline std::size_t judge_checklist(const gfx::Frame& frame,
                                   const std::vector<std::string>& expected, std::FILE* out) {
    const auto fail = [](const std::string& why) { throw ChecklistWrong(why); };
    if (expected.empty()) {
        fail("the flight said it drew no checklist at all");
    }
    const auto layout = gfx::checklist_layout(frame.width, frame.height, expected.size());
    // One line more than expected, which must be empty: a checklist with
    // something under it is a checklist that has drawn more than it said.
    const auto shown = gfx::read_text(frame, layout, expected.size() + 1,
                                      gfx::checklist_columns(frame.width));

    std::size_t ticked = 0;
    for (std::size_t i = 0; i < shown.size(); ++i) {
        const std::string want = i < expected.size() ? expected[i] : "";
        if (out != nullptr) {
            std::fprintf(out, "checklist line %zu: \"%s\"\n", i + 1, shown[i].c_str());
        }
        if (shown[i] != want) {
            fail("line " + std::to_string(i + 1) + " reads \"" + shown[i] + "\", not \"" +
                 want + "\"");
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
    const std::size_t space = slash == std::string::npos ? std::string::npos
                                                         : head.rfind(' ', slash);
    if (slash == std::string::npos || space == std::string::npos) {
        fail("the first line \"" + head + "\" does not say how much is done");
    }
    const std::string said = head.substr(space + 1, slash - space - 1);
    if (said != std::to_string(ticked)) {
        fail("the checklist says " + said + " done but " + std::to_string(ticked) +
             " items are ticked");
    }
    return ticked;
}

} // namespace glideslope::test
