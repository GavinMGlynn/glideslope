#include "harness.hpp"

#include "frontend/shown.hpp"
#include "sim/prediction.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

namespace {

// **A switch through long frames**: the aircraft flown straight and level at
// 60 m/s, shown at 60 Hz, and from just before the switch to four frames
// after it every frame a chosen length - the frames a switch's step is
// measured in, as a stalling machine draws them. Drawn from the updates it
// is 100 ms behind where it is predicted, the network checks' own lag, so
// that unblended each switch steps 6 m.
enum class Switch { hand_over, take_back, take_over };
constexpr std::array<Switch, 3> switch_kinds{Switch::hand_over, Switch::take_back,
                                             Switch::take_over};
constexpr std::array<double, 4> long_frames_s{1.0 / 60.0, 0.1, 0.25, 0.4};
constexpr double lag_s = 0.1;

struct Switched {
    double worst_m = 0.0;
    double longest_ms = 0.0;
    std::size_t switches = 0;
    std::string what;
};

Switched switch_through(Switch kind, double long_s) {
    OwnShown shown;
    const std::array<double, 3> v{speed_mps * 0.6, speed_mps * 0.8, 0.0};
    // Its own, and - for a take-over - the AI's it takes, 300 m off.
    const std::array<double, 3> own0{-4648676.0, 2546609.0, -3538006.0};
    const std::array<double, 3> other0{own0[0] + 300.0, own0[1], own0[2]};
    const auto along = [&](const std::array<double, 3>& from, double t) {
        return glideslope::world::Ecef{from[0] + v[0] * t, from[1] + v[1] * t,
                                       from[2] + v[2] * t};
    };
    const double switch_at_s = 2.0;
    const std::uint8_t taken = 3;
    bool switched = false;
    int frames_after = -1;
    double t = 0.0;
    while (t < 4.0) {
        const bool long_now = t >= switch_at_s - long_s && frames_after < 4;
        // Before the switch: predicted (a hand-over, a take-over) or drawn
        // (a take-back). After: the other - a take-back's prediction starting
        // again two frames after the server said it was taken back.
        if (!switched && t >= switch_at_s) {
            switched = true;
            frames_after = 0;
            if (kind == Switch::take_back) {
                shown.switching();
            } else if (kind == Switch::take_over) {
                shown.taken_over(taken);
            }
        }
        bool predicted = kind != Switch::take_back;
        std::array<double, 3> from = own0;
        if (switched) {
            predicted = kind == Switch::hand_over   ? false
                        : kind == Switch::take_back ? frames_after >= 2
                                                    : true;
            if (kind == Switch::take_over) {
                from = other0;
            }
        }
        const glideslope::world::Ecef at = along(from, predicted ? t : t - lag_s);
        shown.frame(t, {at, v, predicted, false});
        if (kind == Switch::take_over && !switched) {
            shown.seen(taken, t, along(other0, t - lag_s), v);
        }
        if (frames_after >= 0) {
            ++frames_after;
        }
        t += long_now ? long_s : 1.0 / 60.0;
    }
    Switched out;
    out.worst_m = shown.worst_step_at_switch_m();
    out.longest_ms = shown.longest_frame_at_switch_ms();
    out.switches = shown.switches();
    out.what = shown.worst_step_what();
    return out;
}

} // namespace

// **A switch through long frames is blended without a step**: each of a
// hand-over, a take-back and a take-over, at frames of a sixtieth of a
// second to 0.4 s around it. Unblended each steps 6 m, past the network
// checks' bounds - 5 m at a hand-over and a take-back, 2.5 m at a take-over
// - so every case tests the rule. This is the one model of a display both
// clients use (frontend/shown.hpp).
GLIDESLOPE_TEST(a_switch_through_long_frames_is_blended_without_a_step) {
    const std::size_t space = switch_kinds.size() * long_frames_s.size();
    std::size_t covered = 0;
    double worst_ratio = 0.0;
    std::string worst_what;
    for (const Switch kind : switch_kinds) {
        const double bound = kind == Switch::take_over ? 2.5 : 5.0;
        check(speed_mps * lag_s > bound, "unblended, every switch steps past its bound");
        for (const double long_s : long_frames_s) {
            const Switched s = switch_through(kind, long_s);
            ++covered;
            const char* name = kind == Switch::hand_over   ? "a hand-over"
                               : kind == Switch::take_back ? "a take-back"
                                                           : "a take-over";
            check(s.switches == (kind == Switch::take_back ? 2u : 1u),
                  "each switch was measured as one - a take-back as two");
            check(s.longest_ms >= long_s * 1000.0 - 1e-6,
                  "the frames around the switch were as long as the case says");
            char what[320];
            std::snprintf(what, sizeof what, "%s at %.0f ms frames: %.3f m (%s)", name,
                          long_s * 1000.0, s.worst_m, s.what.c_str());
            if (s.worst_m / bound > worst_ratio) {
                worst_ratio = s.worst_m / bound;
                worst_what = what;
            }
            if (s.worst_m >= bound) {
                fail(std::string("what was shown stepped past its bound of ") +
                     std::to_string(bound) + " m: " + what);
            }
        }
    }
    std::printf("%zu of %zu cases; the nearest its bound: %s\n", covered, space,
                worst_what.c_str());
    check(covered == space, "every case was flown");
}
