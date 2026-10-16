### A command-line client the server has let go joins again by itself, and a dropped one does not, 2026-09-27 — tail done 2026-09-29, above

*Since done for the client with the window too: see "The client with the
window, let go, joins again by itself", 2026-09-29.*

**What is missing first.** **The client with the window still cannot come
back**: `net::ClientSession` does not notice that it has been let go, and
`client::Online` has no way to be given a new aircraft at its old number, so a
windowed client let go for going quiet goes on sealing under keys the server
has thrown away until it is restarted. Only `glideslope_cli connect` joins
again. So the tail stays open, with that named in it. Nothing tests the
client going back to its old session when a forged refusal ends it during a
blip (below); the code path is there and reviewed, not exercised. A server
that restarts forgets whom its operator dropped.

**What was wrong.** A client the server had let go for silence - a process
stopped, a laptop shut, a network gone for longer than `--timeout` - went on
sealing inputs under the old keys. The server refused each one
(`BAD_HANDSHAKE`, as it refuses any sealed datagram from an address with no
session), and `stay()` ignored every refusal. The plan's line blamed the
`Taken` memory, but a client that resent its *old* initiation would have been
dropped as a copy, and this one sent nothing of the kind: it never tried.

**What it does now** (`src/frontend/cli/main.cpp`):
- **A session gone quiet is knocked on from the client's end.** While nothing
  has opened under it for a second, `stay()` sends a sealed `PING` of its own
  once a second. A server that has the session answers `PONG`; one that has
  let it go refuses it - so a client that sends nothing else still hears.
- **A refusal is believed only of a session gone quiet**: nothing opened for
  `quiet_before_believing_s`, three seconds - three of the server's knocks and
  two of the client's own unanswered - and only from the server's address,
  and only `BAD_HANDSHAKE` (`refusal_from()`). A forged refusal while the
  session works moves nothing (`docs/THREATS.md`, the client's half of the
  refusal rule), and one heard while finishing is ignored.
- **Then it joins again** (`join_again()`): a new `Initiator` with the same
  static key, so a new ephemeral key and not a copy - `Taken` and #25's ghost
  protection are untouched, and a copy of an old initiation is still dropped.
  It resends every quarter of a second until answered, a minute the most, and
  takes no `BAD_HANDSHAKE` for an answer: sealed datagrams sent under the old
  session may still be on their way to be refused. `SERVER_FULL` or `DROPPED`
  from the server ends it, with the reason. **It keeps the old session's
  `Unsealer` meanwhile**, and if anything opens under it the session was never
  gone - a forged refusal, or a blip - and it goes back to it, so a forged
  refusal costs nothing. Then it stays for what is left of
  its SECONDS, flying as before; what it was told to do once (hand over, take
  over, stall) it does not do again.
- **The operator's drop is a ban for the rest of the run, and the client is
  told** (review of #45: without it the drop button became a three-second
  kick). `drop()` in the server - the window's button and a test's
  `--drop-once-flown` both take it - sends the player `LEAVING`, sealed under
  its session, three times, remembers its static key, and lets it go. An
  initiation from a remembered key is refused with a new reason, `DROPPED`
  (`07`), from any address. The client that opens the server's `LEAVING` says
  "dropped by the server's operator; not joining again" and exits 1; if every
  goodbye is lost, it joins again, is refused `DROPPED`, and exits saying so.
  A client older than `07` reads it as `UNKNOWN` and stops all the same.
- **Test flags**: `--stall-once-rolled` (with `--fly`) stops once its
  aircraft has rolled past 90 degrees, sending and answering nothing and
  reading and throwing away whatever arrives, until nothing has come from the
  server for three seconds; then it knocks once a second, stalls again if
  anything opens under the session, and ends the stall on the server's
  `BAD_HANDSHAKE` - the event, not a time. Joined again, it leaves once its
  new aircraft has rolled past 90 too.
  `--done FILE` writes FILE when the client has gone, for another client's
  `--until-exists`.
- **The wire**: one new refusal reason, `DROPPED` (`07`), and `LEAVING` now
  goes server to client too. `docs/TRANSPORT.md` says both, when a client may
  believe a refusal, that this project's command-line client knocks when it
  hears nothing, and that it joins again by itself - and that the windowed one
  does not yet. `docs/THREATS.md` counts eight reasons, seven sent.

**Verification.** `a_client_stalled_past_the_timeout_joins_again_by_itself`
(`tests/cmake/server_joins_again.cmake`, port 24701 - 24801 was outside the
test block and failed the port check) builds the stall: a
server with `--timeout 3` and `--until-empty`, the stalling client flying, and
a second client that stays until the first has written `--done`. It holds the
stalling client admitted twice and the other once; one session let go for
silence and two for goodbyes; no initiation dropped as a copy; exactly three players' aircraft -
the stalling client's first, taken out of the sky when it was let go, its
second, and the other's - and exactly the stalling client's two banked past 90
degrees, so no ghost aircraft and the new one flown. 10.7 s here (Linux
debug). **Seen to fail**: with the refusal never believed, the client stayed
its whole 300 s on the dead session and the test failed with "admitted 1
times, not twice"; reverted. The two tests of #25's ghost protection,
`a_copy_of_an_initiation_arriving_after_its_session_was_let_go_makes_no_second_player`
and
`a_copy_of_an_initiation_from_another_address_arriving_first_does_not_keep_its_client_out`,
still pass.

`a_player_the_operator_drops_is_told_and_kept_out_for_the_rest_of_the_run`
(`tests/cmake/server_drop_keeps_out.cmake`, port 24702): a server with
`--drop-once-flown` drops the first client once it has flown; that client must
say it was dropped and not joining again; the same key started again once it
has gone must be refused, reason 7; the server must say it dropped and refused
it, and admit that key once. 2.7 s here. **Seen to fail twice**: with the
server not remembering the key, the restarted client was admitted again
("admitted 11fc7622" twice) and the test failed; with the client not reading
the server's `LEAVING`, it failed with "the dropped client did not hear it was
dropped" - its fallback did hold: it joined again, was refused `DROPPED`, and
stopped. Both reverted. `the_transport_document_and_the_code_agree_byte_for_byte`
holds the new row; `every_flag_the_server_prints_in_its_usage_is_one_it_takes`
the new flag. The window's drop test (`server_window.cmake`) needs a display
and is run on Windows below.

**macOS CI failed it once** (run 36289235846): the first client was admitted
and dropped before the staying client had joined, so the server found
everybody gone 0.1 s in and stopped, and the restarted client waited a minute
for no answer and wrote nothing. Not a socket difference - the order was never
built. Now the first client joins only once the staying one has heard the
server introduce an aircraft (`--after-ready` on its `--heard` file). Rebuilt
on purpose here (the staying client held back a second): red, with "no answer"
now in the restarted client's file; restored, green in 2.4 s. And
`glideslope_cli connect` now writes its outcome to `--heard` on every way out
(`say_outcome()`): no answer, an answer that does not open, refused when
joining again, or could not join again.
