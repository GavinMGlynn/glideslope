#include "harness.hpp"

#include "sim/fixed_step.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using glideslope::sim::FixedStep;
using glideslope::test::check;
using glideslope::test::fail;

namespace {

// A deterministic stream of uneven chunk lengths, 0 to 50 ms, so that "uneven"
// means the same thing every run.
struct Uneven {
    std::uint64_t state = 0x9E3779B97F4A7C15u;
    std::chrono::nanoseconds next() {
        state = state * 6364136223846793005u + 1442695040888963407u;
        return std::chrono::nanoseconds(
            static_cast<std::int64_t>((state >> 33) % 50'000'001u));
    }
};

// Whole 120 Hz steps in `t`, worked out by the standard library's own exact
// ratio arithmetic rather than by anything FixedStep does.
std::int64_t steps_in(std::chrono::nanoseconds t) {
    using step = std::chrono::duration<std::int64_t, std::ratio<1, 120>>;
    return std::chrono::floor<step>(t).count();
}

// Feeds exactly `total` into a FixedStep in chunks from `chunk`, the last one cut
// short so the total is exact, and checks after every advance - not only at the
// end - that the steps taken so far are the steps in the time fed so far, and
// that the advance returned the difference.
FixedStep feed(const std::string& name, std::chrono::nanoseconds total,
               const std::function<std::chrono::nanoseconds()>& chunk) {
    FixedStep fixed;
    std::chrono::nanoseconds fed{0};
    while (fed < total) {
        std::chrono::nanoseconds c = chunk();
        if (fed + c > total) {
            c = total - fed;
        }
        const std::int64_t before = fixed.steps_taken();
        const std::int64_t returned = fixed.advance(c);
        fed += c;
        if (fixed.steps_taken() != steps_in(fed) ||
            returned != fixed.steps_taken() - before) {
            fail(name + ": after " + std::to_string(fed.count()) + " ns, " +
                 std::to_string(fixed.steps_taken()) + " steps taken, " +
                 std::to_string(steps_in(fed)) + " due");
        }
    }
    return fixed;
}

} // namespace

GLIDESLOPE_TEST(the_fixed_step_counts_steps_from_the_total_time_alone) {
    struct Pattern {
        std::string name;
        std::function<std::chrono::nanoseconds()> chunk;
    };
    auto uneven = std::make_shared<Uneven>();
    const std::vector<Pattern> patterns = {
        {"1 us", [] { return 1us; }},
        {"1 ms", [] { return 1ms; }},
        {"16 ms", [] { return 16ms; }},
        {"33 ms", [] { return 33ms; }},
        {"a 60 Hz frame, rounded to the nanosecond", [] { return 16'666'667ns; }},
        {"a 144 Hz frame, rounded to the nanosecond", [] { return 6'944'444ns; }},
        {"one step exactly less a nanosecond", [] { return 8'333'332ns; }},
        {"100 ms", [] { return 100ms; }},
        {"1 s", [] { return 1s; }},
        {"uneven, 0 to 50 ms", [uneven] { return uneven->next(); }},
        {"all at once", [] { return 10s; }},
    };

    int walked = 0;
    for (const auto& p : patterns) {
        for (const auto total : {1s, 10s}) {
            const FixedStep fixed = feed(p.name, total, p.chunk);
            const std::int64_t want =
                120 * std::chrono::duration_cast<std::chrono::seconds>(total).count();
            if (fixed.steps_taken() != want) {
                fail(p.name + ": took " + std::to_string(fixed.steps_taken()) +
                     " steps in " + std::to_string(total.count()) + " s, expected " +
                     std::to_string(want));
            }
            check(fixed.alpha() == 0.0,
                  p.name + ": whole seconds should leave no fraction");
            ++walked;
        }
    }
    const int expected = static_cast<int>(patterns.size()) * 2;
    check(walked == expected, "walked " + std::to_string(walked) + " of " +
                                  std::to_string(expected) +
                                  " pattern and total pairs");
}

GLIDESLOPE_TEST(the_fixed_step_reports_how_far_it_is_into_the_next_step) {
    FixedStep fixed;
    check(fixed.advance(10s + 4ms) == 1200, "10.004 s is 1200 whole steps");
    // 4 ms is 0.48 of an 8.333 ms step.
    check(fixed.alpha() == 0.48, "4 ms into a step is 0.48 of it");
    // The step is 8,333,333.3 ns long, so 4,333,333 ns more leaves it a third of
    // a nanosecond short, and one nanosecond more completes it.
    check(fixed.advance(4'333'333ns) == 0,
          "4.333333 ms more falls just short of the step");
    check(fixed.advance(1ns) == 1, "and one nanosecond more completes it");
}

GLIDESLOPE_TEST(the_fixed_step_refuses_time_running_backwards) {
    FixedStep fixed;
    try {
        fixed.advance(-1ns);
    } catch (const std::invalid_argument&) {
        check(fixed.steps_taken() == 0 && fixed.elapsed() == 0ns,
              "a refused advance leaves the clock where it was");
        return;
    }
    fail("a negative advance was accepted");
}

GLIDESLOPE_TEST(the_fixed_step_counts_a_century_without_overflowing) {
    FixedStep fixed;
    // 36525 days, which is 3.156e18 ns: inside an int64, but 120 times it is not.
    const std::chrono::nanoseconds century = std::chrono::hours(24) * 36525;
    const std::int64_t steps = fixed.advance(century);
    check(steps == 120LL * 60 * 60 * 24 * 36525,
          "a century is 120 steps per second of it");
    check(fixed.alpha() == 0.0, "a whole number of seconds leaves no fraction");
}
