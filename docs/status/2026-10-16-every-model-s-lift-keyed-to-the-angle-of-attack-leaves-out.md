### Every model's lift keyed to the angle of attack leaves out the crosswind, 2026-10-15 — item done; the tree is red on one walking-pace test (resolved in the entry above, 2026-10-16)

**What is still not true first.**
- **One test fails with this change, and it is another item's file**:
  `every_landplane_rolling_at_walking_pace_in_a_15_kt_crosswind_has_no_brake_added_past_its_ceiling_and_stops_with_her_nose_up`
  (tests/unit/test_lesson.cpp). Handed over at 3 kt in 15 kt across, the
  C172P's autobrake now reaches 0.294 rolling, and the PA-28's 0.295,
  against the 0.300 ceiling the test must see reached (0.300 on main): the
  test says the rolling ceiling is no longer tested on them. Their lift
  standing in the beam wind is gone (the C172P's was up to 240 lb lifting
  her off her brakes), so they stop a little sooner - the C172P 0.70 s at
  walking pace against 0.72 - before the autobrake's ramp reaches its
  ceiling. Every other part of the test passes on them: stopped, nose up,
  no brake past the ceiling. The walking-pace tests are being reshaped by
  another item (`walking_pace_then_stop`); this branch does not touch them.
  It does not merge until that test is green.
- **Lift not keyed to the angle of attack still counts the crosswind
  standing**: flaps, elevators, and the C182's lift at no angle (her CLo).
  Standing in 15 kt across what is left gives the C182 42 to 46 lb and the
  F-22A 260 to 280 (her dCLflap, dCLsb and CLde); every other model nothing
  that shows. A new Phase 9 item.
- **The Mosquito's slipstream lift flips with an undefined angle of
  attack.** `lift-slipstream` is her wing's lift coefficient, read at her
  angle of attack, on her slipstreams' own pressure (u and the propellers'
  induced velocity): it leaves out the crosswind, so it is named, not moved.
  But on her brakes at +9 boost in calm air, creeping at 0.08 ft/s, her angle
  of attack flips from -93 to +87 degrees, and that term from -2,570 lb to
  +2,054 (a probe, not committed). It is what moved her take-off swing
  figure (below). A new Phase 9 item.
- **Every model but the Cub has her wing's drag keyed to the angle of attack
  still counting the crosswind**: the item named lift. A new Phase 9 item.
- **The learnt landing flies a C172P it was not trained on, now further
  from it.** It flies her slipped: in calm air from the centre start, 8 to 10
  degrees of sideslip on the approach and up to 17.7 in the flare (traced in
  the bindings, not committed), where `qbarUW` is 0.91 of `qbar` - so her
  lift there moves by up to 9%, and her landings with it. The policy is not
  trained again; its parity fixture is flown again (below). A new Phase 9
  item, "The learnt landing's policy flies a C172P it was not trained on".
- **The swept wings are moved on the same argument as their pitch terms**
  (simple sweep theory, NACA Report 863; the entry below), not on a
  measurement of a swept wing's lift standing in a beam wind.

**The change.** Each make script names its lift terms keyed to the angle of
attack to `alpha_rate.with_alpha_terms_on_qbar_uw` (axis LIFT) beside its
pitch terms, and its docstring says why, what standing in 15 kt across did
before and after, and which lift terms are not moved and why: `CLalpha` in
the 737-300, 747-400, 787-8, A320, A380, B-2A, C182, F-15C, F-35B and
Learjet 35A (with the flaps' columns where the table has them); `CLwbh` in
the C172P, PA-28 and S.23; `CLalpha` and `CLDlef` (her leading-edge flaps'
lift, keyed to the angle of attack) in the F-22A; `lift-alpha` in the
Mosquito. The Cub's were moved in #205. **Named, not moved**: `CLq` in the
F-22A, PA-28 and S.23 and the Cub's `Lift_pitch_rate` - lift with pitch rate,
the angle of attack picking its coefficient - and the Mosquito's
`lift-slipstream` (above). The guidance is issue #89's and the FlightGear
wiki's "JSBSim Aerodynamics" (QBarUW "is more appropriate for CL than the
generic QBar"); for the swept wings, the same argument as their pitch terms.
**The Mosquito's `lift-alpha` reads the angle of attack through a function**,
`aero/function/lift-coefficient-wing`, her table of it and the flaps:
`with_alpha_terms_on_qbar_uw` now takes a term that reads such a function of
the model (`functions_of_alpha`, transitively, commented-out functions not
read) as reading the angle of attack. **Seen refusing**: with main's
`alpha_rate.py`, make_mosquito_fb6.py fails "lift-alpha does not read the
angle of attack"; with this one it moves it; naming the 737's `CLde` (no
angle) and the Mosquito's `lift-slipstream` (no `qbar`) are refused.

