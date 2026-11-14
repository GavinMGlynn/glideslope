### A client joining while the server's stations change is told the stations they blend from, 2026-11-14 — the window client still open

**What it does not do, first.** **The window client is not done**, and the
item stays `[ ]`. It flies what `net::Told` keeps - each slot's newest
weather and the one before it, and the newest `RUNWAY_CONDITIONS` - in the
order the stations changed (a stable sort on each `WEATHER`'s stations'
change), not each message as it arrives. A slot told three weathers or more
on joining (a station whose own report still blends, then another station
in that slot) is flown from the last two, and a station let go loses its
runways. No test flies the window client's path. What it needs: the window
client hearing every weather message in arrival order through the code the
headless client uses (`net::Session` keeping them in order, a flight made
since flying them again), and a test of it joining at 715 s as below.
The weather a station's report blends from is told one deep, as on
2026-10-08.

**The cause.** A client joining was told each slot's weather now, every
`WEATHER` naming the newest change of the stations as when they changed. Its
`world::StationSlots` made one change from none - flown whole - so until the
change's blend ended (five minutes by default) it flew the new stations
whole while the server flew the blend from the old ones.

**The fix.**
- **`frontend::weather_said_on_joining`** is everything glideslope_server
  tells a joining client of the weather. Not mid-change, each slot's weather
  after what it blends from, as before. **Mid-change**, every change of the
  stations that still reaches the air (`world::StationSlots::Stations`, its
  `from` chain, while each blend has not ended), oldest first, each as the
  slots that differ from the one before at that change's moment, with the
  runways of each station told in an older change after it; then the
  newest change's slots. A station there in both changes is told once, and
  is one weather on the client as on the server.
- **Each weather names the moment it took over**: `ReportedWeather` is made
  with it (`changed_at_s`), so a station let go before any new report is
  told when it was given its slot, not nought. An empty slot names the
  moment of its change (on joining when not mid-change, the moment the
  stations last changed).
- **The chain is bounded** (from the review): `StationSlots::set` lets go,
  at each change, of what the newest change whose blend ended
  `blend_kept_s` (60 s) before it blended from - and with it every change
  before - not only the newest's. So the changes kept are those within a
  blend and 60 s of each other: with changes every 30 s over a 120 s blend,
  7 (it grew to 101 over 100 changes before). A joiner is told only those
  whose blend has not ended: 5 there.
- **A join that cannot be queued is said**: glideslope_server counts the
  join's messages the reliable layer refuses (256 in flight) and says so on
  its standard error.
- **The runways' conditions now follow every slot's weather on joining**,
  not each slot's own: an order change, written in `docs/TRANSPORT.md`. No
  byte and no version changed.

**Verification** (linux-debug):
- `a_client_joining_while_the_servers_stations_change_flies_the_servers_air`:
  each slot set as `Fleet::set_slot` sets it (`StationSlots::set`, the
  change's moment its own). YSSY in rain and YSWS from 0 s, a new report of
  YSWS at 600 s, YSWS let go for YSRI and YWLM and UUEE (runway 24 iced,
  laid on its runways) found at 650 s, YWLM and UUEE let go for YSCB at
  700 s, YSSY's rain over at 710 s (its runways still wet), each over 120 s.
  Joined at 715 s - inside both changes' blends and YSWS's own, checked;
  slot 1 told 3 weathers, checked - through the wire's bytes, `net::Told`
  and `frontend::HeardAir`: at 5 places, 3 heights and 10 times, 150 points
  (counted, 12 on ice and 30 wet), the joiner's air is the server's to the
  last bit, as a client's there from the start is. **Seen red**: told each
  slot now, as before, the joiner's air was the server's at 66 of 150 and
  slot 1 told 1 weather; without the runways of the stations let go, 138 of
  150 (the ice).
- `the_stations_flown_keep_only_the_changes_that_still_reach_the_air`:
  100 changes 30 s apart, 120 s blend: at most 7 changes kept, reached; a
  joiner told 5 weathers. **Seen red** with only the newest's blend let go,
  as before: 101 kept.
- The related unit tests (#31 to #56, the transport document's checks among
  them), and end to end `a_client_joining_while_a_weather_blends_in_blends_from_the_same_weather`,
  `a_client_flies_the_stations_around_it_...`, `the_stations_flown_are_kept_when_the_box_...`
  (204, 404) and the window client's `the_client_with_the_window_flies_the_servers_weather_...`
  pass; the unit tests on Windows debug too.

On the Windows development machine the new join test crashed once at exit,
in a thread, after printing that it passed; run again it passed. Neither
new test starts a thread, so the cause is outside this change (Webroot, or
the harness's exit), not traced; CI's Windows jobs are the comparison.

The selftest's hash does not move: it flies no weather and no server.
