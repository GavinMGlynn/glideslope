### The AI flares to its wheels and touches down within what its gear takes, 2026-10-01 — tail done

**On main after #73, #80 and #82** (2026-10-02), the table below is this
branch rebased on them. #82 gave the F-15C NASA's pitching moment: she
flies the approach at about 10.8 degrees of incidence with her stabilator
to spare, so the incidence test's naming of her as unable to be pulled past
her path and four went stale (she is pulled to 20.2) and is gone. But she
touched at 158 knots, 12.5 degrees nose up, still rotating, and rose 2.4 ft
(approach), 2.5 (circuit) and 3.4 taken back at the touch. Found, step by
step in the approach lesson:
- **The flare's nose-up trim was carried into the lowering** and held her
  elevator at neutral: her nose stayed at 12.7 degrees, her wing lifting
  1.1 times her weight. A jet's trim now starts again from none at the
  touch, and the lowering's target two degrees below the touch - PR #73's
  investigation of the B-2A first found both. Alone: 1.4 ft.
- **The lowering's damping fought the lowering**: it damped the whole pitch
  rate, so she came down at 3.5 degrees a second against 4.4. It damps the
  difference from the rate wanted now. With it: 0.7.
- **She met the runway still rotating** at half a degree a second. In the
  last fifth of the flare's height the attitude is now held, not raised.
  With it: 0.4 (approach), 0.3 (circuit), 0.9 taken back at the touch
  (bound 3). **Seen to fail** without the hold: 1.19 and 1.22 ft. The light
  aeroplanes touch a little firmer for it (the C172P at 192 ft/min where
  139, under the 300 their own test allows).

**What is not done first.**
- **Jets touch down further along the runway than they did**: the 737-300
  at 615 m past the threshold where it touched at 327, the F-15C at 809
  where 262, the A380, B-2A, Learjet and F-35B at 520 to 633. Every one is
  on the runway and stopped on it; none is measured against a touchdown
  zone, and nothing yet says how far along a jet should touch - an F-15C
  touching at 800 m would run off a 6,000 ft runway. A tail.
- **The F-15C balloons in her flare**: she climbs at about 100 ft/min at
  five feet before settling on to the runway at 416 ft/min - the hardest of
  any, inside the 600 her gear takes. A tail.
- **The F-35B lands on her power.** Her model flies its glidepath at 19.5
  degrees of incidence and its flare's nose reaches her tail-strike bound
  with her still sinking at 800 ft/min; the flare opens the throttle to
  arrest it, and she touches at 92 ft/min (approach lesson), 195 (circuit).
  That is how her model, not how an F-35B, lands. A tail.
- **The touchdown sink each aeroplane is flared to is set, not published**:
  200 ft/min for every jet, 40 otherwise (below). A tail.
- **The AI does not go around from a balloon.** A Mosquito given back from
  a pilot's flare pulled 0.4 of the stick harder zooms to some fifty feet on
  the pilot's stick and comes down at 958 ft/min; she is named and not
  judged in the hand-back test below. A tail.
- **The selftest hash does not move** (`d36123c1eecc3e23`, linux-release):
  the selftest replays a pilot's input log and no lander flies in it.

**What was wrong.** The flare was flown to the height of the centre of
gravity, from a height of a quarter of a foot a knot capped at thirty, to a
sink of forty feet a minute at the runway's height, with the nose stopped at
twelve degrees of incidence and the power closed from its first step.
- A 787's main wheels hang 15 ft below her centre of gravity, an A380's 16
  and a B-2A's 11 (a Cessna's 4): they met the runway with a third of the
  flare still to fly. The 787 touched sinking at 708 ft/min, past the 600
  (10 ft/s) the server's rule judges any gear to take (`sim/crash.hpp`).
  The B-2A touched still rounding out, her wing lifting more than her
  weight, and rose 1.9 ft after.
- The F-15C comes down her glidepath at 196 knots and 1,050 ft/min: from
  thirty feet her nose could not come up in time, and she touched at 978.
- The F-35B flies her glidepath at 19.5 degrees of incidence, over the
  twelve, so her nose never came up at all: 883 ft/min.
- Once raised the nose was never let down, so a flare that had arrested its
  sink early floated.

**What changed** (`sim::Lander`, `lander.cpp`):
- **The flare is flown to the main wheels' height.** The lander finds the
  main wheels in the model's contacts as `Aircraft::stance` does (the
  lowest wheels off the centreline) and works the lowest one's height from
  the centre of gravity's (`inertia/cg-x-in`, `cg-z-in`) at her pitch and
  bank. The flare begins when the wheels, not the centre of gravity, reach
  its height, and the sink it asks for falls with their height.
