### One handed to the AI between two layers is turned away; one handed over in a spiral is rolled level before it is pulled, 2026-10-09 — item still open

**Port, fixed after CI.** The spiral hand-over test first took port 24704,
which the window client's stalled-rejoin test already uses as its port + 1;
`every_fixed_test_port_lies_outside_the_ephemeral_ranges_and_no_two_tests_share_one`
refused it on every platform (CI run 37891046392). It is on 24940 now.

**What is missing first.** **Arriving between two layers inside 1.5 nm of
one, an aircraft is under the minimum until it is 1.5 nm away**: no height
in a 1,000 ft gap is more than 500 ft from both, so until it has turned
away that far it is at the minimum at best - 38.9 s in the built case
below, against a stated bound of 44 s, and never after 44 s.
Nothing yet moves the layers' own aircraft apart to make room for it, which
is the only way to zero. **A handed aircraft flying a copilot's route is
not measured in a run.** And, out of this item's scope, **nothing keeps a
person's aircraft clear**: the item is about AI aircraft, and a person's
is given way to, not measured. So the item stays open, with those named.

**The case, and what was wrong.** The 2026-10-08 run that lost separation
for 69.6 s (`--fly`: the player's full aileron for two seconds, then the
hand-over) was traced step by step on this branch: the AI was given the
aircraft at 65 degrees of bank, 49 nose down, 498 ft over the higher
layer, and it **pulled in the spiral before rolling level** - the bank
asked walked back at a turn's 5 degrees a second, the pitch asked rose at
3 a second, and the elevator's trim wound nose-up all the way down - so it
spiralled down through the higher layer's height (0 ft at least, within
1.5 nm), took 20 s to roll level, and then zoomed 400 ft back up through
the height it was held to, to 52 kt. Between the layers the monitor's
"squeezed between two" rule held it to whichever side asked less of it,
which is within the minimum of the other: held there, the old monitor
loses separation for as long as they fly together.

**What changed.**
- **Squeezed between two, turned away** (`sim::separate`,
  `HeightLimit::heading_deg`): an aircraft that gives way and has no
  height 700 ft from all those it gives way to is given a heading directly
  away from the nearest it would pass through, and held in the middle of
  the gap until it is 1.5 nm from every one of those; then it is taken
  above them all or below, the side that asks less of it, and not into the
  ground, still turned away until it is clear. This is what a controller's
  safety alert does - "traffic alert, advise you turn left heading ...,
  climb and maintain ... immediately" (FAA JO 7110.65, 2-1-6) - turning and
  climbing at once; TCAS's resolution advisories are vertical only, and no
  vertical manoeuvre alone keeps it apart here. The turn is the autopilot's
  own heading loop (`Autopilot::turn_away`, through `Controller`): its
  25 degrees of bank, rolled into at its 5 degrees a second; let go, the
  heading loop's integral found on the away heading is let go too. The
  server says it: "aircraft 0, c172p (slot 0), is held at 3427 ft, clear
  of aircraft 5, turned away to heading 185" (rounded before it is printed,
  so 359.6 is 000, not 360).
- **Latched** (`sim::Squeeze`, handed back to `separate` as
  `Traffic::squeezed` each step - the server keeps it per aircraft): the
  side (above them all or below) and the heading are chosen once, when it
  is first squeezed, so two layers equally far cannot flip it from step to
  step; and once clear (1.5 nm from every one it passes through) it stays
  clear until under 1.3 nm (`Separation::clear_again_m`, 370 m of
  hysteresis), so a distance that dips back under 1.5 nm mid-climb does not
  send it back to the middle of the gap.
