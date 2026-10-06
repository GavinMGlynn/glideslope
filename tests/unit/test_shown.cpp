#include "harness.hpp"

#include "frontend/shown.hpp"
#include "sim/prediction.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

using glideslope::frontend::OwnShown;
using glideslope::test::check;
using glideslope::test::fail;

namespace {

// **Corrections built large on purpose**, where the network gives them only
// by chance: on the loopback the largest was 1.1 m, and with the blend taken
// out the hand-over test did not fail at all. Here the prediction is flown
// straight and level at 60 m/s, a frame at a time, and at one frame it is put
// right - moved by a correction of a chosen size and direction, flagged as
// the client flags one small enough to hide - and then flown on.

// The bound the tests on a server hold what is shown to away from a switch
// (tests/cmake/client_hands_over.cmake, server_take_over.cmake).
constexpr double bound_m = 2.5;
constexpr double speed_mps = 60.0;

// Twenty sizes, a metre apart from a metre, the last just under too large to
// hide.
constexpr std::array<double, 20> sizes_m{1.0,  2.0,  3.0,  4.0,  5.0,  6.0,  7.0,
                                         8.0,  9.0,  10.0, 11.0, 12.0, 13.0, 14.0,
                                         15.0, 16.0, 17.0, 18.0, 19.0, 19.99};
static_assert(sizes_m.back() < glideslope::sim::snap_beyond_m);
// Both ways along each of the Earth-centred axes. None lies along the
// velocity, (0.6, 0.8, 0) of 60 m/s: they are not ahead, behind or across.
// Nor need they be - the step a correction makes, blended or not, scales with
// its size and the frame's length and not with its direction, so the
// directions add no coverage; they are kept to show as much.
constexpr std::array<std::array<double, 3>, 6> directions{{
    {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
    {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0},
}};
// Frames from a fast screen's to a slow machine's quarter of a second.
constexpr std::array<double, 5> frames_s{1.0 / 144.0, 1.0 / 60.0, 1.0 / 30.0, 0.1, 0.25};
// One correction alone, or that and a second putting it back 50 ms on (the
// server's update rate) or, at frames longer than that, on the next frame -
// taken up mid-way through the first's blend, or at 250 ms frames after it.
constexpr std::array<int, 2> corrections_in_a_row{1, 2};

struct Case {
    double worst_m = 0.0;
    double moved_m = 0.0; // how far the corrections moved the prediction
};

Case fly(double size_m, const std::array<double, 3>& towards, double frame_s, int corrections) {
    OwnShown shown;
    std::array<double, 3> at{-4648676.0, 2546609.0, -3538006.0};
    const std::array<double, 3> v{speed_mps * 0.6, speed_mps * 0.8, 0.0};
    const double correct_at_s = 2.0;
    Case out;
    int made = 0;
    double next_correction_s = correct_at_s;
    for (double t = 0.0; t < 4.0; t += frame_s) {
        for (std::size_t i = 0; i < 3; ++i) {
            at[i] += v[i] * frame_s;
        }
        bool corrected = false;
        if (made < corrections && t >= next_correction_s) {
            const double sign = made == 0 ? 1.0 : -1.0;
            for (std::size_t i = 0; i < 3; ++i) {
                at[i] += sign * towards[i] * size_m;
            }
            out.moved_m += size_m;
            corrected = true;
            ++made;
            next_correction_s = t + 0.05;
        }
        shown.frame(t, {glideslope::world::Ecef{at[0], at[1], at[2]}, v, true, corrected});
    }
    check(made == corrections, "every correction was made");
    check(shown.switches() == 0, "nothing here is a switch");
    out.worst_m = shown.worst_step_otherwise_m();
    return out;
}

} // namespace

// **Every correction small enough to hide is taken up without a step**: every
// size to just under sim::snap_beyond_m, both ways along each axis, at every
// frame rate, alone and put back by the next. Shown as they come, each would step its whole
// size; blended, what is shown moves by a fifteenth of it a sixtieth of a
// second - 1.3 m at the most.
GLIDESLOPE_TEST(every_correction_small_enough_to_hide_is_taken_up_without_a_step) {
    const std::size_t space =
        sizes_m.size() * directions.size() * frames_s.size() * corrections_in_a_row.size();
    std::size_t covered = 0;
    // The rule is tested only by a case whose correction, shown as it came,
    // would pass the bound. **Counted by arithmetic, not observed**: at a
    // constant velocity the unblended step is the correction itself, so it is
    // every case whose size is above the bound. (The blend was taken out once
    // by hand and this test then failed at 19.99 m; PROJECT_STATUS.md.)
    std::size_t testing = 0;
    double worst = 0.0;
    std::string worst_what;
    for (const double size : sizes_m) {
        for (const auto& towards : directions) {
            for (const double frame : frames_s) {
                for (const int n : corrections_in_a_row) {
                    const Case c = fly(size, towards, frame, n);
                    ++covered;
                    check(std::abs(c.moved_m - size * n) < 1e-9,
                          "the prediction was moved as far as the case says");
                    if (size > bound_m) {
                        ++testing;
                    }
                    if (c.worst_m > worst) {
                        worst = c.worst_m;
                        char what[160];
                        std::snprintf(what, sizeof what,
                                      "%d correction(s) of %.2f m along (%+.0f %+.0f %+.0f) at "
                                      "%.0f ms frames",
                                      n, size, towards[0], towards[1], towards[2],
                                      frame * 1000.0);
                        worst_what = what;
                    }
                }
            }
        }
    }
    std::printf("%zu of %zu cases, %zu of them past %.1f m unblended; the largest step "
                "%.3f m, at %s\n",
                covered, space, testing, bound_m, worst, worst_what.c_str());
    check(covered == space, "every case was flown");
    check(testing == (sizes_m.size() - 2) * directions.size() * frames_s.size() *
                         corrections_in_a_row.size(),
          "every size above the bound tests the rule");
    if (worst >= bound_m) {
        fail("what was shown stepped " + std::to_string(worst) + " m at a correction, the bound " +
             std::to_string(bound_m) + " m: " + worst_what);
    }
}
