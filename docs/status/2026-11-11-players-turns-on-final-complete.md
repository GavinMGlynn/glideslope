### Players' turns on final: the window, AI leaders, the runway, the bound, 2026-11-11

**What is still not true first.** **In wind past her measured limits a player
is not put on final** (the owner's ruling, 2026-11-11): she starts where the
plan does, told why: past 15 kt ahead, 5 behind or 3 across, every
aeroplane alike since the re-sweep of 2026-11-14 (below). A crabbed start
is not measured on her trim in her wind (below; a Phase 12a tail). **A
measured limit, which the owner's
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
That was at 15 kt only, and before 2026-11-12, when she was trimmed in still
air and met the wind on her first step: a crabbed start on her trim in her
wind is not measured, and whether it would lift her 3 kt across is the
Phase 12a tail "A crabbed start on final, measured on the trim in her
wind".

Then swept in light wind, the same way, each speed one run:

| | ahead | behind | across from the right | across from the left |
| --- | --- | --- | --- | --- |
| C172P | 5, 8 at the gate; 10, 13 not (heading 5 to 7 off) | 2, 4 at it | 1, 2 at it; 3 to 10 not (heading 5 to 12 off) | 1 to 8 at it; 10 not |
| PA-28 | 5, 10, 13 at it | 2, 4 at it | 3, 5, 6 at it; 7 not (60 m off) | 3 at it; 4 not (62 m off) |
| J-3 Cub | 3, 5, 8, 10 at it | 2, 4 at it; 5 not | 1 not (heading 11 off) | 1 not (heading 7 off) |
| C182 | 1, 2, 3, 5 not (heading 6 to 7 off) | 1, 2 not, 4 at it | 1, 2 not | 1, 3, 5 at it |

**Then every whole knot up to each limit**, ahead, behind and across from
either side, one run each (57 runs), and one quartering wind each with both
parts near their limits: every knot held but the Cub at 5 ahead (heading 5
off, where 3, 4, 8 and 10 held) and the C172P quartering - 6 kt from 187
(5.7 ahead, 1.95 across) and 7 from 184 failed (heading 5 off), 5 from 190
(4.7 ahead, 1.97 across) and 4 from 194 held; the PA-28 quartering at 13
from 181 (12.7 ahead, 2.9 across) held; the Cub at 4 kt from 174, 0.4
across, held, but is refused (below).

**Her limits**, kept in her figures (`on_final_headwind_kts`,
`on_final_tailwind_kts`, `on_final_crosswind_kts`), the highest at which
every whole knot below it held: C172P 5 ahead (8 held alone; 5 for a wind
both ahead and across), 4 behind, 2 across; PA-28 13, 4, 3; J-3 Cub calm
only - 1 to 4 kt ahead and behind held, and up to 0.55 kt across from the
left, but 0.21 kt from the right turned her 6 degrees off, so her crosswind
limit is nought, and a wind exactly down a runway heading 167.9 is no
METAR's whole degree; C182 only where no weather is flown (`on_final_still_air_only`): her heading
wandered 5 to 7 degrees in a knot of almost any wind, and 6 degrees in a
METAR of calm (20 degrees, 1015 hPa), where in still air she stayed 202 m
inside the gate. **Judged strictly**: any wind past a limit, however little,
is outside it - a crosswind limit of nought refuses 0.4 kt across.
**Not swept, and why**: one temperature and pressure (the METAR's 20 degrees
and 1015 hPa, near the standard atmosphere at Sydney: the limits are for the
wind); no gusts (no gust group: her gust factor 0); her model's own loading;
16R at 8 ft only; no half knots or other directions than the five named.

**Re-swept on the corrected start, 2026-11-14.** The limits above were
measured when she was trimmed in still air and met the session's wind on her
first step; since 2026-11-12 (#365, "Every aircraft trims in the session's
air") she starts trimmed and moving with it. Swept again the same way -
the command-line client joined as the window client joins, one run per knot,
on a copy of the data with the limits taken out - knot by knot from 1 up
each way until the first that did not hold, and calm and still air:

| | ahead | behind | across from the right | across from the left |
| --- | --- | --- | --- | --- |
| C172P | 1 to 15 at the gate (236 to 457 m inside) | 1 to 5 (209 to 145 m) | 1 to 3 (223 m); 4 not, 64 m left of the centreline | 1 to 3; 4 not, 61 m right |
| C182 | 1 to 15 (196 to 419 m) | 1 to 5 (169 to 107 m) | 1 to 3 (186 m); 4 not, 65 m left | 1 to 3; 4 not, 60 m right |
| J-3 Cub | 1 to 15 (373 to 594 m) | 1 to 5 (345 to 283 m) | 1 to 3 (361 m); 4 not, 64 m left | 1 to 3; 4 not, 61 m right |
| PA-28 | 1 to 15 (196 to 420 m) | 1 to 5 (170 to 107 m) | 1 to 3 (186 m); 4 not, 64 m left | 1 to 3; 4 not, 61 m right |

All four in calm and in still air were at the gate. 15 ahead and 5 behind
are the gate's own most (LearntGate), so not swept past. Quartering, both
parts near the new limits: 15 kt from 179 (14.7 ahead, 2.9 across) and 5
from 330 (4.8 behind, 1.5 across), every aeroplane at the gate (C172P 452
and 148 m, C182 414 and 110, Cub 590 and 287, PA-28 414 and 111). 128
runs: 30 an aeroplane (still air, calm, 15 ahead, 5 behind, 4 from each
side) and the 8 quartering.

**Her limits now**, every aeroplane alike: 15 ahead, 5 behind, 3 across
(before: C172P 5, 4, 2; PA-28 13, 4, 3; the Cub calm only; the C182 only
where no weather is flown). What failed before was the start, not the
aeroplane: her heading wandered 5 to 11 degrees as the wind reached her on
her first step. What fails now is plain drift: she is started pointing down
the runway, not crabbed, and in 4 kt across drifts 2 m/s, 60 to 65 m off the
centreline in thirty seconds, where the gate is within 60 - the same in all
four. `on_final_still_air_only` is taken out: no aeroplane needs it. The
Richmond run moves to 6 kt from 108: 5 behind 28 is now inside her limit,
so 28 is refused by her gate's tailwind ("YSRI 28 6 kt of tailwind; the gate
is within 5"). The sweep's scripts were scratchpad/on-final-turns-complete/
(sweep.sh), not committed.

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
  ahead, behind, from the right and from the left, at her limit she is on
  final and at the gate 30 s on, and a knot past it she is not put on final
  and her client is told why, once, and refused by the part of the wind
  that is past her limit ("16 kt ahead; she is put on final in 15 ahead at
  most"); quartering winds ahead (15 kt from 179) and behind (5 from
  330). (Before 2026-11-14 also calm, and 0.4 across for the Cub's nought.) The cases that reach
  their verdict are counted against the cases listed: 10 each, since
  2026-11-14 (before: C172P 9, PA-28 9, Cub 6, C182 5). Passed on the new
  limits, linux-release, each about 320 s. Seen red on the new limits: the
  C172P's crosswind limit put at 4, a knot too wide ("c172p, 4 kt from
  the right (inside her limits): not at the gate 30 s on"). The refused-by
  check seen red with the behind cases labelled ahead ("c172p, 6 kt behind:
  refused, but not for the wind ahead"). No case of a limit of nought or
  under the quartering winds' parts is left in it: such limits fail the
  test until their cases are written.
  Before 2026-11-14:
  The C172P at 5 ahead 304 m inside the gate, 4 behind 162 m, 2 across 225
  m either side, quartering 299 m; the PA-28 396, 126, 188 and 187 m,
  quartering 390 m; the Cub in calm, the C182 in still air, 201 m. Seen red: with the limits not applied, all four ("...past her
  limit: put on final"); with each limit a hundredth of a knot tighter, the
  at-limit cases of all three with a limit ("13 kt ahead (inside her
  limits): not put on final") - a wind exactly at a limit never arises, so
  `>=` for `>` alone did not; with half a knot of slack across, as before,
  the Cub's 0.4 across ("put on final"). **Told her own reason, on her
  session**: the reason is kept on the session it was given to, not on the
  aircraft's number, which is given again once that aircraft has gone; in
  each test's first case past a limit a second player joins too, and each
  is told once, never the other's - seen red with every proven session told
  each reason ("told why 2 times, not once").
- **Another end refused by her limit, not her gate**:
  `a_player_held_past_the_bound_whose_crossing_runway_is_past_her_crosswind_limit_but_not_her_gates_waits_told_why`
  (port 24852), the `+circlesouth` run's orbit over 16R's start in 6 kt
  from 168: 34L and 34R 6 behind (her gate's 5), 16L not clear of the
  orbit, and 25 and 07 about 6 across - inside her gate's 15, past her
  limit of 3. The C172P behind is refused each for its own reason, the two
  crossing ends by "kt across; she is put on final in 3 across at most",
  and waits, told why, until the one orbiting goes. Seen red with
  `on_final_wind_refuses` taken off the other ends ("place 2, the c172p: no
  other runway refused": she was started on final to 07).
- **Only ends inside her limits past the bound** (before 2026-11-14; now
  6 kt from 108, refused by her gate - see the top): the Richmond run, a wind
  from 108 at 5 kt, puts its C172P on final to 10 (5 ahead, inside her 5)
  and refuses 28: "YSRI 28 5 kt behind; she is put on final in 4 behind at
  most" - a tailwind her gate admits (5), outside her limit; seen red with
  that check off ("no other runway refused as it should be"). Not the J-3
  Cub: with no crosswind limit she is put on final in no wind that is not
  exactly down the runway, and Richmond's 10 (107.6) is no METAR's whole
  degree. The 34L run is in 3 kt from 348 (inside the PA-28's 4 behind on
  16R), the 25 run a C172P in 2 kt from 250 (inside her 2 across).
- `glideslope_cli selftest`, linux-release: hash 197366fd3fe6a7dc before the
  runway kept for a player's learnt landing, 197366fd3fe6a7dc, unmoved after - the
  selftest flies no learnt landing.
- Coverage is asserted across the nine runs: every aeroplane with a learnt
  landing follows at least once, and the second, third and fourth places
  are flown. The server-timeouts check passes, its totals moved to 70 and
  58 (the wind-limits test's two servers, long ones).
  The other learnt-landing tests pass with the runway now cleared: 109
  tests matching learnt, on_final, gate, ports, flags and usage, and the
  66 window and client tests, with the window now opened before joining
  (two skipped: they need a display, which these runs do not have).
