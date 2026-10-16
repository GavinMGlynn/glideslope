### Tests that need a download skip when it cannot be had, 2026-09-27 — tail done

**What was wrong.** Windows CI, 2026-09-26 (run 36252249115): Open-Meteo
answered a 200 whose body was not JSON ("JSON at byte 0: not a value") to
every try. `fetch_winds_aloft` threw that as `DemError`, not
`ServiceUnavailable`, so `fetch_weather` never said "the weather could not be
had", and the HUD test flying a plan
(`the_hud_says_the_ai_flies_its_plan_and_shows_its_controls_on_direct3d12`)
matched only "could not download" - which, with GLIDESLOPE_REQUIRE_NETWORK
set, fails. The regexes of PR #23 were right; the classification under them
was not. The live unit test
`the_weather_now_at_an_airfield_is_fetched_from_aviationweather_and_open_meteo`
skipped on any `DemError`, but only where the network was not required.

**What changed.**
- `world::answered_json` (download.hpp, download.cpp): a weather service's
  200 is first read as JSON. A body *plainly* not JSON - empty, white space,
  or beginning with anything but `{` or `[`, as HTML does - on every one of
  `parse_attempts` tries is `ServiceUnavailable`, the service not answering.
  One that begins as JSON and does not parse is retried and then a
  `DemError` without "could not download": it may be our parser that broke,
  and that must fail the weather tests, never skip them for good (from the
  review). JSON that is not a METAR or a forecast is not retried, and is a
  `DemError` naming the service's URL, again without "could not download".
- `glideslope_skip_when_not_downloaded(rc err [also])` in
  `tests/cmake/client.cmake` is the one rule the client tests skip by: "the
  weather could not be had" is a skip always; "could not download" (or the
  script's `also`, "without their imagery" for the imagery tests) is a skip
  unless GLIDESLOPE_REQUIRE_NETWORK is set. It replaces the copies in
  client_aircraft, client_autopilot, client_memory, client_views,
  frame_checklist, frame_hud, frame_hud_jet, frame_terrain and frame_weather.
  Left as they were, because each reads its own failure differently:
  terrain_provider, terrain_mismatch and ion_stalled (keys and providers
  besides the DEM), and frame_hud_no_weather (which builds the weather's
  failure on purpose). cli_plan_flown uses it too, with "cannot download|no
  network" as its `also`, and now looks at the exit code first and fails
  where the network is required.
- The live unit test skips (exit 77) with the service's reason on
  `ServiceUnavailable`, required network or not, and fails on anything else.
- `only_a_weather_service_that_does_not_answer_is_weather_not_to_be_had`: "a
  200 that is never JSON" is now weather not to be had, at both services;
  so is an empty 200; "a 200 that begins as JSON and does not parse" and "a
  200 of JSON that is not an answer" are faults, and name the service. The
  not-JSON test walks six bodies at both services - HTML, empty, white
  space, plain text (the service not answering) and two that begin as JSON
  and do not parse (failures) - counting the fetches of each (an empty body
  is also retried inside each try by `fetch_with_retries`, 15 in all).

**Verification.** Linux debug, GLIDESLOPE_REQUIRE_NETWORK=1 throughout:
- Offline (`unshare -rn`): the live weather unit test is *skipped* ("Could
  not resolve host: aviationweather.gov"); with its skip replaced by a
  rethrow it fails. `only_a_weather_service_...` and
  `an_answer_that_is_not_json_...` pass; with the not-JSON answer thrown as
  `DemError` again both fail, and with every body that does not parse taken
  for `ServiceUnavailable` (the rule before the review) both fail too.
- New `a_hud_test_whose_weather_service_answers_an_empty_200_is_skipped_on_<driver>`
  flies the plan HUD test against glideslope_http_stub, which answers
  aviationweather.gov with a recorded METAR and Open-Meteo alone with an
  empty 200 - CI's failure exactly - and passes because the run inside
  skipped on the `/v1/forecast` answer that was not JSON; with `DemError` back it fails ("the HUD test exited 1, not
  77"). The 400 case still fails the inner run, and the no-weather plan case
  still skips.
- Online: the live unit test, the plan HUD test, the jet HUD test, the
  weather frame test and `a_plan_that_takes_off_and_orbits_the_cbd_is_flown_over_the_dem`
  (callers of the shared rule) pass.
