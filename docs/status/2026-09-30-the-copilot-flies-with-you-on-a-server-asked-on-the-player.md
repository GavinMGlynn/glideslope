### The copilot flies with you, on a server: asked on the player's machine, flown by the server, 2026-09-30 — item done

**What is not done**: a client predicting its own aircraft does not know its
engine has stopped (a tail) - and that is ordinary play, not a test's alone:
the state update's `02` is said for any engine the flight model has stopped,
a tank run dry as much as `--fail-engine-at`. In the tests the AI is flying
it by then, so nothing is predicted. A client older than `02` refuses every
state update while any aircraft has an engine stopped. The
server says nothing back of a route it refuses: the client takes the route
it sent as the one flown. The window client's copilot is tested by one
flight; its routine looks and its standing by are the same code as the
headless client's, tested there.

**An engine failure on a server, made true after the review** (it had been
ticked on the command line's alone). The state update's condition byte says
`02`, flying with an engine stopped (TRANSPORT.md); the server stops every
player's first engine at `--fail-engine-at S`; the player's copilot, told,
is asked at once, "the engine has stopped", and answers with a glide the
server checks and flies.
`a_players_copilot_glides_its_aircraft_on_a_server_when_the_engine_stops_as_recorded`:
Claude Haiku 4.5, recorded (`tests/data/copilot/engine-server-anthropic.jsonl`),
first flew to Manly, then, the engine stopped 30 s in, glided at 65 kt to
Sydney's runway 25 - 8,778 m from it at 67 kt, then 7,828 m at 61 kt. Red
with the condition never said: the client never heard its engine stop.

**It flies only what its player gave it** (from the review): taken back
with A, the copilot stands by - nothing more asked, an answer still to come
not sent - until the player asks again.
`a_players_copilot_stands_by_when_its_pilot_takes_the_aircraft_back_as_recorded`:
routine looks every 20 s, taken back 40 s in, and the answer that came after
was not heard; the server handed the aircraft to the AI once. Red with the
standing by taken out.

**Nothing of it is on the thread that steps or draws** (from the review):
its ground - a geoid, a DEM and the world's runways, which a fresh cache
downloads - is made on a thread of its own as the copilot is made, when the
client has an aircraft rather than at the first C; and what the copilot is
told is worked out on the question's own thread (`copilot::Copilot::ask`
with a function). It keeps a DEM of its own, not the client's: a
`world::Dem` is not for two threads, and the tiles are shared on disk.
Going away, a copilot abandons its model's request at once
(`platform::HttpRequest::abandon`) and asks nothing more:
`a_copilot_going_away_gives_up_its_question_at_once`, red with the request
left to run - it was waited out. A model silent for two minutes, not five,
is given up.

**What a player's copilot is told** now includes whether the engine runs
(above); its speed is its speed over the ground, which is what the updates
carry. The server's own AI aircraft do not ask a model in flight: each is planned once,
by the model its server chooses, with the operator's key ("Each AI aircraft
is planned by the model its server chooses", below).

**The key stays on the player's machine** (the project owner, 2026-09-30;
REQUIREMENTS.md section 5 and 6.4, TRANSPORT.md, THREATS.md). The player's
client asks the model with the player's key, and sends the server only the
route it answered, as a new reliable message, `COPILOT_ROUTE` (`09`):
waypoints and orbits, and a glide airspeed, at most 12 waypoints, 936 bytes at
its limits, written byte for byte in TRANSPORT.md. **The server trusts it no
further than a plan**: it reads it as one (`sim::parse_flight_plan`) from
where its own aircraft is, checks it with the checks a copilot's answer is
held to (`copilot::change_refusal`, which the server now links) against the
aircraft as the server has it - its ground from the server's DEM, its height
above the sea from the server's geoid - and refuses what fails, printing why
and changing nothing. What passes is flown by the server's AI: the aircraft
handed to it first if the player was flying it, announced as any hand-over
is, so the client stops predicting it and draws it from the updates. The
server says where each copilot's route has got to every half minute.

- **The client with the window**: `glideslope --copilot TASK
  [--copilot-provider P] [--copilot-model M]`; C asks, and the copilot looks
  again a minute after each answer. **The headless client**: `glideslope_cli
  connect ... --copilot AIRCRAFT TASK` with `--copilot-provider`,
  `--copilot-model`, `--copilot-record`/`--copilot-playback`, `--copilot-at S`
  on the session's clock, `--copilot-answers N` and `--copilot-stay S`; and
  `--send-route FILE`, a route sent as it is, never checked on this side. Both
  use `frontend::PlayersCopilot` (`src/frontend/players_copilot.hpp`): told
  its position from the updates, its height above the sea from the geoid and
  the ground from the DEM as the player's machine has them, its speed over
  the ground (no update carries airspeed) and the runways near it.
