### Players joining on final take turns in the gate, 2026-11-11

**What is still not true first.** **A player whose turn has not come is
not told why**: the server gives her no aircraft, and her updates say so
(`no_aircraft`, `FF`), as they always could; no message says she is
waiting for the final, and the window client says only that the server has
given it no aircraft yet. **Only players' aircraft are counted**: an AI
aircraft on the same final (`--ai-on-final`) neither holds a player back
nor is held back by one. **A player ahead who leaves the approach without
landing holds the next back** for as long as she flies within three miles
of the start. **Nothing clears the runway**: a player released when the
one ahead lands may come down the final to find her stopped on it (the
plan's runway tail). The tests fly the command-line client joined as the
window client joins (`--predict --from-held`); the window client's own
waiting is compiled and its join changed, but no test runs a window client
that waits. Run here on linux-release only.

**What changed.**

- **Every player on final starts where her gate holds her thirty seconds**
  (2.26 to 2.35 miles, as the first did), in her turn: a player joining
  while another player's aircraft flies within three miles of where she
  would start is admitted with no aircraft and waits, oldest first, one
  given a step, until every such aircraft is three miles from there or on
  the ground. **Three miles** is the radar minimum in trail on a final
  approach (FAA JO 7110.65, 5-5-4); between two light aeroplanes no wake
  category adds to it, and the 2.5 miles it allows inside ten miles of the
  runway needs runway occupancy of 50 s or less, which nothing here
  measures. **Or on the ground**: a start 2.3 miles out leaves the one ahead
  stopped on the runway short of three miles from it. Until now the second,
  third and fourth started 2.9, 3.4 and 3.9 miles out, past the gate's far
  edge (2.4). The server says "slot 1 waits for its turn on final: aircraft
  0 is 0.10 miles from where she starts on final, and flying ..." and "slot
  1's turn on final: aircraft 1, 148.9 s on the session's clock".
- **The window client waits**: it gave up joining ten seconds after it
  began; now ten seconds after it last heard the server, saying once that
  the server has given it no aircraft yet.
- **glideslope_cli**: `--learnt-landing-at S` counts from when its aircraft
  is given, not from joining (the same for every test that is given one at
  once); `--until-answered` leaves once the learnt landing is handed over
  or refused.
- docs/TRANSPORT.md says a server may admit a player with no aircraft until
  her turn comes; no byte changes, and the version stays.

**Verification.**

- `four_players_joining_on_final_a_c172p_c182_cub_and_pa28_take_turns_and_each_is_at_the_gate_thirty_seconds_after_she_starts`
  and
  `a_c172p_joining_on_final_behind_a_pa28_waits_her_turn_and_is_at_the_gate_thirty_seconds_after_she_starts`
  (tests/cmake/server_players_take_turns_on_final.cmake, ports 24834 and
  24848): each player joins once the one before is admitted, touches
  nothing, asks for the learnt landing 30 s after her aircraft is given,
  and must be handed over at least 100 m before the gate's near edge; each
  after the first must have been told to wait and given her aircraft only
  after the one ahead was handed over. Coverage asserted across the two
  runs: every aeroplane with a policy in the data follows another at least
  once, and the second, third and fourth places are flown.
  linux-release, 529 s and 174 s:

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