**Standing on full brakes in 15 kt straight across** (linux-debug, a probe
not committed: each landplane settled ten seconds in calm air, the wind on
from either side, the next fourteen seconds read; the S.23 afloat). The
moved terms, lb, before and after, and her angle of attack before:

| model | angle of attack, deg | the moved terms, before | after |
|---|---|---|---|
| 737-300 | -85 to 95 | -688 to +202 | 0 |
| 747-400 | -82 to 91 | -2,924 to +2,581 | 0 |
| 787-8 | -70 to 92 | -2,570 to +4,112 | 0 |
| A320 | -87 to 119 | -246 to +1,675 | 0 |
| A380 | -158 to 172 | -1,414 to +2,040 | 0 |
| B-2A | -74 to 124 | -7,262 to +4,723 | 0 |
| C172P | -49 to 131 | -32 to +239 | 0 |
| C182 | -72 to 118 | -76 to +202 | 0 |
| F-15C | -4 to 92 | -98 to +589 | 0 to 0.1 |
| F-22A | -175 to 120 | -263 to +659 | 0 |
| F-35B | -15 to 110 | -292 to +472 | 0 |
| Learjet 35A | -36 to 100 | -302 to +263 | 0 |
| Mosquito (`lift-alpha`, with `lift-slipstream`) | -179 to 179 | -479 to +672 | +228 to +265, `lift-slipstream` alone |
| PA-28 | -17 to 175 | -72 to +196 | 0 |
| S.23, afloat | -3 to 14 | +288 to +2,096 | +21 to +406 |

**What it did to them standing**: the C172P's pitch moved 1.65 degrees and
at up to 1.8 deg/s over those fourteen seconds, now 0.13 and 0.3; the
C182's 2.0 degrees, now 0.8. Every other landplane's pitch moved under 0.2
degrees either way, as before; the S.23 afloat rolls up to 4.8 degrees, as
before (4.9); the Cub stands as she did.

**Their figures** (linux-debug, every test in test_figures.cpp, 148, before
and after; all pass both ways). 124 print the same to the printed digit,
every C172P, C182, Cub, B-2A, A380 and F-15C figure among them. Moved: the
Learjet's take-off field length 5,341 -> 5,379 ft (+0.70%, range 4,770 to
5,830); the 787-8's 10,877 -> 10,902 (+0.23%); the 737's 7,793.8 ->
7,793.4, the 747's 11,268.9 -> 11,269.4, the A320's in the third decimal;
the airliners' engine-out climbs in the third decimal; the Mosquito's
take-off distance 2,520.3 -> 2,522.0 ft, her single-engine ceiling 12,479.8
-> 12,479.1, her safety speeds and climbs in the third decimal; the S.23's
long-range water take-off 798.3 -> 798.9 yd and 34.11 -> 34.13 s, her
standard 26.83 s; the PA-28's ground roll and stall in the third decimal;
the F-22A's climb at 50,000 ft in the second; the F-35B's hovers by a few
hundredths of a foot. **One over 1%, traced: the Mosquito's take-off swing,
2.580 -> 2.614 lb/sq in of port lead (+1.3%, range 0.2 to 3.0).** It is
measured standing on her brakes at +9 boost, where her airspeed is a creep
of 0.08 ft/s and `lift-alpha` is 0.002 lb on either pressure. But her
`lift-slipstream`, unmoved, reads the angle of attack of that creep: -93
degrees with this model, -52 with main's, and with a 0.3 lead +87 against
-50 - so that term is -2,570 or +2,054 lb, and her pitch on her gear 6.27
degrees nose down against 6.01. The swing is the yaw and roll moments
resolved at that pitch, so it moves with it. The new item above.

**The other flights** (linux-debug, 159 tests: every crosswind heading
hold, landing, take-off, wet runway, walking-pace, tail-wheel, braked-stop
and standing test, and every learnt landing): all pass but the walking-pace
test above (and two that need a DEM the binary run had not fetched, skipped
both ways). 50 print something different, all small: the heading holds in
20 kt across by a hundredth of a degree of heading and a foot of height; the light aeroplanes' crosswind landings by a
metre or a foot a minute; braked to a stop in 15 kt across the C172P's most
pitch rate 8.8 -> 9.2 deg/s and the PA-28's 4.9 -> 5.3 (bound 20), the
Mosquito's named runs the same five; the steady-crosswind stops within a
metre and a tenth of a second.

