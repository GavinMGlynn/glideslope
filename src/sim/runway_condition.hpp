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

// **What a braked wheel has to grip with**: her main tyres' maximum
// operating pressure, which 25.109(c)(1) reads the wet curve at, and the
// anti-skid that modulates her brakes, which (c)(2) takes a share of it by.
// Every landplane's catalogue file gives both, each from a source
// (`brakes` in sim/catalogue.hpp, docs/ASSETS.md).
enum class AntiSkid {
    fully_modulating, // 0.80, 25.109(c)(2)
    quasi_modulating, // 0.50
    on_off,           // 0.30
    none,             // a wheel braked by hand, sliding when it locks
};

struct WheelBrakes {
    double tyre_pressure_psi = 100.0;
    AntiSkid anti_skid = AntiSkid::fully_modulating;
    bool operator==(const WheelBrakes&) const = default;
};

// **The tyre AC 25-32's table is written for**: 100 psi, with a fully
// modulating anti-skid - what every aircraft was given before each had its
// own.
inline constexpr WheelBrakes reference_wheel_brakes{};

// **A wheel braked without anti-skid on a wet runway** is braked until it
// locks, and a locked wheel skids: it grips with the skidding coefficient,
// not the peak. NASA TM X-72650 (Horne, *Wet Runways*, 1975), figure 8,
// wet-runway braking tests: mu skid over mu max reads about 0.6 at 5 to 20
// kt (0.45 of 0.75, 0.36 of 0.62) and about 0.5 from 60 kt (0.13 of 0.27,
// 0.08 of 0.16); 0.55 is taken, the middle, at every speed. AC 25-32 table
// 2, note 3, leaves an aeroplane without anti-skid "case-by-case"; this is
// the case made for her.
inline constexpr double no_anti_skid_share = 0.55;

// The share of the wet runway's maximum coefficient an anti-skid gets,
// 25.109(c)(2): 0.80, 0.50 and 0.30; and a wheel without one, braked by hand,
// its locked wheel's skidding coefficient over the peak (runway_condition.cpp
// gives the source).
double anti_skid_efficiency(AntiSkid anti_skid);

// **The smooth wet runway's maximum tyre-to-ground braking coefficient**,
// 14 CFR 25.109(c)(1): its cubic in V/100, V the groundspeed in knots, for
// tyres at 50, 100, 200 and 300 psi, linearly interpolated between them as
// (c)(1) allows - and held at 50 psi below it and 300 above, where the rule
// has no curve: a softer tyre is given no more grip than 50 psi's. Before
// any anti-skid's share.
double wet_runway_maximum_coefficient(double groundspeed_kts, double tyre_pressure_psi);

// **The wet runway's coefficient, 14 CFR 25.109(c)**: (c)(1)'s maximum at the
// tyre's pressure times (c)(2)'s efficiency for her anti-skid. For the
// reference tyre, 0.64 at a standstill, 0.38 at 50 knots and 0.22 at 100.
double wet_wheel_braking_coefficient(double groundspeed_kts, const WheelBrakes& brakes);

// **The coefficient a runway of condition `code` gives at a groundspeed**,
// AC 25-32 table 2: code 5, 25.109(c)'s (above); 4, 0.203; 3, 0.163; 2, half
// 25.109(c)'s but no more than 0.163 below 85 per cent of the tyre's
// hydroplaning speed - 9 sqrt(P), P her tyre's pressure (note 4) - and 0.053
// from it; 1, 0.083. Table 2's figures are for a fully modulating anti-skid;
// note 3 multiplies them by 0.625 for a quasi-modulating one and 0.375 for
// on-off - the efficiencies over 0.80 - and so here by her efficiency over
// 0.80 for each. Throws std::invalid_argument for dry (6), which is the
// model's own and has no figure here, and for any code not from 1 to 6.
double wheel_braking_coefficient(int code, double groundspeed_kts, const WheelBrakes& brakes);

// **A free-rolling tyre's grip sideways** - its cornering-force friction
// coefficient, the side force over the weight on it at the yaw angle that
// gives the most - on a runway that is not dry.
//
// **On a wet runway it is a share of the runway's maximum braking
// coefficient**, from NASA TN D-7743 (Dreher and Tanner, 1974), table II:
// five 30 x 11.5-14.5 type VIII tyres at 265 psi, braked and yawed on the
// same damp concrete at 5, 50 and 100 knots. The most unbraked cornering
// coefficient over 4, 8 and 12 degrees of yaw, over the unyawed tyre's
// maximum braking coefficient, averages 0.571 at 5 kt, 0.603 at 50 and
// 0.769 at 100 (each tyre's in tests/unit/test_weather.cpp). It is read
// linearly between, and held beyond.
double wet_cornering_share(double groundspeed_kts);

// **The dry runway's cornering coefficient**, what the wet's is measured
// against: NASA TN D-7351 (Tanner and Dreher, 1973), figure 7, a 40 x 14-16
// type VII tyre at 155 psi and 10 degrees of yaw, unbraked, on dry concrete
// - its fairing read as 0.48 at 5 kt falling to 0.43 at 100, read linearly
// between and held beyond.
double dry_cornering_coefficient(double groundspeed_kts);

// **The runway's maximum coefficient**: what a braked wheel on runway
// condition `code` grips before any anti-skid takes its share -
// `wheel_braking_coefficient` for a fully modulating anti-skid over its
// 0.80. On a wet runway, 25.109(c)(1)'s curve at the tyre's pressure.
double runway_maximum_coefficient(int code, double groundspeed_kts, double tyre_pressure_psi);

// **The free-rolling tyre's cornering coefficient on runway condition
// `code`**: TN D-7743's share at the groundspeed times the runway's maximum
// coefficient for the tyre's pressure. Predicted for TN D-7351's tyre, it
// is that report's damp-runway figure 7 within 0.05 (the test). Codes 1 to
// 4 carry the wet runway's share to their own maximum coefficient, which
// no source here measured cornering on. Throws for dry, as
// `wheel_braking_coefficient` does.
double cornering_coefficient(int code, double groundspeed_kts, double tyre_pressure_psi);

// **The share of her dry side grip a tyre keeps on runway condition
// `code`**: its cornering coefficient over the dry runway's, at most 1 -
// the wet runway's loss of side grip as the reports measured it, laid on
// her flight model's own dry side friction as the brakes' share is laid on
// its braking. 1 on a dry runway.
double side_grip_share(int code, double groundspeed_kts, double tyre_pressure_psi);

// **A wet runway's landing distance is 1.15 times the dry**: 14 CFR
// 121.195(d) holds a turbojet to an effective runway at least 115 per cent
// of what (b) asks of a dry one when the runway may be wet or slippery.
inline constexpr double wet_landing_factor = 1.15;

} // namespace glideslope::sim
