### The PA-28's learnt landing, 2026-11-04 - part of an item, not done

Phase 10b, "Learnt landings for other aircraft": the PA-28's part. The item
stays open.

**What is still not true first.**
- **The PA-28 has no learnt landing yet.** Her training file is written and
  her training is under way; her policy is not committed.
- **The J-3 Cub has none either.**

**What now works.**
- **tools/rl/aircraft/pa28.txt**: her model, her reference speed - 64.35 kt,
  sim::approach_speeds' 1.3 times her published 49.5 kt stall with 40
  degrees of flap (57 mph CAS, Airplane Flight Manual, Model PA-28-180, the
  same figure as section I of the Cherokee 180 "E" Owner's Handbook) - and
  her fuel, 150 lb a tank as her model starts (full, 25 US gallons). JSBSim
  trims her from every one of the verification's 27 starts, so she is
  trimmed as the C172P is, not flown to her trim as the C182 is.
- **The server test's aircraft with no learnt landing is now the J-3 Cub**:
  `a_players_c172p_handed_to_the_learnt_landing_on_final_is_landed_by_the_server_and_a_j3_cub_and_one_outside_the_gate_are_refused`,
  passed (linux-release, 164 s).
- The C182's 27-start helper in tests/unit/test_learnt.cpp names the
  aircraft it flies, for the PA-28's tests to come.

**Verification** (linux-release, no display): the server test above, and
every C182 and C172P learnt-landing test it shares a file with, 20 of 20
passed.
