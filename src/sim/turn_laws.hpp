#pragma once

// The laws a turn onto a leg is flown by, shared by the navigator and the
// autopilot that use them and by the tests that predict a turn from them, so
// that retuning one moves the prediction with it. The most the autopilot
// banks is `most_bank_deg` (sim/plan.hpp).

namespace glideslope::sim {

// Heading to bank: a degree of bank for each degree off (sim/autopilot.cpp).
inline constexpr double bank_per_degree_off = 1.0;
// The bank asked moves at 5 degrees a second (sim/autopilot.cpp).
inline constexpr double bank_rate_degps = 5.0;
// Back towards the leg: 30 degrees for each kilometre off it
// (sim/navigator.cpp), 30 at most.
inline constexpr double intercept_deg_per_metre = 30.0 / 1000.0;
// A fly-by waypoint's turn is flown round an arc at this bank, 10 degrees
// under the autopilot's most, for the loiter law to pull her back onto it
// with (sim/navigator.cpp).
inline constexpr double fly_by_bank_deg = 15.0;

} // namespace glideslope::sim
