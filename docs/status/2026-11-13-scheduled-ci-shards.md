# The scheduled run's gust sweep in shards of its own, the debug presets' a day of the week at a time

**Not verified at this head.** No run of this head has dispatched the sweep.
Dispatch run 43384660740 ran the same layout one round earlier, all seven
days, 242 jobs, and no shard reached its limit (below). Since then:

- the days come from a hash of each test's name rather than from the cost
  tables;
- the counts were re-sized from the measured tables;
- every dealing now uses ELSEWHERE.

The tests named below were run locally on linux-release.

## What was wrong

Every scheduled run of ci.yml from 2026-11-05 to 2026-11-11 was cancelled
(runs 42115457538 to 43111884667). In 43111884667, all 41 test shards were
cut off at their job limits. The last green scheduled run was 41918525620,
on 2026-11-04.

From 11-05 to 11-11, 51 long tests labelled gust_sweep were added:
- fifteen pairs going around together;
- fifteen circuits with every straight-in and departure;
- fifteen with every person straight in, departing or staying;
- six more.

No pull request's run includes gust_sweep, and tools/ci_test_costs.py had
only been given pull requests' runs. So none of these tests had a cost, and
tools/ci_shard.cmake dealt each at its default 60 s into the pull requests'
shard counts. One shard's log warns of this: "103 tests of macos-release are
not in tests/ci_costs/macos-release.txt and count 60 s each". On CI they
took 1,400 to 3,550 s each on release. On debug they ran past their limits.

## The owner's decision, 2026-11-13: the debug gust sweep rotates weekly

The gust sweep stays on every preset, debug included:
- Each release preset runs every gust_sweep test every night.
- Each debug preset runs one seventh of them a night, chosen by the
  scheduled run's UTC weekday, so every test runs on every debug preset
  once a week.
- A dispatch's `sweep_day` input runs today's day, a day from 1 to 7, or
  all seven.

## What lands

- **Sweep jobs (ci.yml).** They run on a schedule and on a dispatch only.
  The shard jobs leave gust_sweep out on every event, so a pull request
  runs no sweep test.
  - Release presets: ubuntu-sweep, rocky-sweep, macos-sweep and
    windows-sweep run `-L gust_sweep`.
  - Debug presets: ubuntu-debug-sweep, macos-debug-sweep and
    windows-debug-sweep each have a matrix of shards × days. The days come
    from the sweep-days job, which reads `date -u +%u` or the dispatch's
    input. Each shard deals and runs `-L ^sweep_day_N$`.
- **A test's day is its name's.** tests/CMakeLists.txt takes the MD5 of the
  name, its first seven hex digits modulo 7, plus 1. Because a re-measure
  cannot move a test's day, no test goes a fortnight unrun or runs twice
  in a week. 589 sweep tests fall 77 to 86 a day.
- **Long tests split, so that each fits a debug job.** The rule is that a
  test may cost at most 80 minutes on debug: about 12 × release, × 1.5 for
  its limit, inside 120. A debug runner takes about twelve times a release
  one's: the racetrack test is 856 s on linux-debug and 68 s on release.
  Each split has a coverage test, each seen red once:
  - **Every landplane with every person:** 15 tests became 450 pieces. Each
    pair has 8 flights straight in, departing or staying, and 8 flying
    away. The whole tests took up to 3,553 s on release.
    the_person_pieces_between_them_fly_every_landplane_with_every_person_each_flight_once
    states the space: 15 × 15 pairs, 16 flights a pair, 3,600 flights. A
    piece flies exactly the calls that `person_piece_calls` lists, and the
    test counts those calls' flights, so what is flown is what is counted.
    A piece flies every call even when one fails, and reports every
    failure together at the end. With two bearings broken it reported both:
    "2 of 8 calls went wrong: ... flying away 45 deg ...; ... 90 deg ...".
    Reverted.
    With the raised-ground call dropped from the list, it failed: "the
    space is sixteen flights a pair: 3375". Reverted. With one pair's
    pieces left out, it failed: "16 flights dropped".
  - **The lining-up test:** 30 pieces (3,424 s on release before the
    split). Covered by the_lining_up_pieces_between_them_measure_every_landplane_pair_once.
  - **The circuit tests:** 45 pieces, a situation each (797 s), and **the
    three diversion tests:** 9 pieces, a third of the landplanes each
    (687 s). Covered by the_circuit_and_diversion_pieces_between_them_fly_every_case_once.
  - **The other seven bearings of a person flying away:** 7 tests, a
    bearing each (967 s). Covered by the_tests_of_a_person_flying_away_fly_every_bearing_once.
