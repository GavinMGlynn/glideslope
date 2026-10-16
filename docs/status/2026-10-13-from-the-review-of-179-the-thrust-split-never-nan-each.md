### From the review of #179: the thrust split never NaN, each flaperon's droop and roll held to its travel; her short take-off 592.5 ft, 2026-10-13 — item done

**What is still missing, first**: as the entry below says - nothing flies
her between jet-borne and wing-borne flight, and **the 600 ft is met on an
estimate**: without the 20-degree flaperon droop, which nothing published
gives, she takes 699.4 ft. Her figures file now says the result does not
validate the droop.

- **The split could make every thrust NaN.** Fully aft (-1) early in the
  conversion (stovl-pos-norm 0 to about 0.12) it asked the nozzle to pass
  more than the engine made: the reverser's cosine reached 1.00037 and its
  angle was NaN. The split is now sent back no further than leaves the
  nozzle passing all the engine makes (`fcs/thrust-split-share` at least 1
  - 1 / the nozzle's share at the split's middle), and the cosine is
  clipped to -1..1 as well. The fan's and posts' base is now the engine's
  own thrust of the step before (the nozzle's over the reverser's cosine it
  had) times this step's nozzle share and swivel, so they follow the
  conversion and the split in the step they move. At the split's middle,
  converted and not swivelled, this is the same base as before.
  `the_f35bs_thrust_split_at_either_end_and_the_middle_keeps_her_thrust_finite_and_its_total_the_engines`:
  standing on the brakes, spooled up to her STOVL top, then converting -
  at split -1, 0 and 1, every step of the 8 s conversion (at least 958
  steps strictly between 0 and 1, asserted) and a second converted - the
  nozzle's, fan's, posts' and side thrusts, the reverser angle, the base
  and the split's share are finite, and her total as a share of the
  engine's own thrust (the nozzle's over this step's engine thrust, the fan
  and posts over the step before's, which they are reckoned from) is the
  middle's to 1e-9. Seen red without the clip (thrust-lbs NaN at split -1,
  stovl-pos-norm 0.001), reverted.
- **Each flaperon's droop and roll are held to its travel together**: each
  is the droop plus its roll, clipped to the 25 degrees (0.4363 rad) the
  roll alone is held to; the wing sees their mean as the droop (lift, drag,
  pitch) and half their difference as the roll (roll, yaw, side force). So
  drooped 20 degrees she rolls on what is left. With no droop this is
  exactly the roll it was, so her conventional figures do not move.
- **Measured again**: the short take-off **592.5 ft** (was 591.5) - still
  inside 600 - at the same gains, swing and attitude; 699.4 ft without the
  droop. Hover thrust 40,644.52 lbf, hover height 0.02 ft, vertical landing
  179.54 ft/min and hover endurance 5.57 min, as on the branch before; her
  conventional figures unchanged to the CLI's last digit. **The hover's
  wind envelope**: all 34 hovers hold; the most her height strays is 0.54
  ft and she drifts at most 9.0 ft (both at 20 kt from behind, 37,530 lb),
  was 0.55 and 7.2 before the droop. The selftest hash does not move,
  `182dd6c996e0ee4c`. 99 tests - every F-35B test, take-offs, strikes,
  hovers and the committed assets - re-run on linux-release.
