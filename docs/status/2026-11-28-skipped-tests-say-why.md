### The HUD tests fly recorded weather, and a skip on macOS CI says why, 2026-11-28

Phase 12c, "On macOS a dozen window-client tests report themselves skipped
after 300 s" - its missing part, the HUD tick test on Metal
(the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_metal).

**What is still not true, first.**
- **The cause is read from timings, not from what the test said.** No run
  kept its output: ctest prints nothing of a skip. macOS's test jobs now
  print it (below), so a skip anywhere in them says why in its own words.
- The live weather's own handling - a service that does not answer is a
  skip - is still tested, by frame_hud_no_weather.cmake's tests and the
  package smoke; nothing here makes a live service answer.

## Why it skipped

The test flies in the weather reported at Sydney now. Its script has one
way to exit 77 on CI: "the weather could not be had", which the client says
when a weather service did not answer through every retry
(GLIDESLOPE_REQUIRE_NETWORK is set, so a download cannot skip it).

- **334 s is five stalled requests.** On macOS a request goes through
  NSURLSession, given up after 60 s with nothing arriving
  (HttpRequest::stall_timeout_seconds). fetch_with_retries makes five tries,
  waiting 2, 4, 8 and 16 s between them: 5 x 60 + 30 = 330 s, and the
  client's start makes 334. The other skips seen fit fewer stalls: 155 s
  (run 45314548432, 2026-11-24), 279.84 s on macOS debug (run 45725279071,
  2026-11-26).
- **It passes when the weather answers.** In the nine macOS release runs
  that ran it on Metal (2026-11-20 to 2026-11-27), it passed seven times:
  six in 20 to 32 s, and once in 140 s - a try or two stalled, and a later
  one was answered. On macOS debug it passed four times of five: in 73 and
  86 s, and in 224 and 242 s. Its jet sibling, which flies no weather, took
  19 to 25 s every time; the sibling flying its plan, the same script with
  the weather, once took 166 s.
- **The same night, other platforms' weather tests skipped too.** In PR
  #413's run 45918072973, which the 334 s skip is from (01:55 to 02:01 UTC),
  Windows release skipped the_weather_now_at_an_airfield_is_fetched_from_aviationweather_and_open_meteo
  after 128 s (03:17 UTC) and the HUD tick test on Vulkan after 129 s
  (02:37 UTC); Windows debug and Linux debug each skipped a HUD sibling
  after 132 and 184 s. A weather service was not answering that night,
  everywhere.
- Not terrain, and not Metal: the test passes on Metal whenever the weather
  answers, and its terrain is the warm downloads cache.

## The fix: the HUD tests fly recorded weather

The HUD tick test is about the HUD showing the flight's state at the tick
it was shot; it has no need of today's weather. **frame_hud.cmake, given
STUB, METAR_FILE and FORECAST_FILE, flies recorded weather**: it runs
glideslope_http_stub beside the flight, answering aviationweather.gov with
YSSY's report of 1600Z on 2026-09-17, as recorded, and Open-Meteo with that
day's Sydney forecast, its hours alone made today's
(tests/cmake/recorded_weather.cmake, the dating client_quits_mid_refresh.cmake
already did), and the flight's weather goes to the stub
(GLIDESLOPE_WEATHER_SERVICE). It fails unless the stub answered the METAR
exactly once and the forecast at least once from its files, and nothing
else - the stub now says how many it answered for each prefix - and a
flight that says "there is no weather to fly in" on recorded weather fails
rather than skips. **Its wait for the stub ends**: on the stub's port, at
once when the stub has exited without one (tests/cmake/run_then_say_exited.cmake
writes when it has), or after 600 looks 0.05 s apart; and the stub, which
ends only when asked, is asked up to five times. The three HUD tests flown in weather - the pilot flying, the AI
holding, the AI flying its plan - fly so on every driver: Metal, Vulkan and
Direct3D 12. The HUD still shows Open-Meteo's credit, and the check still
reads it.

**Other window-client tests on live weather: none left.** Every script
that runs the window client with `--weather` was read: client_quits_mid_refresh,
window_stations and window_joins_mid_change already fly a stub's recorded
weather, and the rest are given a METAR (`--metar`). The one other on live
weather is the package smoke (package_weather.cmake), which is about what
the package does when the weather does not answer, and stays live.

## And a skip on macOS CI says why

**tools/ci_skipped.cmake** reads ctest's JUnit report (`--output-junit`),
which keeps every test's output, and prints each skipped test's name,
ctest's reason and the last 60 lines of what it said. A test that ran is not
printed (a failure's output is ctest's own to print), and neither is a
disabled one. The macOS release and debug test jobs write the report and run
it after ctest, whether the tests passed or not. LastTest.log could not do
it: it says "Test Passed." of a skip, and a test that runs ctest itself
writes it afresh. Its test is below.

## Verification

- **The three HUD tests on recorded weather pass**: on Vulkan, linux-release
  locally with GLIDESLOPE_REQUIRE_NETWORK set, in 8 to 10 s with the caches
  warm, the stub having answered from its files alone. The six tests of a
  live weather service not answering (frame_hud_no_weather.cmake) still pass.
- **Seen red**: with the HUD's SPD drawn 2 kt high (src/gfx/hud.cpp), all
  three failed - "SPD shows 80 at tick 600 when the state is 78.242000" and
  the like. Reverted, all three passed. **And with the stub given a bad
  argument** (status 999, which it refuses, exiting 2), the HUD tick test
  failed in 0.07 s - "the stub exited (2) without listening" - where it
  would have waited out ctest's 900 s. Reverted, it passed. Metal's and Direct3D 12's are the
  same script and check, run on CI; there is no Mac here to break one on.
- **A throwaway probe** of the weather services from a macOS runner (PR
  #430) sat queued for 90 minutes behind other runs and was cancelled
  unrun: with the weather recorded, its answer no longer decides anything.
- **what_every_skipped_test_said_is_printed_from_ctests_report_and_nothing_of_any_other**
  writes seven tests, one for each way ctest ends a test - skipped by return
  code, by regular expression, for a required file missing, and by return
  code saying 13 lines with `; [ ] < > & "` among them; passed; failed;
  disabled - runs ctest on them with --output-junit and holds the tool to
  the report: the four skips printed as said (the long one's last 5 lines
  and not its 8th), the other three not at all. It fails if it judged fewer
  than seven.
- **Seen red**: with the '[' guard taken out, the long skip's line with
  brackets was not printed as said; with the skip filter taken out, the
  passed, failed and disabled tests were printed. Each reverted, 7 of 7.
- **Searched**: aviationweather.gov and api.open-meteo.com timing out from
  GitHub Actions runners; NSURLSession -1001 "The request timed out" on
  macos-15 runners; URLSession HTTP/3 and QUIC timeouts where UDP is
  blocked. Nothing names these services stalling from macOS runners.

## Done

Ticked on train #431, whose full run is the item's verification: every window-client test run on macOS release, none skipped but for want of a key. Checked from run 46054882634's six macOS release shard logs before it merged: of the window-client tests only the Google and Cesium ion terrain tests on Metal skipped, each saying it had no key, as CLAUDE.md requires; none skipped for terrain or weather.
