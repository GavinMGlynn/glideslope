### Players' turns on final: the window, AI leaders, the runway, the bound, 2026-11-11

**What is still not true first.** **In wind past her measured limits a player
is not put on final** (the owner's ruling, 2026-11-11): she starts where the
plan does, told why; the C182 is put on final only in calm. **A measured limit, which the owner's
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

**Left alone in wind, she leaves the gate: so she is put on final only in
light wind** (the owner's ruling, 2026-11-11; REQUIREMENTS 6.5 and section
9). First measured with the command-line client joined as the window client
joins (`--predict --from-held`), one player on final to 16R, the server's
verdict 30 s on, at the gate's wind limits (15 kt ahead, 15 across - 12 for
the Cub - 5 behind), started three ways: down the runway; crabbed by the
whole wind correction angle, asin(crosswind / airspeed); crabbed at most 4.5
degrees. None held: the C172P 10 degrees off heading or 147 m off the
centreline in 15 across, the C182 189 m off in 15 straight ahead, the PA-28
143 to 201 m off across, the Cub 12 degrees off even straight ahead; the
gate's 5-degree heading limit is less than her crab at 6 kt across. The
crabbed start was taken out. Searched: the wind correction angle, and
FlightGear's `--in-air`, `--offset-distance` and `--wind` starts and
X-Plane's start on final - none crabs an aircraft started on final.

Then swept in light wind, the same way, each speed one run:

| | ahead | behind | across from the right | across from the left |
| --- | --- | --- | --- | --- |
| C172P | 5, 8 at the gate; 10, 13 not (heading 5 to 7 off) | 2, 4 at it | 1, 2 at it; 3 to 10 not (heading 5 to 12 off) | 1 to 8 at it; 10 not |
| PA-28 | 5, 10, 13 at it | 2, 4 at it | 3, 5, 6 at it; 7 not (60 m off) | 3 at it; 4 not (62 m off) |
| J-3 Cub | 3, 5, 8, 10 at it | 2, 4 at it; 5 not | 1 not (heading 11 off) | 1 not (heading 7 off) |
| C182 | 1, 2, 3, 5 not (heading 6 to 7 off) | 1, 2 not, 4 at it | 1, 2 not | 1, 3, 5 at it |

