### The Cessna 182S against its handbook, 2026-09-19 — part of an item

The first of the three light aircraft in Phase 5's "the light aircraft fly
to their figures"; the Piper Cub and the PA-28 Cherokee 180 follow, and the
item is not done until all three are.

**JSBSim's c182 did not fly to the 182S handbook**, and on the ground it did
not fly at all. Its main wheels castored (`max_steer` 360), so nothing held
it straight: at full throttle it turned circles, and lightly loaded its
state went to NaN. It is JSBSim's c172 with a bigger engine - the same
inertias, geometry, centre of gravity and, largely, aerodynamics - and its
flaps stopped at 30 degrees where the 182S's go to 38. `tools/make_c182.py`
makes glideslope's from the pinned files with each change listed and
justified, and a test fails if the committed files differ from what it
makes.

| Figure | Handbook | Range | JSBSim's, with the handbook's weights and flaps | glideslope |
| --- | --- | --- | --- | --- |
| Static RPM, full throttle | 2300 to 2400 (TCDS) | 2300 to 2405 | 2400 | 2400 |
| Take-off ground roll, flaps 20 | 795 ft | ±10% | turned circles | 793 |
| Climb, sea level, 80 KIAS | 924 ft/min | ±10% | 818 | 895 |
| Cruise, 6,000 ft, 80%, best power | 140 KTAS | ±3 kt | - | 138.8 |
| Maximum speed, sea level | 145 KTAS | ±3 kt | 136 | 144.0 |
| Glide, 75 KIAS | 8.9:1 (read from a chart) | ±10% | 9.76 | 8.93 |
| Stall, flaps up | 54 to 56 KCAS | 52 to 58 | 59.6 | 55.3 |
| Stall, flaps 20 | 50 to 52 KCAS | 48 to 54 | 54.7 | 51.4 |
| Stall, flaps full | 49 to 50 KCAS | 47 to 52 | 54.0 | 49.8 |

The handbook is the Cessna Model 182S Skylane Information Manual (1997), with
its airspeed calibration table converting indicated speeds; the static rpm
and the flaps' travel are from the type certificate data sheet, 3A13.

**What the changes are, in short:** the handbook's empty weight, seats and
tanks; flaps to 38 degrees; fixed main wheels; a lift curve reaching the
handbook's stalls; the elevator's drag cut to what a tailplane that size
gives in trim (JSBSim charged it a fifth of all the drag at full speed); the
drag at incidence raised 15%, standing in for the windmilling propeller the
handbook glides with - in JSBSim the propeller of a stopped engine stops, its
tables ending where it would windmill; a stopped engine's friction; and 12%
more propeller thrust at low advance ratio, for the take-off.

**The figure flights learned three things.** A cruise figure can name a
manifold pressure and have the mixture leaned for best power, as Cessna's
tables do, and a level speed can be in knots. And a stall is measured to the
point where the stalled aircraft gathers speed again: the 182, with the stick
held back past its stall, dived and zoomed, and the lowest speed of the zoom
had been taken for its stall. The Cessna 172P's stalls and the Mosquito's
measure what they did.
