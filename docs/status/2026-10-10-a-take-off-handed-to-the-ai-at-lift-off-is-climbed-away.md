### A take-off handed to the AI at lift-off is climbed away, 2026-10-10 — item done

**What is still not done, first.** The climb-out is for a landplane: a flying
boat handed over as her hull leaves the water is given the plain autopilot
as before (she has no wheels to have left, and the test names her). A
take-off is recognised from the ground her wheels last bore weight on, along
the track she rolled: the real runway is not looked up, so she is climbed on
her lift-off track, not the runway's heading. While the take-off's own law
flies her nothing limits her height or speed (as for a take-off the AI was
given), and the climb-out ends where a take-off's does - through 500 ft with
her flap up, or 1,500 ft - so the airliners, raising their flap from 400 ft
at their climb speed, are handed on at 850 to 1,300 ft. Found on the way (a
Later item): the autopilot holding 200 ft with the A380 at her take-off flap
and gear swings 15 ft either side of it at up to 350 ft/min for three
minutes.

**Just after lift-off** (`Controller::lifting_off`): off her wheels and out
of the water, with the pilot's throttle more than half open - the line the
roll's hand-over already draws between a landing and a take-off - and either
within a second of her wheels (the skip's second: at the very lift-off 12 of
13 are climbing under 100 ft/min, the A380 alone at 194) or climbing at more
than 100 ft/min (what the stall recovery counts as level); never since her
wheels left higher than 400 ft over the ground she left (14 CFR 25.111(c)(4):
nothing of her configuration changes below it on the take-off path, and it
is the highest any class raises its take-off flap from), and within five
minutes of it (14 CFR 1.1's rated take-off power, where the take-off law
itself ends). So a go-around from an approach is not caught - she has been
higher than 400 ft since her wheels last bore weight, or never had them down
- nor a hand-over in level flight low down; a balked landing that touched and
climbs away at power is, and is climbed out as a take-off. The controller
records, every step her wheels bear weight, where they were, her track and
height, and the highest she has been since.

**The climb-out** (`Departure::from_lift_off`): the take-off's own law, as
though it had flown her off - rotated, the climb held off the runway to the
35 ft screen height, the take-off trim washed out, the flap brought up as a
take-off brings it (#144) - from the controls the pilot had: the stick taken
into its trim, the throttle opened on from where it was to full, and every
control reached at a hand's pace as a landing's are. Then the plan
(`to_ai(plan)`, its navigator begun where the climb-out leaves her) or the
hold. A controller learns her departure speeds by `takes_off_with`; the
client's and the server's controllers are told them from her figures
(without them the hand-over is the plain autopilot, as before, and the
server says so).

**Measured** (linux-release), handed over on the step her wheels left:

| Aeroplane | handed over at | climbing | highest in 3 min | at 3 min |
|---|---|---|---|---|
| 737-300 | 3.9 ft | 21 ft/min | 1,079 | 949 |
| 787-8 | 16.2 | 44 | 851 | 654 |
| A320 | 8.7 | 19 | 1,295 | 1,118 |
| A380 | 17.9 | 194 | 1,241 | 1,107 |
| B-2A | 11.4 | 71 | 1,073 | 502 |
| C172P | 4.6 | 28 | 679 | 501 |
| C182 | 4.8 | 38 | 800 | 502 |
| F-15C | 7.8 | 91 | 990 | 501 |
| F-35B | 6.0 | 47 | 869 | 501 |
| J-3 Cub | 4.1 | 65 | 580 | 500 |
| Learjet 35A | 4.8 | 34 | 824 | 612 |
| Mosquito | 7.3 | 87 | 633 | 501 |
| PA-28 | 3.8 | 16 | 658 | 502 |

None touches the runway again. On the code before, given the plain
autopilot, none passed 500 ft and seven (the 737-300, 787-8, A320,
A380, F-15C, F-35B and Learjet 35A) touched the runway again.

**Tests**:
- New: `every_landplane_handed_over_at_lift_off_climbs_away_through_500_ft_and_never_touches_the_runway`
  - every landplane taught the approach (13 of 14; the Short S.23 named,
  counted and asserted), taken off by the pilot with the take-off autopilot's
  controls and handed over on the step her wheels leave, is given the
  climb-out, never touches the runway, passes 500 ft and is the autopilot's
  within three minutes. **Seen red** with the climb-out disabled: 33 wrong,
  all 13 not given it and none passing 500 ft, seven touching the runway
  again; reverted.
- New: `a_take_off_levelled_low_down_and_handed_over_is_not_given_the_climb_out`
  - the same 13 taken off, levelled at 200 ft by an autopilot of the
  pilot's engaged at 35 ft (never above 259 ft), held within 25 ft for ten
  seconds and handed over 20 to 53 s after their wheels, under 100 ft/min,
  with the throttle in hand pushed fully open - asserted for each, so that
  only her climb tells her from a take-off - are given the plain autopilot.
  **Seen red** with the climb rule taken out: 13 given the climb-out;
  reverted.
- `a_take_off_handed_over_at_lift_off_is_given_the_plain_autopilot_not_a_landing`
  (#160) is renamed `a_take_off_handed_over_at_lift_off_is_not_given_a_landing`,
  as she is now given the climb-out; it still asserts no landing for all 13.
- Run (linux-release, 269 tests by name - take-offs, departures, lift-offs,
  take-backs, hand-overs, skips, the lander, landings and plans): 260
  passed, the 9 that ask a live model skipped; none failed.
- **The selftest hash does not move**, `182dd6c996e0ee4c`: the selftest
  hands nothing over at lift-off.
