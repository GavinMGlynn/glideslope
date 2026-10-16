### Every aircraft's glide measured round its tightest orbit, each figures file giving its slowest glide, 2026-10-06 — tail not done; the F-22 glides nowhere, why not found

**What is missing, first.** **The F-22 glides at no speed**: from 255 kt, its
slowest under power, to 335 it departs past 95 degrees of alpha before it
is a fifth of the way round, and at 340 it is a third of the way round when
it reaches 1,000 ft. It is not a floor to raise; why is not found. Its
`<glide_speeds>` is its slowest under power, and its test names it, so the
tail "A glide may still be asked of a jet at its approach speed" stays open.

**Measured** (`sim::glide_tightest_orbit`, src/sim/orbit_trial.cpp): begun
on the circle allowed at the speed, flying along it, clean, at 30,000 ft,
every engine failed at once, flown by the controller's glide once round, or
to 1,000 ft. **Stalled** is the most angle of attack flown more than half a
degree past the angle at the greatest lift coefficient seen (how the
autopilot learns where a wing stalls), or past 25 degrees whatever the lift
did - for a wing whose lift goes on rising into a deep stall, as the
F-15C's peaked at 35.7 as she settled at 43; no glide flown unstalled
passes 19 (the F-35B at 204 kt). **Round** is once round, or half way where
1,000 ft comes first (the fastest jets' circles of 8 to 13 km).

**Changed:**
- **Every figures file gives the slowest it may glide at**,
  `<glide_speeds slowest_kcas>`, read by `read_published_figures` (refused
  missing, not above 0, or twice) and given in every brief
  (`Brief::glide_slowest_kts`, frontend::brief_for). Measured by
  **`glideslope_cli glide-speeds AIRCRAFT`**: from the slowest a route may
  fly it up by 5 kt to the fastest a plan may, the first that goes round
  both ways unstalled. Raised above the slowest a route may fly: 747-400
  220 -> 235, 787-8 193 -> 203, A320 162 -> 167, F-15C 160 -> 170 (at 160
  and 165 she settles into her deep stall at 112 kt and 43 degrees),
  Mosquito 123 -> 128. The other ten glide at their slowest routed speed.
- **A glide's slowest is that, or the slowest a route may fly it where that
  is more** (`copilot::glide_speeds`), to its best climb or that slowest. It
  was the approach speed, a flaps-down figure flown clean: from 15,000 ft
  the 737-300 stalled at every glide from 137 to 162 kt, past 50 degrees of
  alpha. The light aeroplanes' ranges do not move (the C172's 60 to 75 kt,
  which every recorded copilot conversation was told).
- **A glide's steepest sink is 12 degrees of flight path, or 2,500 ft/min
  where that is more** (`Controller::gliding`). Held to 2,500 ft/min, at
  30,000 ft the F-15C at every glide from 170 to 200 kt sagged to 112 kt
  into her deep stall, and the F-35B at 204 departed. No light aeroplane
  glides fast enough for the flight path to bind. The controller does not
  know where the ground is: the trial's stop at 1,000 ft is the trial's
  protection only, and a copilot's glide is kept off the ground by its
  route, as before.

**Verified**: 68 band tests (test_navigator.cpp,
`the_<aircraft>_glides_round_its_tightest_orbit_[turning_left_|turning_right_]from_<a>_to_<b>_kt_without_stalling`,
or `at_<a>_kt` for one speed): every 5 kt from its slowest glide and at its
fastest, round and unstalled; and the band that begins at its slowest pins
the file's figure - never below the slowest a route may fly, and where
above it, 5 kt slower does not glide round unstalled.
`every_glide_of_every_aircraft_each_way_round_is_flown_by_exactly_one_glide_test`
asserts the bands tile the space - 292 glides, each in exactly one of 68
bands, and no band empty - seen red with the C172P's band cut short at 70
kt. The F-22 is named, and its name turns its tests red once it glides.
First written as four tests by class, the airliners' and business jets'
took 1,661 s in CI's linux-debug and was cancelled at the job's limit; one
to an aircraft, the A380's was 238 s here, about 1,000 on CI at the 4.19
times this machine that CI's linux-debug measured on the one-step-past
test. In bands, the longest here, four at once, is 32 s (the Mosquito's),
about 133 on CI. tests/ci_costs/linux-debug.txt has these times times 4.19,
and every other preset's are those scaled by its median ratio to
linux-release over its tests of 30 s or more, against linux-debug's 3.18 -
estimates until CI measures them.
`the_slowest_a_glide_may_fly_an_aircraft_at_is_read_and_refused_where_it_is_wrong`
(test_figures.cpp). Each seen red: the approach speed put back as the
slowest, the 737-300 stalls at 137 to 162 kt; the sink put back to 2,500
ft/min, the F-15C at 170 to 200 and the F-35B at 204 stall; the alpha
ceiling put at 15, the F-35B at 204 is called stalled; the trial stopped at
0.6 of a turn, the Mosquito at 128 is not round; the Mosquito's file put at
133, it glides at 128, 5 below.

**The stall recovery's load, looked at and not changed.** Traced left thirty
seconds in its stall: the **A320's 2.18 g comes while the nose is still
going down**, the elevator at full nose-down and pitching down at 5 degrees
a second, as its wing comes back through its lift's peak at 179 kt - its
lift returning, not a pull-out, so the pull-out's limit cannot reach it. The
**Mosquito's 2.23 g is its pull-out**, the elevator going to 0.66 nose-up
once the wing is flying. Looking half a second ahead at the load's trend
changed neither figure; freezing the elevator trim through the recovery
held the Mosquito to 1.48 g but left it never recovered, and put the A320 at
2.28. Neither is in.

The selftest's hash does not move, d36123c1eecc3e23: it flies no glide.
