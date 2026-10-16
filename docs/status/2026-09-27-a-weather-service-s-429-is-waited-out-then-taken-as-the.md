### A weather service's 429 is waited out, then taken as the weather not to be had, 2026-09-27 — tail done

**What was wrong.** Windows CI, 2026-09-27 (run 36281639276, release):
Open-Meteo answered 429, Too Many Requests - CI's many HUD and weather
tests at once ran into its rate limit - and
`the_hud_says_the_ai_holds_the_aircraft_and_shows_its_controls_on_direct3d12`
and `..._on_vulkan` failed with "could not download ... status 429".
`fetch_with_retries` returned a 429 at once, as any 4xx, and the weather
fetches threw it as `DemError`, a fault of ours.

**What changed.**
- `fetch_with_retries` (download.hpp, download.cpp) tries a 429 again, as a
  5xx. Its `Retry-After`, in seconds, is the wait before the next try; without
  one in seconds (none, a date, or not a number) it waits the doubling backoff
  a server error does. Every wait after a 429 is capped at `retry_after_limit`
  (10 s) whatever the header says, so five tries all turned away wait at most
  40 s; waits after a 5xx or no answer are the uncapped backoff, as before. A
  real Retry-After overrides a test's shortened `retry_wait`. A `Sleep`
  parameter lets a test count the waits instead of waiting them.
- It reaches DEM tiles and pinned files too, through `get()`: a 429 there now
  waits instead of failing at once.
- Rate-limited CI now skips slowly - up to about 40 s a fetch, five requests
  instead of one - rather than failing fast.
- Tail found by the review: `Flight`'s `std::async` weather refresh has no
  cancellation, so quitting during a slow or rate-limited refresh can hang
  the exit for up to about 80 s.
- `fetch_metar` and `fetch_winds_aloft` take a 429 still there after every
  retry as `ServiceUnavailable` - the weather not to be had - as a 5xx is.
  Every other 4xx stays a `DemError`, and fails the tests.
- `glideslope_http_stub` sends `Retry-After: 1` with a 429, and
  `frame_hud_no_weather.cmake` reads ANSWER=429 as a case that must skip.

**Verification.** Linux debug:
- New unit test
  `a_429_is_waited_out_as_its_retry_after_asks_within_a_limit_then_returned`
  walks twelve cases, counted, of Retry-After (3 s, none, an hour, a number
  too long to hold, 0, a date, not a number, mixed with a 503, and a 429 to
  every try with none, with dates, with a minute then none, and with a
  minute), checking the fetches made and each wait taken - counted through the
  `Sleep` parameter, never waited - and that no wait passes 10 s nor all of
  them 40 s - the three cases added from the review failed before the cap on
  the backoff after a 429 (16 s by the fourth wait, 46 s in all); and eight
  other 4xx, with a Retry-After, fetched once and never
  waited on. Before the fix it failed on every 429 case (one fetch, no wait).
- `only_a_weather_service_that_does_not_answer_is_weather_not_to_be_had`: "a
  429 to every retry" is now weather not to be had at both services; before
  the fix both cases failed.
- New `a_hud_test_whose_weather_service_answers_429_throughout_is_skipped_on_<driver>`
  flies the AI-holding HUD test with aviationweather.gov answered by a
  recorded METAR and Open-Meteo by the stub's 429 to every request: the stub
  must be asked the forecast at least five times, and the run inside must
  exit 77 on the `/v1/forecast` 429. Before the fix it failed (one request,
  the inner run exited 1).
