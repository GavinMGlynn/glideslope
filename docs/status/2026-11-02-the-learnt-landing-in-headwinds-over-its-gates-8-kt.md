### The learnt landing in headwinds over its gate's 8 kt, 2026-11-02 — item done

**What is still not true first.**
- **The policy was not trained for this.** The committed policy (part 12,
  2026-11-02) already lands the five starts, so the gate was widened and
  nothing was trained, exported or given a new parity fixture.
- **The five starts were flown in choosing that policy** (they are five of
  its forty held-out starts), so the new test's figures are not an
  independent sample, as with the verification's starts and the corners.
- **Wind ahead and across together is not flown at the gate's corners**:
  as before, its corners are flown in its five edge winds one at a time, and
  15 kt ahead with 15 across (21 kt) is admitted but not flown.
- **It still flies its approach fast** (its own Phase 10b item, open).
- The policy file's header (assets/rl/c172p-landing.txt, line 49) says the
  held-out headwinds were "more than training's 8". Training's wind
  (tools/rl/env.py's `wind`, unchanged since 2026-09-30) draws up to 15 kt
  from anywhere with no more than 5 behind; 8 was the gate. env.py's
  docstring said thirteen across and eight down it, and is corrected here.
  The header is left as written, being the export's record.

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

**The gate** (`sim::LearntGate::most_headwind_kts`) is **15 kt ahead, not
8**: training's wind blows up to 15 kt from anywhere, so 15 ahead is inside
what it was trained on, and from every corner at 15 kt ahead the policy
lands inside the 5 per cent margin it was chosen by (under 285 ft/min,
within 4.75 m, at least 15 m past the threshold). corners.py's headwind is
15 with it. The gate's refusal case for too much headwind is 17 kt (it was
10). No limit is changed.

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

**Verification.** linux-release: the new test green; the 31 tests named
learnt or rudder green, among them the headwind corners at 15 kt (32
landings, worst 2.38 m, 160 ft/min) and the gate's refusals (17 kt
refused). The full linux-release suite (`-LE nightly`, `ctest -j4`, no
display): see the pull request.
