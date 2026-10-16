### Every flare begins at the attitude the glidepath was flown at, 2026-09-27 — tail done

**What is missing first.** **The F-35B does not flare at all.** She flies
the glidepath at about 19.5 degrees of alpha, above the flare's 12-degree
incidence guard, so from the flare's first step her nose is held at the path
attitude and never rounds out: what changed is that it is no longer pushed
down six degrees. She meets the runway sinking at 883 ft/min at 16.4 degrees,
where on main it was 1,106 ft/min at 11.7 - still past the 600 the gear is
judged to take, which is the open tail "the AI's own touchdown in the 787-8,
F-15C and F-35B is a crash by the server's rule". She also still lands
conventionally at 156 knots, and cannot hover or land vertically (its own
tail). Of the tail itself nothing is missing: every flare begins at the path's
attitude and touches short of the strike.

**The check.** `the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief`
and `the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief` now watch
every flare: the attitude on the last step of the glidepath before the flare
began, the lowest attitude from there to the touch, and the attitude at the
touch. Each fails an aeroplane whose nose goes more than 1.5 degrees below its
path attitude in the flare, or that touches within 2 degrees of its
tail-strike attitude (the take-off's own margin). The strike attitude is read
from the aeroplane's contacts: the working that the take-off autopilot had
(`Departure::read_the_gear`) is now `Aircraft::stance`, shared by both. Both
tests still assert all fourteen aeroplanes are flown; four have no tail to
strike and are named in the test, which fails if the contacts say otherwise:
the F-15C and the Learjet 35A (no contact behind the main wheels), the J-3 Cub
(a tail wheel) and the Short S.23 (a flying boat, whose keel and floats give a
meaningless strike attitude of -74 degrees). **Seen to fail** on main's
lander: the F-35B flew the approach lesson's glidepath at 16.5 degrees and the
flare put her nose down to 11.7 (the circuit: 15.9 to 11.6); every other
aeroplane passed.

**The strike half, seen red on a situation built for it.** No aeroplane's
flare comes near its strike attitude - the incidence guard stops every nose
first - so the landings never exercise that half: a deliberate bug bounding
the flare at the strike attitude plus a degree left both lessons green. So the
approach lesson also feeds the check, for each of the twelve aeroplanes in the
catalogue with a tail to strike (the sixteen less the four named), a touch at
the lander's own bound, two degrees short of the strike, which it must name,
and one three degrees short, which it must not; it asserts twelve were fed.
The check fails a touch at or above 2.5 degrees short of the strike, so a
flare that has run up to the lander's two-degree bound is caught. Seen red:
with the comparison as first written (above 2.0 short), all twelve touches at
the bound went unnamed and the test failed; restored, it passes.

**The cause, and the fix.** The flare's attitude was clamped to at most ten
degrees, so a flare begun above ten stepped down to it at once. It now starts
from the attitude she flew the path at and is never pushed down from it; the
bound only stops the nose rising past two degrees short of the tail-strike
attitude, or past the three-point attitude on a tail wheel. With nothing
behind the main wheels only the flare's existing incidence limit (12 degrees
of alpha) holds the nose (`src/sim/lander.cpp`, `most_flare_pitch_deg_`).

**Before and after** (release build, Linux): pitch on the path / lowest in the
flare / at the touch, in degrees, and where she touched, metres beyond the
threshold; the strike attitude from the contacts.

| aeroplane | strikes at | approach lesson | circuit |
|---|---|---|---|
| 737-300 | 12.8 | 2.1 / 2.0 / 4.9, 327 m (same) | 1.7 / 1.6 / 4.4, 323 m, 135 kt (same) |
| 787-8 | 10.5 | -0.3 / -0.3 / 0.6, 239 m (same) | -0.2 / -0.2 / 0.6, 231 m, 147 kt (same) |
| A320 | 14.3 | 4.2 / 4.2 / 6.8, 282 m (same) | 4.1 / 4.1 / 6.7, 281 m, 145 kt (same) |
| A380 | 34.0 | 0.8 / 0.8 / 1.7, 270 m (same) | 0.8 / 0.8 / 1.7, 264 m, 136 kt (same) |
| B-2A | 14.3 | 4.5 / 4.5 / 7.5, 295 m (same) | 4.4 / 4.4 / 7.4, 296 m, 139 kt (same) |
| C172P | 10.3 | 2.3 / 2.3 / 3.9, 313 m (same) | 2.2 / 2.2 / 3.8, 313 m, 58 kt (same) |
| C182 | 11.3 | 2.9 / 2.9 / 5.7, 308 m (same) | 2.9 / 2.9 / 5.6, 312 m, 60 kt (same) |
| F-15C | none | 4.4 / 4.4 / 6.3, 262 m (same) | 4.3 / 4.3 / 6.2, 256 m, 194 kt (same) |
| **F-35B** | 22.2 | 16.5 / **11.7 / 11.7**, 255 m -> 16.5 / **16.4 / 16.4**, 263 m | 15.9 / **11.6 / 11.6**, 254 m, 157 kt -> 15.9 / **15.8 / 15.8**, 262 m, 156 kt |
| J-3 Cub | none | 0.4 / 0.4 / 1.5, 290 m (same) | 0.3 / 0.3 / 1.3, 289 m, 42 kt (same) |
| Learjet 35A | none | 3.0 / 3.0 / 8.1 -> 8.5, 712 m | 2.8 / 2.8 / 8.0 -> 8.2, 710 m, 102 kt |
| Mosquito FB.VI | 15.3 | -1.4 / -1.4 / 8.3 -> 8.4, 710 m | -1.5 / -1.5 / 8.3 -> 8.4, 710 m, 93 kt |
| PA-28 | 14.2 | 0.6 / 0.6 / 3.8, 336 m (same) | 0.5 / 0.5 / 3.7, 336 m, 58 kt (same) |
| Short S.23 | none | -0.2 / -0.2 / 1.3, 254 m (same) | -0.3 / -0.3 / 1.1, 245 m, 87 kt (same) |

The Learjet and the Mosquito had asked for the old ten degrees late in the
flare; they now touch a few tenths of a degree higher, at the same place and
speed.

**Verified**: the approach tests (the lesson, its demonstration, the fast
approach, and the three taken back on the roll), the circuit tests (the
lesson, its demonstration and the low downwind fault), the lander's own tests
and the wind-shear approach pass on Linux (release build); so do the take-off
tests the moved gear reading serves - the take-off lesson, its demonstration,
rotating early, and every landplane leaving the runway near its rotation
speed and at every loading.
