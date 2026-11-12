### Players joining on final take turns in the gate, 2026-11-11

**What is still not true first.** The item stays open for these.
**The window client's window does not open while she waits**: it waits
in its join, before its window, and says why in its output - the
server's reason - but shows nothing on a screen until her turn. **Only
players' aircraft are counted**: an AI aircraft on the same final
(`--ai-on-final`) neither holds a player back nor is held back by one.
**Nothing clears the runway**: a player let by when the one ahead lands may
come down the final to find her stopped on it (the plan's runway tail). A
player let by at the bound (waited out) may start within three miles of
one still flying her final. Run here on linux-release only; the Windows
cost figures in tests/ci_costs are copied from Linux, not measured.

**What changed.**

- **Every player on final starts where her gate holds her thirty seconds**
  (2.26 to 2.35 miles, as the first did), in her turn: a player joining
  while another player's aircraft flies within three miles of where she
  would start is admitted with no aircraft and waits, oldest first, one
  given a step, until every such aircraft has let her by
  (`sim::turn_on_final`):
  - **3 miles on** from her start: the radar minimum in trail on a final
    approach (FAA JO 7110.65, 5-5-4); between two light aeroplanes no wake
    category adds to it, and the 2.5 miles allowed inside ten miles of the
    runway needs runway occupancy of 50 s or less, which nothing here
    measures;
  - **landed** (5 ft or less over the ground): a start 2.3 miles out leaves
    the one ahead stopped on the runway short of three miles from it;
  - **gone around**: 100 ft over where she started on final - on final she
    only comes down. A tower sequences the next arrival behind a go-around
    afresh, and its separation from her is the tower's to make (FAA notice
    N JO 7110.531 on go-around separation; no rule fixes a follower's
    spacing behind one);
  - **off the final**: 1.5 miles across its course, the lateral minimum the
    server keeps aircraft apart by;
  - **waited out**: the one ahead on final as long as three miles take her
    at her approach speed, and a minute more (240.6 s behind a C172P, 311.7
    s behind a J-3 Cub), **and 1.5 nm or more from where the next would
    start**, the lateral minimum: past the bound the next is never put on
    top of her, and waits on for one of the others. So the wait is bounded
    by the one ahead landing, climbing away, leaving the final, or being
    1.5 nm on once the bound has passed - not by the clock alone: one who
    circles over the start holds the next for as long as she does.
- **She is told why**: a `TRAFFIC_INSTRUCTION` to aircraft `FF` - hers, as
  she has none - once a session: "waiting for the aircraft ahead: aircraft
  0, 0.02 miles from where she starts, until it is 3 miles on or landed, at
  most 228 s more" - and again when the aircraft she waits on changes. The
  window client says it while it waits; the server
  says what let her by ("slot 1's turn on final: aircraft 1, 142.2 s on the
  session's clock: aircraft 0 landed, 2.52 miles from her start").
- **A session begun again on her key while she waits keeps her turn and
  her place** (her first session's admission time), and her aircraft, when
  it comes, is every session of her key's.
- **The window client waits**: it gave up joining ten seconds after it
  began; now ten seconds after it last heard the server.
- **glideslope_cli**: `--learnt-landing-at S` counts from when its aircraft
  is given, not from joining (the same for every test that is given one at
  once); `--until-answered` leaves once the learnt landing is handed over
  or refused.
- docs/TRANSPORT.md: a player admitted with no aircraft, and an instruction
  to `FF`; no byte changes, and the version stays.

**Verification.**

- `four_players_joining_on_final_a_c172p_c182_cub_and_pa28_take_turns_and_each_is_at_the_gate_thirty_seconds_after_she_starts`,
  `the_window_client_joining_on_final_behind_a_pa28_waits_told_why_and_is_at_the_gate_thirty_seconds_after_she_starts`
  and
  `a_player_begun_again_on_her_key_while_she_waits_her_turn_on_final_keeps_it_and_is_at_the_gate_thirty_seconds_after`
  (tests/cmake/server_players_take_turns_on_final.cmake, ports 24834, 24887
  and its relay, 24897): each player joins once the one before is admitted, touches
  nothing, asks for the learnt landing 30 s after her aircraft is given,
  and must be handed over at least 100 m before the gate's near edge; each
  after the first must have been told to wait and given her aircraft only
  after the one ahead was handed over. Coverage asserted across the two
  runs: every aeroplane with a policy in the data follows another at least
  once, and the second, third and fourth places are flown. Each after the
  first must be let by at touchdown or three miles on, never sooner. The
  second run's follower is the window client (`glideslope --headless`, as
  every window test runs it), which must say it has no aircraft yet, the
  server's reason, and then hers, and press L 30.0 s after; the third's is
  the command-line client begun again on its key while it waits, its first
  session going silent, after a third player (a C182) has joined behind it:
  it must be given her, and before the third. linux-release, 529 s, 185 s
  and 174 s (the window and restart runs' C172P 2.33 to 1.818 and 1.829
  miles, 218 and 238 m to spare, each given at 142.2 s when the PA-28
  landed 2.52 miles from her start):

  | run | place | aeroplane | given her aircraft | starts | 30 s in | before the near edge |
  | --- | --- | --- | --- | --- | --- | --- |
  | 0 | 1 | C172P | at once | 2.33 miles | 1.829 | 238 m |
  | 0 | 2 | C182 | 148.9 s | 2.35 | 1.809 | 202 m |
  | 0 | 3 | J-3 Cub | 293.4 s | 2.26 | 1.901 | 372 m |
  | 0 | 4 | PA-28 | 497.1 s | 2.35 | 1.809 | 202 m |
  | 1 | 1 | PA-28 | at once | 2.35 | 1.808 | 200 m |
  | 1 | 2 | C172P | 142.2 s | 2.33 | 1.828 | 238 m |

  Each was given her aircraft when the one ahead touched down - about 145
  s after that one started - and each before her landed under 170 ft/min
  (C172P 167, C182 149, Cub 122). Seen red on the old start (later players
  2.9 miles out and no waiting, put back for the run): "0 players were told
  to wait their turn, and 1 join behind another"; that run's C172P started
  2.90 miles out and was 2.394 miles out 30 s on - inside the gate by 11 m
  of its far edge, where the third, 3.4 miles out, would not have been.
  Seen red with three miles made one: "place 2, the c172p: let by at 1.00
  miles" (the window run), and the unit test's "one mile on: 3 miles on". Seen
  red with the waiting not carried to a session begun again on its key: "the
  session begun again was never given her aircraft" (its time cut to 250 s
  for the run).
  Seen red too with the follower's turn read greedily (the PA-28's number
  taken for the Cub's: "given her aircraft before the one ahead was handed
  over"), which is why each player is read from where she is given her
  aeroplane on.
