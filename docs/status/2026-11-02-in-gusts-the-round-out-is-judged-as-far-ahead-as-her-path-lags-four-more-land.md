### In gusts the round out is judged as far ahead as her path lags her nose: the 747-400, A320, Learjet 35A and Mosquito land on every seed, 2026-11-02 - item still open

**What is still missing, first**: the item. Nine of fifteen are still named
in `not_yet_landed_in_gusts()` (tests/unit/test_lesson.cpp): the 737-300 and
787-8 no longer balloon but come down hard on a few of forty seeds; the
A380 comes down hard and floats past the touchdown zone; the F-15C comes
down hard; the B-2A puts a wingtip down; the Cub still balloons; the C172P,
C182 and F-35B for touches named before on other platforms or seeds. The
change is not yet seen on Windows or macOS (CI).

**What now works**: in moderate turbulence the AI lands six of fifteen
within limits, each with its margin, on all eight gust seeds: the PA-28 and
the flying boat as before, and now the 747-400, A320, Learjet 35A and
Mosquito, unnamed. Over forty seeds each of those four passes all forty.
Calm air is untouched (the change is gusty air's alone), and so is the
flying boat, whose rough-water hold-off is not the landplanes' flare.

**How a pilot handles a balloon** (sources, each quoted as printed):

- The FAA's Airplane Flying Handbook (FAA-H-8083-3C), ch. 9, "Ballooning
  During Round Out", p. 9-32: "If the pilot misjudges the rate of sink during
  a landing and thinks the airplane is descending faster than it should,
  there is a tendency to increase the pitch attitude and AOA too rapidly.
  This not only stops the descent, but actually starts the airplane
  climbing. This climbing during the round out is known as ballooning." And
  p. 9-33: "Depending on the severity of ballooning, the use of throttle is
  helpful in cushioning the landing ... throttle should be closed
  immediately after touchdown", and "When ballooning is excessive, it is
  best to execute a go-around immediately".
- The same chapter, "Round Out", p. 9-8: "the round out should still be
  executed at a rate that takes the airplane's downward motion into
  account"; p. 9-9: "Once the actual process of rounding out is started, the
  pilot should not push the elevator control forward. If too much
  back-elevator pressure was exerted, this pressure is either slightly
  relaxed or held constant, depending on the degree of the error. In some
  cases, it may be necessary to advance the throttle slightly".
- The same chapter, "Turbulent Air Approach and Landing", p. 9-20: "The
  proper approach attitude and airspeed require a minimum round out and
  should result in little or no floating during the landing."
- Airliners: Airbus's Safety First, "A Focus on the Landing Flare", cites
  the FCTM: once the flare is begun the pilot flying avoids nose-down
  inputs and may release the back stick slightly; an ATSB report on a 737
  hard landing (AO-2023-010) cites Boeing's FCTM bounce recovery: hold or
  re-establish the normal landing attitude and add thrust as necessary. The
  FCTMs themselves were not found to quote, and these two were found by
  search and their pages not read whole.

(Chapter 9 read from the FAA's PDF as hosted at faasafety.gov, its pages as
its footers print them.)

**What the AI was doing**: traced on the 737-300's seed 2 (climb 139 ft/min
here, 113 in the named list), her flare began at 35 ft sinking 850 ft/min;
her nose came up to 5.2 degrees, about two more than the 175 ft/min wanted
needed, because the sink was judged three tenths of a second ahead and her
path follows her nose by about a second. The sink was arrested at six feet
and she went on up, with her nose already coming down, at 139 ft/min. That
is the handbook's cause - the pitch increased too rapidly for the sink - and
the cure is its p. 9-8: a round out flown on the downward motion still to
come. The A380 (seed 2) showed the same: her nose at 4.4 degrees when the
sink was gone, climbing to 176 ft/min with it coming down.

**The change** (src/sim/lander.cpp, `gusty_lead_s_per_kt`): in gusts the
flare judges the sink as far ahead as her path lags her nose - her speed
over g times her wing's incidence from no lift, taken as eight degrees:
0.0075 s a knot, a second at an airliner's 135 kt, half that at a
Cherokee's 70, never less than calm air's 0.3 s. The power's lead (its
spool-up, or this, whichever is longer) and the nose's let-down when she is
seen to be about to climb (`ahead_climb_fpm`) use the same lead. Calm air
still judges 0.3 s ahead.

