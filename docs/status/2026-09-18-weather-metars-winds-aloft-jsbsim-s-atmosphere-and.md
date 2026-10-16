### Weather: METARs, winds aloft, JSBSim's atmosphere and turbulence, 2026-09-18 — items done

Proved in CI on every platform (run 35250647710, after the retries below), and
every package fetched the weather and flew its frame in it (run 35248205205).

**What is missing first:** the weather is one airfield's. A flight is flown in
the weather reported at the station it names - its METAR at the surface and
Open-Meteo's winds aloft above it - wherever it flies; nothing chooses the
nearest station or blends between stations. There is no cloud, visibility,
precipitation or icing, and nothing is drawn: JSBSim is given wind,
temperature, pressure and turbulence, and that is all. Turbulence is off unless
something asks for it - no report gives it, and nothing yet sets it outside the
tests. Gusts in a METAR are read, and not flown. The server, which will own the
weather, does not exist.

**METARs** (`world/metar.hpp`, `world/weather.hpp`). aviationweather.gov's JSON
response is read for each report's raw text and its station's position, and
the raw text is decoded here: station, time, wind in knots, metres a second or
kilometres an hour, with gusts and variability; temperature and dew point,
with the tenths of a US `T` remark; QNH from `Q` or `A` groups. Trend groups -
`BECMG`, `TEMPO` - are skipped, so a forecast wind is never read as the
observation. A station is checked to be four letters or digits before it goes
into a URL, and a station with no report is said to have none.

**Winds aloft** (`world/winds_aloft.hpp`). Open-Meteo's forecast for the current
UTC hour on 19 pressure levels, 1000 to 30 hPa: each level's wind as north and
east components, its temperature, and its geopotential height converted to
geometric height. Between levels the wind is interpolated as a vector.

**The weather at a height** (`world::conditions_at`): the METAR's wind up to
10 m above the station, where it is measured; the winds aloft from the lowest
level above that; linear in height between. The temperature follows the same
profile as an offset from the International Standard Atmosphere, so the air at
the station is the METAR's temperature. The pressure is the METAR's QNH.

**JSBSim's atmosphere** (`sim/weather.hpp`). The simulation still knows nothing
of reports: `Aircraft::set_weather` takes any `sim::Weather`, and before every
step the aircraft asks it for the conditions where it is and sets JSBSim's wind,
temperature offset, sea-level pressure and MIL-F-8785C turbulence severity. The
sea-level values rebuild JSBSim's atmosphere, so they are set only when they
change. Turbulence is seeded the same every time, so a flight in it repeats.

**Changing weather** (`world::ReportedWeather`): a new report blends in over an
interval, every value moving linearly from the old report's to the new one's,
and turbulence changing halfway. The client fetches the weather again every
fifteen minutes of flight, on another thread, and blends it in over five; a
fetch that fails is reported and the weather kept.

**Where it shows.** `glideslope_cli weather STATION` prints the METAR and the
winds aloft over it. `glideslope --weather STATION` flies the flight in it, with
"WEATHER DATA BY OPEN-METEO.COM" along the bottom of the HUD, as Open-Meteo's
CC BY 4.0 terms ask, and `--trace` now includes JSBSim's wind. JSON is read by
a strict RFC 8259 parser written here (`world/json.hpp`) rather than a library.
Sources, terms and the credit are in `ASSETS.md`.

**The tests.**
- JSON: every kind of value, escapes and surrogate pairs, numbers at their
  limits, and duplicate keys read; trailing commas, comments, leading zeros,
  control characters, lone surrogates, bad escapes, truncation and nesting past
  128 refused.
- Ten recorded METARs, from Toronto, Heathrow, Boston, Denver, Christchurch,
  Utqiagvik, Punta Arenas, Moscow, Sydney and Beijing, decode as
  aviationweather.gov's own decoding beside each says - station, temperature,
  dew point, wind, direction and altimeter setting. Every group's forms, and a
  trend's wind and temperature not taken for the observation's.
- Each recorded METAR, flown at its station, gives JSBSim the wind it reports
  to 0.01 ft/s, the temperature at the station to 0.05 C, and its QNH to
  0.01 hPa.
- A recorded Open-Meteo response for Sydney: all 19 levels read as the JSON
  says; JSBSim's wind at each level's height is that level's, and halfway up to
  it from the level below - or from the surface - halfway between.
- A minute holding a northerly heading at 3,000 ft in 20 knots from the west
  drifts the aircraft east of the still-air flight by what 20 knots covers in a
  minute, 617.3 m, within 2%, with no turbulence at all. With moderate turbulence (severity 3), each component's RMS
  is within 0.4 to 1.6 of JSBSim's MIL-F-8785C intensity at that height, 7.21
  ft/s; no gust exceeds five times it; the aircraft stays within 30 degrees of
  bank; and the same turbulence flown twice ends in the same place.
- A report of 30 knots from the east, replacing 10 knots from the west and
  blended over 60 seconds: the old wind before it, halfway at halfway with the
  pressure halfway, the new wind after, and no step between ticks larger than
  the whole change spread evenly over the interval.
- Live: Sydney's weather now, from both services - where the station is, a
  report from today or yesterday, 19 levels in order with 500 hPa between 5,000
  and 6,200 m; stations that are not stations refused before any fetch; a
  station with no METAR said to have none. Skipped without the network, except
  in CI.
- The HUD test now flies in Sydney's weather now, and reads the credit back
  out of the frame as well as the numbers.

**Watched to fail:** turbulence type left off (every component's RMS 0.000
ft/s against 7.21); the credit removed from the HUD ("the credit reads "", not
"WEATHER DATA BY OPEN-METEO.COM""); the station check reduced to refusing
nothing but an empty name (a three-letter station reached a URL).

**Tried again.** CI's first run of this failed on Ubuntu because
aviationweather.gov answered the HUD test with 504 Gateway Timeout; the
packages, minutes later, fetched it everywhere. Weather and DEM downloads are
now tried three times, two and then four seconds apart, when a server fails
(5xx) or nothing answers, and never after any other status
(`world::fetch_with_retries`; watched to fail with every status taken as a
failure).
