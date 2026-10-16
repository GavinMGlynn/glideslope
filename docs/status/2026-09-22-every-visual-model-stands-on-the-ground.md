### Every visual model stands on the ground, 2026-09-22 — tail done

**The alignment's height was fitted, and the fit could buy accuracy by
burying the aeroplane.** Where a mesh and its flight model disagree about
where the undercarriage is - the 747-400's model has four bogies where its
flight model has three legs, so no fit of three to four can be right - sinking
the model brings its contacts nearer the mesh on average. Measured against the
ground: the 747-400 was drawn 2.15 m under it, the B-2 1.67, the F-22 0.86 and
the Mosquito 0.69.

**The height is now anchored, not fitted.** A model drawn with its
undercarriage down has a tyre as its lowest point, and an aeroplane standing
on a runway has every wheel on the ground together, so that point belongs at
its lowest wheel contact. There is nothing to fit. Only x is fitted now; y is
nought by symmetry.

**What it cost and what it bought.** The `on` column - the worst distance from
a wheel contact to the mesh - grew, because it now reports the real
disagreement instead of hiding it: the 747-400's from 2.479 m to 3.402. The
`at` column, the worst distance from an airframe contact to the mesh, fell to
**0.002 m** for all four aeroplanes whose airframes were measured from their
meshes, where the 747-400's had been 2.155. Two independently computed things
now agree to 2 mm, because they finally share an anchor. No x moved, and no
airframe contact moved.

**Verified**, `every_visual_model_stands_on_the_ground_rather_than_in_it`: all
fifteen models sit within a centimetre of their lowest wheel contact, which is
the alignment file's own rounding. **The failing case was measured rather than
run**: with the previous alignment the same quantity was 2.15 m for the
747-400, far outside the centimetre this holds, but the test binary was not
run against it.
