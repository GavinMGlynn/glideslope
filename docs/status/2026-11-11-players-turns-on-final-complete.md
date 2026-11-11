### Players' turns on final: the window, AI leaders, the runway, the bound, 2026-11-11

**What is still not true first.** **A measured limit, which the owner's
ruling of 2026-11-11 allows**: where a person holds the final over the
start past the bound and no other runway end at the airfield takes her -
each behind her, across her more than her gate admits, or not clear - the
next waits, told why, for as long as that person stays; the server never
moves a person's aircraft (REQUIREMENTS 6.5). The window shows why she
waits as text over the sky, not the airfield: no flight is built until her
turn. No AI aircraft waits for a player. Closing the window while she waits
quits the client cleanly (it says so and exits 0), but no test closes one.
Run on linux-release only; the Windows cost figures in tests/ci_costs are
copied from Linux, not measured.

**Left alone in wind, she leaves the gate; the item is open for it.**
Measured 2026-11-11 with the command-line client joined as the window
client joins (`--predict --from-held`), one player on final to 16R, the
server's verdict 30 s on, at the gate's wind limits (15 kt ahead, 15 across
from either side - 12 for the Cub - and 5 behind), each start three ways:
pointing down the runway, as now; crabbed into the wind by the whole wind
correction angle, asin(crosswind / airspeed); and crabbed at most 4.5
degrees, inside the gate's heading limit.

| | 15 ahead | 5 behind | 15 from the right | 15 from the left |
| --- | --- | --- | --- | --- |
| C172P, down the runway | refused: 15 kt of headwind (the estimate over 15) | at the gate, 141 m | refused: 15 kt of crosswind | heading 10 degrees off |
| C172P, crabbed 14.5 | as above | at the gate | 15 kt of crosswind | 147 m left of the centreline |
| C182, down the runway | 189 m right of the centreline | heading 5 degrees off | 15 kt of crosswind | 15 kt of crosswind |
| C182, crabbed 13.5 | 189 m right | at the gate, 107 m | 15 kt of crosswind | 227 m left |
| PA-28, down the runway | 15 kt of headwind | at the gate, 104 m | 143 m left | 201 m right |
| PA-28, crabbed 13.5 | 15 kt of headwind | at the gate | 81 m right | heading 10 degrees off |
| J-3 Cub (12 across), down the runway | heading 12 degrees off | heading 5 degrees off | 0.7 miles past 25's gate | 0.9 miles past 07's |
| J-3 Cub, crabbed 16.2 | as above | as above | as above | as above |

