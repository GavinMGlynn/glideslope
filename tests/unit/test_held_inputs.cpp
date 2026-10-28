#include "harness.hpp"

#include "net/catching_up.hpp"
#include "net/held_inputs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

using glideslope::net::CatchingUp;
using glideslope::net::ControlList;
using glideslope::net::HeldInputs;
using glideslope::test::check;

namespace {

ControlList with_throttle(double t) {
    ControlList c{};
    c[0] = t;
    return c;
}

// The first step from `from` on which an input is flown, if one is within
// `steps`.
std::optional<std::int64_t> flown_from(HeldInputs& held, std::int64_t from, std::int64_t steps,
                                       std::uint32_t& which) {
    for (std::int64_t s = from; s < from + steps; ++s) {
        if (const auto due = held.due_by(s)) {
            which = due->sequence;
            return s;
        }
    }
    return std::nullopt;
}

} // namespace

// **An input heard while the server owes steps is flown from the step due
// when it came**, not the step the server had flown to (net::HeldInputs).
// Every case of what is owed when it is read: none, a pass's worth (4, which
// the old rule - flown once the pass's steps were - got right too), the 24 of
// a 200 ms stall, a quarter of a second (30) and past it (200 - flown from
// 300, never sooner: held at most 30 on, as it was, it was flown 170 steps
// early, and a server stopped 3 s put its client 104 m off). And of
// two read in passes apart, each from its own due step; of two read in one
// pass, the newer alone, the older let go; and at a switch the newest flown
// at once and the rest let go. Seen to fail with the old rule (due at
// `stepped + min(owed, 4)`): the 24 steps owed flown from 104, not 124.
GLIDESLOPE_TEST(an_input_heard_while_the_server_owes_steps_is_flown_from_the_step_due_when_it_came) {
    const std::int64_t stepped = 100;
    struct Case {
        std::int64_t owed;
        std::int64_t from;
    };
    const Case cases[] = {{0, 100}, {4, 104}, {24, 124}, {30, 130}, {200, 300}};
    std::size_t tried = 0;
    for (const Case& k : cases) {
        HeldInputs held;
        held.heard(7, with_throttle(0.5), 120);
        held.stamp(stepped, k.owed);
        std::uint32_t which = 0;
        const auto from = flown_from(held, stepped, 300, which);
        check(from == k.from && which == 7,
              "owing " + std::to_string(k.owed) + " steps, flown from " +
                  (from ? std::to_string(*from) : std::string("never")) + ", not " +
                  std::to_string(k.from));
        check(held.empty(), "nothing left held once flown");
        ++tried;
    }
    check(tried == 5, "five amounts owed: " + std::to_string(tried));

    // Two read in passes apart, owing 24 and then, four steps on, 30: each
    // from its own due step, 124 and 134.
    {
        HeldInputs held;
        held.heard(1, with_throttle(0.1), 120);
        held.stamp(100, 24);
        held.heard(2, with_throttle(0.2), 120);
        held.stamp(104, 30);
        held.stamp(108, 30); // stamped once only
        std::uint32_t which = 0;
        check(flown_from(held, 100, 300, which) == 124 && which == 1, "the first from 124");
        check(flown_from(held, 125, 300, which) == 134 && which == 2, "the second from 134");
    }
    // Two read in one pass: the newer alone, from their due step.
    {
        HeldInputs held;
        held.heard(1, with_throttle(0.1), 120);
        held.heard(2, with_throttle(0.2), 120);
        held.stamp(100, 24);
        std::uint32_t which = 0;
        check(flown_from(held, 100, 300, which) == 124 && which == 2,
              "of two read together, the newer is flown");
        check(held.empty(), "and the older let go");
    }
    // Not yet stamped: never due.
    {
        HeldInputs held;
        held.heard(1, with_throttle(0.1), 120);
        std::uint32_t which = 0;
        check(!flown_from(held, 0, 1000, which), "an input not yet stamped is not flown");
        // At a switch, the newest at once.
        held.heard(2, with_throttle(0.2), 120);
        const auto now = held.take_newest();
        check(now && now->sequence == 2 && held.empty(), "a switch flies the newest at once");
    }
    // A flood: past the most held, the oldest let go.
    {
        HeldInputs held;
        for (std::uint32_t n = 1; n <= 130; ++n) {
            held.heard(n, with_throttle(0.1), 120);
        }
        check(held.size() == 120 && held.newest() == 130u, "a flood keeps the newest 120");
    }
}

namespace {

// **A server flown pass by pass**: each pass `gap_s` long on its clock, at
// most four steps a pass as glideslope_server takes, for `passes` passes;
// what it owes and has let slip at the end. Whole steps come due as the
// clock passes them.
struct Flown {
    std::int64_t owed = 0;
    std::int64_t slipped = 0;
    std::int64_t most_owed = 0;
};
Flown fly_passes(CatchingUp& c, double& clock_s, std::int64_t& due_so_far, double gap_s,
                 int passes, std::int64_t& taken) {
    Flown f;
    for (int i = 0; i < passes; ++i) {
        clock_s += gap_s;
        const auto due_now = static_cast<std::int64_t>(std::floor(clock_s * 120.0 + 1e-6));
        c.came_due(due_now - due_so_far, gap_s, taken, 120.0);
        due_so_far = due_now;
        f.most_owed = std::max(f.most_owed, c.owed());
        taken = std::min<std::int64_t>(c.owed(), 4);
        c.took(taken);
    }
    f.owed = c.owed();
    f.slipped = c.slipped();
    return f;
}

} // namespace

