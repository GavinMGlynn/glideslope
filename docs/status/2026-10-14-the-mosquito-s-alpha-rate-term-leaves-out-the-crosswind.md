### The Mosquito's alpha-rate term leaves out the crosswind, 2026-10-14 — item done

**What is still not true first.** **No test brakes her to a stop in a
crosswind**: #198's test judges nose-wheel aeroplanes only, and in #196's
sweep the Mosquito noses over braked at 0.6 and above, which may be the real
nose-over of a tail-wheel aeroplane braked hard; that is not judged here.
**Her `ci2vel` is unchanged**, as in #198: the term's c/2V is still on the
whole airspeed. **The full suite was not run**: the tests below.

**This supersedes** the entry "The alpha-rate terms use the dynamic pressure
without its sideways part" below, where "The Mosquito is not changed" was
true when written: she now is.

**The cause.** Her `aero/moment/pitch-alpha-rate` multiplied the alpha rate
by `aero/qbar-psf`, which counts the crosswind (tools/alpha_rate.py, JSBSim
issue #89). Her model had no script, so #198 did not reach her.

**The fix.** `tools/make_mosquito_fb6.py` (the entry below) applies
`alpha_rate.with_qbar_uw` to her airframe, its docstring listing the change:
her one alpha-rate term is on `aero/qbarUW-psf`. A new test,
`every_flight_models_alpha_rate_terms_leave_out_the_crosswind`, reads every
aircraft under `assets/jsbsim/aircraft/` against a stated space - 16, the 15
with alpha-rate terms listed by name and the B-2A named as having none - and
fails if any property naming a dynamic pressure in such a term is other than
`aero/qbarUW-psf`, or if the directory differs from that list.

**Verification.** The new test passes; seen red with the change taken out of
the script (it named her `pitch-alpha-rate`), green with it put back; and,
after the review of #200, red on a copy of the aircraft with her term spelt
`aero/qbar-uw-psf` and a seventeenth directory added, naming both.
`glideslope_cli figures mosquito-fb6`, before and after, all 15 in range
in both builds; two moved, the rest to the digit:

| figure | default build | `-march=x86-64-v2` build |
| --- | --- | --- |
| take-off over 50 ft (B Mk IV) | 2520.31 to 2520.32 ft | 2520.41 to 2520.34 ft |
| swing on take-off | 2.58 to 2.59 lb/sq in port lead | 2.60 to 2.61 |

**The selftest hash does not move**: `de9bf01f96427593` in the default build
and `fefe2a7f5d0f52a5` in the `-march=x86-64-v2` build, before and after (it
flies a C172P). Of the 311 tests matching mosquito, tail_wheel, stance, stand,
crosswind, stall, take_off, takeoff, landing, lands and committed - less the
visual-model, open-terrain, window and players' copilot tests the pattern
also catches - 306 pass in both builds and 5 skip for want of a language
model's key; the three Mosquito and alpha-rate checks were run again in both
after the new test was added, and pass.
