### The window client hears the server's air in the order it was said, 2026-11-18 - no end-to-end run of it yet

**What it does not do, first.** No test runs the window client itself
(`glideslope --headless`) joining a server mid-change: the unit test below
puts the joining messages through the real `net::ClientSession` and makes the
air as `client::Online` does, but `Online::hear`'s few lines around it and
`Flight::fly_server_air` are run by no test of a mid-change join. The item
stays `[ ]` until one does. An end-to-end run needs a server whose stations
change three times in one slot within a blend - `--weather around` against a
stub that answers the box differently in turn - and a way to compare the
window client's air with the server's to the last bit, which neither prints
today.

**The cause** (2026-11-14's open part). The window client flew what
`net::Told` keeps of each slot - its newest weather and the one before - sorted
by when the stations changed. A slot told three weathers or more on joining
(a chain of changes, each blending from the one before) was flown from the
last two, and a station let go lost its runways.

**The fix.**
- **`net::Told::air_said`** says what `hear` has just taken of the air: a
  whole weather and its forecast, or a slot's runways (`net::AirSaid`).
- **`net::ClientSession::take_air`** hands up everything said of the air
  since last asked, in the order said; a session joined again starts afresh.
- **`frontend::HeardAir::said`** takes one of them: flies the weather or
  lays the runways. The headless client already took each message as it
  came; the window client now does the same, through the same HeardAir.
- **The window client's air is the session's, not a flight's**:
  `client::Online` keeps one `HeardAir`, made of everything taken, standing
  on the flight's geoid (now shared). `Flight::fly_server_air` flies its
  stations on the aircraft's own clock. A flight made since - another
  aeroplane taken over - flies the same air, every change still blending,
  where before it was given each slot's newest and the one before. Joined
  again, the air is made afresh.
- The stand-in server of test_session_over_a_socket.cpp is shared
  (`tests/unit/stand_in_server.hpp`) and sends reliable messages.

**Verification** (linux-debug):
- `the_window_client_joining_while_the_servers_stations_change_flies_the_servers_air`:
  #376's scenario (`joined_mid_change`, shared with the headless client's
  test): joined at 715 s, slot 1 told 3 weathers. The messages a joining
  client is told are sent by a stand-in server through the reliable layer,
  sealed, over the loopback, to a real `net::ClientSession`; taken with
  `take_air` and made into air with `HeardAir::said`, as `Online` does, every
  one of them taken (counted against what was sent). At 150 points (12 on
  ice, 30 wet) the joiner's air is the server's to the last bit.
  **Seen red**: with `take_air` handing up each slot's newest and the one
  before, sorted by the stations' change, as the window client flew them
  before - only 12 of the 17 things said were taken, slot 1 two weathers of its three.
- `a_client_joining_while_the_servers_stations_change_flies_the_servers_air`
  now hears through `Told::air_said` and `HeardAir::said`, and passes.
- The session tests over a socket, and the window client's
  `the_client_with_the_window_flies_the_servers_weather_...`, pass.

The selftest's hash does not move: it flies no weather and no server.

`the_stations_flown_are_kept_when_the_box_around_an_aircraft_answers_204`
failed once here, run beside three other end-to-end tests ("the air at the
first step was not YSSY's alone ... 0.986 YSSY's"), and passed alone. It
flies the headless client, which this change does not touch; the load is
the likely cause, not traced.
