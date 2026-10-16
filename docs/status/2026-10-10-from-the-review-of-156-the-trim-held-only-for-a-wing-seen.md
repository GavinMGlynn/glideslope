### From the review of #156: the trim held only for a wing seen to stall, the stall lesson's modes in the simulation, the AI's notice re-armed and tested both ways, 2026-10-10 — item still open

**What is still missing, first** (unchanged by this entry): at the warning
the Learjet 35A (385 of 350 ft) and the Short S.23 (184 of 200, short of
the 10% margin) are named, and the Mosquito is never level again. The AI's
notice is not watched in the take-off or the approach, which fly their own
laws, and its go-around circuit and engine-out glide are not flown by any
test of it. The **747-400 and F-22A publish no stall speed and are not
watched**. Its warning is the stall **in the landing configuration**, the
lowest she has: clean, or banked, the real stall is higher and the warning
comes late - never early.

**1. The stall lesson's modes are the simulation's.** The entry
(`fly_the_stall_entry`: ten knots under the stall, the height held level,
`hold_height_to_the_stall`) and the recovery (`fly_the_stall_recovery`: the
autopilot's recovery at five knots past the lesson's recovery speed) moved
from test_lesson.cpp to src/sim/lesson.cpp. The lesson's own flight
(`fly_a_stall`) and the instructor's demonstration (`demonstrate_a_stall`,
through a `Controller`) both fly them. **Nothing else in src/ enters a
stall**: no frontend runs a lesson (src/frontend has no lesson code), so
these two are the only lesson flights there are, and the checks fly them.

**2. The trim's stall rule engages only for a wing seen to stall.** The
rule read the angle of attack alone against the learnt peak, and the peak
is never forgotten: one set low by a gust or a pull could refuse nose-up
trim in ordinary slow flight later. It now needs the wing **seen going
over its peak** (`seen_to_stall_`): past it by three tenths of a degree,
the angle rising and the lift coefficient falling, both on quarter-second
trends; it lets go once the angle is back under the peak. New test,
`the_trim_is_never_held_for_a_stall_on_approaches_and_climbs_in_moderate_turbulence`
(test_autopilot.cpp): the first aeroplane of each of the seven classes
that publishes how she lands - 737-300, B-2A, C172P, F-15C, Learjet 35A,
Mosquito, Short S.23 - on an approach (landing flap, gear, approach speed,
700 ft/min down) and a best-rate climb, ninety seconds each in moderate
turbulence: the rule never engages, and the nose follows the pitch asked
within 4 degrees on average (the 737-300's approach the worst, 3.3; the
rest 0.5 to 1.5). **The B-2A's approach is named and not flown**: at her
approach speed on the plain autopilot in moderate turbulence she departs
(177 degrees of alpha), with the rule or without it. **Seen to fail on the
old rule** (the angle alone): the F-15C's approach held the trim 1 step,
the Mosquito's 7, the S.23's climb 7 - red - and green on this one. The
stall figures do not move but the S.23 left thirty seconds, 142 -> 182 ft
of 619.

**3. The AI's notice, re-armed and tested both ways.**
- **Re-armed only once clear of the warning by its own margin again**
  (`stall_armed_`): handed back, the speed must pass the warning by the
  warning's margin over the stall (5 kt or 5%) before a stall is noticed
  again.
- `the_ai_pilot_notices_a_second_stall_after_handing_the_first_back`: the
  172P slowed into a stall, recovered and handed back, then asked again (at
  50.3 s): noticed and recovered twice.
- `the_ai_pilot_notices_no_stall_cruising_or_climbing_in_moderate_turbulence`:
  all fourteen that publish how they land (747-400 and F-22A named,
  unwatched), three minutes' cruise at their start speed and three
  climbing at 500 ft/min at their best-climb speed, moderate turbulence:
  none noticed. Slowest against the warning: 737-300 219.0/111.0, 787-8
  245.7/119.4, A320 245.6/119.0, A380 244.9/110.1, B-2A 225.3/100.4, C172P
  63.3/51.0, C182 75.2/54.5, F-15C 197.2/113.8, F-35B 196.7/128.4, **J-3
  Cub 40.1/38.0**, Learjet 235.4/101.5, Mosquito 140.3/99.5, PA-28
  64.3/54.5, S.23 79.2/71.3. **The Cub flies within 2 kt of her warning in
  turbulence at her own start speed**: her cruise is that slow, and a
  stronger gust would have her noticed. Asked to climb at 1,000 ft/min the
  S.23 - whose autopilot has no climb floor - slowed to 68.8 kt, under her
  71.3 kt warning, and was rightly noticed. The take-off (and so the Cub's
  lift-off) is the departure's law, which the notice does not watch.

**4. The A320's 1.81 g, perturbed**: the check left thirty seconds with
the peak margin at 0.25 and 0.35 degrees, and the entry at 9.5 and 10.5 kt
under the stall: 1.81 g each time (to the hundredth), so at most 1.815 x
1.1 = 1.997, inside 2. Nothing changed for it.

**Verified on** this branch rebased on #155 as restacked (`c6cccbb1`),
linux-release with every target: the tests matching stall, recover, upset,
lesson, instructor, take, glide, orbit, rolled, autopilot and trim (below).
The selftest hash does not move, `182dd6c996e0ee4c`.
