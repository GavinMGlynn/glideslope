### The STOVL at-once tests' pitch on every platform, and the windowed client's law on the server's trim, 2026-10-29

**What is still not true first.** FIGURES_PENDING

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
SPREAD_PENDING
