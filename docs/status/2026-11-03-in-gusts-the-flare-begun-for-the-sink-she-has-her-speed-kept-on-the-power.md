### In gusts the flare is begun for the sink she has and her speed kept on the power: the F-15C and F-35B land on every seed, 2026-11-03 - item still open

**What is still missing, first**: the item. Seven of fifteen are still named
in `not_yet_landed_in_gusts()` (tests/unit/test_lesson.cpp): on forty seeds
the 737-300 comes down hard on one (462 ft/min, seed 40), the 787-8 on three
(352 to 377), the A380 on one (350), touches past the zone on one and
strikes her airframe on one; the B-2A puts a wingtip down on ten; the Cub
balloons on two and touches at 356 on one; the C182 touches at 377 on two;
the C172P for macOS release's seed 3. The F-15C, unnamed here, goes around on
one seed of forty (34) from a float that would touch past the zone, and her
worst touch of the eight (seed 7, 332 ft/min) is 28 inside her limit: other
platforms are not yet seen.

**What now works**: in moderate turbulence the AI lands eight of fifteen
within limits, each with its margin, on all eight gust seeds: the PA-28, the
flying boat, the 747-400, A320, Learjet 35A and Mosquito as before, and now
the F-15C and F-35B, unnamed. Over forty seeds the F-35B passes all forty
(was 39), the F-15C 39 (was 30). Calm air and the crosswinds are untouched:
both changes are gusty air's alone, and a hull's hold-off is not changed.

**What the AI was doing** (traced on linux-release, the hardest touches):

- **Her power off and her speed gone**: the F-15C on seed 32 began her flare
  at 56 ft and 167 kt; the gusty flare eased her throttle to half what it
  found (0.13) and held it there, and over six seconds she bled to 138 kt.
  From 15 ft her wing was at the flare's incidence limit (14 degrees), her
  nose could come up no further, and her sink grew from 310 to 429 ft/min to
  the touch. The F-35B on seed 40 bled from 160 to 128 kt and met the runway
  at 421. The A320, whose throttle stayed high (0.36), lost three knots and
  passes every seed.
- **The flare begun at the path's height for a sink past the path's**: the
  height the gusty round out begins at is worked from her path's sink at her
  approach speed. The 737-300 on seed 40 met her flare's height (40 ft)
  sinking 790 ft/min after a down-gust and was still sinking 790 at 14 ft; the
  A380 on seed 1 entered hers sinking 1,214 ft/min against her path's 785 and
  met the runway at 393.

A lull at the round out is the second's cause, seen in the sink as it
begins; the half-gust additive was flown down the approach and lost in the
flare (the first).

**The published technique** (each quoted as printed):

- The FAA's Airplane Flying Handbook (FAA-H-8083-3C), ch. 9, "Turbulent Air
  Approach and Landing", p. 9-20: "For landing in turbulent conditions, the
  pilot should use a power-on approach at an airspeed slightly above the
  normal approach speed"; "Pilots often use the normal approach speed plus
  one-half of the wind gust factors in turbulent conditions"; and "Use an
  adequate amount of power to maintain the proper airspeed and descent path
  throughout the approach, and retard the throttle to idling position only
  after the main wheels contact the landing surface. Care should be exercised
  in closing the throttle before the pilot is ready for touchdown. In
  turbulent conditions, the sudden or premature closing of the throttle may
  cause a sudden increase in the descent rate, resulting in a hard landing."
  (Read from the chapter's pages 9-20 to 9-23 as the FAA's own PDF, hosted at
  faasafety.gov, prints them.)
- The same chapter, "Round Out", p. 9-8 (as quoted in the 2026-11-02 file):
  "the round out should still be executed at a rate that takes the
  airplane's downward motion into account".
- Airliners: Boeing's 737 FCTM is cited on PPRuNe and AVSIM as "bleed off
  airspeed in excess of VREF + 5 knots + gust correction when below 200 feet
  AGL. Maintain the gust correction to touchdown", and as reducing the
  thrust levers smoothly from the flare's start to reach idle at the touch.
  These are forum quotations; the FCTM itself was not found to quote.

**The change** (src/sim/lander.cpp, gusty landplanes alone):

1. **Her speed kept on the power** (`gusty_speed_throttle_per_kt`,
   `gusty_held_kts`): the gusty flare's throttle never falls below half what
   the flare found, as before, and below her approach speed less 5 kt it is
   opened 0.05 a knot, at no more than 0.5 a second - the gust additive
   kept to the touch, as the FCTM is quoted, with 5 kt let go in the flare.
   The floor holds after the cushion's take-off too. All three figures
   (0.05 a knot, 5 kt, 0.5 a second) are tuned on the sweep below, not
   sourced; the rate is the one the flare already closes the throttle at.
