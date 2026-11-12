# The gust_sweep tests in shards of their own, on a schedule and a dispatch

**Not verified yet.** A dispatched run has to show every shard finishing
inside its limit. Until then this item is open.

## What was wrong

Every scheduled run of ci.yml from 2026-11-05 to 2026-11-11 was cancelled
(runs 42115457538 to 43111884667). In run 43111884667, every one of the 41
test shards was cut off at its job limit: 45 minutes on macOS, Rocky and
Windows, and 60 on Ubuntu. The last green scheduled run was 41918525620,
on 2026-11-04.

- From 2026-11-05 to 11-11, 51 tests labelled gust_sweep were added beside
  the gust seeds 9 to 40 (commits ef5592b6, 94f4bfd6, 65515620, 83845b55
  and 1913acb2): fifteen pairs going around together, fifteen circuits with
  every straight-in and departure, fifteen of every person straight in,
  departing or staying in the circuit, and six more.
- No pull request's run includes gust_sweep, and tools/ci_test_costs.py had
  only been given pull requests' runs. So none of these tests had a cost,
  and tools/ci_shard.cmake dealt each of them at its default of 60 s. One
  shard's log warns of this: "103 tests of macos-release are not in
  tests/ci_costs/macos-release.txt and count 60 s each".
- They are far longer than that. On CI, the person tests take 1,400 to
  2,200 s each on release presets. On linux-debug, an_ai_aircraft_behind_a_person_who_flies_away_on_every_other_bearing
  was still running at 2,486 s when its shard was cut off.
- The shard counts are sized for a pull request's tests, so the scheduled
  run got the same shards plus several hours of work. Below is the
  gust_sweep work that the new tables hold for each preset. Each figure is a
  floor, because many of these tests were cut off before they finished,
  and is given at the preset's tests-at-once. The work of every other test
  is in brackets for comparison:
  - linux-debug: 274 min (529)
  - linux-release: 102 min (83)
  - macos-debug: 135 min (172)
  - macos-release: 109 min (82)
  - windows-debug: 155 min (320)
  - windows-release: 152 min (113)
  - windows-clang: 151 min (99)

## What changed

- tools/ci_test_costs.py --finished now also counts a test that was cut off
  when its shard was. Its cost is the time it had run, and the table's
  header gives the number of such floors. A job whose log is gone is
  skipped with a warning. Run 43111884667's linux-debug shard 11 has no
  log (BlobNotFound).
- The tables were measured again from the four pull-request runs that
  already fed them, plus the scheduled runs 43111884667 (cancelled) and
  41918525620 (the last green one, which has seeds 9 to 40). 34 of the
  costs are cut-off floors.

- **The sweep jobs** in ci.yml: ubuntu-sweep, rocky-sweep, macos-sweep
  and windows-sweep. They run on a schedule and on a dispatch only, and
  carry the tests labelled gust_sweep alone (`-L gust_sweep`). The shard
  jobs now leave gust_sweep out on every event, so a pull request's shards
  and their 45-minute limit are unchanged.
  - Each row's `minutes` is its job's timeout-minutes. It is the preset's
    longest sweep test × 1.5, at least 60 (linux-debug's longest floor of
    2,486 s gives 63).
  - The debug presets get 90 minutes, because most of their figures are
    floors and a person test's ctest limit is 3,600 s.
  - Each preset has the fewest shards that hold every shard's work to
    three fifths of 60 minutes (63 for linux-debug). The counts are
    linux-debug 10, linux-release 5 on Ubuntu and 5 on Rocky 9,
    macos-release 4, macos-debug 7, windows-debug 6, windows-release 5 and
    windows-clang 5: 47 jobs.
- **tools/ci_shard.cmake** has three new options:
  - INCLUDE deals the tests of one label, matching the run's `-L`.
  - DEFAULT=longest costs a test that the table does not name at the
    longest that it does.
  - MINUTES checks the sizing: each shard's work at most three fifths of
    MINUTES, and the longest test × 1.5 inside it. CI gets a warning on the
    run's page; with STRICT the check fails.
- **tests/cmake/ci_shards.cmake**, which ctest runs as
  every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once,
  now also checks the sweep jobs:
  - It reads each sweep job's rows, and deals and reads back each one with
    `-L gust_sweep`, STRICT at its row's minutes.
  - It asserts that every preset that is sharded is also swept, and that
    every sweep job's limit is its row's minutes.
  - It deals the shard jobs' new other-event ways, which leave out
    timing|gust_sweep and gust_sweep.
  - Seen red twice, then reverted both times:
    - macos-debug's rows set to 30 minutes failed the test: "shard 1 holds
      32.3 min of work".
    - One sweep job's limit written as 90 failed it: "4 gust sweep runs, 4
      jobs named -sweep, 3 limits of the row's minutes".
- **The gust_sweep tests' ctest TIMEOUT** now comes from the cost tables, at
  configure time. It is 1.5 × the slowest preset's figure, or the limit
  already set if that is longer. Five circuits with every straight-in and
  departure had given up at 1,800 s on linux-debug in run 43111884667; they
  now have 2,702 s. Other circuits, pairs and the person tests have
  2,082 to 3,600 s.
- **tools/ci_test_costs.py** also measures shards named "gust sweep".

- **The shard jobs' counts**: each shard's dealt work is now at most three
  quarters of its job's limit. tests/cmake/ci_shards.cmake asserts this for
  every shard job (MINUTES is read from the job's timeout-minutes, with
  SHARE=75 and STRICT), and the limits are unchanged. #367's full run had
  Windows debug tests 6/8 finish 206 of 206 at 16:33:07 and get cut off at
  16:33:20; #359's had shard 8/8 cut off at 203 of 207. With the counts
  unchanged, the test failed: "1_linux-debug ... shard 2 holds 51.4 min of
  work, over 75% of 60". The largest shard's dealt work, in minutes, for a
  pull request's way and then a night's (without gust_sweep):

  | preset (limit) | before | after |
  | --- | --- | --- |
  | linux-debug (60) | 11 shards: 51.4 / 51.9 | 13: 43.4 / 43.9 |
  | linux-release, Ubuntu (60) | 2: 59.8 / 62.4 | 3: 39.9 / 41.6 |
  | linux-release, Rocky 9 (45) | 4: 29.9 / 31.2 | 4, unchanged |
  | macos-release (45) | 3: 40.5 / 42.4 | 4: 30.4 / 31.8 |
  | macos-debug (45, night only) | 5: 44.3 | 7: 31.6 |
  | windows-debug (45) | 8: 42.9 / 43.7 | 11: 31.2 / 31.8 |
  | windows-release (45) | 4: 35.8 / 37.3 | 5: 28.6 / 29.8 |
  | windows-clang (45) | 4: 32.5 / 34.0 | 5: 26.0 / 27.2 |

  These are from the re-measured tables, which take the longer of the pull
  requests' and the scheduled runs' figures. The new counts add eleven
  test jobs, nine of them to a pull request's full run (macos-debug runs
  only after a merge and at night). A full run's jobs now queue beyond the
  plan's 40 at once.

## Runner time

A scheduled run, or a dispatch, gains about 1,520 minutes of dealt work.
With each shard's setup (about 8 minutes, 47 jobs) that comes to about
1,900 runner-minutes a night. About 420 of those are on macOS, at five jobs
at once. Pull requests' runs are unchanged.

## Verification

Not yet verified: one `gh workflow run ci.yml --ref scheduled-ci-shards`
has to show every shard inside its limit.
