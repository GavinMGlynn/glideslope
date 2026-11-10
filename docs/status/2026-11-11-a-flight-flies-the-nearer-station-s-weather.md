### On a server a flight flies the nearer station's weather, with no step in the wind, 2026-11-11

**What it does not do, first.**
- **The plan item is not done: a flight on its own does not.** A flight of
  the window's own, with no server, flies the one station its `--weather`
  names wherever it goes; that is what the item still owes.
- **Only a server asked for it flies the stations around it** (`--weather
  around`). A server given one `--weather` station, or a `--metar`, flies
  that one everywhere, as before.
- **A client joining while the server's stations change flies the new ones
  whole** until the change's blend ends (five minutes by default): it is
  told each slot's weather now, not the stations before the change. A
  station's own new report is blended from on joining, as before. A tail.
- **The AI's planners are told no wet runway under `--weather around`**:
  the stations are found after the fleet is built, and a station's runway
  states reach the air (and every client) but not the plans. A tail.
- **Every station shares Open-Meteo's forecast at its own place**, one
  request a station; nothing blends the forecasts but as the stations blend.

**What it does.**
- **The stations around every aircraft** (`world::fetch_stations_weather`).
  aviationweather.gov's METARs are asked for in a box 200 km each way of
  every aircraft (`bbox=`; a place within 1 km of one already asked about is
  not asked again), and the six nearest stations within 200 km of each are
  kept (`world::stations_around`), each once, at most 32: about 6,500
  stations report METARs (MADIS's count) over about 149 million km^2 of
  land, one for every 150 km or so evenly spread, so 200 km finds one for
  most places over land. Open-Meteo's winds aloft are fetched over each.
  The server finds them before its clock starts, and again every
  `--weather-refresh` and whenever an aircraft has flown 50 km from where
  they were last found, off the stepping thread (`world::StationsFetch`).
- **Each place flies the nearer station's weather** (`world::station_weights`,
  `world::StationsWeather`): each station's share is
  `1 - (d - d_nearest) / 20 km`, none below nought, divided by their sum -
  the nearer station's air alone, to the bit, wherever the next is 20 km or
  more farther, and between two a band 20 km wide across the halfway line
  where the shares move with the place. Every value is the shares' weighted
  sum but the runway's condition code and the turbulence's severity, which
  are the greatest share's. A step of `s` metres moves a share by at most
  `2 s / 20 km`.
- **Stations found or let go blend in time** (`world::StationSlots`): each
  station keeps a numbered slot; a slot's new report of its station blends
  in as a report always has; a slot given another station, emptied or
  filled changes the stations, and the air of those before blends into the
  air of those after over the weather's blend, from that moment.