- **It begins high enough to round out in**: the higher of the quarter of
  a foot a knot and the height in which the glidepath's sink at the
  reference speed is arrested at a tenth of a g - 47 ft for the F-15C, 26
  for the 787, four for a Cessna, whose quarter of a foot a knot is the
  more.
- **The sink it brings the wheels to is the aeroplane's, from its
  figures**: `touchdown_fpm` on `published_figures`, read into
  `ApproachSpeeds::touchdown_fpm`. Forty where none is given, as before;
  200 for the ten jets' figures, flown on to the runway rather than held
  off to float (the FAA's Airplane Flying Handbook, FAA-H-8083-3C, chapter
  16) - a third of the 600 the gear takes. It is not a published figure,
  and each file says so. One not above 0, or not under 600, is refused. Flared to
  forty at their wheels in the trials, the jets floated: the B-2A 2.5 km,
  the A380 1 km.
- **The nose is let down in a float**, two degrees a second while she sinks
  slower than wanted, as far as the attitude the flare began at (the
  handbook's chapter 9, "Floating"). In the trials without it the B-2A,
  flared to 200 ft/min, still floated 1.8 to 2 km.
- **The incidence guard limits, not prevents**: twelve degrees, or four
  more than the glidepath was flown at, whichever is higher - and above
  twelve, two degrees short of the stall the reference speed implies: the
  greatest lift is 1.3^2 = 1.69 times the path's, which a lift rising in
  proportion from a zero-lift incidence at or below zero reaches at no less
  than 1.69 times the path's incidence (`Lander::most_flare_alpha_deg`).
  The path's incidence is learnt on every step before the flare, and while
  a pilot flies her (`watch`) until her wheels first reach the flare's
  height, and kept when she is given back (`resume`).
- **A wing over the limit is brought back under it**: the flare's attitude
  comes down by as much as the incidence is over, and never stays above
  the tail-strike bound - as she can be when given back from a pilot's
  flare.
- **The power comes off as the sink is arrested**: held while she sinks
  more than 300 ft/min faster than the flare wants, and once the nose is
  within a degree of its bound or past the incidence guard, opened in
  proportion from where the flare found it - the path flown on the power,
  as the approach flies it on the back of the drag curve.
- **The flare's attitude is flown three times as firmly** as the path's: at
  the path's gain a jet's nose lagged the flare's attitude by two to three
  degrees all the way down.
- **`still_landing`'s height** is no less than ten feet over where the
  flare begins, centre of gravity over the wheels: the F-15C's begins at
  52 ft, and the pilot's flare she was taken back from was let go at 50.
  The flare begins by height over the runway's elevation and this reads
  height over the ground beneath; the ten feet are for ground under the
  approach a little lower than the runway. Not seen to fail without them:
  every test's ground is level with the runway.
- **From the review**: the nose is let down in a float only while she sinks
  slower than wanted; the flare's attitude gain comes in over a second
  rather than stepping from one to three; the throttle closes on the
  runway over at most a second, not in a step from the power the flare may
  have added; the approach's near-ground throttle band is measured from the
  wheels against twice the flare's own height.
- Tried and not kept: PR #73's trim dropped and nose target lowered at a
  jet's touch. With the flare to the wheels nothing needs it (the B-2A rose
  0.0 ft without it, on main's model and on #73's), and taken out no test
  went red.

| Approach lesson (circuit) | Touched, ft/min, before | After | Rose after the touch, ft, before | After |
| --- | --- | --- | --- | --- |
| 737-300 | 333 | 134 (118) | 0.0 | 0.0 (0.0) |
| 787-8 | 708, wrecked | 325 (345) | 0.0 | 0.0 (0.0) |
| A320 | 534 | 104 (28) | 0.1 | 0.0 (0.0) |
| A380 | 525 | 225 (249) | 0.0 | 0.0 (0.0) |
| B-2A | 240 | 247 (246) | 1.9 | 0.0 (0.0) |
| C172P | 150 | 192 (192) | 0.0 | 0.0 (0.0) |
| C182 | 140 | 218 (211) | 0.0 | 0.0 (0.0) |
| F-15C | 978, wrecked | 469 (438) | 0.0 | 0.4 (0.3) |
| F-35B | 883, wrecked | 53 (135) | 0.0 | 0.0 (0.0) |
| J-3 Cub | 142 | 132 (132) | 0.0 | 0.0 (0.0) |
| Learjet 35A | 409 | 167 (167) | 0.0 | 0.0 (0.0) |
| Mosquito FB.VI | 364 | 114 (112) | 0.0 | 0.1 (0.1) |
| PA-28 | 132 | 201 (201) | 0.0 | 0.0 (0.0) |
| Short S.23 | 314 | 294 (341) | 0.0 | 0.0 (0.0) |

