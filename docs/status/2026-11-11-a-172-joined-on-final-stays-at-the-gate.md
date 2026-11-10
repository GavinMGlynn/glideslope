### A 172 joined on final stays at the gate, 2026-11-11

**What is still not true first.** Only the four aeroplanes with a learnt
landing (C172P, C182, J-3 Cub, PA-28) have a gate to stay at; the other
twelve are named in the test as having none. The verdict is taken once,
thirty seconds in, when L asks for the learnt landing: the gate is not
judged at every step between. Run here on linux-release only; the change
to the server is an option's parsing and a line it prints, nothing
platform-specific.

**Why she left the gate.** Measured, not argued. The plan item (from
2026-10-08) said the client kept the server's flaps and throttle but not its
pitch trim. Since protocol 0F (2026-10-21,
`frontend::keep_held`) every update carries her pitch, roll and yaw trim and
both clients begin a player's inputs from them. To learn which of them was
why, the window client was run on today's code with each put back to what
it sent before 0F - nought - and her prediction traced (`--trace`), a C172P
joined 2.35 miles out on final to YSSY 16R (runway heading 167.9) and left
alone:

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

- `--players-on-final AIRPORT/RUNWAY[@MILES]`: the first player starts
  MILES out (0.5 to 10; two, the gate's middle, as before, unless said), each
  after half a mile further. From two miles she flies out of the gate's near
  edge (1.7 miles) in about seventeen seconds, however well she holds the
  rest, so the test starts her at 2.35. `--dry-run` says the distance; the
  server says where each player on final starts ("slot 0's c172p starts on
  final to YSSY 16R 2.35 miles out").

**Verification.**

- `every_aeroplane_with_a_learnt_landing_joined_on_final_and_left_alone_is_still_at_its_gate_thirty_seconds_later`
  (tests/cmake/client_stays_at_gate.cmake, port 24890 and its relay at
  24891): for each aeroplane with a policy in the data, a server of its own
  starts her 2.35 miles out; the window client joins in her, presses
  nothing but L thirty seconds in (asserted: no other key), and the server
  must hand her to the learnt landing - its own gate check, on its own
  aircraft. Coverage asserted: the policies found must be exactly the four
  flown, and those four with the twelve named as having no gate must be the
  whole catalogue of 16. linux-release, 173 s: all four handed over at 30 s.
  Seen red three ways: with the client's roll and yaw trim put at nought
  ("c172p: ... not at the learnt landing's gate: YSSY 25: 0.4 miles past"),
  with its pitch trim at nought ("0.8 miles past"), and with the PA-28 left
  off the test's list ("the aeroplanes with a learnt landing are
  c172p;c182;j3cub;pa28, and this test flies c172p;c182;j3cub").
- Without the new start distance, from two miles, the C172P was
  refused at 30 s for the distance alone ("1.5 miles out"), which is why the
  option exists.
- No protocol change: the version stays where it is.