Crabbed at most 4.5 degrees did no better (C182 115 m, PA-28 67 and 126 m
off the centreline). So the crab is not the cause: an aeroplane left alone
in wind wanders - the Cub and the C182 even with it straight ahead - and in
a crosswind the gate's 5-degree heading limit is less than her crab at 6 kt
across. A crabbed start was built, measured and taken out; it is not in
this change. Searched: the wind correction angle (WCA = asin(crosswind /
true airspeed)), FlightGear's `--in-air`, `--offset-distance` and `--wind`
starts, and X-Plane's start on final - none of them crabs an aircraft
started on final. Both this item and "A 172 a window client joins on final"
are unticked for it.

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
- **Past the bound, another runway**: when the aircraft holding her is past
  the bound and still over the start, the server
  - tells that aircraft's player to leave the final (a
    `TRAFFIC_INSTRUCTION`: "leave the final to YSSY 16R: go around and clear
    it; traffic has waited behind you past its time, and is put on another
    runway if one will take her"), and
  - starts the waiting player on final to another runway end at that
    airfield: of those whose wind her learnt landing's gate admits
    (`sim::learnt_gate_wind_refuses`: 5 kt behind, 15 ahead, her crosswind,
    15 in all), the most into the wind first and the runway's own other end
    first among equals, the first whose start is clear by three miles (the
    bound not eased there). A tower turns a following arrival to another
    runway rather than keep it waiting behind one holding the final; the
    only phraseology the search found is "go around" or "break off the
    approach" and resequencing, with no published rule for an aircraft
    that will not (Doc 4444 has one dangerously placed on final told to go
    around).
  - **Where none will do**, she waits, and is told why ("...; past its
    time, and no other runway here will take her: YSSY 34L 13 kt of
    tailwind; ..."), sent again when that changes; the server says it once.
    It never moves a person's aircraft: the owner's ruling of 2026-11-11
    (REQUIREMENTS 6.5).
- **An AI aircraft whose own plan lands on that final is a leader**: the
  server's plan it flies, its own (a model's or a copilot's), or an
  approach it flies straight in, to the same runway end (threshold within
  50 m, heading within 5 degrees), counted on final from when it first
  comes within three miles of the start.
- **Her wheels' touch is said as it happens** ("aircraft N, ID, touched
  YSSY 16R"): the touchdown's figures are said once she has stopped.
- `outside_learnt_gate` judges the wind with `learnt_gate_wind_refuses`, not
  a copy of it; the order of its reasons is unchanged.
- **`Controller::straight_in()` now names a player's learnt landing**: with
  its runway kept, a player handed to the learnt landing is on final for
  other aircraft's spacing (the AI's giving way to one straight in, as it
  gives way to an AI's), where before she was not.
- **The window's close while she waits** quits the client: it says
  "closed while waiting for an aircraft; quitting" and exits 0.

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
- **Runway cleared before the next touches**: where a follower's wheels
  touch in a run ("aircraft N, ..., touched YSSY 16R", said at the touch),
  the one ahead must have left the runway before it ("has left YSSY 16R,
  stopped beside it"); the checks are counted, and the four-player run (2)
  and the one begun again (1) fail if they made none. Seen red with the
  runway not kept for a player's learnt landing (no vacating): "place 2, the c172p: came down before the one ahead had left the runway" (the run begun again).
- **AI whose plan lands there**:
  `a_player_joining_on_final_behind_an_ai_whose_plan_lands_there_waits_for_it_and_is_at_the_gate_thirty_seconds_after`
  (port 24820): the AI flies tests/data/plans/landing-on-16r-from-6nm.plan
  from 6.2 miles out; the player joins at 90 s on the session's clock (the
  server's `--changed-file` for a METAR of calm again, an event), 2.23
  miles behind it, and waits; she is given her aircraft at 330.0 s, the AI then 1.87 miles on past the bound (it flies its final slower than the bound allows for: on final from 90 s, at its speed of the plan, and the bound reckoned at 60 kt), and is 231 m inside the gate 30 s on (362 s in all). Seen red with plan-landing AI
  not counted: "0 players were told to wait their turn".
- **The other ends' wind**:
  `a_runways_other_end_is_offered_only_into_a_wind_its_learnt_landings_gate_admits`
  (tests/unit/test_learnt.cpp), eight cases: a southerly of 10 kt is 10 kt
  behind 34L, refused, and ahead on 16R, admitted; a northerly the other
  way round; calm admitted; 5 kt straight behind admitted and 5.2 refused;
  16 kt across refused. Seen red with the tailwind not refused: "a southerly
  is refused on 34L for its tailwind".
- **Into the wind on its own other end**:
  `a_player_held_past_the_bound_by_one_orbiting_over_the_start_starts_on_final_to_the_other_end_and_is_at_its_gate`
  (port 24927), in a METAR northerly of 8 kt, the C172P's copilot route
  orbits 1,000 m round the 16R start at 700 ft
  (`tests/data/copilot/route-orbit-over-the-16r-start.txt`), and a PA-28
  joins behind her. At 240.6 s (the bound behind a C172P) she starts 2.35
  miles out on final to 34L, its own other end and most into the wind, and
  is 317 m inside that gate 30 s on; the C172P's client heard it was told
  to leave the final.
- **A crossing runway**:
  `a_player_held_past_the_bound_with_the_wind_across_her_runway_starts_on_final_to_a_crossing_runway_into_it`
  (port 24937), the orbit in a METAR wind from 250 at 10 kt: 25, the
  crossing runway, is the end most into it (16L not clear of the orbit),
  and the PA-28 starts there at 240.6 s, 2.35 miles out, and is 352 m
  inside its gate 30 s on (422 s in all). Tried first with the Cub in 13 kt
  and the C172P's other runways across a southerly: a J-3 Cub left alone
  on final in 13 kt of headwind turned 12 to 18 degrees off the runway's
  heading in 30 s, and a PA-28 started on 25 with 9 kt across drifted 138 m
  - a player is started pointing down the runway, not crabbed into the
  wind, which is a limit of the start for every player in a crosswind,
  named here and not this item's.
- **None will do**:
  `a_player_held_past_the_bound_with_no_other_runway_in_the_wind_waits_told_why_until_the_one_holding_it_goes`
  (port 24828), at Richmond (YSRI), a field of one runway: players on
  final to 10, the C172P orbiting its start
  (`tests/data/copilot/route-orbit-over-the-ysri-10-start.txt`) in a METAR
  wind from 100 at 8 kt, 28 behind her ("held past the bound, and no other
  runway will take her: YSRI 28 8 kt of tailwind; the gate is within 5").
  The PA-28 is told "waiting for aircraft 0 ahead, past its time over where
  she starts: no other runway here will take her", waits, and is given her
  aircraft at 420.1 s, when the C172P's client goes, 319 m inside the gate
  30 s on. The run holds that the server moved nobody, and ends by
  `--until-empty` once both have gone (452 s).
- **Seen red, the three past the bound**: with no other runway end tried,
  the 34L run "place 2, the pa28: not started where her gate holds her, on
  final to 34L" and the 25 run "... on final to 25"; with the wind not
  refused there, the Richmond run sent her to 28: "... on final to 10".
- **Never moved, asserted**: in each past-the-bound run the orbiting
  player's client tracks her (`--track`), and every two updates heard of
  her must be within 200 m (she flies 2 m between two); the run fails if
  fewer than 1,000 were heard. Seen red with a person's aircraft put 0.03
  degrees north once, in the Richmond run: "the one orbiting jumped
  between two updates (5363129 square metres)".
- **Every run ends by `--until-empty`**: the server's "everybody who joined
  has gone" is required of each.
- `glideslope_cli selftest`, linux-release: hash 197366fd3fe6a7dc before the
  runway kept for a player's learnt landing, 197366fd3fe6a7dc, unmoved after - the
  selftest flies no learnt landing.
- Coverage is asserted across the eight runs: every aeroplane with a learnt
  landing follows at least once, and the second, third and fourth places
  are flown. The server-timeouts check passes with its totals unchanged.
  The other learnt-landing tests pass with the runway now cleared: 109
  tests matching learnt, on_final, gate, ports, flags and usage, and the
  66 window and client tests, with the window now opened before joining
  (two skipped: they need a display, which these runs do not have).
