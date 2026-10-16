### `THREATS.md`, 2026-09-22 — item done

**It leads with the fact that the server accepts nothing yet.** There is no
handshake and no sealing, so `glideslope_server` binds a port, counts what
arrives and drops every datagram unread. Every defence in the document is
marked built, unwired or not built, so that none of it reads as protection
that is presently doing anything: the envelope reader is built but **unwired**
- `read_envelope` is called by its test and by nothing else in `src/`.

**It names all four datagram types and all seven reliable messages** with the
defence each has, citing the limits the code enforces rather than describing
them in general terms, and says which of the seven a server would accept from
a client at all - only `CONTROLLER_SWAP`; the rest are server-to-client, which
materially narrows the surface. Amplification, replay, an unauthenticated
datagram, exhaustion of the reliable layer's queues, a client claiming a slot
or a player count, and a wrong terrain dataset each have a section.

**Writing it found six disagreements between the code and the documents**,
which is what writing it was for. Four were fixed the same day: the oversized
`WEATHER` above; a turbulence severity byte free to carry anything when the
flag before it said there was none; a slot index with no range check in three
messages; and a server receive buffer of 1500 bytes where a datagram is 1232.
Two were not, and are items in `COMPLETION_PLAN.md`: the reliable layer
believing a forged acknowledgement, and no message rejecting a NaN.

**Verified**, `every_message_the_server_accepts_is_named_in_the_threats_document`:
every message kind the code knows is named in the document, and the count is
held against the code's own list so a new kind cannot be added without being
named in both.
