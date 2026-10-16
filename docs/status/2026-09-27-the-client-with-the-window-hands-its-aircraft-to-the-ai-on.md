### The client with the window hands its aircraft to the AI on a server, 2026-09-27 — tail still open

**The tail stays open**: its verification asks that what is shown does not
step, and nothing blends the switch yet. Ticking the hand-over alone would
split the item to tick the easy half, which the plan does not allow.

**What is missing first.** What it shows of its own aircraft is not blended
at a hand-over or a take-back: handed over, the frames before the updates
have given it two positions show it where it was last predicted, and then
where the updates put it 100 ms behind; taken back, it is put where the
next update says. That is the open tail "the client with the window does
not blend its own aircraft at a switch", and nothing here measures the step.

**The tail.** Pressing A on a server swapped the pilot in the client's own
flight and nowhere else: the server never heard, and the prediction went
on as before - A online did nothing.

**What changed** (`frontend/client/online.*`, `frontend/client/main.cpp`):
- **A on a server asks the server** (`CONTROLLER_SWAP` for its own number,
  to the AI or back to the person, as the command-line client's
  `--hand-over-at` and `--take-back-at` do); offline it swaps the pilot
  locally as before. `--hand-over-after S` and `--take-back-after S` do it
  S seconds of flight in, for tests.
- **Who flies it is what the server's updates say**, by the newest word
  only. Handed to the AI, its own aircraft is no longer predicted - the
  flight is not stepped, nothing sent flies it - and is drawn from the
  updates, 100 ms behind, as any other aircraft is; the client rides along
  in it (`WATCH` of its own number, unless riding along in another), so
  its seat and its controls as the AI moves them are shown. Taken back, the
  flight is put where the next update says (`Flight::adopt`) and predicted
  from there, as after a take-over.
- **The HUD says who flies it as the server says**, not as the local
  flight's controller would. At each switch a frame is drawn, shot or not,
  and the client prints that the server says the AI (or the pilot) has its
  aircraft, and what the HUD read.

**From the review** (same day):
- **Handed over, riding along in nothing is riding along in its own
  aircraft.** W past the last aircraft, and the switch, watch its own
  number while the AI has it, so the camera and HUD follow the aircraft as
  the updates put it, not the local flight, which is not flown then and
  would sit where it was handed over. W's order leaves its own out and
  wraps to it. T on its own AI-flown aircraft is a take-back, and says so.
- **The test waits on events**: the shot is held, up to the minute past its
  tick the ride-along shot already allows, until the take-back asked for is
  heard and the server has flown an input sent since; it prints how long it
  waited and what it heard.
- **A narrow race, left open**: A pressed while a take-over is in flight is
  sent for the aircraft this client still holds, which the server may then
  hand back after the take-over - named in the plan's tail.

**Verification**:
`the_client_with_the_window_hands_its_aircraft_to_the_ai_on_a_server_and_takes_it_back`
(`tests/cmake/client_hands_over.cmake`): the headless client flies its own
Cessna against a server with one AI, hands it over 4 s in and takes it back
10 s in, and shoots 16 s in. It must say once each, the AI first, that the
server said the AI had its aircraft - the HUD then reading FLYING AI and not
FLYING PILOT - and that the server said the pilot had it - the HUD then
reading FLYING PILOT; and at the shot that the server says the pilot has it
and has flown it by inputs sent since it was taken back. Seen to fail: with
A on a server put back to what it did on `main` - swapping only the local
flight's pilot, the flags calling that instead of asking the server - it
failed with "the client said 0 times, not once, that the server gave its
aircraft to the AI"; reverted, it passes (about 55 s on Linux debug), as do
the ride-along and take-over tests beside it. It also rides along in the
AI's aircraft 6 s in and on past the last 7 s in (`--next-aircraft-after`,
what W does), and must then ride along in its own aircraft with the camera
within 5 m of its centre as the updates put it. Seen to fail: with the wrap
watching nothing, as before the review, it failed with "W past the last
aircraft did not ride along in its own aircraft 0", the view being the
local flight's; reverted, it passes.
