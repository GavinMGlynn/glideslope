### Players' turns on final: the window, AI leaders, the runway, the bound, 2026-11-11

**What is still not true first.** **The item stays open: an AI aircraft
whose own plan ends in a landing on that final** (a plan's `land`, or a
copilot's route) **does not count as a leader**, only one put there with
`--ai-on-final`; no test builds one on the approach as a player joins.
**Closing the window while she waits quits the client cleanly** (it says
so and exits 0), but no test closes one. The window shows why she waits as text
over the sky, not the airfield: no flight is built until her turn. Only an
AI aircraft put on the final by the operator (`--ai-on-final`) counts as a
leader; one landing at the end of its plan, or flying near the final for
another reason, does not, and no AI aircraft waits for a player. A leader told to leave
the final is not made to; the next is put on the other end whether she
goes or not. Run on linux-release only. The Windows cost figures in
tests/ci_costs are copied from Linux, not measured.

**What changed.**

- **The window opens before joining**, and while the server has given her
  no aircraft it shows WAITING FOR THE AIRCRAFT AHEAD and the server's
  reason over the sky, redrawn ten times a second. For tests,
  `--shot-waiting FILE` writes that frame once the reason has come.
- **An AI aircraft put on the final counts as a leader** (`--ai-on-final`):
  it holds a player until it is three miles on, has landed, has gone around,
  is off the final, or has waited out, the same as a player ahead. It is
  remembered as on final from when it was put there.
- **The runway is cleared**: a player's aircraft that the learnt landing has
  stopped turns off and stops beside the runway, as an AI's landing does.
  `Controller::to_ai_learnt_approach` now records its runway, so the
  controller's vacating takes her off. Before this she stayed where she
  stopped. Her learnt landing now also goes around if the runway is not
  clear, or if traffic is close ahead on final, as an AI's does.
- **Past the bound, the other end**: when the aircraft holding her is past
  the bound and still over the start, the server does two things:
  - it tells that aircraft's player to leave the final (a
    `TRAFFIC_INSTRUCTION`: "leave the final to YSSY 16R: go around and clear
    it; traffic has waited behind you past its time, and is put on final to
    YSSY 34L");
  - it starts the waiting player on final to the runway's other end, once
    that end's start is clear by the same rule.

  The two starts are more than six miles apart, so one aircraft cannot hold
  both, and the wait is bounded - where the other end's wind is one her
  learnt landing's gate admits (`sim::learnt_gate_wind_refuses`: 5 kt
  behind, 15 ahead, her crosswind, 15 in all). A southerly at Sydney is
  behind 34L: then she stays held until the one holding the final goes, and
  the server says why once ("held past the bound, and the other end, YSSY
  34L, refused: 10 kt of tailwind; the gate is within 5"); the wait is not
  bounded then, which a tower would answer by turning the one holding the
  final away - here only told.
- **`Controller::straight_in()` now names a player's learnt landing**: with
  its runway kept, a player handed to the learnt landing is on final for
  other aircraft's spacing (the AI's giving way to one straight in, as it
  gives way to an AI's), where before she was not.
- **The window's close while she waits** quits the client: it says
  "closed while waiting for an aircraft; quitting" and exits 0. A tower does the same when it turns a
  following arrival away from a final someone is holding. The only
  phraseology the search found for this is "go around" or "break off the
  approach" and resequencing; there is no published rule for an aircraft
  orbiting on final.

**Verification.**

The tests are runs of `tests/cmake/server_players_take_turns_on_final.cmake`.

- **The window shows why**: in
  `the_window_client_joining_on_final_behind_a_pa28_waits_told_why_and_is_at_the_gate_thirty_seconds_after_she_starts`,
  the window client (`glideslope --headless`) prints "the window, waiting,
  shows WAITING FOR THE AIRCRAFT AHEAD / WAITING FOR THE ..." with the
  aircraft ahead named, and writes the frame. It is then given her aircraft
  at 142.2 s and is 220 m inside the gate 30 s on. Seen red with the
  waiting screen not drawn: "the window did not show why it waited".
- **AI leaders**: in
  `a_player_joining_on_final_behind_an_ai_aircraft_on_it_waits_for_it_and_is_at_the_gate_thirty_seconds_after_she_starts`
  (port 24917), with `--ai-on-final YSSY/16R`, the C172P waits for the AI.
  She is given her aircraft at 188.5 s, when the AI has landed 2.49 miles
  from her start, and is 238 m inside the gate 30 s on. Seen red with AI
  aircraft not counted: "0 players were told to wait their turn".
- **Runway cleared**: every run now requires each player who lands and
  stays to leave the runway ("aircraft N, ..., has left YSSY 16R, stopped
  beside it"), and the next's touchdown to come after that in the server's
  word. Seen red with the runway not
  recorded: "place 1, the pa28: never left the runway".
- **The other end's wind**:
  `a_runways_other_end_is_offered_only_into_a_wind_its_learnt_landings_gate_admits`
  (tests/unit/test_learnt.cpp), eight cases: a southerly of 10 kt is 10 kt
  behind 34L, refused, and ahead on 16R, admitted; a northerly the other
  way round; calm admitted; 5 kt straight behind admitted and 5.2 refused;
  16 kt across refused. And end to end:
  `a_player_held_past_the_bound_with_the_wind_behind_the_other_end_waits_on_and_starts_into_it`
  (port 24937), the orbit below in a METAR southerly of 10 kt: 34L
  refused for its tailwind, the PA-28 held until the one orbiting has gone,
  then started on 16R into the wind and at its gate 30 s on.
  She was given her aircraft at 300.0 s, when the C172P's client left, and
  was 349 m inside the 16R gate 30 s on (332 s in all). The northerly run's
  PA-28 started on 34L at 240.6 s and was 318 m inside its gate.
- **The bound**: in
  `a_player_held_past_the_bound_by_one_orbiting_over_the_start_starts_on_final_to_the_other_end_and_is_at_its_gate`
  (port 24927), in a METAR northerly of 8 kt (into 34L), the C172P's
  copilot route orbits 1,000 m round the 16R start at 700 ft (`tests/data/copilot/route-orbit-over-the-16r-start.txt`), and a
  PA-28 joins behind her. At 240.6 s (the bound behind a C172P) the PA-28
  starts 2.35 miles out on final to 34L, 7.12 miles from the orbit. She is
  318 m inside that gate 30 s on. The C172P's client heard that it was told
  to leave the final. Seen red with the other end not used: "the one
  orbiting over the start was not told to leave the final".
- `glideslope_cli selftest`, linux-release: hash 197366fd3fe6a7dc before the
  runway kept for a player's learnt landing, 197366fd3fe6a7dc, unmoved after - the
  selftest flies no learnt landing.
- Coverage is asserted across the six runs: every aeroplane with a learnt
  landing follows at least once, and the second, third and fourth places
  are flown. The server-timeouts check passes with its totals unchanged.
  The other learnt-landing tests pass with the runway now cleared: 109
  tests matching learnt, on_final, gate, ports, flags and usage, and the
  66 window and client tests, with the window now opened before joining
  (two skipped: they need a display, which these runs do not have).
