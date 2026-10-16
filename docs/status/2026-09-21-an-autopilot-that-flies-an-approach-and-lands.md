### An autopilot that flies an approach and lands, 2026-09-21 — item done

Phase 8's approach-and-landing item, **brought forward on purpose**: Phase
5c's lessons teach take-off, the circuit, stalls, approach and landing, and
until something can land there is nothing to demonstrate them with. Reading
what the AI pilot could do settled it - it holds a heading, an altitude, a
vertical speed and a speed, follows waypoints in the cruise, and does nothing
else. So this came first although it is written three phases later.

**All four light aircraft fly an approach from five miles out and land**, in
calm air and in a ten-knot crosswind:

| Aircraft | Vref | calm: sink, across, stopped | crosswind: sink, across, stopped |
| --- | --- | --- | --- |
| Cessna 172P | 59.8 kt | 128 ft/min, 0.14 m, 570 m | 217 ft/min, 1.39 m, 569 m |
| Cessna 182S | 64.4 kt | 72 ft/min, 1.89 m, 406 m | 255 ft/min, 2.72 m, 442 m |
| Piper PA-28 | 64.4 kt | 77 ft/min, 1.61 m, 722 m | 243 ft/min, 3.99 m, 708 m |
| Piper J-3 Cub | 42.9 kt | 177 ft/min, 0.19 m, 539 m | 149 ft/min, 1.66 m, 491 m |

held to under 300 ft/min, within 5 m of the centreline, and stopped on a
3,000 m runway.

**The reference speed is not a number written here.** `approach_speeds` reads
the aeroplane's own published figures and takes a third above its stall in
the landing configuration - the most flap it publishes a stall speed at,
which is all of it for an aeroplane with flaps and none for a Cub. An
aircraft that publishes no stall speed is refused, because a reference speed
guessed is a reference speed that means nothing; the B-2 publishes none, and
a test holds that it is refused.

**It is not the cruise autopilot, and could not have been.** That one holds
an altitude on the elevator and a speed on the throttle and never touches the
flaps, the gear or the brakes. `sim::Lander` flies the glidepath on the
elevator and the speed on the throttle - an autopilot and an autothrottle -
and works the flaps, the gear and the brakes as the stages need them:
approach, flare, rollout, stopped. **The simulation links no world library**,
so the geometry is a local one: the metres in a degree of latitude and
longitude at the threshold, which over five miles is right to better than a
metre.

**Four things had to be learnt from watching it fly.**

1. *The glidepath aims past the threshold, not at it.* Aiming at the
   threshold puts the flare before it and the wheels on the grass - the
   Skylane touched down 36 m short. Aiming 300 m down the runway puts the
   aeroplane about fifty feet up as it crosses, which is where it should be.
2. *The flare is a sink that decays with height, not an attitude.* A fixed
   nose-up ramp landed the Cessna gently and the heavier Skylane at 472
   ft/min. Commanding the sink instead - forty feet a minute at the ground,
   more the higher it is - is one law that fits a Cub and a Skylane.
3. *Stopped is over the ground, not through the air.* An aeroplane standing
   still in a ten-knot wind still reads ten knots of airspeed, so the first
   version never noticed it had stopped.
4. *The stick comes back and stays back on the rollout.* Brakes with the
   stick forward put the Cub on its nose: its pitch went to -71 degrees and
   JSBSim asserted inside its own aerodynamic tables. The stick full aft
   holds a tailwheel down and keeps a nosewheel light, so one rule fits both,
   and the brakes come on as the aeroplane slows rather than the moment it
   touches. The flare also stops raising the nose past twelve degrees of
   incidence, for the same reason: JSBSim asserts rather than extrapolating,
   so an aeroplane flown off the end of its tables ends the flight.

**Verification run.** Three tests:
`every_light_aircraft_is_flown_down_a_glidepath_and_lands_in_calm_air`,
`every_light_aircraft_lands_on_the_centreline_in_a_ten_knot_crosswind` and
`the_approach_speed_is_a_third_above_the_published_landing_stall`. Each
states that the light aircraft are four and fails if that moves. Watched to
fail with the cross-track integral taken out: the Cessna touched down 11.74 m
from the centreline instead of 1.39 m, and was 16.33 m off at its worst on
final - which is the standing offset a proportional loop alone leaves in a
crosswind, and the reason the integral is there. The two flying tests take 80
seconds each. 297 of 297 tests pass locally at `-j4`, in 1471 s.

**What it is not.** It lands on a runway it is given; nothing finds a runway
for it, there is no airfield data, and it does not go around. It flies no
part of a circuit and cannot take off, which is still what Phase 5c's lessons
need next.

**Superseded 2026-09-24** (see "Landings that stay landed"): every one of
these landings bounced, which nothing asked. The rollout no longer puts the
stick full aft at the touch, the brakes hold an autobrake's deceleration, the
centreline is flown by L1 guidance with no cross-track integral, and the
flare, the speed loop and the power near the ground are all different. Flown
now, calm then crosswind: the Cessna 172P 121 and 123 ft/min, 0.09 and 0.08 m,
stopped at 674 and 629 m; the 182S 131 and 138 ft/min, 1.55 and 0.75 m, 658
and 633 m; the PA-28 133 and 135 ft/min, 0.98 and 1.60 m, 630 and 614 m; the
Cub 139 and 140 ft/min, 0.02 and 0.04 m, 479 and 431 m - each staying down,
within 6.3 degrees of bank and 3.3 nose down from the touch to the stop.
