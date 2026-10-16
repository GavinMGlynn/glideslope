### The B-2A slows down its approach with its drag rudders, 2026-10-01 — tail done 2026-10-02, with the flare to the wheels above

**What is missing first.**
- **The B-2A still floats after it first touches**: in the approach lesson
  she touches at 122 knots, 9.7 degrees nose up and 0.7 ft/s down, rises
  1.7 ft and is airborne for about a second before she settles. The item's
  bound: she rises less than half a foot after her first touch. Not met;
  what was found is below.
- **A pilot flying her by hand cannot open the drag rudders**: nothing binds
  the speedbrake lever to a stick, a quadrant or a key (a new tail). So the
  bomber lessons' bands, which a pilot is debriefed against, stay at
  `vref+20`; the AI pilot flies the approach at vref to vref+2 and the
  circuit's final at vref-4 to vref+10.
- The B-2A's reference speed is the model's own: 1.3 times its measured
  95.4-knot stall, at the light loading.

**What was wrong.** The B-2A has no flap, and the model had nothing else to
add drag. Down a three-degree glidepath at its reference speed of 124 knots
it could not be trimmed: its throttles closed and it ran on to 138 knots
over the threshold.

**What changed.**
- **The model has speedbrakes: its drag rudders opened together.** The B-2's
  wingtip drag rudders are split into upper and lower halves. Opened on one
  side they turn it, opened on both they slow it, and they are open in flight
  at low speed: Sweetman 2005 and Chudoba 2001 as quoted, and
  targetlock.org.uk's systems page; docs/ASSETS.md lists each, with URLs.
  `tools/make_b2.py` gives the model a speedbrake lever
  (`fcs/speedbrake-cmd-norm` to `fcs/speedbrake-pos-norm`, two seconds to
  full) and their drag, ΔCD 0.065 fully open. That figure assumes two
  100 sq ft drag rudders, estimated by eye from published photographs, each
  half at 45 degrees, with a flat plate's 1.2 on the area they show. Only
  ΔCD times the lever matters, and the drag is linear in the lever: half
  open is half the drag. Opened together the halves are taken to add no
  lift or pitch, and their drag is separate from the yaw drag CDrudders
  already had.
- **The approach is flown with them half open, from the figures.**
  `assets/figures/b2.xml` gives `approach_speedbrake="0.5"`. It is
  calibrated so that the approach is flown at the reference speed with about
  a third of the throttle on; it is not a published figure. `approach_speeds`
  reads it into `ApproachSpeeds::speedbrake` (0 for every other aeroplane,
  whose figures give none). `sim::Lander` holds the lever there in the air.
  `InitialConditions::speedbrake` starts an aeroplane with them out, settled
  in the trim as the flaps are. A value outside 0 to 1 is refused.
- **On the runway the lever goes fully out, and now it does something.**
  `sim::Lander` puts the speedbrake lever fully out on the landing roll and
  after a bounce (`lander.cpp`, the rollout's and the bounce's
  `c.speedbrake = 1.0`). Until now the B-2A's model had no speedbrake, so for
  her that did nothing. Now it opens the drag rudders fully. In the circuit
  she stops 1,669 m beyond the threshold, touching at 287 m: a roll of
  1,382 m. With the lever held at the approach's half on the roll it is
  1,681 m, so 12 m of the roll is theirs; the brakes do the rest.
- **Tied to the gear first, the opening broke the stall checks.** Opened by
  the model's flight controls whenever the gear was down, the B-2A in the
  stall lessons (gear down) lost 473 ft at the first sign of the stall,
  against 414 named, and pulled 1.95 g from thirty seconds in it, against
  1.8 allowed (main: 1.64). Closed as the throttles came up past 0.7, it
  still pulled 1.98 g. Held by the approach autopilot alone, as flap is, the
  stall recoveries are exactly as on main.
- **The lessons' wording** no longer has a pilot set flap she has not got.

