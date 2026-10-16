### A light aeroplane slowed from cruise into her climb captures her climb speed, 2026-10-10 — item still open

**What is still missing, first**: the J-3 Cub's climb in moderate
turbulence, 40.9 kt against her 39.9 warning (was 40.1) - 1.0 kt in hand,
short of the 3 the test asks; she stays named. The capture was a part of
her dip (calm, 42.6 -> 47.8), but the rest is the gusts themselves, in a
steady climb at full throttle and her best-rate speed: in moderate
turbulence her airspeed swings about 7 kt either way, and at 47.8 kt, 1.2
times her stall at that weight, nothing in the plan or the capture gives
her more. And the six aeroplanes with no speed floor - B-2A, F-15C,
F-22A, F-35B, Mosquito, S.23 - still sink 2.0 to 6.4 kt past their climb
speeds slowing into a climb; named in the new test, a Later item.

**Why it sank** (traced, the C172P in calm air, cruise 100 kt to 75.4 at
500 ft/min): the throttle law cut her to idle for the slower speed and
began to open again only near it, at its quarter of a travel a second -
four seconds to full - while she slowed 2.3 kt a second with the nose up
for the climb. A light aeroplane's climb at her best-rate speed wants all
her power, so the throttle law alone could never have caught it; the
speed floor (the underspeed protection, sim/autopilot.cpp) that holds it
waited for the throttle to be at its stop, at 69.3 kt. A lead on the
throttle's speed error was tried first and not kept: at 2 and 4 s it moved
the 172P's slowest by 0.6 and 0.2 kt and the F-22A's 1.2 the wrong way,
because the throttle reached its stop no sooner than the speed did. Nor
was restoring the cruise throttle in time: that is not the climb's power.

**The fix** (`Autopilot::fly`, the climb floor): the floor also takes over
when, on her speed trend, she would reach the least before the throttle
could reach its stop. Then the throttle opens fully at once, and the
climb she has is held, not wound down on her deceleration, until she is
within a knot of the least or no longer slowing; from there the floor's
own integral finds the climb that keeps the speed and lets go once she
has the climb asked. Only while she slows by more than a knot a second:
without that gate, level at her best-climb speed a wobble's tenths of a
knot a second engaged it, and the C172P, C182 and PA-28 near-ceiling
turn tests failed at 3,000 ft (16.7 ft level, 23.3 in the turn, against
20) - seen in the broad run, fixed, and run again. Engaged early with the integral wound at once, the
172P went down 800 ft/min at 80 kt and round three times; held, she
comes to her climb speed no more than 1.3 kt under it (table). It is a flight level change's speed capture for the
one transition this autopilot flies on the throttle alone. No control
moves faster than before: the throttle at its rate, the pitch at its.

**Measured** (linux-release), slowed from cruise into a 500 ft/min climb
at her climb speed in calm air, slowest from then on, before -> after:

| Aeroplane | climb speed | before | after |
|---|---|---|---|
| C172P | 75.4 | 69.1 | 74.1 |
| C182 | 82.0 | 77.1 | 81.3 |
| J-3 Cub | 47.8 | 42.6 | 47.8 |
| PA-28 | 73.9 | 68.1 | 72.7 |
| B-2A (named) | 230.0 | 227.1 | 227.1 |
| F-15C (named) | 200.0 | 198.0 | 198.0 |
| F-22A (named) | 220.0 | 213.6 | 213.6 |
| F-35B (named) | 200.0 | 197.6 | 197.6 |
| Mosquito (named) | 148.0 | 142.7 | 142.7 |
| Short S.23 (named) | 100.0 | 97.8 | 97.8 |
| 747-400, Learjet 35A | 170.0, 240.0 | 171.3, 238.1 | the same |

The 737-300, 787-8, A320 and A380 climb faster than their cruise, and are
counted as not slowed. In moderate turbulence (the notice test), the
climbs: Cub 40.1 -> 40.9, C172P 67.8 -> 69.1, C182 75.3 -> 76.3, PA-28
68.2 -> 67.7, the rest unchanged; none noticed.

**Tests**: new, `slowed_from_cruise_into_a_climb_every_aeroplane_sinks_no_more_than_2_kt_under_its_climb_speed`
- every aeroplane, the six without a floor named, counted. **Seen red** on
the code before: ten sank past, the four light aeroplanes among them.
Run on the final code, all passing (linux-release, 506 tests, the live
ones left out): every test whose name speaks of a climb, a take-off, a
first leg, a departure, an orbit, a glide, a plan, a hand-over, a ceiling,
the autopilot, a height, turbulence, a stall or a turn - among them
`every_light_aeroplane_is_handed_to_its_plan_at_its_climb_speed_and_climbs_its_first_leg_at_it`
(#153's 2 kt bound), the four AI-notice tests and the near-ceiling turns.
**The selftest hash does not move**, `182dd6c996e0ee4c`: its C172P is never
slowing towards her floor with her throttle short of its stop.
