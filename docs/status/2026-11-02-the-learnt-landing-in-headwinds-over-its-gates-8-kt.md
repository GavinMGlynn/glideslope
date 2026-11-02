### The learnt landing in headwinds over its gate's 8 kt, 2026-11-02 — item done

**What is still not true first.**
- **The policy was not trained for this.** The committed policy (part 12,
  2026-11-02) already lands the five starts, so the gate was widened ahead
  and nothing was trained. It was exported again only to correct one line
  of its header (below); its weights and parity fixture are unchanged.
- **The gate is narrower behind than training**: with any wind behind it
  admits no more than 2.75 kt across, where training blew up to 14.1 across
  with 5 behind. From the left the policy fails there (below); its own plan
  item is to train it there, so that the gate can open to training's 15.
- **The five starts were flown in choosing that policy** (they are five of
  its forty held-out starts), so the new test's figures are not an
  independent sample, as with the verification's starts and the corners.
- **The total-wind edge is flown at 45 degrees only**: between 15 across
  and 15 ahead the gate's edge is an arc, flown at its middle from each side,
  not along it.
- **It still flies its approach fast** (its own Phase 10b item, open).
- The policy file's header (assets/rl/c172p-landing.txt, line 49) said the
  held-out headwinds were "more than training's 8". Training's wind
  (tools/rl/env.py's `wind`, unchanged since 2026-09-30) draws up to 15 kt
  in all from anywhere with no more than 5 behind; 8 was the gate. env.py's
  docstring said thirteen across and eight down it, and is corrected here.
  The header is corrected by exporting part 12's checkpoint again
  (`scratchpad/learnt-landing-retrained/p15/step32562500.zip`, the sha256
  its header names; r12's rerun zip differs in its bytes) with the header's
  own seed, steps, commits and notes, that one note changed: the policy file
  differs from the committed one in line 49 alone, and the parity fixture
  it wrote is byte for byte the committed one.

**Measured before choosing, in the bindings** (evaluate.py --held-out at 75
and 100 lb a tank; corners.py with the headwind corner set by hand), the
committed policy:
- The five starts the policy of 2026-10-20 touched short from - held-out
  27 at 75 lb a tank, and 10, 24, 27 and 31 at 100 lb, 9.6 to 13.1 kt ahead
  - all land: 54 to 148 m past the threshold, 158 to 171 ft/min, within
  2.34 m of the centreline. The forty held-out starts at both loads: 40 of
  40 within the limits.
- The gate's 32 corners with 13.5 kt ahead: least 57 m past the threshold,
  worst 165 ft/min, 2.08 m. With 15 kt ahead: least 29 m, worst 160 ft/min,
  2.38 m, none rising after the touch (the 2026-10-20 note had her bounce 7
  ft there). With 8 kt (the gate as it was): 159 ft/min, 0.62 m, 162 m.

**The gate** (`sim::LearntGate`) is **15 kt ahead, not 8**, and now also
**15 kt in all** (`most_wind_kts`) and **no more than 2.75 kt across with
any wind behind** (`most_crosswind_behind_kts`). Training's wind is up to
15 kt in all with no more than 5 behind; the gate's three per-axis limits
(15 across, 15 ahead, 5 behind) had admitted 15 across with 15 ahead (21
kt, never trained) and - on main before this change too - 15 across with 5
behind. From every corner at 15 kt ahead the policy lands inside the 5 per
cent margin it was chosen by (under 285 ft/min, within 4.75 m, at least 15
m past the threshold); not at every corner with 5 behind and 14.1 across
from the left:

- **c00** (1.6 nm, 59.5 m left, 19.5 m low, 4.9 degrees left, 3 kt slow):
  412 ft/min, 5.46 m off the centreline, 49 m before the threshold.
- **c08** (the same, turned 4.9 degrees right): 254 ft/min, 7.36 m off the
  centreline, 311 m before the threshold.

**Bisected at 5 kt behind, from the left** (32 corners each, bindings;
least distance past the threshold): 2 kt 116 m, 2.5 kt 103, 2.75 kt 52,
2.8 kt 39, 2.875 kt 20, all within the rule; 3 kt fails (7 m short), and 4,
6, 8, 10 and 12 kt fail. From the right 2.75 and 14.1 kt pass. At 2.75 kt
across with 2.5 and 1 kt behind: 250 and 243 m. The gate takes 2.75, the
lesser side's, with its 0.05 kt of rounding (2.80) measured inside the
rule. The 45-degree corners at 15 kt in all: least 131 m (left) and 71 m
(right), worst 194 ft/min, 2.67 m.

corners.py flies the gate's nine edge winds with it. The gate test's
refusals are now 17 kt ahead (it was 10), 17 kt in all (12 across, 12
ahead) and 5 across with 3 behind. No limit of a landing is changed.

**The new test**,
`the_learnt_policy_lands_past_the_threshold_from_the_held_out_starts_it_once_touched_short_from_in_headwinds`:
the five starts, their values landing.py's to the last digit, placed as the
corner tests place them (its `placed` now takes a fuel load), each found at
the gate by `outside_learnt_gate`, flown by the simulation's learnt lander
and held to the verification's limits and stopped on the runway; it asserts
that five were flown and every one is over the old gate's 8 kt. In
linux-release: 5 of 5, 54 to 148 m past the threshold, 158 to 171 ft/min,
within 2.34 m - the bindings' figures.

**Seen red**: with the policy of 2026-10-20 (`git show
f8cff054~1:assets/rl/c172p-landing.txt`) copied over the build's
data/rl/c172p-landing.txt, it fails, 0 of 5: touched 29, 0.09, 32, 65 and 33
m before the threshold. The build's copy was put back and compared equal.

**The corners in nine winds**: the five edge winds as before, and four
new tests, one a wind - 15 kt in all from 45 degrees ahead from either
side, and 2.80 kt across with 5 behind from either side (the gate's 2.75
and the 0.05 kt it lets in past every wind limit, so the edge it admits:
worst 2.45 m, 243 ft/min) - with the test of
the winds now asserting nine, each at an edge of the gate and within its 15
in all: 288 landings, in linux-release worst 2.67 m and 281 ft/min, none
short. **Seen red**: with the total-wind check removed from
`outside_learnt_gate`, the gate test fails (17 kt in all not refused: "heading
8 degrees left of the runway's" instead); with the crosswind-behind check
removed, it fails (5 across with 3 behind admitted). Both restored.
`the_corners_the_training_tools_fly_with_the_wind_behind_are_the_gates`
reads corners.py's BEHIND_ACROSS and ALLOWANCE from its source and holds
them to the gate's; seen red with BEHIND_ACROSS set to 3.0, then restored.

**Verification.** linux-release: the new test green; the 35 tests named
learnt or rudder green, among them the nine winds' corners and the gate's
refusals. The full linux-release suite (`-LE nightly`, `ctest -j4`, no
display): 1435 of 1435 passed, 24 of them skipped (no key, device or
service). On the draft's CI run, linux-debug's shard 6 timed out one test
unrelated to the learnt landing,
an_aeroplane_going_around_beside_rising_ground_flies_its_circuit_height_over_it_and_lands
(900 s, against a measured 762 s there; it flies no policy, and passed in 27
s here).
