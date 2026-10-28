### The STOVL at-once tests' pitch on every platform, and the windowed client's law on the server's trim, 2026-10-29

**What is still not true first.** Rocky 9's figures for the plain and slow
at-once tests, and Ubuntu debug's for the slow one, were not said in CI's
full run 40733120388 (below), so no bound rests on them yet; this change
makes them said, and the next full run is the first to show them. Until they
are seen the item stays open.

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
- `every_test_servers_silence_timeout_is_thirty_seconds_or_more_unless_the_test_is_about_the_silence`:
  its totals 61 and 49 for the new test's `--timeout 30`.

**(a) The figures, every platform.** The at-once tests said their pitch only
when they failed: a passed test's output is in ctest's LastTest.log and in no
job's log, so CI's runs after #254 (40703093232, #262; 40660270759, #255)
have no figure to read - every job's log was searched. Each at-once test now
says it as `figure (WORK): predicted pitch within X degrees` before judging
it, and every test job of CI has a step, "The figures said", that prints
every such line from LastTest.log whether the tests passed or not.

**Why some were missing.** A test that runs ctest itself in the same build -
the ports check, the shards check - writes LastTest.log afresh, and the
figures of the tests before it in its shard went with it. Each at-once test
now also writes its figure to `figures.txt` in its work directory, and the
step prints those files.

**The figures, CI's full run 40733120388 (0cb3f404), degrees:**

| | plain | slow client | first asking ignored |
|---|---|---|---|
| Ubuntu linux-release | 0.006 | 0.002 | 0.001 |
| Ubuntu linux-debug | 0.004 | not said | 0.005 |
| Rocky 9 linux-release | not said | not said | 0.003 |
| macOS release | 0.005 | 0.006 | 0.006 |
| Windows debug | 0.009 | 0.004 | 0.005 |
| Windows release | 0.012 | 0.007 | 0.005 |
| Windows clang | 0.005 | 0.006 | 0.008 |
| here, linux-release | 0.003 | | |

Every one is under 0.012 - where before #254 Windows had 0.58 and 0.96 and
Linux 0.20 against the half-degree bound. **The bounds, twice the most each
test was seen off**: plain 0.024 (from Windows release's 0.012), slow 0.014
(Windows release's 0.007), first asking ignored 0.016 (Windows clang's
0.008), passed to the script as `PITCH_BOUND_MDEG`, the reason and the run in
tests/CMakeLists.txt. Twice is room for a replay longer on a busier runner,
not a guess at a distribution: one run per platform is what there is.

**The run's one failure** was Rocky's
`a_window_client_whose_imagery_is_refused_in_whatever_order_still_draws_its_frame_on_the_dems_colours`,
2.6 s in, in a shard with none of this change's tests: not this item's.

**After rebasing onto #241** (its law-state adoption in `Flight::adopt`, the
take-over in main.cpp kept, the levers forgotten before it), run here on
linux-release: the three at-once tests, the window test, #241's join,
mid-bank, take-over and both take-back tests, the timeouts check (64 uses, 52
long with #241's three) and the ports check - 14 passed.
