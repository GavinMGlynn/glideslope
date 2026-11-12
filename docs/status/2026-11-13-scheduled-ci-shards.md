# The scheduled run's gust_sweep tests costed (in progress)

**Not done yet.** The cost tables now hold the gust_sweep tests' costs, but
the scheduled run still has the pull requests' shard counts, so it still
cannot finish. The shard capacity for those tests is the open part of this
item.

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

## Verification

Not yet verified. The scheduled run, or a dispatch, has to show every shard
finishing inside its limit, and that needs the shard capacity above.
