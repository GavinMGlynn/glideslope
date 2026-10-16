### A copy of a handshake read after its session has gone makes no second player, 2026-09-26 — tail still open

**What is not done first.** Why the fourth client's first session went quiet
for three seconds on CI is not known: the client had its answer, and had read
the server's introductions under it, yet the server read nothing from it. The
server reads one datagram per pass, which delays what a client sends, but
pinning the four-player test to one CPU here and stopping the server for 30 s
after it bound its socket did not reproduce it. The hundred Windows debug runs
the tail asks for have not been done, and `tools/windows_build.sh` was not run
(the Windows working copy was in use); so the tail stays open. **One key on two
addresses still flies two aircraft** - a new tail, not this one's cause. The
same goes for a client whose NAT rebinds its port between resends. **A copy of
an initiation from a different address is still answered**, on purpose (see
the review below), so a replayer can still make aircraft nobody flies until
`--timeout`, one per spoofed address, while a slot is free.

**Found by CI**: the four-player test counted six players' aircraft on Linux
debug (run 36140964489) and five on Windows debug (run 36132849221, first
attempt). Both logs say the same: the fourth client, from **one address**, was
admitted to the session three times (twice on Windows). The first two sessions
were each let go after 3.0 s of silence, their aircraft barely banked, and the
client flew none of them. It was not a client sending a new initiation: the
client sends the same bytes every time (`Noise_IK` makes one), and a repeat of
those bytes to a live session gets that session's answer.

**The cause.** A copy of the initiation read *after* the session it made had
been let go. The session table is keyed by address, so with the session gone
the copy was a stranger's handshake. The server made a new session and gave it
a new aircraft. It sent an answer the client ignored, because the client
already held the keys of the first. Nothing the client sealed opened under the
new keys. So the server heard nothing, let the session go after `--timeout`,
and printed a player's aircraft nobody had flown. Every copy still to be read
did the same.

**Found by CI before the merge.** A client dropped by the operator goes on
sending its initiation, and the server said every copy it dropped. On a slow
runner that filled the dashboard's log with them until the drop itself was
pushed out, and the server window's test failed. Each dropped initiation is
now said once. The test's client sends its copy five times, as such a client
does; with every copy said, it fails: "dropped a copy 5 times, not once". Its
two tests were also on ports another test uses (47888 and 47889); they are on
47895 and 47897.

**The fix.** The server remembers every initiation that has made a session,
by its first 32 bytes (the initiator's ephemeral key) and the address it came
from (`Taken` in `src/frontend/server/main.cpp`). A copy from that same
address is dropped, whether its session is live or gone, and the headless
server prints "dropped a copy of an initiation already taken from ADDRESS".
From any other address a copy is answered as it always was. A live session's
exact repeat still gets the same answer as before. A client that starts again
makes a new ephemeral key, and its initiation is taken as ever. The memory is
a flat ring of the newest 16,384 keys (80 bytes each) and a `std::set` of the
same keys (a node of about 112 bytes each): about 3 MiB when full.
`docs/TRANSPORT.md` and `docs/THREATS.md` say so, and say what it does not
claim.

**The review of PR #25 found the first version keyed on the ephemeral key
alone.** A copy dropped from every address let anybody who saw a player's
initiation inject a byte-for-byte copy from a spoofed address to arrive first.
It was taken and remembered, and the player's own initiation and every resend
were dropped: no session at all, where before the player had one of its own.
Keying on the address as well fixes that. The review also found the memory
figure wrong (2 MiB for 65,536 entries was about a third of the real cost). It
found that the first test could pass without the server ever reading the
copy, since a copy lost or a server gone also leaves it unanswered. And it
found that `docs/TRANSPORT.md` said a response that does not complete the
handshake is dropped while the client waits on, where this project's clients
give up. All four are fixed.

**The first guess, a player per key with the newest session
winning, would not have fixed this.** Each ghost here was made after the one
before it had gone, so there was never a second live session for the key to
take over. It is still a real hole, for a client that restarts on a new port.
It is the new tail.

**Verified**:
`a_copy_of_an_initiation_arriving_after_its_session_was_let_go_makes_no_second_player`
(tests/cmake/server_initiation_again.cmake) builds the situation. A client
(`glideslope_cli connect --again-when-let-go`) completes its handshake and says
nothing more. When the server's once-a-second knocks have stopped for 3 s, its
session is gone, and it sends the same initiation once more from the same
address. A second client flies until the first has written its verdict
(`--until-exists FILE`, an event rather than a fixed 20 s). The test asserts:

- the server says it dropped the copy, exactly once;
- the copy is not answered;
- the silent client is admitted once;
- two sessions are let go, and two players' aircraft are counted, one of them
  flown past 90 degrees.

An answer the same as the first means the copy reached a session still live,
and the test fails as "the situation was not built", rather than passing for
the wrong reason. **Seen to fail:** with the check that drops a taken
initiation disabled, the test failed with "the server took a copy of an
initiation it had already taken for a new handshake". The server's log showed
CI's pattern exactly: `admitted 11fc7622` twice, each session let go "after
3.0 s of silence", and a player's aircraft banked 9 degrees counted twice. The
check was then put back. With the drop made but not said, it failed with "the
server said it dropped a copy 0 times, not once", and that was put back too.

`a_copy_of_an_initiation_from_another_address_arriving_first_does_not_keep_its_client_out`
(tests/cmake/server_initiation_elsewhere.cmake) builds the review's race. The
client (`--first-from-elsewhere`) sends its initiation from a second socket and
waits for that copy to be answered. Only then does it send it from its own
address, and fly from there. The test asserts:

- the key is admitted twice, once for each address;
- no copy is said to be dropped;
- two players' aircraft are counted, and exactly one is banked past 90
  degrees, the one flown from the client's own address.

**Seen to fail** against the first version, keyed on the ephemeral key alone:
"the key was admitted 1 times, not twice". The client never got a session from
its own address, and the only aircraft, the copy's, banked 9 degrees.
