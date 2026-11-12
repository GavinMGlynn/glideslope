# The gust_sweep tests in shards of their own, on a schedule and a dispatch

**Not verified yet, and the gust sweep's debug presets may not fit at all.**
The first dispatch, run 43257673789, cut off 22 jobs: 18 gust sweep shards
and 4 test shards. That run's figures resized everything below, and a
second dispatch has to show every shard finishing inside its limit. Every
person test timed out at 3,600 s on all three debug presets, so its real
time there is unknown. If the debug sweep still does not fit, the person
tests need splitting, or the gust sweep has to leave the debug presets.
That is the owner's decision.

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
  - Each row's `minutes` is its job's timeout-minutes: the preset's
    longest sweep test × 1.5 (that test's own ctest limit) plus 10 minutes
    for the job's setup, and at least 60. Each preset has the fewest shards
    that hold every shard's work to three fifths of its minutes.
  - The first round, from the tables before run 43257673789, gave 47
    jobs: linux-debug 10×90, linux-release 5×60 on Ubuntu and on Rocky,
    macos-release 4×60, macos-debug 7×90, windows-debug 6×90,
    windows-release 5×60 and windows-clang 5×60. 18 of those shards were
    cut off.
  - Now (shards × minutes), 46 jobs: linux-debug 7×142, linux-release
    4×99 on Ubuntu and on Rocky, macos-release 5×60, macos-debug 8×101,
    windows-debug 7×111, windows-release 6×99 and windows-clang 5×99.
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
- **Every test's ctest TIMEOUT** now comes from the cost tables, at
  configure time, not only the gust_sweep tests'. It is 1.5 × the slowest
  preset's figure, or the limit already set if that is longer.
  - #365's run 43261047217 had
    the_one_behind_two_going_around_whose_leader_goes_around_again_flies_a_racetrack_and_both_land
    time out at 900 s on linux-debug, where its table figure is 856 s.
  - A figure that is a timeout, such as
    two_going_around_together_behind_the_f35b's 900, raises the test's
    limit the next time the tables are measured.
  - every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once
    gets 1,800 s by hand. It timed out at 900 s on Windows debug and
    Windows release in run 43257673789, once the sweep's shards were dealt
    as well.
  - The 54 tests the tables raise (seconds):

    | test | limit before | now |
    | --- | --- | --- |
    | a_737_300_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | a_737_300_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 4019 |
    | a_747_400_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | a_747_400_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 3753 |
    | a_787_8_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | a_787_8_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 2702 |
    | a_b2_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | a_b2_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 2702 |
    | a_c172p_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5400 |
    | a_c172p_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 3470 |
    | a_c182_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | a_c182_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 4055 |
    | a_circuit_and_a_straight_in_or_a_departure_the_slowest_and_the_fastest_each_with_each_are_kept_apart_and_in_order | 1800 | 2264 |
    | a_flight_on_its_own_flies_the_stations_around_her_the_nearers_weather_with_no_step_in_the_wind_on_vulkan | 900 | 1044 |
    | a_j3cub_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | a_j3cub_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 3953 |
    | a_learjet35a_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | a_learjet35a_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 3723 |
    | a_mosquito_fb6_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | a_mosquito_fb6_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 2702 |
    | a_pa28_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | a_pa28_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 4055 |
    | an_a320_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5237 |
    | an_a320_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 4055 |
    | an_a380_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | an_a380_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 3125 |
    | an_ai_aircraft_behind_a_person_who_flies_away_on_every_other_bearing_comes_back_and_lands_kept_apart | 5400 | 7892 |
    | an_f15c_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | an_f15c_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 4055 |
    | an_f22_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | an_f22_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 4055 |
    | an_f35b_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart | 3600 | 5402 |
    | an_f35b_circuit_and_every_straight_in_and_departure_are_kept_apart_and_in_order | 1800 | 2702 |
    | every_first_party_source_compiles_clean_under_clang_too | 900 | 930 |
    | every_landplane_pair_with_a_person_lining_up_under_it_at_200_ft_is_measured | 3600 | 5402 |
    | the_737_300_and_a320_handed_over_at_any_half_second_of_the_ten_after_lift_off_climb_away_and_never_touch_the_runway | 900 | 924 |
    | the_cessnas_handed_over_at_any_half_second_of_the_ten_after_lift_off_climb_away_and_never_touch_the_runway | 900 | 1049 |
    | the_f22a_enters_an_orbit_from_a_waypoint_in_calm_air_heading_north_or_northeast_and_holds_its_circle | 900 | 908 |
    | the_one_behind_two_going_around_whose_leader_goes_around_again_flies_a_racetrack_and_both_land | 900 | 1284 |
    | two_going_around_together_behind_the_737_300_are_kept_apart_and_both_land | 1800 | 2379 |
    | two_going_around_together_behind_the_747_400_are_kept_apart_and_both_land | 1800 | 3155 |
    | two_going_around_together_behind_the_787_8_are_kept_apart_and_both_land | 1800 | 2702 |
    | two_going_around_together_behind_the_a380_are_kept_apart_and_both_land | 1800 | 2232 |
    | two_going_around_together_behind_the_b2_are_kept_apart_and_both_land | 1800 | 2657 |
    | two_going_around_together_behind_the_c172p_are_kept_apart_and_both_land | 1800 | 2270 |
    | two_going_around_together_behind_the_c182_are_kept_apart_and_both_land | 1800 | 1907 |
    | two_going_around_together_behind_the_f15c_are_kept_apart_and_both_land | 1800 | 2082 |
    | two_going_around_together_behind_the_f22_are_kept_apart_and_both_land | 1800 | 2606 |
    | two_going_around_together_behind_the_f35b_are_kept_apart_and_both_land | 1800 | 2373 |
    | two_going_around_together_behind_the_j3cub_are_kept_apart_and_both_land | 1800 | 2535 |
    | two_going_around_together_behind_the_learjet35a_are_kept_apart_and_both_land | 1800 | 2162 |
    | two_going_around_together_behind_the_mosquito_fb6_are_kept_apart_and_both_land | 1800 | 2339 |
    | two_going_around_together_behind_the_pa28_are_kept_apart_and_both_land | 1800 | 2096 |
    | two_going_around_together_the_slowest_the_c172p_and_the_fastest_each_behind_each_are_kept_apart_and_both_land | 1800 | 2010 |
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
  | linux-debug (60) | 11 shards: 51.4 / 51.9 | 13: 44.5 / 44.9 |
  | linux-release, Ubuntu (60) | 2: 59.8 / 62.4 | 3: 40.7 / 42.4 |
  | linux-release, Rocky 9 (45) | 4: 29.9 / 31.2 | 4, unchanged: 30.5 / 31.8 |
  | macos-release (45) | 3: 40.5 / 42.4 | 4: 30.8 / 32.2 |
  | macos-debug (45, night only) | 5: 44.3 | 7: 32.6 |
  | windows-debug (45) | 8: 42.9 / 43.7 | 11: 31.6 / 32.2 |
  | windows-release (45) | 4: 35.8 / 37.3 | 5: 29.4 / 30.6 |
  | windows-clang (45) | 4: 32.5 / 34.0 | 5: 26.7 / 27.9 |

  "Before" is from the tables measured before run 43257673789 and "after"
  from the tables measured with it. Both take the longer of the pull
  requests' and the scheduled runs' figures. Run 43257673789 confirms the
  "before" column: on the old counts, macos-debug tests 1/5 and 3/5 were
  cut off at 45 minutes, and Ubuntu linux-release tests 2/2 at 60. The new counts add eleven
  test jobs, nine of them to a pull request's full run (macos-debug runs
  only after a merge and at night). A full run's jobs now queue beyond the
  plan's 40 at once.

