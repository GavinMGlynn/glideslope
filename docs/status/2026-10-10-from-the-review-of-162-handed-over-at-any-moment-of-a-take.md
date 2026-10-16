### From the review of #162: handed over at any moment of a take-off's first 40 s, a touch-and-go, and the controllers' figures tested, 2026-10-10 — item stays done

**What is still not done, first.** A frontend that stopped calling
`Controller::told` would not be caught: the test reaches the one function
both call, not the server's `controller_for` or the client's `Flight`
themselves, which no test constructs. A landing bounced at power and handed
over in the bounce is climbed out, but the 787-8 and the Mosquito touch the
runway once more before they climb away; it is measured, not asserted. **The
rare low go-around**: a pilot who takes off, never climbs above 400 ft, and
within five minutes goes around at power from an approach that does not
touch is taken for a take-off and climbed out to 500 ft - what a go-around
flies anyway; it is not tested.

**The review's finding, which was real**: the item's tick rested on the
hand-over on the very step of lift-off, inside the one second the rule then
allowed whatever her climb. Handed over later in ground effect, under 100
ft/min, she was given the plain autopilot. A sweep of hand-overs from 0 to 10 s
after lift-off found 52 failures: the A380 at 7 to 10 s (the take-off's own
law has her sinking at up to 1,100 ft/min at 8 s, 140 to 190 ft up), the B-2A
at 5 to 7 s (level at 68 ft), the Mosquito at 7.5 to 10 s (level at 100 ft),
the PA-28 at 1 to 3 s and 8.5 to 10 s (accelerating level at 5 to 21 ft).

**The rule now** (`Controller::lifting_off`): within 40 s of her wheels
(`lift_off_settling_s`), whatever her climb, or climbing at more than 100
ft/min within five minutes; never above 400 ft since; the throttle over half.
**40 s from measurement**: flown off by the take-off's law for two minutes,
the last moment each was below 400 ft and not climbing at more than 100
ft/min - 737-300 0.33 s, 787-8 0.27, A320 0.34, A380 10.46, B-2A 7.44, C172P
0.36, C182 12.92, F-15C 0.02, F-35B 0.16, J-3 Cub 0.14, Learjet 0.20, Mosquito
12.02, PA-28 31.61 - the slowest, the PA-28's, and a quarter more. So a
level hand-over low down and at power inside 40 s of lift-off is taken for a
take-off and climbed out to 500 ft; after it, a level one is not.

**One place the controllers are told** (`sim::her_figures`,
`Controller::told`): her approach speeds and her departure speeds, read
together, with what is missing said. The server's `controller_for` (its
per-model cache is now of these) and the client's `Flight` both tell their
controllers from it, and the take-off tests tell theirs from it too.

**Tests**:
- New: **the lift-off sweep**, 13 landplanes, 21 hand-overs each, 273:
  every one given the climb-out, never touching, past 500 ft and the
  autopilot's within three minutes; and each landplane's measured settling
  time asserted inside the rule's 40 s. **Seen red** with the old
  one-second rule: 52 failures. As one test it took 60 s in linux-release,
  which would have passed ctest's 900 s in CI's sanitized debug build, so it
  is seven tests by aircraft (`the_737_300_and_a320_...`,
  `the_787_8_and_a380_...`, `the_f15c_and_f35b_...`,
  `the_b2_and_learjet_...`, `the_cessnas_...`, `the_cub_and_pa28_...`,
  `the_mosquito_handed_over_at_any_half_second_of_the_ten_after_lift_off_climbs_away_and_never_touches_the_runway`),
  and `the_lift_off_sweeps_parts_hand_every_landplane_over_at_every_half_second_exactly_once`
  asserts every landplane in exactly one part, 273 hand-overs (**seen red**
  with the Mosquito left out of hers). Here in linux-debug, one at a time:
  92, 124, 108, 115, 110, 90 and 53 s (linux-release 6 to 9.4 s).
  `tests/ci_costs/` has estimates for all seven presets, scaled from these
  by what `an_aeroplane_landed_by_hand_and_handed_over_on_its_roll_is_landed_to_a_stop`
  costs here (1.62 s release, 18.26 debug) and on each CI runner - CI's
  linux-debug about 4.3 times this machine's, so the longest part about
  530 s.
- New: `the_figures_the_server_and_client_tell_their_controllers_give_every_aircraft_how_she_takes_off`
  - all 16 aircraft told their departure speeds, 14 their approach speeds
  (the 747-400 and F-22, which publish no stall, named). **Seen red** with
  `told` not passing the departure speeds: 16 failures.
- New: `a_touch_and_go_handed_over_as_she_leaves_the_runway_again_is_climbed_away`
  - the 13 flown down final from 688 ft by the approach autopilot's controls
  as the pilot's, rolled a second, given full power and the take-off's
  controls, and handed over as their wheels leave 1.1 to 12.3 s later: all
  given the climb-out, none touching again, 559 to 1,266 ft. **Seen red**
  with the climb-out disabled: 33 failures.
- `a_take_off_levelled_low_down_and_handed_over_is_not_given_the_climb_out`
  now hands over past the 40 s, and asserts it: the 13 at 40 to 53 s, 185 to
  202 ft, -99 to 64 ft/min, all given the plain autopilot.
- Run (linux-release, 416 tests by name - take-offs, departures, lift-offs,
  touch-and-goes, take-backs, hand-overs, skips, the lander, landings,
  plans, the server and the client): 403 passed, the 13 that ask a live
  model skipped; none failed.
- **The selftest hash does not move**, `182dd6c996e0ee4c`.