- `a_players_c172p_handed_to_the_learnt_landing_on_final_is_landed_by_the_server_and_an_f22a_and_one_outside_the_gate_are_refused`
  now waits its turns: the F-22A is given her aircraft once the C172P is
  down and refused for having no learnt landing; the third, a C172P, asks
  50 s after her turn and is refused 1.5 miles out, flown past the gate.
  The C172P's landing is unchanged: 167 ft/min, +2.02 m across, stopped
  600 m along, -0.05 m across. 204 s.
- `the_next_player_on_final_waits_until_the_one_ahead_is_three_miles_on_landed_gone_around_off_the_final_or_waited_out`
  (tests/unit/test_learnt.cpp): fourteen cases, each way of letting her by
  at its edge and just short of it, one mile on held, the bound pinned
  (240.6 s behind a C172P, 311.7 s behind a J-3 Cub), and past it held
  still with the one ahead 2,700 m on or over the start. Red with three
  miles made one.
- **Thirty seconds on the session's clock, not the client's** (from CI run
  42930877921, Windows debug, tests 4/8): the window run failed "place 2,
  the c172p: only 89 m before the gate's near edge". The window client
  counted `--press-after` from its own first tick, which comes only once
  its flight is built: a slow debug client built hers about 4 s after the
  server gave her (89 m against 220 m here is 131 m, 4.2 s at her 31 m/s),
  and asked 30 s after that. Reproduced here with the client held up 5 s
  on purpose (`--slow-start 5`): 66 m, failed. Now, on a server, its
  seconds after joining are the session's clock from the step the server
  first said she was its own (`Joined::given_steps`) to the newest it has
  said; with `--slow-start 5`, 238 m to spare, as the command-line client's.
  The window run keeps `--slow-start 5`, so that a slow client is the case
  it tests. **What moved is `--press-after` alone**, for every window test
  on a server (#357's at-the-gate test and the learnt-landing window test
  among them): the client's other test timings (`--copilot-after`,
  `--learnt-landing-after`, `--hand-over-after`, `--stall-after`,
  `--take-back-after`, `--next-model-after`, `--next-aircraft-after`) stay
  on its own ticks, since on the session's clock a slow client would catch
  up on them at once and send on consecutive passes what they space out.
  **A take-over or a joining again keeps the step her presses count from**
  (`joined = *taken` put it to nought): the window client says when it was
  given ("given at 0.042 s on the session's clock") and what each press
  counted from, and
  `the_client_with_the_window_takes_over_the_ai_aircraft_it_rides_along_in`
  presses K 12 s after joining, after its take-over, counted from its
  given time - seen red with the step put to nought on the take-over
  ("counted from 0.000").

  From the review of the clock round: that take-over test now refuses a
  given time of nought, where a clock reset across the take-over could not
  be told apart (she is given at 0.042 s, and "12.[0-4]" alone cannot see a
  42 ms move), and matches the given time escaped, not as a pattern. The
  guard was checked on 0, 0.000, 0.042 and 12.500, and the escaped match
  refuses a time differing only where the dot was.
