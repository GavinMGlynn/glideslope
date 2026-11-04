### The 737-300 light and the Mosquito at her heaviest land without a balloon, 2026-11-05

**What is still wrong first.** The gust item stays open. Neither the
A380 nor the F-22A is named whole any more, but both still have
single-seed gust names:
- the F-22A on 12 seeds on Linux, 14 on Windows and 12 on macOS, and the
  owner is still to rule on whether her per-seed names close her;
- a few others on one seed each, which close on their figures by the
  2026-11-04 ruling.

This removes the item's two loading names. Nothing is named at any
loading now.

**What now works.** Every aeroplane taught the approach (16) lands light and
heavy in calm air with none named. Before this, the 737-300 at her light
loading and the Mosquito at her heaviest were named and not judged
(`every_aeroplane_lands_light_and_heavy_without_a_balloon_a_bounce_or_a_go_around`,
tests/unit/test_lesson.cpp). Their names are off.

| case | before | after |
|---|---|---|
| 737-300 light, 76,098 lb, 112 kt | climbs 140 ft/min in her flare, touches 366 ft/min, 502 m | most climb -116 ft/min, touches 116 ft/min, 382 m |
| Mosquito heaviest, 21,462 lb, 134 kt | climbs 22 ft/min in her flare, touches 72 ft/min, 870 m | most climb -32 ft/min, touches 46 ft/min, 827 m |

The limit is 10 ft/min of climb in the flare. The touchdown zone runs 0 to
914 m.

**The 737-300 light: the cause.** It was not her flare. Her whole final
was a pitch oscillation. Her nose swung about four degrees either side of
her path attitude every eleven seconds, and her sink swung from 0 to
1,300 ft/min, from the 2 nm gate to the flare. She entered the flare
pitching up and climbed. The lander's pitch gains (0.05 of elevator a
degree, damped 0.05 a degree a second) were set where she flies at her
114,000 lb landing weight, 137 kt. At 112 kt her elevator has two thirds
of that authority, since authority goes as airspeed squared, and the
attitude loop lagged the path loop.

Traced with a temporary trace, not committed:
- the gains at 0.5 times made her diverge;
- damping alone at 2 times barely helped;
- the whole law at 1.5 times, which is 137/112 squared, flew her steady.

**The 737-300 light: the fix.** This is lander code driven by her figures.
`pitch_gains_kcas` in assets/figures/737-300.xml gives the speed at which
her pitch gains are right. Below it, the elevator law is scaled by that
speed over her approach speed, squared. That is scheduling the gain on
dynamic pressure, which is standard autopilot practice. It is never scaled
down. Every approach of hers at her landing weight, gusty or calm, flies
at or above 137 kt, so it is untouched.
- **Sourced**: that elevator authority goes as dynamic pressure.
- **Measured**: 137 kt is her reference speed at 114,000 lb, 1.3 times
  her measured 105.72 kt stall, rounded down.
- **Tuned**: that 137 kt is where her gains are right. They were set
  there, and she lands there.

**The Mosquito heaviest: the cause.** She flies at 134 kt, 1.3 times her
stall scaled for her weight. Her sink was arrested 17 ft up with her nose
still rising, and she rose at up to 22 ft/min before settling into a float.
The calm flare judges her sink 0.3 s ahead.

**The Mosquito heaviest: the fix.** `flare_lead_s` in
assets/figures/mosquito-fb6.xml makes her calm flare judge the sink 0.5 s
ahead. It does not change her gusty lead.

| lead | light, most climb | heavy, most climb | heavy, along |
|---|---|---|---|
| 0.3 | -34 ft/min | 22 ft/min | 870 m |
| 0.4 | -57 ft/min | -6 ft/min | 855 m |
| 0.5 | -63 ft/min | -32 ft/min | 827 m |
| 0.6 | -64 ft/min | -45 ft/min | 789 m |

0.5 is taken. It is **tuned**, not sourced.

**What the research found.** Searched:
- Boeing 737 FCTM landing technique when light;
- 737 FCTM wind additive and bleed-off;
- the Mosquito pilot's notes, AP 2019, on landing heavy and approach
  speeds.

The 737 FCTM itself was not found. As PPRuNe quotes it, the flare begins
at about 20 ft with 2 to 3 degrees of pitch and the thrust smoothly to
idle. On light weight it gives only that the aeroplane floats.

A.P. 2019E (1950, as copied at zenoswarbirdvideos.com, MosquitoFB6Manual.pdf):
- para. 47 approaches the Mosquito "engine assisted" at 100-105 kt from
  17,000 to 18,000 lb, "at full load ... increased by about 5 knots";
- para. 43 gives her stall, power off, as 95-100 kt.

The AI flies her about 20 kt faster than the book, at 1.3 times her stall.
That is left alone: her 10 kt stall warning over a 95 to 100 kt stall
leaves the book's speed no room. Both files record this.

**Every other aeroplane is bit-identical** in what was diffed:
- gust seeds 1 to 40;
- the light-and-heavy test;
- the approach at her figures' loading and at her model's own weight.

These were run before and after in an x86-64-v2 build. There, no
multiply-add is fused, so the unscaled law is the same arithmetic. Only the
737-300 and the Mosquito lines differ, and only in calm air. Their gust
lines are identical too.

Her own other lines move:
- the 737-300 at her model's 96,540 lb touches at 198 ft/min, against
  345;
- the Mosquito at 18,000 lb touches 680 m along, against 724, and at
  19,800 lb 720 m, against 777.

On the local x86-64-v3 build (FMA), the J-3 Cub's figures-loading touch
moved from 116 to 113 ft/min. Nothing changed pass or fail. Bisected, the
move came with the elevator's scheduled branch: with the branch taken out
and the `lead_s` change kept, she touched at 116 again. So the likely
source is how the compiler fused the unscaled law beside the new code.
`lead_s` becoming a load from a constant did not move it. How GCC fused it
was not inspected. CI's builds are not FMA.

**From the review:**
- the pitch-gain factor is capped at 2;
- the law is computed once and scaled only when the factor is over 1.

**Verification**:
- `every_aeroplane_lands_light_and_heavy_without_a_balloon_a_bounce_or_a_go_around`:
  32 cases, all judged and passing. Seen red with both figures taken out
  of the built data and the names off: "737-300 (light) climbed at 140.14
  ft/min" and "mosquito-fb6 (heavy) climbed at 21.70 ft/min". Then
  restored.
- `a_pitch_gains_speed_and_a_calm_flare_lead_are_read_and_refused_outside_their_bounds`
  (tests/unit/test_figures.cpp) reads both figures. It refuses four out of
  bounds, coverage asserted.
- The gust seeds 1 to 40 and both approach-weight tests pass on Linux
  release and x86-64-v2.
- Windows release (tools/windows_build.sh, 1011b38): the light-and-heavy
  test, both approach-weight tests, the figures test and gust seeds 1 to 8
  pass.
- The server's landing tests pass on Linux release: its AI on final at
  the model's own weight, the learnt-landing hand-over, planned and
  copilot-routed landings, and the gusty climb-out.

The selftest hash is unaffected: no aircraft's own flight model changed.

**After the review's fix round** (the factor capped at 2, the law computed
once): in the x86-64-v2 build, which has no FMA as on CI, every gust,
light/heavy and approach-weight line is unchanged from the push before it,
the 737-300 light's included. The Cub's FMA-build figure was not measured
again.
