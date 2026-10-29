### A test that fails twice for one cause runs nightly, named, 2026-10-29

**What was wrong.** A test failing twice on CI for the same cause is, by the
owner's rule (2026-10-28), fixed or named and moved to the nightly run, and
there was no way to move one. A pull request's run is the only place it could
fail.

**What changed (CI and test registration; no product code).**
- A new ctest label, `nightly`. Every test job's ctest line in
  `.github/workflows/ci.yml` leaves it out on `pull_request` events
  (`-LE nightly`, or `-LE "timing|nightly"` on debug presets, which already
  left `timing` out). Scheduled, push and manual runs are unchanged: they run
  the label, except `timing` on debug as before.
- `tools/ci_shard.cmake` costs a nightly test at nothing (it is dealt, so the
  dealing still counts every test once, but a pull request never runs it). The
  nightly run runs it in whatever shard it was dealt to: a few minutes of slack.
- `tests/cmake/nightly_tests.cmake` names every nightly test with its reason
  and the CI runs that put it there. `tests/cmake/ci_shards.cmake` (the
  existing `ci_shards` test) now fails unless the tests labelled nightly are
  exactly those named (count 1), and unless all 7 test jobs leave the label out
  of pull requests. It still asserts every test is dealt once per preset.
- First test moved: `a_client_predicting_against_a_server_held_up_unevenly_every_second_is_within_a_metre_for_ninety_five_updates_in_a_hundred`
  (RUN_SERIAL kept). Windows 95th percentile 1,143 mm in run 40743161009 and
  3.3 m worst in 40673705998 (its worst bound has since covered that); 0.49 m
  and 0.66 m elsewhere. The cause is the open Phase 12a item on Windows' 15.6 ms
  timers; the test runs nightly until it is fixed.

**Verification.** `ci_shards.cmake` run against a scratch build of 121 tests
with the label on one: 1 nightly test named, 7 of 7 jobs leave it out, all 8
dealings dealt once. With the label removed from the scratch build it fails
("1 nightly tests expected; 1 named, 0 labelled"). The worktree had no
initialised submodules, so the real build was not configured here: CI is the
proof, and the PR's own logs show the test's absence. No actionlint was
available; the workflow edit is read by eye, one expression text across 7 lines.

**A named test's red nightly result is reported, not a red tree.** A test
named in `nightly_tests.cmake` may fail in the scheduled run until its item is
fixed; that is reported in that item (here the open Phase 12a item on
Windows' timers). The owner decided it on 2026-10-29, and CLAUDE.md says so:
a nightly failure is a red tree but for a test named there. Any other test
failing at night is still a red tree.

**From the review.** The workflow check now reads each test job's ctest line
whole: it must end in one of the two gate expressions exactly, so a gate
inverted or one leaving `timing` in a debug pull request's run fails it, and
the label is listed as `-L "^nightly$"`, so `nightly_x` is not counted. Seen
red against a scratch build of 41 tests, each then put back: a labelled test
not named ("1 named, 2 labelled"); the named test unlabelled ("1 named, 0
labelled"); `timing` dropped from windows-debug's line, and the gate inverted
on a Windows job ("does not end in a gate that leaves the nightly label out").
With `nightly_x` on the extra test instead, it passes.

**The pull request's half, seen.** Full run 40770584919 (green) ran, on
Windows debug, `ctest --preset windows-debug -I %CD%\shard.txt -LE
"timing|nightly"`; on Linux release `ctest --preset linux-release -I
"$PWD/shard.txt" -LE nightly`; on macOS release `ctest --preset macos-release
-I "$PWD/shard.txt" -LE nightly`.

**Owed.** That the test runs every night is owed until the first scheduled run
of ci.yml on main after this lands (cron 16:00 UTC); its item stays open until
that run's log shows it run.
