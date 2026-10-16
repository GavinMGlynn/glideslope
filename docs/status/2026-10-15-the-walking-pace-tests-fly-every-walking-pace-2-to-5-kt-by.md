### The walking-pace tests fly every walking pace, 2 to 5 kt by half a knot, 2026-10-15 — item done

**What is still not true first.**
- **The Mosquito weathervanes rolling up from rest in 15 kt across**: at
  0.3 to 1.7 kt, her rudder full, she swings 46.6 degrees off the runway's
  heading before she is straightened, because the rudder bar's steering
  brake fades with her speed to nothing at a stop (lander.cpp). Not judged
  here - the heading is judged from the hand-over, as before - and added to
  the open Phase 10b item "The Mosquito swings below walking pace".
- **The rolling ceiling is not counted handed over at 3 kt or less** on a
  dry runway or in 15 kt across (named: at 3 kt four never reach it and
  the rest only just), nor at 2 kt wet, nor below 3.5 kt wet on the Cub and
  the Mosquito, whose brakes go on as on a dry runway: her brakes stop her
  first. It is counted in the 156 runs at 3.5 kt and more, and at 2.5-3 kt
  wet on the eleven nose-wheel aeroplanes.
- **These tests now cost about 13 times what they did**: 12 s each before,
  three tests; now six tests of 63 to 94 s here (linux-debug, four at once),
  139 to 216 s on CI's linux-debug (run 38690215083), against its 900.

