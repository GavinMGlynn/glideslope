#pragma once

// What a runway's surface gives a braked wheel.
//
// **The runway's condition is the FAA's runway condition code**, the
// Runway Condition Assessment Matrix of TALPA (AC 25-32 table 2, AC 91-79A):
// 6 dry; 5 wet - damp, or up to 1/8 in (3 mm) of water, slush or snow - or
// frost; 4 compacted snow at -15 C and colder; 3 wet and "slippery when wet",
// over 1/8 in of dry or wet snow, or compacted snow warmer than -15 C; 2 over
// 1/8 in of water or slush; 1 ice. Code 0 (wet ice, nil braking) is a runway
// nobody operates on, and is not one here.
//
// **Each code's wheel braking coefficient is AC 25-32's**, for a fully
// modulating anti-skid system - the braking force of a braked wheel over the
// weight on it - except dry, which is the aircraft's own: its flight model's
// friction, what every figure it has been held to was flown on.

namespace glideslope::sim {

inline constexpr int dry_runway = 6;
inline constexpr int wet_runway = 5;
inline constexpr int most_slippery_runway = 1;

// **The wet runway's coefficient, 14 CFR 25.109(c)**: (c)(1)'s maximum
// tire-to-ground braking coefficient on a smooth wet runway, its curve for a
// 100 psi tyre, at a groundspeed in knots,
//   -0.0437 (V/100)^3 + 0.320 (V/100)^2 - 0.805 (V/100) + 0.804,
// times (c)(2)'s 0.80 for a fully modulating anti-skid system: 0.64 at a
// standstill, 0.38 at 50 knots and 0.22 at 100.
double wet_wheel_braking_coefficient(double groundspeed_kts);

// **The coefficient a runway of condition `code` gives at a groundspeed**,
// AC 25-32 table 2: code 5, 25.109(c)'s (above); 4, 0.203; 3, 0.163; 2, half
// 25.109(c)'s but no more than 0.163 below 85 per cent of the tyre's
// hydroplaning speed - 9 sqrt(100 psi), 90 knots - and 0.053 from it; 1,
// 0.083. Throws std::invalid_argument for dry (6), which is the model's own
// and has no figure here, and for any code not from 1 to 6.
double wheel_braking_coefficient(int code, double groundspeed_kts);

// **A wet runway's landing distance is 1.15 times the dry**: 14 CFR
// 121.195(d) holds a turbojet to an effective runway at least 115 per cent
// of what (b) asks of a dry one when the runway may be wet or slippery.
inline constexpr double wet_landing_factor = 1.15;

} // namespace glideslope::sim
