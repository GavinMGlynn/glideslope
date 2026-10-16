### From the review of #208: the rolling ceiling named per aeroplane, the swing rolled up judged, the steering brakes one law, 2026-10-16 — item done

**What is still not true first.**
- **The Mosquito's swing rolled up in 15 kt across is named, not judged**:
  46.6 degrees off the runway's heading below 2 kt (46.5 in linux-release),
  the open Phase 10b item. Dry and wet she swings 17.9 degrees rolling up,
  and is judged there.
- **The rolling ceiling is required from 3 kt, not below**: handed over at
  2 or 2.5 kt dry or across no landplane reaches it (0 to 0.29), nor at 2
  kt wet. Seven aeroplane-conditions are named from 3.5 kt instead, each
  with her figure at 3 kt (below).
- macos-debug's costs for the six tests are still estimates: it does not
  run on pull requests.

**This supersedes** the entry below where it names every landplane for the
rolling ceiling at 3 kt, prints the named ones that reach it, and copies the
rollout's steering-brake law into the test.

**The changes.**
- **The rolling ceiling, required per aeroplane.** `walking_pace_then_stop`
  takes the pace from which the ceiling must be reached (3 kt dry and
  across, 2.5 wet) and names, each with her figure, those whose brakes stop
  her first at 3 kt and who must reach it only from 3.5:
  - dry: the C172P (0.287 at most rolling), F-15C (0.288), Cub (0.278) and
    Mosquito (0.287);
  - 15 kt across: the C172P (0.296), F-15C (0.272) and Cub (0.27; 0.268 in
    linux-debug, 0.274 in linux-release) - the Mosquito reaches it there;
  - wet: the Cub and the Mosquito, braked as on a dry runway (0.278,
    0.287).
  **A named run that reaches the ceiling is now wrong** ("name her no
  longer"), and asserted none: so a change that brings a named one up to it,
  or leaves one short that is not named (#207's lift, which puts the C172P
  and PA-28 at 0.294-0.295 across, must name them), fails here.
- **Her swing rolled up and held, judged**: from the roll-up to the
  hand-over her heading stays within 30 degrees of the runway's, but where
  named - the Mosquito in 15 kt across, with the Phase 10b item. Coverage
  asserted: every run's swing judged or named. The most, judged: 17.9
  degrees (the Mosquito dry and wet), then the PA-28 7.1 and the C182 5.5
  across; the rest 5.2 or less.
- **`Lander::toe_brakes`** (lander.hpp/.cpp): the rollout's rudder-bar
  braking - past half the rudder's travel the inside brake, up to 0.6,
  fading below walking pace - as one static helper, called by the rollout
  and by the test's hand. Same arithmetic: every walking-pace figure, and
  the selftest hash `ba8e97281095464f`, are as before.
- **The most throttle in the hold is printed**: 0.15 (the C182 across) to
  0.59 (the PA-28 across) - the break-away from rest - against a taxi power
  of 0.05 to 0.44 at the hand-over.
- `tests/ci_costs/`: each of the six at the larger of CI runs 38690215083
  and 38694861283 in the six presets that ran them (linux-debug 140 to 216
  s, windows-debug 65 to 125, linux-release 14 to 19, windows-release 10 to
  27, windows-clang 16 to 26, macos-release 7 to 12); macos-debug keeps its
  estimates, 56 to 83.
- The crosswind tests' comment has its blank line back; the long printf is
  rewrapped.

**Seen red**, linux-debug:
- **The naming**: the C172P unnamed dry and the C182 named in her place -
  2 wrong: "c172p at 3.0 kt: her autobrake reached only 0.287352" and "c182
  at 3.0 kt: named, and her autobrake reached the rolling ceiling all the
  same". Restored, green.
- **The swing**: the Mosquito unnamed in 15 kt across - 4 wrong at 2 to 3.5
  kt, "swung 46.551269 deg off the runway's heading rolled up and held".
  And with `Lander::toe_brakes`'s steering brake taken out (0.6 to 0): 8
  and 12 wrong in the dry tests, the Mosquito swung 180 degrees rolled up at
  every pace, turned 78 to 180 after the hand-over and struck her airframe
  at 4.5 and 5 kt. Restored, green.

**Verification.** linux-debug and linux-release: the six walking-pace
tests, the Cub held at every walking pace, the gone-over test, the
tail-wheel stance test, the braked stop and stick-central Cub tests in 15
kt across, and the wet and steady crosswind roll-outs pass, the six with
the same figures as before but the swing, now measured to the hand-over;
in linux-debug the four selftest tests too. The `-march=x86-64-v2` build
was not made again (removed to save the disk).
