### The client with the window, let go, joins again by itself, and a dropped one does not, 2026-09-29 — tail done

*Going back is since tested, for both clients: see "A client refused by a
forger while its session is quiet goes back to it", 2026-10-01.*

**What is still not covered.** Nothing tests either client going back to its
old session when a forged refusal ends it during a blip; the code path is
there in both and reviewed, not exercised - an open tail in the plan, whose
test would forge a `BAD_HANDSHAKE` from the server's address while the
session is quiet (a relay holding the server's updates for three seconds),
and hold that the client goes back and the server admits it once. Nothing
tests the window client refused `DROPPED` when joining again after every one
of the server's goodbyes was lost (the command-line client's test does); the
path is the same `standing()` end - an open tail too, needing the drop's
three `LEAVING`s lost on the way, which the relay cannot yet pick out from
the updates around them. **Nothing tests `client::Online` taking an aircraft
from a server whose clock started again** (a server restarted with its key
through `--store`): the fix below is reviewed, and the session's half of it
is pinned by a unit test, but `Online` lives in the client with the window
and needs a `Flight`, which no unit test builds. A server that restarts
still forgets whom its operator dropped.

**What was wrong.** `glideslope_cli connect` joined again by itself (#45);
the client a person flies did not. `net::ClientSession` never noticed it had
been let go, and `client::Online` could not take a new aircraft at its old
number, so a window client let go for silence went on sealing under keys the
server had thrown away until it was restarted.

**What it does now.**
- **`net::ClientSession` keeps #45's rules itself**
  (`src/net/session.cpp`): a sealed `PING` of its own once a second while
  nothing has opened for a second; a `BAD_HANDSHAKE` believed only from the
  server's address after `quiet_before_believing_s` (3 s) of nothing
  opening; then a fresh `Initiator` with the same static key, resent every
  quarter of a second for a minute at most, from `poll()` - the frame loop
  never blocks on it. The old session's `Sealer` and `Unsealer` are kept, and
  anything opening under them takes it back (`went_back()`). Joined again, the
  reliable stream, the roster and its own aircraft start afresh.
  **`standing()`** says how it stands: `joined`, `joining_again`, or one of
  three ends after which nothing is sent - `dropped` (the server's sealed
  `LEAVING`, or `DROPPED` when joining again), `refused` (`SERVER_FULL`),
  `gave_up`.
- **`client::Online` takes the new aircraft as it takes a take-over**
  (`noticed()`): a session joined again forgets its own number, what it rode
  along in and what the server applied, and the next update naming an
  aircraft of its own - the old number or another - is handed to the caller
  through `taken_over()`, marked `again`. `main.cpp` adopts it into the
  flight (built afresh if another aeroplane), says "joined again: the server
  gave this client aircraft N", and predicts from there; nothing is blended
  from the old aircraft's place.
- **The operator's drop is respected**: `dropped`, `refused` or `gave_up`
  stops the window client with the reason ("dropped by the server's
  operator; not joining again") and exit 1, and no goodbye is sealed after.
- **The shot's wait for terrain keeps the session** (`KeptAlive`, as while
  the flight is built): on a cold cache the wait was longer than the
  server's three-second timeout, and the joined-again session was let go for
  silence too - seen on the first run of the test below.
- **Test flag** `--stall-after S`: S seconds of flight in, the session sends
  and answers nothing, reading and throwing away what comes, until nothing
  has come for three seconds; then it knocks once a second, and the server's
  refusal is the let-go, believed by the same rule as any other. The shot
  then waits (a minute past its tick at most) until the client has joined
  again and the server has applied an input sent since.
- **From the review of #50**:
  - Joined again, `client::Online` also forgets its newest word
    (`reconciled_s_`), the session's clock, where it drew everybody and the
    frame it drew them in: a server restarted with its key counts simulated
    time from nought, and every update of it would have been older than the
    last one heard, so the new aircraft was never adopted.
  - `net::ClientSession` hands up no update the old session left waiting
    once it has joined again (`fresh_` cleared with the rest); one would
    have been taken for the new aircraft.
  - `send_message()` returns false and queues nothing unless in a session:
    while joining again it went into a reliable stream the new session
    threw away. Messages the old session had not had acknowledged are lost
    with it.
  - A client ending while joining again says no goodbye, and
    `docs/TRANSPORT.md` now says so.
  - The test's skip is decided by the client's own "glideslope: no GPU
    device", not by any "device" or "could not" in the server's output on
    the same stream.
  - `a_session_joined_again_hands_up_nothing_of_the_one_let_go`
    (`tests/unit/test_session_over_a_socket.cpp`): a stand-in server over
    loopback lets a session go and answers it joining again; the client's
    clock is handed to `poll()`, so the three seconds are simulated. It must
    refuse a message while joining again, hand up no update of the old
    session, and hand up the new one's, from a clock started again at 0.5 s.
    **Seen to fail twice**: without `fresh_` cleared, "no update of the old
    session is handed up as the new one's"; with `send_message()` queuing,
    "a message asked for while joining again is refused, not queued"; both
    reverted.
- **Rebased onto #53 (one key, one aircraft)**: a session proves itself by
  sealing at once and knocking every `prove_every_s` until anything opens;
  a session joined again now does the same (`prove_at_once()`), since the
  server sends an unproven session nothing. Past proving, the knock after a
  second of quiet stands. So
  `a_client_whose_first_sealed_datagram_is_lost_still_proves_its_session`
  counts "stopped proving" over the 0.9 s before a quiet knock is due, and
  `a_session_joined_again_hands_up_nothing_of_the_one_let_go` also holds
  that a session joined again, its first sealed datagram lost, seals another
  within 0.9 s of its clock - **seen to fail** with the session joined
  again not proving itself: "the new session sealed something at once";
  reverted. A server's `LEAVING` now means dropped *or*
  taken over by a newer session for this key, so the window client says
  what the command-line one says: "the server ended this session (dropped,
  or taken over by a newer session for this key); not joining again".
- **Docs**: `docs/TRANSPORT.md` now says both clients knock and join again;
  `docs/THREATS.md` names where each client keeps the refusal rule. The wire
  did not change.

**Verification.**
`the_client_with_the_window_stalled_past_the_timeout_joins_again_by_itself`
(`tests/cmake/client_joins_again.cmake`, port 24703 and its relay 24704;
headless): a server with `--timeout 3`, the window client stalling two
seconds in, and a command-line client keeping the server up until the shot
exists. The window client must say it was let go after at least 3 s of
nothing, that it joined again and was given an aircraft, and at the shot that
it flies that aircraft and the server has applied its inputs sent since
joining again; the server must admit it twice, let one session go for
silence and two for goodbyes, and drop no copy of an initiation. It came back
as aircraft 0, its old number. 69 s here from a cold Cesium cache (Linux
debug). **Seen to fail**: with the refusal never believed, "the client did
not notice it was let go"; reverted. **Not on macOS**: both tests fly on
Vulkan, which macOS does not have, and CI's macOS runs failed them because
the client is given its aircraft before it opens the GPU, so the other checks
ran before the skip. The skip now comes first, and the two tests are
registered only where the platform has Vulkan (Linux and Windows).

`the_client_with_the_window_dropped_by_the_operator_says_so_and_does_not_join_again`
(same script with `DROP`, port 24705 and 24706): the server's
`--drop-once-flown` drops it; it must say it was dropped and not joining
again, never say it is joining again, exit 1, and be admitted once. 3.6 s
here. **Seen to fail**: with the server's `LEAVING` ignored, "the dropped
client did not say it was dropped"; reverted.