**Her limits**, the most at which every speed swept up to it was at the
gate, kept in her figures (`on_final_headwind_kts`, `on_final_tailwind_kts`,
`on_final_crosswind_kts`, the crosswind the lesser side's): C172P 8 ahead, 4
behind, 2 across; PA-28 13, 4, 3; J-3 Cub 10, 4, 0; C182 still air only
(`on_final_still_air_only`): her heading wandered 5 to 7 degrees in a knot of
almost any wind, and 6 degrees in a METAR of calm (20 degrees, 1015 hPa),
where in still air - no weather flown - she stayed 202 m inside the gate.
Judged to the nearest knot: a knot past a limit is outside it.
Only the speeds swept are known: the PA-28 at 1 and 2 across is not, nor
is any half knot.

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
- **Put on final only in light wind**: a player is put on final only where
  the wind at her start is inside her figures' limits
  (`sim::on_final_wind_refuses`, read into her learnt landing's policy);
  otherwise the server says "slot N's MODEL is not put on final to YSSY 16R:
  N kt across; she is put on final in M across at most; it starts where the
  plan does" and tells her client the same (a `TRAFFIC_INSTRUCTION` to her
  aircraft); she waits for nobody's turn. A runway end chosen past the bound
  must be inside them too, as well as inside her gate's.
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
  (port 24927), in a METAR wind from 348 at 3 kt (3 behind 16R, inside
  the PA-28's limit), the C172P's copilot route
  orbits 1,000 m round the 16R start at 700 ft
  (`tests/data/copilot/route-orbit-over-the-16r-start.txt`), and a PA-28
  joins behind her. At 240.6 s (the bound behind a C172P) she starts 2.35
  miles out on final to 34L, its own other end and most into the wind, and
  is 239 m inside that gate 30 s on; the C172P's client heard it was told
  to leave the final.
- **A crossing runway**:
  `a_player_held_past_the_bound_with_the_wind_across_her_runway_starts_on_final_to_a_crossing_runway_into_it`
  (port 24937), the orbit in a METAR wind from 250 at 2 kt (inside the
  C172P's 2 across on 16R): 25, the crossing runway, is the end most into
  it (16L not clear of the orbit), and a C172P behind starts there at
  240.6 s, 2.33 miles out, and is 262 m inside its gate 30 s on (423 s in
  all).
- **None will do**:
  `a_player_held_past_the_bound_with_no_other_runway_in_the_wind_waits_told_why_until_the_one_holding_it_goes`
  (port 24828), at Richmond (YSRI), a field of one runway: players on
  final to 10, the C172P orbiting its start
  (`tests/data/copilot/route-orbit-over-the-ysri-10-start.txt`) in a METAR
  wind from 108 at 5 kt, straight down 10 and 5 behind 28 ("held past the
  bound, and no other runway will take her: YSRI 28 5 kt behind; she is put
  on final in 4 behind at most"). The J-3 Cub behind is told "waiting for
  aircraft 0 ahead, past its time over where she starts: no other runway
  here will take her", waits, and is given her aircraft at 420.0 s, when
  the C172P's client goes, 441 m inside the gate 30 s on. The run holds that the server moved nobody, and ends by
  `--until-empty` once both have gone (452 s).
- **Seen red, the three past the bound**: with no other runway end tried,
  the 34L run "place 2, the pa28: not started where her gate holds her, on
  final to 34L" and the 25 run "... on final to 25"; with the wind not
  refused there, the Richmond run sent her to 28: "... on final to 10"
  (with the PA-28 and the winds before this round's; the runs were not
  seen red again with the Cub and the C172P restored).
- **Never moved, asserted**: in each past-the-bound run the orbiting
  player's client tracks her (`--track`), and every two updates heard of
  her must be within 200 m (she flies 2 m between two); the run fails if
  fewer than 1,000 were heard. Seen red with a person's aircraft put 0.03
  degrees north once, in the Richmond run: "the one orbiting jumped
  between two updates (5363129 square metres)".
- **Every run ends by `--until-empty`**: the server's "everybody who joined
  has gone" is required of each.
- **Her limits, both ways, every aeroplane**:
  `a_c172p_is_put_on_final_only_inside_her_measured_wind_limits_and_at_her_gate_thirty_seconds_on_inside_them`
  and the C182's, the J-3 Cub's and the PA-28's (tests/cmake/server_on_final_wind_limits.cmake,
  ports 24810, 24818, 24830, 24836): the limits read from her figures;
  ahead, behind, from the right and from the left, at her limit (calm for
  a limit of nought) she is on final and at the gate 30 s on, and a knot
  past it she is not put on final and her client is told why - 8 of 8
  cases each, counted, 32 in all, about 247 s each: the C172P at 8 ahead
  352 m inside the gate, 4 behind 160 m, 2 across 223 m either side; the
  PA-28 394, 124, 185 and 186 m; the J-3 Cub 512, 298, 358 and 359 m; the
  C182 in still air 200 to 201 m - and each a knot past, not put on final
  and told why ("9 kt ahead; she is put on final in 8 ahead at most"; the
  C182 in any METAR, "she is put on final only in still air"). Seen red
  with the limits not applied: all four, "...past her limit: put on final".
  A refusal told to a client whose session is not yet proven was dropped
  until now; it is kept until it is.
- **Only ends inside her limits past the bound**: the Richmond run, a wind
  from 108 at 5 kt, puts its J-3 Cub on final to 10 (5 ahead, inside her
  10) and refuses 28: "YSRI 28 5 kt behind; she is put on final in 4 behind
  at most" - a tailwind her gate admits (5), outside her limit. The 34L run
  is now in 3 kt from 348 (inside the PA-28's 4 behind on 16R), the 25 run a
  C172P in 2 kt from 250 (inside her 2 across), the Richmond run a J-3 Cub
  again - the Cub and the C172P runs restored.
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
