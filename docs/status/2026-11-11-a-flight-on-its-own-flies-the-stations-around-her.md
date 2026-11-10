### A flight on its own flies the stations around her, 2026-11-11

**What it does not do, first.**
- **Her own stations are hers alone**: a flight of the window's own is no
  server's, and nothing she flies is sent anywhere.
- **A `--microburst` is put into one station's weather**, and is refused
  with `--weather around`: which station's would be a guess.
- What the stations around a server do not do (the item's earlier status
  file, `2026-11-11-a-flight-flies-the-nearer-station-s-weather.md`) she does
  not do either: the AI's planners told no wet runway, and her forecast
  each station's own.

**What it does.** The window's flight takes `--weather around`, as
glideslope_server does, and flies the stations within 200 km of her by the
same code: found by `world::fetch_stations_weather` (a box that answers
nothing keeps the stations flown), slotted by `world::slot_changes` - moved
out of glideslope_server's Fleet, which now calls it too - flown and blended
by `world::StationSlots`, over the ground she flies over, and looked for
again when `world::StationsAround` says - every `--weather-refresh`, or when
she has flown 50 km (`world::restation_m`) - which glideslope_server's
refresh now is too. Each station's runway states are laid on its runways as
they come. The sky drawn is the nearest station's. The air she flew is said
as she is quit by `frontend::AirFlown`, the headless client's, moved out of
glideslope_cli, which now says the first station's share too.

**Verification** (linux-debug, vulkan),
`a_flight_on_its_own_flies_the_stations_around_her_the_nearers_weather_with_no_step_in_the_wind_on_<driver>`
(`tests/cmake/window_stations.cmake`): her weather services answered by
`glideslope_http_stub` from aviationweather.gov's answer around Sydney
recorded at 2100Z on 2026-11-10 and a recorded Open-Meteo forecast, never
the network, she keeps YSSY, YSWS, YSRI and YWLM (not YSCB, 248 km), and
flying her plan from 10.5 km out of YSSY along the line to YSWS for 7,200
steps the air is YSSY's alone at the first step (1.000 of it), YSSY's share
falls to 0.824 - into the blend - and the wind changes by at most 0.0049
m/s in a step, against a bound of 0.05. **Seen to fail**: with only the
first station flown, as one station would be, "YSSY's share never fell
below one"; and before this, `--weather around` was refused. Reverted. The
server's tests (`server_stations.cmake`, three cases) pass on the moved
code. 14 s in linux-debug, the time put for it in every
`tests/ci_costs/*.txt` for each driver its platform runs, until CI measures
its own.

The selftest hash does not move: the selftest flies no weather.
