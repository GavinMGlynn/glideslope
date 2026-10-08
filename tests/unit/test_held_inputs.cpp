#include "harness.hpp"

#include "net/held_inputs.hpp"

#include <cstdint>
#include <optional>
#include <string>

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
// a 200 ms stall, the most held (30) and past it (200, held only 30). And of
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
    const Case cases[] = {{0, 100}, {4, 104}, {24, 124}, {30, 130}, {200, 130}};
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
