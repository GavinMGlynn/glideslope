### The owner's decisions of 2026-10-09: the F-15C held to her manual's speed, the touchdown sink sourced, learnt landings for others to Later — one item closed, one rewritten and open

**What is still missing, first.** **The F-15C does not meet her manual's
speed.** Held at full aft stick from 200 knots at 10,000 ft, clean (36,946
lb), idle, she settles at 116.5 to 116.6 KCAS over the last 20 of 90 s,
sinking 7,683 ft/min, at 42.4 to 42.5 degrees of alpha; T.O. 1F-15A-1
section VI says 100 knots or less. Why: her wing loading is 60.8 lb/ft2,
so 116.6 knots (q about 46 lb/ft2) means a resultant force coefficient of
about 1.32, and 100 knots (q about 34) would need about 1.79 - a third
more lift and drag than her model's tables give at the alpha she
settles at, and the tables end at 50 degrees. No source was found for her
lift and drag past 50 degrees at low speed: the FUN3D paper (Yang et al.,
AIAA 2015-0549) is transonic and at most about 30 degrees. The manual
gives no weight for its figure, and an indicated airspeed at 45 units
reads through the pitot's error at that attitude, which no source here
gives either; so the model was not changed. The item stays `[ ]`.

**The decisions** (`REQUIREMENTS.md` section 9, Closed 2026-10-09):
- The F-15C is held to the manual's speed only; her angle of attack is
  reported, not judged, since no primary source turns the manual's units
  into degrees (NASA TM-72861 and the F-15 high-alpha papers do not).
- The touchdown sink is sourced from NASA's go-around criteria study:
  Zaal et al., "Go-Around Criteria Refinement for Transport Category
  Aircraft", AIAA Journal of Air Transportation, NTRS 20205010611, whose
  touchdown criterion is 6 ft/s, 360 ft/min; a normal touchdown is 100 to
  300.
- "Learnt landings for other aircraft" moves to Later: each needs its own
  reinforcement-learning training, and the 172P's is retrained first.
- The Learjet's centre-of-gravity height and the F-35B's approach angle of
  attack stay in the goal without a public source; the Cherokee's stall
  recovery raises her flaps to the handbook's go-around setting. (Recorded
  only; no code here.)

**The changes.**
- The ten figures files that give `touchdown_fpm` (737-300, 747-400, 787-8,
  A320, A380, B-2A, F-15C, F-22A, F-35B, Learjet 35A) cite the study in
  their comment, and `src/sim/lander.hpp`'s `ApproachSpeeds::touchdown_fpm`
  does too. No value changed: 200 for those ten, the autopilot's 40 for the
  other six.
- `every_aircrafts_touchdown_sink_is_within_nasas_go_around_criterion`
  (new): walks the catalogue, all 16 aircraft, asserting the count; each
  one's target - its figures' `touchdown_fpm` or the autopilot's 40 - is
  above 0 and at most 360 ft/min, and for the 14 the AI lands (all but the
  747-400 and F-22A, which publish no stall speed) `landing_speeds` gives
  that target. Seen to fail: with the criterion set to 199 it went red on
  the 737-300's 200, and was reverted.
- `the_f15c_held_at_full_aft_stick_settles_past_the_peak_of_her_lift` is
  now `the_f15c_held_at_full_aft_stick_settles_no_faster_than_117_knots_against_her_manuals_100`:
  same flight; it no longer judges alpha (it prints it), asserts her speed
  settled within 4 knots over the last 20 s and is no more than 117 - a
  bound on today's figure, to be lowered to the manual's 100 when met. Seen
  to fail: with the bound at 110 it went red at 116.6, and was reverted.
  Renamed in the CI cost files; the new test costed 1 s.

**Verification.** One linux-debug ctest run of the two tests above,
`the_sink_a_flare_touches_down_at_is_read_and_refused_past_what_the_gear_takes`
and `every_published_figure_has_a_flight_and_every_flight_a_figure`: 4 of
4 passed. The selftest hash is untouched: no model or flight code changed.
