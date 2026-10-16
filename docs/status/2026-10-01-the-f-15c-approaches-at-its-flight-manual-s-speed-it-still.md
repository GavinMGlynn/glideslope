### The F-15C approaches at its flight manual's speed; it still has no published stall, 2026-10-01 — item not done

**What is missing, first.** The F-15C does not stall near a published speed,
because its flight manual publishes none and the model could not follow it
if it did. T.O. 1F-15A-1, section VI, "1 G stalls": light buffet from about
18 units of angle of attack, wing rock above 30, and at full aft stick the
angle of attack "stabilizes at 45 units or above with airspeed 100 knots or
less", the vertical speed "probably pegged going down"; in the landing
configuration buffet begins at about 23 units. No speed in knots, no weight.
The model's measured stall, 151 KCAS at 36,946 lb, is not a stall at all:
flown by `stall_speed` with a probe printing each half second, the
stabilator is at full nose-up travel (-1.00) from 63 s, alpha pinned at
18.3 degrees, 0.90 g, sinking 3,300 to 4,900 ft/min, and the speed bottoms at
151.2. That is where its pitch authority ends, far short of 45 units.

**Why, as far as a public source says.** NASA TM-4604 (Corda, Stephenson,
Burcham and Curry, *Dynamic Ground Effects Flight Test of an F-15 Aircraft*,
1994, <https://ntrs.nasa.gov/api/citations/19950005778/downloads/19950005778.pdf>)
flew NASA's F-15A down approaches at 150 knots flaps down and 170 flaps up;
its figure 5 example, 166 knots flaps up, holds about 10 degrees of alpha,
and the nominal point its derivatives are taken at is 8 degrees and -5 of
stabilator, from the Dryden F-15 simulator: CL-alpha 0.065 and CL-delta
0.005 a degree, Cm-alpha -0.0021 a degree. The model's lift slope agrees
(0.069), but its Cm-alpha at 8 degrees is about -0.009 a degree - four times
as stable - and its stabilator's lift twice NASA's, so every degree of alpha
costs nose-up stabilator and the stabilator's download costs lift. Trimmed,
it never makes more than about 0.7 of lift coefficient. Changing the pitching
moment moves every F-15C figure, its take-off (fitted to figure A3-6) and the
pitch loops tuned to it; one derivative at one point is not enough source for
that, so it is not done here and the item stays open with this named.

**What is done: the approach is the manual's.** Appendix A's figure A8-1,
Landing Approach Speed (change 4, 23 August 1985, flight test; 21 units, any
drag index), gives KCAS by gross weight, flaps up and down. Read from the
Internet Archive's scan at full resolution, by finding the grid's 111 rows
(225 to 115 knots) and the curves' pixels between them: 160.5 KCAS flaps up
and 147 flaps down at 36,946 lb; 179 and 165.5 at 45,713. The chart's own
sample, 30,000 lb flaps down, 130.5 knots, reads 130.8 this way. The model
has no flaps, and section II's no-flap landing is the same approach
"slightly faster at on-speed AOA", so the flaps-up line is the one: **160
KCAS at the `clean` loading**, which is where the F-15C's stall and climb
were already measured.

- `assets/figures/f15c.xml` gives it as `<approach kcas="160"
  loading="clean">`, with its figure and how it was read.
  `sim::read_published_figures` reads it into
  `PublishedFigures::approach_kcas` and `approach_loading`, refusing one
  without both, one at a loading the file has not got, one not above zero
  and a second.
- `sim::approach_speeds` flies it in place of 1.3 times the landing stall,
  and `ApproachSpeeds` now carries `stall_kts` itself: the lessons' `stall`
  had been worked back as `vref / 1.3`, which for the F-15C would now be 123
  knots, below anything it can fly. The tests that did that read
  `stall_kts`; `load_for_the_approach` loads the approach speed's own
  loading where there is one.
- The fighter approach lesson's note says where its `vref` comes from;
  `docs/ASSETS.md` records the manual's figure with the scan's SHA-256.

**What the model does at the manual's speed.** Two miles out on the
glidepath to the stop, the AI's approach holds 159.3 to 163.5 KCAS. Alpha
swings between 10.5 and 18.3 degrees as the speed and path settle - a slow
pitch oscillation through the whole approach - with the stabilator at -0.48 to
-0.98 of its nose-up travel, and is 17.5 at fifty feet. It lands by the
book: all three stages, an empty debrief, stopped on the runway. It is flown
close to the edge of what the model can do, where the aeroplane flies it at
10 degrees with room; that is the missing part above, not a reason to fly it
faster.

**Verified** (linux-release):

- `the_f15c_flies_its_approach_at_its_flight_manuals_speed_for_its_weight`:
  the figures give 160 at 36,946 lb, `approach_speeds` gives it, and the
  approach stage is held within five knots of it (159.3 to 163.5), landed by
  the book to a stop. **Seen red twice**: with the `<approach>` taken out of
  the data the tests read ("the F-15C's figures give the manual's 160 KCAS
  approach, not 0"), and with `approach_speeds` made to ignore it ("the
  approach is flown at the manual's speed, not 196.378"). Both reverted.
- Every F-15C test re-run, and the lesson, landing, take-off and figure
  tests around them: the approach lesson by the book (F-15C stage 0 159 to
  164, -22.9 to -6.6 ft/s, 3 of 3), an approach flown fast is named (vref
  160), the circuit by the book (8 of 8 stages, highest 1,536 ft, stopped on
  the runway), the stalls lesson by the book, and both stall-recovery checks
  with the F-15C's figures unmoved - 281 ft against 500 at 1.70 g at the
  warning; 958 against 2,952 at 1.89 g left thirty seconds, still the named
  load exception at 1.89 against 2.00. `glideslope_cli figures f15c`: 8 of
  8 in range, stall 151.17. Selftest hash unchanged, `d36123c1eecc3e23`.
- One unrelated failure in a wide `-R` run,
  `the_client_with_the_window_stalled_past_the_timeout_joins_again_by_itself`,
  matched by the word "stall"; a window client test this change does not
  touch, run alongside seventy flights at -j4, and green run again on its own.
  A second run of 148 take-off, figure, lesson, landing and plan tests: all
  passed, three skipped for want of a key or a filesystem.
