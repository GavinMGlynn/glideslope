### A key has one aircraft: a second session takes over once it has sealed something, 2026-09-29 — tail done

**What is still missing first.** On a full server a player who starts again is
refused `SERVER_FULL` until their old session is let go for silence: `take()`
refuses a full session before building a `Responder`, so it never learns whose
initiation it is (`docs/THREATS.md` keeps that bound). That is a new tail in
the plan. The client with the window came back by itself only from #50 on (the rejoin
tail); this is the server side only.

**What was wrong.** A client started again from a new port while its old
session was live - before `--timeout` had let it go - was admitted to the same
slot (`Slots::admit` is by key) but given a second aircraft by `Fleet::give`,
and the player flew two until the old one timed out.

**What it does now** (`src/frontend/server/main.cpp`):
- **A key has one aircraft, however many sessions it has.** An initiation for
  a key already connected at another address makes a session that shares that
  key's aircraft number instead of calling `give()`. A take-over of an AI's
  aircraft sets the new number on every session of the key.
- **The take-over waits for proof** (`Connection::proven`): at the first
  sealed datagram that opens under a session, every other session on its key
  is sent `LEAVING` (`tell_leaving`, shared with the operator's drop) and let
  go, printed as `<new> took over from <old>: a new session for <key>`.
  `LEAVING` rather than silence, because a client still running on the old
  session would otherwise be refused, join again, and take the aircraft back.
- **Why not at the answer.** The initiation has no timestamp, so nothing in it
  says it is fresh; anybody who captured one could replay it from an address
  of their own and, if that took over, end a live player's session. Sealing
  needs the initiation's ephemeral secret, which only the client has, so a
  replay's session never proves itself: it shares the aircraft, flies nothing
  (it cannot seal an input), and goes after `--timeout`.
- **`let_go` takes the aircraft and the slot only with the last session on the
  key**, and `drop()` lets go every session on the dropped key.
- `glideslope_cli connect --again-from-elsewhere` sends a copy of its
  initiation from a second port once its session is up, waits for it to be
  answered, and flies on - the replay a test needs.

**Verification** (Linux release, 2026-09-29):
- `a_second_session_for_a_key_takes_over_its_players_slot_and_aircraft`
  (`server_one_key_two_addresses.cmake`, port 24731): two clients with one key,
  the second started once the first is in its session, the first staying until
  the second has gone. The key is admitted twice, taken over once, the first
  client hears `LEAVING`, and the server flew one player's aircraft. Watched
  failing with the sharing taken out (every session given its own aircraft):
  "the server flew 2 players' aircraft for one key, not one".
- `a_replayed_initiation_takes_neither_a_live_players_session_nor_its_aircraft`
  (`server_initiation_replayed_while_live.cmake`, port 24735): a live client's
  initiation replayed from another port. Admitted twice, nothing taken over,
  the client never told to leave, the copy's session let go for silence, one
  aircraft, banked past 90 degrees by the client. Watched failing with the
  take-over moved to the answer: "a replayed initiation took over a live
  session".
- `a_copy_of_an_initiation_from_another_address_arriving_first_does_not_keep_its_client_out`
  now expects one aircraft, not two, and one take-over; it also failed with the
  sharing taken out.
- Also passed: the initiation-again, joins-again, drop-keeps-out, goodbye,
  repeated-handshake and four-players tests, and the port check.

**From the review of PR #53** (the security core was sound; these were not):
- **A resent initiation kept an unproven session alive for ever** - the
  `already` branch of `take()` refreshed `last_heard_s`. It no longer does, for
  any session: only a sealed datagram that opens is hearing from a session. So
  an unproven session goes `--timeout` after it was admitted.
- **Reflection**: unproven sessions were sent state at 25 Hz, `AIRCRAFT`
  definitions and pings. Now they are sent their handshake answer and nothing
  else (the ping loop and the state loop skip them; `tell_leaving` too). Every
  client seals something at once: `glideslope_cli` and the document client
  already did (a `PONG` nobody pinged for, and inputs); `net::ClientSession::connect`
  - the window client's - now sends that `PONG` as it returns, since `Online::join`
  waits for a state update before it sends anything. The window-client tests
  (joins, flies, takes over, hands over, rides along) and `--online` passed.
- **At most two unproven sessions per key** (`most_unproven_per_key`): a
  third lets the oldest go, so the newest, perhaps an honest restart, is never
  the one refused.
- **The aircraft and slot go with the key's last proven session** (`let_go`):
  when it goes, every unproven session left on the key goes too ("let go X,
  unproven, with its key's last proven session"), so a replayed session cannot
  keep a player's aircraft flying after their goodbye. An unproven session
  going while others remain takes nothing; the key's only session going takes
  both. The cost: a restart whose first sealed datagram has not yet arrived
  when the old session goes is let go with it, and its client, refused, joins
  again.
- **A taken-over client** used to print "dropped by the server's operator";
  `glideslope_cli` now says "the server ended this session (dropped, or taken
  over by a newer session for this key)". PR #50, the window client, has the
  old phrase and was open when this was written.
- `--again-from-elsewhere FILE` now keeps resending the copy every quarter of a
  second from its own port, with a sealed datagram that opens under nothing;
  the server refuses that only once the copy's session has gone, and then FILE
  is written with how many datagrams the copy's session was sent besides its
  answer.

**Verification of the review's fixes** (Linux release, 2026-09-29):
- `a_replayed_initiation_takes_neither_a_live_players_session_nor_its_aircraft`
  now resends the copy faster than the server's six-second `--timeout` and
  leaves only once the copy's session is gone (`--until-exists`), so the order
  is built. It requires the copy let go for silence, sent 0 datagrams besides
  its answer, and the player's aircraft out of the sky only after the player
  said it was leaving. Watched failing three ways, each reverted: with
  `let_go` taking the aircraft whenever any session went ("the player's
  aircraft went before the player did"); with the refresh put back ("the
  copy's session was never let go while its initiation was resent", after the
  client's 120 s); with pings and state sent to unproven sessions ("sent 200
  datagrams besides its answer").
- `a_player_the_operator_drops_is_let_go_from_every_address_their_key_is_at`
  (`server_drop_every_session.cmake`, port 24737): a player with a replayed
  copy's session beside their own is dropped (`--drop-once-flown`); both
  sessions are let go at once, none for silence under a one-minute timeout, and
  the client is told. Watched failing with `drop()` and `let_go()` as they were
  before this PR (only the dropped row let go, and the aircraft kept while any
  session on the key remained): "the drop let 1 of the key's sessions go, not
  both". Either change alone keeps it green - each lets the other session go.
- Passed with the fixes: the two new tests above and the take-over test, the
  from-another-address, initiation-again, joins-again, drop-keeps-out,
  goodbye, repeated-handshake, four-players and document-client tests, the
  window client's five server tests, `--online`, and the port check.

**From the re-review: the window client's proof is resent.** One sealed `PONG`
from `ClientSession::connect` was all the window client sent before waiting
for a state update, and the server now sends an unproven session nothing: had
that `PONG` been lost, `Online::join` would have given up after ten seconds
and said "flying alone" while the server let the session go.
`ClientSession::poll` now seals a `PING` every `prove_every_s`, a quarter of a
second, from its first call until anything has opened under the session, and
stops then. PR #50 (the window client joining again) knocks once a second
after a second of quiet; the two meet in `poll()` on its rebase. Held by
`a_client_whose_first_sealed_datagram_is_lost_still_proves_its_session`
(`tests/unit/test_session_over_a_socket.cpp`): a stand-in server answers the
handshake, throws away the client's first sealed datagram, and waits on the
client's own clock, stepped a hundredth of a second at a time, for another
that opens - within two seconds of it - and then, once it has sealed
something back, requires the knocking to stop (at most one more). Watched
failing both ways: with the resend taken out ("the client sealed another ...
within two seconds of its own clock"), and with the knocking never stopping
("8 sealed after"). TRANSPORT.md now says a client keeps sealing until
anything opens, that `LEAVING` from the server means dropped or taken over and
is not sent to an unproven session, that answering `PING`s keeps a session
only once it is proven, and names a client that loses every answer for a
whole `--timeout` as not defended. THREATS.md says a replay needs no live
capture, and that a steady flood of two or more replays per round trip locks
a restart out, holding the slot and aircraft, for as long as it lasts - and
why the cap of two is still the trade to make.

CI run 36529359658 (the head before this) failed one test on one Linux debug
shard: `the_client_with_the_window_hands_its_aircraft_to_the_ai_on_a_server_and_takes_it_back`,
its guard that the aircraft was fast enough at a switch for the test to mean
anything - "carried at 24.1 m/s", the floor 25. That is the test declining to
count, not the step bound failing; it passed here (Linux release) and in every
other shard, and nothing in this branch changes how fast an aircraft flies.
It is left for the switch tests' owner, and named here rather than hidden.

`tests/ci_costs` has the four new tests, from run 36529359658's shards where
they ran; the unit test at a second, as its neighbours are; and, for the
Windows shards still running when this was written, the same preset's cost of
the test of the same shape (from-another-address, drop-keeps-out).
