### The Learjet's heights from her drawings: her centre of gravity, nacelles and tanks, 2026-10-09 — no item closed

**What is still missing, first.** Both items stay `[ ]`. **She still cannot
be rotated early**: held fully back from 85 percent of the speed the book
rotates her at, her nose wheel comes off at 113 knots (was 114.5) and she
leaves at 124.1 (was 125.7) against her rotation speed of 125.3 - before
it, but inside the five knots her lesson allows, so it is not called early,
and she is five feet up at 128 (was 130). Her centre of gravity's height,
the missing source the item named, was not the cause. What would be: her
stabilizer's rigging neutral on the flight model's scale (3.6 degrees,
estimated; the AFM's take-off setting alone would put it at 2.25 to 3.29,
which cannot trim the cruise), or her elevator's power - neither has a
source. Tried and not kept: at 2.8 degrees her nose comes off at 110 and,
hauled off, she strikes her tail at 121 knots. **Her tail strike**: no
published attitude was found; her drawings allow 14.0 to 15.6 degrees
(below), and 14.0 is kept.

**The sources.** The NTSB's docket for DCA00MA005 (the 1999 Learjet 35,
Airworthiness 13, attachment 2, "Configuration Drawings, Learjet Model 35",
SHA-256 `9e49e221...`) prints her maintenance manual's chapter 6; the
attachment's link that had been found returns nothing, and the docket's own
listing gives the one that works (`docs/ASSETS.md`). Its figure 1 sheet 2
has the front and side views; MM 6-00-01 (the intaerotechamt copy already
pinned) gives the fuselage's outside diameter, 5 ft 3 in, and 6-00-02 that
WL 0 is its bottom: the centreline is WL 31.5. Rendered at 200 dpi, the
front view scaled by its 8 ft 3 in tread draws the fuselage 63.0 in across,
and the side view scaled by its length and wheelbase draws it 62.5 deep.

**Measured, each from the fuselage, not the drawn ground**:
- the nacelles' centreline: front view 17.2 in above the fuselage's centre
  and 47.6 in out; side view 15 in under its top. **WL 48, BL 47.5** (were
  WL 22, BL 42, estimated). MM 6-00-03's engine beams are at WL 40.
- the tip tanks: 24 in across, centres WL 19 (tanks were at WL -4, below the
  fuselage); their strike contacts at their bottoms, WL 7 (were WL -8).
- the wing: WL 14.5 at the fuselage's side, mid-depth; its tanks at WL 16.
  The fuselage tank WL 20 and the payload WL 26 (the floorboards, WL 5.87,
  and a seated person) are estimated.
- **The drawn ground is some 4.5 in high**: the drawn main wheel is 13 in
  across with its axle 14.7 in under the fuselage, where the tyre is 17.5 in
  and the AFM's axle WL -16.72. The model's ground stays the AFM's. The
  ventral fin was measured from the drawn ground (2026-10-06); from the
  fuselage it would be 6 in higher, a strike at 15.6 degrees rather than
  14.0. The 12 ft 3 in height reads 2 in short one way and 3 in over the
  other, so the drawing does not settle it, and the fin is left as drawn.

**The centre of gravity's height - an estimate.** No document prints it. It
is built up as a designer does before weighing: Raymer's class I mass
groups for transports (as tabulated in Scholz's Aircraft Design notes,
chapter 10, table 10.2) at her maximum take-off weight, each at its height
from the drawings - wing 938 kg at WL 16.5, fuselage 1,296 at 31.5,
horizontal tail 136 at 120, fin 96 at 85, gear 357 at -5 to -8, power plant
855 at 48, systems and items 1,411 at 25. **WL 30.5**, near the fuselage's
centreline, as a low-wing aeroplane's with her engines on her fuselage is;
28.5 to 32.3 as the systems go from WL 18 to the centreline. The sum, 5,090
kg, is 10% over her 4,614 empty. **The aerodynamic reference point moved from WL -5
to WL 30.5**, the same height: her pitching moments are NASA's, identified
about her own centre of gravity. Raymer's general-aviation column instead
would put the whole at WL 29.8 (its sum 2,909 kg, 37% short of her empty
weight; the transport column is used, as she is certificated under part
25). Loaded, her centre of gravity is WL 25.8
(maximum) to 29.6 (light).

**Why the rotation hardly moved.** Raised 26 in, the thrust's nose-down
moment about the main wheels grew by about 14,700 ft-lb; raised 28 in, the
acceleration's nose-up moment at the centre of gravity grew by about
14,300. They nearly cancel, as they must when both rise with the fuselage.

**Test.** `the_learjet_35a_carries_her_weight_and_thrust_at_the_heights_her_drawings_give`
(new): empty (no fuel, no payload) her centre of gravity is WL 28.0 to
32.5, the estimate's own range (it is 30.5); at each of her figures' four
loadings, counted, WL 24.0 to 31.0 (25.8 to 29.6); every tank between WL 7
and 31.5, her two tip tanks at WL 19 within 2 in; each engine's thrust
within 2 in of WL 48, BL 47.5. **Seen to fail** five ways: on the old model
("empty, her centre of gravity at WL 0.0"); with the engines alone put back
at WL 22; with a wing tank alone put back at WL -4; with the tip tanks
alone at WL 10; with the empty centre of gravity alone at WL 26.

**Unchanged**: the selftest's hash, be036519d2c19ea0. The committed Learjet
is what its script writes. **Not the full suite**: two targeted runs in
linux-release, 78 and 286 tests, all passed - every test whose name says
Learjet (her figures, the cruise trim on her stabilizer - so the cruise still
trims - her stalls, orbits both ways, and the plan-speed tests), every
take-off, rotation, over-rotation (she strikes at 14.3 degrees against 14.0
by her contacts), lesson and selftest test, and every approach, landing,
circuit, crosswind, stall, orbit and committed-asset test. **Lateral and
directional, for the engines' 5.5 in wider butt line and the tip tanks'
contacts 15 in higher**: rerun were her engine-out climb, her heading in a
20 kt crosswind at every plan speed, her orbits turning left and right, and
the landing and approach tests that judge a wing-tip strike; not rerun were
the multi-process tests that stop an engine on a server, and no test flies
her with an engine stopped on the take-off roll. The comments that gave
her early-rotation figures (the script, the two take-off tests, the
business-jet take-off lesson) give the new ones.
