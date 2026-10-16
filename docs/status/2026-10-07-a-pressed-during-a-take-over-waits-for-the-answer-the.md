### A pressed during a take-over waits for the answer; the server says when it refuses one; protocol version 7, 2026-10-07 — tail done

**What was missing.** A pressed while a take-over was on its way was sent
at once for the aircraft the client still held - which, by the time the
server read it, was the aircraft it had just left, under a number the
server had moved it off. The server did nothing with it, and the aircraft
taken stayed the pilot's. The client could not wait for the answer,
because a refused take-over had none: the server said nothing.

**What changed.**
- **`TAKE_OVER_REFUSED` (`0A`), protocol version 7.** A take-over the server
  will not make - forbidden by `--no-take-over`, a player's aircraft, not the
  AI's, a wreck, no number free - is answered to the client that asked, and
  to no other, with the aircraft's number as it was asked for. Two bytes.
  `TRANSPORT.md` describes it, `THREATS.md` lists it (server-sent, never
  accepted by a server), the client written from the document and the
  gearstick refusal (`474c4453070401`) say 7; the collision ground is
  unchanged. `net::ClientSession::take_refusals` hands them up; a session
  joined again forgets them.
- **The window client knows a take-over is on its way**
  (`Online::taking_over`): from the request being sent until the update
  that gives it the aircraft, the refusal, or the session being let go.
- **A pressed meanwhile is held** (`glideslope: A held until the server
  answers the take-over of aircraft N`) and, once answered, asks for
  whichever aircraft is its own by then (`A, held, is for aircraft N`). A
  refusal is said: `the server refused to take over aircraft N`.
- `--press-a-with-take-over`, a test flag: A in the frame
  `--take-over-after` asks.
- **From the review of #117: A can no longer be held for ever.** The server
  answers every take-over it reads: past the client's rate of requests, and
  from a client with no aircraft, it now refuses with `TAKE_OVER_REFUSED`
  where it said nothing, and says when a refusal could not be queued. The
  window client never asks for its own aircraft as a take-over
  (`Online::take_over` returns false; T on its own was a take-back
  already), and gives up a take-over unanswered after five seconds of flight
  (600 steps, counted in ticks flown), saying so. Tests:
  `a_take_over_past_the_servers_rate_of_requests_is_refused_and_said`
  (twelve `WATCH`es first, `--watches-before-take-over 12`) and
  `the_client_never_asks_to_take_over_its_own_aircraft_and_a_is_not_held`
  (`--take-over-own`). Seen to fail: with the server silent past the rate,
  "the client was not told the take-over of aircraft 4 was refused"; with
  the client asking for its own, "the client asked for its own aircraft as
  a take-over"; both reverted, both pass.
- **The selftest was run again** after these changes (linux-debug): hash
  `30ac70b84cab7d7c`, unchanged - nothing here touches a flight without a
  server.

**Verification**:
`a_pressed_during_a_take_over_hands_the_aircraft_taken_to_the_ai` and
`a_pressed_during_a_refused_take_over_hands_the_aircraft_kept_to_the_ai`
(`tests/cmake/client_rides_along.cmake`, `A_DURING`): the window client
rides along in the one AI Cessna, asks to take it over 4 s in and presses A
in the same frame. Taken: A must be held, the aircraft taken over, and
then exactly one hand-over asked - for the aircraft taken - which the
server must say the AI has. Refused, on a server started `--no-take-over`:
the client must be told the take-over was refused, nothing taken, and its
one hand-over be for the aircraft it kept, which the server gives the AI.
Seen to fail: with A sent at once, the taken test failed - the hand-over
asked for aircraft 0, the one being left, and the server still said the
pilot had aircraft 4, the one taken; with the server's refusal not sent,
the refused test failed - A held for ever, nothing said of the refusal.
Both reverted, both pass (144 and 147 s in the sanitized debug build). The
message unit tests walk ten kinds now: every pair (100), truncation,
trailing byte, single-byte change, and every loss pattern (4,094 of 4,096
losing something).