The sink is the centre of gravity's on the step before the touch, as the
server's rule takes it. With PR #73's drag rudders merged in (locally, not
pushed, before the review's changes) the B-2A crossed the threshold at
124.1 knots against 124.0, touched at 232 ft/min and rose 0.0 ft, and #73's
tests passed.

**The check.** `test::AfterTouch` (`tests/unit/after_touch.hpp`) now judges
every step it watches by the server's own `sim::GroundJudge`, keeps the
sink at the touch, and `how_the_gear_took_it` names a wreck or a rise of
more than a stated bound after the touch.
`the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief` and
`the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief` put every
aeroplane they land to it with half a foot (`settled_within_ft`): both
already assert fourteen of fourteen landed, and name the 747-400 and F-22A
as left out for having no reference speed. The light aircraft's
`every_light_aircraft_is_flown_down_a_glidepath_and_lands_in_calm_air` and
`..._in_a_ten_knot_crosswind` do the same, four of four. The take-back
tests already judged the AI's touch from the take-back by the crash rule and
are unchanged. `the_sink_a_flare_touches_down_at_is_read_and_refused_past_what_the_gear_takes`
(new) reads the 737-300's 200 and the C172P's none, reads 599 back, and
refuses 0, -40, 600 and 900, each naming `touchdown_fpm`. `AfterTouch` is
told by each test whether she alights on water (the catalogue's `seaplane`,
as the server is), and refuses to watch until it is.

Two new tests, from the review:
- `the_flares_incidence_limit_is_the_approachs_and_a_pilots_flare_does_not_move_it`:
  every aeroplane taught the approach (14 of 14) is flown to the flare's
  first step, a pilot pulls the stick fully back for a second with the
  lander watching, and gives her back. The pilot's flare takes every wing
  past the path's incidence and four (asserted: the C172P to 21.2 degrees,
  the F-35B to 30.8), and the limit is unmoved and is what the path gives -
  12 for all but the F-35B, whose is 23.5. **Seen to fail** with the limit
  taken from her incidence at `resume` (the reviewer's case): 13 of 14
  moved, the 737-300 to 14.4, the A320 to 16.2.
- `a_flare_the_pilot_pulls_hard_and_hands_back_in_the_air_is_landed_without_a_stall`:
  every landplane taught the approach: on the flare's first step the pilot
  takes her and flies the AI's own flare with the stick 0.4 further back,
  and gives her back in the air after a second (or with her wing two
  degrees over its limit). From the take-back her incidence goes no higher
  than it was given back at or its limit, with a degree's overshoot; she
  touches unwrecked, upright and settled, and stops on the runway. 12 of 13
  judged; the Mosquito is named and not judged (above), and the flying boat
  left out. Before the wing was brought back under its limit, and in
  harsher forms of the test - the stick held fully or half back, or the
  pilot's flare held for a second and a half - the AI could not recover
  several (the 737-300, A380, B-2A, C182, F-15C and Learjet came down at
  600 to 1,400 ft/min): it does not recover a badly botched flare, which is
  the go-around tail. Not seen red against the limit taken at `resume`:
  given back after a second, few wings are over twelve degrees - the test
  above is the one that shows that.

**Seen to fail.** On the old law the approach lesson failed with the 787-8,
F-15C and F-35B wrecked at 708, 978 and 883 ft/min and the B-2A risen
1.86 ft. With the wheels' hang made 0 as a deliberate bug (the flare flown
to the centre of gravity again, everything else kept), both lessons failed:
the 787-8 wrecked at 671 and 672 ft/min, the F-15C and F-35B risen 0.7 to
1.0 ft, the Mosquito bounced 3.7 ft; restored, both pass.

(The hang made 0 was watched before the review's changes.)

**Verified again** on main after #73, #80 and #82 (linux-release): 202
tests - the same, with #73's B-2A and speedbrake tests and #80's mixture
tests - none failed, one skipped (Windows's own); the B-2A crosses at 124.1
knots against 124.0 and rises 0.0 ft, so #73's item is ticked; the selftest
hash is `d36123c1eecc3e23`.

**Verified** in linux-release, rebased on main: 184 tests - every lesson,
landing, approach, circuit, take-back, hand-over, no-step, learnt-landing,
stall, autopilot, plan, crash, figures and selftest test, the window and
live-model ones aside - none failed; one skipped, Windows's own. (Before the
review one run had
`a_player_takes_over_an_ai_aircraft_with_no_step_at_200_ms_and_a_players_is_refused`
miss its 10 m bound at 12.55 m under load, the open tail of a server behind
real time; it passed three of three alone and in this run.)
