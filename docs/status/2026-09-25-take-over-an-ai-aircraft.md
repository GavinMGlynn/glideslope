### Take over an AI aircraft, 2026-09-25 — item done

**What is missing first.**
- **The aircraft left keeps flying where it was, as the AI's**, holding what
  it was doing. It is not sent back onto a plan.
- **Nothing limits how often a client may ask**, as with a swap
  (`THREATS.md`).
- **The client with the window does not hand its own aircraft over online**:
  that is still a tail, and taking over does not change it.
- **Taking over an aircraft of another type** rebuilds the client's flight as
  that type, keeping its input sequence and checklist. No test flies it,
  because a server flies one type for its players and its AI alike (choosing
  an aeroplane is not built).
- **Two refusals are not built in a test**: a wreck, and an aircraft nobody
  is flying. Each is one line of `Fleet::take_over` beside the two that are.

**What works.**
- **`CONTROLLER_SWAP` to `PERSON` for an aircraft the AI is flying**, not the
  sender's own, is a take-over (`Fleet::take_over`). The sender's slot moves to
  that aircraft, and its inputs fly it from then on. The aircraft it had goes
  to the AI under a new number. Both are announced to every client.
- **Aircraft numbers are used again, lowest first** (`free_number`). They
  were counted up, and a client taking over back and forth could have run
  the count round into the players' numbers and `no_aircraft`: found by
  review.
- **Refused**, acknowledged and nothing more: another player's aircraft, one
  the AI is not flying, a wreck, and anything at all on a server started with
  `--no-take-over`. `TRANSPORT.md` and `THREATS.md` say so.
- **A client learns it has taken over** from its state update, which names the
  new aircraft as its own and carries its motion. It predicts from there. The
  change from what it showed is blended like any change of source.
- **The client with the window**: riding along, `T` takes the aircraft over,
  if the AI is flying it (otherwise it says so and asks nothing), and
  `--take-over-after S` does it S seconds of flight in. The flight becomes
  that aircraft, rebuilt if it is another model. The HUD then says
  `FLYING PILOT`, and the camera is in its own seat again.
- **`glideslope_cli connect --take-over-at S`** does the same, for the network
  checks. `--take-over-aircraft N` asks for a particular one, and
  `--once-the-ai-flies-it` waits until an update shows the AI flying it.
- **Corrections small enough to hide are blended** in what the client shows,
  as a change of source already was.

**Found on Windows before the merge: the step measured the frames, and the
display stopped for one.** The 200 ms take-over failed one run in three or
four on Windows debug, stepping 5.1 to 6.4 m against its 5 m bound, where
Linux drew 1.3 to 1.8 m. Four things, each fixed:
- **The step was a second difference frame by frame**, which counts uneven
  frames as steps: 60 m/s drawn 10 ms and then 100 ms apart is 5.4 m. It is
  now the distance from where the frames before carry the aircraft, each at
  its own time.
- **A blend started from the last frame shown**, which held the aircraft
  still for one frame: a step as big as its speed times the time between
  frames, at every switch and at every correction, 20 to 30 times a second.
  Found by review: easing the blend, tried first, had passed six runs in six
  only because the frames were short. A blend now starts from where the last
  frame carries the aircraft, each part by its own rule: the source it is
  drawn from at that source's own velocity, and the blend on it as the blend
  goes. Guessed from the last two frames' positions, a jump between them and
  a long frame after carried it 399 m on a Windows clang runner. Carried at
  one velocity for the whole, an eased blend's curve over a long frame was
  missed by 23 m on a macOS runner.
  - **A source's own velocity** is its flight model's, turned into the
    Earth-centred frame where the aircraft is (turned at the session's origin,
    100 km off it was a degree out), or, drawn from the updates, the
    interpolated path's own: `net::Interpolated::path_velocity`, the line
    between the two updates either side, the velocity a guess is carried on
    at, nought when held, less the offset being taken up. Found by review:
    worked out from where the path was a frame before, a jump in it was
    counted twice, the second time scaled up by a long frame after.
- **The step counts the blend's own motion too**, at the pace it went that
  frame over a sixtieth of a second. Found by review: carried on along its
  own curve, a blend cancelled out of the step, and a blend of a millisecond
  crossed the whole gap in one frame and read as nought. A long frame does not
  make a smooth blend a step. The pace is over the part of the frame the
  blend was going in, so a blend over within a frame counts whole however long
  the frame (second review: paced over the whole of a 100 ms frame, a blend
  of a millisecond would have read 1.8 m). Counted whole wherever it was no
  longer than its frame, as first tried, a correction that took up the rest
  of a switch's blend over 0.25 s counted 8 to 27 m across the built 0.4 s
  frame.
- **A long frame after every switch is now built on purpose**: with
  `--long-frame-after-switch`, which both network checks pass, the
  command-line client draws nothing for 0.4 s after the third frame after a
  switch while it flies on - the third, so the frames before it show a blend
  too quick. It says how many it built against how many switches, and the
  checks fail unless every switch had one or was overtaken by another before
  its own came: a take-back is two switches a frame or two apart, handed back
  and then predicted again, and the second's long frame is the first's; at
  most one is overtaken, and a frame the runner made 0.4 s long by itself
  counts. With every switch counted as overtaken, both checks went red. (It
  was an environment variable, read by a bare `getenv`, which MSVC
  deprecates; review.)