- **A change in the middle of a blend blends from the air then** (from the
  review of #358): what a new report blends from is the whole weather as it
  was, its own blend and all (`world::ReportedWeather`), and what new
  stations blend from is the stations as they were, theirs and all
  (`world::StationSlots::Stations`) - so a jet finding stations again every
  200 s against a 300 s blend steps nothing. A blend that ended
  `blend_kept_s` (60 s) before the next change is let go then.
- **The air is pure in time** (from the review): nothing is let go when the
  air is asked for past a blend's end, so a client flying steps again from
  before it (sim::Prediction) flies what the server flew. Before, the
  blend was dropped when asked past its end, on either end.
- **A box that answers nothing keeps the stations flown** (from the
  review): an aircraft's box that fails, or answers 204 or no report,
  keeps the stations flown near it, as they are, and the server says so
  ("stations kept: ..."); a station whose forecast fails keeps the one it
  had. Only no place answering, with nothing flown, is the weather not had.
- **The wire carries each slot** (protocol 18, `12`): `WEATHER`,
  `WEATHER_ALOFT` and `RUNWAY_CONDITIONS` begin with the slot, and `WEATHER`
  says when the stations last changed; `docs/TRANSPORT.md` writes them out.
  `net::Told` keeps each slot's newest weather, the one before it and its
  runways; `frontend::HeardAir` flies them as a `world::StationSlots`, made
  by the same calls the server makes, so a predicting client flies the
  server's air to the last bit. The window client draws the sky of the
  station whose air is most of it where the aircraft is.

**Why 20 km, and why not inverse distance.** Searched 2026-11-11: FlightGear
local/advanced weather and METAR interpolation (its `local_weather.nas`
takes an inverse-distance-weighted average of every station defined; the
wiki's built-in weather averages halfway between two); X-Plane real weather
METAR interpolation (Laminar's notes blend METAR snapshots into a regional
forecast; no distance rule published); MSFS live weather station blending
(no official rule; forum descriptions of "bubbles" around airports with a
transition, a proposed 25-30 nm one); the aviationweather.gov data API
(`/api/data/metar` and `/api/data/stationinfo` take `bbox`); Meteonorm's
interpolation (the nearest station alone when close, IDW of up to six
otherwise); and METAR station counts and spacing (MADIS 6,579 stations; no
published spacing). Inverse distance makes every place a mix, so a flight
would never fly the nearer's weather, which the item asks for; nearest alone
steps. The band is the nearest-station rule with its edge softened. 20 km is
an estimate: wider than a METAR's vicinity (16 km), narrower than the 40 to
100 km between most reporting aerodromes.

**Verification** (linux-debug):
- `a_flight_between_two_stations_flies_the_nearers_weather_with_no_step_in_the_wind`:
  two stations 60 km apart, 27010KT Q1013 and 09020KT Q1003 (and gusting
  25 and 35 kt), flown straight from one to the other at 300 m, 120 Hz
  steps - each way, at 50 and 250 m/s, steady and gusting: eight flights,
  counted. At every step: the nearer's air to the bit where it is 20 km
  nearer; the start's and the end's air theirs; the wind moving by no more
  than `2 s / 20 km` times the stations' winds' difference plus the most
  either station's own air moved in the step; the band's steps its width
  over the step. The band is pinned in the test, not read from the code.
  Worst step in steady air 0.00064 m/s at 50 m/s, 0.0032 at 250 m/s; in
  gusts 0.20 and 0.63 m/s, each station's own gusts.
- `every_stations_share_of_the_weather_is_whole_and_continuous_over_a_grid`:
  three stations, 101 by 101 places 1 km apart (counted): shares whole,
  the nearest's the greatest, alone outside the band, none moving by more
  than `2 n s / 20 km` between neighbours.
- `a_station_found_or_let_go_blends_in_over_the_blend_and_steps_nothing`:
  a station found at 100 s and the first let go at 400 s, each over 60 s;
  at every 120th of a second for 600 s (counted) the air is the one
  station's, the two's or the other's to the bit outside the blends, and
  the worst step 0.0015 m/s against an even spread's 0.0021.
- `the_stations_kept_are_the_six_nearest_within_200_km_of_each_aircraft`,
  `the_metars_around_a_place_are_asked_for_by_a_box_200_km_each_way` (the
  box asked for, cut at the antimeridian, read from aviationweather.gov's
  answer around Sydney recorded at 2100Z on 2026-11-10, now in
  `tests/data/weather/`).
- `a_client_flies_the_servers_stations_to_the_last_bit_through_every_change`:
  three slots, a station's new report, a station swapped for another and one
  found, and a slot emptied, each through the wire's bytes, `net::Told` and
  `frontend::HeardAir`; 144 points of the air, each the server's to the bit.
- The message refusals: a slot of 32 and stations changed before nought,
  for each message, refused; slot 31 read.
- `changes_of_the_stations_and_their_reports_that_overlap_step_nothing`:
  three stations, five changes - stations found, a report changed, a
  station let go - 10 to 20 s apart, each over 60 s; every 120th of a second
  for 400 s the worst step 0.0026 m/s against five blends' even spread,
  0.018. With what blends made the newest alone, as before the review, it
  stepped 4.02 m/s: seen to fail, and reverted.
- `a_client_flying_again_from_before_a_blends_end_flies_the_servers_air`:
  a client flown to 400 s and again from 90 s, 666 points, each the
  server's to the bit; with a blend let go when asked past its end, as
  before the review, it failed at 90 s: seen, and reverted.
- `a_place_whose_box_answers_nothing_keeps_the_stations_it_had`: three
  aircraft, one box answered, one 503 to every retry, one 204; the last two
  keep their stations, YSSY its forecast; with nothing kept and nothing
  answered, the weather is not had.
- **End to end**, `a_client_flies_the_stations_around_it_the_nearers_weather_with_no_step_in_the_wind`
  (`tests/cmake/server_stations.cmake`): a server given `--weather around`,
  its weather services answered by `glideslope_http_stub` from the recorded
  answer and a recorded Open-Meteo forecast, never the network, found and
  kept YSSY, YSWS, YSRI and YWLM (YSCB, 248 km, not), and a predicting
  client starting 11.6 km from YSSY on the line to YSWS, inside the band,
  was told all four slots and flew 4,698 steps: most of the air YSSY's, its
  share down to 0.923, the wind changing by at most 0.0014 m/s in a step
  (bound 0.05), and the prediction error's median 0.000 m over 950 updates
  (bound 0.25 m).
- **And with the stub server**, `the_stations_flown_are_kept_when_the_box_around_an_aircraft_answers_204`
  and `..._404`: the same flight, the stub answering the first five
  requests from the files and every one after 204, or 404; the player's
  joining makes the server look again, the box answers nothing, and the
  four stations are kept and said; no slot is told still air. With keeping
  taken out, the 204 case failed: the client was told still air.
- Each end-to-end test took 44 s in linux-debug; that is the time put for it
  in every `tests/ci_costs/*.txt`, until CI measures its own.
- **Seen to fail**, each introduced, watched and reverted: the band made
  1 m stepped the unit flight's wind 2.72 m/s and the grid's shares; the
  first station flown everywhere failed "b's air alone near b"; a client
  that keeps slot 0 alone - one station, as before - failed the end-to-end
  test: "YSSY's share was never below one".

The selftest hash does not move: the selftest flies no weather, and nothing
it flies is changed here.
