#pragma once

// **What a server behind real time owes, and what it lets slip.**
//
// The server's clock says which step is due; the steps it has not yet flown
// are owed, and an input read now is flown from the step due now
// (net::HeldInputs) - the step it has flown to plus what it owes - never
// sooner. Flown sooner, an input began that much earlier on the server's
// clock than on its client's, the client's clocks' difference (the least of
// two seconds of words, sim::Prediction) fell by it and stayed down while the
// server caught up, and every word in those two seconds was placed that far
// off: a server stopped alone 3 s put its client 104 m off, 329 steps
// (docs/status, 2026-10-28). It used to be flown at most a quarter of a
// second on, whatever was owed.
//
// **Owed is caught up when the server can catch it up, and let slip when it
// cannot.** A server held up once - a runner that stopped it for seconds -
// and fast enough afterwards flies every step it owes, and its clients see
// the same clock throughout. One persistently behind - a debug server on a
// loaded runner, `--test-step-ms` - never catches up, and what it owes would
// grow without end, its inputs flown ever later. So once it owes more than
// `most_steps_held`, what it owes past that slips - its clock falls behind the
// wall by it, for good - unless, flying flat out, it has been taking steps
// faster than they come due: then it is catching up, and is let. A client
// sees a slip as the clocks' difference falling by it, which the least of its
// words takes at once, and its pace follows (sim::Pacing); a slip is the
// server's aircraft not having flown those steps, so it costs a correction of
// that much flying, where catching up costs none.
//
// **Never past `most_steps_caught_up`**: a client keeps four seconds of
// inputs to fly again (sim::most_unacknowledged), and a word about a step
// further back than that cannot be placed. A stall longer than that slips
// what is over.
//
// Pure arithmetic on the time it is handed, so a unit test can hold a server
// up as it likes.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace glideslope::net {

// A quarter of a second of steps at 120 a second: owed up to this is always
// caught up.
inline constexpr std::int64_t most_steps_held = 30;
// Three and a half seconds: half a second inside the four a client keeps,
// for the round trip and its jitter.
inline constexpr std::int64_t most_steps_caught_up = 420;

class CatchingUp {
public:
    // **How long the server's pace flying flat out is remembered**, in
    // seconds of its clock: long enough that a pass held up by a busy machine
    // does not decide it, short enough that a server gone slow is let slip
    // within a second or two.
    static constexpr double remembered_s = 1.0;
    // **How much of that it must have seen** before it is believed. Until
    // then a server is taken to be able to catch up.
    static constexpr double known_after_s = 0.25;
    // **A pass longer than this is a hold-up, not a pace** - the gap it ends
    // says nothing of how fast the server flies when it flies - **unless the
    // pass before was as long**: a server whose every pass is that slow is
    // that slow (four steps in 400 ms, the unit test, caught up nothing and
    // owed the most caught up, 3.5 s, until it was).
    static constexpr double hold_up_s = 0.25;

    // **A pass begins**: `due` steps more have come due over `gap_s`, on the
    // server's clock, since the last began, and the last took `taken` steps.
    // Returns the steps it slipped, if any.
    std::int64_t came_due(std::int64_t due, double gap_s, std::int64_t taken,
                          double steps_per_second) {
        // The pass before flew flat out if it left steps owed: its gap is
        // how long `taken` steps took, network and all.
        // **Forgotten on every pass**, flat out or not, so that a pace long
        // past says nothing: remembered over flat-out passes alone, 2.7 s
        // behind and then five minutes keeping time still slipped a stall of
        // 3 s, 330 steps (the unit test; the review of #256). A pace
        // forgotten is taken as able to catch up.
        const bool long_pass = gap_s >= hold_up_s;
        if (gap_s > 0.0) {
            const double keep = std::exp(-gap_s / remembered_s);
            flat_s_ *= keep;
            flat_steps_ *= keep;
            if (owed_ > 0 && (!long_pass || long_pass_before_)) {
                flat_s_ += gap_s;
                flat_steps_ += static_cast<double>(taken);
            }
        }
        long_pass_before_ = long_pass;
        owed_ += due;
        std::int64_t slip = 0;
        if (owed_ > most_steps_held && !catching_up(steps_per_second)) {
            slip = owed_ - most_steps_held;
        }
        slip = std::max(slip, owed_ - most_steps_caught_up);
        if (slip > 0) {
            owed_ -= slip;
            slipped_ += slip;
        }
        return slip;
    }

    // `n` of the steps owed taken.
    void took(std::int64_t n) { owed_ -= n; }

    // **Whether the server, flying flat out, has been taking steps faster
    // than they come due** - or has not flown flat out long enough to say.
    bool catching_up(double steps_per_second) const {
        if (flat_s_ < known_after_s) {
            return true;
        }
        return flat_steps_ > flat_s_ * steps_per_second;
    }

    std::int64_t owed() const { return owed_; }
    std::int64_t slipped() const { return slipped_; }
    // **Let go of everything owed and everything slipped**: a run that stops
    // where it is. A server's end takes what it owes and what it let slip, so
    // that it flies the time it was given; one stopped on a count of steps
    // (`--steps-after-hand-over`) is past its count at either. Slipped kept,
    // a Windows debug server stopped at 14,400 steps with the AI flew 7 more
    // at its end, and its handed aircraft was measured over 14,407 (CI,
    // 2026-10-28).
    void forgive() {
        owed_ = 0;
        slipped_ = 0;
    }

private:
    std::int64_t owed_ = 0;
    std::int64_t slipped_ = 0;
    double flat_s_ = 0.0;
    double flat_steps_ = 0.0;
    bool long_pass_before_ = false;
};

} // namespace glideslope::net
