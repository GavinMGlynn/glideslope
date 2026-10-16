### Every landplane braked to a stop in a crosswind, a tail-wheel one as one is braked, 2026-10-15 — item still open; one found

**What is still not true first.**
- **The item stays open on the Cub's crosswind pitch term** (a new Phase 9
  item, "The J-3 Cub's nose is pushed down by a crosswind"; below). The sweep
  flies her stick back, as a tail-wheel aeroplane is braked, and she passes
  there. Flown stick central she still goes over: solo, at brakes 0.3 to 1.0
  (67 to 83 deg/s). So does her walking-pace run in 15 kt across, whose
  test's hand flies her stick central and still names her
  (`tests/unit/test_lesson.cpp`, not touched: another item's agent is
  working in it).
- **The Mosquito goes past the bound at brakes 0.6 to 1.0, and the item
  stays open on it.** There her braking friction (0.49 to 0.80) is past her
  tip ratio (0.40), and her brakes tip her onto her nose (30 to 52 deg/s,
  striking). Each run is flown and named with both figures, the levels
  pinned. The plan's verification, unchanged, does not excuse it. That it is her geometry, not her aerodynamics: the same runs gave
  the same pitch rates with her pitch-alpha and pitch-stall terms on
  `aero/qbarUW-psf` (tried in the build's copy only): 29.9 and 29.9 deg/s at
  0.6. Whether a real Mosquito tips at that grip is not measured here.
- **A tail-wheel aeroplane is judged only as one is braked**: stick held
  fully back and full aileron into the wind.
- **The full suite was not run**: only the test below, in three builds.

**From the review of #196.** The first commit ticked the item. It was
judged pilotless, and its tip-ratio cut hid a Cub failure that was her
crosswind pitch term, not her geometry. So the item is open again, its
verification as it was. Changed:
- **Loaded as she is braked.** The Cub carries her figures' `solo` loading:
  a 170 lb pilot in the rear seat and her 72 lb tank, 922 lb. She had been
  flown at 752 lb, with no one in her. The Mosquito carries the pilot and
  navigator, 200 lb each, that her model already holds.
- **The tip ratio is measured as she has settled**, after the five-second
  stand. It uses her pitch then, each main wheel's contact raised up its
  strut by its compression (`gear/unit[i]/compression-ft`), her loaded
  centre of gravity, and the least over her two main wheels. If she has not
  two, the run is wrong and the ratio is not used. The figures are the Cub
  0.46 at 12.03 degrees and the Mosquito 0.40 at 11.72.
- **Past her ratio she is flown, not skipped.** One that stays on her
  wheels there is judged like any other. The Cub, solo, stays on her wheels
  at every level (9.5 deg/s at full brakes), so she is judged at all ten.
  The Mosquito is judged at 0.1 to 0.5: 0.5 is past her ratio, and she
  stays up (3.3 deg/s). Which runs past it are named is the second review's
  rule (below).
- **The message**: "slowing at that grip tips her", not "held still".
- **Asserted per aeroplane**: every level of each accounted for; each
  nose-wheel aeroplane judged at all ten; the Cub past her ratio at five
  levels, judged at ten; the Mosquito past hers at six, judged at five,
  tipped at five; the Cub loaded solo and the Mosquito as her model is; and
  the flying boat named is the S.23.

**From the second review of #196.**
- **A run past her ratio is named only if her brakes tipped her.** The
  first rule named any failure past the ratio with her nose below level,
  so it would have named a nose-over the air caused, or a wing going down.
  Now, all of these must hold:
  - her tail wheel was on the runway as the brakes came on (a tail the air
    lifted in the run-up is not her brakes' doing);
  - it came up while she braked, with her gear's pitching moment
    (`moments/m-gear-lbsft`) nose down and the air's (`m-aero-lbsft`) not;
  - she went wrong with her nose below level;
  - no wing went down past 10 degrees before she went wrong;
  - her attitude stayed a number.

  A grip measure was tried first, the gear's force along her body over her
  weight at the step she first went wrong. It does not discriminate: the
  Mosquito's tip at 0.6 begins at a grip of 0.23 to 0.33 (body axis, her
  tail still carrying weight), and the first wrong step is her nose
  striking, late in the tip. Her tail leaving the ground is the start of a
  tip, and the moments then say whose it is.
  - The Mosquito at 0.6 to 1.0, stick back: her tail came up with gear
    -10,136 to -39,957 ft-lb and air +12,796 to +22,614 (her elevator
    holding). Named, and the levels are pinned: 0.6, 0.7, 0.8, 0.9, 1.0.
  - **Seen red, refusing an aerodynamic nose-over**: flown stick central,
    the solo Cub's tail came up at every level with the air nose down
    (-396 to -562 ft-lb). She went over at 0.3 to 1.0 (67 to 83 deg/s), and
    all eight were judged and failed, the five past her ratio (0.6 to 1.0)
    included. The rule refused them. Reverted, green.
- **A NaN is latched**: an attitude or pitch rate that is no number ends the
  run as a fault of its own, never named. A later number cannot overwrite
  it, and her least pitch never takes one in.
- **Renamed** to state what it pins:
  `every_landplane_braked_to_a_stop_in_a_15_kt_crosswind_keeps_her_pitch_rate_under_a_bound_unless_her_brakes_grip_tips_her`.
- **Her stance is found once per aeroplane**, not at every level, and the
  tail-wheel aeroplanes are counted by it. That they are the Cub and the
  Mosquito is asserted by name.
- **The plan's two Phase 9 items are cut to a few lines.** The open item
  now names the Mosquito as missing: she goes past the bound at 0.6 to 1.0,
  and its verification, unchanged, does not excuse that.
- **Re-verified** in linux-debug, `-march=x86-64-v2` debug and
  linux-release. All pass; the debug builds agree, and release is within
  0.1 deg/s of them. The sign-error red was seen again on the new code: the
  Cub's ratio came out as 0.01, past it at all ten levels.

**From the third review of #196.**
- **Her tail must lift before she goes wrong.** That it lifted is latched
  only while she has not yet gone wrong. A run that spikes or strikes with
  her tail wheel down, and lifts after, is judged.
- **"Nose down" is read at the step she goes wrong**: her pitch rate then
  must be nose down. Her least pitch over the run is no longer used. The
  Mosquito's named runs were pitching at -9.3 to -20.3 deg/s at that step.
- **A ceiling on a named run's pitch rate: 60 deg/s**, just above the
  Mosquito's 30 to 52. A tip goes no faster than her weight swings her; a
  gear impulse (the -376 deg/s this item began with) at 0.6 to 1.0 is
  judged, and fails. Asserted with her named levels.
- **Seen red, each clause on its own**, each reverted to green:
  - **The air's clause.** With it removed and the stick-central Cub, the
    ceiling alone refused her nose-overs past her ratio (75.7 to 82.9 deg/s).
    With the ceiling also lifted, she was named at 0.6, 0.7, 0.8, 0.9 and
    1.0. So the air's clause by itself refuses them: at her tail's lift-off
    her gear was nose down (-121, -300, -493, -698 and -895 ft-lb) but so
    was the air (-477, -467, -432, -396 and -409).
  - **The ceiling.** Lowered to 40 deg/s, the Mosquito at 0.8 to 1.0 (44.0
    to 52.3 deg/s) was judged and failed.
  - **The order.** Made to go wrong at 0.5 deg/s, before her tail lifts,
    the Mosquito at 0.6 to 1.0 was judged and failed: "not lifted ...
    before she went wrong".
- **Re-verified** in linux-debug, `-march=x86-64-v2` debug and
  linux-release (figures below, unchanged).

**Why the Cub went over** in the first sweep, traced step by step in
linux-debug with a probe printing her moments (not committed). These are
her pilotless runs, at 752 lb, her centre of gravity 5.8 in behind her
main wheels and 56.6 in above them, structurally:
1. **Her angle-of-attack pitch term counts the crosswind.**
   `aero/moment/Pitch_alpha` is Cm-alpha times the angle of attack times
   `aero/qbar-psf`, the whole dynamic pressure.
   - Standing in 15 kt straight across, her angle of attack is 22 degrees
     and her sideslip -82, so the angle means nothing. The term still gives
     -534 ft-lb, nose down, against about 1,080 her weight holds her tail
     down with.
   - Run up to 10 kt at full power stick central, her tail came up: pitch
     11.8 to 2.3 degrees.
   - With the term on `aero/qbarUW-psf` (tried in the build's copy only),
     it gave -8 ft-lb at rest and her pitch was 11.3 at 10 kt.
   - Braked stick back as she stopped, her angle of attack ran from 16 to
     53 degrees as u went to nothing, and the term from -500 to -1,334
     ft-lb, as her elevator's propwash faded. At 0.8 she went over at 38
     deg/s; on `qbarUW` she did not (6 deg/s).
   - This is the alpha-rate terms' fault (JSBSim issue #89) in a second
     term. It is a new Phase 9 item, since the change moves her figures and
     every model has such a term.
2. **The test's pilot: stick central.** With her tail up at 2.3 degrees,
   her centre of gravity was 8.1 in behind her main wheels and 56.3 in above
   them, so a grip of 0.14 tipped her. She was braked at 0.2 (friction
   0.18), against an engine still giving 182 lb of thrust and taking 2 s to
   fall to 50. A tail-wheel pilot brakes with the stick held fully back
   (FAA-H-8083-3, tailwheel airplanes), and the sweep flies her so.
3. **The crosswind rolled her.** Stick back on Departure's ailerons (-0.3
   to -0.4), she leaned 16 to 21 degrees onto her downwind wheel and struck
   her wing. With full aileron into the wind, as she is taxied, she stays
   within 3 degrees while rolling.
4. **The geometry.** A tail-wheel aeroplane slowed by a braking friction
   past her tip ratio goes onto her nose. JSBSim's braking friction is
   rolling + brake x (static - rolling): 0.02 + 0.78 x brake for both. This
   alone is the Mosquito's fall (above).

**The change.** The sweep is renamed
`every_landplane_braked_to_a_stop_in_a_15_kt_crosswind_keeps_her_pitch_rate_under_a_bound_unless_her_brakes_grip_tips_her`
(tests/unit/test_departure.cpp). It judges every landplane at every tenth of
brake, 0.1 to 1.0, where it judged the nose-wheel ones at five levels.
- A tail-wheel aeroplane is flown stick fully back with full aileron into
  the wind, loaded as above, and her tip ratio is measured as above.
- A run ends at 45 degrees of pitch or roll, or at no number, before
  JSBSim's tables see NaN.
- Coverage: 150 runs, 145 judged and 5 named (the Mosquito at 0.6 to 1.0).
- The nose-wheel aeroplanes are flown as before. Their figures do not move:
  the worst is still the C182's 12.1 deg/s at full brakes.

**Verification.** The test passes in linux-debug, in a debug build with
`-march=x86-64-v2` (no fused multiply-adds, as CI builds), and in
linux-release.
- The two debug builds print the same figures. Linux-release differs from
  them by at most 0.1 deg/s in four runs.
- The Cub reaches at most 9.5 deg/s. The Mosquito, judged, reaches 3.3.
- **Seen red**, each reverted to green:
  - With the tail-wheel aeroplanes flown stick central on Departure's
    ailerons (the old pilot), the solo Cub went over at 0.3 to 1.0 (67 to
    83 deg/s). All were judged, the ones past her ratio included (the
    second review's rule, above).
  - With a sign wrong in the tip ratio (her height's term subtracted), the
    Cub's ratio came out as 0.01. The test failed on her count: past her
    ratio at ten levels, not five.
- **The selftest hash does not move**: no simulation code or model changed.
