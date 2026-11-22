#pragma once

// **The clocks' difference a predicting client places the server's words
// by**, in steps, from each word's sample (sim::Prediction::hear_clock).
//
// **It moves down only when many words say so, and up as the least does.**
// Each sample is where the server flew an input less where this client sent
// it: the clocks' difference plus how late the network and the server's
// passes made that input. Taken as the least of the last `offset_window`
// samples, as it was until 2026-11-22, a busy machine moved it: a server
// held up unevenly reads most inputs a step or two late and now and then one
// at once, about one sample in a hundred a step below the rest. Each such
// sample lowered the least by a step for the 50 words it was in the window
// and raised it again as it left; and every change put the words of the
// round trip after it a step off - six words, 1.4 m each at the F-35B's
// 330 kt, compared against steps flown on the figure before. Beside twice as
// many busy loops as cores that was 18 to 32 changes in 3,000 words and the
// 95th percentile 0.87 m in six runs of ten (docs/status, 2026-11-22).
//
// So, once settled, the figure is held: it goes down only when at least
// `offset_lowered_after` of the window's samples are below it, to the
// `offset_lowered_after`-th least of them, and up whenever the least of the
// window is above it, to that least. A sample now and then a step low moves
// it no more. **The cost, named**: a server that really slips - its clock
// put back by the steps it let go - is followed `offset_lowered_after` words
// late, not at the first, each of those words placed that many steps off.
// This is NTP's clock filter's choice made the other way: it trusts the
// least-delayed sample (Mills, "Clock Filter Algorithm"), which is right
// where the least delay is common and wrong here, where it is rare.
//
// Before it is settled (`offset_settled` words) it is the least, as before:
// joining, the least is still coming down as inputs that waited less
// arrive.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace glideslope::sim {

// How many of the server's words the clocks' difference is taken over: two
// seconds of updates at 25 a second, long enough that one input in it came
// through with next to no jitter, short enough to follow a server whose clock
// runs slow.
inline constexpr std::size_t offset_window = 50;
// **How many words before the clocks' difference is taken as known**: a
// second of updates. Before then the least is still coming down as inputs
// that waited less arrive, and each time it does the client is put right by
// the steps it moved - two metres, three times, a second after joining at
// 200 ms (PROJECT_STATUS.md). Those corrections are hidden like any other;
// a prediction error measured before this is the estimate settling, not the
// prediction.
inline constexpr std::size_t offset_settled = 25;
// **How many of the window's samples must be below the clocks' difference
// before it is lowered**: ten of fifty, one in five. Replayed over the
// samples of ten loaded runs, ten left 0 or 1 changes a run where the least
// made 18 to 32; five left up to 7 (docs/status, 2026-11-22).
inline constexpr std::size_t offset_lowered_after = 10;

class ClockDifference {
public:
    void hear(std::int64_t sample) {
        window_.push_back(sample);
        while (window_.size() > offset_window) {
            window_.pop_front();
        }
        const std::int64_t least = *std::min_element(window_.begin(), window_.end());
        if (!held_ || window_.size() < offset_settled || least > *held_) {
            held_ = least;
            return;
        }
        if (below(*held_) >= offset_lowered_after) {
            std::vector<std::int64_t> sorted(window_.begin(), window_.end());
            const auto nth = sorted.begin() + static_cast<std::ptrdiff_t>(offset_lowered_after - 1);
            std::nth_element(sorted.begin(), nth, sorted.end());
            held_ = *nth;
        }
    }
    // The figure, none before a sample.
    std::optional<std::int64_t> value() const { return held_; }
    // How many of the window's samples are below `d`.
    std::size_t below(std::int64_t d) const {
        return static_cast<std::size_t>(
            std::count_if(window_.begin(), window_.end(), [d](std::int64_t v) { return v < d; }));
    }
    // The least of the newest `n` samples - what a pace is steered by.
    std::optional<std::int64_t> recent_least(std::size_t n) const {
        if (window_.empty()) {
            return std::nullopt;
        }
        const auto from = window_.size() > n
                              ? window_.end() - static_cast<std::ptrdiff_t>(n)
                              : window_.begin();
        return *std::min_element(from, window_.end());
    }
    std::size_t size() const { return window_.size(); }
    bool empty() const { return window_.empty(); }

private:
    std::deque<std::int64_t> window_;
    std::optional<std::int64_t> held_;
};

} // namespace glideslope::sim
