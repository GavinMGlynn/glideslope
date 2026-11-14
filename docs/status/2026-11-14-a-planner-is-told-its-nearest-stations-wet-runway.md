# A planner under the stations around it is told the runway its nearest station reports wet, 2026-11-14

Phase 12a: "The AI's planners are told no wet runway under `--weather
around`" is done.

## What was wrong

`glideslope_server` tells each AI planner where the runways are wet
(`copilot::PlanRequest::wet_runways`) from one weather report, `planned_in`:
the `--weather` station's, or a `--metar` with `--station`. Under `--weather
around` there was none - the stations around the aircraft are looked for
only once the fleet is built, from the aircraft's places, and the planners
are asked while it is being built. So a planner under the stations around it
was told no runway wet and none contaminated, whatever the stations
reported; the air flown had the rain and the runway states, the plans did
not.

## What now works

- `frontend::wet_runways_around(lat, lon, time, fetch, all)`: the stations
  around a place (`world::fetch_stations_weather`), the nearest of them to
  it, its runway-state groups laid on its runways from `all`
  (`world::contaminated_runways`), read as `frontend::wet_runways` reads the
  one weather. No station reported around it is the weather not had, thrown
  as `fetch_stations_weather` throws.
- The server's planners are given a `PlannersWet`, asked in `plan_by_model`
  once the airport's runways are had, with the airport's first end and the
  whole runways file: a fixed answer for one station (as before), and under
  `--weather around` `wet_runways_around` at the airport. Stations that
  cannot be had are said, and that planner told no runway wet; the server
  goes on. Each planner says what it was told:
  `planner at YSSY told its nearest station wets no runway, and 0 runways
  reported contaminated`.

## Verification

`a_planner_under_the_stations_around_it_is_told_the_runway_its_nearest_station_reports_wet`
(tests/unit/test_copilot.cpp): a C172P planned at YSBK, runway 29C 600 m (558 m
needed dry, 642 m wet). aviationweather.gov's answer around it is built in
the test, two stations: one 1 km north of the runway, one 5 km south - both
inside the 8 km a METAR's weather reaches, so that only which is nearest
decides. The forecasts are the recorded Open-Meteo Sydney answer. Six
cases of six walked, the count asserted: near rain and far dry, refused 29C;
near dry and far rain, taken; both rain, refused; both dry, taken; the near
reporting ice on 29C (`R29C/7900//`), told the runway laid at code 1 on
both its ends and not the far's rain; none reported, the weather not had
(thrown, which the server says and tells that planner nothing wet).

**Seen to fail**: with `wet_runways_around` answering none - what the server
told every planner under `--weather around` before - the test failed at its
first case: "the near raining, the far dry: told wet within 8 km of the near
station". Then the real one put in, and it passed.

## Not done here

A copilot - the server's hand-over's, and a player's own - under `--weather
around` is still told one station's wet runways: the hand-over's the first
slot's, a player's her flight's one report. A tail item in
`COMPLETION_PLAN.md`.
