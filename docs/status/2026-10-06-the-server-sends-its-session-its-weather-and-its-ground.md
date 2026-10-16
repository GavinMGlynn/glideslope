### The server sends its session, its weather and its ground; clients fly its weather, 2026-10-06 — tail done

What was missing first: the server's air has **no ground's lift** (ridge and
wave lift, thermals over the terrain) - on the server and every client - and
**no test flies the window client in the server's weather**. Both are tails.

- **The server flies a weather** (`--weather STATION`, fetched again every
  `--weather-refresh`, default 900 s; or `--metar REPORT --station
  LAT,LON[,M]`; `--metar-then S REPORT` for tests; `--weather-blend`, default
  300 s), on the session's clock (`frontend::SessionClocked`), for every
  aircraft it makes. None is still air, as before.
- **Each client is told**, once proven: `TERRAIN_DATASET`, `SESSION`,
  `LOBBY` (again whenever it changes), `WEATHER` and `WEATHER_ALOFT` (again
  at each change). `net::Told` takes them in both clients;
  `frontend/same_air` turns a report into messages and back, and hashes the
  ground (coverage, strips, rules). Protocol version 3: `WEATHER` gained when
  it changed, its blend and whether a forecast follows; `docs/TRANSPORT.md`
  says it byte for byte.
- **Clients fly it**: `glideslope_cli connect --predict` and the window
  client (`Flight::heard_weather`, which then fetches none of its own; a
  window client on a server is refused `--weather`). `--own-air` is the
  test's client that ignores it.
- **The ground's lift is off on a server**: 256 DEM heights and a 512-point
  transform per step made the predicting client fall 4.7 s behind in a
  sanitized build, and without it a client was 6 m out from a server with it.
  With neither end flying it the error was 0.03 m (REQUIREMENTS 9, 2026-10-06).
- **A METAR keeps its raw words** (`Metar::raw`), cut to 256 bytes by whole
  words before the server flies it.

Verification: `a_client_flies_the_servers_weather_and_every_change_of_it_not_its_own`
(35 kt west, then 40 kt south-west at 12 s over 5 s; two predicting clients
behind 250 ms relays: told the ground, session, lobby and both weathers; the
one flying the server's weather worst 0.57 m, the one flying still air
2.7 m; bounds 1 and 1.5 m);
`a_client_told_the_servers_collision_ground_refuses_other_ground_and_leaves`;
unit tests `the_weather_a_server_sends_is_flown_to_the_last_bit_by_its_clients`
(144 points, bit for bit, through a change),
`still_air_is_said_as_an_empty_metar_and_flown_as_no_weather`,
`a_weather_is_taken_whole_or_not_at_all`,
`a_metar_too_long_to_send_is_cut_to_whole_words_and_flown_as_cut`,
`the_collision_ground_said_is_the_builds_coverage_strips_and_rules`,
`every_refusal_the_document_names_for_a_weather_is_refused`; the message,
NaN (27 fields) and limit tests extended.

From the review, the same day: **the air a predicted step flies is the air at
the moment the server flies that step**, replays included
(`sim::Prediction::session_time_s`, the clocks' difference applied to the
step), held exactly by
`every_step_a_client_flies_meets_the_air_at_the_moment_the_server_flies_it`
(84 replayed steps; seen red with replays left on one moment). Gusting
weathers in the multi-process test (30G45, 35G50) showed it - 1.6 m on the
old clock, 0.57 m on the new - but not reliably: while the clocks'
difference settles the moment is a step or two out, and a gusting run was
2.7 m on CI's macOS and 16 m at 500 ms here. That test flies steady winds
again; gusts matched while the difference settles are a tail. A METAR a client cannot read is refused and the air before it
kept (`a_weather_whose_metar_cannot_be_read_is_refused_and_the_air_before_it_kept`);
a `WEATHER` with a station past either pole or 180 degrees, a change before
nought or a blend outside 0 to a day is refused (seven more cases in the
refusal test; the NaN test counts 7 numbers out of range). The window client
counts weathers by the session they came in, so one joined again is never
left flying the old session's, and flies none before its ground is compared
or on other ground. A joining client mid-blend flies the new weather whole -
a tail. `glideslope_cli selftest`: hash 30ac70b84cab7d7c; it flies no weather.
Seen to fail: the conversion
dropping the turbulence severity, the server not resending a changed
weather, and every ground compared equal each turned their test red.
