# The nightly's multi-process tests in shards, dealt by what they cost

**The shards fit their runners; the nightly is still red.** In dispatch run
43507588675 every shard finished inside its limit. Six were green. Shard 5
failed on one intermittent test, which is followed in its own item. The
nightly stays red until that item lands (below).

## What was wrong

nightly.yml's job "The multi-process tests, five times each" runs every test
whose name says it runs programs against each other (`-R
'server|client|window|player|collision|systemd|online|package' -LE timing`),
five times each, in one job of 120 minutes. It was cut off at that limit
every night from 2026-09-28 to 2026-11-12, six weeks, and nobody saw it: a
cancelled job is not a failed test.

Measured from the job logs (every run of every test, its seconds summed):

| night      | job          | tests picked | finished | test-minutes run |
|------------|--------------|-------------:|---------:|-----------------:|
| 2026-09-27 | 108695499124 | 100 | 100, in 98 of 120 min | 184 |
| 2026-09-28 | 109162051891 | 100 | 98, cut off           | 225 |
| 2026-10-06 | 112508614975 | 185 | 106, cut off | |
| 2026-10-15 | 116644172769 | 226 | 123, cut off | |
| 2026-10-23 | 120631421671 | 245 | 149, cut off | |
| 2026-10-29 | 123743720341 | 268 | 143, cut off | |
| 2026-11-10 | 130160911232 | 275 | 110, cut off | 234 |
| 2026-11-11 | 130851270956 | 278 | 113, cut off | 232 |
| 2026-11-12 | 131539088468 | 282 | 85, cut off  | 232 |

**What pushed it over** was slower tests first: on 2026-09-28 the same 100
tests took 22% longer than the night before (the server window's dashboard
test 917 s for its five runs against 1,520 s; the Vulkan view tests 26-40%
slower), in a job already at 82% of its limit. After that the names picked
nearly three times as many tests - 100 to 282, as tests with "player",
"client" or "server" in their names were added - so the job could not have
finished on any night since. Every test run that finished in the three
nights 11-10 to 11-12 passed (558, 567 and 427 runs).

## What lands

- **nightly.yml's repeat job is a matrix of seven shards**, each dealt by
  tools/ci_shard.cmake from tests/ci_costs/linux-debug.txt: the same
  machinery and the same table as ci.yml's shard jobs. The names it picks
  are one variable, `MULTIPROCESS`, given to the dealing and to ctest.
  One shard saves the night's downloads cache.
- **tools/ci_shard.cmake takes REGEX and REPEAT.** REGEX is the run's -R;
  the list is taken with `-FA ".*"`, since ctest numbers -I's tests before
  it adds a fixture's setup that -R did not pick (the downloads): listed
  with it, every test after it was dealt one place off, which the read-back
  below caught. REPEAT is `--repeat until-fail:N`: each test costs N times
  its figure, and the longest test's N runs, half as long again, must fit
  the limit.
- **Four at a time, not two.** The job ran `-j2`; the shards run the test
  preset's four, which is how every cost in the table was measured, so the
  dealing's minutes are the table's. The table is conservative here: the
  nightly's own runs at two at a time took a median 0.57 of the table's
  figure for the same test.
- **tests/cmake/ci_shards.cmake checks the nightly's job** as it checks
  ci.yml's: its rows are one preset's shards 1 to n once each; its dealing
  is its own row's, with the job's names, its ctest line's repeats and the
  job's limit; every shard's -I file, read back by ctest with that -R and
  -LE, runs exactly what it was dealt, and the shards between them run all
  281 picked tests once; and on a Linux host every shard holds at most three
  quarters of the job's 120 minutes (STRICT). A night's job that grows
  fails this test on a pull request before it is cut off at night.

## The shards

As CI's linux-debug dealing gave them in dispatch run 43507588675: 281
tests picked, each run five times, four at a time.

| shard | tests | dealt work | limit | three quarters |
|------:|------:|-----------:|------:|---------------:|
| 1 | 40 | 82.6 min | 120 | 90 |
| 2 | 40 | 82.6 min | 120 | 90 |
| 3 | 41 | 82.6 min | 120 | 90 |
| 4 | 40 | 82.6 min | 120 | 90 |
| 5 | 40 | 82.6 min | 120 | 90 |
| 6 | 40 | 82.6 min | 120 | 90 |
| 7 | 40 | 82.6 min | 120 | 90 |

## The other failures of 2026-11-03 to 2026-11-11

"Every speed a plan may ask of every aircraft" failed every night from
2026-11-03 to 2026-11-11 on one aircraft: the F-22A held 39 of her 42 speeds,
not 197, 222 and 237 kt round her orbits in calm air (artifact of run
43118116357: 2-3 ft off, her speed hunting). Genuine, and already fixed:
#363 (bca0d82c, 379ce47d) gave her throttles to her engines without a lag
and the autothrottle a lead, and the night of 2026-11-12 (run 43320350722)
held all 42 (docs/status/2026-11-11-the-f22a-holds-every-plan-speed-again.md).

## Verification

- `every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once`
  passes on linux-release (ctest -j4, no display).
- Seen red, each reverted: the job's limit at 90 minutes (every shard's 82.6
  minutes over three quarters of it); the ctest line repeating three times
  against the dealing's five; and, before `-FA` in the dealing's list, shard
  1's read-back running other tests than it was dealt.
- Dispatch run 43507588675 (`gh workflow run nightly.yml --ref
  nightly-multiprocess-shards -f linux_repeats=true -f
  macos_network_repeats=0`), at 6ef5028f. Every shard finished inside its
  120-minute limit:

  | shard | job minutes | result |
  |------:|------------:|--------|
  | 1 | 70.8 | green |
  | 2 | 79.8 | green |
  | 3 | 86.2 | green |
  | 4 | 69.8 | green |
  | 5 | 80.5 | failed |
  | 6 | 85.0 | green |
  | 7 | 70.9 | green |

  Shard 5 failed on
  `a_player_joining_after_another_has_left_is_put_on_a_layer_no_aircraft_near_the_start_holds`.
  It passed its first run (50 s) and failed its second (55 s), saying
  (job 132169186462):

  ```
  server_rejoin_layer.cmake:63: 2 players' starts were said, not the three joins
  ```

  Its cause is not yet found. It is followed in its own item in Phase 12a,
  not here, and the nightly stays red until that item lands.
- **The nightly's ctest also says `--output-on-failure`**, and the shards
  test asserts it in the ctest line. This is belt-and-braces: the test
  preset already sets outputOnFailure (CMakePresets.json), which is why the
  message above was kept. The macOS network checks already said it.
