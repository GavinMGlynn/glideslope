### A server behind real time hears every client, 2026-09-26 — main made green

**What is missing first: a client the server has let go still cannot come
back**, and a client still cannot say it is leaving: two tails.

**Main went red** on the four-player test, on Ubuntu linux-debug: "the
player's aircraft number 3 banked only 2 degrees", on `main` itself and on
three pull requests in a row. The server's log said the fourth client was
admitted, then let go after 3.0 s of silence, and its handshake, sent again,
was dropped as a copy already taken (the handshake fix, #25). Before #25 that
copy made a second session, and the test counted five aircraft instead - the
tail's own "five players' aircraft".

**The cause, found by review: the server read one datagram a pass.** A
server behind real time takes four steps between looks at its socket; on CI
it was 4,000 to 5,000 steps behind, about 27 looks a second, while four
flying clients send about 120 datagrams a second. The rest waited in its
socket or were dropped by the kernel, and a client still joining, sending
four copies of its handshake a second, went unheard for three seconds. (A
first guess, a client starved of the processor, is not what the log shows.)
The command-line client had the same fault and was fixed the same way.

**Now**:
- The server reads every datagram waiting on each pass, up to 512, so that a
  flood cannot hold its steps up.
- `--test-step-ms MS` makes every step take at least that long, so that a
  test can put the server behind real time on any machine.
- `a_server_behind_real_time_hears_every_client_and_lets_none_go_that_is_joining`
  builds it: the four-player case with every step taking 30 ms, a quarter
  of real time, against a three-second timeout. It fails unless the server
  says it was at least 240 steps behind, all four players flew past 90
  degrees, and no handshake was sent again after its session was let go.
  With the drain it passed three runs in three, some 1,400 steps behind.
  **Seen to fail** with one datagram a pass put back: two runs in two, three
  clients let go and a player's aircraft banked only 11 and 17 degrees, as on
  CI. At 12 ms a step (900 behind) one a pass still passed: the socket must
  fill before anything is lost.
  The clients fly twenty seconds, not the four-player test's eight: at a
  quarter of real time eight gave a macOS runner two seconds of flight, and
  an aileron held full rolled its aeroplane only 72 and 75 degrees. With
  twenty, one datagram a pass still fails (16 degrees).
- Seven multi-client tests whose subject is not the timeout
  (`server_slots`, `server_fly`, `server_collision`, `server_impaired`,
  `server_late`, `server_swap_wreck`, `server_window`) no longer give the
  server `--timeout 3`; they use its own ten seconds, some seven seconds
  longer each with `--until-empty`, about a minute across the four shards.
  Kept short on purpose: the two windowed clients that stand still five
  seconds to prove the keep-alive holds, the two handshake-copy tests, the
  new test, and the timeout's own. `server_leave` (a one-second timeout)
  stays exposed to a server far behind until the let-go client can rejoin.