- **A take-over blends from where the aircraft taken was going**: its last
  frame as another aircraft carries its interpolated path's velocity, not the
  one its update reported (second review). And the client with the window
  takes who flies its own aircraft from the newest update only, the
  take-over's own included.
- **An aircraft drawn again after a pause took up a guess seconds old.**
  CI's Windows release runner once stepped 294.8 m at a switch; the report
  now says what made its largest step, and on a loaded Linux machine it said:
  the third frame after a switch, drawn from the updates, carried at
  1,194 m/s. The client's own aircraft is drawn from the updates before its
  prediction starts; if the last such frame was a guess through a gap, the
  interpolation kept that guess, unasked, until the hand-over drew from it
  again seconds later, and then took up the difference between the guess and
  where the aircraft was - hundreds of metres - over a quarter of a second.
  The interpolation now forgets a guess, and anything it was taking up,
  when it has not been asked for longer than a guess is held: nothing was
  drawn to keep on from. `an_aircraft_drawn_again_after_a_pause_is_drawn_where_it_is`
  builds it; without the fix it is drawn 589.8 m from where it is.
- **An update from before a take-over, reordered past it, took the old
  aircraft over again.** Through 200 ms of jitter the command-line client
  once said "took over aircraft 4", then "1", then "4" - the 200 ms take-over's
  unexplained failure. An update older than the newest heard, naming another
  aircraft as this client's, is now dropped, and only the newest may change
  which aircraft is its own. `--late-update-after-take-over`, which the
  take-over check passes, hears the last update from before each take-over
  again after it, every run. The client with the window had the same fault
  and has the same guard; nothing reorders its updates in a test yet.
- **A switch is eased in and out**, so its speed changes smoothly at both
  ends. A correction is not eased: it is restarted with every update, and
  easing would hold each back longer.
- **A switch was marked only by starting a blend**, so with no blending at
  all the step at a switch read 0 and the test passed. A switch is now marked
  whenever the source changes, except at the first frame of all. With no
  blend, the take-over fails at 30 m.

The windowed-client take-over test and the ride-along test shared a Cesium
cache and locked it when run together; each case now has its own.

**Still to say:** the blend is the command-line client's model of a display.
The client with the window does not blend at a switch at all: a tail.

**Found by CI on another branch: a client slow to start was put right too
far.** The client with the window, joining a slow Linux debug runner's
server, was once put right by more than the 20 m a correction may hide. Built
on purpose, with the client standing still five seconds after joining
(`--slow-start 5`, now in `client_on_server.cmake`), it failed every time,
at 45 m. Two causes, each fixed and each seen to fail without its fix:
- **The session kept the first 128 updates** that came while nobody asked
  for them, about five seconds' worth, and dropped the rest. A client that
  asked later heard the oldest, then one from seconds on. It now keeps the
  newest 128. Without this: 42 m.
- **The first update after joining was taken as a correction** to where the
  client had been at the join, seconds before. It is now where the aircraft
  is, as the command-line client already had it. Without this: 57 m.

**Verified.**
- **`a_player_takes_over_an_ai_aircraft_with_no_step_at_100_ms_and_a_players_is_refused`**,
  and at 200 ms. There are three players.
  - The second rides along through `glideslope_impair`.
  - The third hands its own aircraft to the AI.
  - A player's aircraft is refused both ways: the third asks for the
    second's, flown by hand, and the first asks for the third's, flown by the
    AI.
  - Then the second takes the AI's over. What it showed stepped 0.7 m across
    the change at 100 ms and 1.3 m at 200 ms, with a long frame built after
    it and an update from before it heard again after it, against the 5 m
    the swaps are held to, with no correction too large to hide. The
    hand-over and take-back in the network checks stepped 0.7 m and 1.1 m.
  - **The server flies it by that player's inputs.** At least fifty updates
    come after the take-over, sent once the server had applied an input sent
    since, with the prediction error within its bound.
  - A server with `--no-take-over` then refuses.
- **`the_client_with_the_window_takes_over_the_ai_aircraft_it_rides_along_in`**:
  the client with the window rides along in the AI's Cessna and takes it over
  four seconds in. By the shot it says it is flying that aircraft, and the
  server says the pilot has it and has applied inputs sent since. The HUD
  says `FLYING PILOT`.

**Seen to fail.**
- **With the server's check for a player's aircraft removed**, the first
  player took the third's, flown by the AI, and the 100 ms test went red:
  "the server did not refuse player's aircraft 2".
- **With the taken aircraft left to the AI**, both went red. The windowed test
  said "the server was not flying aircraft 4 by this client's inputs". The
  command-line client waited for inputs that were never applied, past its
  neighbour in the pipe, and died of it.
- **With the client with the window sending no request**, its test went red:
  "the client did not take the AI's Cessna over".
- **With an update from before the take-over taken as newer**, the
  take-over check went red, taking the old aircraft and the new over in turn.
- **With a blend of a millisecond**, the take-over stepped 10.8 m and the
  hand-over and take-back 26.9 m. **With no blend**, 29.4 m and 31.8 m.
  **With the source carried on at no velocity**, the hand-over and take-back
  stepped 11.7 m.
- **With the interpolated path's velocity said to be the updates'**, the
  unit test `the_velocity_an_aircraft_is_drawn_moving_at_is_its_paths_own`
  went red, 20 m/s out; it walks every state the path can be in and says how
  many frames it judged in each.

Each was put back.
