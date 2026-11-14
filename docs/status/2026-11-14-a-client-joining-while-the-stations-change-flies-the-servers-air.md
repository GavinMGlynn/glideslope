### A client joining while the server's stations change flies the server's air, 2026-11-14

**What it does not do, first.** The window client keeps each slot's newest
weather and the one before it (`net::Told`), so a joiner told a chain of
changes longer than that - a slot given a station at one change and another
at the next, both still blending, and a station let go whose own report
blended too - flies what the two it kept make of it. The headless client
(`glideslope_cli`) hears every message in order and flies the whole chain.
The weather a station's report blends from is told one deep, as on
2026-10-08: a report arriving while the one before it still blended in is
told as blending from that one alone.

**The cause.** A client joining was told each slot's weather now, every
`WEATHER` naming the newest change of the stations as when they changed. Its
`world::StationSlots` made one change from none - flown whole - so until the
change's blend ended (five minutes by default) it flew the new stations
whole while the server flew the blend from the old ones.

**The fix.**
- **`frontend::weather_said_on_joining`** says everything a joining client
  is told of the weather, and glideslope_server sends it on joining. Not
  mid-change, it is what was sent before: each slot's weather, after what it
  blends from while it does. **Mid-change** - the newest change has stations
  it blends from, and its blend has not ended - it tells every change of the
  stations the server still flies (`world::StationSlots::Stations`, its
  `from` chain), the oldest first, each as the slots that differ from the
  one before, naming that change's moment: so the joiner's `StationSlots`
  makes the same changes from the same stations, and a station there in
  both changes is told once and is one weather on the client as on the
  server. A station let go since is told with its runways after it.
- **`world::StationSlots::Stations::slots`** keeps the slots as they were
  at each change; `StationSlots::air()` gives the newest; and
  `ReportedWeather::changed_at_s` / `blending_from_changed_at_s` say when a
  weather let go took over.
- **The window client flies the weathers it is told in the order the
  stations changed** (a stable sort on each `WEATHER`'s stations' change),
  not slot by slot: heard slot by slot, a slot's new station would make a
  change after another slot's older one.
- **No byte and no version changed**; `docs/TRANSPORT.md` says the order
  told on joining, and that the runways' conditions now follow every slot's
  weather rather than each its own.

**Verification** (linux-debug):
`a_client_joining_while_the_servers_stations_change_flies_the_servers_air`
(unit): YSSY and YSWS from 0 s, a new report of YSWS at 600 s, YSWS let go
for YSRI and YWLM found at 650 s, YWLM let go and YSCB found at 700 s while
the change at 650 s still blends, a new report of YSSY at 710 s, each over
120 s. A client joining at 720 s - inside both changes' blends, checked - is
told what the server tells a joiner, through the wire's bytes, `net::Told`
and `frontend::HeardAir`: at 4 places, 3 heights and 10 times from 720 s to
1000 s, 120 points (counted), its air is the server's to the last bit, as a
client's there from the start is. **Seen red** on today's code first - the
function saying each slot now, as the server did - failing at the first
point, 720 s at -33.75, 150.80, 9 m.

The selftest's hash does not move: it flies no weather and no server.
