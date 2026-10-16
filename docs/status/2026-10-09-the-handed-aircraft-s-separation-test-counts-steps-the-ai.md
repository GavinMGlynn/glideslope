### The handed aircraft's separation test counts steps the AI has it, not the clock; 34 tests costed for linux-debug, 2026-10-09 — fix

**What failed.** `an_aircraft_handed_to_the_ai_is_measured_against_every_other_ai_aircraft_and_kept_apart_from_them`
on CI's linux-debug: "the handed aircraft was measured over fewer than
2760 steps: ... over 2536 steps". The client stayed thirty seconds of the
machine's clock, and a slow runner stepped fewer of them after the
hand-over. **Seen to fail** here the same way with the old script and the
server put behind real time (`--test-step-ms 9`): "measured over fewer than
2760 steps".

**What changed.** Two server flags for tests: `--steps-after-hand-over N`
stops the run once the AI has flown an aircraft in a player's slot for N
measured steps (`Fleet::handed_steps`, counted in `measure_apart` - and the
steps a slow machine still owes are not taken after it, which at first
made the run measure 5,881), and `--stopped-file FILE` is written as the
run ends; the client leaves by it (`--until-exists`). The test asks for
3,600 and asserts both pairs with the handed aircraft were measured over
exactly 3,600 steps - and does, with the server behind real time
(`--test-step-ms 9`) as well as without.

**CI's costs.** The 34 tests of linux-debug missing from
`tests/ci_costs/linux-debug.txt` are added with estimates, not
measurements: this machine's linux-debug time and half again, or 60 s for
a client test not run here. `tools/ci_test_costs.py` replaces them with
CI's own on its next run.
