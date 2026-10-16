### The autobrake adds no brake past 0.3 at walking pace, 0.45 at a creep; one learnt start named, her gear throwing her over near a stop, 2026-10-14 — item still open; two found

**What is still missing, first**: **the verification rests on named
cases**, so the item stays open and this lands as an improvement:
- The Mosquito swings 36 to 155 degrees below walking pace (the heading
  check, below). Measured with the steering-brake fade taken out, in the
  same three walking-pace tests, she swings 80.6 degrees in calm air, 81.0
  wet and 155.8 in 15 kt across, against 84.8, 36.2 and 155.4 with it. The
  fade does not cause her swing; on a wet runway it halves it. Its own item
  (Phase 10b).
- The Cub in 15 kt across goes over before the rollout has her. She does
  the same without the fade.
- One learnt start is named (below), for the Phase 9 gear impulse. (Since
  #197 and #198 she lands within the limits, and the name is off: the
  entry above.)
- The 0.8 allowance for an idle creep is barely exercised on the jets: the
  gust test pushes only the four light aeroplanes past 1.3 kt, and the jets'
  brakes hold them at 1 kt and 0.45. It also sits above the 0.6 that tipped
  a C172P held against power, though here the throttle is closed.

**Named, also**: **one learnt landing is left out.** In
`the_learnt_policy_lands_the_c172p_within_the_limits_from_every_start_with_half_its_fuel`,
the start from -50 m across, -15 m high, in a 10 kt crosswind is thrown on
her nose at 1.2 kt and does not stop. On main she stops 608 m along. The
test names that start (shown, not judged), asserts it is the only one
named, and fails if it lands within the limits, so the name comes off when
it is fixed. Traced step by step: at 132.458 s she is rolling straight at
1.16 kt, nose 1.6 degrees, pitch rate 0.4 deg/s, autobrake 0.303 on both
wheels, no steering brake, throttle closed. One 120 Hz step later her pitch
rate is -376 deg/s and her groundspeed 6.97 kt, with no control having
moved. She goes over, and the rollout then sees her off the ground and
flies her away at full power. None of the changes below moved a control at
that step: the brake was steady (under the 0.415 ceiling at her speed), the
steering brake was nil, and no creep was being driven. It is an impulse from
her gear at the end of her roll, met at this brake history and not at
main's.

**From the review of #195**:
- **The idle-creep allowance is bounded**: it adds brake only while every
  engine's throttle is closed (not engine 0 alone), and only up to
  `Lander::idle_pedal`, 0.8. Uncapped, a gust could wind the brakes to full.
  0.55 was tried: the F-15C's idle beat it and she never stopped in 15 kt
  across. At 0.8 she stops as on main. New:
  `a_gust_pushing_every_landplane_from_1_to_2_kt_with_her_throttle_closed_winds_her_brake_no_further_than_idle_needs`.
  Each landplane is rolled to 1 kt and given to the rollout, then a 20 kt
  gust blows from behind for 5 s. Judged: no brake added below 5 kt past
  0.8, nose above -5 degrees, and she stops. The C172P, C182, Cub and PA-28
  are pushed past 1.3 kt (1.34 to 3.0), asserted. The jets and the Mosquito
  are held at 1 kt by their brakes, and are flown and judged. A 25 kt gust
  pushed the C182 to 5.1 kt with her nose at -5.5. Seen red with the cap
  at full: the C172P and PA-28 at 1.0, the Cub at 0.88.
- **The wet runway's brakes are held to the same limit below walking
  pace.** They went to full at a hand's pace whatever her speed. New:
  `every_landplane_rolling_at_walking_pace_on_a_wet_runway_has_no_brake_added_past_its_ceiling_and_stops_with_her_nose_up`.
- **A heading check in the walking-pace tests**: each must stop within 30
  degrees of her heading at the hand-over. Every landplane does, at most
  4 degrees, except the Mosquito. She is named, and a new item opened (Phase
  10b, "The Mosquito swings below walking pace"): 85 degrees in calm air,
  36 wet and 155 in 15 kt across. With the fade taken out: 81, 81 and 156
  (above). The fade does not cause it.
