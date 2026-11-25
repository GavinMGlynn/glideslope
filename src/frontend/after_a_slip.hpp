#pragma once

// **The words compared after a server's slip, counted apart from the rest** -
// for glideslope_cli, whose network tests hold the prediction to its bounds.
//
// A server that cannot catch up what it owes lets it slip (net::CatchingUp):
// its clock falls behind the wall for good, and the words' own differences -
// where the server flew an input less where this client began it - move by
// the steps it let go. The clocks' difference a word is placed by is held
// once settled, and lowered only when `sim::offset_lowered_after` of the
// window's words are below it (sim::ClockDifference, the owner's choice,
// 2026-11-22). So after a slip, words are placed by a figure that stands
// above their own difference until ten slipped words have come, and the
// figure and the pace (sim::Pacing) then move to the new difference a step
// at a time, each move putting the words of the round trip after it a step
// or more off. That is the held figure's accepted cost: 6.7 and 17.4 m in two
// of ten loaded runs of the STOVL selector test (docs/status, 2026-11-22).
//
// **A slip is seen** in a word compared against a step flown under a
// difference `from_steps` or more above the word's own. One step above is
// the figure held against a sample now and then low, which is what holding it
// is for, and is not a slip. **The words after a slip** are that word and the
// `words_after` after it - the figure's window and the pace's, as a machine's
// stall's are (frontend::MachineStalls) - begun again at each word that sees
// one. The rest's worst is then the prediction's own.
//
// **What a word after a slip may be off by** is the steps it was placed off,
// and no more than the rest: its error beyond the travel, at her speed then,
// of the steps its difference stood above its own (`worst_beyond_m`). A word
// placed where it was sent is held as the rest are.
//
// Presentation-free: arithmetic on what each word says.

#include "sim/fixed_step.hpp"
#include "sim/prediction.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace glideslope::frontend {

class AfterASlip {
public:
    // Two steps or more above the word's own difference is a slip; one is the
    // held figure's noise.
    static constexpr std::int64_t from_steps = 2;
    static constexpr std::size_t words_after = sim::offset_window + sim::pace_window;

    // A word compared: the difference its step was flown under less its own,
    // its error, and her speed at the step it was compared against.
    void compared(std::int64_t above, double error_m, double speed_mps) {
        if (above >= from_steps) {
            if (left_ == 0) {
                ++slips_;
                window_ = 0;
            }
            left_ = words_after + 1;
            most_above_ = std::max(most_above_, above);
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
        const double placed_off_m = static_cast<double>(std::max<std::int64_t>(above, 0)) *
                                    speed_mps / static_cast<double>(sim::steps_per_second);
        worst_beyond_m_ = std::max(worst_beyond_m_, error_m - placed_off_m);
    }

    std::size_t slips() const { return slips_; }
    std::size_t after() const { return after_; }
    std::size_t rest() const { return rest_; }
    std::size_t longest() const { return longest_; }
    std::int64_t most_above() const { return most_above_; }
    double worst_after_m() const { return worst_after_m_; }
    double worst_beyond_m() const { return worst_beyond_m_; }
    double worst_rest_m() const { return worst_rest_m_; }

    std::string said() const {
        char line[512];
        std::snprintf(line, sizeof line,
                      "after a slip: %zu seen, %zu updates compared after them, at most %zu "
                      "after one, placed at most %lld steps above their own, the worst %.3f m, "
                      "the worst beyond the steps above %.3f m; the rest: %zu updates, the "
                      "worst %.3f m",
                      slips_, after_, longest_, static_cast<long long>(most_above_),
                      worst_after_m_, worst_beyond_m_, rest_, worst_rest_m_);
        return line;
    }

private:
    std::size_t slips_ = 0;
    std::size_t after_ = 0;
    std::size_t rest_ = 0;
    std::size_t left_ = 0;
    std::size_t window_ = 0;
    std::size_t longest_ = 0;
    std::int64_t most_above_ = 0;
    double worst_after_m_ = 0.0;
    double worst_beyond_m_ = 0.0;
    double worst_rest_m_ = 0.0;
};

} // namespace glideslope::frontend