- **Estimates until measured.** A piece's ctest COST is its whole test's
  largest release figure divided by its number of pieces. tools/ci_shard.cmake
  deals by that COST, × 12 on a debug preset, where the table has none.
- **tools/ci_shard.cmake** gained these options:
  - **INCLUDE:** a run's `-L`.
  - **DEFAULT=longest:** cost a test the table does not name as the
    longest it does.
  - **MINUTES, SHARE, STRICT:** each shard's work must be at most SHARE%
    of MINUTES, and the longest test × 1.5 must fit in MINUTES. On CI this
    gives a warning; with STRICT it fails.
  - **MODEL_DEBUG:** a debug preset's sweep is dealt at least at 12 ×
    release. A debug figure is often a floor; three pairs going around
    gave up at 1,800 to 2,096 s on linux-debug in 43384660740.
  - **ELSEWHERE:** every dealing passes it, CI's and the check's alike. A
    test that this preset's table does not name, nor any table of its own
    operating system, but another system's table does, costs nothing: it
    is that system's own and this preset never runs it. The check and CI
    therefore cost every test the same.
    - This is why only Windows hosts failed #368's run 43384652902. The
      dealing test deals every preset from the test list of the build it
      runs in. A Windows build has 46 Direct3D 12 tests that linux-debug's
      table never measured, each costed at 60 s. That is 2,760 s over 13
      shards × 4, 0.9 min a shard: 45.4 of 45 minutes, as job
      131752441587 logged.
    - A test of this preset's own system that no table of that system has
      measured, and no other system's either, costs DEFAULT in both.
    - The case it gets wrong: a new test that runs everywhere and that a
      Linux table measures before macOS's tables are measured again costs
      0 on macOS, not DEFAULT, because only another system's table names
      it. macOS debug is the likeliest victim, since it is measured only on
      scheduled runs. It corrects itself at the next re-measure of macOS's
      tables (tools/ci_test_costs.py over a run with macOS shards), which
      gives the test its own figure there.
- **A shard job is sized on its own operating system's hosts.**
  - What went wrong: the dealing test deals every preset from the test
    list of the build it runs in. Train run 43446046011 failed it on macOS
    and Windows alone. A macOS host dealt macos-debug's night at 34.7
    minutes a shard at 7 shards (job 131962073795), and a Windows host
    dealt windows-debug's at 34.2 at 11 (job 131969273982), both past
    33.75. A Linux host dealt the same at 31.1 and 32.5, because it lacks
    those systems' own tests (_on_metal, _on_direct3d12).
  - Now every host asserts every preset's coverage, but sizes (STRICT, a
    pull request's way and a night's) only the shard jobs of its own
    system's presets. Each preset is therefore sized on its own list,
    wherever the check runs.
  - A full pull-request run has Linux, macOS and Windows hosts, so every
    preset is sized; a draft's has Linux and Windows only. Locally (Linux)
    only the Linux presets are sized. The macOS and Windows presets are
    verified by the next full run on those hosts.
  - An earlier version of this round dealt the missing tests as "phantoms"
    from the table, which reproduced the hosts' figures within about 0.1
    min (34.7 and 34.3 from Linux, against 34.7 and 34.2 logged). It
    dropped those tests' properties, though: four hold the runner and
    seven share a fixture dealt whole. So it was removed rather than kept
    as an unchecked assumption.
  - The counts were set from those figures: macos-debug 8 shards (30.4)
    and windows-debug 12 (31.4).
- **tests/cmake/ci_shards.cmake** (ctest:
  every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once)
  now also checks the following:
  - every sweep job's rows and limit;
  - every debug day dealt into its shards at 75% of its limit, each
    shard's -I file read back;
  - that the seven days between them are every gust_sweep test, each
    once. With one piece left without a day it failed: "the seven days:
    588 runs of 588 tests; 589 gust_sweep tests; 0 run twice; dropped: ...".
    Reverted;
  - that every sharded preset is swept;
  - that every shard job is sized at 75% of its limit, a night's run as
    well as a pull request's.

  Its own limit is 3,600 s. It takes 463 s on linux-release locally, and
  once ran 901 s on Windows, past the presets' 900.
