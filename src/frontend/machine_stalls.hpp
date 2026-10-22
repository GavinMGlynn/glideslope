#pragma once

// **The machine's stalls, and what is counted apart after each** - for both
// clients that predict their own aircraft: the client with the window and
// glideslope_cli, whose network checks hold the prediction to its bounds.
//
// A frame the machine mostly took - the time between it and the frame before
// that was not this client's thread's CPU time, nor a sleep it asked for, nor
// a write it waited on - by `from_s` or more is a machine that did not run
// it, and on a runner it does not run the server or the relay either: CI's
// Windows runners stopped the client 1.0 to 3.5 s between two frames and it
// was put right by 26 to 138 m (2026-10-21). Stopped by hand together 3 s
// here, client and server, it was put 7 m off and then 4.6 m (docs/status,
// 2026-10-22). The server then catches up the steps it owes, sending a word
// every 4.8 of them, and reads the inputs that came meanwhile all at once,
// flying them at most a quarter of a second late (net::most_steps_held): a
// word placed by them puts the clocks' difference - the least of
// `sim::offset_window` words - steps low until it has left the window, and
// the pace's `sim::pace_window` after it.
//
// So the words about a time before this client's reckoning of the session's
// clock at the frame after the stall - the server's catching up - and
// `words_after` words after them have their updates and corrections counted
// apart, each stall's own, and named: how many, the worst, and from which of
// the words after it the error was back within `back_within_m`. Every word
// after them is judged with the rest, so a recovery that never comes still
// fails; a median or percentile taken is of all of them. A stall shorter than
// a quarter of a second the server flies where it came; one that is this
// client's own time is not the machine's, and is not counted apart.
//
// Presentation-free: arithmetic on times, for both clients.

#include "net/held_inputs.hpp"
#include "sim/fixed_step.hpp"
#include "sim/prediction.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::frontend {

class MachineStalls {
public:
    static constexpr double from_s = static_cast<double>(net::most_steps_held) /
                                     static_cast<double>(sim::steps_per_second);
    static constexpr std::size_t words_after = sim::offset_window + sim::pace_window;
    // The bound on all but five updates in a hundred
    // (server_paced_prediction.cmake).
    static constexpr double back_within_m = 1.0;

    struct Stall {
        double at_s = 0.0;
        double machines_s = 0.0;
        // The session's time as this client reckoned it at the frame after
        // the stall: words about an earlier time are the server's catching up.
        double caught_up_s = 0.0;
        std::size_t catching_up = 0;
        // Words after those, up to `words_after`.
        std::size_t words = 0;
        std::size_t compared = 0;
        double worst_error_m = 0.0;
        // The last of the words after whose update was off by more than
        // `back_within_m`; 0 if none.
        std::size_t over_until_word = 0;
        std::size_t corrections = 0;
        double worst_correction_m = 0.0;
        std::size_t snapped = 0;
    };

    // **A frame**, at `local_s`: `between_s` since the last, `own_s` of it
    // this client's, and the session's time as it reckons it now, if known.
    void frame(double local_s, double between_s, double own_s, std::optional<double> session_s) {
        const double machines = between_s - own_s;
        if (session_s && machines >= from_s && machines > 0.5 * between_s) {
            stalls_.push_back({local_s, machines, *session_s});
        }
    }

    // **A word on its own aircraft**, about `about_s` on the session's
    // clock: the stall it is across, counted, or null.
    Stall* word(double about_s) {
        if (stalls_.empty()) {
            return nullptr;
        }
        Stall& s = stalls_.back();
        if (about_s < s.caught_up_s) {
            ++s.catching_up;
            return &s;
        }
        if (s.words < words_after) {
            ++s.words;
            return &s;
        }
        return nullptr;
    }

    static void compared(Stall& s, double error_m) {
        ++s.compared;
        s.worst_error_m = std::max(s.worst_error_m, error_m);
        if (error_m > back_within_m) {
            s.over_until_word = s.words;
        }
    }

    static void corrected(Stall& s, double moved_m, bool snapped) {
        ++s.corrections;
        s.worst_correction_m = std::max(s.worst_correction_m, moved_m);
        s.snapped += snapped ? 1 : 0;
    }

    const std::vector<Stall>& stalls() const { return stalls_; }

    // What it says: one line in all, and one for each stall.
    std::vector<std::string> said() const {
        std::vector<std::string> lines;
        std::size_t apart = 0, corrections = 0;
        for (const Stall& s : stalls_) {
            apart += s.compared;
            corrections += s.corrections;
        }
        char line[384];
        std::snprintf(line, sizeof line,
                      "the machine's stalls: %zu frames of %.0f ms and more the machine's, "
                      "%zu updates compared and %zu corrections in the server's catching up "
                      "and the %zu words after it, counted apart",
                      stalls_.size(), from_s * 1000.0, apart, corrections, words_after);
        lines.emplace_back(line);
        for (std::size_t i = 0; i < stalls_.size(); ++i) {
            const Stall& s = stalls_[i];
            std::snprintf(line, sizeof line,
                          "the machine's stall %zu: %.0f ms, %.1f s in; %zu words catching up "
                          "and %zu after, %zu updates compared, the worst %.3f m, back within "
                          "%.0f m from word %zu after; %zu corrections, the worst %.3f m, %zu "
                          "too large to hide",
                          i + 1, s.machines_s * 1000.0, s.at_s, s.catching_up, s.words,
                          s.compared, s.worst_error_m, back_within_m, s.over_until_word + 1,
                          s.corrections, s.worst_correction_m, s.snapped);
            lines.emplace_back(line);
        }
        return lines;
    }

private:
    std::vector<Stall> stalls_;
};

} // namespace glideslope::frontend
