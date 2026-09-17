#include "sim/fixed_step.hpp"

#include <stdexcept>

namespace glideslope::sim {

namespace {

constexpr std::int64_t ns_per_second = 1'000'000'000;

// Whole steps due after `ns` nanoseconds, and the nanosecond-steps left over.
// Split into whole and part seconds so that nothing overflows for as long as an
// int64 of nanoseconds lasts (292 years), where `ns * 120` would overflow after
// two and a half.
struct Due {
    std::int64_t steps;
    std::int64_t remainder; // in units of 1/(120 * 1e9) s, below 1e9
};

Due due_after(std::int64_t ns) {
    const std::int64_t whole = ns / ns_per_second;
    const std::int64_t part = (ns % ns_per_second) * steps_per_second;
    return {whole * steps_per_second + part / ns_per_second, part % ns_per_second};
}

} // namespace

std::int64_t FixedStep::advance(std::chrono::nanoseconds elapsed) {
    if (elapsed.count() < 0) {
        throw std::invalid_argument("FixedStep::advance: time does not run backwards");
    }
    elapsed_ns_ += elapsed.count();
    const std::int64_t due = due_after(elapsed_ns_).steps;
    const std::int64_t n = due - steps_;
    steps_ = due;
    return n;
}

double FixedStep::alpha() const {
    return static_cast<double>(due_after(elapsed_ns_).remainder) /
           static_cast<double>(ns_per_second);
}

} // namespace glideslope::sim
