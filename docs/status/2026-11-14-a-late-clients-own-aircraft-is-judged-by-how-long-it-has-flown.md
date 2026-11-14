# A late client's own aircraft is judged by how long it has flown

**A red tree, fixed.** `ai_aircraft_keep_flying_with_nobody_connected_and_a_late_client_finds_them_mid_flight`
failed on macOS debug in ci.yml's scheduled run 43513776356 on main
(3c35ac0d): "the client's own aircraft is at -33.90489, away from the plan's
start, though it was made the moment it joined". It had failed in the same way
on macOS debug twice before: run 41471213896 (main, 2026-11-02, -33.90489) and
run 41703342974 (2026-11-03, -33.90487).

**Cause.** The test is timing-dependent. #365 (every aircraft trims in the
session's air) did not cause it. Two failures came before #365, and this test
flies in still air, which #365 leaves alone (`moved_with_the_wind` returns
when there is no wind). The client's own aircraft starts in the air at the
plan's start at 100 kt. The server flies it from the step it makes it, at the
handshake, but sends the first update only once the session is proven and the
next twenty-fifth of a second comes round. The old control allowed 11 m from
the start, which is a fifth of a second at 100 kt. On a quiet machine the gap
is about a tenth of a second (5 to 6 m). A slow runner's handshake took longer
than a fifth of a second, and the aircraft had flown 12 m.

**Reproduced.** I put the client behind `glideslope_impair` with 100 ms each
way (a copy of the test in the scratchpad). The old check then fails as on CI:
"the client's own aircraft is at -33.90484, away from the plan's start".

**Fix.**
- The server's "a player's aircraft starts on layer N" line now ends with "at
  T s on the session's clock".
- The test checks that she was made after the session's first second, not
  with the AI at the start.
- It checks how far north of the start she is, signed, since the plan heads
  her north. That must be no more than the plan's 100 kt can carry her
  between T and the first update's clock, with a fifth added for true
  airspeed up to the top layer, plus 11 m for rounding. It must also be no
  less than four fifths of that speed for the same time, less the same 11 m.
  East of the start she may be no more than the 11 m.
- The floor is there because of a review of #374. A bound on the distance
  alone passed an aircraft made 40 m south of the start: flying north, back
  towards it, she was 23 m from it after 290 ms. The relay makes her fly long
  enough for the floor to lie north of the start.
- It checks that she is nearer the start than either AI.
- It asserts still air, so that her ground speed is her airspeed.
- The test now always joins through the 100 ms relay (at PORT + 12), so the
  slow case is built every run and not left to chance. The relay passes the
  server's output on to standard error, where the test reads the made time.

**Verification.** Built on linux-debug:
- The fixed test passes. A typical run reads "made at 5.100 s, 15
  hundred-thousandths of a degree from it of the 26 it could have flown".
- It passes five and then four repeats under `-j4` load.
- With a deliberate bug (a player started 0.0005 degrees north of the start)
  it fails: "67 hundred-thousandths of a degree from the plan's start, and in
  the 334 ms since it was made ... it could have gone 28". The bug was then
  reverted.
- With the floor added, a typical run reads "made at 5.108 s, 17
  hundred-thousandths of a degree from it of the 2 to 28 it would have
  flown".
- A second deliberate bug started her 0.00036 degrees (40 m) south of the
  start. The test failed: "-19 hundred-thousandths of a degree north of the
  plan's start, and in the 334 ms since it was made ... it would have gone 2
  to 28". The distance-only bound would have passed it (19 against 28). The
  bug was then reverted.
- The test then passed five repeats under `-j4` load.
- `every_fixed_test_port_lies_outside_the_ephemeral_ranges_and_no_two_tests_share_one`
  passes. It caught the first choice, PORT + 2, which was server_leave's
  relay.
