### A session, over a socket, 2026-09-22

**What is missing first: a session does nothing yet.** A client connects, is
given a slot and can seal to the server, and nothing is done with what it
seals - no input reaches an aircraft and no state goes back. So a client can
connect and be counted, and cannot fly.

**But it connects.** Run by hand: `glideslope_server` printed
`server key bedf297d...`, `glideslope_cli connect 127.0.0.1:47900 <key>`
answered `session with bedf297d...` and sealed 35 bytes, and the server
logged `admitted 1f141fb4 to slot 0`. Held automatically by
`a_client_and_a_server_complete_a_session_over_a_socket`, which puts a
102-byte initiation and a 54-byte answer through the loopback in the
envelopes they really travel in and seals both ways under different keys -
every other test of the transport hands bytes from one object to another, and
proves nothing about a wire that was never used.

**Sealing.** A `SEALED` body is its sequence number and ChaCha20-Poly1305
ciphertext. The number is the nonce and the additional data, so changing it
only stops the body opening. A replay window of 64 refuses anything already
opened and anything further behind than that, **and the limit is stated**: a
datagram delayed by more than the window cannot be told from a replay. Only a
body that opens moves the window, so something that is not ours cannot push it
forward. Held over **all 720 orders of arrival** of six datagrams, each
delivered once and twice, every one opening exactly once; 7,650 one-byte
changes and 30 truncations refused. Both replay tests were watched failing
with the window taken out.

**Every one of the server's flags drives something now.** `--key` takes a
static key or mints one, and prints the public half because a client cannot
begin an `IK` handshake without it; `--timeout` lets go a client that has gone
quiet and gives its slot back; `--players` bounds who gets in, and a fifth is
refused with `SERVER_FULL`; and `--store` is below.

**A datagram that does not open is dropped without a word.** A refusal would
tell a forger that the address was right.

**A line that had gone stale under me.** The settings block still printed "this
build cannot mint one" of `--key` - true before libsodium and false after. Left
alone it would have been a small lie at every startup.

**And the same name collision twice.** The handshake's result was called
`Session`, which is already the `SESSION` message's structure. It is
`SessionKeys` now. The compiler caught it, as it caught `Slots` earlier.
