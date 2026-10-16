### CI's cost tables measured, every one; the shards re-counted; Windows' compiler cache measured; caches pruned as they are saved, 2026-10-07 — two tails done, three still open, two found

**What is still wrong first.** One test, the AI aircraft kept 500 ft or
1.5 nm apart, takes 882 to 1,295 s in Linux debug, so its shard ran 26.1
minutes of its 30 (run 37570543750) whatever the dealing: a new tail. The
cache pruning below runs only on main, so it is not yet seen working. A
Windows configure still takes 3-6 minutes now and then (below). No new
Windows compiler came this week, so vcpkg's one-rebuild-per-image is not
yet seen. The rate-limit test failed once on Windows debug (a new tail).

**Before** (run 37561340650, green, 80 minutes from push to result with two
other pull requests' runs beside it): the tables still held 60 s estimates
for the newest tests, and the shards came out uneven - Linux debug's from
9.3 to 27.4 minutes of tests, Windows release's two 20.0 and 21.7, Windows
debug's 10.0 to 21.6, macOS release's 11.9 and 19.3. Six shards were over
two thirds of the 30-minute limit.

**Now.** `tools/ci_test_costs.py 37561340650 37561317531 37561187175` - the
three green runs of 2026-10-07, between them every test on main - wrote
every table again, each test's longest of the three. Every test a shard
runs is now in its table; the only names a debug table lacks are the three
`timing` tests, which the debug presets never run (`-LE timing`), and
`tools/ci_shard.cmake` now counts a timing test as costing nothing in a
`-debug` preset rather than warning of it at 60 s. Measured work, at four
tests at a time (three on macOS): Linux debug 164 min, Windows debug 108,
macOS debug 77, Windows release 42, clang-cl 37, Linux release 31, macOS
release 32. The counts are set so that each shard holds about 15 minutes of
that: Linux debug 10 to 11, Windows debug 6 to 7, Windows release and
clang-cl 2 to 3, macOS debug 4 to 5; Linux and macOS release stay at 2.
The longest single tests set a floor: the AI separation test, 882 s in
Linux debug.

**Seen on CI, run 37570543750** (this pull request's first push; four other
runs queued beside it, so 66 minutes before macOS's tests could start). No
shard warned of an unmeasured test. Test jobs: Linux debug 9.5 to 21.1
minutes but one at 26.1 (the AI separation test, 1,295 s that run); Linux
release 13.2 and 15.5; Windows debug 11.1 to 19.4; Windows release 11.9 to
14.5; clang-cl 13.5 to 14.2. Before (run 37561340650): Linux debug up to
28.6, Windows release 21.9 and 23.9, Windows debug up to 23.1, macOS
release 20.5.

Tests that land after these three runs - the speedbrake pull request's,
rebased under this one - count 60 s until the tables are measured again;
each shard's log names them.

**Run 37577077798** (the second push): 63 minutes from push to result.
Every job but macOS's had finished at 37 minutes; macOS's builds waited 24
minutes for a runner (the account has five macOS runners, and other runs'
jobs held them) and its seven test jobs ran in two waves. Test jobs: Linux
debug 8.2 to 18.9 minutes and the AI separation test's shard 26.4; Linux
release 14.1 and 15.9; Windows debug 12.3 to 19.3; Windows release 13.7 to
15.0; clang-cl 10.3 to 14.2; macOS debug 13.6 to 17.2; macOS release 14.7
and 15.8. Red on one test, the known one -
`the_client_with_the_window_flies_the_servers_aircraft_and_draws_the_others`,
"its own aircraft was put right too far to hide" (the open tail "A server
that falls behind real time puts its clients' prediction off by metres").

**Run 37583810544** (rebased onto main's speedbrake work): red on Windows
debug and clang-cl, both on
`a_client_predicting_its_aircraft_stops_its_engine_when_the_server_says_and_is_put_right_no_more`
- "with the engine stopped the median prediction error was 47 mm (429 mm),
more than the 43 mm (51 mm) before it or 2 cm". A test that measures
prediction against a loaded runner; nothing here changes what it runs, only
which tests run beside it. A new tail.

**Run 37587492168**: red twice (with its rerun) on Linux debug shard 10 of
11, `the_client_with_the_window_flies_the_servers_aircraft_and_draws_the_others`
put right 20.6 m against its 20 m, 19.5 m of it while the clocks were
learnt; main's runs pass it. The new dealing put it beside heavy tests, and
the debug server fell behind real time (the open "server behind real time"
item). It and `a_client_whose_frames_are_held_most_of_a_second_is_never_put_right_too_far_to_hide`
(the same script, slow frames; release only) are now RUN_SERIAL: nothing
runs beside them. `tools/ci_shard.cmake` already weighs a serial test at
its time times the jobs it holds, so the shard that gets it is dealt that
much less.

**Run 37596019954**: red twice (with its rerun) on Windows debug,
`the_client_with_the_window_away_past_the_servers_timeout_building_its_flight_or_in_a_frame_is_not_let_go`:
"the client was given no aircraft", its log "cannot reach 127.0.0.1:24773"
before the server's "listening on port 24772". Not load needing
RUN_SERIAL but a start-up race: the test started server, relay and client
at once and the client's five-second handshake gave up before a slow
debug server listened - the race other server tests closed on 2026-10-02.
`tests/cmake/client_kept_while_away.cmake` now has the server write
`--ready-file` and the client wait on it (`--after-ready`), each case its
own file. Passed locally (Linux debug, both cases kept 2295 and 2081
times). The rate-limit test failed once more, on clang-cl - its open tail.

**Windows' compiler cache, measured cold and warm.** Warm, in this run:
MSVC debug 3.9 and release 3.4 minutes a job. clang-cl with a pull
request's changes: 722 of 824 compiles hit, a 7.2-minute job (run
37561340650); cold, 0 of 824, 17.4 minutes (run 37570543750). That closes
"CI's Windows builds have no compiler cache".

**Why clang-cl was cold: two generations of caches at once.** Main's run
37569407795 saved `ccache-windows-windows-clang-a2257bf...` at 04:17; when
its prune job ran at 04:47 the entry was gone - GitHub had evicted it. One
generation of main's ccaches is about 4.2 GB and the downloads 3 GB, and
each merge saved a whole new generation (downloads included, though nothing
they pin had changed) while the old one stood until the run's end: over the
repository's 10 GB. Now:
- `.github/actions/prune-cache` deletes main's older entries of a kind as
  soon as the build job has saved its new one (curl and jq; Rocky's
  container installs jq), and the build jobs have `actions: write` for it.
  The run-end prune job stays, for the downloads.
- Rocky's job saves the downloads on a push to main only when what it
  restored was not of the same pinned lists; the nightly run saves them
  every night still.
The script was run by hand against the real cache API with its DELETE
replaced by an echo: with a prefix two entries shared it named the other
for deletion, with only this run's entry it named none, and with this
run's entry missing it kept everything. Not otherwise verifiable on a pull
request (only main saves); to be seen in the first main run after this
lands: each build's log saying "deleting" its
older entry, and the next pull request's builds all restoring.

**The Windows configure, counted over a week of main** (163 configures,
2026-09-30 to 2026-10-07, `ci.yml` runs on main): after the two rebuilds
of 2026-09-30 (21-24 minutes, the new image's compiler), none rebuilt;
every one inspected restored 39 packages from NuGet in 25-60 s. But 8 took
3.0 to 6.0 minutes, all after vcpkg had finished: in SDL's `try_compile`
checks, one of them (`CAN_PRESEED`) 61 s (job 110340482569). The item's
3-minute bound is not met, so it stays open with that named.