// **A server held up once, and fast enough after, catches up every step it
// owes and lets none slip** (net::CatchingUp), so that its clients' clock is
// the same before and after. Every stall from a quarter of a second to the
// most caught up, 3.5 s - 0.3, 1, 2 and 3.5 s - after a second keeping time
// at 10 ms a pass, and then flown flat out at 4 steps in 20 ms (1.67 of real
// time) for 15 s: nothing slipped, nothing owed at the end. A stall of 5 s
// is past what a client keeps, and slips what is over 3.5 s - 180 steps -
// and catches up the rest. Seen to fail with what is owed past a quarter of
// a second slipped whatever the server's pace: the 0.3 s stall let 6 slip.
GLIDESLOPE_TEST(a_server_held_up_once_and_fast_enough_after_catches_up_every_step_it_owes) {
    struct Case {
        double stall_s;
        std::int64_t slipped;
    };
    const Case cases[] = {{0.3, 0}, {1.0, 0}, {2.0, 0}, {3.5, 0}, {5.0, 180}};
    std::size_t tried = 0;
    for (const Case& k : cases) {
        CatchingUp c;
        double clock_s = 0.0;
        std::int64_t due = 0;
        std::int64_t taken = 0;
        fly_passes(c, clock_s, due, 0.010, 100, taken);
        check(c.owed() <= 4 && c.slipped() == 0, "keeping time owes a pass's steps at most");
        // The stall: one pass of `stall_s`.
        fly_passes(c, clock_s, due, k.stall_s, 1, taken);
        const Flown after = fly_passes(c, clock_s, due, 0.020, 750, taken);
        check(after.slipped == k.slipped && after.owed <= 4,
              "a stall of " + std::to_string(k.stall_s) + " s, flown at 1.67 after: " +
                  std::to_string(after.slipped) + " slipped, not " +
                  std::to_string(k.slipped) + ", and " + std::to_string(after.owed) +
                  " owed at the end");
        ++tried;
    }
    check(tried == 5, "five stalls: " + std::to_string(tried));
}

// **A server persistently behind lets what it owes past a quarter of a second
// slip**, so that an input is flown at most that long after it came and not
// ever later. Every pace from a little behind to far behind - 4 steps a pass
// at 34, 45, 90 and 400 ms (0.98, 0.74, 0.37 and 0.08 of real time) - flown
// 30 s: never more owed than 30 and a pass's steps due, and the rest let slip,
// as many as it fell behind by. And held up 2 s first, then persistently
// behind: the stall is let slip too, once the server is seen not to catch up
// - within two seconds. And every pass held up as long as a stall, 400 ms:
// not a stall but a pace. Seen to fail with the server always taken to be
// catching up: at 0.98 of real time it owed 74 steps, and growing.
GLIDESLOPE_TEST(a_server_persistently_behind_lets_what_it_owes_past_a_quarter_of_a_second_slip) {
    const double gaps[] = {0.034, 0.045, 0.090, 0.400};
    std::size_t tried = 0;
    for (const double gap : gaps) {
        CatchingUp c;
        double clock_s = 0.0;
        std::int64_t due = 0;
        std::int64_t taken = 0;
        const int passes = static_cast<int>(30.0 / gap);
        const Flown f = fly_passes(c, clock_s, due, gap, passes, taken);
        const auto came = static_cast<std::int64_t>(gap * 120.0) + 1;
        const std::int64_t behind = due - static_cast<std::int64_t>(passes) * 4;
        check(f.most_owed <= 30 + came,
              "4 steps a pass of " + std::to_string(gap) + " s: at most " +
                  std::to_string(f.most_owed) + " owed, not over " + std::to_string(30 + came));
        check(f.slipped + f.owed + passes * 4 >= due && f.slipped > 0 &&
                  f.slipped + f.owed + passes * 4 <= due + 4,
              "and the rest slipped: " + std::to_string(f.slipped) + " of " +
                  std::to_string(behind) + " behind");
        ++tried;
    }
    check(tried == 4, "four paces: " + std::to_string(tried));
    // Held up 2 s, then behind at 0.74.
    {
        CatchingUp c;
        double clock_s = 0.0;
        std::int64_t due = 0;
        std::int64_t taken = 0;
        fly_passes(c, clock_s, due, 0.010, 100, taken);
        fly_passes(c, clock_s, due, 2.0, 1, taken);
        const Flown f = fly_passes(c, clock_s, due, 0.045, 44, taken);
        check(f.owed <= 30 + 6 && f.slipped >= 200,
              "a stall then behind is let slip within two seconds: " +
                  std::to_string(f.owed) + " owed, " + std::to_string(f.slipped) +
                  " slipped");
    }
}
