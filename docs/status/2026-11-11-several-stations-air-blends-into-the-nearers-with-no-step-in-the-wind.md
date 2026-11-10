### Several stations' air blends into the nearer's with no step in the wind, 2026-11-11

**What it does not do, first.**
- **No flight flies more than one station yet.** The server takes one
  `--weather` station (or one `--metar`), and `WEATHER`, `WEATHER_ALOFT`
  and `RUNWAY_CONDITIONS` carry one station's weather: a client's
  prediction can fly only what the wire carries, so the server may not fly
  several until the wire carries them all, or its clients drift. That - a
  station index on the three messages, a protocol bump, the server keeping,
  refreshing and laying runways for each, `net::Told` and
  `frontend::HeardAir` keeping each - is what the plan item still owes.
- **Nothing finds the stations around an aircraft.** aviationweather.gov's
  data API answers a METAR request by bounding box (`bbox=`), each report
  with its station's place (`parse_aviationweather` already reads them), so
  no station list need be pinned; that is not wired either.
- **The sky drawn is one station's**: `StationsWeather::nearest` says which,
  and nothing draws from it yet.

**What it does.** `world::StationsWeather` flies several stations'
`ReportedWeather`s - each its own report, blended in time as it changes,
with its own runways, gusts and turbulence from its own seed - by a share of
each at the place (`world::station_weights`). Each station's raw share is
`1 - (d - d_nearest) / 20 km`, none below nought, divided by their sum: the
nearer station's air alone, to the bit, wherever the next is 20 km or more
farther, and between two stations a band 20 km wide across the line halfway
between them (10 km either side, along the line joining them). Every value
is the shares' weighted sum but the runway's condition and the turbulence's
severity, codes that cannot be mixed, which are the greatest share's. A
step of `s` metres moves a difference in distance by at most `2 s`, so
between two stations a share moves by at most `2 s / 20 km` a step.

**Why 20 km, and why not inverse distance.** Searched 2026-11-11: FlightGear
local/advanced weather and METAR interpolation (its `local_weather.nas`
takes an inverse-distance-weighted average of every station defined;
the wiki's built-in weather averages halfway between two); X-Plane real
weather METAR interpolation (Laminar's notes blend METAR snapshots into a
regional forecast; no distance rule published); MSFS live weather station
blending (no official rule; forum descriptions of "bubbles" around
airports with a transition, a proposed 25-30 nm one); the aviationweather.gov
data API (`/api/data/metar` and `/api/data/stationinfo` take `bbox`); and
Meteonorm's interpolation (the nearest station alone when close, IDW of up
to six otherwise). Inverse distance makes every place a mix, so a flight
would never fly the nearer's weather, which the item asks for; nearest alone
steps. The band is the nearest-station rule with its edge softened. 20 km is
an estimate: wider than a METAR's vicinity (16 km), narrower than the 40 to
100 km between most reporting aerodromes.

**Verification** (`glideslope_tests`, linux-debug):
- `a_flight_between_two_stations_flies_the_nearers_weather_with_no_step_in_the_wind`:
  two stations 60 km apart on a meridian, 27010KT Q1013 and 09020KT Q1003
  (and the same gusting 25 and 35 kt), flown straight from one to the other
  at 300 m, 120 Hz steps - each way, at 50 and 250 m/s, steady and gusting:
  eight flights, counted. At every step: where one station is 20 km or more
  nearer, the air is that station's own to the bit; the start's and the
  end's air are theirs; the wind moves by no more than `2 s / 20 km` times
  the stations' winds' difference plus the most either station's own air
  moved in the step; and the steps in the band are the band's width over
  the step, within two. The band is pinned in the test at 20 km, not read
  from the code, so a narrower one fails. Measured: the worst step in
  steady air 0.00064 m/s at 50 m/s and 0.0032 m/s at 250 m/s (48,000 and
  9,600 steps blended); in gusts 0.20 and 0.63 m/s, which is each
  station's own gusts moving in a step, inside the bound at every step.
- `every_stations_share_of_the_weather_is_whole_and_continuous_over_a_grid`:
  three stations about 50 km apart, every place of a 101 by 101 grid 1 km
  apart (counted): shares sum to one, the nearest's is the greatest, the
  nearest's alone outside the band, and no share moves by more than
  `2 n s / 20 km` between neighbours.
- **Seen to fail**, both bugs introduced, watched and reverted: with the
  band made 1 m - the nearest station alone, as a bare nearest-station rule
  would be - the flight's wind stepped 2.72 m/s in one step (steady,
  50 m/s, against 0.00064 allowed) and the grid's shares stepped between
  neighbours; with the first station flown everywhere - what a flight flew
  before - the flight failed "b's air alone near b".

The selftest hash does not move: nothing flown uses more than one station.
