### Every circuit touches down on the runway, past its threshold, 2026-09-27 — tail done

**Found by review, now tails:** the flare starts at the path pitch but no more
than 10 degrees, so the F-35B's nose goes down some 6.5 degrees at 30 ft,
which is why she touches down flat at 157 kt; and nothing checks touchdown
pitch against a tail-strike attitude. Separately, a Linux debug CI shard ran
27 min 43 s of its 30 (the circuit lessons take about 980 s each there).

**What is missing first.** Nothing of the tail: every circuit and every
approach lesson touches down on the runway. Still open beside it: the B-2A
crosses the threshold fast (its own tail), and taking an aeroplane back on its
landing roll does not finish the landing (its own tail).

**The check.** `the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief`
recorded where each aeroplane touched and checked only how far off the
centreline; it now also fails a touch before the threshold or beyond the
runway's 3,000 metres. `the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief`
now records the lander's touchdown point and checks the same, and the
centreline within ten metres. **Seen to fail** on main's lander: the F-35B's
circuit touched 2,073 m short of the threshold at 165 knots, and her approach
lesson - started trimmed on the path - 142 m short. Every other aeroplane
passed both.

**The cause: the approach autopilot's pitch limit, below what the F-35B needs
on the path.** At her landing weight and reference speed (159 knots) she trims
to 16.5 degrees of pitch and 19.5 of alpha on a three-degree glidepath; the
lander limited the attitude it asks for, and the one it learns, to fifteen.
Held below her path attitude she sank under the glidepath, and with her nose
at the limit nothing brought her back: from the circuit she was handed the
approach about 700 ft under the path at 5.8 nm, sank at a steady 630 ft/min
with the pitch pinned at 15, and her six knots over the reference wound the
throttle back as fast as the sink opened it. The limit (`src/sim/lander.cpp`,
`most_path_pitch_deg`) is now twenty. No other aeroplane reaches fifteen: every
other aeroplane's touchdown, stop and final speeds are to the metre and the
knot what they were. (A first try that let the speed run up while the nose was
at its limit put her on the runway but at 172-178 knots on final, outside the
lessons' bands; it was not kept.)

**Where each touches down**, metres beyond the threshold and right of the
centreline, on the circuit and on the approach lesson (release build, Linux):

| aeroplane | circuit | approach lesson |
|---|---|---|
| 737-300 | 323, -0.6 (135 kt) | 327, -0.7 |
| 787-8 | 231, -2.4 (147 kt) | 239, -0.8 |
| A320 | 281, -0.8 (145 kt) | 282, -0.8 |
| A380 | 264, +1.9 (136 kt) | 270, -0.7 |
| B-2A | 296, -0.6 (139 kt) | 295, -0.7 |
| C172P | 313, -0.2 (58 kt) | 313, -0.2 |
| C182 | 312, -0.5 (60 kt) | 308, -5.1 |
| F-15C | 256, -6.1 (194 kt) | 262, -1.7 |
| F-35B | **254, -1.5 (157 kt)**, was -2,073 | **255, -1.1**, was -142 |
| J-3 Cub | 289, -0.0 (42 kt) | 290, -0.0 |
| Learjet 35A | 710, -0.5 (102 kt) | 712, -0.5 |
| Mosquito FB.VI | 710, +2.6 (93 kt) | 710, +2.7 |
| PA-28 | 336, +0.7 (58 kt) | 336, +0.7 |
| Short S.23 | 245, -1.9 (87 kt) | 254, +2.6 |

The F-35B's circuit now flies final at 159-160 knots, where it flew 165-166.
The 747-400 and the F-22A fly neither lesson, having no reference speed, as
both tests already name.

**Verified**: the circuit tests (the lesson, its demonstration and the low
downwind fault), the approach tests (the lesson, its demonstration and the fast
approach) and the lander's own tests pass on Linux (release build) and on
Windows.
