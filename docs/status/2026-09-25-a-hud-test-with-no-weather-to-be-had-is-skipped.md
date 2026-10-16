### A HUD test with no weather to be had is skipped, 2026-09-25 — tail done

**What is not covered first.** Only the three HUD tests fly in live weather
(`--weather YSSY`); nothing else a test runs asks for it but the weather's own
unit test, which fetches Sydney's weather to hold it to what it must be and
still fails, where the network is required, when a service does not answer.
A weather refresh that fails mid-flight was already only reported, and the
flight flown on in the weather it had.

**Found by CI**: `glideslope: could not download: https://api.open-meteo.com/...
no response (WinHTTP error 12002)`, failing
`the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_*` from
`frame_hud.cmake`. **Cause.** The test did skip on "could not download" - but
not where `GLIDESLOPE_REQUIRE_NETWORK` is set, as CI sets it, because there
the DEM and the geoid must arrive: they are pinned and kept in `.downloads`.
Live weather is neither; it is somebody else's service, asked afresh each run.

**Now** only a service that did not answer is weather not to be had:
nothing answered at all (`HttpError`), or a server error (5xx) to every one of
`fetch_with_retries`' tries. `fetch_metar` and `fetch_winds_aloft` throw those
as `world::ServiceUnavailable`, and `fetch_weather` - that alone - as "the
weather could not be had: ...". A refusal (4xx), an answer that is not JSON,
and JSON that is not an answer stay the faults they were, so a misspelt
Open-Meteo variable, answered 400, fails the HUD tests rather than skipping
them for good. `frame_hud.cmake` reports itself skipped (exit 77) on "the
weather could not be had", whatever is required; a DEM that could not be had
still fails where the network is required.

To build these on purpose, `GLIDESLOPE_WEATHER_SERVICE` (read by
`platform::weather_service`, used through `world::weather_host`) names a
scheme and host asked instead of both `https://aviationweather.gov` and
`https://api.open-meteo.com`; unset, the services' own are asked. It is read
in every build, so it is held to an `https://` host or `http://127.0.0.1` /
`http://localhost`, with a port or without and nothing after
(`world::weather_service_allowed`); anything else stops the flight with the
variable named. `THREATS.md` says what that does and does not defend.
`fetch_weather` and the two fetches take the retry wait, 2 s by default, so a
test can walk the failures without the 30 s each retrying takes.

**Verified.**
- `a_hud_test_with_the_{pilot_flying,ai_holding,ai_flying_its_plan}_is_skipped_when_there_is_no_weather_to_be_had_on_*`
  (`frame_hud_no_weather.cmake`) runs `frame_hud.cmake` for each of the three
  HUD tests' cases with the weather asked of `http://127.0.0.1:1`, where
  nothing listens, and `GLIDESLOPE_REQUIRE_NETWORK` set, and requires exit 77
  with the weather named.
- `a_hud_test_whose_weather_service_answers_400_fails_rather_than_skipping_on_*`
  runs it against `glideslope_http_stub`, a server on the loopback answering
  every request 400, and requires a failure naming the 400 - not a pass, not a
  skip - and that the stub was asked. One case, the pilot's: the three differ
  only after the weather is fetched.
- `only_a_weather_service_that_does_not_answer_is_weather_not_to_be_had`
  walks twelve failures at each service - nothing answering, 500, 502, 503,
  504; 400, 401, 403, 404, 429; a 200 that is not JSON, and JSON that is not
  an answer - 24 cases, counted, the first five weather not to be had and the
  rest faults.
- `the_weather_may_be_asked_elsewhere_only_over_https_or_of_the_loopback`: 8
  values allowed and 18 refused, among them `http://127.0.0.1.example.org`,
  `http://localhost:8080@example.org` and paths.
- The three HUD tests still pass in Sydney's weather.

**Seen to fail:**
- With `frame_hud.cmake`'s weather skip taken out, all three skip tests fail
  with "exited 1, not 77", which is CI's failure. A first version of the
  wrapper, which skipped itself on any "could not download" where the network
  is not required, let that bug through as a skip. It now skips only on the
  DEM's failure.
- With `fetch_weather` calling every download error the weather's, as it
  first did, the 400 test fails ("exited 77, when it should fail") and so
  does the unit test, at aviationweather.gov's 400.
- With any `http://` host allowed, the allowed-hosts test fails at
  `http://example.org`.
