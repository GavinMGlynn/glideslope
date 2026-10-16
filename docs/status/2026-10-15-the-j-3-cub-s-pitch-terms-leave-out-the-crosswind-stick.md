### The J-3 Cub's pitch terms leave out the crosswind; stick central she keeps her tail down in 15 kt across, 2026-10-15 — item still open; one found

**What is still not true first.**
- **Only the Cub's terms are moved.** The item's verification is every
  model's angle-of-attack pitch term, and every one of the other fifteen
  carries the same fault: a pitching-moment term keyed to the angle of
  attack, as a factor or a table's input, on the whole `aero/qbar-psf` -
  `Cmalpha` in the 737-300, 747-400, A320, A380, B-2A, C172P, C182 (and
  `CmDe`), F-15C, F-35B, Learjet 35A and PA-28; `Pitch_alpha` in the
  787-8; `Cma_M`, `CmDh`, `CmDtv` and `Cmq` in the F-22A; `pitch-alpha`
  and `pitch-stall` in the Mosquito; `Cm_basic` and `Cmq` in the S.23
  (Systems/datcom_aero.xml). That list is the helper's own (below), run
  over each committed model without writing it: it finds those terms and
  no others, and applies cleanly to all fifteen. Calling it from their
  fifteen scripts and flying every figure again is the rest of the item,
  not done here: a whole-suite run. Whether their figures in flight move
  is not measured (the Cub's do not, below); the swept wings are where
  issue #89 says the guidance was not investigated.
- **Braked hard stick central she still lifts her tail once in 15 kt
  across**: at her gross weight, braked at 0.5 with the wind from the
  right, her tail wheel leaves the runway for a moment (she stops on her
  wheels) where in calm air it stays down. Her **lift** still counts the
  crosswind: her air's upward force as the brakes come on is 358 lb, against
  104 in calm air. Named in the test, pinned.
- **Her lift on the same footing, tried and not taken**: with her lift
  terms keyed to the angle of attack (Lift_alpha, Lift_propwash,
  Lift_pitch_rate) on `aero/qbarUW-psf` too, the new test passed with
  nothing named, but in the 3 kt walking-pace test in 15 kt across she
  rolled 21.1 degrees and struck her airframe (pitch 15.3, roll 20.7),
  where on main she rolls 3.0 and with only her pitch terms moved 5.4. Why
  is not traced. A new Phase 9 item, "The J-3 Cub is rolled onto her wing by
  a crosswind at walking pace".
- **Rolled at 4.5 kt in 15 kt across at taxi power she still strikes** -
  and it is not her pitch term. The walking-pace crosswind test flown at
  4.5 kt (`held_kts`, in the build only, not committed): on main she
  struck her airframe 5.01 s after the hand-over at 15.2 degrees of pitch
  and 21.1 of roll, and with her pitch terms moved 4.45 s after it at 14.5
  and 20.8. Her nose is up, above her 12-degree stance: she is rolled onto
  her wing. The new item above; the walking-pace item names it.
- **The full suite was not run**: the 83 tests below.

**Why, on the real output** (linux-debug, a per-step trace through the new
test, not committed). Stood on her brakes, stick central, when a 15 kt wind
comes on straight across she pivots a little into it and her angle of
attack - atan2(w, u), with u and w a few feet a second - jumps to 50 to 60
degrees. `aero/moment/Pitch_alpha`, Cm-alpha times that angle times the
whole dynamic pressure (`aero/qbar-psf`, the crosswind's 0.76 psf in it),
gave -1,430 to -1,518 ft-lb, nose down, against the ~1,080 her weight holds
her tail down with: solo, her tail came up 0.38 s after the wind came on,
and she went over standing on her brakes. Every loading, either side.

**Is `qbarUW` right?** JSBSim's own answer, from issue #89 ("Pitch
oscillations at rest, Brakes ON, with pure crosswind",
https://github.com/JSBSim-Team/jsbsim/issues/89): Bertrand Coconnier
showed that the air's component along an unswept wing's span makes no lift
(the Kutta-Joukowski lift of its bound vortices, checked against a panel
method: L = 0.5 rho (U^2 + W^2) S CL); and the guidance then written into the
FlightGear wiki's "JSBSim Aerodynamics" page (dany93, reviewed by
Coconnier, 2019) is that `QBarUW`, the dynamic pressure of u and w alone
(FGAuxiliary: `qbarUW = 0.5 rho (u^2 + w^2)`), "is more appropriate for Cm
than the generic QBar", and likewise for CL - offered as guidance, tested on
the C172, with swept wings not investigated. The FlightGear C172P (c172p-team
issue 1297) moved only her alpha-rate terms. In flight the two pressures
differ by the cosine squared of the sideslip, and the Cub's figures do not
move at the printed digit (below).

**The change.** `tools/alpha_rate.py` gains `with_pitch_alpha_on_qbar_uw`:
every function inside a model's PITCH axis (or the axes a script names)
whose product reads `aero/alpha-rad` or `aero/alpha-deg`, as a factor or a
table's input, has its `aero/qbar-psf` replaced by `aero/qbarUW-psf` (or
`aero/qbar-area` by `qbarUW` times the wing's area); a model with none
fails. `tools/make_j3cub.py` applies it: her `Pitch_alpha` and
`Pitch_propwash` move; its docstring says so, and why not her lift. Standing
in 15 kt across, the air's most nose-down moment on her is now 37 to 239
ft-lb (her figures' loadings, either side), where before it was 1,391 to
2,016 at the step her tail came up.

**The test.**
`the_j3_cub_flown_stick_central_in_a_15_kt_crosswind_keeps_her_tail_wheel_on_the_runway_wherever_she_does_in_calm_air`
(tests/unit/test_departure.cpp). At every loading her figures give (solo,
trainer, gross; her model's own, with no one aboard, has no pilot and is not
flown), in calm air and 15 kt straight across from the left and the right,
at every tenth of brake: stood five seconds, the wind on and ten more, run up
at full power to 10 kt, braked at that level to a stop; elevator central,
ailerons full into the wind (FAA-H-8083-3). Her tail wheel's weight on
wheels is read every step from her settling, with GroundJudge, 45 degrees and
NaN. Standing and run up her tail stays down in every wind; braking in 15 kt
across, wherever it stays down in calm air. Named: the levels where it lifts
braking in calm air too (her brakes' grip, stick central: solo and trainer
0.5 to 1.0, gross 0.6 to 1.0, pinned), and gross, right, 0.5 (her lift,
above; pinned to still lift and stop on her wheels). Coverage asserted: 90
runs; the 60 crosswind runs 25 judged, 35 named.

**Before the test was right**: the first draft read her tail's weight on
wheels from her first step, where she is started a little above her wheels,
and failed every run in calm air; and it flew her ailerons on Departure,
where she leaned 10 to 17 degrees onto her downwind wheel, the crosswind's v
showing in her w, and her empty model's (751 lb, no pilot) tail came up in
the run-up at 8 kt.

**Her figures** (linux-debug): every Cub test - static rpm, rated power,
climb (449.0 ft/min), cruise (64.65 KTAS), glide (9.63:1), stall, the
ceiling, the orbits, the heading holds in a 20 kt crosswind, the landing
distance - prints the same figures to the printed digit with the change and
without (all 18 pass both ways).

**Verification.** In linux-debug: the 83 tests matching
`committed_|cub|j3cub|light_air|light_aero|walking_pace|tail_wheel|landplane|in_a_steady_15|selftest|every_published_figure`
pass, the committed-model tests (all seventeen) among them (one run of
them used a test binary stale from the 4.5 kt probe; the four walking-pace
tests were rebuilt and run again, and pass). In the crosswind brake sweep,
stick back, the Cub's pitch rate falls to 1.6 to 2.8 deg/s, from up to 9.5:
her pitch term was in that too. In a `-march=x86-64-v2` debug build (no
fused multiply-adds, as CI builds) and in linux-release, the 26 tests of
the Cub, the walking pace, the tail wheels, the crosswind stops and the
brake sweep pass, with the figures above the same to the printed digit.
**Seen red**: with main's Cub model put back in the build's copy, the new
test failed on all 60 crosswind runs - her tail came up standing, 0.38 to
0.60 s after the wind came on (the air -1,391 to -2,016 ft-lb then), and
she went over. Restored, green. The
selftest hash does not move: it flies the C172P, whose model is unchanged.