- **Every test's time limit is from the cost tables** (end of
  tests/CMakeLists.txt). It is at least 1.5 × the test's slowest figure in
  any table. A gust_sweep test in a debug build also gets 1.5 × 12 × its
  slowest release figure. Below are the limits this raises in a
  linux-release build; a debug build raises the gust_sweep tests' further.
- **tools/ci_test_costs.py**:
  - `--finished` counts a test cut off with its shard at the time it had
    run, a floor;
  - it skips a job whose log is gone;
  - it measures the sweep shards' jobs;
  - it retries `gh`.

  The tables were measured from pull-request runs 43138678345,
  43129942786, 42838785343 and 42103685797, and from scheduled or
  dispatched runs 43111884667, 41918525620, 43257673789 and 43384660740.
  The 37 whole tests that were split were then dropped from them.

## Shard counts (each shard's dealt work against its job's limit)

Pull requests' shard jobs: the limits are unchanged, and each shard holds at
most 75% of its limit. The figures are for a pull request's way, then a
night's.

| preset (limit) | before | now |
| --- | --- | --- |
| linux-debug (60) | 11 shards: 51.4 / 51.9 | 14: 42.0 / 42.4 |
| linux-release, Ubuntu (60) | 2: 59.8 / 62.4 | 3: 41.5 / 43.3 |
| linux-release, Rocky 9 (45) | 4: 29.9 / 31.2 | 4: 31.1 / 32.5 |
| macos-release (45) | 3: 40.5 / 42.4 | 4: 26.0 / 27.4 |
| macos-debug (45, no PR) | 5: 44.3 (night) | 8: 30.4 (night) |
| windows-debug (45) | 8: 42.9 / 43.7 | 12: 30.9 / 31.4 |
| windows-release (45) | 4: 35.8 / 37.3 | 5: 29.4 / 30.6 |
| windows-clang (45) | 4: 32.5 / 34.0 | 5: 26.8 / 28.0 |

A pull request's full run gains eleven shard jobs, a draft's seven
(linux-debug three, windows-debug four); macOS debug's three more run only
at night. The macOS and Windows figures are unverified until a full run
deals them on those hosts.

Gust sweep, 60% of a release limit and 75% of a debug one:
- Release, every night, 60 minutes: linux-release 6 shards on Ubuntu and 6
  on Rocky 9, macos-release 5, windows-release 10, windows-clang 8.
- Debug, one day, 120 minutes: linux-debug 6 shards (dealt 437–465 minutes
  a day), windows-debug 8 (483–632) and macos-debug 4 (222–299).

## Runner minutes a night (dealt minutes, plus about 10 minutes of setup a job)

| | jobs | dealt minutes | runner-minutes |
| --- | --- | --- | --- |
| debug sweep, one day | 18 | about 1,320 | about 1,500 |
| release sweep | 35 | about 1,200 | about 1,550 |
| a night's sweep | 53 | about 2,520 | about 3,050, about 520 on macOS |
| a dispatch of all seven days | 161 | about 10,440 | about 12,050 |

## Dispatch run 43384660740 (the previous round's layout, all seven days)

| preset | tests: slowest / limit | gust sweep: slowest / limit |
| --- | --- | --- |
| linux-debug | 47.6 / 60 (13 shards) | 94.4 / 120 |
| linux-release, Ubuntu | 39.7 / 60 | 34.4 / 60 |
| linux-release, Rocky 9 | 34.2 / 45 | 34.0 / 60 |
| macos-debug | 35.8 / 45 | 26.0 / 120 |
| macos-release | 30.3 / 45 | 19.6 / 60 |
| windows-debug | 36.8 / 45 | 49.3 / 120 |
| windows-release | 31.4 / 45 | 54.7 / 60 |
| windows-clang | 29.1 / 45 | 41.5 / 60 |

Tests that failed in it:
- an_f22_behind_a_persons_j3cub_flying_away_on_every_bearing_comes_back_kept_apart
  failed on every preset: "7 flights came back short of fuel, 8 named".
  This is the j3cub test #367 fixes.
- Three pairs going around timed out on linux-debug at 1,800 to 2,096 s.
  The twelve-times-release limit now covers them.
