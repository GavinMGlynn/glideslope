### The light aeroplanes, the F-15C, the F-35B and the Learjet rotate at a held four degrees a second; the F-22A not yet, 2026-10-13 — item still open

**What is still not true first.** **The F-22A's rotation is not held**: her
figures give no rate, and she rotates under the old law. **Her own pitch
loop winds up on her wheels** (the entry of #175, round three); it was not
tried again here. JSBSim's `<integrator>` is a PID whose trigger holds
(positive) or zeroes (negative) it, and her stabilator is driven by that
integrator alone, so zeroing it on her nose wheel would leave her no
stabilator to lift it with - a ground mode has to put the stick on the
stabilator directly, as #175's round three did, and that moved her
measured rotation speed from 110 kt to 140. The item stays open for her.

**What landed.**
- **Six more aeroplanes are given a rate of four**, `rotation_degps="4"`
  on their figures' root - **a tuning value**, the top of the rule of thumb
  "three to four degrees per second" for most aircraft (code7700.com,
  "Rotation Rate"; the FAA's Airplane Flying Handbook, chapter 5, asks only
  for a smooth rotation): the C172P, the C182, the PA-28, the F-15C and the
  F-35B, whose manuals give none. **The Learjet 35A's is chosen above what
  is cited**: the Gulfstream G500's flight manual, quoted there, has ten
  degrees in three to four seconds, 2.5 to 3.3 degrees a second; held at
  three (#175) she left 11 kt past her rotation speed at her light
  loading, and at four she leaves 8.6. 3.3 was not tried.
- **The rate loop's gain is each aeroplane's own, and it reads her pitch
  rate smoothed.** `rotation_stick_per_degps` on her figures' root is the
  stick asked per degree a second off the rate (read into
  `DepartureSpeeds::rotation_stick_per_degps`; 0.3, the airliners', where
  none is given), and the loop reads her pitch rate through a first-order
  filter of two and a half steps (`rate_filter_steps`). The C172P's is a
  tenth, the PA-28's 0.15 - tuning values, found by flying them. **Why**:
  held at four with 0.3 read step by step, the C172P's stick rang from
  stop to stop twenty times a second on her wheels - a limit cycle of a
  loop whose gain, with one step's lag, is too high for her elevator - and
  where the swing stood as her wheels left, she was handed over with the
  stick at its stop; `Departure::from_lift_off` took it as trim, her nose
  went from 7.9 to 12.6 degrees and she struck the runway again. Turns of
  her stick on her wheels (below), filtered, against the gain: 39 and 34 at
  0.3, 8 at 0.15, 3 and 5 at 0.1, 1 at 0.06. The PA-28's at 0.3 turned 4
  and 7, at 0.15 3. The C182 at 0.1 rotated at 5.2 deg/s at her model's
  loading, so she keeps 0.3 (4 turns).
- **The slew limit of #181's first commit is gone.** It bounded the swing
  (two of the stick's travel a second, faster than a hand's pace) but the
  loop still turned back 15 times; with the gain and filter it is not
  needed, and the hand-over tests pass without it. Also tried and dropped
  in that commit: bounding the rate's part of the stick at a quarter
  either way, or on the pull side alone (the A320 at her light loading came
  up at 4.33 and 4.15), and a six-step mean of the pitch rate at 0.3 (the
  C172P's stick at its stop on 222 of her 316 held steps).
- **The test**
  (`every_aeroplane_given_a_rotation_rate_rotates_on_her_wheels_at_every_loading_no_faster_than_it`)
  flies twelve aeroplanes at 44 loadings, four named and left out (the
  flying boat, the two tail-wheel aeroplanes, the F-22A), and **now counts
  the stick's turns on her wheels**: each step it moves more than 0.002 the
  other way from its last move, once the rotation has begun. No loading
  may turn more than eight times; every one turns at most five (the C172P
  at her figures' loading). **Seen red** on the loop of #181's first commit
  (gain 0.3, read step by step): the C172P turned 15 and 10 times with its
  slew limit in, 62 and 56 without. Peak rates, against four and the
  degree's tolerance: the F-35B 4.43 at her vertical-landing loading (8.6
  under the old law), the C182 4.38 at her model's (8.2), the F-15C 4.37,
  the Learjet 4.32 at her light loading (8.6), the PA-28 4.14, the C172P
  3.93. The worst of each airliner and the B-2A moves by at most 0.02 deg/s; theirs
  is still the 747-400 light, 3.89. Each leaves at least 3.7 degrees below
  her strike attitude. Past her rotation speed at lift-off: the Learjet at
  her light loading 8.6 kt, the F-35B 6.4 to 7.8, the F-15C 5.4 to 6.8, the
  light aeroplanes 1.1 to 4.6.
- **A figures-parser test**,
  `the_rotation_loops_stick_per_degree_a_second_is_read_and_refused_outside_0_to_1`:
  the C172P's tenth read, the A320's none read as nought, 1 read back, and
  0, -0.1 and 1.5 refused, each naming the attribute. Seen red with the
  C172P's file given 0.2. `the_rotation_rate_a_take_off_is_held_to_is_read_and_refused_outside_0_to_10`
  reads the F-22A's figures for "none" now, the C172P's having one.
- **Seen red, the hand-over**: with the C172P given four on the 0.3 loop
  and no slew limit,
  `every_landplane_handed_over_at_lift_off_climbs_away_through_500_ft_and_never_touches_the_runway`
  and `the_cessnas_handed_over_at_any_half_second_of_the_ten_after_lift_off_climb_away_and_never_touch_the_runway`
  failed ("c172p handed over at lift-off touched the runway again"); now
  they pass.
- **Measured take-off speeds do not move**: only the 747-400 and the F-22A
  have them, and neither is changed;
  `the_measured_take_off_speeds_in_the_747_and_f22_figures_are_what_the_measurement_makes`
  passes.

**Verification.** linux-release, the server and client built: every test
matching "take|rotat|lift|hand|depart|climb_away|strike|lesson|fault|selftest",
189 tests, 179 pass, one failed (below) and nine are skipped (eight for want of a language
model's key, one Windows-only); the new parser test, run after it was registered, passes. The one,
`a_players_aircraft_handed_to_the_ai_in_the_air_is_planned_by_claude_and_flown_as_recorded`,
failed once in 2.3 s in the parallel run and passed twice alone (77 s); not
traced - another worktree's tests were running at the same time. The
selftest, run: 182dd6c996e0ee4c, unmoved.
