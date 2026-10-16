### Every fixed test port lies outside the ephemeral ranges, 2026-09-26 — tail done

**What is left out first.** The check walks the tests of the build it runs in,
so a test defined on one platform only is checked on that platform's CI job,
not on the others. Two tests name a port and listen on nothing, and are named
by the check as left out: `the_server_takes_every_flag_the_item_names_at_once`
(`--port 47999 --dry-run`) and `the_server_refuses_a_port_that_is_not_one`
(`--port 70000 --dry-run`). The server's own default port, 47801, is in
Linux's ephemeral range too; a server is started once, before its clients,
so it is not this race, and it is not changed here. The block assumes each
platform's default dynamic range: a Windows host whose range was widened to
start at 1024 has WinNAT reserve ports anywhere, this block included.

**Fixed ports still collide across working copies**: two worktrees running
the network tests at once on one machine ask for the same ports, and one of
them fails to listen. The block keeps a test's port from the system, not from
another checkout.

**Cause.** Every test that runs a server listened on a fixed port in 47852 to
47899. Linux hands out 32768 to 60999 to a socket that asks for no port, and
every client in these tests is such a socket, so on a busy runner a client of
one test could hold the port another test's server was about to listen on:
"cannot listen on port 47853" on CI, 2026-09-26. macOS and Windows hand out
49152 to 65535, which 478xx is below, so only Linux could lose this race.

**Now** every fixed test port comes from one block, **24700 to 24799**,
stated once in `tests/CMakeLists.txt` (`GLIDESLOPE_TEST_PORTS_FIRST` and
`_LAST`, with the reason): below 32768, the lowest ephemeral port of the three
platforms, and above the well-known ports. Each test kept its last two digits
(47853 is now 24753). The two relays, server_impaired's and
server_take_over's, still listen at `PORT + 1`. **server_leave used a second
port nobody had counted**: it ran its second server on `PORT + 1` by counting
`_port` up in its loop, which no check read. It now says `${PORT} + 1` for
that setting in so many words, and its 24765 is claimed.

**The check**, `every_fixed_test_port_lies_outside_the_ephemeral_ranges_and_no_two_tests_share_one`
(`tests/cmake/test_ports.cmake`), asks ctest itself for every test and its
command (`ctest --show-only=json-v1`), so there is no list kept by hand. What
it reads:
- **`-DPORT=N`** claims N. Any other `-D` whose name has PORT in it fails, as
  a port it does not read.
- **The script's set() and math() calls on `${PORT}`**: `set(V ${PORT})` is a
  second name for the port, and `math(EXPR V "${PORT} + K")` or
  `"K + ${PORT}"` claims N + K. Any other set() or math() on `${PORT}`, or on
  a variable made so - a second step, or a port counted up in a loop - fails
  as a port it cannot read.
- **The words a program is run with**: each argument of the command, and the
  words of an `ARGS` handed to expect_run.cmake. A word `--port` claims the
  word after it (or `--port=N`), unless a word is `--dry-run` exactly. A
  pattern (`-D..._MATCHES`) is not a command line and is not read, so a
  `--dry-run` inside one does not count.
It counts the tests given a port and, apart, the tests whose script uses
`${PORT}`, and fails when the two differ; then fails on any claim in
32768-65535, outside the block, or claimed twice, naming both tests. On
linux-debug: *walked 584 tests: 26 run a script that needs a port and 26 are
given one; 31 ports claimed, all in 24700-24799 and no two the same; 2 left
out as listening on nothing.*

**Seen to fail**, each put in and reverted: server_window's port put back to
47853 ("port 47853 is in an ephemeral range"); a take-over test's port moved
onto 24789, the rides-along test's ("is also taken by ..."); swap_wreck's
moved onto 24781, the 100 ms impaired test's relay (named with its
`${PORT} + 1`); server_collision's `-DPORT` removed ("25 tests are given a
port and 26 tests' scripts use one"); and one moved to 24690, below the block.
And each reading rule, the same way: a test run as `glideslope_server --port
47000` ("port 47000 is in an ephemeral range"); `--dry-run` taken from the
47999 test (47999 claimed, and in the range); `-DRELAY_PORT=24754` beside a
`-DPORT` ("names a port this check does not read"); server_impaired's relay
as `${PORT} - 1` ("derives a port this cannot read"), while `1 + ${PORT}`
passes; a `math(EXPR _relay2 "${_relay} + 1")` in server_take_over, and
server_leave's old `_port` counted up in its loop (each "cannot read"); and
the 70000 test's `--dry-run` moved from its ARGS into a `-DSTDOUT_MATCHES`
(70000 claimed, and in the range).

**Verified** on linux-debug: the 26 tests that listen on a fixed port, and
the check, run on their new ports with `ctest -j4`; 26 of 27 passed. The one
that did not, `a_player_takes_over_an_ai_aircraft_with_no_step_at_200_ms_and_a_players_is_refused`,
missed its prediction bound (10.559 m and 11.303 m against 10 m, in two of
four runs; it passed the other two), which a port number does not reach: it
listened, its clients joined, and it took over. The take-over tests are
RUN_SERIAL, so this is not load from the other tests; it is written into the
plan's tail "Over a network a client's own aircraft is corrected by metres"
as one more thing that tail's fix is to answer.
On Windows, `tools/windows_build.sh` built windows-debug with MSVC and ran the
check and six network tests on their new ports - a client hears where every
aircraft is, a repeated initiation, a gearstick client refused, the client
written from TRANSPORT.md, the dashboard's round trip, and the timeout: 7 of 7
passed.