- `a_players_copilot_asked_on_the_players_machine_is_flown_by_the_server_as_recorded`:
  the server and the headless client, played back from Claude Haiku 4.5's
  recorded answer to "fly to Manly at 3,000 ft, then orbit over Manly beach"
  (`tests/data/copilot/manly-server-anthropic.jsonl`), no key. The server
  handed the aircraft to its AI and flew the route - to MANLY, 6,188 m from
  it at 30 s and 4,579 m at 60 s. The client leaves 60 s of the session's
  clock after sending it; the server runs until it has gone. Red with the
  server not flying what it took.
- `a_route_the_server_cannot_fly_is_refused_by_it_though_a_client_sent_it`:
  a client sends a waypoint 200 ft over the sea
  (`tests/data/copilot/route-200-ft-over-the-sea.txt`); the server says "LOW
  is at 200 ft, below 500 ft", and neither hands the aircraft over nor flies
  it. Red with the server's check taken out.
- `the_client_with_the_window_asks_its_copilot_and_the_server_flies_its_route`:
  the client with the window, headless on Vulkan, asks its copilot three
  seconds in (`--copilot-after 3`, what C does) from the same recording; the
  server hands its aircraft to the AI, and what the client showed of its own
  aircraft stepped 0.180 m at the switch from predicting it to drawing it from
  the updates - under the 5 m bound. Red with the route not sent.
- The message is walked by every message test: it writes and reads back,
  every truncation, trailing byte and single-byte change is refused or read,
  its six floating-point fields refuse NaN and infinity (25 fields in all
  now), it fits a datagram at its limits, and TRANSPORT.md and THREATS.md name
  it; `every_message_writes_and_reads_back_what_went_into_it` was red with an
  orbit's direction dropped on reading.
- **A glide is checked on its own terms**: the engine-failure flight must
  hold the glide airspeed the copilot asked for within 5 kt, not merely a
  steady one. A played-back flight that goes another way now ends where the
  recording runs out, says what it measured, and fails after (exit 3), so
  with the glide not flown the test is red for the glide - "67 to 72 kt, not
  within 5 kt of the 60 kt asked for" - before it is red for the recording.

