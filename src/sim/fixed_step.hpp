#pragma once

#include <chrono>
#include <cstdint>

namespace glideslope::sim {

// The simulation's rate. Every aircraft, on every machine, steps at this rate
// and at no other.
inline constexpr std::int64_t steps_per_second = 120;

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
