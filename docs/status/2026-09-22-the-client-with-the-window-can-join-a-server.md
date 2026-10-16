### The client with the window can join a server, 2026-09-22

**What is still missing: it joins and then flies alone.** The session is kept
- the server's knocking is answered, its state updates are read - and nothing
is drawn from them. The aircraft on screen are still only this client's own.
That is the item below this one.

**The session lives in `src/net/` now.** It was written inside
`glideslope_cli`, which is why the client with the window could not connect to
anything at all: the code existed and was in the wrong place. `net::ClientSession`
owns its socket, does the handshake with retransmission, answers the pings
and reads the state updates, and both frontends use the one implementation.

**Both clients reach the server `server.txt` names**, and there is a test for
each. The command-line one needs no GPU and runs everywhere; the one with the
window draws, so it skips where there is no driver. Both matter - for most of
this project's life `--online` worked only in the client nobody flies.

**A day and a half of hung processes.** Five `glideslope --terrain ion` runs
were still alive from a previous session, each inside a `timeout -s TERM` of
four to eight minutes that had fired long ago. They hold the GPU and the
Cesium cache, and they are what the "database is locked" flake is: a run
meeting a process from another day. The TERM is sent and ignored. Recorded as
a tail with its own verification.
