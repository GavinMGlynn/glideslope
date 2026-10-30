### The flying boat lands in gusts: held off as the rough-water book lands her, her hull damped from its first touch, 2026-10-31

**What now works**: the AI lands the Short S.23 in moderate turbulence on
all eight gust seeds within the landplanes' limits, with their margins, and
she is no longer named in the gust tests (three of fourteen are judged now).
In calm air she lands as before: touching at 97 ft/min, she hops 0.34 ft on the step (0.35 before; limit 0.5). Whether she holds on macOS's and Windows's
floating point is what CI's full run says, as for every aeroplane judged
there.

**The owner's decisions, 2026-10-31** (recorded in the item): a gust's climb
in a level hold-off is no balloon if she holds level, and is judged by her
height and her touch; her hull may touch at the rough-water book's flatter
attitude. The judge's change is the hull's alone; the landplanes' limits are
unchanged.

**Her model** (tools/make_short_s23.py, "Heave and pitch damping"). The
water's added mass, rho (pi/8) b^2 V, which damps her heave and pitch on the
step, was wetted in over the step's first half foot - an estimate
(docs/status, 2026-10-13). Her planing lift is Savitsky's with no deadrise
correction, a flat plate's, and a flat section's wetted width is its whole
beam the moment it meets the water (Wagner's and von Karman's impact theory;
Zarnick's sectional added mass rho (pi/2) c^2 with c the half-beam, b/2): so
the damping is whole from the first touch, brought in only over the travel
of one 120 Hz step at the hardest sink her hull is judged to take, 600
ft/min, 0.083 ft. Held off and touching at 64 to 81 kt, with the half-foot
fade she was still nearly undamped as her step met the water, and her hull's
and wing's lift threw her back off it 0.82 to 1.02 ft on seeds 2, 3 and 6.
(Searched for a measured heave damping of a hull like hers - NACA's
porpoising and tank landing reports, Perring and Glauert's R&M 1493 - and
found no figure to set beside it; the change is the model's own theory made
consistent, not a new figure.)

**Her landing in gusts** (`sim::Lander`, a hull in gusts only):
- **The flare, as the seaplane handbook lands her on rough water**
  (FAA-H-8083-23, ch. 6, p. 6-7: "level off just above the water surface
  ... and then reduce the power to touch down"): none of the landplane's
  gust power held, but its cushion kept; her keel flown to 1.5 ft over the
  water and held level there to the touch; her nose capped at 5.0 degrees,
  inside NACA TN 923's stable range (lower limit 4.5, best trim about 6), so
  that as she slows her wing gives her up to the water. A gust's climb is not
  met by letting the nose down. Measured: capped at 4.5 she flew off the
  water again, 2.70 ft (seed 6); at 5.5 a lull put her on at 401 ft/min
  (seed 3).
- **On the water, flatter**: her step attitude no higher than 3.5 degrees,
  TN 923's floor ("abnormally low trims (possibly 3 degrees or less) should
  be avoided ... on landing") with half a degree in hand, as the handbook's
  "somewhat flatter pitch attitude than usual ... prevents the seaplane from
  being tossed back into the air"; and held so between skips too - given the
  landplane's gust stick off the water, four degrees under her touch, her
  nose went to 0.8 degrees and she porpoised.

**The judge** (tests/unit/test_lesson.cpp, `how_the_hull_held_off`): for a
hull held off, her climbs in the hold-off are not her flare's; instead her
nose is raised no more than a degree while she climbs (0.35 at most,
measured), her keel never climbs back to half the height her flare began at
(3.42 ft at most, of 21.5), and she touches no flatter than TN 923's 3
degrees (3.51 at least), each with a quarter's margin.

**Her figures**, the eight seeds (linux-release):

| seed | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| touched, ft/min (limit 335) | 108 | 244 | 294 | 187 | 313 | 177 | 25 | 153 |
| rose, ft (limit 0.75) | 0.00 | 0.61 | 0.31 | 0.22 | 0.18 | 0.25 | 0.18 | 0.14 |
| touch, deg | 5.12 | 4.07 | 4.27 | 4.40 | 4.43 | 4.25 | 4.52 | 3.51 |
| touch, kt | 77.4 | 77.2 | 64.3 | 70.0 | 72.0 | 72.1 | 81.2 | 76.3 |
| touched along, m | 705 | 609 | 728 | 739 | 528 | 653 | 498 | 657 |

**Verification**:
- `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1` to
  `_8`: the S.23 judged and passing all eight, figures above. **Seen red**
  on her model's half-foot fade put back in the built data: seeds 2, 3 and 6
  fail, rising 1.02, 0.82 and 0.87 ft.
- `a_hulls_hold_off_is_judged_by_her_nose_her_height_and_her_touch_each_seen_red`
  (new): a hold-off as she flies it passes, and four with one fault each -
  never down to the hold, the nose raised 0.8 degrees in a climb, back up to
  half the flare's height, a touch at 3.2 degrees - are each named for their
  own fault alone. Coverage asserted: four of four.
- `the_committed_short_s23_is_what_its_script_writes`: the regenerated model.
- The full linux-release suite: 1,420 of 1,420 passed.
