### The F-22A's flare flown on her power: at 1.3 times her measured VS0 she touches within 360 ft/min - 2026-11-02

**What is still not true first.** The "runway too short" item stays open.
The F-22A still has no stall figure in `assets/figures/f22.xml`, so still no
approach speed, no landing figure, and every landing is still refused her.
Her flare is fixed; writing the stall figure is not done, because it brings
her into every test that walks the aircraft with an approach speed: with
the copilot's, autopilot's and navigator's lists of her updated, 16 tests
were still red for her (below). Her landing distance was measured
in a scratch build: 1,861 m, 6,300 ft by tools/measured_landing.py - not
committed.

**Where the 360 ft/min comes from, and why it is kept.** The owner's
decision of 2026-10-09 (docs/status/2026-10-09-the-owner-s-decisions-of-2.md):
NASA's go-around criteria study, Zaal et al., "Go-Around Criteria Refinement
for Transport Category Aircraft" (NTRS 20205010611), whose touchdown
criterion is 6 ft/s. It is a transport's figure, held to every aircraft by
`every_aircrafts_touchdown_sink_is_within_nasas_go_around_criterion`.
Searched 2026-11-02 for a fighter's own:
- **JSSG-2006** (Aircraft Structures, 30 October 1998, everyspec.com),
  3.2.10.1 "Landing sink speeds": a blank to be filled for each aircraft;
  its guidance gives no number for land-based aircraft. 4.10.6 names "land-based
  aircraft with maximum limit sink rates less than or equal to 10 feet per
  second" - the 10 ft/s design limit, a structural limit, which
  sim/crash.hpp's 600 ft/min already is. Not a landing's figure.
- No F-22 landing gear design sink speed, landing technique or AFM text was
  found; F-16 forum accounts (an 11-to-13-degree flare, power to idle in it)
  are not citable. NASA's F-15 ground-effect flights (NTRS 19950026603) saw
  0.8 to 6.5 ft/s in 24 test landings - a test spread, not a limit.
So no sourced limit applies to her, and nothing is loosened.

**The tail strike is already measured from her geometry**: 16.54 degrees,
from her visual model by tools/ground.py (`with_tail_strike`, her
LEFT/RIGHT_TAIL_STRIKE contacts), pivoting on her main wheels; the flare's
bound is it less two, as the take-off's. Not changed.

**The fix: a jet with no nose left to flare flies her flare on her power**
(src/sim/lander.cpp, `least_flare_pitch_deg`, `power_flare_`). The FAA's
Airplane Flying Handbook (FAA-H-8083-3C, ch. 16, "The Flare") raises a jet's
nose "only 1 to 3 degrees" in the flare and reduces the thrust "smoothly ...
to idle as the flare progresses", warning that "reducing the thrust too
early may result in ... a high sink rate touchdown". At 1.3 times VS0 her
nose is 14.24 to 14.30 degrees as the flare begins, 0.24 to 0.36 short of
the bound. So where a jet's nose has less than the handbook's least, one
degree, left to the bound as her flare begins:
- her throttle is held, not closed from the flare's first step;
- the sink is flown on it from the first foot a minute past what the flare
  wants, worked her spool ahead as the gust cushion is, the same gain, and
  taken off as the sink is arrested (at twice a hand's pace);
- her nose is held where the flare began, so the bound does not do it.
Every other aircraft begins her flare more than five degrees short of her
bound (measured from `glideslope_cli land`, calm and 15 kt across: 737-300
8.8, 747-400 8.8, 787-8 9.6, A320 6.6, A380 9.0, B-2A 7.5, F-15C 5.4,
F-35B 9.7, Learjet 9.5; the F-22A 0.30 and 0.36), and no other aircraft
took the power flare in any of those 30 landings, so theirs fly as before.

**Her figures now** (scratch stall figure, `land f22`, 2 nm, full tanks,
63,960 lb): 157 ft/min calm, 153 and 151 in 15 kt across, nose 14.2 to
14.3, stopped 1,843 to 1,861 m in. Before: 383 ft/min, nose 14.6. Held to
the bound instead of the nose she began with, she touched at 197 to 200
ft/min with her nose at 14.4, past the bound in the approach lesson (14.58).

**Verification.**
`the_f22a_lands_at_1_3_times_her_measured_vs0_on_the_centreline_within_360_ft_min_in_calm_air_and_15_kt_across`
(renamed from `the_f22a_flies_her_final_...`) now judges her touch: within
360 ft/min and her nose short of the bound, from 5 nm at 61,570 lb: 176
ft/min calm, 202 and 199 in 15 kt across, final within 0.49 to 2.80 m.
**Seen to fail** with `least_flare_pitch_deg` set to 0: 392 ft/min in calm
air; restored. linux-release, `ctest -LE nightly -j4`, no display: see the
pull request for the full suite.

**Left: her stall figure, and what it brings.** Written as
`stall_speed_landing` (steady="1", 90.0 KCAS, combat loading) with her
measured landing distance, these went red, and each needs a fix or a named
reason before it can be committed (the patch is kept for the next round):
- counts and lists that name her as having no approach speed or landing
  distance (copilot: 5 tests; autopilot's approach heading list; navigator's
  plan speeds; lesson counts in 5 tests; the measured-landing count in
  tests/CMakeLists.txt; take-off flap's "publishes no stall");
- **real failures**: her stall lessons (recovered at the warning she loses
  2,964 ft against 500; left thirty seconds, 16,295 against 7,938), the AI's
  stall watch ("slowed under her stall"), her approach in gusts (went round,
  a balloon above the flare; 7.9 kt slow against 5), the autopilot's heading
  from her approach speed gear down (she departs under 131 kt on it), the
  flare-incidence test (a pilot took her wing to 21.8 against 21.2), and the
  approach lesson's strike margin (her path is flown 2.2 degrees short of
  her strike, inside its 2.5);
- the recorded copilot exchange `cbd-orbit-f22-anthropic.jsonl`, whose
  prompt changes when she has an approach speed, recorded again.