- every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once
  failed on Windows hosts. ELSEWHERE fixes this (above).
- Package / macOS arm64 tarball: "the weather could not be had: could not
  download: https://api.open-meteo.com/...". That is a network fetch, not
  this item's.
- Not this item's: a_client_joining_an_f35b_mid_bank_with_the_first_datagrams_lost_still_begins_on_the_servers_law_state
  failed on linux-debug in dispatch run 43257673789. It is labelled
  nightly (tests/cmake/nightly_tests.cmake).

## Verification at this head (local, linux-release, -j4, no display)

- every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once
  passed in 463 s.
- the_person_pieces_between_them_fly_every_landplane_with_every_person_each_flight_once
  and the registration check passed.
- Four person pieces flown through their calls (c172p with f15c, j3cub
  with pa28, both kinds each) passed, 34 to 68 s each.
- The lining-up, circuit and diversion coverage tests passed.

In the previous round, all 450 person pieces ran in 5,608 s, and 449
passed (the j3cub piece above). The 66 circuit, diversion and bearing
pieces and checks passed.

## The time limits the tables raise (a linux-release build)

| tests | how many | limit before | now |
| --- | --- | --- | --- |
| person pieces, straight in, departing or staying | 200 | 900-900 | 902-2490 |
| person pieces, flying away | 225 | 900-900 | 902-3932 |
| lining-up pieces | 13 | 900-900 | 933-1409 |
| circuit pieces | 45 | 900-900 | 987-3467 |
| diversion pieces | 9 | 900-900 | 1592-2655 |
| bearing pieces | 7 | 900-900 | 1052-1695 |

| test | limit before | now |
| --- | --- | --- |
| a_circuit_and_a_straight_in_or_a_departure_the_slowest_and_the_fastest_each_with_each_are_kept_apart_and_in_order | 1800 | 2264 |
| a_flight_on_its_own_flies_the_stations_around_her_the_nearers_weather_with_no_step_in_the_wind_on_vulkan | 900 | 1083 |
| every_first_party_source_compiles_clean_under_clang_too | 900 | 930 |
| the_737_300_and_a320_handed_over_at_any_half_second_of_the_ten_after_lift_off_climb_away_and_never_touch_the_runway | 900 | 924 |
| the_cessnas_handed_over_at_any_half_second_of_the_ten_after_lift_off_climb_away_and_never_touch_the_runway | 900 | 1049 |
| the_f22a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle | 900 | 908 |
| the_one_behind_two_going_around_whose_leader_goes_around_again_flies_a_racetrack_and_both_land | 900 | 1284 |
| two_going_around_together_behind_the_737_300_are_kept_apart_and_both_land | 1800 | 3153 |
| two_going_around_together_behind_the_747_400_are_kept_apart_and_both_land | 1800 | 3248 |
| two_going_around_together_behind_the_787_8_are_kept_apart_and_both_land | 1800 | 3116 |
| two_going_around_together_behind_the_a320_are_kept_apart_and_both_land | 1800 | 2702 |
| two_going_around_together_behind_the_a380_are_kept_apart_and_both_land | 1800 | 3323 |
| two_going_around_together_behind_the_b2_are_kept_apart_and_both_land | 1800 | 3228 |
| two_going_around_together_behind_the_c172p_are_kept_apart_and_both_land | 1800 | 2391 |
| two_going_around_together_behind_the_c182_are_kept_apart_and_both_land | 1800 | 2862 |
| two_going_around_together_behind_the_f15c_are_kept_apart_and_both_land | 1800 | 3120 |
| two_going_around_together_behind_the_f22_are_kept_apart_and_both_land | 1800 | 2630 |
| two_going_around_together_behind_the_f35b_are_kept_apart_and_both_land | 1800 | 3105 |
| two_going_around_together_behind_the_j3cub_are_kept_apart_and_both_land | 1800 | 2535 |
| two_going_around_together_behind_the_learjet35a_are_kept_apart_and_both_land | 1800 | 2952 |
| two_going_around_together_behind_the_mosquito_fb6_are_kept_apart_and_both_land | 1800 | 3461 |
| two_going_around_together_behind_the_pa28_are_kept_apart_and_both_land | 1800 | 3146 |
| two_going_around_together_the_slowest_the_c172p_and_the_fastest_each_behind_each_are_kept_apart_and_both_land | 1800 | 2244 |
