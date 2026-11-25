### The engine-stop prediction test against a server held up for up to a second, 2026-11-25

**What is still missing, first.** The plan item "The engine-stop prediction
test fails on Windows clang when the server is held up" stays open. Its
verification asks for the test to pass with the server held up for a second
on purpose on every platform, and then for a month of CI runs. Until today
CI never held this test's server up. The month of CI runs with the held-up
variant starts the day this lands. A commit on this branch first ticked the
item on a search of CI logs, and that was wrong (below).

**What CI ran before.** `tests/cmake/server_engine_prediction.cmake` ran two
ways:

- `a_client_predicting_its_aircraft_stops_its_engine_when_the_server_says_and_is_put_right_no_more`,
  plain;
- `a_client_predicting_its_aircraft_against_a_server_behind_real_time_stops_its_engine_and_is_off_by_centimetres`
  (`-DSLOWED=ON`), the server's clock at a steady 0.6 of real time
  (`--test-pace 0.6`).

Neither holds the server up by fits and starts. The 2026-10-17 run that
stopped the server for 20 to 800 ms at a time used SIGSTOP. That was done by
hand on linux-debug and never run on CI, and SIGSTOP does not exist on
Windows. The withdrawn status file also named the wrong test,
`an_engine_stopped_on_the_server_winds_down_...`, which is a unit test.

**The change.** A third run, `-DHELD_UP=ON`:
`a_client_predicting_its_aircraft_against_a_server_held_up_for_up_to_a_second_stops_its_engine_and_is_put_right_no_more`.
Every second on its clock, the server runs nothing for 250, 500, 750 and
then 1000 ms in turn (`--test-held-up 1 1000`, the server's own flag, as
`server_paced_prediction.cmake -DSERVER_HELD=unevenly` and
`server_first_input.cmake` use it). That works the same on every platform.
The test counts the hold-ups from the server's own word rather than assuming
them: at least 10 from the engine's stop (20 s) on, and at least 2 of them a
full second. The bounds are the plain run's, not loosened. It runs alone
(RUN_SERIAL), so that the only hold-ups are the ones the test makes. Port
24872.

**Costs.** The new test is costed in every `tests/ci_costs/*.txt` table at its
plain twin's figure (36 to 43 s) until CI measures it, since both fly on the
session's clock. With it added, the fourteen Linux debug shards each held
45.0 minutes, past 75% of their hour, and
`every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once`
failed. Linux debug's tests now run in fifteen shards (`ci.yml`), and the
shards test passes.

**Verification.**

- linux-debug: passes. The server was held up 32 times, 13 of them from the
  engine's stop on and 4 of those a full second. The median speed error
  where it was came to 0.0047 m/s after the stop against 0.108 before, and
  the median distance to 10 mm against 39 mm.
- linux-debug under load, with every one of the machine's 20 cores kept
  busy, three runs: all pass. Speed after the stop 0.0043, 0.0090 and
  0.0036 m/s; the same 32/13/4 hold-ups each time.
- **Seen red**: with the prediction's engine left running
  (`Prediction::hear_engine_stopped` not calling `fail_engine`), the
  held-up run fails. Its median speed error after the stop was 0.506 m/s,
  against the 0.1 m/s bound. Reverted, and it passes again.
- windows-debug (`tools/windows_build.sh`): the held-up run passes, and so
  do the plain and slowed runs.
- The plain and slowed runs pass on linux-debug, as does
  `every_fixed_test_port_lies_outside_the_ephemeral_ranges_and_no_two_tests_share_one`.

**What the variant does not reproduce.** In these runs the clocks'
difference placed the words 0 steps off at the median. The CI failures it
stands for showed 15 to 95 steps off, as did the 2026-10-17 SIGSTOP
reproduction, which also paced the client at 1.19 to 1.33. A hold that the
server catches up within the second does not move the difference that far.
What it does pin is the held-up server, built on every platform.

**The earlier CI search is withdrawn, not redone.** It searched failed
jobs only, ignoring cancelled ones, with `gh api .../logs` run without
`--allow-escape-sequences`, and under the wrong test's name. Doing it again
properly is a big job. From 2026-10-23 there were 951 runs and 37,514 jobs;
4,825 of those were test jobs that failed, were cancelled or timed out,
with names like `..._-_tests` or `multi-process_tests`. Fetched 8 to 12 at a
time, their logs came at about one job every few seconds, which is hours.
The result also no longer decides the item: the month it must show has to
be run with the held-up variant, so it starts the day this lands. When it
is owed, the search goes over every failed, cancelled and timed-out test
job since then, with
`gh api repos/<repo>/actions/jobs/<id>/logs --allow-escape-sequences`, for
the three tests' names alongside `***Failed`, `***Timeout`, `(Failed)` or
`(Timeout)`.

No code in `src/` changed. The selftest hash is not touched.
