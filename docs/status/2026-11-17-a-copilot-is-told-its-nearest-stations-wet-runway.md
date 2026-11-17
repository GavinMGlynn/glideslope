# A copilot under the stations around it is told the runway its nearest station reports wet, 2026-11-17

Phase 12a: "A copilot is told one station's wet runways under `--weather
around`" is done.

## What was wrong

Under `--weather around` a server and a client fly several stations, each in
its slot. The copilots were told one of them:

- `glideslope_server`'s hand-over copilot (`situation_of`), and its check of
  a player's copilot's route (`fly_route_on`), were told
  `wet_runways(weather())` - slot 0's station, whichever aircraft was asked
  about and wherever it was.
- `glideslope_cli`'s headless player's copilot was told the last `WEATHER`
  the server sent, of whichever slot, and its runway conditions only when
  they were for that one.

## What now works

- `frontend::wet_runways_nearest(stations, lat, lon)`: of the stations flown
  (empty slots passed over), the one nearest the place (`sim::distance_m`),
  read as `frontend::wet_runways` reads the one weather - its rain within
  8 km, and its runways laid from its runway-state groups. It says which
  station; none where none is flown.
- The server: `Fleet::wet_nearest` over its slots, judged at the aircraft's
  place as each question is asked (`situation_of`, `fly_route_on`). A
  hand-over's question says what it was told: `the hand-over's copilot for
  aircraft 2 told by YSBK, its nearest station: the runways wet near it, and
  0 runways reported contaminated`.
- `glideslope_cli`: keeps each slot's `WEATHER` as told (still air empties
  it), lays each `RUNWAY_CONDITIONS` on its own slot's, and tells its copilot
  the nearest to its aircraft as it looks.
- The window client already did: its flight's `weather_report()` is set
  every step to the station nearest her (`Flight::refresh_weather`, on her
  own stations or the server's), and its copilot is told that one as it
  looks. Unchanged.

## Verification

`a_copilot_under_the_stations_around_it_is_told_the_runway_its_nearest_station_reports_wet`
(tests/unit/test_copilot.cpp): aviationweather.gov's answer built in the
test, two stations - YSBK 1 km north of YSBK 29C (600 m: the C172P needs
558 m dry, 642 m wet) and YSCN 20 km south of it. The stations are found by
`world::fetch_stations_weather` around a place 1 km south of YSCN and set in
a `world::StationSlots` as the server and a flight on her own set them, so
YSCN is slot 0 (asserted); the aircraft is then at 29C. Five cases of five,
the count asserted: near rain and far dry, told wet at YSBK and 29C refused;
near dry and slot 0 raining, told nothing wet and 29C taken; the near
reporting ice on 29C, told 29C laid at code 1 and not YSCN's rain; the same
stations judged back where they were found, told YSCN's; no stations, told
of none.

**Seen to fail**: with `wet_runways_nearest` answering the first slot's
station - what the server told every copilot before - the test failed at
"the near raining: told by YSBK, the nearest: 'YSCN'". Reverted; it passes.

`the_hand_overs_and_a_players_copilot_under_the_stations_around_them_are_told_the_runway_their_nearest_station_reports_wet`
(tests/cmake/copilots_nearest_station.cmake): glideslope_server under
`--weather around` with `--on-leave ai` and a played-back hand-over planner,
and glideslope_cli joined with its own played-back copilot, against
glideslope_http_stub answering the box with three made-up calm stations
about the plan's start: YXFA dry 1.00 km south (slot 0, asserted), YXNE
raining 1.05 km north, YXZZ dry 30 km west (slot 2, the last told the
client, asserted). She flies north, is asked from 5 s, and leaves at 20 s.
The server's hand-over copilot, its check of every route sent (her
copilot's and the hand-over's), and her own copilot each say they were
`told by YXNE, its nearest station: the runways wet near it, and 0 runways
reported contaminated`; none says another. About 40 s.

**Seen to fail**: with the server's `situation_of` and `fly_route_on` told
slot 0's station again, it failed: "the server's hand-over copilot was not
told by YXNE ... but 'told by YXFA ...'" (both route checks said YXFA too).
With the CLI's copilot told only the last slot told, it failed: "the
client's copilot was not told by YXNE ... but 'its copilot told by YXZZ
...'". Each reverted; it passes.

The runway states a slot's `RUNWAY_CONDITIONS` lays in the CLI are not
reached by this run (no station reports one): the unit test's ice case
pins the laying, not the CLI's per-slot matching of it.

## The stations test that failed here

`a_client_flies_the_stations_around_it_the_nearers_weather_with_no_step_in_the_wind`
failed twice in a linux-debug run while another working copy was building
("the air at the first step was not YSSY's alone ... but 0.990 YSSY's").
Run again with the machine quiet (load about 2), it passed on this branch
(202 s) and on its base, 96f65181 (201 s). Nothing here touches the air
flown; it is not this change's.
