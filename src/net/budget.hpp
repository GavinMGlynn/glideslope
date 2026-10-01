#pragma once

// **A budget of work a second** - how many initiations a full server reads.
//
// A server with a free slot reads every initiation it is sent, which costs it
// X25519 operations for a datagram anybody can send (`docs/THREATS.md`). A
// full server used to read none: it refused `SERVER_FULL` before any
// asymmetric work. It now reads one far enough to know whether its key is one
// of the players' already in - so a player started again from a new port is
// let back in at once rather than waiting out the old session's `--timeout` -
// and this is what bounds that: at most `per_second` reads a second, with as
// many at once and no more. An initiation past the budget is refused unread,
// as every one was before.
//
// A token bucket: it fills at `per_second` a second up to `per_second`, and
// each read takes one.

#include <algorithm>

namespace glideslope::net {

class Budget {
public:
    explicit Budget(double per_second) : per_second_(per_second), left_(per_second) {}

    // **Takes one, at `now_s`**, if there is one to take. `now_s` is any
    // clock that does not go backwards; one that does is taken as standing
    // still.
    bool take(double now_s) {
        if (now_s > last_s_) {
            left_ = std::min(per_second_, left_ + (now_s - last_s_) * per_second_);
            last_s_ = now_s;
        }
        if (left_ < 1.0) {
            return false;
        }
        left_ -= 1.0;
        return true;
    }

private:
    double per_second_;
    double left_;
    double last_s_ = 0.0;
};

// **How many initiations a full server reads a second.** Each costs it one
// X25519 operation, or two for one that claims a player's key and is not
// theirs: at some tens of microseconds apiece, 32 a second is a few
// milliseconds of a core's second, whatever is sent. Far more than players
// starting again need - there are at most four of them.
inline constexpr double full_server_reads_per_second = 32.0;

} // namespace glideslope::net
