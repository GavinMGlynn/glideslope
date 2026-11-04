### A test whose server cannot listen fails as that, and its relay ports are held too - 2026-11-04

**What was not true.** Only the spiral hand-over test checked its own
server's "cannot listen" before what its client heard. Every other test
that listens on a fixed port, its server unable to, failed on its client's
words - "no answer", a refusal from whatever held the port - or on its
server's exit code, and the relay ports a test derives from its port
(PORT + K) were not locked machine-wide.

**Now.** `tests/cmake/port_lock.cmake` gives every script two functions:

- `port_lock_hold(N)` takes port N's machine-wide lock as the include does
  for PORT; holding a port the process (or the script that ran it) already
  holds is nothing. GLIDESLOPE_PORT_LOCK_HELD is now a list. Every port a
  script derives from PORT is held straight after the math() that makes it:
  30 of 30.
- `port_listened(<streams>...)` fails the test, naming the port, if any
  stream says "cannot listen on port N" (the server's and the relay's words)
  or "could not listen on port N" (a script run by a script). Every call that
  runs a program on a held port is followed by it, before anything is judged:
  79 calls in 72 scripts, among them server_started_again.cmake, which
  client_server_gone.cmake runs, and deploy_systemd.cmake, whose server's
  standard error is a file. The spiral test's own check is now this.

test_ports.cmake asserts both, and counts: a call listens if it runs a
program with `--port ${V}`, a relay (`"${IMPAIR}" ${V}`), or a script handed
a server and the port, V being PORT or a port derived from it. Each such
call must be followed by `port_listened(...)`, handed that call's
`ERROR_VARIABLE`, before any `message(FATAL_ERROR` and before the next call
that listens; a script a script runs on its port is read too. A script that
uses a port and has no call listening on it fails unless it is named, with
its reason: two are, frame_terrain_imagery_refused.cmake and
imagery_stale_refused.cmake, whose PORT is one nobody listens on. It
prints: "30 of 30 ports derived from one held too; 72 scripts make 79 calls
that listen on a held port, 79 of them, in 72 scripts, checked by
port_listened before anything is judged".

**What it does not do.** A test fails as "cannot listen" only once its
client has given up: a client waiting on the server's ready file waits its
five minutes, and one connecting waits its own bound. Nothing fails it
sooner; the failure is named right, not made fast.

**Verification.**
- Every test given a port (124, and the download fixture they need) run
  with every port of the tests' block, 24700-24999, held by a bare UDP
  listener (a Python socket bound and never answering), linux-release, -j4,
  no display: **107 failed "a program could not listen on port N"**, in 2.5
  to 387 s - none timed out; those whose client waits on the server's ready
  file took its five minutes. Not failed so: 4 imagery tests, which listen
  on nothing (their port is one nobody listens on) and passed; the
  vertical-landing loading test's ALONE case, which runs no server and
  passed; 6 tests that ask a live model and the server's window test, which
  skipped before they run anything (no key, no display) - covered by
  test_ports.cmake alone; and three window-client tests that ran after the
  listener's two hours had run out, and so passed unheld: the hand-over,
  server-weather and NaN tests. The hand-over test, run again held (24744
  and its relay 24745), failed "could not listen on port 24744" ("impair:
  cannot listen on port 24745" with it) in 302 s. **Still to do**: the
  weather (client_in_server_weather.cmake) and NaN
  (client_let_go_of_a_nan.cmake) window tests run held; the item stays open
  until they are.
- Held one at a time: the dashboard test (server_ping.cmake) with 24758
  held failed "a program could not listen on port 24758"; with the check
  taken out it failed "the server stopped badly". The relay test
  (impair_last_words.cmake) with its relay port 24721 held failed "could not
  listen on port 24721" ("impair: cannot listen"); with the check taken out
  it failed on its client's "no answer". Both pass with nothing held, as does
  the take-over test at 100 ms, relay and all.
- test_ports.cmake seen red with port_listened taken out of server_ping.cmake
  and of server_started_again.cmake's first call, and port_lock_hold out of
  server_take_over.cmake ("72 scripts make 79 calls ... 70 scripts' 77 calls
  are checked"; "scripts derive 30 ports from PORT and hold 29 of them"),
  then restored.
- The lock test (`a_second_run_of_a_test_on_a_port_another_run_holds_...`)
  now has each party hold PORT + 1 too, the holder twice, and checks
  port_listened's four cases: the server's, the relay's and a nested
  script's words fail it, a server listening passes. Seen red with the relay
  lock let go at once in port_lock_hold ("waiter: took the relay's ... while
  the holder held it") and with port_listened reading only "cannot"
  ("port_listened passed 'the server could not listen on port 7'"), each
  reverted.
- Windows (tools/windows_build.sh, windows-debug): built; the coverage test
  and the lock test pass there. No server test was run held on Windows.
- The full linux-release suite was not run on this branch: the held run took
  the machine's ctest for two hours, and another agent was waiting on it.
  CI's run is the suite.

**Looked up first** (the owner's rule): CMake's file(LOCK) - locking a file
twice in one process is refused "File already locked", hence the held list;
ctest's RESOURCE_LOCK, which holds only within one ctest run, not across
builds; Windows' SO_EXCLUSIVEADDRUSE and SO_REUSEADDR, and macOS's UDP
SO_REUSEADDR behaviour - the server and relay bind INADDR_ANY with neither
option set, so a second bind on a held port fails on every platform rather
than sharing it.
