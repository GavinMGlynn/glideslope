### Visual models from FlightGear's aircraft, 2026-09-20 — item done

Phase 5's "visual models from FlightGear aircraft, each licence checked" is
done. **Fourteen of the sixteen aircraft ship a visual model, and nothing
draws any of them yet** - the renderer has never been handed one, and that is
the views item.

`tools/make_models.py` fetches FlightGear's aircraft, follows their model XML
to the geometry, flattens each aircraft's exterior into one mesh in the body
frame and writes `assets/models/<model>.mesh`. Every file it reads is pinned
by URL and SHA-256 in `assets/models/sources.txt` - 69 of them now - in the
four-field form `tests/cmake/fetch.cmake` reads, so the test fetches exactly
what the script converted. `src/gfx/model.cpp` reads the meshes back; it is
its own library, `glideslope_model`, because it is presentation that links
nothing presentational, so the unit tests read a model without SDL or Cesium
Native behind them.

| Model | Triangles | Length | Published | Span | Published |
| --- | --- | --- | --- | --- | --- |
| c172p | 45,451 | 8.17 m | 8.28 | 11.32 m | 11.00 |
| c182 | 30,169 | 8.50 m | 8.84 | 11.15 m | 11.00 |
| pa28 | 91,464 | 7.33 m | 7.25 | 10.69 m | 10.67 |
| j3cub | 51,387 | 6.86 m | 6.83 | 10.65 m | 10.74 |
| 737-300 | 48,318 | 33.30 m | 33.40 | 28.89 m | 28.88 |
| 747-400 | 24,382 | 70.94 m | 70.66 | 65.42 m | 64.44 |
| 787-8 | 28,759 | 56.68 m | 56.72 | 59.60 m | 60.12 |
| a320 | 107,449 | 37.61 m | 37.57 | 35.77 m | 35.80 |
| a380 | 17,278 | 73.51 m | 72.72 | 79.78 m | 79.75 |
| b2 | 5,612 | 21.21 m | 21.03 | 52.20 m | 52.43 |
| f15c | 162,347 | 19.51 m | 19.43 | 12.60 m | 13.05 |
| f22 | 29,949 | 18.92 m | 18.92 | 13.56 m | 13.56 |
| mosquito-fb6 | 12,506 | 12.49 m | 12.55 | 16.26 m | 16.51 |
| short_s23 | 85,604 | 26.82 m | 26.82 | 34.72 m | 34.75 |

16 MiB in all. Positions are quantised to 16 bits across each model's own
bounding box - under a millimetre on the largest - normals to signed bytes and
colours to unsigned, which is what keeps that figure down.

**The six that state no licence.** Eight of the fourteen come from a
FlightGear directory that states its terms, quoted per model in
`docs/ASSETS.md` with its source and revision - FGAddon at Subversion r21588,
and the c172p from the c172p team's own repository at commit `84477612`. The
other six - the A380, B-2, F-15, F-22, Mosquito and Short Empire - state none
at any level of their directory, and **the project owner decided on 2026-09-20
that they ship on FGAddon's project-wide requirement that its content is
GPL**, recorded in `ASSETS.md` as the basis: a policy, not a grant by the
author. Each entry also names what was looked at and found - which file names
the author, and what it says. One of the six turned out to say more than the
first pass found: the Short Empire's `Short_Empire-set.xml` and its model XML
each carry "Copyright (C) 2007 - 2025 Anders Gidenstam ... This file is
licensed under the GPL license version 2 or later", which is a grant, but it
is on those two files and not on the geometry, and its `AUTHORS` credits the
propeller models to the Boeing 314 and the engine model to the Lockheed-Vega
without terms. Two aircraft have no FlightGear model at all: there is no
Learjet of any mark in FGAddon, and the only F-35 is the F-35B, a different
airframe with a lift fan.

**The A380 needed a second geometry format.** Its fuselage and wing are AC3D
like every other aircraft here, but its horizontal tailplane, its four pylons
and its four engines are 3D Studio (`.3ds`), and without them it was a
fuselage and a wing. The script now reads both: a .3ds is a tree of chunks,
each a 16-bit identifier and a 32-bit length that counts its own header, and
only the chunks geometry needs are read. AC3D is authored with up along +Y
and 3D Studio with up along +Z, so a .3ds in the same aircraft is the AC3D
frame turned a quarter circle about X - the turn that keeps up pointing up,
since the other one would put it underground. Read as if it were AC3D, the
A380's tailplane is 2 m across and 30 m thick; read properly it is 30.38 m
across, against a published 30.37, and its pylons at 14.8 m either side come
out as exact mirrors of each other.

**A file's offsets place everything in that file**, its own geometry and its
`<model>` children alike, and the script applied them only to its own
geometry. The Short Empire found it: its model XML turns the aeroplane
through 180 degrees, so its four engines stood six metres ahead of the bow.
They now sit on the cowlings and nacelles its hull already carries. The same
bug had the A320's fuselage not getting the 0.831 m rise its engines were
given by `A320-common.xml` - a file with that offset and no geometry of its
own, which is the proof that the offset is meant for the children - so its
engines rode 0.831 m high. `a320.mesh` changes with this commit for that
reason and no other; its triangle count, length and span are unchanged.

