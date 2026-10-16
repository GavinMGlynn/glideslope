### A visual model put where its aeroplane is, 2026-09-21 — item done

Phase 5's "a visual model put where its aeroplane is" is done. **A model is
drawn at its flight model's visual reference point, moved by a measured
offset; nothing draws one yet**, which is the views item.

**JSBSim already had the anchor.** Its `<metrics>` carries a VRP - a visual
reference point, which is what a VRP is for - and every one of the sixteen
defines one. Anchoring there rather than at the structural origin is most of
the answer on its own: it puts the Cessna 172P's wheels within 0.05 m of its
flight model's, where the structural origin had them 1.03 m out, and the
Cub's, the PA-28's and the Short Empire's within 0.02 m.

**What is left is measured, not guessed.** `tools/align_models.py` reads the
committed meshes and the flight models - no network, no build - and writes
`assets/models/alignment.txt`: three metres of offset per aircraft, and what
the fit leaves over. A test fails if the committed file differs from what the
script makes. It measures the meshes, so it is stale if they change.

**The undercarriage is the contacts the aeroplane rests on**, and which those
are is geometry, not a label. A flight model's `<contact>` points are not all
wheels: the A320's include its wingtips, its nose tip and the top of its fin,
and the Short Empire's are the keels of a hull. No field tells them apart -
the A320's wingtip carries the same rolling friction as its wheels - so the
script takes the hull of the contact set seen from below, along the span of
it the centre of gravity lies over. That picks a tricycle's nose and mains, a
taildragger's tail and mains, the A380's nose and four bogies, and the flying
boat's forward keel and step, and nothing else. **JSBSim confirms it**: with
each aeroplane stood on the ground, the contacts it puts weight on are
exactly as many as the script worked out without running anything.

| Aircraft | Offset, m (x, y, z) | Left at its wheels | At its shape |
| --- | --- | --- | --- |
| 737-300 | +0.21, 0, -0.52 | 0.47 | - |
| 747-400 | +1.86, 0, +1.41 | 2.48 | - |
| 787-8 | +0.54, 0, -0.21 | 0.91 | 2.16 |
| a320 | -1.78, 0, -0.91 | 0.34 | 1.65 |
| a380 | -22.66, 0, -3.26 | 0.63 | 2.04 |
| b2 | +0.11, 0, +2.37 | 0.53 | - |
| c172p | +0.21, 0, +0.07 | 0.03 | 0.29 |
| c182 | +1.02, 0, +0.37 | 0.22 | 0.51 |
| f15c | +0.71, 0, +0.30 | 0.13 | 0.46 |
| f22 | -3.87, 0, +0.54 | 0.98 | - |
| j3cub | +0.03, 0, -0.02 | 0.07 | 0.07 |
| mosquito-fb6 | -0.50, 0, +0.42 | 0.45 | 0.86 |
| pa28 | -1.31, 0, -0.13 | 0.14 | 0.57 |
| short_s23 | +1.36, 0, +0.20 | 0.35 | 1.35 |

The offset across the centreline is held to zero: both the model and the
flight model are symmetric, so a lateral offset would be a mistake, not a
measurement.

**A flight model and a visual model of the same aeroplane do not always
agree**, and no placement can make them, so what is left over is recorded and
each aircraft held to its own figure. JSBSim's 747-400 has one main leg a
side, 5.5 m out; the aeroplane has two, at 3.7 m and 11.4 m, and FlightGear's
model draws both, which is the 2.48 m. The B-2's flight model, written here,
puts its wheels 2.37 m below where FlightGear's model draws them; the model
is moved to the flight model, because the flight model is what flies.

**The walk needs a good start.** Fitting from nothing settles in the wrong
place for an aircraft whose model is far from its flight model - the A380's
is 22.7 m away along the fuselage - so it is walked from three starts and the
one that settles closest is taken.

**What the flight models say about their shape.** Ten of the fourteen carry
contacts beyond the undercarriage - wingtips, tailcones, a radome, propeller
tips, a belly - and all 97 contacts across the fourteen are within their
aircraft's stated distance of the model. Four - the 737-300, 747-400, B-2 and
F-22 - describe nothing but their wheels, so their span is the only shape
they can be held to, and the test names those four so a fifth cannot join
them unnoticed. Every model's span is within 6% of its flight model's, the
A320 the tightest at 5.5%, its model having sharklets its flight model's span
does not; the PA-28 is excepted and named, because FlightGear's is the
PA-28-161 Warrior II with a 35 ft wing where the flight model is the
PA-28-180 Cherokee with a 30 ft one, and it is held to the wing it actually
draws.

**Standing.** Thirteen aircraft are stood on the ground in JSBSim, brakes on,
for twenty seconds, and the model under every wheel taking weight is on the
ground - above it by no more than what the fit left over, below it by no more
than that plus the gear's compression, which is real and not a fault: a rigid
model sinks by however far the legs squash, 0.07 m on the Cessna 172P and
0.62 m on the 747-400. The flying boat is not stood: it floats, and its hull
sits below the surface by its draught, so "the wheels on the ground" is not a
fact about it; its keels are held to the flight model's by the geometry check
instead.

**Verification run.** Locally, 272 of 272 tests pass in 351 s at `-j4`. The
four new checks are `the_committed_model_alignment_is_what_its_script_measures`,
`every_visual_model_is_aligned_to_the_aeroplane_it_draws`,
`each_visual_models_wheels_sit_on_the_ground_the_aeroplane_stands_on` and
`each_visual_model_is_where_its_flight_model_says_the_aeroplane_is`. Every one
was watched to fail: the A380's offset zeroed (its wheels then have no model
under them, and its nose contact is 3.29 m from it, against the 2.06 m it is
held to), a line removed (14 models, 13 alignments), an aircraft's wheel count
wrong (JSBSim puts weight on three where the alignment says two), an aircraft
given a shape contact it does not have, and a committed figure altered by
3 mm.
