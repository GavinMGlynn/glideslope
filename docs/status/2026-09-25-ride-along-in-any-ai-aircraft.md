### Ride along in any AI aircraft, 2026-09-25 — item done

**What is missing first.**
- **Taking over** the aircraft ridden in is the next item.
- **Airspeed is not sent**, so the HUD shows ground speed (`GS`).
- **What the AI is doing**, holding or navigating, is not sent, so the HUD
  says `FLYING AI` and no more.
- **Mach and flight level** are not shown for an aircraft ridden in.

**What works.**
- **`WATCH`**, a new reliable message, is how a client names the aircraft it
  rides along in, or none. Its own state updates then carry that aircraft's
  controls: aileron, elevator, rudder, throttle, flaps and gear, each as the
  wire's 16-bit fraction, with gear that does not retract said so. That adds
  14 bytes; a full update is 1,144 bytes sealed, of 1,232. `TRANSPORT.md` and
  the doc client were updated, and so was `THREATS.md`, brought up to date as
  well with what Phase 7 changed.
- **The client with the window.**
  - `W` steps through every aircraft in the sky and back to your own;
    `--ride-along` watches the first AI aircraft from the start.
  - The camera goes to the ridden aircraft's seat, at its pilot's eye as its
    own flight model has it.
  - The HUD shows that aircraft's ground speed, height above the sea,
    heading, climb, pitch and bank, who is flying it, and its controls, 100 ms
    behind the clock as its position is.
- **Every other aircraft is drawn exactly where its own client draws it**:
  its model at its visual reference point, read from its flight model. Until
  now it was drawn about its centre of gravity, a few metres off.
- **`glideslope_cli connect --watch-ai`** rides along too, for the network
  checks.

**Found by CI, fixed before the merge: a slow machine was let go.** The
client with the window joins a server and then builds its flight, window and
renderer, saying nothing meanwhile. On a Windows debug runner that took longer
than the test's server waits for a silent client, 3 s. It was let go, heard one
update, and drew its HUD with no controls: the shot's tick had come first.
- A thread now keeps the session, answering the server's pings, from the join
  until the frame loop takes over.
- `--slow-start S` stands the client still S seconds after joining, so that
  the ride-along test builds that situation on every machine: 5 s against
  the server's 3.
- Without the keep-alive, the test failed on Linux as the runner had: no
  controls heard in the minute after the shot's tick. With it, it passes on
  Linux and on Windows.
- The shot now waits for the controls, at most a minute of the flight past its
  tick, and then says what it heard.
- Controls are held at the oldest update kept when the clock is behind them
  all, as they are held past the newest.

**Verified.**
- **The network checks**, at 100 and 200 ms with jitter, loss and gaps. A
  client riding along in the AI through the relay has its controls judged,
  frame by frame, against a client riding along in it over a clean line. Every
  control must be within half a hundredth, which is what the HUD shows them
  to. The worst was 0.0004, the wire's rounding.
- **`the_client_with_the_window_rides_along_in_an_ai_aircraft_and_shows_its_controls`.**
  The client rides along in the AI Cessna, and its camera is 1.5 m from the
  Cessna's centre, which is its seat. Its HUD reads `FLYING AI` and every
  control, and no gear line, since a Cessna's gear is fixed.
- **`a_state_packet_carries_the_controls_of_the_aircraft_its_client_watches`**
  covers the watched block: it reads back, fixed gear is said so, the flag is
  walked, and the value that means fixed is refused in any other control.
- **Every message walk now asserts** that it walks every kind the code knows.
  A new kind had passed them all, unwalked.

**Seen to fail.**
- Controls shown as they came, not 100 ms behind: 0.023 out, 267 frames over
  the bound.
- The camera left with your own aircraft: 446.8 m from the aircraft ridden in.
- "Fixed" read as a control: refused no longer, and red.
