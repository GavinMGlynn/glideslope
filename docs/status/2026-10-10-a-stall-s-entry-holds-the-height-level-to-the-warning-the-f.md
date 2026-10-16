### A stall's entry holds the height level to the warning: the F-15C and F-35B recovered within their lesson's height, 2026-10-10 — item still open

**What is still missing, first.** Left thirty seconds, the A320 (1.90 g)
and the Mosquito (2.22 g) are over 2 g with the 10% in hand. At the warning
the Learjet 35A (385 of 350) and the Short S.23 (184 of 200) are named, and
the Mosquito is never level again. The AI's notice keeps its two names (the
entry below).

**The entry** (`AutopilotModes::hold_height_to_the_stall`, src/sim/plan.hpp
and autopilot.cpp). A stall lesson enters level: the FAA's stall tasks hold
the altitude as the speed comes back to the warning (Airplane Flying
Handbook, FAA-H-8083-3C, chapter 5; AC 120-109A's approach to stall). The
autopilot's height hold could not: its pitch envelope tops out at 15
degrees, and in her landing configuration the F-35B needs 29 degrees of
alpha at her warning, so she sank into it at 4,400 ft/min, and the F-15C at
3,400. In this mode the 15 degrees bound the flight path, not the nose -
the nose may rise by the wing's angle of attack - but **never more than 5
degrees above the nose**, and **only until the wing is three tenths of a
degree past the angle its lift peaked at**; from then until the mode is let
go the top is 15 again. The lesson's flight (`fly_a_stall`) and the
instructor's demonstration both enter by it (`ask_for_the_stall_entry`);
the recovery lets it go. No other mode changes.

Why each bound (each measured):
- Without the 5 degrees above the nose, the C172P - whose lift is flat at
  its peak, so she mushes without ever passing it by three tenths - had her
  pitch command wind up to 30 degrees with her nose at 7. The recovery,
  engaged from that command, zoomed her to 20 degrees of pitch and back to
  her height, so a recovery left 35 seconds late finished the lesson
  without losing its 300 ft: `a_stall_recovered_badly_is_named_in_the_debrief`
  went red ("c172p recovering late and lazily is not faultless"). That is
  the test seen to fail when the debrief stops naming a bad recovery; with
  the bound it names her again (lowest 4,476 ft), and the test is unchanged.
- Letting go a degree past the peak, the A320 left thirty seconds pulled
  2.13 g; two degrees, 2.22; three tenths, 1.90. Never letting go, the
  737-300 was never recovered and the F-35B and S.23 pulled 2.9 g; holding
  the nose where it was at the peak, the 737-300 was never recovered.

**The figures** (linux-release; lost from the hand-over / bound, peak g):

| Aeroplane | at the warning: before -> now | left 30 s: before -> now |
|---|---|---|
| 737-300 | 773 / 1,300, 1.15 -> 777, 1.13 | 1,223 / 2,476, 1.72 -> 1,235 / 2,492, 1.74 |
| 787-8 | 657, 1.16 -> 657, 1.16 | 766 / 1,740, 1.20 -> 766, 1.20 |
| A320 | 610, 1.20 -> 584, 1.20 | 1,240 / 1,584, **1.95** -> 1,332 / 1,602, **1.90 named** |
| A380 | 908, 1.30 -> 908, 1.30 | 1,484 / 2,908, 1.66 -> same |
| B-2A | 112 / 300, 1.14 -> 113, 1.14 | 998 / 2,001, 1.61 -> 999 / 2,002, 1.61 |
| C172P | 75 / 300, 1.05 -> same | 75 / 568, 1.28 -> 75, 1.29 |
| C182 | 71 / 300, 1.05 -> same | 38 / 539, 1.53 -> same |
| F-15C | **522 / 500 named** -> **303**, 1.33 | 1,424 / 3,577, 1.57 -> 1,431 / 3,467, 1.60 |
| F-35B | **773 / 500 named** -> **419**, 1.13 | 3,252 / 6,257, 1.68 -> 4,502 / 7,436, 1.71 |
| J-3 Cub | 29 / 300, 1.05 -> same | 42 / 308, 1.26 -> same |
| Learjet 35A | **385 / 350 named** -> same | 434 / 1,359, 1.26 -> same |
| Mosquito | **never level, named** -> same | 1,258 / 1,899, **2.22 named** -> same |
| PA-28 | 131 / 300, 1.20 -> same | 120 / 515, 1.26 -> same |
| Short S.23 | **184 / 200 named** -> same | 146 / 640, 1.33 -> same |

Names taken off: the F-15C's and F-35B's heights at the warning. The A320's
load is held at its new 1.90, the Learjet's height at 385. **Seen to
fail**: with the mode ignored, the warning check red for the F-15C (521.9
against 500) and the F-35B (773.5) with their names off.

**The Short S.23 is inside her lesson's 200 ft** (184), and is named only
for the 10% the checks keep in hand for other machines: 184 x 1.1 = 202.4.
**The Learjet's 385 against 350 reads as the speed her lesson's recovery
ends at, not her power, flap or the pitch she is handed at** (from her
hand-over and the estimate, not a fresh trace): she is handed over level
(-188 ft/min) at 101.5 kt and 13.1 degrees of alpha, her nose at 12.3 - not
sinking, and not at the envelope's top, which is why the level entry does
not move her. Her lesson ends its recovery at stall+35, 131.5 kt: 30 kt to
find at 20,000 ft with her gear and flaps down, which from height alone
would be 575 ft (the check's own estimate); full power gives the rest. Her
figures give no go-around flap, so the recovery raises none; one would need
a source, as the Cherokee's has.

**Verified** (linux-release, all targets): the 286 tests matching stall,
recover, upset, lesson, instructor, take, glide, orbit, rolled or autopilot
pass but `the_f22a_held_at_130_kt_on_the_autopilot_departs`, which fails on
#155's base without this branch (sent there). The selftest hash does not
move, `182dd6c996e0ee4c`.
