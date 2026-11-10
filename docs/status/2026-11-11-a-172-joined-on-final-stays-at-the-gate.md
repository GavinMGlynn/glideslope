### A 172 joined on final stays at the gate, 2026-11-11

**What is still not true first.** **Only the first player on final starts
in the gate**: the players after her start 2.9, 3.4 and 3.9 miles out,
outside its far edge (2.4 miles), and a person who joins second is not at
the gate thirty seconds later. Half a mile apart, two cannot both fit in its
0.7 of a mile and each still be in it thirty seconds on; starting each later
one where the first started, once the one ahead has gone on, is a delayed
start the server does not have. A plan item of its own, "A second player
joining on final starts outside the gate". Only the four aeroplanes with a learnt
landing (C172P, C182, J-3 Cub, PA-28) have a gate to stay at; the other
twelve are named in the test as having none. The verdict is taken once,
thirty seconds in, when L asks for the learnt landing: the gate is not
judged at every step between. "The window client" is the window client's
own program, `glideslope`, run `--headless` with a Vulkan driver, as every
window test runs it: not a person at a window. Run here on linux-release
only; the change is in the server's start on final and the simulation's
gate, nothing platform-specific.

**Why she left the gate.** Measured, not argued. The plan item (from
2026-10-08) said the client kept the server's flaps and throttle but not its
pitch trim. Since protocol 0F (2026-10-21,
`frontend::keep_held`) every update carries her pitch, roll and yaw trim and
both clients begin a player's inputs from them. To learn which of them was
why, the window client was run on today's code with each put back to what
it sent before 0F - nought - and her prediction traced (`--trace`), a C172P
joined 2.35 miles out on final to YSSY 16R (runway heading 167.9) and left
alone:

(These runs started her 2.35 miles out, `@2.35`, so that the gate's
distance did not refuse her first.)

| client's first input | 6 s: heading, pitch, bank, kt | 30 s: heading, bank | the server at 30 s |
| --- | --- | --- | --- |
| every trim kept (today) | 167.86, -0.32, 0.03, 59.8 | 167.52, -0.07 | at the gate, handed over |
| no pitch trim | 169.93, -9.57, 3.05, 71.1 | 227.75, 13.1 | refused: 0.8 miles past YSSY 25 |
| no roll or yaw trim | 158.98, -0.93, -11.4, 59.9 | 294.50, -44.5 | refused: 0.4 miles past YSSY 25 |

So the 8 degrees off the runway's heading six seconds in was the missing
roll and yaw trim (8.9 degrees here), which the server's lateral trim finds
and 0F carries; without the pitch trim she would have left the gate too,
nose down and 11 kt fast. Nothing was left to fix in the client.

**What changed.**

- **A player on final with no distance said starts where her gate holds her
  thirty seconds** (`sim::learnt_gate_start_m`), not at the gate's middle:
  in thirty seconds she flies at most her vref plus the 8 kt the gate admits
  over it, and the start is halfway between the near edge (1.7 miles) that
  far out and the far edge (2.4) - as much to spare at either end. C172P
  2.333 miles, C182 and PA-28 2.351, J-3 Cub 2.262. From two miles, where
  every player started until now, the C172P was refused 30 s in for the
  distance alone ("1.5 miles out"): she leaves the near edge in about
  seventeen seconds however well she holds the rest. An aeroplane with no
  learnt landing, or one at a weight it was not trained at, starts two
  miles out as before.
- **The players after the first are half a mile apart from half a mile past
  the furthest the first can be** (the gate's far edge, 2.4 miles, or the
  distance said): 2.9, 3.4 and 3.9 miles, where they were 2.5, 3.0 and 3.5.
  Counted from the first player's own start, the F-22A in
  server_learnt_landing.cmake was put 0.22 of a mile behind its C172P and
  flew into her (both wrecks; the test timed out at 600 s).
