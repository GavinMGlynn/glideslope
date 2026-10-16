### A player asks for an aeroplane as they join a server, and flies it: protocol version 04, 2026-10-06 — a tail, not yet closed

**What is still not chosen first.** **Where to start**: a player's aeroplane
starts where the server's plan starts, at its own starting speed, in the air.
**The window client draws another's aeroplane** from the same `AIRCRAFT`
roster the command-line clients are told (`client::Other::aircraft_id`, as
it always has); no test shows a window client another player's chosen
aeroplane, only the AI's. **Nothing says no to the
player**: an aeroplane the server's catalogue does not hold is said on the
server's standard output, and the client learns what it flies from its
`AIRCRAFT` message, not from a refusal.

**What it does now.**
- **The handshake initiation's payload is the aeroplane asked for**
  (`net::write_asked_aircraft` / `read_asked_aircraft`): a `u8` length, 1 to
  32, and that many bytes of a catalogue id (`a`-`z`, `0`-`9`, `-`, `_`),
  nothing after; empty asks for nothing. Sealed with the initiation, so no
  new message and no round trip: the server gives a player an aircraft as it
  admits them. **The protocol's version moves to `04`** (TRANSPORT.md:
  envelope, refusal, the version list and "Starting a session"); the ground
  is unchanged, and `the_protocol_version_moves_with_the_collision_ground`,
  the document's client (`kVersion`) and the gearstick refusal test say `04`.
- `Fleet::give(slot, asked)` looks the id up among the catalogue's
  (`sim::known_aircraft`, never a path) and builds that aeroplane - its model,
  its starting speed, whether it is a seaplane - where the plan starts; an id
  it does not hold, or a payload that does not read, gives the plan's. It
  says "slot N asked for ID, and flies it" or "... which this server does not
  have: it flies ID".
- **The clients ask**: `glideslope_cli connect --aircraft ID` (refusing one
  that is not an id's shape), and the client with the window when given
  `--aircraft` on a server (`ClientSession::connect`'s `aircraft`). Joining
  again asks again: `net::Rejoin` carries the payload.
- REQUIREMENTS section 9 records the decision.

**Verification.**
`a_player_who_asks_for_an_aeroplane_when_joining_flies_it_and_every_client_is_told_so`
(`tests/cmake/server_aircraft_choice.cmake`, port 24805): the plan flies the
Cessna 172P; three command-line clients join - one asking for `pa28` and
flying full left aileron, one asking for nothing and one for `no-such-plane`,
the two staying until the first is done. It holds that the server gave the
PA-28 and said it had no such other plane; that the only player's aircraft
banked past 90 degrees is the one every client was told is the PA-28, and
each client was told of exactly one PA-28 and three Cessnas. 18 s (Linux
debug). **Seen to fail** with the server ignoring the payload: "the server
did not give the PA-28 asked for"; reverted.
`the_aeroplane_asked_for_reads_back_and_a_malformed_ask_reads_as_none`
(unit): five ids read back as themselves, five that are not ids are not
written, and an empty payload, a length of 0, too few bytes, a byte
trailing, a byte outside an id's and 33 bytes all read as none. **Seen to
fail** with the trailing byte allowed ("a byte trailing is none"); reverted.
`the_client_with_the_window_flies_the_servers_aircraft_and_draws_the_others`
(`client_on_server.cmake`) had the window client ask for an F-15C and pinned
that the server's Cessna overrode it; it now pins that the server gives it
the F-15C asked for and it flies that, still drawing the AI's Cessna.
