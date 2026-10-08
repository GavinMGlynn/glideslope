#pragma once

// The inputs a server has heard from a client and not yet flown.
//
// **An input is flown from the step that was due when it was read**, not
// the step the server had flown to. A server held up owes steps; the time
// they stand for had passed before the input arrived, so flying it from the
// step it had got to flies it early by the steps owed. The client reckons
// the server's clock from when each input began there, and takes the least
// of that over two seconds of updates (sim::Prediction), so inputs flown
// early lowered it by the steps owed for those two seconds, and the server's
// word was placed that far off: 14.9 m on macOS debug (run 37759639784), and
// a server paused 200 ms by hand - 24 steps - put the client off by 4.2 m,
// 0.43 m with the rule here (PROJECT_STATUS.md, 2026-10-08).
//
// **At most `most_steps_held` after the step being flown**, so that a server
// that never catches up still flies what it is sent, that long after it came.

#include "net/inputs.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace glideslope::net {

// A quarter of a second of steps at 120 a second: above the 24 steps of the
// 200 ms stall measured, and of the 23 the macOS run's clocks' difference
// moved by; a longer stall is flown early by what is over.
inline constexpr std::int64_t most_steps_held = 30;

class HeldInputs {
public:
    struct Held {
        std::uint32_t sequence = 0;
        ControlList controls{};
        // The step it is flown from; -1 until stamped.
        std::int64_t due = -1;
    };

    // The newest input's number, if any is held.
    std::optional<std::uint32_t> newest() const {
        if (held_.empty()) {
            return std::nullopt;
        }
        return held_.back().sequence;
    }
    bool empty() const { return held_.empty(); }
    std::size_t size() const { return held_.size(); }

    // An input read, newer than any held; past `most` waiting the oldest is
    // let go - a flood.
    void heard(std::uint32_t sequence, const ControlList& controls, std::size_t most) {
        held_.push_back({sequence, controls, -1});
        while (held_.size() > most) {
            held_.pop_front();
        }
    }

    // **The inputs read in this pass, stamped** with the step due now: the
    // server has flown `stepped` and owes `owed`.
    void stamp(std::int64_t stepped, std::int64_t owed) {
        for (Held& h : held_) {
            if (h.due < 0) {
                h.due = stepped + std::min(owed, most_steps_held);
            }
        }
    }

    // **What is flown from step `step`**: the newest input due by then, the
    // older ones it overtook let go.
    std::optional<Held> due_by(std::int64_t step) {
        std::optional<Held> due;
        while (!held_.empty() && held_.front().due >= 0 && held_.front().due <= step) {
            due = held_.front();
            held_.pop_front();
        }
        return due;
    }

    // **The newest, now, whenever it was due**, and every other let go: at a
    // switch, so that what the client sent is flown by the aircraft it sent
    // it for. One held for steps the server owes is flown that much early -
    // a switch is a moment of its own, and the client predicting starts
    // again from it.
    std::optional<Held> take_newest() {
        if (held_.empty()) {
            return std::nullopt;
        }
        Held newest = held_.back();
        held_.clear();
        return newest;
    }

private:
    std::deque<Held> held_;
};

} // namespace glideslope::net
