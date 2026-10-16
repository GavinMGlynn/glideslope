### A client says it is leaving, and is let go at once, 2026-09-26 — tail done

**What is missing first.** A goodbye is not reliable: three copies, and if
all three are lost the server's `--timeout` still lets the client go, as
before. A crashed client sends none. **A client the server has let go still
cannot come back** by sending its old handshake again - the sibling tail;
this does not fix it, because a goodbye ends the session the same way the
timeout does, and a client that returns must make a new initiation either
way. The document says a client may leave its goodbye out, since the
timeout covers it.

**The wire.** A new kind inside the seal, `LEAVING` (`06`), whose whole
plaintext is that one byte - a 31-byte datagram sealed. `docs/TRANSPORT.md`
has it under "Leaving: `LEAVING`", and `docs/THREATS.md` under "Ending
somebody else's session". A `06` with anything after it is not a goodbye and
is ignored. **It is sealed, so only the session's own client can end it**: the
server acts on it only once it has opened under the keys of the session its
address has. **It is sent three times, back to back, each sealed afresh**
(`net::leaving_copies`) - not through the reliable layer, since a client that
is going will not wait a round trip for an acknowledgement, and one datagram
sealed once and sent three times would be refused twice by the replay window.
The first copy to open lets the session go; the others reach an address with
no session and are refused, `BAD_HANDSHAKE`, which nobody reads.

**The server** lets a session go for a goodbye exactly as its timeout sweep
does - the same `let_go`, so `--on-leave` removes the aircraft or hands it to
the AI the same way - and says `let go ADDRESS after it said it was leaving,
T s after it was admitted`, on its own clock. With `--until-empty` it now says
when on its clock everybody had gone.

**The clients.** `glideslope_cli connect ... SECONDS` says goodbye at the end
of its stay (`--no-goodbye` leaves in silence, for a test of the timeout; a
`connect` with no SECONDS, the timeout test's client, still says nothing
after its handshake). `net::ClientSession` says goodbye as it goes - an
explicit `leave()`, and its destructor for every other way out - so the
client with the window says it when it quits, its flight ends, or anything
else ends its session, and prints `said goodbye to the server` on the way out
of its frame loop.

**Verification**, each seen to fail:
- `a_client_that_says_it_is_leaving_is_let_go_at_once`
  (`tests/cmake/server_goodbye.cmake`, `said`): a client stays 3 s and says
  goodbye to a server with a **two-minute** timeout, running `--until-empty`.
  It must be let go by its goodbye no sooner than its stay (no upper bound
  on the clock but the timeout: a slow runner may take its time over the
  forger's second session),
  and never "of silence". Here: 3.006 s after admission; the
  server stopped 3.0 s in. **Seen to fail** with the server ignoring
  `LEAVING`: let go of silence after the two minutes.
- `a_goodbye_from_another_address_or_session_lets_nobody_go` (`forged`): the
  client (`--forge-leaving`) makes a second session, then sends its own
  goodbye from the second session's address, the second session's goodbye
  from its own address, and its own goodbye from an address with no session -
  waiting for that one's refusal, reason 6, so all three are known to have
  arrived. Then it stays 3 s and both sessions say goodbye for real. Both
  must be let go by their own goodbye, no sooner than the stay: here
  3.018 and 3.011 s. A replay into a later session from the same address is the
  second case (a later session has its own keys); a copy within a session is
  the replay window's, held by its own tests. **Seen to fail** with a sealed
  datagram that does not open letting its address's session go: "a session
  was let go 0.002 s after it was admitted".
- `a_goodbye_is_the_one_byte_06_and_nothing_else_is`: every length 0 to 10
  and every first byte walked, 267 cases, one goodbye, found twice. Seen to
  fail with a longer `06...` taken for one. The kinds test is now six kinds,
  and the document test checks `LEAVING`'s row; `is_leaving` is the fuzzer's
  fifteenth parser.
- `a_player_leaving_has_their_aircraft_removed_or_flown_on_by_an_ai_as_the_session_says`
  now walks both `--on-leave` settings for both ways of leaving - four runs,
  counted - the goodbye with a one-minute timeout it cannot pass by. Seen to
  fail with `LEAVING` ignored.
- `the_client_with_the_window_joins_the_server_server_txt_names` runs the
  client with the window a second time with the server last, a two-minute
  timeout and `--until-empty`: the server must let it go for its goodbye
  (here 36.9 s after admission, which is the client's whole run on a
  software GPU). **Seen to fail**, in 167 s, with `ClientSession::leave` made
  to send nothing. The second run costs the test about 25 s.

- `a_client_written_from_the_transport_document_alone_completes_a_session`:
  the client written from `TRANSPORT.md` now says goodbye, three times, and
  its server runs `--until-empty` with a two-minute timeout, last in the
  pipeline so that its log is read (the client's report moved to standard
  error for that). The server must say it let the client go for its goodbye.
  **Seen to fail**, after 131 s, with the send left out.

**What it saves.** The multi-client tests that run their server
`--until-empty` no longer wait out its timeout after their last client has
gone - ten seconds for the seven on the server's default, three for the six
on `--timeout 3` - except where a client in them goes quiet on purpose. No
test's timeout was changed for this.
