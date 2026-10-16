### The Piper PA-28-180 Cherokee against its handbook, 2026-09-19 — part of an item

The second of the three light aircraft in Phase 5's "the light aircraft fly
to their figures", after the Cessna 182S; the Piper Cub, above, completes the
item.

**JSBSim's pa28 is the right airframe with the wrong propeller.** Its 30 ft
constant-chord wing, fixed gear, 40-degree flaps and 180 hp engine are the
1962-72 Cherokee 180's, but it had a constant-speed propeller, which only the
retractable Arrow has; one seat and one tank, so the handbook's 2,400 lb could
not be loaded; main wheels that castored, as the 182's did; flaps whose
first detent was 15 degrees, not 10, and gave less lift at 25 than at 10; and
a tail half as powerful as a stabilator's, too weak to lift the nose for
take-off or to reach the stall with full flap. It could not be flown to the
handbook as it stood, so there is no column for it below. `tools/make_pa28.py`
makes glideslope's from the pinned files with each change listed and
justified, and a test fails if the committed files differ from what it makes.

| Figure | Handbook | Range | glideslope |
| --- | --- | --- | --- |
| Static RPM, full throttle | 2275 to 2450 (TCDS) | the same | 2348 |
| Take-off ground roll, flaps 25 | 720 ft | ±10% | 747 |
| Climb, sea level, 85 mph | 725 ft/min | ±10% | 712 |
| Cruise, 7,000 ft, 75%, 2,640 rpm | 143 mph, 124.3 KTAS | ±3 kt | 124.5 |
| Top speed, sea level | 152 mph, 132.1 KTAS | ±3 kt | 132.0 |
| Stall, flaps up | 67 mph CAS, 58.2 KCAS | ±2 kt | 57.4 |
| Stall, flaps 40 | 57 mph CAS, 49.5 KCAS | ±2 kt | 48.8 |

The handbook is the Cherokee 180 "E" Owner's Handbook (1969, revised 1974),
in mph with no airspeed calibration; the stalls, as calibrated, are the 1962
Airplane Flight Manual's, and the static rpm the type certificate data
sheet's, 2A13. Neither gives a glide ratio, so there is no glide figure.

**What the changes are, in short:** the handbook's empty weight, seats and
tanks; flaps at 10, 25 and 40 degrees; fixed main wheels; a fixed-pitch 76 in
propeller, with less power and more thrust at low advance ratio for the
static rpm and the take-off; the 172's gear dampers; a stabilator's moment
and lift; a lift curve and flap lift reaching the flight manual's stalls;
induced drag with ground effect, which the model had on its lift and not its
drag; and its zero-lift and gear drag, for the top speed.

**glideslope raised fixed gear.** Every flight begun in the air puts the gear
lever up, and glideslope set the gear's position from it whatever the gear
was. The Cessnas never noticed; the pa28 charges its gear's drag by that
position, and with its fixed gear "up" it shed more than half its zero-lift
drag - 33 knots at full throttle, 140 against 107. Fixed gear now stays down
whatever the lever says, and a test holds it there.
