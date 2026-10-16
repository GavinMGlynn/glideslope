### The AI touches down in the touchdown zone, flares without ballooning and goes around from a balloon; the A320 taken at the touch held to three feet, 2026-10-06 — four tails done

**What is still open first.** The F-35B still lands on her power (her
model's glidepath incidence, 19.5 degrees, is her model's, not a published
one); the touchdown sink each aeroplane is flared to is still set, not
published; an aeroplane landed by hand with no approach given to the AI is
still handed the plain autopilot on its roll; a pilot still has no
speedbrake control. A go-around climbs straight ahead at the landing flap to
500 ft over the runway and is then the plain autopilot's: it is not flown
round a circuit to another approach, and nothing yet goes around from a
landing that is merely long or fast - only from a balloon.

**The touchdown zone.** Every aeroplane the approach and circuit lessons
land (fourteen each) must now touch inside the first 3,000 ft (914 m) of
the runway from its threshold - the touchdown zone as the FAA's
Pilot/Controller Glossary defines it - and not short of the threshold
(`inside_the_touchdown_zone`, `tests/unit/test_lesson.cpp`). They touch 246
to 725 m along it (the Mosquito the furthest, 723 and 725);
the 520 to 809 m of 2026-10-01 had already come down with #82's F-15C (now
282 m), and the flare below brought the rest in further. **Seen to fail**:
with the glidepath aimed 900 m along rather than 300, ten of the fourteen
touched 924 to 1,324 m along and were named.

**No aeroplane climbs in its flare.** The flare watch (`FlareWatch`) keeps
her greatest climb from the flare's first step to the touch, and more than
10 ft/min is named as a balloon in both lessons. On this branch's base the
F-15C no longer ballooned (most climb -467 ft/min), but three others did:
the 737-300 at 25 ft/min at five feet, the A380 at 11 at 19 ft, and the
Mosquito at 70 at 20 ft. **Seen to fail** there, before the change below.
- **The flare flies the sink three tenths of a second ahead** (`lander.cpp`):
  the sink's trend, smoothed over a quarter of a second, is added to it.
  The path follows the nose a second or so behind, so a nose raised until
  the sink was arrested went on arresting it: the Mosquito's sink fell from
  650 ft/min to a climb of 70 at twelve feet with her nose still rising at
  two degrees a second. A quarter to four tenths of a second ahead gives
  the same clean result in both lessons; six tenths and a whole second
  raise the nose too little, and the Mosquito met the runway flat and fast
  and bounced three to seven feet.
- **Only a jet's attitude is held in the last fifth of the flare's
  height** (the hold put in for the F-15C's rotation at the touch): a
  propeller aeroplane's nose is raised to the touch, as the FAA's Airplane
  Flying Handbook (FAA-H-8083-3C, chapter 9) rounds out. Held, the
  Mosquito, no longer ballooning, met the runway at 107 knots sinking 120
  ft/min against the 40 wanted, and bounced four feet.

| Approach lesson (circuit) | Touched, ft/min | Most climb in the flare, ft/min | Touched along, m | Rose after, ft |
| --- | --- | --- | --- | --- |
| 737-300 | 46 (102) | -46 (-100); was +25 | 418 (386) | 0.00 (0.00) |
| 787-8 | 343 (360) | -341 (-358) | 267 (257) | 0.00 (0.00) |
| A320 | 165 (134) | -164 (-133) | 343 (350) | 0.00 (0.00) |
| A380 | 107 (129) | -58 (-45); was +11 | 459 (467) | 0.00 (0.00) |
| B-2A | 193 (194) | -136 (-138) | 434 (433) | 0.00 (0.00) |
| C172P | 131 (131) | -131 (-131) | 333 (333) | 0.00 (0.00) |
| C182 | 157 (161) | -157 (-156) | 320 (324) | 0.00 (0.00) |
| F-15C | 479 (449) | -477 (-447) | 281 (284) | 0.19 (0.18) |
| F-35B | 47 (45) | -45 (-16) | 483 (521) | 0.00 (0.00) |
| J-3 Cub | 92 (92) | -92 (-92) | 306 (306) | 0.00 (0.00) |
| Learjet 35A | 118 (120) | -103 (-108) | 488 (484) | 0.00 (0.00) |
| Mosquito FB.VI | 60 (60) | -24 (-24); was +70 | 723 (725) | 0.00 (0.00) |
| PA-28 | 133 (132) | -133 (-132) | 343 (344) | 0.00 (0.00) |
| Short S.23 | 299 (317) | -298 (-315) | 254 (246) | 0.00 (0.00) |

The most climb is the least sink from the flare's first step to the touch,
so for most it is the sink at the touch.

**A balloon is gone around from.** A new stage, `go_around`: in the flare,
in the air, climbing, with her wheels higher than the flare's own height,
the lander goes around, as the handbook (chapter 9, "Ballooning" and
"Bouncing During Touchdown") has an excessive balloon or a severe bounce
gone around from at once. Full power over a second; the wing flown at the
incidence the glidepath was flown at (learnt before the flare, and while a
pilot flew it), so the climb is the power's and the wing is never near the
stall however slow the balloon left her; the flap and gear left as they
are, the speedbrake in. At 500 ft over the runway (`Lander::go_around_ft`)
the landing is given up and the controller engages the plain autopilot from
the controls the go-around left, which steps nothing. `still_landing` is
false in a go-around, so a pilot who takes her then is not given the
landing back.
`a_flare_the_pilot_pulls_hard_and_hands_back_in_the_air_is_landed_without_a_stall`
no longer names the Mosquito: every landplane (13 of 13) given back from the
pilot's over-pulled flare either lands as before or goes around - never
touching, its wing under the same incidence bound - and at least one must
go around. The Mosquito, given back at 27 ft climbing, goes around and is
never lower than 24.7 ft; the other twelve land. **Seen to fail** with the
go-around never begun: the Mosquito's wing went to 14.0 degrees, past 13,
and she was wrecked sinking at 967 ft/min.

**The A320 taken at the touch** rose 4.1 ft after the take-back on
2026-09-27, and the take-back test named a bound of 4.5 ft for her. She now
rises 0.8 ft (the F-15C 0.7, the rest 0.1 or less), with the flare flown to
her wheels (2026-10-01) and to the sink ahead, and the named bound is gone:
every landplane taken back at the touch is held under three feet. Not seen
red again on this branch: the bound is the one every other aeroplane in the
test has been held to since 2026-09-27, when it was seen red on all of them.

**The selftest hash does not move** (`d36123c1eecc3e23`, linux-release): no
lander flies in it.

**Verified** (linux-release): the approach and circuit lessons, the three
take-back tests, the hard-flare hand-back, the flare's incidence limit, the
light aircraft's calm and crosswind landings, the B-2A, F-15C and fast
approach tests, the learnt-landing tests, and every test matching circuit,
approach, demonstration, selftest, hand-over, flare and controller.
