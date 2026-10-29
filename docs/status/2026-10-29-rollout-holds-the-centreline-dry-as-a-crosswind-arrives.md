### The AI's roll-out holds the centreline dry as a 15 kt crosswind arrives, 2026-10-29 - item open

**What is still not true first.**
- **Wet, the item is not done.** Rolled out on a wet runway as 15 kt of
  crosswind arrives, the jets end 5.0 to 6.6 m from the centreline at worst
  (the A320 6.58, the 787-8 5.64, the A380 5.29, the 737-300 5.15, the F-15C
  5.01), the C172P 5.33, the PA-28 14.8 and the Mosquito 18.4. The item's
  5 m is met dry and not wet; it stays `[ ]`.
- **The PA-28 and the Mosquito wet are not the roll-out's steering.** The
  PA-28, handed over at 43 kt as the gust arrives, has full rudder and one
  brake full from the first second and still swings 16 degrees into the
  wind: the steering has nothing left to give. The Mosquito, handed over at
  109 kt, is light on her wheels and slides downwind, then lifts off and is
  flown by the flying law. Neither was investigated further here.
- **The C172P's 4.75 m dry is a bounce**: at 50 kt as the gust arrives she
  lifts off and drifts in the air under the flying law, then is steered
  back on the ground. It is the closest to the bound, 0.25 m inside it.
- **The Mosquito is named dry, not asserted**: braked dry she goes over on
  her nose and strikes the ground with her airframe, as she did before this
  change (the 2026-10-14 status says so); the test checks it is still so.
- **Tried and not kept**: holding the aileron into the wind (the AFH's
  crosswind roll-out) helped the C172P and the Mosquito and made the PA-28
  worse at either sign; steering by the track in the fast loop as well as
  the slow one made the A380 weave off the runway wet (50 m).

**What now works.** The roll-out's rudder (`Lander::fly_laws`, the rollout
branch) had two faults in a crosswind that arrives:

- **Its gains left a steady error.** At 0.10 of rudder a degree of heading
  error and 0.30 a degree a second of yaw rate, the rudder that held an A320
  against the weathervaning took 4 degrees of heading error to ask for; at
  4 degrees off at 100 kt she rolled 21 m from the line before the line's own
  correction (three seconds of travel to regain it) turned her back. Now 0.4
  and 0.5.
- **Nothing learnt the steady push.** The rudder a crosswind needs, and the
  crab that cancels her tyres slipping downwind, are now learnt by an
  integral of her **track** error over the ground (her heading below 5 kt,
  where a track is not worth reading), at 0.1 of rudder a degree-second,
  not learnt past the rudder's stop; it starts from nothing at each hand-over
  (`ground_rudder_trim_`, reset in `resume`).
- **Not at a walking pace.** From 10 kt down to 5 kt the gains ease back to
  the old 0.10 and 0.30 and nothing more is learnt. Held at the new gains
  all the way down, the rudder brought in the steering brake more at a
  walking pace, and five landplanes handed over at 2.5 kt across (and the
  Mosquito at 3 kt dry and wet) reached the brakes' rolling ceiling, which
  the three walking-pace tests pin as not reached: they went red, and are
  green again with the easing, the roll-out figures unchanged.

Worst distance from the centreline from the hand-over to the stop, 15 kt
across arriving at once, Linux release, before and after:

| | dry before | dry after | wet before | wet after |
|---|---|---|---|---|
| 737-300 | 12.88 | 1.66 | 9.40 | 5.15 |
| 787-8 | 16.85 | 2.02 | 13.63 | 5.64 |
| A320 | 21.39 | 3.15 | 18.08 | 6.58 |
| A380 | 12.62 | 2.00 | 12.06 | 5.29 |
| B-2A | 12.81 | 1.79 | 11.75 | 1.66 |
| C172P | 4.97 | 4.75 | 4.91 | 5.33 |
| C182 | 4.30 | 0.96 | 2.67 | 1.68 |
| F-15C | 17.61 | 3.24 | 21.04 | 5.01 |
| F-35B | 21.19 | 2.48 | 22.17 | 2.07 |
| Learjet 35A | 14.26 | 1.32 | 13.33 | 2.50 |
| Mosquito | nosed over | nosed over | 25.87 | 18.43 |
| PA-28 | 6.80 | 3.14 | 15.00 | 14.78 |

**Verification.**
- `every_landplane_rolled_out_dry_as_15_kt_of_crosswind_arrives_stays_within_5_m_of_the_centreline`
  (new): the roll-out of the wet roll-out test, dry; eleven landplanes
  asserted within 5 m, stopped and unwrecked; the Mosquito named (and her
  nose-over checked); the flying boat, 747-400, F-22A and J-3 Cub left out,
  named, as in the wet test. Coverage asserted, 11 + 1 + 4 of 16. Wet is
  printed beside it, not asserted. **Seen red** on the old law: ten
  landplanes 6.8 to 21.4 m.
- The wet roll-out test (30 m), the wet side grip's slide test (every jet
  still slides at least 1.2 times as fast wet; least now the B-2A at 1.60)
  and the full suite pass on Linux release.
