### From the review of #275: the Cub's crosswind tests' coverage can fail, their source worded, and their cost cut, 2026-10-30

**What is still not true first.**
- **The CI costs are not CI measurements.** They were measured on a local
  linux-debug build and scaled to each table by the ten-knot landing test,
  which ran beside them. They should be measured again from a green run.
- **The figure is still not Piper's.** The owner accepted it as her limit
  on 2026-10-30, when asked directly.
- **From a mile out she is still settling on the centreline**: 1.7 to 2.1 m
  off it at the most inside the mile at 12 kt, against 0.05 m from 5 nm. The
  touch does not move: at 12 kt she touches at 89 ft/min, 0.5 to 0.7 m off
  the centreline and 293 m along, from either start.

**What changed.**
- **Wording.** The 12 kt limit is one instructor's "about 12 knots", in an
  EAA Inspire article (Steve Krog, 2019). It is not the EAA's own figure.
  The plan item, PROJECT_STATUS.md, the test's comment and the 2026-10-29
  status file now say so, and say that the owner accepted it as her limit on
  2026-10-30.
- **The coverage check can fail.** Each landing's checks are gathered, not
  thrown at the first failure, and `landed` counts only the landings that
  passed them all. The failures are listed under the count. `stayed_down_and_upright`
  is now built on `wrong_after_the_touch`, which gives the same checks as a
  list. **Seen to fail** with the half-width cut to 1 m: "the Cub landed in
  every whole knot to her limit: 1 of 12". Reverted.
- **Seen red, all four tests**, with the roll-out's rudder held central in
  `lander.cpp` (reverted, so `lander.cpp` is unchanged):

  | test | landed | first failure |
  | --- | --- | --- |
  | dry, from the left | 3 of 12 | 34.26 m off after the touch in 4 kt |
  | wet, from the left | 6 of 12 | 33.33 m off in 5 kt |
  | dry, from the right | 3 of 12 | 36.75 m off in 4 kt |
  | wet, from the right | 4 of 12 | 31.34 m off in 4 kt |

- **The step is cheaper.** Each Cub landing starts 1 nm out on the
  glidepath, not 5 nm: the item is the touch and the roll-out, and a mile is
  about a minute of her final. Release: about 2 s a test, down from 7.
- **CI costs.** Measured serially on the local linux-debug build:
  the four Cub tests 15.3 to 15.9 s each, the 15 kt test 37.6 s, and the
  ten-knot test 21.8 s. Each table's figure is that time multiplied by the
  table's ten-knot cost over 21.8. On Linux debug that gives 37 s for each
  Cub test and 90 s for the 15 kt test: 238 s in all, against #275's
  estimate of 934 s. The Cub tests were added to macos-debug's table too,
  which lists the ten-knot test.

**Verification.** The four Cub tests and the 15 kt test are green on
linux-release and on linux-debug locally. Each red run above was watched
and reverted.
