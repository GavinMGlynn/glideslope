### A player started again on a full server flies again at once, 2026-10-02 — tail done

**What is still not covered first.** A player started again *while somebody
floods the server* with initiations may still be refused: a full server reads
at most 32 initiations a second, and one past that is refused `SERVER_FULL`
unread, so that player waits out the old session's `--timeout` as every
restarting player did before. A forger who can time a refusal to tens of
microseconds can tell whether a public key it already has is a player's (one
X25519 operation for a stranger's key, two for a player's); the refusal
itself is the same. The client-side ghost of PR #78 (a client going back to a
let-go session, leaving its fresh initiation admitted) is untouched by this,
and no worse: a key whose session was let go is not in a full server's slots,
so its initiation takes the path it always did.

**What was wrong.** `take()` refused `SERVER_FULL` before constructing a
`Responder` whenever the session was full - sparing strangers the asymmetric
work, and sparing the player whose client was started again from a new port
nothing: on a server of one player, that player could not fly at all until
the old session's `--timeout` let it go.

**What it does now.**
- **`Responder::only_for`** (`src/net/handshake.*`): asked about the
  initiator's static key as soon as it is unsealed under `es`, the first
  X25519 operation, and before `ss` and the answer's three. A key not wanted
  goes no further (`unwanted()`); `x25519_done()` counts what reading the
  last initiation cost - 1 for a key not wanted, 2 for a wanted key whose
  initiation does not complete, 5 for one answered.
- **A full server reads on only for its players' keys**, and at most
  `full_server_reads_per_second` (32) times a second - `net::Budget`, a token
  bucket of a second's depth, in `src/net/budget.hpp`; past it an initiation
  is refused unread. Everything a full server does not answer is refused
  `SERVER_FULL`, never `BAD_HANDSHAKE`, so the refusal says nothing about
  whether a claimed key is a player's. A player's key is answered and the
  existing take-over does the rest: the new session shares the key's slot and
  aircraft, and takes over at the first thing sealed under it that opens;
  the old session is sent `LEAVING` and let go, so nothing is left behind.
- **`glideslope_cli connect`** also writes to `--heard` which steps of the
  simulation it was told of, and how many of its inputs the server applied,
  so that a test counts in steps across two clients.
- `docs/TRANSPORT.md` ("Refusals", "Starting a session") and
  `docs/THREATS.md` (`HANDSHAKE_INITIATION`, the reliable layer's peers, an
  unauthenticated datagram) say what a full server now does, and what a flood
  or a forger can make it do. Nothing on the wire changed; the version stays
  `02`.

**Verification**, each seen to fail with the bug named:
- `a_player_started_again_on_a_full_server_flies_again_at_once`: a server of
  one player and a one-minute timeout (7,200 steps); the player's client
  stops without a goodbye, and the same key starts again from a new port.
  The key admitted twice, one take-over, no session let go for silence, the
  new client's inputs applied, one player's aircraft, and the server ending
  when the new client leaves (no ghost). The player was off the server **10
  steps** here (the first client's last update to the second's first). Red,
  with a full server reading nothing as before: the second refused, the
  server letting the old session go "of silence" after the minute.
- `a_stranger_on_a_full_server_is_refused_after_reading_no_further_than_its_key`:
  a server of one player, a stranger with another key refused reason 5, the
  server saying it read the key and no further at one X25519 operation, the
  player admitted once and never let go. Needs no network. Red with
  `only_for`'s check disabled.
- `a_full_server_reads_a_strangers_initiation_no_further_than_its_key`
  (unit): the stranger at 1 operation and `unwanted`, a forger claiming the
  player's key at 2, the player answered at 5 and completing, and without
  `only_for` the stranger answered at 5. Red with the check disabled.
- `a_full_server_reads_at_most_its_budget_of_initiations_a_second` (unit): a
  thousand at once read 32 times, half a second later 16, a clock going
  back a second and forward again to where it was nothing, an hour idle 32.
  Red with the cap on the refill removed, and (after review) with a refill on
  the size of the step either way.
- **On Windows** (`tools/windows_build.sh`, MSVC, windows-debug, at f690159):
  the server, the CLI and the tests build, and all four tests above pass.
  A first attempt was not a result: another session switched the shared
  Windows working copy to its own branch mid-run, so the build held none of
  these tests; it was run again with the copy idle.