- **An upset is rolled level before it is pulled** (`Autopilot`,
  `upset_bank_deg`): banked past 45 degrees, no turn is asked until the
  wings are level, the bank asked comes back at 15 degrees a second (three
  times a turn's), the nose is not raised and the elevator's trim is not
  wound - unload, roll, then pull, the nose-low recovery of the FAA's
  Airplane Upset Prevention and Recovery Training Aid (AC 120-111), whose
  own definition of an upset's bank is past 45 degrees. In both pitch laws:
  the plain holds, and the airspeed on the elevator (an approach, a glide,
  a stall recovery). The controls still move no faster than a hand, full
  travel in a second: the most the handed Cessna's aileron, elevator or
  rudder moved in a step was 0.0083 of full travel - 1/120, a hand's pace,
  at the limit and not past it.

**Verification.**
- `an_aircraft_handed_to_the_ai_between_two_layers_is_turned_away_and_kept_apart_from_both_after_its_arrival`
  (`test_separation.cpp`): three C172Ps in JSBSim, built exactly - layers
  at 3,000 and 4,000 ft flying north at 100 kt, the handed one 600 m behind
  the lower at 3,400 ft holding north and 3,400 - flown 300 s (36,000 of
  36,000 steps, asserted) without and with the monitor. Without: 400 ft at
  least within 1.5 nm of the lower, lost 300 s. With: turned away 45.7 s,
  lost 38.9 s from the lower (least 400 ft, its arrival), never after 38.9 s,
  never from the higher (least 516 ft); its controls at most 0.0083 a step.
  **The 38.9 s, split**: the climb to the gap's middle, 3,500 ft, is slow -
  the last 100 ft asked at 3 ft/min a foot while banked 25 degrees in the
  turn - and it is within 20 ft of it only at 33.6 s and no higher than
  3,488 ft before 1.5 nm; so it is under the minimum of the lower all the
  way to 1.5 nm, which the turn about and the opening at 200 kt reach at
  38.9 s. The loss ends exactly there. Bound: 44 s lost and none after
  44 s - the opening's 38.9 s and an eighth.
  **Seen to fail** with the old `separate` (the side that asks less):
  "turned away for 0.0 s", at least 300 ft, lost 300 s.
- `an_aircraft_its_player_rolled_into_a_spiral_over_two_ai_layers_handed_to_the_ai_is_rolled_level_and_never_loses_separation_from_either`
  (`tests/cmake/server_handed_spiral_kept_apart.cmake`): the 2026-10-08
  run, as a test - two plan-file AI aircraft, the client's aircraft a
  thousand feet over them rolled with full aileron and handed over two
  seconds in - measured over 14,400 steps of the hand-over (two simulated
  minutes, asserted for both pairs with it), bound zero. Measured: the
  handed aircraft at least 849 ft from AI 2 within 1.5 nm (350 m at
  closest), 1,850 ft from AI 1 (606 m), separation lost for 0 s with
  either; AI 1 and AI 2 1,000 ft, never lost.
  **Seen to fail** on the code before this branch: AI 2 and the handed
  aircraft 0 ft at least within 1.5 nm, lost 67.6 s; AI 1 498 ft, 1.35 s.
  And with the monitor's turn but not the upset's roll: 1 ft, lost 44.75 s
  (and in a second run, 285 ft and 24.97 s from AI 1) - the spiral, not
  the gap, is what this run builds, and it does not now come down between
  the layers: which is why the unit test builds that exactly.
- `the_monitor_holds_...`: its squeezed rule rewritten - held in the middle
  and turned south, away from the one it passes; 1.5 nm from it, taken
  below them all, still turned; and nothing turned that is not squeezed.
  And latched: at 1.40 nm, clear before - still clear, below them all, its
  heading kept (200, not chosen again); at 1.24 nm, back to the middle;
  the side chosen (above) kept though below asks less. **Seen to fail**
  with the latch ignored: "clear before, dipped to 1.4 nm: still clear,
  still taken below them all".
- `an_autopilot_handed_a_spiral_rolls_its_wings_level_before_it_raises_the_nose_in_both_its_pitch_laws`
  (`test_autopilot.cpp`): the Cessna rolled past 65 degrees with full
  aileron at 6,000 ft and held there until 20 degrees nose down, then
  handed to the AI in each pitch law (on the elevator asked for 60 kt, so
  that its law would raise the nose): handed over at 76 degrees, within 45
  in 2.69 and 2.72 s (11.5 and 11.4 degrees a second - the 15 asked, less
  the aeroplane's lag), the pitch asked never raised and the trim never
  moved meanwhile, no control step over 0.00446. **Seen to fail** with the
  rule off: 7.03 s (4.4 degrees a second), pitch asked up 12.8 degrees,
  trim 0.54. **What it cannot show**: with only the clamp in the
  airspeed-on-the-elevator law taken out, it still passes - in a spiral
  that law's own pull-out limit (no nose raised while the load is past its
  most) already holds the nose. The clamp is kept there as the rule's
  statement, for a bank past 45 degrees pulled below that load.
- `a_plan_turned_away_from_and_let_go_is_flown_on_past_every_waypoint_in_order`
  (`test_controller.cpp`): the Cessna on a plan of three waypoints up the
  coast from Bondi, turned away south for 60 s from 20 s in and let go:
  heading 181 at the release; then all three waypoints passed, one at a
  time, in order, by 332 s; no control step over 1/120.
- Re-run and green: the AI aircraft kept 500 ft or 1.5 nm apart over the 15-minute run
  (425 s), the handed aircraft measured and kept apart, the four
  hand-overs in the air flown as recorded or holding course, taken back at
  once, the climb-through and head-on Cessnas, the ceiling and floor, the
  four CBD orbits as recorded (OpenAI's, Anthropic's, the 747-400's and the
  F-22's), and the selftest's five: 20 of 20, linux-debug. For the
  autopilot's upset rule, every test matching stall, rolled, take-over,
  taken, recover, bank, turns ninety, spin, hand-over, handed, heading and
  spiral but the window client's and the live models' was run, but **not
  cleanly**: the first pass ran beside the pre-push build, which relinked
  the test binary under it, and the 152 it left not run or failed passed
  only when run again. CI's run settles it. After the review's fixes, the
  monitor's rules, the between-layers case, the upset rule and the
  turned-away plan pass here; the wider run (183 tests) was still going
  when this was committed.

The new tests' linux-debug costs in `tests/ci_costs/linux-debug.txt` are
estimates - this machine's time and half again - until CI's own replace them.

**The selftest hash does not move**: it flies a fixed input log through
the test pilot, which neither the monitor nor the autopilot touches
(`the_selftest_prints_the_same_hash_every_run` passes).