**The selftest hash moves**, deliberately: it flies a C172P, whose `CLwbh`
is now on `qbarUW`. `ba8e97281095464f` -> `07cc7d4a4d42a300` in
linux-debug; its printed flight (roll -0.77, pitch 0.92, heading 349.61,
106.41 KCAS, 20.2 ft/min, 2447 RPM) is the same to every printed digit.

**The learnt landing's parity fixture, flown again** with
`tools/rl/export.py --parity-only` (its header's `model` line now the sha256
of this c172p.xml, d14de7ee...b508). Against the fixture's old flights, from
the 27 verification starts (new less old): **calm air**, 4 to 10 ft/min less
sink (now 149 to 155), 1.70 to 1.76 m less across (now +0.62 to +0.94), 76
to 82 m shorter (now 137 to 158 m along); **10 kt from one side** (+10 kt),
46 to 105 ft/min more sink (now 152 to 184), 1.40 to 1.57 m less across
(now +0.24 to +0.88), 56 to 100 m shorter (now 256 to 288); **10 kt from the
other** (-10 kt), 72 to 73 ft/min more sink (now 210 to 213), 0.95 to 1.01
m less across (now +0.55 to +0.64), 53 to 59 m shorter (now 57 to 74). All
within her limits (5 m, 300 ft/min); in the simulation the parity test
passes apart from the bindings by at most 0.25 ft/min, 0.001 m across and
0.20 m along, and every learnt-landing test passes (worst sink 213 ft/min,
worst 0.94 m across). **Why so much, traced**: she flies her approach and
flare slipped, 8 to 18 degrees in calm air (above), so her lift there is
cos^2 of it less: a few percent in the flare, which she meets by flaring
sooner and landing shorter.

**The test.**
`every_flight_models_angle_of_attack_lift_terms_leave_out_the_crosswind_or_are_named_with_their_reason`
(tests/tools/alpha_lift_terms.py), a sibling of the pitch check: in every
committed model's LIFT axis, comments left out, every function that reads
`aero/alpha-rad` or `aero/alpha-deg` - itself or through a function of the
model that does - and names a dynamic pressure (those that multiply the
alpha rate left to their own check) must be listed, moved, every pressure in
it `aero/qbarUW-psf`, or named, on `qbar`, with its reason. Coverage
asserted: sixteen aircraft, 18 terms moved and 4 named, and it fails if the
directory, a term or a count differs. **Seen red**, each on a copy of the
models: main's models (16 terms on `qbar`, 2 of 18 moved, the Cub's); a
named term moved (the PA-28's `CLq`); a term renamed (the Mosquito's
`lift-alpha`: unlisted, and the listed one missing - found through her
lift-coefficient table); the Mosquito's `lift-alpha` put back on `qbar`;
the F-22A's two commented-out `CLDh` terms uncommented (found, neither moved
nor named); an aircraft not listed; a listed one missing. Each restored,
green.

**Verification.** **The full suite in linux-debug** (`ctest -j4`, with no
display): of 1,303 tests, 1,278 pass, 24 are skipped (the live model calls,
the Vulkan sky and server window, and the Windows file-sharing tests), and
one fails - the walking-pace test above. (The run was stopped by its time
limit with 111 tests to go, and the disk filled under it, so that the
cockpit view's terrain tile could not be written and its six sibling views
were not run; those 118 were run again on their own, and passed.) Among
them every committed-model test - each make script writes the committed
models unchanged - the three term checks, the selftest's, and the four
measured landings. **In a `-march=x86-64-v2` debug build** (no fused
multiply-adds, as CI builds) **and in linux-release**: the 148 figures and
the 159 other flights, the same as linux-debug: all pass but the walking-
pace test, the C172P at 0.294 and the PA-28 at 0.295 in both. The v2 build
prints the same 148 figures as linux-debug to the last digit; release
differs in two, the Mosquito's take-off distance (2,521.9 ft) and swing
(2.544, the term above). Selftest hashes: `07cc7d4a4d42a300` in linux-debug
and v2, `b79bca7f3d8f6429` in release. **On CI**, run 38690110280 on
652e7652 (the models, the fixture and the docs): on every job of every platform the
one failure is that same walking-pace test; the Rocky 9 linux-release job
ran all 1,303 and failed it alone.