- The tables were measured again with dispatch run 43257673789 added. Each
  figure for a test that was cut off, or that hit its ctest limit, is a
  floor. The gust sweep's work, in minutes at each preset's tests-at-once,
  went from the floors above to: linux-debug 577, linux-release 215,
  macos-debug 461, macos-release 147, windows-debug 435, windows-release
  303 and windows-clang 261.

## Runner time

A scheduled run, or a dispatch, gains about 2,610 minutes of dealt sweep
work. With each job's setup (about 10 minutes, 46 jobs) that comes to
about 3,070 runner-minutes a night, of which about 690 are on macOS, five
jobs at once. A pull request's full run gains nine shard jobs (above), and
the dealt minutes do not change.

## Dispatch run 43257673789 (round 1): the slowest shard per preset

| preset | tests: slowest / limit | gust sweep: slowest / limit |
| --- | --- | --- |
| linux-debug | 54.8 / 60 | 90.3 / 90, 3 of 10 cut off |
| linux-release (Ubuntu) | 60.3 / 60, 1 of 2 cut off | 60.2 / 60, 1 of 5 cut off |
| linux-release (Rocky 9) | 28.9 / 45 | 60.1 / 60, 1 of 5 cut off |
| macos-debug | 45.3 / 45, 2 of 5 cut off | 90.4 / 90, 3 of 7 cut off |
| macos-release | 38.3 / 45 | 44.3 / 60 |
| windows-debug | 44.1 / 45 | 90.1 / 90, 5 of 6 cut off |
| windows-release | 40.7 / 45 | 60.1 / 60, 5 of 5 cut off |
| windows-clang | 37.2 / 45 | 60.1 / 60, 4 of 5 cut off |

Tests that genuinely failed, besides the ones that timed out:
- a_j3cub_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart
  failed on linux-release (Ubuntu and Rocky 9), macos-release and
  windows-clang. #367 is fixing it.
- a_client_joining_an_f35b_mid_bank_with_the_first_datagrams_lost_still_begins_on_the_servers_law_state
  failed on linux-debug ("***Failed 120.02 sec"). It is labelled nightly
  (tests/cmake/nightly_tests.cmake).
- On linux-debug, macos-debug and windows-debug, every person test timed
  out at its 3,600 s, as did most circuits at 1,800 to 2,702 s.

## Verification

Not yet verified: one `gh workflow run ci.yml --ref scheduled-ci-shards`
has to show every shard inside its limit.