**The float, investigated and not fixed.** Traced step by step in the
approach lesson:
- **She meets the runway still rounding out.** At the touch her wing lifts
  1.19 times her weight: CL 0.79 where 0.665 holds her. The flare is still
  arresting a sink of 3.5 ft/s when the wheels arrive. The flare is flown to
  the height of her centre of gravity, and her wheels hang 11 ft below it
  (a Cessna's hang about 4 ft below, against a 15 ft flare). So for her the
  wheels arrive with a third of the flare still to fly.
- **Ground effect** adds a fifth to her lift there: Aeromatic's kCLge is
  1.153 at 0.066 of her span, which is plausible for a wing of aspect ratio
  5.7. The FAA's Airplane Flying Handbook (FAA-H-8083-3C, chapter 9) has
  descending into ground effect make an aeroplane float. The only B-2-specific
  word is that she "sits on a cushion" and "has to be forced to land"
  (Chudoba, as the Virginia Tech project gives it). Nothing published
  supports changing kCLge, so it is left alone.
- **The gear does not throw her.** The main struts are damped at about 0.8
  to 1.2 of critical. The spike at the touch is damping, and the strut is
  fully extended again within 0.3 s.
- **The landing law lowers her nose too late.** At the touch the jet's law
  lowers the nose, and the drag rudders open fully, but it inherits the
  flare's nose-up trim. The elevator stays nose-up for 0.4 s, and the wheels
  are off by 0.3 s.

What was tried, each in `sim::Lander`, each run against all fourteen
landings of the approach lesson:
- **The trim dropped at the touch, for a jet:** she rises 0.8 ft. Every other
  landing is unchanged.
- **And the nose's target starting 2 degrees below the touch:** 0.5 ft.
  Every other landing is unchanged.
- **The flare flown to the wheels' height, from her model's wheel
  positions and her pitch:** she rises 0.0 ft. But she now floats 963 m
  before touching (287 m before), and the A380, the other tall one, rises
  2.8 ft where it rose 0.0.

None meets the bound without moving other aeroplanes' landings or
lengthening hers by 700 m. So per the tail's brief this is reported and the
lander is left as it was. The next step is the flare's own timing for tall
gear: start the round out by the wheels' height, keeping the touchdown sink
the centre-of-gravity law gives today. That is a change to every landing,
and an item of its own size.

**The 2021 Whiteman landing is weak evidence of anything.** It is the only
B-2A landing published (the Air Force's abbreviated accident report,
pp. 12-13). It was an emergency landing with two hydraulic systems failed,
flown fast - 143 knots, "slightly higher than the recommended approach
speed", for forecast wind shear. Its main wheels lifted off one side at a
time and it was airborne again for a second, and its left main gear then
collapsed. No number in the model is set from it.

| B-2A | Before | After |
| --- | --- | --- |
| Over the threshold, approach lesson (vref 124) | 138.0 kt, +13.9 | 124.1 kt, +0.1 |
| Approach lesson, the glidepath / over the threshold | 124-138 / 138 kt, started untrimmed | 124-126 / 124 kt, trimmed |
| Circuit's final (AI pilot) | vref-4 to vref+15 | 120-134 kt, vref-4 to vref+10 |
| Rose after touching, approach lesson | 1.9 ft | 1.7 ft |
| Circuit: touched / stopped beyond the threshold | - | 287 m at 123 kt / 1,669 m (1,681 with the drag rudders held half open on the roll) |
| Stall at first sign / left thirty seconds | 414 ft, 1.59 g / 921 ft, 1.64 g | the same |

**Verification.**
- `the_b2a_crosses_the_threshold_within_five_knots_of_its_reference_speed`
  (new): 124.1 knots against 124.0.
- **Seen to fail** on the model as it was: 138.0, +13.9. With the figures'
  setting put to 0 in the build's data as a deliberate bug it fails again
  (138.0); restored, it passes.
- `the_speedbrake_an_approach_is_flown_with_is_read_and_refused_outside_its_travel`
  (new): each file is the B-2A's own figures with only the attribute
  changed. 0.25 is read back. -0.1 and 1.5 are refused, and the refusal names
  `approach_speedbrake`. The B-2A's 0.5 is read, and the 737's absent setting
  is read as 0. **Seen to fail** with the 0-to-1 check taken out: "both
  settings outside 0 to 1 were refused, not 0".
- `the_committed_b2_is_what_its_script_writes` passes.
- The selftest hash is unchanged: `d36123c1eecc3e23` in linux-release.
- linux-release, rebased on main with the F-15C's approach speed, the
  lesson, take-off, landing, stall, circuit, figures, autopilot, checklist,
  handing and F-15C tests (the multi-process and window tests aside): 189
  tests, none failed. Four were skipped: three need a language model's key,
  and one is Windows's.
