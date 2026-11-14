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

- `frontend::wet_runways_around(lat, lon, fetch, all)`: the METARs around a
  place (`world::fetch_metars_around` - surface reports alone, no winds
  aloft, which a planner is not told), the station nearest it, its
  runway-state groups laid on its runways from `all`
  (`world::contaminated_runways`), read as `frontend::wet_runways` reads the
  one weather. It says which station; none where none is reported, and it
  throws as `fetch_metars_around` does when the service does not answer.
- The server's planners are given a `PlannersWet`, asked in `plan_by_model`
  once the airport's runways are had, with the airport's first end and the
  whole runways file: a fixed answer for one station (as before), and under
  `--weather around` `wet_runways_around` at the airport - **asked once an
  airport**, every planner there sharing it. What they were told is said:
  `planners at YSSY told by YSSY, its nearest station: the runways wet near
  it, and 0 runways reported contaminated`. No station reported around the
  airport, or the service not answering, is said on stderr, and its
  planners told no runway wet; the server goes on.

## Verification

`a_planner_under_the_stations_around_it_is_told_the_runway_its_nearest_station_reports_wet`
(tests/unit/test_copilot.cpp): a C172P planned at YSBK, runway 29C 600 m (558 m
needed dry, 642 m wet). aviationweather.gov's answer around it is built in
the test, two stations: one 1 km north of the runway, one 5 km south - both
inside the 8 km a METAR's weather reaches, so that only which is nearest
decides. Seven cases of seven walked, the count asserted: near rain and far
dry, refused 29C; near dry and far rain, taken; both rain, refused; both
dry, taken; the near reporting ice on 29C (`R29C/7900//`), told the runway
laid at code 1 on both its ends and not the far's rain; none reported,
told of no station and nothing wet; the service answering 503, thrown after
asking again. In every answered case one request is made, the METAR box
alone - no forecast.

`a_servers_planner_under_the_stations_around_it_is_told_the_runway_its_nearest_station_reports_wet`
and `a_servers_planner_under_the_stations_around_it_is_not_told_a_farther_stations_rain`
(tests/cmake/server_planner_stations.cmake): glideslope_server, `--ai 1`
with a played-back planner for the YSSY task and `--weather around`,
against glideslope_http_stub answering the box with YSSY and YSBK (17 km
west), and the recorded Sydney forecast. With YSSY raining the server says
its planners were told the runways wet near YSSY; with YSBK raining, no
runway wet by YSSY's weather; once each. (Told of rain, the recorded model
answer no longer matches the request and is refused; the aircraft flies the
plan file. That is not what this tests.)

**Seen to fail**: with `wet_runways_around` answering none - what the server
told every planner under `--weather around` before - the unit test failed at
its first case: "the near raining, the far dry: told wet within 8 km of the
near station". With the server's `--weather around` `PlannersWet` not
installed, both server tests failed: "the planner was not told 'planners at
YSSY told by YSSY ...'". Each reverted, and they pass.

## Not done here

A copilot - the server's hand-over's, and a player's own - under `--weather
around` is still told one station's wet runways: the hand-over's the first
slot's station, a player's her flight's one report. A tail item in
`COMPLETION_PLAN.md`.
