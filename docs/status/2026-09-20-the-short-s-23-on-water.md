### The Short S.23 on water, 2026-09-20 — item done (CI run 35472220036)

Phase 5's "the Short S.23 on water, from JSBSim's model and its
hydrodynamics": the Short S.23 Empire flying boat of 1936, from Anders
Gidenstam's JSBSim model and its hull's and floats' hydrodynamics, written
into the data by `tools/make_short_s23.py`, takes off from and alights on
the sea and lakes where the DEM's water body mask puts them, and flies to
the figures Short Brothers and Flight published.

| Short S.23 | Measured | Published |
| --- | --- | --- |
| Take-off from calm water at 45,000 lb, from full throttle | 32.7 s, 825 yd | 30.5 s and 795 yd ± 10% (Gouge's tests, Flight 17 Dec 1936) |
| The same from the Tasman Sea off Sydney and from Lake Macquarie, on the DEM | 32.6 s, 819 and 820 yd | the same |
| Take-off at 40,500 lb, the standard boat | 25.0 s | 24 s (Gouge's Table II), 21 s (the specification) |
| Draught at the main step, afloat at 40,500 lb | 3.69 ft | 4.0 ± 0.5 ft, scaled from Flight's general arrangement |
| Level at 5,500 ft, +2 1/2 lb boost, coarse pitch | 202 mph | 200 ± 5% |
| Climb at sea level, +2 1/2 lb, fine pitch | 912 ft/min | 950 ± 10% (933 in Gouge's Table II) |

It alights on water, its nose on the step, and with its engines stopped
comes to rest afloat, its centre of gravity 7 ft above the water; the client
starts it `--on-ground` afloat, in Rose Bay, and refuses to stand a
landplane on water, where it would ditch.

**What it took.** The hull's hydrodynamics keep a water level of their own,
at JSBSim's sea level: glideslope now puts it where the terrain's water is
before every step - over the sea off Sydney 72 ft above JSBSim's sea level,
the ellipsoid - and out of reach over land, and a model with hydrodynamics
floats rather than ditches. On land the hull sank into the ground, meeting
it only at its bow, tail and wings; keel and float skids, as JSBSim's wheels
that water does not bear, now set it on its keel, heeled onto a float. Its
airscrews' pitch levers were the other way round from glideslope's
propeller control, and every take-off was made in coarse pitch, never
leaving the water; and its drag, a DATCOM estimate, flew it at 225 mph where
its maximum is 200, climbing half as fast again as it did, its engines
overspeeding to 945 hp. Times 1.9, the one change flies both figures and
lengthens the take-offs by no more than a tenth.

**What is missing first:** no draught, alighting run or take-off with a
wind or a sea running is published, and none is flown; the draught is
measured off a 1936 magazine's drawing, give or take half a foot. Its
stalling speed, 73 mph, is not flown: at its weight it needs more lift than
Gouge's own wind tunnel gave, so it was at some lighter weight or with
power, neither stated. The water is calm and all one density: no waves, no
current, and a lake as salt as the sea - the mask says ocean, lake or
river, not salt or fresh. And the client has no propeller or mixture lever
for the pilot: the S.23's airscrews stay in fine pitch and its mixture full
rich, through the gate to take-off boost, so that at full throttle in the air
its engines turn 3,185 rpm and give 1,185 hp where the Pegasus is rated at
2,600 and 920 - a tail in `COMPLETION_PLAN.md`.