2. **The flare begun for the sink she has** (`begin_flare_ft`): in gusts the
   flare begins at the higher of the path's height and the height that
   arrests the sink she has at the same 0.07 g, for no more sink than
   1,000 ft/min: the Flight Safety Foundation's ALAR Briefing Note 7.1 has
   a stabilized approach sink no more than 1,000 ft/min, so a sink past
   that is an approach gone wrong, not a reason to begin higher still. So
   the flare never begins higher than about 62 ft, or the path's own height
   where that is higher (the F-15C's 49 ft is the highest). The balloon
   go-around above the flare is judged against the height it began at.
   **The cap, measured**: uncapped, the highest flare of the 600 began at
   90.6 ft (the A380, seed 25, sinking 1,216 ft/min against her path's
   35.9 ft); 20 began above one and a half times the path's height, 6
   above twice it. Capped, the highest is 61.7 ft. It changes 14 landings'
   figures and two outcomes, both for the better: the A380 passes seed 13
   (338 down to 221 ft/min) and touches seed 18 at 350 (was 397), still
   hard. Nothing passing before fails.

**Tried and measured** (linux-release, forty seeds, passing every margin;
the trial dropped the S.23, whose hold-off is not this):

| Variant | 737 | 787 | A380 | F-15C | F-35B | Worse |
|---|---|---|---|---|---|---|
| As it was | 37 | 36 | 33 | 30 | 39 | - |
| Speed held to the approach speed, 0.02 a knot | 37 | 36 | 31 | 38 | 40 | A380 floats (21, 37 struck) |
| Same, 0.05 a knot | 36 | 37 | 29 | 38 | 38 | A380, 747 (2), Cub |
| Held to 5 kt under it, 0.03 | 37 | 36 | 33 | 39 | 40 | - |
| Held to 7.5 kt under it (Vref), 0.05 | 37 | 36 | 33 | 38 | 40 | - |
| Held to 5 under, 0.03; begun for her sink | 39 | 37 | 36 | 38 | 40 | B-2A 32 to 30 |
| Held to 5 under, 0.03; begun higher by her path's lag at her path's sink | 39 | 38 | 35 | 28 | 37 | F-15C floats, 747 36, Mosquito |
| Held to 5 under, 0.03; begun for her sink plus a quarter of her lag | 39 | 39 | 36 | 37 | 39 | 747 39, A320 39 |
| Held to 5 under, 0.03; begun for her sink at 0.06 g | 40 | 40 | 32 | 35 | 40 | 747 37, A320 39 |
| Same, at 0.065 g | 39 | 39 | 36 | 36 | 40 | 747, A320 39 |
| Begun for her sink alone, no speed held | 39 | 37 | 36 | 28 | 39 | F-15C as before |
| Held to 5 under, 0.05; begun for her sink | 39 | 37 | 36 | 39 | 40 | B-2A 32 to 30 |
| **Same, begun for no more than 1,000 ft/min (taken)** | **39** | **37** | **37** | **39** | **40** | **B-2A 32 to 30** |

Begun for her sink at 0.06 g landed the 737-300 and 787-8 on all forty but
put the 747-400 and A320, judged, down on seeds they passed: not taken.

**Forty seeds, as it was against the change** (pass with every margin):

| Aircraft | Before | After | Worst after |
|---|---|---|---|
| 737-300 | 37 | 39 | 462 (seed 40) |
| 747-400 | 40 | 40 | sink 314 |
| 787-8 | 36 | 37 | 352, 358, 377 (21, 25, 40) |
| A320 | 40 | 40 | sink 306 |
| A380 | 33 | 37 | 907 m (7), 350 (18), struck (34) |
| B-2A | 32 | 30 | wingtip on ten |
| C172P | 40 | 40 | sink 327 |
| C182 | 38 | 38 | 377 (23, 33), as before |
| F-15C | 30 | 39 | goes around from a float (34); sink 332 |
| F-35B | 39 | 40 | sink 277, was 421 |
| J-3 Cub | 35 | 35 | climbs 137, 132 (20, 37); 356 (33); rises 0.82 (39) |
| Learjet 35A | 40 | 40 | sink 286 |
| Mosquito | 40 | 40 | sink 267 |
| PA-28 | 40 | 40 | sink 320 |

**Made worse, named with figures**: no judged aeroplane fails a seed it
passed. The B-2A, named, puts a wingtip down on seeds 17 and 25 now (ten of
forty, was eight). The F-15C's seed 34, a hard touch at 314 ft/min before,
is now a go-around from a float that would touch 1,046 m along: her speed
kept, she floated, as the handbook warns of excess speed. The 787-8's seed 15
and the A380's seeds 1 and 13 pass now.

**A compiler's note**: first written `std::max(0.0, -s.climb_rate_fpm) /
60.0` and squared, the begin height did not read her sink at all, and the
flare began at the path's height whatever her sink. Undefined behaviour was
looked for and not found: `sink_fps` is a value, not a reference, and the
temporaries `std::max` returns a reference to live to the end of its
expression. A standalone file reproduces it with no references kept:
`double f(const S& s) { const double k = std::max(0.0, -s.v) / 60.0; return
k * k / 4.5; }` returns 0 for `s.v = -1200` under GCC 14.3.1 (Red Hat
14.3.1-4) at -O2, with or without `-march`, and 88.8 at -O1, at -O2 with
`-fno-tree-vrp`, under clang at -O2, and under -fsanitize=address,undefined
(which reports nothing). Written as a ternary it is right. It looks like a
GCC fault in its range analysis of floating point, not found reported. It is
written here as a negation with its sign tested, its disassembly reads the
sink, and a comment says why. No other `std::max(0.0, -...)` is in src/.

**Verification**: `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1`
to `_8` pass in linux-release and in an `-march=x86-64-v2` build (no FMA, as
CI); the outputs differ in the last figures of four seeds (a 787-8's touch
by 4 ft/min, seed 6), the outcomes the same. Least margins of those judged
over the eight seeds: sink 28 ft/min (the F-15C, seed 7), zone 169 m, rise
0.39 ft, flare climb 105 ft/min. The full linux-release suite on the first
round: 100% of 1,438 passed, 24 skipped for want of a key or a display, in
4,539 s; on the review's round, with the cap, 100% of 1,438 again, the same
24 skipped, in 3,937 s, and the eight gust seeds pass in the x86-64-v2 build
with the same least margins (sink 28 ft/min).
