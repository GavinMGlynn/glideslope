#pragma once

// **The words compared after a server's slip, counted apart from the rest** -
// for glideslope_cli, whose network tests hold the prediction to its bounds.
//
// A server that cannot catch up what it owes lets it slip (net::CatchingUp):
// its clock falls behind the wall for good, and the clocks' difference moves
// by the steps it let go. The figure a word is placed by is held once
// settled, lowered only when `sim::offset_lowered_after` of the window's
// words are below it (sim::ClockDifference, the owner's choice, 2026-11-22),
// and this client's pace (sim::Pacing) steers its clock back meanwhile. So
// after a slip words are placed whole steps from where they fall on the path
// flown, until the figure and the pace have followed, and the words of the
// round trip after each move of the figure are placed off again. That is the
// held figure's accepted cost: 6.7 and 17.4 m in two of ten loaded runs of
// the STOVL selector test, words 5 and 21 steps off (docs/status,
// 2026-11-22).
//
// **A slip is seen** in a word placed `from_steps` or more steps from the
// step nearest it on the path flown - off when, not off where. One step is
// what a held figure leaves now and then on a busy machine, and is not a
// slip. A word nearest an end of the steps kept was placed at least as many
// steps off as it says, so it sees one too. **The words after a slip** are
// that word and the `words_after` after it - the figure's window and the
// pace's, as a machine's stall's are (frontend::MachineStalls) - begun again
// by each word that sees one. The rest's worst is the prediction's own.
//
// Presentation-free: arithmetic on what each word says.

#include "sim/prediction.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace glideslope::frontend {

class AfterASlip {
public:
    // Two steps or more off is a slip; one is the held figure's noise.
    static constexpr std::int64_t from_steps = 2;
    static constexpr std::size_t words_after = sim::offset_window + sim::pace_window;

    // A word compared: how many steps from the step nearest it on the path it
    // was placed, and its error.
    void compared(std::int64_t steps_off, double error_m) {
        if (steps_off >= from_steps) {
            if (left_ == 0) {
                ++slips_;
                window_ = 0;
            }
            left_ = words_after + 1;
            most_off_ = std::max(most_off_, steps_off);
        }
        if (left_ == 0) {
            ++rest_;
            worst_rest_m_ = std::max(worst_rest_m_, error_m);
            return;
        }
        --left_;
        ++after_;
        ++window_;
        longest_ = std::max(longest_, window_);
        worst_after_m_ = std::max(worst_after_m_, error_m);
    }

    std::size_t slips() const { return slips_; }
    std::size_t after() const { return after_; }
    std::size_t rest() const { return rest_; }
    std::size_t longest() const { return longest_; }
    std::int64_t most_off() const { return most_off_; }
    double worst_after_m() const { return worst_after_m_; }
    double worst_rest_m() const { return worst_rest_m_; }

    std::string said() const {
        char line[512];
        std::snprintf(line, sizeof line,
                      "after a slip: %zu seen, %zu updates compared after them, at most %zu "
                      "after one, placed at most %lld steps off, the worst %.3f m; the rest: "
                      "%zu updates, the worst %.3f m",
                      slips_, after_, longest_, static_cast<long long>(most_off_),
                      worst_after_m_, rest_, worst_rest_m_);
        return line;
    }

private:
    std::size_t slips_ = 0;
    std::size_t after_ = 0;
    std::size_t rest_ = 0;
    std::size_t left_ = 0;
    std::size_t window_ = 0;
    std::size_t longest_ = 0;
    std::int64_t most_off_ = 0;
    double worst_after_m_ = 0.0;
    double worst_rest_m_ = 0.0;
};

} // namespace glideslope::frontend