- `--players-on-final AIRPORT/RUNWAY[@MILES]`: the first MILES out (0.5 to
  10), kept for a test that wants a set distance. `--dry-run` says which;
  the server says where each player on final starts ("slot 0's c172p starts
  on final to YSSY 16R 2.33 miles out"), and, handing one to the learnt
  landing, how far out she is and how far before the gate's near edge
  (`sim::learnt_gate_along_m`).

**Verification.**

- `every_aeroplane_with_a_learnt_landing_joined_on_final_and_left_alone_is_still_at_its_gate_thirty_seconds_later`
  (tests/cmake/client_stays_at_gate.cmake, port 24890 and its relay at
  24891): for each aeroplane with a policy in the data, a server of its own
  with `--players-on-final YSSY/16R` and no distance; the window client
  joins in her, presses nothing but L, by 30.5 s (asserted: "pressed L,
  30.[0-4] s in", on the flight's clock since joining, and no other key),
  and the server must hand her to the learnt landing - its own gate check,
  on its own aircraft - at least 100 m before the gate's near edge.
  Coverage asserted: the policies found must be exactly the four flown, and
  those four with the twelve named as having no gate must be the whole
  catalogue of 16. linux-release, 166 s:

  | aeroplane | starts | 30 s in | before the near edge |
  | --- | --- | --- | --- |
  | C172P | 2.33 miles | 1.817 miles | 218 m (6.9 s at her 59.8 kt) |
  | C182 | 2.35 | 1.797 | 179 m (5.4 s at 64.35 kt) |
  | J-3 Cub | 2.26 | 1.892 | 356 m (16.1 s at 42.9 kt) |
  | PA-28 | 2.35 | 1.797 | 180 m (5.4 s at 64.35 kt) |

  Those margins are what the press's clock offset has to fit inside: the
  press is asserted by 30.5 s, more than four seconds inside the least.
  Seen red four ways: with the client's roll and yaw trim put at nought
  ("c172p: ... not at the learnt landing's gate: YSSY 25: 0.4 miles past"),
  with its pitch trim at nought ("0.8 miles past"), with the PA-28 left
  off the test's list ("the aeroplanes with a learnt landing are
  c172p;c182;j3cub;pa28, and this test flies c172p;c182;j3cub"), and with
  the start put back at the gate's middle ("the server did not start her
  where her gate holds her").
- `every_learnt_landings_default_start_on_final_is_inside_its_gate_and_still_inside_it_thirty_seconds_later_at_its_fastest`
  (tests/unit/test_learnt.cpp): the catalogue walked, each aircraft with a
  learnt landing found as a session finds it, counted against the policy
  files in the data (4 of 4): each inside the gate at the start
  and still inside it 30 s later at vref + 8 kt - 125, 90, 255 and 90 m to
  spare (C172P, C182, Cub, PA-28); 248, 213, 379 and 213 m at vref. Red
  with the start put back at the gate's middle, and with the C182 skipped
  in the walk ("every learnt landing in the data walked: 3 of 4").
- Every other test that uses `--players-on-final` passes:
  `a_players_c172p_handed_to_the_learnt_landing_on_final_is_landed_by_the_server_and_an_f22a_and_one_outside_the_gate_are_refused`
  (its C172P now starts 2.33 miles out, not 2.0: 167 ft/min, +2.02 m
  across, stopped 600 m along, -0.05 m across; on 2026-10-08 from two
  miles, 214 ft/min, +1.37 m, 463 m; the third player, 3.4 miles out, is
  refused for distance as before - its comments and its pattern now say
  2.33 and 3.4 miles), and
  `the_client_with_the_window_on_final_is_refused_the_learnt_landing_with_its_flaps_moving_told_why_and_handed_to_it_with_l`
  (167 ft/min, +2.02 m across, stopped 600 m along, -0.04 m across); the
  server's flag and usage tests and the test-ports check too.
- tests/ci_costs: the new window test at 166 s in linux-release,
  windows-release and windows-clang (where its sibling window test runs),
  the unit test at 1 s in the six presets that run the learnt-landing unit
  tests.
- No protocol change: the version stays where it is.
