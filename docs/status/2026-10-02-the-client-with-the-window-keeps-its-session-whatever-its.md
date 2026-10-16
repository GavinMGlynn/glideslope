### The client with the window keeps its session whatever its frame loop is doing, 2026-10-02 — tail done

**What is still not covered first.** The session is kept while the frame loop
is away, not the window: SDL's events are still read once a pass, so a long
build or a long pass still leaves the window unresponsive for as long as it
takes. The command-line client is unchanged - it builds nothing slow between
polls - and its half of a session is still written twice (its own tail). What
held the debug client silent for more than five seconds in PR #78's run was
not reproduced; the keeper covers every place the frame loop can be, so it
covers that too, whichever it was.

**What was wrong.** The window client polled its session once a pass of its
frame loop, in `Online::hear`. Two scoped `KeptAlive` threads covered two
known waits - from being given an aircraft to the first pass, and the shot's
wait for its terrain - and nothing else: a long pass, a flight built again at
a take-over of another aeroplane, a DEM tile fetched on a step, all left the
server hearing nothing. A pass longer than the server's `--timeout` was let go
(seen: the test below, with the keeper not polling).

**What it does now.** `client::Online` keeps its session from a thread of its
own, started with it and stopped first as it goes: whenever the frame loop has
not polled the session for `kept_after_s` (0.2 s), the keeper polls it every
50th of a second - the server's knocks answered, what must arrive
acknowledged, and the updates left waiting, oldest first, for the frame loop
to hear when it is back, as `KeptAlive` did - the newest five seconds of them
(`ClientSession::most_states_kept`, 128 at 25 a second): an absence longer
than that loses the oldest, and the flight is put right from what is left.
It touches the session and nothing else: the flight and everything `Online`
keeps of it are the frame loop's alone (`sim::Aircraft` is single-threaded),
and the session is behind a mutex every `Online` method that uses it holds.
**`hear()` lets go of it before it puts the flight right**: a correction
replays the inputs since over the collision DEM, which may fetch a tile, and a
replay in a sanitized build is long by itself; held across them, the lock
kept the keeper waiting and the session silent (found in review). No test
builds that case - a DEM fetch made slow in the middle of a correction needs a
fault put into the DEM's fetching for one test - and it is seen by reading:
nothing after the lock is let go in `hear()` touches the session. `fly()` takes in a join-again the keeper
heard before an input goes into the new session. `Online::session()`, a
reference the keeper could change under its reader, became `standing()`, a
copy. Both `KeptAlive`s are gone. The prediction starts as it did: the frame
loop's clock begins when the loop does, so a slow build owes no ticks. The
client says how long it was away while kept: once its flight is built, at the
shot, and at the end.

**Verification**, each seen to fail with the keeper measuring but not polling
(the server let the session go for silence, the client away 8.5 s and 3.3 s; and the one test, since review, red in its build case, away 9.8 s):
- **The build case** (then its own test, now a case of the one below):
  a server with `--timeout 2`; the client stands still six seconds after
  joining (`--slow-start 6`). It must say it was away past the 2 s building its
  flight (8.1 s here), and the server let nobody go for silence, admitted it
  once and let it go once, for its goodbye; it flew its aircraft by the pilot
  at the shot (tick 1200).
- **The frame case** (likewise):
  the same, with every pass held three seconds (`--slow-frames 3000`); away
  3.3 s since flying began, before the shot. This is the case the old code
  had nothing for: outside the shot's wait, nothing kept the session in the
  frame loop (the old code itself was not run against it - it does not say
  how long it was away, which the test requires).
- Every window-client, online, prediction, rejoin, slow-frames and session
  test (48, `-j4`) passed but one, the command-line copilot's
  `..._engine_stops_as_recorded` (refused reason 6 under load; it uses no
  window client), which passed run again alone.
- **The two cases are one test since review**,
  `the_client_with_the_window_away_past_the_servers_timeout_building_its_flight_or_in_a_frame_is_not_let_go`,
  run one after the other on ports 24772/24773: the first ports given,
  24746-24749, were already the copilot cases', and the tests' block has no
  two more pairs side by side. It asserts both cases ran.
  `every_fixed_test_port_lies_outside_the_ephemeral_ranges_and_no_two_tests_share_one`
  was run, and passes.
- **On Windows** (`tools/windows_build.sh`, MSVC, windows-debug, at 3cf65d4):
  it builds, and both tests pass (87 s and 91 s). At 8568a0b both tests, and
  the first Linux run of them, **timed out at ctest's 900 s** while they ran
  at once on one cold Cesium cache file - the two cases shared it; the
  stalled run's output was lost with it. Given a file each, and run at once
  from cold caches, they passed in about a minute here; the two cases now run
  one after the other, on one file. That two programs on one cold cache could stall that
  long, where `gfx::open_cesium_cache` is said to cost only a wait, was not
  investigated further: a possible tail. The ride-along, stall, forged-refusal
  and both command-line rejoin tests also passed there at
  8568a0b. **The one test, at cab3773**: timed out once at 900 s, run beside
  the port test, and passed run again alone in 312 s; each case run by hand
  there, outside ctest, took about 80 s, nearly all of it the shot's wait for
  its terrain (59 s and 61 s away at the longest). What stalled the timed-out
  run is not known - its output dies with it - and is the same open tail.
- After review, the targeted Linux suite (48 tests, `-j4`) passed whole.
- Ports 24772 and 24773 (the relay at PORT + 1).

- Ports 24746/24747 and 24748/24749 (the relay at PORT + 1).
