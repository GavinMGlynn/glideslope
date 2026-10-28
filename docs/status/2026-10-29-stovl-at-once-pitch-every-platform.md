### The STOVL at-once tests' pitch on every platform, and the windowed client's law on the server's trim, 2026-10-29

**What is still not true first.** Nothing of the item: every platform's
figure for each at-once test is seen, in two full runs. Not explained: the
slow-client test's 0.032 on Windows debug in the second run, four times its
0.004 in the first and its most anywhere else; its bound, twice it, is
the widest of the three.

**(b) The windowed client's law, on the server's trim: two causes, both
fixed.**

1. *Asked once.* The client with the window asked the server for the levers
   it holds her on (`WATCH` of its own) once as it joined, and gave up when
   the ten seconds joining had left were gone. A request past the server's
   rate is acknowledged and ignored (net/budget.hpp), so a player whose first
   asking was spent - keys pressed while joining - was never told, and her
   prediction's controller was made from nothing. Now it asks again once a
   second until told, as glideslope_cli does, and waits for as long as the
   server is heard rather than for a share of a fixed ten seconds
   (`client::Online::join`). Never told now means the server fell silent for
   the whole timeout, when there is no flight to predict.
2. *Told, then forgotten.* Even told, the window client's prediction was
   begun again from the first word of her own it heard after joining
   (`Online::hear`'s own word, `Flight::adopt`), a `sim::Prediction` made
   from nothing: the levers it had been told were put on the prediction that
   was thrown away. Found by this test, green on the asking and still red: told
   her levers ("pitch trim 0.075"), its law flew her on a trim of 0. Now
   `Flight` keeps the levers it is told and puts them on every prediction it
   begins (`Flight::held_by_server`, `Flight::adopt`), and forgets them at a
   take-over, since they are the aircraft's it joined with.

3. *Kept too long* (from the review): kept for every prediction begun, the
   levers of her joining were put back on the prediction begun again when
   she is taken back from the AI and when a prediction gone to a number not
   finite is let go of - mid-flight, on what she was held on minutes before.
   Both now forget them (`Online::heard` at the take-back,
   `Flight::let_go_if_lost`), as a take-over did. And the joining's wait for
   them ends after a minute of askings all told, heard or not: past that the
   server will not say them, and she flies on the client's own, said so.

**Verification (b).**
`the_client_with_the_window_whose_first_asking_for_her_levers_is_ignored_flies_her_stovl_law_on_the_servers_pitch_trim`
(tests/cmake/client_stovl_at_once.cmake, ports 24880-24881): the server and
plan of the at-once tests, its standard output through a relay that delays
nothing; the window client, headless, asks for the F-35B, spends the
server's second of requests on nothing just before asking for her levers
(`--test-spend-requests-first`), and selects STOVL mode with its very first
input (`--test-stovl-at-once`). The server must say it ignored a request; the
pitch trim the client's law flies her on at 1,200 steps must equal the
server's law's to the printed 17 digits, the server's not nought; and the
client must have been told her levers. Green here (linux-release): both
0.074587237159337141, 14 s.
- **Seen red, the asking as it was** (once, given up at ten seconds): "the
  prediction's law flew her on a pitch trim of 0, the server's on
  0.074587237159337141".
- **Seen red, the levers not kept across the prediction begun again**: run
  by hand with the test's programs and arguments, told her levers (pitch trim
  0.075), her law flew her on 0 against the server's 0.074587237159337141.
- **The take-back and the restart**: the window client says at its shot
  whether the prediction flying her was begun on the levers told at
  joining. `the_client_with_the_window_hands_its_aircraft_to_the_ai_on_a_server_and_takes_it_back`
  and `the_client_with_the_window_lets_go_of_a_prediction_given_a_nan_and_flies_on_from_the_servers_state`
  each require "no". **Seen red** with neither forgetting: both said "yes".
- `every_test_servers_silence_timeout_is_thirty_seconds_or_more_unless_the_test_is_about_the_silence`:
  its totals 64 and 52 - #241's three 30 s uses and the new test's `--timeout 30`.

**(a) The figures, every platform.** The at-once tests said their pitch only
when they failed: a passed test's output is in ctest's LastTest.log and in no
job's log, so CI's run 40703093232 (#262), after #254, has no figure to
read - every job's log was searched. Each at-once test now
says it as `figure (WORK): predicted pitch within X degrees` before judging
it, and writes the same line to `figures.txt` in its work directory; every
test job of CI has a step, "The figures said", that prints those files -
not LastTest.log - whether the tests passed or not.

**Why some were missing.** A test that runs ctest itself in the same build -
the ports check, the shards check - writes LastTest.log afresh, and the
figures of the tests before it in its shard went with it. Each at-once test
writes its figure to `figures.txt` for that reason (first run 40733120388
read LastTest.log, and lost them).

**The figures, CI's full runs 40733120388 (0cb3f404) / 40743161009
(c153eff6), degrees:**

| | plain | slow client | first asking ignored |
|---|---|---|---|
| Ubuntu linux-release | 0.006 / 0.005 | 0.002 / 0.003 | 0.001 / 0.004 |
| Ubuntu linux-debug | 0.004 / 0.003 | not said / 0.004 | 0.005 / 0.007 |
| Rocky 9 linux-release | not said / 0.002 | not said / 0.006 | 0.003 / 0.006 |
| macOS release | 0.005 / 0.006 | 0.006 / 0.005 | 0.006 / 0.005 |
| Windows debug | 0.009 / 0.005 | 0.004 / **0.032** | 0.005 / 0.006 |
| Windows release | 0.012 / 0.008 | 0.007 / 0.006 | 0.005 / 0.005 |
| Windows clang | 0.005 / 0.010 | 0.006 / 0.005 | 0.008 / 0.006 |
| here, linux-release | 0.003, 0.005, 0.002 | 0.006, 0.003 | 0.003, 0.001 |

Every one is under 0.033 - where before #254 Windows had 0.58 and 0.96 and
Linux 0.20 against the half-degree bound. **The bounds, twice the most each
test was seen off in either run**: plain 0.024 (Windows release's 0.012),
slow 0.064 (Windows debug's 0.032), first asking ignored 0.016 (Windows
clang's 0.008), passed to the script as `PITCH_BOUND_MDEG`, the reason and
the runs in tests/CMakeLists.txt. The first round's slow bound, 0.014 from
the first run alone, was failed by the second run's 0.032: two runs a
platform is still few, and twice is room, not a distribution.

**The first run's one failure** was Rocky's
`a_window_client_whose_imagery_is_refused_in_whatever_order_still_draws_its_frame_on_the_dems_colours`,
2.6 s in, in a shard with none of this change's tests: not this item's.
The second run's others: Windows clang's #241 test
`a_client_joining_an_f35b_mid_bank_with_the_first_datagrams_lost_still_begins_on_the_servers_law_state`
(median 72 mm against its 60) and Windows release's
`a_client_predicting_against_a_server_held_up_unevenly_every_second_is_within_a_metre_for_ninety_five_updates_in_a_hundred`,
neither touching what this changes.

**On Windows here**, the take-back test (tools/windows_build.sh, debug)
passed; its first try was skipped, the client gone on an access violation in
a worker thread while every imagery fetch to EOX timed out (WinHTTP 12002) -
seen once, not again.

**After rebasing onto #241** (its law-state adoption in `Flight::adopt`, the
take-over in main.cpp kept, the levers forgotten before it), run here on
linux-release: the three at-once tests, the window test, #241's join,
mid-bank, take-over and both take-back tests, the timeouts check (64 uses, 52
long with #241's three) and the ports check - 14 passed.