**Tried and measured (linux-release, eight seeds; then forty)**:

| Variant | Passing 8/8, of fifteen | What went wrong |
|---|---|---|
| As it was | 7 (737, A380, F-15C 4/8) | balloons: 737 155, A380 145 ft/min |
| Nose capped at the approach attitude plus 1.5 times the path's change | 6 | A380 balloons 178; F-35B, Mosquito touch at 392, 411 |
| Same, 1.0 times | 4 | 787-8 touches at 558, F-15C 442 |
| Attitude held in a predicted climb, not let down (the AFH's literal "held constant") | 7 | 737 climbs 312, A380 313: held, the gust's lift and ground effect climb her further |
| Let down 1 or 2 deg/s instead | 6 | 737 305, A380 302 |
| Lead 0.5 s | 10 | 737 87, A380 106 |
| Lead 0.7 s | 12 | A380 31, 737 59 |
| Lead 0.85 s | 12 | A380 goes around (twice), F-15C hard |
| Lead 1.0 s, every aeroplane | 12 | over forty: a PA-28 at 359 ft/min (seed 21), so not taken |
| Lead 1.2 s | 12 | A380 wrecked (seed 3), hard (seed 1, 414) |
| Lead 1.5 s | 9 | 747-400 357, 787-8 342, F-35B 339: the nose too little |
| **Lead 0.0075 s/kt (taken)** | **12** | A380, F-15C, B-2A |

The existing let-down of the nose when she is seen about to climb (to no
lower than the flare began at - the back pressure the round out added,
relaxed) is kept: holding the attitude instead, as the AFH's p. 9-9 reads
literally, climbed the jets twice as fast, since their climbs come from a
gust's lift and the ground's effect on a wide wing with the nose still up.
The cure for the balloon is not to raise the nose past what the sink
needs, and the let-down remains a relaxing of the round out's back
pressure, not a push below the attitude the flare began at.

**Forty seeds, as it was against the change** (pass with every margin):

| Aircraft | Before | After | Worst after |
|---|---|---|---|
| 737-300 | 27 | 37 | touches 490, 406, 522 (seeds 36, 38, 40); climbs 19 at most |
| 747-400 | 35 | 40 | sink 326 |
| 787-8 | 34 | 36 | touches 344 to 379 on four (15, 21, 25, 40) |
| A320 | 37 | 40 | sink 312 |
| A380 | 22 | 33 | 393 (seed 1), 710 (18), around from past the zone (6, 7, 25) |
| B-2A | 32 | 32 | wingtip |
| C172P | 40 | 40 | sink 334 |
| C182 | 38 | 38 | 380, 378 (seeds 23, 33), as before |
| F-15C | 22 | 30 | 336 to 429 |
| F-35B | 39 | 39 | 421 (seed 40), was 412 |
| J-3 Cub | 35 | 35 | climbs 135, 132 (seeds 20, 37); rises 0.81 (39) |
| Learjet 35A | 40 | 40 | sink 294, climb 6 (was 62) |
| Mosquito | 39 | 40 | sink 278 |
| PA-28 | 40 | 40 | sink 321, was 310 |
| S.23 | 32 | 32 | untouched (her hold-off is not this flare) |

**Made worse, named with figures**: no judged aeroplane fails a seed it
passed. Seeds that passed before and fail now, all of named aeroplanes:
737-300 seed 38 (283 to 406 ft/min); 787-8 seeds 15, 21, 25 (156, 150, 136
to 361, 344, 360); A380 seeds 6 (around from 1,006 m), 14 and 34 (wrecked);
F-15C seeds 6, 14, 25, 28 (to 336, 398, 373, 344); B-2A seeds 10, 23, 33,
38, 39 (wingtip, against five others it no longer puts down); Cub seed 39
(rises 0.81 ft). The PA-28's worst touch of forty went from 310 to 321
ft/min, inside its margin.

**Verification**: `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1`
to `_8` pass in linux-release and in an `-march=x86-64-v2` build (no FMA,
as CI), whose outputs are identical line for line; least margins of those
judged over the eight seeds: sink 47 ft/min (seed 5), zone 176 m, rise 0.39
ft, flare climb 105 ft/min. The full linux-release suite (`-LE nightly`) is
recorded in the pull request.
