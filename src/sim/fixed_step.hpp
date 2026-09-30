#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace glideslope::sim {

// The simulation's rate. Every aircraft, on every machine, steps at this rate
// and at no other.
inline constexpr std::int64_t steps_per_second = 120;

// **How many of the steps due a frame loop flies in one pass**; the rest are
// let go, never carried to the next. On a server, up to four seconds' worth:
// a dropped step there is the prediction falling behind the server's clock
// unflagged, and CI's sanitized software Vulkan draws a frame in 250 ms to
// 2.4 s. Four seconds is also as many inputs as the prediction holds
// (sim::most_unacknowledged); a pass longer than that - a laptop that slept,
// a debugger's pause - is not flown on unattended for as long as it was
// away. Alone, a fifth of a second's, as it always was: nothing is waiting
// to be kept up with, and time away is not flown.
inline constexpr std::int64_t most_steps_a_pass_alone = 24;
inline constexpr std::int64_t most_steps_a_pass_on_a_server = 4 * steps_per_second;
inline std::int64_t steps_to_fly(std::int64_t due, bool on_a_server) {
    return std::min(due, on_a_server ? most_steps_a_pass_on_a_server : most_steps_a_pass_alone);
}
// **How long a pass's held keys move their levers for**, in seconds: the
// steps flown, but never more than a fifth of a second's, so that a long
// frame does not move a held key's lever four seconds' worth at once.
inline double key_seconds(std::int64_t steps_flown) {
    return static_cast<double>(std::min(steps_flown, most_steps_a_pass_alone)) /
           static_cast<double>(steps_per_second);
}

// Turns elapsed time, in whatever pieces it arrives, into whole simulation
// steps.
//
// **The number of steps taken depends only on the total time elapsed**, never on
// how it was divided up: time is kept in whole nanoseconds, and the steps due are
// computed from the total each time rather than by adding up fractions of a step.
// A frame loop that stutters, a test that feeds time in odd pieces and a server
// that ticks on a timer all take exactly the same steps for the same total.
class FixedStep {
public:
    // Adds `elapsed` and returns how many steps have become due. Throws
    // std::invalid_argument for negative time.
    std::int64_t advance(std::chrono::nanoseconds elapsed);

    // How far the time since the last step has got towards the next one, in
    // [0, 1). Presentation interpolates between the last two states by this.
    double alpha() const;

    std::int64_t steps_taken() const {
        return steps_;
    }
    std::chrono::nanoseconds elapsed() const {
        return std::chrono::nanoseconds(elapsed_ns_);
    }

    // One step's length in seconds, as the flight model is told it.
    static double step_seconds() {
        return 1.0 / static_cast<double>(steps_per_second);
    }

private:
    std::int64_t elapsed_ns_ = 0;
    std::int64_t steps_ = 0;
};

} // namespace glideslope::sim
