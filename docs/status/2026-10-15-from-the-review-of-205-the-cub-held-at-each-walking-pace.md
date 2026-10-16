### From the review of #205: the Cub held at each walking pace, not handed over at it, 2026-10-15 — item done

**What is still not true first.**
- **`Drag_basic` on `qbarUW` still points along the whole relative wind**:
  JSBSim's DRAG axis is the wind's, so what is left of it - with her
  angle of attack near 90 degrees `qbarUW` is about `qbar` times the sine
  squared of her lean, a few lb - is applied sideways. Negligible here; not
  changed.
- **A tail wind standing is not flown**: her angle of attack then passes 90
  degrees and her wing's drag table clamps at its flat plate's 1.41, on a
  `qbarUW` that holds all of the wind from behind. A new Phase 9 item, "A
  J-3 Cub standing tail to the wind meets a flat plate's drag".
- **The walking-pace tests still fly only 3 kt**, as the entry below says.

**The finding.** The review read the new test right: it did not hold her
at her pace. Handed to the rollout at the pace, its autobrake stopped her in
about 1.8 s - handed over at 2 kt she spent 0.03 s at walking pace - and she
stood at taxi power. That is the situation that rolled her, but not the
item's "held at every walking pace". This supersedes the entry below's
account of the test.

**The change.** The test,
`the_j3_cub_held_at_every_walking_pace_in_a_15_kt_crosswind_stays_on_her_wheels`,
now flies each of its 56 runs in two parts, judged throughout (wheels only,
wings within 15 degrees, not over):
- **Held at her pace**, as a pilot taxis a Cub "at the speed of a fast
  walk" (her Owner's Manual, Taxiing): brakes off, the rollout's rudder
  steering and its stick back, ailerons full into the wind (FAA-H-8083-3),
  and the test's hand on the throttle, for 40 s. **Asserted**: at least 25
  s of them within half a knot of the pace. She spends **31.7 to 36.7 s**
  there (linux-debug), leaning at most **2.7 degrees**.
- **Then handed to the rollout**, as before: stopped, stood at no more than
  the throttle that held her pace for half a minute, ailerons wings-level,
  let go, stopped. Her most roll in any run, either part: **4.4 degrees**.
The hand is the walking-pace tests' law, now one function,
`walking_pace_hand`, with its rates as arguments: those tests keep 0.03 a
second a knot on and 0.3 off; holding her pace it is 0.05 both ways. With
the walking-pace tests' rates she broke away from rest on more throttle
than she rolls on (0.61 against 0.33 at 4 kt, gross), overshot, the power
came off to nothing and came back slowly, and she spent only 7.3 to 16.2 s
of 20 at her pace.

**Seen red** with main's Cub in the build's copy: 31 wrong. Held at her
pace with her ailerons into the wind she stayed on her wheels at every pace
(most roll 5.5 degrees); handed over and standing, she struck her airframe
at 20.1 to 20.7 degrees of roll in nine runs (her model's own from either
side at 3.5 to 5 kt, solo from the left at 5 kt); the rest were held too
short (13.4 s at least). Restored, green. So on main a pilot holding
aileron into the wind kept her up at a walking pace; what rolled her was
standing with the ailerons level, and that is what the model change fixes.

**Also**: the crosswind walking-pace test's comment gives the Cub's
rolling autobrake on main as 0.16 (measured), not 0.21; the plan's
walking-pace and autobrake items rewrapped and reworded.

**Verification.** The 61 tests of the entry below pass again in
linux-debug, the `-march=x86-64-v2` debug build and linux-release, and the
new test's figures are the same in all three: 31.7 to 36.7 s of 40 at her
pace, at most 2.7 degrees of roll held there, 4.4 in all.
