### A client joining again goes back only on its old session's answer to its own knock: no ghost from held updates, 2026-10-06 — fixes a tail

**What is still wrong first.** The client's half of a session is still written
twice: `glideslope_cli`'s `stay()` and `net::ClientSession` are separate code.
Only joining again is one piece now (`net::Rejoin`, below), which both use; the
rest of the session - knocking, believing a refusal, reading updates - is still
two. Its tail stays open.

**What was wrong.** A client joining again kept its old session's keys and went
back to the old session on *anything* that opened under them, and an update the
server sealed before it let the session go opens as well as a live one. So a
client whose last updates were held on the way went back to a dead session
while the server admitted its new initiation: a ghost session holding a slot
and an aircraft until its timeout, and the client lost for about 13 s
(seen 2026-10-01 in the window client's forged-refusal test's first form).

**What it does now.**
- **`net::Rejoin`** (`src/net/rejoin.hpp`) is joining again, used by both
  clients: `net::ClientSession::keep_joining_again()` and `glideslope_cli`'s
  `join_again()`. With each initiation (every quarter of a second) it sends a
  sealed `PING` under the old session's keys carrying a token drawn at random
  for this attempt (`randombytes_buf`), and goes back only on the `PONG`
  carrying it. Anything else that opens under the old keys is counted
  (`stale()`) and dropped. `net::refusal_from` moved there from both clients.
- `glideslope_cli` says "while joining again, N opened under the old session
  that were not its answer"; `ClientSession::stale_while_joining_again()`.
- `--leave-once-back` also waits for a server knock answered in the stay it
  is back in: back in the old session, the held updates (and knocks) are no
  longer handed to that stay, and its first update could already carry an
  applied input, so it left with `answered 0 pings` and failed its own
  "never in a session" rule. The server knocks once a second.
- **The relay** (`glideslope_impair --hold-until-let-go-after N`): after N
  datagrams from the server it holds what the server sends and drops what the
  client sends, replaying to the server every quarter of a second the last
  sealed datagram the client sent before the hold - one it has opened, so a
  replay, dropped in silence while it has the session and refused
  `BAD_HANDSHAKE` once it has let it go. That refusal is the event: from it
  refusals pass and the client's datagrams go through, so the client hears the
  server's own refusal. The client's initiation ends the hold and the held
  updates are delivered first.
- `docs/TRANSPORT.md` ("Joining again") and `docs/THREATS.md` say so; no
  byte on the wire changed (a `PING` token was always the sender's choice), so
  no version bump.
- The test port block is widened from 24700-24799 to **24700-24899**: the
  hundred had two ports left.

**Verification.**
`a_client_let_go_with_its_last_updates_held_joins_again_without_going_back`
(`tests/cmake/server_stale_rejoin.cmake`, port 24800, relay 24801): a staying
client straight to the server (so `--until-empty` does not stop it at the
let-go) | server `--timeout 3` | relay `--hold-until-let-go-after 50` |
`glideslope_cli connect --fly --leave-once-back`. It holds that the relay saw
the server let the session go and the hold ended on the initiation with
updates held; the client believed the refusal, did not go back, said held
updates opened under the old session (81 here), and joined again; the server
admitted it twice (first, and joining again), let one session go for silence
(not a ghost's second) and two for goodbyes. 7.9 s (Linux debug). **Seen to
fail** with `Rejoin` going back on anything that opens: "the client went back
to a session the server had let go"; reverted.
`joining_again_goes_back_only_on_the_old_sessions_answer_to_its_own_knock`
(unit): `due()` sends the initiation and a knock carrying the attempt's token,
a quarter of a second apart; an update and a `PONG` to another token are
nothing (counted stale), garbage is nothing, the `PONG` to its token is the
old session answering; a second attempt's token differs, and the server's
answer to its initiation is a new session with the server's keys. **Seen to
fail** with the same bug ("an update under the old session is not a reason to
go back"); reverted. `a_session_joined_again_hands_up_nothing_of_the_one_let_go`
now reads past the knock that goes with the initiation. The forged-refusal
tests for both clients, the stall-and-join-again tests and the session tests
pass unchanged otherwise.
