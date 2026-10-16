### A hole found by re-reading the threat document, 2026-09-22

**What is missing first: nothing a client sends drives an aircraft.** A client
connects, holds a slot, answers the server's knocking and is drawn on the
dashboard. It cannot fly.

**A second handshake initiation used to take a live session away.**
`connections[who] = std::move(c)` was unconditional, so any datagram that the
server could answer gave that address fresh keys - and **anybody can make a
valid `IK` initiation, because it only needs the server's public key**. Replay
a captured one with a player's source address and that player is off the
server, in silence, for the price of one packet. It bit honest clients too:
the client resends its initiation every 250 ms until answered, so two answers
in flight left the client on the first set of keys and the server on the
second.

**It was found by making `THREATS.md` true again, not by a test.** The document
had been written when the server accepted nothing; bringing it up to the code
meant reading `take()` line by line against what the document claimed, and the
claim "there is no per-peer connection table" turned into "there is one, and
here is what it does wrong". Three more came out of the same reading: a
displaced key was never released from `Slots`, so four `connect` runs from one
address filled a four-player server for good; the table is keyed by address, so
a replayed initiation from many spoofed addresses costs an X25519 apiece; and
`SERVER_FULL` and `BAD_HANDSHAKE` go to senders that have not authenticated.
The first two are fixed below. The third and fourth are written down and not
defended - there is no rate limit of any kind.

**The fix is one session per address.** The initiation that made a session is
kept beside it: the same bytes get the same answer back, which is what an
honest client whose answer was lost needs; different bytes are dropped without
a word, so a stranger cannot rekey somebody's session. A slot goes back only
when no connection is left on that key, because two addresses may share one.

**Held by a test that was watched failing.**
`a_repeated_handshake_initiation_does_not_take_a_live_session_away` runs a real
client and a real server at once - the client completes a session, sends its
initiation again, and must still be answering pings three seconds later.
Against the code as it was it fails with "0 slots have a client on them after
one client connected twice".

**What it cost to find is worth saying.** The subagent that re-read the
document found it; the hole was in code written earlier the same day, by me,
and no test I wrote for that code would ever have caught it, because every one
of them had a well-behaved client in it.