**The change.** `walking_pace_then_stop` (tests/unit/test_lesson.cpp) flies
every landplane (13; the flying boat named) at every walking pace from 2 to
5 kt by half a knot, each run in two parts, judged throughout:
- **Held at her pace** for 25 s: rolled up from rest on the throttle's slow
  ramp, then the test's hand on the throttle (`walking_pace_hand`, 0.05 a
  second a knot either way, as holding the Cub's pace in her own test), the
  rollout's rudder steering her. **Asserted**: at least 18 s of the 25
  within half a knot of the pace. Every run holds it for **20.6 to 25.0 s**
  (the C182 in 15 kt across the least, 4.4 s at most coming back from the
  break-away's overshoot).
- **Then handed to the rollout** at the pace, as before: its autobrake stops
  her, the hand holds at no more than her taxi power - the throttle that
  held her pace - for half a minute, then lets go for a minute.
Judged, from the roll-up to the stop: nothing but her wheels touches the
runway (`sim::GroundJudge`), she does not go over, her nose above -5
degrees and her wings within 15; after the hand-over, the autobrake's
rolling and standing ceilings and her heading within 30 degrees, as
before. **Coverage asserted**: 13 landplanes by the test's paces, every run
held and judged, and the rolling ceiling reached in every run but those
named. Each condition is two tests, 2 to 3.5 kt and 4 to 5 kt -
`every_landplane_rolling_at_a_walking_pace_of_2_to_3_5_kt_...` and
`..._of_4_to_5_kt_...`, dry, `on_a_wet_runway` and `in_a_15_kt_crosswind` -
in place of the three at 3 kt, to keep each within CI's time.

**Two findings, both in the test's pilot.**
- **Her idle rolls most of them past a walking pace, brakes off.** Rolled up
  and held on the throttle alone, the throttle at idle, the C182 ran to 20
  kt, the F-15C to 18, the Cub and the F-35B to 9-11, the PA-28 and the
  C172P to 5-9, the Learjet to 3-6 (dry, linux-debug): held at their pace
  for 0.5 to 1.9 s of 40. So the hand rides both brakes as a pilot taxis
  (`walking_pace_brake`: on at 0.02 a second for each knot over, 0.05 a
  knot proportional) - **0.028 to 0.110** of brake at most, the Mosquito
  in 15 kt across the most.
- **Brakes off, the Mosquito ground-looped from rest.** Her tail wheel
  castors and her rudder does nothing at a crawl: at full rudder she swung
  from 0.3 kt and was 136 degrees round by 4.5 kt, held there turning
  circles at full power, and handed over she turned 49 to 180 degrees, her
  nose down to -7 degrees. The hand now works her brakes as the rollout's rudder
  bar does (lander.cpp: past half the rudder's travel the brake on the side
  the nose is wanted, up to 0.6, fading below walking pace with her speed),
  and she turns at most **4.4 degrees** after the hand-over at any pace.
  The earlier "turns at most 17.2 at 3 kt" and "109 at 4.5 kt across" were
  measured with the test's brakes held off in the roll-up. The old 3 kt tests never saw it: they
  judged her heading only from the hand-over, and their roll-up had already
  turned her about 70 degrees.

**What each flew** (linux-debug; held = seconds of 25 within half a knot of
the pace, over 7 paces; then the most roll, least pitch, and most turn
after the hand-over in any run):

| | dry | wet | 15 kt across |
|---|---|---|---|
| 737-300 | 24.3-25.0 s; 0.0°, 0.5°, 0.0° | the same | 25.0; 0.1°, 0.5°, 0.2° |
| 787-8 | 23.7-25.0; 0.0, -0.8, 0.0 | the same | 25.0; 0.0, -0.8, 0.1 |
| A320 | 24.2-25.0; 0.0, 0.0, 0.0 | the same | 25.0; 0.0, 0.0, 0.2 |
| A380 | 23.6-25.0; 0.0, -0.7, 0.0 | the same | 23.6-25.0; 0.3, -0.7, 0.0 |
| B-2A | 23.6-25.0; 0.0, -0.3, 0.0 | the same | 23.9-25.0; 0.1, -0.2, 0.2 |
| C172P | 23.2-25.0; 0.1, 2.2, 0.1 | 23.2-25.0; 0.1, 1.9, 0.1 | 23.9-25.0; 0.1, 2.0, 0.3 |
| C182 | 21.9-22.1; 0.1, -3.0, 0.2 | 21.9-22.1; 0.1, -3.2, 0.1 | 20.6-23.3; 0.2, -3.0, 0.2 |
| F-15C | 25.0; 0.0, 1.2, 0.0 | 25.0; 0.0, 1.0, 0.0 | 25.0; 1.7, 1.1, 0.2 |
| F-35B | 24.1-25.0; 0.0, -0.9, 0.0 | 24.1-25.0; 0.0, -1.0, 0.0 | 23.8-25.0; 0.1, -1.0, 0.2 |
| J-3 Cub | 23.2-25.0; 0.1, 11.8, 0.0 | the same | 24.4-25.0; 4.3, 11.8, 0.3 |
| Learjet 35A | 24.3-25.0; 0.0, 0.3, 0.0 | 24.3-25.0; 0.0, 0.2, 0.0 | 25.0; 0.1, 0.3, 0.2 |
| Mosquito | 22.5-24.4; 0.1, 11.4, 4.4 | the same | 21.9-22.4; 0.1, 11.5, 0.2 |
| PA-28 | 22.7-25.0; 0.0, 3.4, 0.1 | 22.7-25.0; 0.0, 3.2, 0.1 | 23.0-25.0; 0.0, 3.7, 0.3 |

No strike and no going over in any of the 273 runs. Wet holds as dry: the
runway's condition changes only the rollout's braking.

**Where the rolling ceiling bites**, measured: handed over at 2 kt every
landplane is below walking pace in 0.0 to 0.28 s, her autobrake at 0.00 to
0.14. Dry and in 15 kt across, at 3 kt the C172P, the F-15C, the Cub and
(dry) the Mosquito reach 0.27 to 0.296, after 0.63 to 0.71 s rolling; the
rest reach 0.3 after 0.75 to 0.91 s - too near the line to count on another
platform, so 3 kt is named for all, and those that reach it there are
counted and said. From 3.5 kt every landplane reaches it, after 0.78 s or
more (the F-15C in 15 kt across the least). Wet, the nose-wheel
aeroplanes' brakes go to all of the pedals from the touch, and every one
reaches it from 2.5 kt; the Cub and the Mosquito, braked as on a dry
runway, as dry.

**Seen red**, linux-debug:
- **Brakes off in the hold** (the old roll-up's hand): 65 wrong at 2-3.5 kt
  dry and 88 at 4-5 kt across - 61 runs held at their pace for 0.4 to 17.6
  s, autobrakes wound to 0.8 standing against their idle, the Mosquito
  turned 105 to 149 degrees and struck her airframe at 5 kt, 4.43 s after
  the hand-over, at -4.5 degrees of pitch. Restored, green.
- **The rolling ceiling made the standing one** (`walking_pace_ceiling`
  returning 0.45 from 2 kt): every counted run failed, and only they - 39 of
  39 in each 4-5 kt test, and at 2-3.5 kt 22, 23 and 35 wrong, wound to 0.32
  to 0.45 rolling. Restored, green.

**Cost.** Main's three tests took 12 s each here (linux-debug, three at
once); the six take 62.8 to 93.9 s (four at once). `tests/ci_costs/`
carries them in all seven presets: measured on CI run 38690215083 (the
shards that finished before a later push cancelled it) in linux-debug, 139
to 216 s, linux-release, 7 to 11, and macos-release, 6 to 12; in the other
four, estimates - the local time scaled by each preset's cost of
`the_787_8_and_a380_handed_over_at_any_half_second_...` (146 s here):
windows-debug 108 to 162, macos-debug 56 to 83, windows-release 44 to 65,
windows-clang 18 to 27. The estimate for linux-debug was 229 to 342 s.

**Verification.** In linux-debug, the `-march=x86-64-v2` debug build and
linux-release: the six new tests, the Cub held at every walking pace, the
gone-over test, the tail-wheel stance test, the braked stop and
stick-central Cub tests in 15 kt across, and the wet and steady crosswind
roll-outs - 13 tests - pass. The six tests' figures are the same in all
three to the tenth (above), but the Cub's in 15 kt across in
linux-release, held 23.6 to 24.7 s; and the same 19 named runs reach the
rolling ceiling all the same in each. In linux-release the six take 6.9 to
9.1 s.
