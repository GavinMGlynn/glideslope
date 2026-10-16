### Quitting the window client mid weather refresh is tested in the program, 2026-09-29 — tail done

Finishes the 2026-09-27 entry below. **Only the window client refreshes the
weather** (`glideslope`, the flight screen with `--weather`);
`glideslope_cli weather` asks once and has no flight, so it has no refresh to
quit.

**What changed.**
- Two tests per GPU driver, from `tests/cmake/client_quits_mid_refresh.cmake`:
  `quitting_the_flight_while_its_weather_refresh_is_answered_429_ends_it_within_2_s_of_a_quit_without_one_on_<driver>`
  and
  `quitting_the_flight_while_its_weather_refresh_is_part_way_through_a_transfer_ends_it_within_2_s_of_a_quit_without_one_on_<driver>`.
  Each flies the flight screen headless at Sydney with `--autopilot`, the
  weather services sent to `glideslope_http_stub`, twice: **a control** that
  quits at tick 400, and **the quit** at tick 610.
- Two new client test flags. **`--weather-refresh S`** sets how often
  `--weather` is fetched again (900 s unless given); the tests give 5, so the
  refresh begins at tick 600, five frames of two ticks before the quit.
  **`--quit-at TICK`** ends a `--shot` flight at that tick, straight after
  its steps, drawing nothing more and waiting for no tile, and says whether a
  weather refresh is under way (`Flight::refreshing_weather`) and "quitting at
  tick N", flushed. The shot, at tick 700, is never reached; it paces the
  flight by ticks, not the clock. The flight also says, flushed, when it
  begins fetching the weather again.
- The stub answers each flight's first fetch from files (`--files-for 4`: the
  METAR, and the 2026-09-17 Open-Meteo forecast given three days of hours -
  yesterday's, today's and tomorrow's UTC - so a run crossing midnight still
  finds its hour), and the refresh with 429 and `Retry-After: 10`
  (`--retry-after`), the most that is waited, or (`--stall`) with a head
  promising a megabyte, a few bytes of it, and the connection held.
- **The situation is built, not hoped for.** The quit must say a refresh is
  under way and the control that none is; a run in which the refresh ended
  by itself first fails as not testing its rule.
- **The bound is against the control**, so that it measures the refresh and
  not the machine: new `glideslope_exit_timer` reads the client's standard
  output through a pipe and says how long after "quitting at tick", and after
  the refresh began, the program ended. The quit must end within 2 s of the
  control's time, and within 10 s.
- **The stub cannot see a held transfer let go**: it is stopped after the
  client has ended, when the system has closed the client's end either way.
  It says only that it held one; the time is what shows the abandon.
- `glideslope_exit_timer` runs in a pipeline, so it ignores SIGPIPE as the
  stub does, and is in the table of
  `every_program_a_test_pipes_into_another_outlives_the_reader_going`, which
  failed on it ("died of ... a pipe nobody read: glideslope_exit_timer") until
  it did.

**What Windows debug showed, and why the quit is no longer the shot.** Quit
first by the shot - at tick 116000, and then at 108120, a second after a
15-minute refresh was due - the 429 test failed on Windows debug, both
drivers, "no weather refresh was under way at the quit", the stub having
answered all five 429s. It was not WinHTTP: `glideslope_cli weather` against
the stub answering 429 with Retry-After 10 took 41.7 s there, five tries and
four 10 s waits, as on Linux. The timeline did it: the refresh had begun
55.3 s before the program ended, one frame before a shot that waited that
long for its terrain tiles - longer than a refused refresh's 40 s. A person
quitting waits for no tiles, so the tests now quit with `--quit-at`; and
flying 15 minutes to reach the refresh cost 250-335 s a test there, so the
refresh is due after 5 s.

**Verification.** Linux release, lavapipe: the control ended 22 ms after it
quit and the quit 25 ms (429); the control 22 ms and the quit 998 ms
(stalled, libcurl's progress callback coming about once a second while
nothing arrives); 2-3 s a test. Windows debug (`tools/windows_build.sh` with
`WINDOWS_TEST=quitting`, the development machine, four at once): all four
pass, 21-27 s each; control and quit 219 and 260 ms, 443 and 447 ms (429),
291 and 396 ms, 574 and 600 ms (stalled) - the first program-level run of
WinHTTP's abandon. Seen to fail, on Linux, with `WeatherFetch`'s destructor
not raising its flag (both: 42 s and 362 s a test, against 2-3), and with
`http_fetch` not handing the flag to the request as `abandon` (the stalled
test alone, 68 s; the 429 test passed). The three HUD tests that use the stub
(400, an empty 200, 429 throughout) still pass with its new arguments.
`tests/ci_costs`: 3 s for linux-release and 27 s for windows-debug, measured
on the development machine, not on CI (a cold run there took 20-26 s);
estimates, not measured, for linux-debug (20), windows-release and
windows-clang (15), macos-debug (20) and macos-release (10). All are to be
measured again with `tools/ci_test_costs.py` from CI's runs once this lands.