- **The constants are global defaults**, noted in lander.hpp: every
  aircraft uses them, and none has her own.

**Found on the way, a new item (Phase 9): a rolling aeroplane near a stop
can be thrown on her nose by her gear in one step.** JSBSim's switch between
rolling and static friction near zero speed is the likely cause, and it may
be what tips the Cub on full brakes. Verification: every landplane rolled
to a stop at a sweep of brake levels shows no pitch rate over a stated
bound.

**The change** (`sim::Lander`'s rollout brakes):
- Below 5 kt over the ground (`autobrake_off_kts`) the autobrake adds no
  brake past what she has, or past `walking_pace_ceiling` at her speed if
  she has less: 0.3 (`walking_pace_pedal`) at 2 kt (`creeping_kts`) or more,
  ramped to 0.45 (`standing_pedal`) by 1 kt. It still lets brake off where
  she slows faster than it asks.
- **0.45, measured**: 0.3 all the way down left a C172P at idle in 15 kt
  across creeping at 1.0 kt - her idle thrust, 44 lbf, more than 0.3 holds -
  for 273 s more than on main. A flat 0.4 stopped her, but left a learnt
  corner (2.40 nm, +60 m, -20 m, -5 deg, 15 kt from the left) unstopped.
  0.6 put a C172P held against the throttle on her nose at -18.7 degrees.
- **A creep her idle thrust drives is stopped**: under 2 kt, her throttle
  closed and her speed growing, brake may be added past the ceiling. An
  F-15C at idle on 0.43 gathered speed from 1.4 to 1.9 kt in a minute and
  never stopped.
- **The steering brake fades below 5 kt**, to none at a stop. At 0.9 on one
  wheel it held a weathervaned Cub while power was run up against it, until
  she went over.
- Real autobrakes stand down before taxi speed: Airbus's A340 procedure, as
  the TSB gives it (A05H0002, 1.17.6, p. 68), "Autobrake must be disengaged
  before reaching 20 knots."

**Verification** (linux-release):
- New: `every_landplane_rolling_at_walking_pace_has_no_brake_added_past_its_ceiling_and_stops_with_her_nose_up`,
  and the same `..._in_a_15_kt_crosswind_...`. Every landplane taught the
  approach (13 of 14, asserted; the S.23 has no brakes) is rolled up to 3 kt
  on the throttle, given to the rollout, held at 3 kt by the test's
  throttle for 30 s, then let go for 60 s. Judged: no brake added past 0.3
  at 2 to 5 kt, nor past 0.45 below 5 kt (but to stop an idle creep once
  let go); she stops, nose above -5 degrees and wings within 15. Each rolled
  at 2 to 5 kt for 0.7 s (the C172P) to 30.6 s. In 15 kt across the Cub is
  named: rolled up on the throttle before the rollout has her, she goes
  over. Seen red on main's law: every autobrake went to 1.0 below walking
  pace, and the Mosquito's nose to -6.2.
- New: `in_a_steady_15_kt_crosswind_every_landplane_stops_on_the_runway_as_soon_as_on_main`.
  Every landplane taught the approach (13), flown by her lesson in 15 kt
  across, must stop on the runway within 2 s of main's time from her
  lesson's end. Main, measured here: 2.9 s (the Cub) to 10.5 s (the F-15C).
  Now: the same to a tenth of a second for ten; the C182 4.4 against 4.3,
  the Cub 3.7 against 2.9, the F-15C 11.9 against 10.5. It passes on main's
  law too: it guards against the creep, which the 0.3 law failed by minutes.
- Round one's 32 rollout, wet, crosswind, learnt (all five gate winds, the
  fuel tests), reverser and landing tests, and the three new ones: all 35
  pass, and the two added after review (37), the half-fuel learnt test with
  its one start named (above). Against
  main, no stop is more than 1.4 s later (the F-15C in 15 kt across).
- The selftest hash does not move: 182dd6c996e0ee4c.
