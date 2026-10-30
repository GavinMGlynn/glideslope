### The AI's roll-out holds the centreline wet as a 15 kt crosswind arrives, 2026-10-30 - item open

**What is still not true first.**
- **The Mosquito, both ways.** Dry she still goes over on her nose braked
  (as on 2026-10-14 and 2026-10-29; the steering never matters). Wet she
  now goes 9.9 m from the centreline (it was 18.4): handed over at 109 kt
  with her tail up, her stick comes back as she slows - a fifth of it by
  100 kt - her tail goes down to 12 degrees of pitch and her wing carries
  five sixths of her weight, so her tyres hold little. The item's 5 m is
  not met for her, and the item stays `[ ]`.
- **Tried and not kept for her**: holding her tail up to seven tenths of
  her reference speed (the stick coming back over 0.5 to 0.7 Vref, not 0.5
  to 0.9) took her wet roll-out to 3.4 m, but a Mosquito taken back on a
  short runway landed by hand at 93 kt was then braked with her tail up
  and went over on her nose (`a_landing_flown_by_hand_on_a_short_runway_is_stopped_on_it_by_the_ai`
  red). Making it work needs her brakes to wait for her tail, which is
  the autobrake's (another item's, in flight), so it is not done here.
- **Tried and found to do little**: reverse thrust brought to idle while
  she is weathervaned (Boeing's FCTM technique for a wet crosswind
  roll-out) moved no jet by more than 0.4 m; releasing the brakes while
  she is crabbed and drifting likewise. JSBSim's tyre gives the same side
  force braked or not (FGLGear has no friction circle), so the
  "release the brakes for the whole of her cornering force" half of that
  technique has nothing to act on in this model.

**What now works.** Two changes to the roll-out (`Lander::fly_laws`):

- **The rudder steers by her track at once**, 0.5 of rudder a degree of
  track error, beside the heading's gains and the learnt push, eased in
  with them above walking pace. A wet tyre corners at a third to three
  quarters of a dry one, so the crab that holds her is about three times
  the dry one, and the learnt push (0.1 a degree-second) took too long to
  find it: the A320 rolled 6.6 m off. Swept: 0.05 to 0.8 improve every
  aeroplane monotonically; at 1.0 the Mosquito swerved 27 m wet and at 1.5
  the A380 43 m. Held at half of where it fails.
- **Her flaps come up on the roll, but a jet's**, once she has rolled
  faster than walking pace (`flaps_up_`; reset in `resume`; a bounce is
  flown with them down). The FAA's Airplane Flying Handbook puts the most
  weight on her wheels after the touch, for her braking and her nosewheel's
  steering, and retracts the flaps for the greatest stopping effort after
  a short-field touchdown. With them down the PA-28, handed over at 43 kt,
  carried three quarters of her weight on her wing and swung 16 degrees
  into the wind against full rudder and a full brake. A jet's spoilers do
  this already; her flaps stay. Latched above walking pace so that an
  aeroplane handed over at 1 kt keeps the flaps the rollout always gave
  her (the gust test's C182 is pushed by them).

Worst distance from the centreline from the hand-over to the stop, 15 kt
across arriving at once, Linux release, before (2026-10-29) and after:

| | dry before | dry after | wet before | wet after |
|---|---|---|---|---|
| 737-300 | 1.66 | 0.95 | 5.15 | 3.08 |
| 787-8 | 2.02 | 0.86 | 5.64 | 2.47 |
| A320 | 3.15 | 1.57 | 6.58 | 4.04 |
| A380 | 2.00 | 0.75 | 5.29 | 2.03 |
| B-2A | 1.79 | 0.85 | 1.66 | 0.84 |
| C172P | 4.75 | 0.46 | 5.33 | 0.77 |
| C182 | 0.96 | 0.46 | 1.68 | 0.48 |
| F-15C | 3.24 | 1.67 | 5.01 | 3.04 |
| F-35B | 2.48 | 1.22 | 2.07 | 0.92 |
| Learjet 35A | 1.32 | 0.59 | 2.50 | 1.10 |
| Mosquito | nosed over | nosed over | 18.43 | 9.93 |
| PA-28 | 3.14 | 0.43 | 14.78 | 2.02 |

**Verification.**
- `every_landplane_rolled_out_dry_or_wet_as_15_kt_of_crosswind_arrives_stays_within_5_m_of_the_centreline`
  (was `..._rolled_out_dry_as_...`; renamed in `tests/CMakeLists.txt` and
  every preset's CI cost table): eleven landplanes asserted within 5 m,
  stopped and unwrecked, dry and wet; the Mosquito named both ways and
  checked to be what is named (dry: wrecked on her nose; wet: stopped on
  the runway, inside its 30 m, and still past 5 m, so the test says to
  assert her when she is fixed). Coverage asserted: 12 rolled both ways,
  22 asserted, 2 named, 4 left out (flying boat, 747-400, F-22A, J-3 Cub)
  of 16. **Seen red** with the track term and the flaps taken out: the
  737-300, 787-8, A320, A380, C172P, F-15C and PA-28 5.0 to 14.8 m wet.
- Sources: FAA-H-8083-3 (Airplane Flying Handbook), "After-Landing Roll"
  ("putting maximum weight on the wheels after touchdown is an important
  factor in obtaining optimum braking performance"; "after touchdown, the
  nosewheel should be lowered to the runway to maintain directional
  control") and the short-field landing ("maximum stopping effort is
  achieved by retracting the wing flaps, adding back pressure ... and
  applying heavy braking"); the crosswind after-landing roll's "aileron
  control must be held fully toward the wind" (tried 2026-10-29, not kept).
- The full suite on Linux release.
