### A flight on its own flies the stations around her, 2026-11-11

**What it does not do, first.**
- **"A flight on its own" is the window's flight.** glideslope_cli has no
  weather of its own: it flies a server's, or none.
- **Her own stations are hers alone**: a flight of the window's own is no
  server's, and nothing she flies is sent anywhere.
- **A `--microburst` is put into one station's weather**, and is refused
  with `--weather around`: which station's would be a guess.
- What the stations around a server do not do (the item's earlier status
  file, `2026-11-11-a-flight-flies-the-nearer-station-s-weather.md`) she does
  not do either: the AI's planners told no wet runway, and her forecast
  each station's own.

**What it does.** The window's flight - the `glideslope` program - takes `--weather around`, as
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
glideslope_cli, which now says the first station's share too, and where the
worst step came. `--restation M` (for tests) looks again every M metres, not
50 km.

**The MSVC link.** With `sim/aircraft.hpp` included in
`frontend/same_air.hpp`, for `AirFlown::note` to read the aircraft, MSVC
would not link the window client or the tests: LNK1163, "invalid selection
for COMDAT section", in `same_air.cpp.obj`, twice. That is a toolchain or
COMDAT fault, not a fault of the code's definitions, and its cause was not
found. `note` takes the wind and the place instead, which each caller reads
from its aircraft; that is kept as the cleaner design, `same_air` needing
no flight model.

**Verification** (linux-debug, vulkan).
- `a_flight_on_its_own_flies_the_stations_around_her_the_nearers_weather_with_no_step_in_the_wind_on_<driver>`
  (`tests/cmake/window_stations.cmake`): her weather services answered by
  `glideslope_http_stub` from aviationweather.gov's answer around Sydney
  recorded at 2100Z on 2026-11-10 and a recorded Open-Meteo forecast, never
  the network, she keeps YSSY, YSWS, YSRI and YWLM (not YSCB, 248 km). Her
  plan starts 10.5 km from YSSY on the line to YSWS, 42.8 km apart, and she
  flies it 54,000 steps (450 s, about 24 km) - through the whole band and
  across its midpoint. The air is YSSY's alone at the first step (1.000)
  and YSWS's alone at the last (1.000), YSSY's share falling to 0.000; she
  looked again for the stations at 198.9 s and 394.2 s (`--restation
  10000`); and the wind changed by at most 0.0104 m/s in a step, at step
  50,867, where YSSY's share was 0.000 - the terrain's lift under her, past
  the band - against a bound of 0.05.
- The server's, `a_client_flies_the_stations_around_it_the_nearers_weather_with_no_step_in_the_wind`
  (`tests/cmake/server_stations.cmake`; and its 204 and 404 cases), had the
  same gap - its client only entered the band - and now crosses it too: a
  737-300 at 250 kt from the same start, the client predicting her for
  190 s on the trim the server holds her on (`--from-held`), straight along
  the line. Her air was YSSY's alone at the first step and YSWS's alone at
  the last of 22,696; the worst step 0.0096 m/s, at step 2,103, where YSSY's
  share was 0.857 - in the band; the prediction error's median 0.000 m over
  4,701 updates.
- **Seen to fail**, each reverted: with the window's flight given only the
  first slot's station, "the air at the last step was not YSWS's alone ...
  but 1.000"; with a client that keeps slot 0 alone, the server's test the
  same; and before this, `--weather around` was refused.
- The times put in every `tests/ci_costs/*.txt`, for each driver its
  platform runs: 150 s for the window's, 195 s for each of the server's
  three, as measured in linux-debug, until CI measures its own.

The selftest hash does not move: the selftest flies no weather.
