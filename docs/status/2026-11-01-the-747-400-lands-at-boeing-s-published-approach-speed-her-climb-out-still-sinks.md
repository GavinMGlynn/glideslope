### The 747-400 lands at Boeing's published approach speed; handed over at lift-off, her climb-out still sinks back to the runway - 2026-11-01

**What is still not true first.** The item stays open:

- **Handed over at lift-off, she touches the runway again.** The departure
  raises a transport's flaps from 400 ft once she is within 2 kt of her climb
  speed, 170 kt; at her 754,313 lb her clean wing stalls at about 181 kt (CL
  1.2), so with her flaps up she stalls level at 650 ft and sinks back. Red:
  `every_landplane_handed_over_at_lift_off_climbs_away_through_500_ft_and_never_touches_the_runway`
  and the new `the_747_400_handed_over_at_any_half_second_...` sweep part.
  The fix is a flap-retraction schedule by speed (Boeing's flaps-up
  manoeuvre speed), not done here.
- **Her stall recovery is named, not fixed**
  (`the_ai_pilot_notices_a_stall_coming_and_recovers_from_it`): at 754,313
  lb, 124,000 lb over her maximum landing weight, at the lesson's 20,000 ft
  with flaps 30 and the gear down, full thrust is 92,500 lb (Mattingly's
  lapse, 0.40 of sea level) against 96,500 lb of drag - a CD of 0.18 at CL
  1.41, under CR-2144 table IX-1's 0.263 at 1.76 - so she recovers to 167 kt
  but descends at 150 ft/min. The model is not wrong there; the lesson's
  height or weight for her is unsettled.
- **Her gust landings are named** (`not_yet_landed_in_gusts`): on seed 8 she
  touches at 388 ft/min and climbs 99.7 ft/min in her flare.

**What now works.** She has Boeing's approach speed, 153 kt at 630,000 lb,
flaps 30 (Aerodrome Reference Code and Approach Speeds, rev A, August 2026),
and is the fifteenth aeroplane taught the approach: she lands light and heavy
at 257 to 301 ft/min, in a 15 kt headwind, a 10 kt tailwind and 15 kt across,
and stops on a wet runway in 15 kt across.

**The six failures the model change left, each with its cause:**

1. **Take-off field length 11,687 ft against 10,500 +10%: a model change.**
   Her flapped lift kept CR-2144's slope only up to her clean wing's stall
   angle, a CL of 1.82 at flaps 20 and 2.18 at 30. Boeing's certification
   stall speeds (NASA CR-1756 vol. II page 2.0-37, from D6-30643; read from
   the chart: flaps 20 134 KEAS at 700,000 lb, flaps 30 112 KEAS at 600,000
   lb) make 2.08 and 2.57 on the 5,500 sq ft wing. Her flapped lift now keeps
   CR-2144's slope up to that lift (at 15.6 and 16.7 degrees), then falls as
   her clean wing's does. She stalls at 150 kt at 875,000 lb with flaps 20,
   so V2 is about 181 kt, as 747-400 crews fly it; the field length is
   10,781 ft. Her 153 kt approach is now 1.33 times her stall at 630,000 lb.
2. **Stall recovery: named, see above.**
3. **Lift-off hand-over: still red, see above.**
4. **Glide at 230/235 kt: the test's figure was measured from the old
   model.** `glide-speeds 747-400` now writes 230 (225 stalls), so her file
   says 230 and the band test is
   `the_boeing_747_400_glides_round_its_tightest_orbit_from_230_to_235_kt_without_stalling`.
   Her plan speeds, measured again from her approach speed for her weight
   (`plan-speeds 747-400`), are 213 to 300 kt (were 220).
5. **The pilot's-flare incidence test: the test built its situation
   wrongly.** A full pull for one second took her wing from 4.6 to only 6.2
   degrees - a 747's pitch inertia. The test asserts the situation (the wing
   past the path's incidence and four), so the pull is now held until it is
   past it by half a degree, from one second up to three; every other
   aeroplane is past it in the first second, as before.
6. **15 kt crosswind, "no time measured on main": the test's figure did not
   exist.** She had no approach on main, so no landing; her stop time, 8.8
   s, is measured under the same law main's lander flies.

**Measured figures that moved**: her take-off speeds, rotate 150 kt (was
160), climb 170 (`takeoff-speeds 747-400`); plan 213 to 300; glide 230. The
other failures were counts going from 14 to 15 aeroplanes taught the
approach (and 13 to 14 landplanes, 8 to 9 jets on a wet runway, two to one
not watched for a stall), updated where asserted and in the comments.

**Verification.** linux-release, `ctest -LE nightly -j4`: see the PR's
result; the two lift-off tests above are the known red.