**Rebased onto the collision ground (2026-10-01).** The copilot's ground is
the collision ground everywhere now - the DEM with every runway its own
surface, as the aircraft meets it: the server's check of a route, the
player's copilot's "ground beneath" (its own `world::CollisionGround` on its
own DEM, on its question's thread), and `glideslope_cli fly-copilot`'s
aircraft and figures, which had flown over the raw DEM. So the route a
player's copilot plans is checked by the server against the same ground.
`COPILOT_ROUTE` and the condition `02` came without the protocol's version
moving - it is `02`, for the ground - as TRANSPORT.md's rule for layout
changes before a first release says; the doc client reads condition `02`.

**Rebased onto the learnt landing (2026-10-01).** A copilot's route that
arrives while the learnt landing flies (`sim::Controller::to_ai_learnt_approach`)
now lets it go, as it does a take-off or an approach: the autopilot is
engaged afresh from the controls it had, and every hand-over - the learnt
landing's among them, through `engage()` - ends a glide.
`a_copilots_route_during_a_learnt_landing_replaces_it_with_no_step_in_its_controls`:
handed the route five seconds into the learnt approach, the landing is let
go, no control moves more than a hand's step (0.00833 of its travel) in the
second after, and the route is flown, 1,149 m nearer its waypoint in 30 s.
Red with the learnt landing left flying. **No copilot thread touches a
`sim::Aircraft`**, whose property cache is for one thread: a copilot's
question is asked with a copy of what it is told (`copilot::Situation`), made
on the stepping thread in `glideslope_cli fly-copilot` and the server, and
from the state update's copy of the aircraft on a player's machine.

**From the final check (2026-09-30), fixed:**

- **The stand-by test could still go quiet**: a `keep` and a route not heard
  after a take-back were said alike, so a `keep` there passed. They are said
  apart now, the test wants the route, fails on a `keep` played back, and
  asked of a model now skips on one ("the model answered keep; the rule was
  not tested") rather than passing. The hand-written answer's line says so
  itself, in a "note" the playback does not read. Red with a route sent
  after the take-back (handed over twice); a `keep` played back is refused.
- **One question's situation failing no longer ends the copilot**: only its
  ground not had as it is made does; a DEM tile not fetched for one question
  leaves that question unanswered and asks it again, twice at most. Not
  tested on its own.
- `glideslope_cli connect` says a wreck flies again before anything else it
  flies again with - an engine stopped among it.

**From the re-review (2026-09-30), fixed:**

- **CI failed** (run 36686103327, ubuntu linux-debug):
  `the_client_with_the_window_asks_its_copilot_and_the_server_flies_its_route`
  drew its shot at a fixed 25 s, and the copilot, asked, had not answered by
  then - its answer is taken 10 s after it is asked, and now after its ground
  is made too. The shot now waits on the events - the route sent and the
  server saying the AI has the aircraft - up to five minutes of flight past
  its tick. In the same run
  `the_client_with_the_window_dropped_by_the_operator_says_so_and_does_not_join_again`
  failed on Rocky 9; it is not this change's: no copilot is made there, and
  it failed on main as well (run 36674751464).
- `every_flag_the_server_prints_in_its_usage_is_one_it_takes` failed:
  `--fail-engine-at` was not in its list. It is, and a time that is
  negative, not a number or NaN is refused (three tests, red with the check
  loosened); `--dry-run` says when it will stop the engines.
- **The stand-by test could not fail on its rule**: the answer after the
  take-back was `keep`, which is sent by nobody. The recording's second
  answer is now written in by hand as a route (the only answer in any
  recording not a model's), and the test asserts that no route was sent
  after the take-back and that the AI was given the aircraft once. Red with
  the not-engaged answer sent: handed to the AI twice.
- **The glide's airspeed is checked at every half-minute line from 45 s
  after the glide was taken**, at least two of them, not the last alone -
  the server now says the time on those lines. Red with the server not
  flying the glide: 80 kt against the 65 asked for.
- **An engine that stops while a question is out is asked about**: the
  question asked with it running answers with a route the server refuses
  ("the engine has stopped: the route must begin with glide"), which never
  hands the aircraft to the AI; the copilot waited for that before asking
  about the engine, so it never did. It asks as soon as it can, whoever
  flies it, and again if the engine runs again.
  `a_players_copilot_glides_when_the_engine_stops_while_it_is_thinking_as_recorded`
  (Claude Haiku 4.5, recorded; the engine stopped 10 s in): the Manly route
  refused, the glide to runway 25 flown. Red with the old wait.
- `02` is said for any engine stopped, fuel exhaustion included; an older
  client refuses the whole update while one is (TRANSPORT.md).
- **Its ground failing kills its copilot once, and says so**: whatever
  making the ground threw - a fetch, a file - ended every question after
  without a word. Now it says "no copilot: ..." once and is gone for the
  session. Going away, its fetches are given up (`world::FetchesGivenUp`)
  as its model's request is abandoned, and each is waited for only as long
  as giving up takes. Neither has a test of its own.
- The route for another's aircraft is sent for another **player's** - a
  second client's - which only the ownership check refuses: red with that
  alone taken out. Speeds are kept by catalogue id; the engine count is
  kept, not read at each update; `--copilot-routine` wants a positive
  number, and `--route-for-another`/`--route-when-wrecked` want
  `--send-route`.
- Every test in the areas this touches was run - 234, by name: server,
  client, copilot, state, message, flag, refusal, connect, take-over,
  hand-over, ride-along and planned. 233 passed or skipped as they should;
  `an_aircraft_taken_back_from_the_ai_and_wrecked_flies_again_as_its_players`
  failed once, in 2.8 s at the start of that run, and passed in each of three
  runs after, alone and beside another server test. Its output was not
  kept, so why is not known; it touches no copilot code.

**From the review (2026-09-30), fixed:**

- **One route could end the server.** Checking a route worked out the
  aircraft's speeds from its published figures there and then, which throws
  for an aircraft whose figures give none (the 747-400, the F-22), and
  nothing caught it. The speeds are now worked out once, as an aircraft is
  made, and kept by model; a route for an aircraft with none is refused,
  saying why; and whatever else a check throws is a refusal.
  `a_flyable_route_is_refused_when_its_speeds_are_not_known_and_the_server_goes_on`
  gives the server a plan flown by a 747-400 (`tests/data/copilot/747-off-bondi.plan`):
  refused, "747-400 publishes no stall speed", both programs ending as they
  should. Red with the old code: the server died (exit codes 0;1).
- **Whose aircraft**: `a_flyable_route_is_refused_when_it_is_not_this_client_s_aircraft_and_the_server_goes_on`
  sends a flyable route for the AI's aircraft, and
  `a_flyable_route_is_refused_when_it_is_a_wreck_and_the_server_goes_on` one
  for the client's own once it is a wreck; each refused, nothing handed
  over. Red with each check taken out.
- **A waypoint's name is letters, digits and underscores**, and something,
  or the message is refused: a newline had smuggled plan lines past the
  12-waypoint limit, a `#` commented one out, and an escape reached the
  operator's terminal. `every_refusal_the_document_names_for_a_copilot_route_is_refused`
  builds all twelve refusals TRANSPORT.md names, one by one - red with the
  names unchecked. The message at its limits is **936 bytes**, not the 925
  first written: a text's length is two bytes.
- **An answer that could not be sent is refused back to the model**: a name
  over 32 bytes or not a word, more than 12 waypoints, or round an orbit
  more than 255 times - which the client had sent anyway, for the server to
  drop unread. The limits are the message's, asserted equal at compile
  time.
- A glide ends with every hand-over (`sim::Controller`, to the AI or back);
  a copilot's route ends with its wreck and its take-back. A route replaces
  the server's plan for an aircraft a player took over, on purpose: wrecked,
  it flies again holding its course, as any player's aircraft the AI flies.
- THREATS.md says the ground along a route is not checked - only 500 ft
  above the ground beneath the aircraft as it is read.
