### The 747-400 lands at Boeing's published approach speed, her flaps raised by her manoeuvre speeds; over-rotated on her wheels her tail strikes a degree low - 2026-11-01

**What is still not true first.** The item stays open:

- **Held fully back from the roll, she strikes her tail on her wheels at
  11.5 degrees**, where her contacts make 12.7:
  `an_over_rotated_take_off_strikes_the_tail_and_its_debrief_says_so` holds
  a strike on the wheels within a degree of her contacts. **Her contacts are
  not what is wrong.** Moving her mains from station 1554 to 1404 brought
  her contacts' strike from 14.5 degrees (TAIL_STRIKE, 281 in above her
  mains over 1,087 in) to 12.7 (TAIL, 199 in over 881) - Boeing's AERO
  tailstrike article gives the 747-400 12.5, lift-off near 10. Before the
  move she struck with her wheels off; now she rotates on them, and at the
  strike (clean, stick fully back, 151 kt, 874,766 lb) her main legs are
  compressed 1.68 ft, which lowers her tail 20 in: 11.5. Her model's two
  main legs, 150,000 lb/ft each where she has four main gears, sink about
  2.5 ft standing at 875,000 lb. Raising her tail contact to strike at 12.5
  on those legs would put her contacts at 13.8, a degree and more from both
  Boeing's figure and the test. The likely fix is her main legs' stiffness from a
  747 load-stroke curve (DTIC ADA250134 has them; it could not be fetched
  here). Not fixed.
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
2. **Stall recovery: a data fix.** Her recovery left her landing flap, 30,
   out at the lesson's 20,000 ft: at 754,313 lb her full thrust, 92,500 lb
   (Mattingly's lapse, 0.40 of sea level at Mach 0.36), was less than her
   drag, 96,500 lb (a CD of 0.18 at CL 1.41; CR-2144 table IX-1 has 0.263
   at 1.76), and she recovered to 167 kt descending at 150 ft/min. Her
   figures now give `go_around_flaps_deg="20"`, her go-around flap (her
   FCOM's go-around procedure, as her crews quote it; CR-2144's power
   approach flap), which the recovery raises her flaps to: she recovers
   level, losing 1,114 ft. Seen red: named, the test failed "recovered:
   take its name off"; the name is gone.
3. **Lift-off hand-over: a flap manoeuvre schedule, as data.** The climb out
   raised a transport's flaps from 400 ft at her climb speed, 170 kt - under
   her clean wing's 181 kt stall at 754,313 lb - and she stalled level at
   650 ft and sank back to the runway. Figures files may now give
   `<flap_manoeuvre_speeds>`, knots over the approach speed for her weight
   at each flap; a flap comes up to a setting only at its speed, and the
   climb out accelerates her to it (sim::Departure::manoeuvre_kts,
   climb_out_kts). The 747-400's: Vref30 + 80 up, + 60 flaps 1, + 40 flaps
   5, + 20 flaps 10, + 10 flaps 20: Boeing's 747 Flight Crew Operations
   Manual, D6-30151-400 (October 1, 2009), Flap Maneuver Speeds, PI.20.3
   for the 747-400F/CF6-80C2B1F and PI.10.3 for the PW4056, identical, with
   its rule: retract "when at and accelerating above the recommended flap
   speed for the new position". Read from a third-party copy
   (pdfcoffee.com), marked "do not use for flight"; Boeing does not publish
   it. Her crews quote the same on PPRuNe. Her model's lever stops at 15,
   flown at flaps 10's speed. No other aircraft
   gives one, so every other's flaps come up as before. Seen red: the
   lift-off hand-over and the 747-400's lift-off sweep failed without it
   (she touched the runway again at every one of 21 hand-overs) and pass
   with it.
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

Her CBD orbit recording (`cbd-orbit-747-400-anthropic.jsonl`) was made
again live of Claude Haiku 4.5 with the owner's key, as her brief's speeds
changed, and plays back; no key is in it.

**Verification.** linux-release, `ctest -LE nightly -j4`: see the PR; the
tail-strike test above is the known red.

**The Google terrain on Vulkan failed in the full suite** ("google via
ion-token drew no terrain at all"), and again run alone on this branch's
build; run alone on the main checkout's build it failed once and passed the
next time. It touches nothing this item changed (sim/ and the 747's data):
intermittent, and not this item's.

**Her lesson tests fly her at her approach speed's loading** where she has
no climb figure (load_as_its_figures_were_measured): her speeds are Boeing's
at 630,000 lb, and at her model's 754,313 lb she stalled 11 kt above the
speed her stall lesson warns at, so the stall-lesson tests never warned her.
