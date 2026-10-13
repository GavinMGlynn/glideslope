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
// **A fly-by passes its waypoint no further off than this**, metres: half
// RNP 0.3's 556 m (0.3 nautical miles, the tightest RNAV containment an
// approach is flown to), and a quarter of the kilometre within which the
// copilot's coast is held. Uncapped, a light aeroplane's fly-by passed a
// coastal waypoint 1,275 m off and a 150-degree turn kilometres off; a
// plan that traces a coast or a landmark asks for its waypoints, not
// corners cut by them. Where an arc at fly_by_cap_bank_deg cannot pass this
// near, that arc is flown: the nearest she can pass and still turn onto the
// next leg with bank to spare (sim/navigator.cpp).
inline constexpr double most_fly_by_miss_m = 278.0;
// **The tightest arc the miss's cap tightens a fly-by to**: 3 degrees under
// the autopilot's most, for the loiter law to pull her back onto it with.
// Tightened to 25, with nothing to spare, the F-15C at 200 kt and the
// Mosquito at 148 turning 120 degrees strayed 0.108 and 0.105 of their
// radius off the next leg, against the tenth they are held to.
inline constexpr double fly_by_cap_bank_deg = 22.0;

} // namespace glideslope::sim
