### A window client joining while the server's stations change flies the server's air to the last bit, 2026-11-21

**What it does not do, first.** The new test needs a GPU driver (it is
skipped, exit 77, without one) and runs on Linux and Windows, not macOS, as
the other window tests. A station given new reports quicker than a blend is
told to a joiner as its whole chain, one message per report: the chain is
bounded only as the server bounds it (`blend_kept_s`), and a join that
cannot be queued is said, as since 2026-11-14.

**What was missing.** No test ran the window client itself joining a server
mid-change (2026-11-18's open part). Run end to end, it was **not** the
server's air: 66 of 150 points the same.

**The cause** - the server's, and so the headless client's too. Every look
for the stations gives every station kept a new report (`slot_changes`),
the same text or not, and a report arriving mid-blend blends from the whole
weather as it was. So a station looked for more often than a blend flies a
chain - YSSY's report at D blending from C's, from B's, from A's rain. A
joiner was told only the newest and what it blends from
(`ReportedWeather::blending_from`, one deep), and flew C's report whole:
no rain left in it, a different pressure, a dry runway. The unit test of
2026-11-14 set each slot only when it changed, which the server does not.

**The fix.**
- **`world::ReportedWeather::blending_chain`**: everything a station's
  weather blends from that still reaches the air, oldest first.
- **`frontend::weather_said_on_joining`** tells all of it before each
  slot's weather, where it told `blending_from` alone. The joiner's slot
  makes the same updates, at the same moments, as the server's did.

**The test's tools.**
- `--test-say-air POINTS FILE`, on glideslope_server (as it stops) and on
  glideslope (as it quits at `--quit-at`): the air flown of the server's
  stations at each line TIME LAT LON HEIGHT of POINTS, every field of
  `sim::Conditions`, each double as "%a" (`frontend::air_at_points`).
- glideslope_http_stub: a PREFIX given more than once is answered with its
  FILEs in turn, then STATUS - with `--stall`, a transfer never finished.

**Verification** (linux-debug):
- `a_window_client_joining_while_the_servers_stations_change_flies_the_servers_air_to_the_last_bit`
  (tests/cmake/window_joins_mid_change.cmake): `--weather around`, looked
  for every 2 s, blended over 600 s; the box answered A (YSSY in rain, YSWS,
  YWLM), B (YSSY's rain over, YSWS's new report), C (YSWS let go for YSRI),
  D (YWLM let go), then stalled. The stub's file says the server looked
  after D; only then does `glideslope --headless` connect. Checked: four
  findings and none after; slot 1 told YSWS, its new report, YSRI on
  joining. At 5 places, 3 heights and 10 moments from 120 s to 1,000 s -
  150 points, counted, 30 on YSSY's wet runway - the client's air, said at
  15.1 s, is the server's at 150 of 150, every field to the bit.
  **Seen red**: with the joiner told one deep, as before, 66 of 150 (the
  first: at YSSY the client flew C's report whole, dry, at 1011 hPa).
- The unit tests of the same air (#45 to #50), the server's flags, the
  timeouts' and ports' checks, and the stub's other users (the stations
  tests, end to end, and quitting mid-refresh) pass.

The selftest's hash does not move: it flies no weather and no server.
