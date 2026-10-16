### The alpha-rate terms use the dynamic pressure without its sideways part, 2026-10-14 — item done

**What is still not true first.** **The Mosquito is not changed**: her model
is written by hand - no script makes it, an exception CLAUDE.md's
"`assets/jsbsim/` is made, not written" does not list - so changing her
`pitch-alpha-rate` term means editing an asset by hand, which is for the owner
to decide. Until then it still counts the crosswind; an open item in Phase 9.
**The B-2 has no alpha-rate term.** **Tail-wheel aeroplanes are not judged**
by the new test: in #196's sweep the Cub goes over in a 15 kt crosswind run-up
and the Mosquito noses over braked at 0.6 and above, which may be the real
nose-over hard braking gives a tail-wheel aeroplane. **Whether a braked
aeroplane's nose may go below -5 degrees is not judged**: the C182's goes to
-5.7 at full brakes, calm or not. **The full suite was not run.**

**The cause.** JSBSim's angle of attack is atan2(w, u) and its rate is
worked from u, w and their rates. As an aeroplane braked in a crosswind
stops, the air meets her from the side, u and w go to nothing, the angle of
attack is undefined and its rate swings by hundreds of degrees a second with
the wind steady. Every model here multiplied that rate (`CLadot`, `Cmadot`
and their like) by `aero/qbar-psf`, the whole dynamic pressure, crosswind
included. This is JSBSim issue #89, "Pitch oscillations at rest, Brakes ON,
with pure crosswind"
(https://github.com/JSBSim-Team/jsbsim/issues/89): Bertrand Coconnier traced
it to those terms and advised `aero/qbarUW-psf`, the dynamic pressure of u
and w alone, since the sideways air makes no lift on an unswept wing; the
FlightGear C172P adopted it (https://github.com/c172p-team/c172p/issues/1297).
JSBSim's master has no core change for it (50 commits past our pin, v1.3.1,
none in the gear, the friction solve or the integrators); open issue #1489
concludes the same of the friction solver's non-convergence near a stop in
wind. JSBSim's gear options were tried on the C172P and F-15C and are not the
cause: `dynamic_friction` is used only by airframe contacts, not wheels
(FGLGear::ComputeJacobian), and changed nothing; a rolling friction of 0.05
and a rebound damping of twice the damping left both striking.

**The fix.** `tools/alpha_rate.py`: every function of a model whose product
names `aero/alphadot-rad_sec` has its `aero/qbar-psf` replaced by
`aero/qbarUW-psf` (or `aero/qbar-area` by that times `metrics/Sw-sqft`), and a
model with none fails the script. Fourteen make scripts call it (737-300,
747-400, 787-8, A320, A380, C172P, C182, F-15C, F-22, F-35B, J-3 Cub, Learjet
35A, PA-28, and the S.23's `CLad` and `Cmadot` in her
`Systems/datcom_aero.xml`, from the review of #198): 21 terms. The models are
made again and the committed-model checks pass.

**Verification.**
`every_nose_wheel_landplane_braked_to_a_stop_in_a_15_kt_crosswind_keeps_her_pitch_rate_under_a_bound`:
the thirteen nose-wheel landplanes (the Cub, the Mosquito and the S.23 named),
each stood, given 15 kt straight across, run up to 10 kt and braked to a stop
at 0.2, 0.4, 0.6, 0.8 and 1.0, keep their pitch rate under 20 deg/s from the
brakes on, strike nothing and stop. The worst is now 12.1 deg/s, the C182's
nose dipping as full brakes come on at 9.6 kt, in both the default build and
one built as CI's Rocky 9 and Ubuntu jobs build (`-march=x86-64-v2`, no fused
multiply-adds). **Seen red on main's models** in that build: the C172P at 38
to 62 deg/s, striking her tail at 0.6 to 1.0; the F-15C at 24 and striking,
or not stopping at 0.2 and 0.4; the 787-8 at 28 and striking; the F-22
striking at 0.8. Applied to #195's branch (with #197), the five tests that
failed there on CI pass in that build (that trial also changed the
Mosquito's term), the learnt half-fuel test asking only
for its named start's name to come off.

**The landplanes' flight figures barely move**: `glideslope_cli figures`
for every aircraft before and after, in both builds - every landplane figure
is identical but the 737-300's and 787-8's take-off field lengths, by 0.01
ft. They fly with no crosswind, where the two pressures differ by the cosine
squared of a sideslip near zero. **The S.23's water take-offs do move**, all
still in range (default build): the long-range take-off from 34.09 to 34.75
s (range 25.93 to 35.08) and its run from 797.5 to 823.2 yd (675.8 to
914.3); the standard take-off from 26.82 to 26.83 s, against a top of 27.00.
The figures, take-off, landing, stall, gust, crosswind, rollout and wet tests
and the committed-model checks (412, by name) were run in both builds before
the S.23 was added; then her 51 (her figures, hull, alighting, water and the
committed-model checks) in the default build, all passing. Rebased on #197
after the host restarted, the 94 tests matching the S.23, seaplanes, hulls,
water, alighting, crosswind, alpha rate and the committed-model checks pass
in both builds, her figures all in range in both; the selftest hash does not
move from #198's (`de9bf01f96427593`; `fefe2a7f5d0f52a5` without FMA).

**The selftest hash moves**, from `182dd6c996e0ee4c` to `de9bf01f96427593` on
linux-release (the `-march=x86-64-v2` build: `6e988ba216e5ee07` to
`fefe2a7f5d0f52a5`). The selftest flies the C172P, whose `CLadot` and
`Cmadot` now take the dynamic pressure without its sideways part: in its
log's flight they differ by the cosine squared of her small sideslip, which
moves the last bits of the state.
