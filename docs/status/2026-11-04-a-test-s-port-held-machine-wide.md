### A test's fixed port held machine-wide: the spiral hand-over test's start-up failure was another working copy's run of it, 2026-11-04

**What was not true.** The spiral hand-over test
(`an_aircraft_its_player_rolled_into_a_spiral_over_two_ai_layers_handed_to_the_ai_is_rolled_level_and_never_loses_separation_from_either`)
failed 2.47 s into a full linux-release suite on 2026-11-04 and passed alone.
Its log said `the client never heard its aircraft handed to the AI: refused,
reason 6` - BAD_HANDSHAKE, which a server says only to an initiation made
for another server's key.

**The cause.** Another agent, in another working copy, ran a ctest whose
pattern (`roll|...`) took in the same test, at 21:28-21:42 UTC on 2026-11-03;
the suite's run of it fell between 21:26:49 and 21:36:58. Both used the
test's fixed port, 24940. The second server could not listen ("cannot listen
on port 24940"), and its client's initiation reached the first run's server,
whose key is another: refused at once. test_ports.cmake keeps the ports of one
build's tests apart; two builds on one machine are the same tests on the same
ports, and nothing kept those apart. CI runs one suite per runner and never
meets it.

**Now.** `tests/cmake/port_lock.cmake` takes a lock for the test's port in the
system's temporary directory - TEMP, else TMP, else /tmp, failing loudly if
none is a directory - held by the
cmake -P process until it ends; a second run on the port waits for it, and
says so. A script the holder runs on its own port (client_server_gone.cmake
runs server_started_again.cmake) does not wait on its parent: the holder
names its port in `GLIDESLOPE_PORT_LOCK_HELD`. Every one of the 74 scripts
that uses `${PORT}` includes it before it runs anything, and
test_ports.cmake checks that of every test given a port (124 of 124). The
spiral test's failure there now prints the server's own words too.
**What the lock costs, and what it does not hold.** A run that waits for
the lock waits inside its own test, so the wait counts against that test's
TIMEOUT: a run behind a 125 s test has that much less of its own. And the
lock is the cmake -P process's: if that process is killed and its server is
not, the port is let go while a server still listens on it. Checked on Linux
(ctest 3.31.8): a test holding port 23945, its server listening (its ready
file written), killed by a 3 s TIMEOUT, left no process on 23945, and a new
server listened there at once - ctest killed the server with the script. A
script killed some other way, by its process alone, would leave its server.
The spiral test now checks its own server before the client's words: a
server that "cannot listen" fails it as that, and a server that ended other
than 0 fails it. Seen red with a bare UDP listener on its port ("the server
could not listen on port 23946"), and green after on that port.
Two more: the lock directory is a machine's only if every run sees the same
TEMP and TMP; and a port one test derives (a relay at PORT + K) is not locked
apart, so two working copies whose port numbering differs could still meet on
one; within a version, two runs of a test are kept apart by its PORT.

**Verification.**
- Reproduced: the original test run while another run's server held its port
  (a holder taking the lock and running a server with its own key for 10 s),
  four at a time on four ports alongside six busy cores: **100 of 100 failed**,
  each in 2.5-2.6 s with `refused, reason 6` - the failure as seen.
- Fixed: the same, with the test as it is now: **100 of 100 passed**, each
  having said it waited for the port, then flown its 14,400 steps with
  separation never lost: 135.0-137.7 s a run, the holder's 10 s among them.
- `a_second_run_of_a_test_on_a_port_another_run_holds_waits_for_it_and_a_script_the_holder_runs_does_not`
  builds the situation: a holder takes the lock, a waiter is refused it
  without waiting, the holder's nested script takes it at once, and the
  waiter has it only after the holder is done. Seen red with the lock taken
  out of port_lock.cmake ("waiter: took ... while the holder held it") and
  with the nested guard taken out (the nested script waits on its parent: a
  hang, which the test's 300 s TIMEOUT ends), each on a scratch copy.
- test_ports.cmake seen red with the include taken out of server_ping.cmake
  ("does not hold it") and moved after its first execute_process ("runs a
  program before it holds its port"), then restored.
- Windows (tools/windows_build.sh, windows-debug): the lock test and
  test_ports.cmake pass. linux-release: eight server tests on fixed ports,
  among them a second session on one key and a player leaving, pass with
  their ports held.