**The model XML is parsed, not searched.** The script used to find tags by
searching the text, which meant stripping comments and Nasal by hand because
Nasal is code and can hold anything that looks like a tag. It now parses with
the standard library's ElementTree: comments go on their own, and Nasal is
skipped as the structure it is not. libxml2 through lxml would do as well and
is not installed; expat is in the standard library and this script is to have
no dependency of its own. The change was held to producing the eight existing
meshes byte for byte before anything else was done to it. It matters: the
A380's Nasal is not inside CDATA, so its jetway doors are real elements in
the tree.

**What each aircraft leaves out.** The walk is an allow-list - the entry
XML's own geometry and only the children named per aircraft - because
FlightGear hides the rest with animations this does not interpret. The six
new ones add: the A380's pushback tug, cabin, flight deck and light cones
left out and its wing, tailplane, belly fairing, pylons and engines taken;
the F-15C's missiles, bombs, rails and drop tanks, which are a payload
FlightGear picks and not the airframe, and its boarding ladder; the
Mosquito's propeller blades taken from `pdisk.ac`, because its airframe
carries hubs and no blades, without the two discs FlightGear blurs them into;
and the Short Empire's four propellers, four Pegasus Xc engines and cowling
gills taken, without their blur discs.

**The facing tests had to grow two exceptions.** Every model was held to
being wider at the tail than at the nose and to its highest point being its
fin. Neither holds for all fourteen: the Mosquito's two propellers stand at
its nose and are 3.8 m across, wider than its tailplane, and the B-2 is a
flying wing with no fin at all, whose highest point is its cockpit, 28% of
the way from the nose. The test now states both facts, names which model is
excused which and why, and counts that exactly one is excused each.

| Fact | Held to | Narrowest | Widest |
| --- | --- | --- | --- |
| The aft seventh is 1.3x wider than the forward seventh | all but the Mosquito, which measures 0.77 | the Cub and the c172p, 1.56 | the F-15C, 7.03 |
| The aft seventh reaches higher than the forward seventh by a fiftieth of the model's height | all but the B-2, which measures -21.2% | the F-22, 5.4% | the A380, 58.9% |
| It stands on its undercarriage: the lowest tenth spreads further along it than the highest tenth | the B-2 alone, 55% against 39% | - | - |

The B-2's rule is its alone because a taildragger drawn level has its
tailwheel well above its main wheels: the Cub's lowest tenth is its two main
wheels and spreads over 7% of it.

**What is left, and named elsewhere.**

1. **Nothing draws a model.** They are data on disk; the renderer has not
   been given one. That is the views item.
2. **No model's origin is aligned to its flight model's.** Each mesh is in
   its FlightGear aircraft's own frame, and that aircraft carries its own
   FDM: the Cub's and the 747's wheels land within 0.14 m of where JSBSim's
   contact points put them, but the c172p's are 1.03 m out and the A320's
   0.78 m. Placing a model on the aeroplane glideslope flies needs that
   offset per aircraft, measured and recorded; the views item is where it
   shows.
3. **No texture, so no livery**, and a surface takes the flat diffuse colour
   of its AC3D or 3D Studio material. Liveries are large and separately
   licensed, and the renderer has no texture path for a model yet.
4. **No animation.** Control surfaces, gear and propellers are welded where
   the model has them, gear down, propeller blades in place.
5. **The PA-28 is the wrong mark.** FlightGear has a PA-28-161 Warrior II;
   glideslope's flight model is the PA-28-180 Cherokee, which has the
   constant-chord wing rather than the Warrior's tapered one.
   `docs/ASSETS.md` says so.

**Verification run.** Locally, 268 of 268 tests pass in 416 s at `-j4`. The
five checks are `every_visual_model_that_ships_is_named_in_assets_md_with_its_source_revision_and_licence`,
`every_aircraft_the_data_holds_has_a_visual_model_or_a_named_reason`,
`each_visual_model_is_its_aircrafts_size_and_faces_the_way_it_flies`,
`a_model_file_that_is_damaged_or_of_another_version_is_refused` and
`the_committed_visual_models_are_what_their_script_writes`. Every one of them
was watched to fail: a licence row removed from an entry, a whole entry
removed, an aircraft's reason for having no model removed, an entry naming a
model that does not ship, a model mirrored, a model inverted, a committed
mesh with a byte flipped, and - with the reader's own checks broken in turn -
a model cut short and one holding an index past its vertices. The three
facing facts were each watched to fail on a deliberately broken model in this
round: the Mosquito mirrored nose to tail reaches 18.2% lower at its tail,
the c172p mirrored is 0.95 m wide at the tail against 1.77 at the nose, the
B-2 mirrored is 3.96 m wide at the tail against 26.12 at the nose, the F-22
turned upside down reaches 4.9% lower at its tail, and the B-2 turned upside
down spreads 39.0% at its lowest tenth against 55.4% at its highest. Removing
`a380.mesh` from the built data was watched to fail the two entry checks.
