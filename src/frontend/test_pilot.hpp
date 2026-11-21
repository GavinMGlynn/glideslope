#pragma once

// **A test's pilot**, the same in both clients: the controls keep changing,
// so that when an input arrives - late, early, or flown for more steps than
// the server flies it - matters. A stick held still makes every input the
// same, and a prediction flown for the wrong number of steps of each is
// still right (docs/status, 2026-11-22: the window client's prediction
// error against a server at 0.6 of real time was nought, paced or not,
// until it flew this).

#include "sim/aircraft.hpp"

#include <cmath>
#include <cstdint>

namespace glideslope::frontend {

// The controls of input `sequence`, sent at 30 a second.
inline sim::Controls test_pilot(std::uint32_t sequence) {
    const double t = static_cast<double>(sequence) / 30.0;
    sim::Controls c;
    c.throttle = 0.7 + 0.2 * std::sin(t * 0.7);
    c.aileron = 0.3 * std::sin(t * 1.3);
    c.elevator = 0.05 * std::sin(t * 0.9);
    c.rudder = 0.05 * std::sin(t * 0.5);
    return c;
}

} // namespace glideslope::frontend
