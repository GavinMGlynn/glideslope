### Knocking and believing a refusal are one piece for both clients, 2026-10-08 — narrows a tail, does not close it

**What is still two, first.** `glideslope_cli`'s `stay()` and
`net::ClientSession` still each do their own handshake, read their own
updates, stall their own way (`--stall-once-rolled`, `stall_until_let_go`)
and say their own goodbye: the client's half of a session is still written
twice for everything but joining again, knocking and believing a refusal.

**What changed.** `net/rejoin.hpp`, the piece both already used for joining
again, now holds the two rules before it: `net::lets_go` - a refusal lets a
session go only if it is the server's `BAD_HANDSHAKE`, from its address,
after `net::quiet_before_believing_s` (3 s) of nothing opening - and
`net::sealed_knock`, a knock's datagram. Both clients call them; neither has
its own copy of the constant, the test or the knock any more. Nothing on the
wire changed.

**Verification.**
`a_refusal_lets_a_session_go_only_from_the_server_for_no_session_after_three_quiet_seconds`
(unit): the refusal believed, and the six that differ from it by one thing -
2.9 s, another address, `SERVER_FULL`, `DROPPED`, a datagram that is none, a
byte too long - not; and a knock opens as a `PING` with its token. **Seen to
fail** with the quiet left out of `lets_go`: "a refusal is not believed after
2.9 quiet seconds"; reverted. The rejoin, stale-rejoin, forged-refusal,
stall and dropped tests of both clients pass through it (178 tests matching
the session's, the copilot's and the route's, linux-debug).
