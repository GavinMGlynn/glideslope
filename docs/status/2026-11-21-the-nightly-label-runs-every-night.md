# 2026-11-21: the nightly label's tests run every night and on no pull request

Phase 12a, "A test that fails twice on CI for the same cause has nowhere to
go but the pull request's run": done.

## What now works

A test named in `tests/cmake/nightly_tests.cmake`, with its reason, is left
out of every pull request's run (`ctest -LE "nightly|gust_sweep"`) and run by
the scheduled nightly, five times each. Five tests are named today.

## Verification

- The first named, the held-up-unevenly prediction test, ran five times in
  each of the last two scheduled nightlies, 44418731482 (2026-11-18) and
  44832435697 (2026-11-20), in "The multi-process tests, five times each",
  and passed every time.
- Train #387's pull request run (44716997224) left it out: its name is in
  none of its test jobs' logs.
- `every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once`
  states it: "5 nightly test, named with its reason; 11 of 11 test jobs leave
  it out of pull requests".

## Also recorded

- The engine-stop prediction item: every failed CI job since 2026-10-20
  (265 of 1,000 runs) searched for either engine-stop test; it failed only
  on 2026-10-23, on three branches' Ubuntu linux-debug, "only 144 updates
  were compared", and not since. Its month of CI runs is had on 2026-11-23.
- The busy-machine item: the 2026-11-20 nightly failed the lost-datagram
  STOVL join (a named nightly test, not a red tree).
