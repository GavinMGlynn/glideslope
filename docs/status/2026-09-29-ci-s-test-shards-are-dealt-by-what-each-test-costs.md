### CI's test shards are dealt by what each test costs, 2026-09-29 — tail done

Every test shard's job on this branch's CI run 36512248457 finished under two
thirds of its 30-minute limit, 20 minutes; the worst was Windows debug's 17.7.
The Linux debug floor is still its longest test (below), and one test failed
once on that run and passed when run again (below): neither is hidden by the
tick.

**What was wrong.** Shard k of n ran every nth test from the kth
(`ctest -I k,,n`), so the slow tests fell where their numbers put them. On
main's green run 36291751627 (2026-09-27) the Linux debug shards' jobs took
18.9, 20.8, 23.1 and 26.3 minutes against 30 - and 27 min 43 s on another run -
because the circuit lessons (838 s and 584 s), the three near-the-ceiling
turns (569, 463, 451 s) and the low-downwind circuit (433 s) shared shards.
Windows debug's worst was 18.2, macOS debug's 17.5, Windows release 13.6,
Windows clang 12.9, macOS release 8.4, Linux release 8.4. Rocky 9 runs the
whole linux-release suite in its 60-minute job, 27.4 minutes with the build,
and is left as it is.

**What changed.** `tools/ci_shard.cmake -DPRESET= -DSHARD= -DOF= -DOUT=` deals
every test of `build/PRESET` to one of OF shards, longest first, each to the
shard with the least work so far, and writes the shard's test numbers as
ctest's `-I` file and the costs as ctest's `CTestCostData.txt`, so inside a
shard the longest tests start first.

- The costs are `tests/ci_costs/<preset>.txt`, each test's wall time on CI
  rounded up, the longest of main's green runs 36306798156 and 36302324581,
  measured by `tools/ci_test_costs.py RUN_ID...`. A test not in the table
  counts 60 s; each such test, and each name in the table that is no longer a
  test, is a `::warning::` on the run's page, so the table is measured again.
- A test that holds the runner - `RUN_SERIAL`, or `PROCESSORS` above one, from
  ctest's own listing - weighs its time times what it holds. What it holds is
  counted out of the test preset's `jobs`, read from `CMakePresets.json`, so
  the workflow does not say it a second time.
- **A fixture is dealt whole**: its setup and every test that needs it are one
  unit weighing their sum. The first run dealt them apart, and ctest runs a
  setup in every shard with a test needing it: the cockpit view, which the
  other six views hold to, ran six times on Linux debug (about 830 s nobody
  had counted) and five times per driver on Windows debug. A fixture whose
  tests weigh more than half a shard's fair share - the downloads and the
  model sources, seconds to set up and needed by a third of the suite - is
  left to be set up again wherever it is needed, since dealt whole it would
  be a shard's work in one lump. Two fixtures dealt whole that share a test
  are one unit.
- Linux debug has seven shards (was four) and macOS debug four (was three);
  the others keep theirs.

**Coverage is asserted, twice.** The dealing counts that every test was dealt
exactly once and that no shard is empty, and fails the job if not; seen
failing with one unit dropped from the dealing. And
`every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once`
(`tests/cmake/ci_shards.cmake`, about 12 s) reads the workflow's matrix, checks
that each preset's rows are shards 1 to n once each, all saying "of: n", and
that each test job deals with its own row's figures and hands the file to
ctest; then, for all 7 presets and all 25 shards, deals this build's tests from
that preset's table and reads the -I files back: every test in exactly one.
Seen failing on each: a row naming shard 3 twice and no 4 ("the matrix has
shards 1;2;3;3, not 1 to 4 once each"), a job dealing a fixed shard, a dealing
that drops a unit, and -I files that leave out each shard's first test
("576 tests, 569 dealt ... missing: 1;2;3;4;5;8;14").

**The first run, 36299512726** (seven Linux debug shards, three macOS debug,
fixtures dealt apart, costs from one run): green, and every job under 20
minutes, but uneven - Linux debug 8.7 to 15.6 against equal predictions, the
unaccounted cockpit views - and macOS debug's worst 17.6, no better than
before. Job minutes, worst shard per preset: Linux debug 15.6, Linux release
12.6, macOS debug 17.6, macOS release 7.8, Windows debug 16.6, Windows release
13.9, Windows clang 12.8.

**The floor** for Linux debug is its longest test, the circuit lesson flown by
the book: 838 s on one run and 1053 s on another, so a shard holding it takes
at least 18 to 19 minutes with setup on a slow run. That is under 20 but not by
much; splitting the circuit lesson would lower it, and is not done here.

**Verification: run 36512248457**, rebased on main, fixtures dealt whole,
four macOS debug shards. Job minutes (test step in brackets), each against
20:

| preset | shards | every shard's job | worst | first run's worst | before (main) |
|---|---|---|---|---|---|
| linux-debug | 7 | 17.4, 16.2, 14.5, 14.4, 14.6, 15.5, 14.3 | 17.4 (15.6) | 15.6 | 26.3 |
| linux-release | 2 | 13.5, 9.2 | 13.5 (12.1) | 12.6 | 8.4 |
| macos-debug | 4 | 11.8, 10.6, 10.9, 11.8 | 11.8 (10.9) | 17.6 | 17.5 |
| macos-release | 2 | 9.4, 9.3 | 9.4 (8.1) | 7.8 | 8.4 |
| windows-debug | 6 | 14.1, 16.7, 15.7, 12.9, 17.7, 13.3 | 17.7 (16.0) | 16.6 | 18.2 |
| windows-release | 2 | 11.7, 11.3 | 11.7 (10.2) | 13.9 | 13.6 |
| windows-clang | 2 | 12.2, 12.5 | 12.5 (10.9) | 12.8 | 12.9 |

The dealing's own estimates are work divided by the jobs at once, not a
schedule, and were within a few minutes: the spread left is runner speed and
the tests that hold the runner. Linux release's first shard is the heavier
because the downloads fixture is set up in both and its serial tests landed
together; at 13.5 it is well inside.

**Found on the way.** On this run's first attempt Windows clang's shard 2 failed
`a_player_takes_over_an_ai_aircraft_with_no_step_at_100_ms_and_a_players_is_refused`
at 30.5 s: "the server did not refuse player's aircraft 2". Run again, the
shard passed (12.5 minutes). It is a `RUN_SERIAL` test, alone on the runner
whichever shard it is dealt to, so the dealing does not explain it; it is a
tail in the plan.
