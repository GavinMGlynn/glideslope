### The 172P makes its 160 hp, and its learnt landing is trained again for it, 2026-10-09 — item done

**What is not done first.** Nothing of the item. The training is reproducible
only as far as measured: the same command, on this machine and these
packages, trained the same weights twice (four million decisions each); on
another machine or another build of PyTorch it is not claimed.
**The policy is marginal at the gate.** From one checkpoint to the next,
a million decisions apart, the gate's 160 corners went 121, 160, 135, 160
within the limits (24 to 27 million). The committed checkpoint was chosen
on the forty held-out starts, not on the gate: the gate was not looked at
in choosing it, and that it passes all 160 at the one checkpoint chosen is
partly luck of the draw. Its margins there are wide (190 ft/min against
300, 2.14 m against 5), but a neighbouring checkpoint failing 25 says a
small change to the engine or the training could fail it again.

**The engine** (`tools/make_c172p.py`, owner's decision 2026-10-09):
`eng_io320.xml`'s `<bsfc> 0.32` is taken out, so JSBSim sizes the engine to
its `maxhp` of 160 (the handbook, section 1: "160 rated BHP at 2700 RPM"),
and the propeller's cp_factor goes from 1.22 to 0.88 and ct_factor from
0.955 to 0.96 - #130's figures, unchanged. A rated-power figure is added to
`assets/figures/c172p.xml`, as the other three light aeroplanes have.

**Its figures** (`glideslope_cli figures c172p`, linux-release), 11 of 11
in range: rated power 160.4 hp at 2,700 (160, 3%), static 2,351 rpm,
take-off 906 ft, climb 763 ft/min, ceiling 12,786 ft (13,000), cruise 119.6
KTAS, glide 9.38, stalls 51.3/47.7/45.9 KCAS, turn 99%.

**The policy, trained again** (tools/rl at 9d789a0b, unchanged; JSBSim
1.3.1 and the packages in the policy's header). A fine-tune of the
committed policy's own checkpoint (step23000004, sha256 41cf2907...), on
the new engine, as its last round was trained:

    nice -n 10 python tools/rl/train.py --seed 160 --envs 8 --steps 6000000 \
        --resume ~/.cache/glideslope-rl/r3c/step23000004.zip --lr 3e-5 --log-std -2.5257

Stopped past 27.4 million decisions; the committed policy is its checkpoint
at 27,000,004 (sha256 ae2920df...). **Cost**: 34 minutes on the clock, about
an hour of CPU for the 4.4 million decisions (eight environments, niced,
the machine shared with other builds). **Reproducible**: run again with
`--steps 4000000`, the checkpoint at 27,000,004 had the same weights and the
same normalisation, byte for byte (the logs agree line for line). A resumed
run was already deterministic - PyTorch's default generator starts from a
fixed seed in a fresh process and the environments are seeded by `--seed` -
so export.py's header no longer says training is not reproducible.

**Chosen on the forty held-out starts alone** (evaluate.py --held-out, a
fresh aeroplane each), at 25, 50, 75 and 100 lb a tank, out of 160:

| checkpoint | 25 lb | 50 | 75 | 100 | worst sink at 100 lb |
| --- | --- | --- | --- | --- | --- |
| the old policy, on the new engine | 40 | 40 | 39 | 38 | 311 ft/min |
| 24M | 40 | 39 | 36 | 29 | 308 |
| 25M | 39 | 40 | 40 | 40 | 292 |
| 26M | 40 | 40 | 40 | 39 | 258 |
| **27M** | **40** | **40** | **40** | **40** | **219** |

27M is the first with all 160. The gate's corners were flown in Python for
each as well, after the scores above (a scratch script, not committed), and
swing as much: 121, 160, 135, 160 of 160.

**The old policy's shortfall, reconciled.** The C++ gate test (#130) had 22
of 160 short: 20 at 301-311 ft/min (all but two in the crosswind from the
right) and two touching 12 and 20 m before the threshold. The Python script
had 23: the same 22 and one more in the crosswind from the right at 300.x
ft/min, printed as 300: within the 0.44 ft/min by which Python and the
simulation may differ at the touch, so the C++ test can put it under the
limit (not flown again in C++ to see). The 311 in the
held-out table above is the held-out starts' worst, not the gate's.
The verification's 27 starts, flown by the export: 27 within the limits,
worst 155 ft/min and 2.10 m across.

**Exported** with `tools/rl/export.py` (the policy and the parity fixture
together); the header records the whole lineage - the six parts before
and this seventh - the command line and the choice. Not edited by hand.

**Verification** (linux-release, DISPLAY and WAYLAND_DISPLAY unset):
- `the_learnt_policy_lands_within_its_limits_from_every_corner_of_its_gate_in_*`,
  all five winds, 32 each: worst 2.14 m across (calm) and 190 ft/min (the
  most crosswind from the right and the most tailwind), none short; with the
  old policy on this engine 22 of 160 were short (#130), red.
- `the_learnt_policy_touches_down_in_the_simulation_where_it_did_in_training`
  on the fixture recorded again; every other learnt-landing test, the
  session's, the server's and the CLI's, green.
- `the_cessna_172ps_engine_makes_160_hp_at_2700_rpm`, new: 160.4 hp. **Seen
  to fail**: with the old engine and propeller put back in the build's data,
  221.6 hp, out of range; the data copied again, green.
- `every_published_figure_has_a_flight_and_every_flight_a_figure` counts 121.
- The whole suite, linux-release at -j4: green but for two. The
  cross-platform check counted ten 172P figures and now counts eleven
  (`tests/cmake/cross_platform_flights.cmake`), green. And
  `the_client_with_the_window_stalled_past_the_timeout_joins_again_by_itself`
  failed once with the machine loaded by other builds and passed alone;
  it touches no aeroplane's engine.
- **Restacked on the 182S, Cherokee and F-15C work** (on 2968b729): the 417
  tests that fly the 172P or read its figures - stall recoveries, lessons,
  glides, orbits, plans and routes, departures, the lander and leaner,
  prediction, restore, separation, crashes, the selftest, the learnt
  landing's gate and the cross-platform check - green in linux-release, at
  -j4. The figures count 122 across the catalogue and 11 for the 172P; the
  selftest hash is `182dd6c996e0ee4c` on the combined tree too.

**The selftest hash moves**, deliberately: `be036519d2c19ea0` ->
`182dd6c996e0ee4c` in linux-release. The selftest flies the 172P, whose
engine changed.
