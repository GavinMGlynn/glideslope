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
// **What is owed is bounded by the server, not here** (net::CatchingUp): a
// server that cannot catch up lets what it owes past a quarter of a second
// slip, so that its inputs are flown at most that long after they came; one
// that can catches up, and an input is flown from the step due, however much
// is owed. Held at most a quarter of a second on whatever was owed, as it
// was, an input read after a stall was flown early by the rest, and a server
// stopped alone 3 s put its client 104 m off (docs/status, 2026-10-28).

#include "net/catching_up.hpp"
#include "net/inputs.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace glideslope::net {

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
    // server has flown `stepped` and owes `owed` - never sooner.
    void stamp(std::int64_t stepped, std::int64_t owed) {
        for (Held& h : held_) {
            if (h.due < 0) {
                h.due = stepped + owed;
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
