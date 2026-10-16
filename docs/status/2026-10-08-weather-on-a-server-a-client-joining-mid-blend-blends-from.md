### Weather on a server: a client joining mid-blend blends from the same weather; the window client measured in it; gusts measured, 2026-10-08 — two tails done, two open

**What is still missing first.** **The ground's lift is still off on a
server**, and was not retried: the headless predicting client
(`glideslope_cli connect --predict`) flies over no ground at all - flat
terrain at 0 m - so it has no DEM to fly the lift over, and giving it the
collision DEM comes before the lift can be flown on both ends. **Gusts are
not wholly the server's**: in gusting air the median prediction error is the
steady air's, but the worst was 0.40, 1.56 and 3.98 m over three runs
against the steady air's 0.09 and 0.38, the 3.98 at 12.8 s, inside the change
of weather's blend; while its clocks' difference moved, the pacing said it
was 15 steps from the one held at worst. Its cause is not found. Both stay
`[ ]`.

- **A client joining while a weather blends in is told first what it blends
  from** (`world::ReportedWeather::blending_from`, the server's
  `Fleet::weather_blending_from`), with when that one took over, and then the
  new one - so it blends as the server does. Told only the newest, it flew
  that whole until the blend ended. `net::Told` keeps the weather before the
  newest (`before`, `before_aloft`), and the window client, making its air
  afresh - joined mid-blend, or a flight made since - hears that first.
  **No protocol version change**: no byte of `WEATHER` or `WEATHER_ALOFT`
  changed; a client that keeps only the newest flies it whole as before.
  `docs/TRANSPORT.md` says the order.
- **The server's `--changed-file FILE`** is written once `--metar-then`'s
  METAR has taken over (and the server stops, saying so, if it cannot be), so that a test's client joins mid-blend on that
  event, not on the clock. `glideslope_cli connect` says the server's clock
  when told the session (", N s on its clock").
- **The window client measures its prediction error** as `glideslope_cli`
  does - where the server says the aircraft was at a step against where this
  client had flown it by then, once the clocks' difference is known - and
  says it at its shot (`glideslope: prediction error: N updates compared, the
  median M m, the worst W m`).

Verification: `a_client_joining_while_a_weather_blends_in_blends_from_the_same_weather`
(35 kt west, then 40 kt south-west from 6 s over 120 s; two predicting
clients behind 250 ms relays, one joining at 0.77 s on the server's clock,
the other two seconds after the change at 8.8 s; both told both weathers;
medians 0.046 and 0.043 m against SAME_M 0.25; seen red with the server
sending only the newest: the joiner was told one weather, and its median was
0.757 m); unit test
`a_client_joining_mid_blend_is_told_what_it_blends_from_and_flies_the_servers_air`
(90 points to the last bit, joined at 1000 s of a 300 s blend from 900 s; the
newest alone other air at all 72 inside the blend; seen red with
`blending_from` ending at the change);
`the_client_with_the_window_flies_the_servers_weather_within_the_headless_clients_bound`
(the window client through a 250 ms relay in 35 kt from the west: says it
flies the server's METAR, median 0.003 m, worst 0.007 m over 449 updates,
against the headless client's 0.25 m; seen red, 0.422 m, with the window
client ignoring the server's weather);
`a_clients_gusts_are_the_servers_at_the_servers_moment_and_differ_at_a_skewed_one`
(unit: 30G45 through the wire's bytes, 656 points under the gusts' 610 m; at
the server's moment every one the server's to the last bit, and at one step,
two steps and five seconds out every one other air, by over 1 m/s at five
seconds; seen red with the client's clock a step out). An end-to-end gusting
run was tried and dropped from review: its medians, 0.048 to 0.051 m, were
under the steady test's 0.25 m bound with the gust clock five seconds wrong
(0.150 m) as well, so it did not test its rule; its worst, 1.6 and 4.0 m in
the change's blend, is the open part above. The window client's bound is
0.1 m, not the headless 0.25: thirty times the 0.003 m measured, a quarter of
the 0.42 m flying still air.
**A client counts the extra `WEATHER` as a weather**: `net::Told::weathers`
is one more for each whole weather heard, so a client joining mid-blend says
"weather 1" for the one blended from and "weather 2" for the new one - as a
client there before the change does (the join test reads both lines); the
window client keys each flight's weathers by that count and hears the one
before at the count less one. A client of this version before the change
kept only the newest, and counted the extra one all the same.
`every_flag_the_server_prints_in_its_usage_is_one_it_takes` counts
`--changed-file`. The selftest's hash does not move: it flies no weather and
no server.

Housekeeping: the three items fixed at cause on 2026-10-08 and owed only CI
runs - the rate-limit test on Windows debug, the ground test's two clients
racing, the engine-stop prediction test on Windows - are now lines under
"A month of clean nightly runs" in `COMPLETION_PLAN.md`.
