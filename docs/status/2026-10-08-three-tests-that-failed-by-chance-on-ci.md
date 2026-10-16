### Three tests that failed by chance on CI, 2026-10-08 — two made deterministic, the third's cause fixed

**What is still open, first.** The take-over test's server stall is built
by hand (below), not by the test: the test still meets a stall only when
the machine makes one. A stall longer than a quarter of a second still
moves the clocks' difference by what is over. The "not refused once" of
2026-09-29 was a line split by another writer, fixed then in
`glideslope_impair`; it stays a watched item.

**The learnt policy's 160 corner landings timed out on CI's Ubuntu debug**
(900 s, run 37759639784 and before), and so tested nothing there. Split
into one test a wind - `the_learnt_policy_lands_within_its_limits_from_every_corner_of_its_gate_in_{calm_air,the_most_crosswind_from_the_left,the_most_crosswind_from_the_right,the_most_headwind,the_most_tailwind}`
- each flying all 32 corners and failing on fewer, and
`the_corner_landings_are_flown_in_the_five_winds_at_the_gates_edges_and_no_other`
pinning that the five winds are the gate's edges: calm, the most crosswind
from either side, the most headwind and the most tailwind. Measured in
linux-debug here: 49.7, 50.2, 50.7, 51.1 and 36.0 s; in `tests/ci_costs`
at 4.2 times that on CI's Ubuntu debug (152 to 215 s), and the other
presets scaled by their own cost of the fuel tests against Ubuntu debug's.
**Seen to fail**: a corner left out, "every corner flown: 31 of 32"; the
tailwind a knot inside its edge, "behind is at an edge"; reverted.

**The engine-stop prediction test failed on Windows** (release, debug and
clang): medians of 0.4 to 1.5 m after the stop against about 0.05 m before
(0.428 against 0.077 on windows-release, run 37759639784). **The cause**: the
distance compared is the server's word against where the client had flown
to at the step the clocks' difference places the word at, and on Windows
that placing is a step off for seconds at a time (the paced test's
excursions, below) - 0.45 m at the Cessna's speed, the median in the failed
run. Which half of the run the steps off fall in is chance, so the test
judged the clock, not the engine. **Judged now in speed**: the client says
the median speed error before and after the stop, and the test holds after
to no more than before. A step's misplacing moves the speed by a 120th of a
second's change in it, an engine run on by its thrust over the round trip.
Measured (linux-debug): 0.015 to 0.018 m/s after against 0.135 before (the
throttle's changes flown a step or two apart are most of before's); against
the server at 0.6 of real time, 0.009 after. On the Windows copy (release),
ten runs in a row passed, 0.016 to 0.023 m/s after against 0.123 to 0.144
before (and three earlier runs, 0.011 to 0.013).
**Seen to fail** with the prediction not told the engine had stopped:
0.487 m/s after against 0.136, red; reverted. (The distance's medians were
then 0.183 against 0.042 m.)

**The take-over test failed on macOS debug** (run 37759639784): "the
prediction error was 14.915 m, the bound 8", 12.9 s in - before the
take-over - with the clocks' difference 23 steps off at worst. **The
cause**, read from the taking client's track: the server held up. Inputs
that came in while it was were flown once it went on, at the step it had
got to and not the one due when they came - so they arrived, as the client
reckons it, that many steps early; the least of the clocks' difference over
the fifty words it remembers fell by those steps, and for the two seconds
it remembered them the server's word was placed that far off. Built by
hand on linux-debug - the test's own server paused (SIGSTOP by its pid)
200 ms, 12 s in: the difference fell from 214 to 198 for two seconds and
the prediction was off by 4.2 m (0.4 m in the same run unpaused). Under
the Windows build beside it, a server falling a second behind put it off
22 m. **The fix** (`glideslope_server`): an input is flown from **the step
that was due when it was read**, not the step the server had flown to -
what the server already did within a pass, now across the steps it owes,
at most a quarter of a second (`most_steps_held`) so that a server that
never catches up still flies what it is sent. With it, the same 200 ms
pause: the difference held at 231-232, the prediction off by 0.43 m at
worst. With it, 20 runs in a row on two cores (taskset) passed, and the
137 network tests (prediction, take-over, impaired, paced, behind, hand-over)
pass in linux-debug.

**From the review of #133**: the rule is `net::HeldInputs`
(`src/net/held_inputs.hpp`), the server's `Connection` holding one, and
`an_input_heard_while_the_server_owes_steps_is_flown_from_the_step_due_when_it_came`
pins it: every amount owed - none, a pass's 4, a 200 ms stall's 24, the
most held (30) and past it (200, held 30) - flown from its due step; two
read in passes apart each from its own; two in one pass, the newer; a
switch flies the newest at once (flown early by what is owed - a switch is
a moment of its own); a flood keeps the newest 120. **Seen to fail** with
the old rule (`stepped + min(owed, 4)`): "owing 24 steps, flown from 104,
not 124"; reverted. `docs/TRANSPORT.md` says when an input is flown; the
wire is unchanged, so the version is. The engine-stop test also holds the
median speed error after the stop under 5 cm/s (0.023 the most measured;
an engine run on, 0.487); the distance's medians are not held, a step's
misplacing being chance on Windows.

**Kept**: the engine-stop test keeps the client's
track (`--track`), and the take-over test the taking client's; the client
says the median speed error before and after the engine stopped beside
the median distance, and writes each comparison's speed error to its track.
