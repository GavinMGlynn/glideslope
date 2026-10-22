#!/usr/bin/env python3
"""make_j3cub.py - glideslope's Piper J-3 Cub, made from JSBSim's J3Cub.

JSBSim's J3Cub, as pinned in ext/jsbsim, is an Aeromatic model of the J3C-65
with a Continental A-65-8 and a McCauley metal propeller, its aerodynamics
from a Cranfield thesis on a Cub (Du, 2011). The figures it is held to are the
Owner's Manual for the J3C-65's, Piper's booklet "How to Fly a Piper Cub"
(1945) and the type certificate's, FAA Aircraft Specification A-691, in
assets/figures/j3cub.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_j3cub.py           write the files
    python3 tools/make_j3cub.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Files and names       The pinned model keeps its engine, propeller and
                        flight controls in files whose names have spaces, in
                        its own Engines/ and Systems/ directories; here they
                        are engine/engA65-8.xml, engine/prop_j3cub_74in.xml
                        and aircraft/j3cub/Systems/conventional-controls.xml,
                        and the model is j3cub.

  Airframe (aircraft/j3cub/j3cub.xml)
    Weights and loading, from the type certificate: the booklet's empty weight,
                        680 lb, where the model had 765; the two seats, at +9
                        and +36 in from the wing's leading edge, and 20 lb of
                        baggage at +49, where the model had one seat; the
                        12-gallon tank at -18, where the model's was at the
                        aerodynamic reference point. The model's origin is its
                        aerodynamic reference, the wing's quarter chord, so the
                        leading edge is 15.75 in ahead of it - which puts the
                        main axle, as the manual has it, 2 1/2 in (here 3.15)
                        from the leading edge.

    The elevator's travel, 8 degrees each way -> 34 up and 29 down
                        A-691's (in Systems/conventional-controls.xml). With
                        the model's the elevator could not raise the nose to
                        the wing's stall: the stick fully back, at the gross
                        weight, it flew on at 41 knots, where with the travel
                        widened it stalls at 36.
    Drag_airframe 0.020, added
                        The model's drag at zero lift was its wing section's
                        (the USA-35B's, 0.019) and its gear's, 0.004: nothing
                        for the fuselage, tail, lift struts and wires, and it
                        glided at 13.5 to 1, against the booklet's 10.
    Drag_induced 0.0485 -> 0.0573
                        The model's was a span efficiency of 0.95, a wing's
                        alone; 0.80 is a light aircraft's.
    Main gear springs 1,000 -> 2,093 lb/ft a wheel
                        Derived, not published: no J-3 spring rate or static
                        deflection is in print. From her shock rings' rating
                        and her gear's drawings (see gear_spring_lbs_per_ft):
                        drawing 31423, "Strut assembly, shock" (the strut
                        A-691 requires at 1,220 lb), shows two 31322 shock
                        cords, each wound twice over two spools 6.77 in apart:
                        four strands a ring. Its cord is drawn at 0.67 to 0.69
                        in, the 11/16 in ring (Piper Service Memo 12 and
                        Aircraft Spruce's catalogue p. 306: P/N 1174, 11/16 x
                        7 1/2 in, 730 lb test, four to a J-3). MIL-C-5651D's
                        Table III makes a ring's test load its pull at 100%
                        elongation, twice a single cord's - two strands - so
                        one strand pulls 365 lb at 100%. A spool spacing grown
                        by d stretches a ring of 25.72 in (pi x (7.5 + 11/16))
                        by 4d, and its four strands pull 4 x 365 x 4d / 25.72:
                        454 lb/in for the strut's two rings. Drawing 31513,
                        "Gear installation - landing", puts the vee's hinge,
                        the strut's ends and the tyre where the tyre rises
                        1.61 in for every inch the strut extends; the rate at
                        the tyre is 454 / 1.61^2 = 174 lb/in. Judgements: the
                        cord's pull taken as linear to 100%, which
                        Table III's 50/75/100% figures hold to within 10%;
                        the strands' slant (cosine 0.99) is left out. The
                        later J-3's 1280-HD (3/4 x 8 in, 950 lb) would make
                        2,550 lb/ft; her own drawing draws the 11/16 cord.
    Main gear preloaded, in series with the tyre
                        The rings are stretched when fitted, so a wheel's
                        strut does not move until the wheel carries their
                        pull, and a Cub stands on it. On drawing 31423 each
                        ring goes four times between the spools (6.77 in) and
                        four half turns round them: the spools are 3/4 in
                        (measured 0.75), so round them the cord's centre runs
                        on 3/4 + 11/16 = 1.4375 in. Fitted, a ring is
                        4 x 6.77 + 2 x pi x 1.4375 = 36.11 in against its free
                        25.72: 40.4% stretched. Its eight strands then pull
                        8 x 365 x 0.404 = 1,180 lb along the strut, 731 lb at
                        the tyre (the lever, 1.61) - more than a wheel
                        carries standing at any of her loadings (at most 542
                        lb, at her gross weight), so the strut stands closed and she stands on her
                        tyres. JSBSim's spring has no preload, and a force
                        that jumps from nothing to 731 lb as a wheel touches
                        has nothing to settle on; the tyre in series is what
                        makes it continuous. The tyre: Goodyear's 8.00-4, 4
                        ply (Aviation Data Book 2022, Section 4C,
                        https://www.goodyearaviation.com/resources/pdf/Data-Section-2022.pdf):
                        rated 1,100 lb at 24 psi, 18.0 in across at most, a
                        static loaded radius of 6.65 in at its rated load -
                        2.35 in of deflection at 1,100 lb. Her manual's 15
                        psi (assets/aircraft/j3cub.aircraft) scales it by
                        (p + 0.08 p_rated): NACA TN 4110 (Smiley and Horne,
                        1958), equation 22, a tyre's load at a deflection
                        being in proportion to its pressure plus an
                        effective carcass pressure of 0.08 of its rated one.
                        So 468 lb/in at 24 psi, 306 at 15 - taken as linear
                        through the rated point (equation 23's curve puts her
                        standing deflection about 4% deeper). The wheel's force
                        against its compression is then the tyre's alone up
                        to 731 lb (2.39 in), and the tyre and the rings in
                        series past it, 111 lb/in (gear_strut_force_table).
                        The pinned damping is kept, 500 lb/ft/s and 850 on
                        rebound. JSBSim compresses a wheel along her body's
                        vertical and turns its force normal to the ground,
                        as it does every gear: standing at 13 degrees her
                        wheels drop about 5% less than a tyre squashed
                        straight down, under 0.1 in of her 1.1 to 1.7 in,
                        and that is kept. Judgements: the strands' slant left out, as
                        in the rate (it would add about 1% to their length,
                        some 3% to the preload); the stop disc's end of the
                        stroke not modelled, as before.

  Engine (engine/engA65-8.xml)
    maxrpm 2800 -> 2300 The A-65-8's limit, "all operations, 2300 r.p.m.
                        (65 hp)" (A-691, item 311C(1)). JSBSim's piston engine
                        makes its rated power at maxrpm, so the model's made
                        its 65 hp only at 500 rpm past the Cub's limit.
    MIXTURE table       The FAA's shape of a petrol engine's power against its
                        mixture, for JSBSim's own, which made most power at
                        9.9 to 1; see tools/piston_mixture.py. Its rated 65 hp
                        at 2,300 rpm, full rich at sea level, is unchanged
                        (64.4 hp, against 64.5).
  Float carburettor (aircraft/j3cub/j3cub.xml)
                        A channel meters the mixture as a float carburettor
                        does, richening as the square root of the density
                        rather than as the pressure (tools/piston_mixture.py).
                        With JSBSim's metering the Cub, which has no mixture
                        lever, flown solo stopped climbing at 8,016 ft
                        against its manual's 14,000; with it, 13,637.

  Propeller (engine/prop_j3cub_74in.xml)
    Diameter 75 -> 74 in The McCauley 1A90CF or 1B90CM, A-691's propeller item 2,
                        is "not over 74 inches".
    The tables          The model's propeller tables are JSBSim's generic
                        fixed-pitch ones, the same numbers as the Cessna
                        172P's 75 in propeller's, coarse enough to stop
                        thrusting only at an advance ratio of 1.17: on the
                        Cub's 65 hp it turned 1,590 rpm static, against the
                        type certificate's 1,950 to 2,250. Its name, CM7445,
                        reads as 74 in by 45 in of pitch, and its advance
                        ratios are drawn in by that pitch over the generic
                        blade's (22 degrees at three-quarters of its radius,
                        pi x 0.75 x tan 22 = 0.95), 0.64; its power by 0.57,
                        for 2,060 rpm static; and its thrust by 0.69, for the
                        manual's climb and cruise - the generic curve's
                        efficiency, 0.85 at best, now 0.66.

**The alpha-rate terms on the dynamic pressure without its sideways part**
(tools/alpha_rate.py, JSBSim issue #89): braked to a stop in a crosswind, her
angle of attack is undefined, its rate swings, and multiplied by the whole
dynamic pressure it pitched her by tens of degrees a second.

**Her pitch terms keyed to the angle of attack on the same dynamic
pressure** (alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15): Pitch_alpha,
and Pitch_propwash. With the air from the side her angle of attack is
anything at all - 22 to 60 degrees standing in 15 kt across - and on the
whole dynamic pressure her Cm-alpha term pushed her nose down by up to 1,500
ft-lb against the ~1,080 her weight holds her tail down with: stick central
she went over standing on her brakes in 15 kt across, at every loading.
JSBSim's guidance, from issue #89 and the FlightGear wiki's "JSBSim
Aerodynamics": QBarUW "is more appropriate for Cm" than the whole QBar. In
flight, at a few degrees of sideslip, the two differ by its cosine squared.
Pitch_propwash did not cause it - with Pitch_alpha alone moved, the air's
most nose-down moment standing in 15 kt across is the same, 37 to 239 ft-lb
at her figures' loadings - and is moved as the same kind of term, a pitching
moment keyed to the angle of attack. (Left on the whole dynamic pressure, she
rolled 20.8 degrees and struck her airframe at the end of the 3 kt
walking-pace test in 15 kt across: her wing's drag, below, was why.)

**Her lift and her wing's drag keyed to the angle of attack on the same
dynamic pressure** (2026-10-15): Lift_alpha, Lift_propwash and Drag_basic.
Handed to the rollout at 3.5 to 5 kt in 15 kt straight across, she stopped,
stood at taxi power, and was rolled onto her wing: she struck her airframe
at 21 degrees of roll. Traced step by step, the force that rolled her was
Drag_basic: her wing section's drag against the angle of attack, which at
90 degrees is a flat plate's, 1.41. Standing with the air from the side,
atan2(w, u) is anything at all, and each degree she leaned away from the
wind gave the beam wind a w and took her angle of attack on towards 90 (21
to 81 degrees): on the whole dynamic pressure Drag_basic grew from 33 lb to
155 as she leaned, and the sideways force on her from 171 lb to 274, at her
wing six feet above her wheels - a lean that fed itself, over the edge her
weight holds her on. Her dihedral (Roll_beta, 250 to 330 ft-lb) did not
grow as she went. The wing meets the air face-on only in the plane of u
and w; the sideways part runs along its span and meets its leading edge.
On aero/qbarUW-psf - the guidance of issue #89 and the FlightGear wiki's
"JSBSim Aerodynamics" for lift, and the same reasoning for the drag of the
same wing at the same angle of attack - she leans 4.2 to 4.4 degrees at
every walking pace, 2 to 5 kt, every loading and either side, and stays
there. Her lift moves for the same reason: it counted the crosswind (358 lb
up braking from 10 kt in 15 kt across at her gross weight, against 104 in
calm air). Moved alone (an earlier try), it rolled her the more: without
its ~100 lb she stood a degree more nose-up, her angle of attack 47 degrees
against 42, Drag_basic the more, and she struck at every pace. Not moved: Lift_pitch_rate
(a damping term) and her roll terms keyed to alpha (Roll_beta,
Roll_yaw, Roll_differential_propwash): in flight issue #89 offers no
guidance for the rolling moment, and standing they did not cause it. Her
figures in flight: see PROJECT_STATUS (2026-10-15).

**Her wing's drag carried on past 90 degrees of angle of attack to 180**
(2026-10-16): Drag_basic's table. The pinned table ends at plus and minus
90 degrees with a flat plate's 1.41, and JSBSim holds a table's last value
past its end, so with the air from behind - standing tail to the wind, her
angle of attack near 180 degrees - she was pushed as if her wing were
broadside. A wing in reversed flow meets the air edge-on, its sharp trailing
edge first. The rows past 90 are the NACA 0012's drag coefficient from R. E.
Sheldahl and P. C. Klimas, "Aerodynamic Characteristics of Seven Symmetrical
Airfoil Sections Through 180-Degree Angle of Attack for Use in Aerodynamic
Analysis of Vertical Axis Wind Turbines", SAND80-2114, Sandia National
Laboratories, March 1981, Table 2: at 95 to 180 degrees by fives, the same
at every Reynolds number it lists (hers at 15 kt, 0.7 to 1.0 million, pages
20 to 22) - 0.14 at 170 degrees, 0.025 at 180. A symmetrical section's
drag is even in its angle of attack, so the same rows go to -180.

Two judgements, not the report's:
  - Her cambered USA-35B takes a symmetrical section's curve in reversed
    flow. The report says the four sections it measured had similar drag
    past 20 degrees and that past the stall the data hold at every
    Reynolds number (page 11); that a cambered section's would be alike too
    is an inference from that, not something it measured.
  - The whole curve is scaled by 1.41/1.80 (0.78): their figures are a
    section's, two-dimensional, 1.80 broadside (near Hoerner's 1.98 for a
    two-dimensional flat plate, page 11), and her table's broadside is her
    finite wing's 1.41, so the drag is continuous at 90 degrees. Near 180
    degrees the drag is edge-on profile drag, which a finite wing does not
    reduce, so there the scaling is probably low: 0.0196 at 180 against the
    section's 0.025, and at 164 degrees, where she stands tail to the wind,
    0.19 against 0.24 - about 7 lb at 15 kt.

Stood on her brakes tail to 15 kt, her angle of attack is -164 degrees; the
air's force along the wind on her was 195 lb, her wing's drag the flat
plate's 1.41, and is 29 lb, her wing's 0.19 and her airframe's and gear's
0.024 on top (23 to 24 lb since her lift and pitching moment below, which
stand her at -167 degrees).

**Her wing's lift and pitching moment past 90 degrees** (2026-10-21):
Lift_alpha's table, Pitch_alpha, and a new term, Pitch_reversed_flow.
Tail to the wind her lift was the pinned table's nought beyond 90 degrees,
and her Cm-alpha term, linear in the angle of attack, made 5.8 of nose-up
moment at -164 degrees - 4,150 ft-lb in 15 kt, sitting her on her tail
wheel.
  - Her lift past 90 degrees is the same section's, Sheldahl and Klimas's
    Table 2 at a Reynolds number of 1,000,000 (page 22; at 700,000 the same
    rows) - -0.66 at 160 degrees, -0.85 at 170, nought at 180 - odd in the
    angle, scaled by 1.41/1.80 as the drag is. The owner's decision
    (2026-10-20): the NACA 0012 is the nearest measured section to her
    USA-35B through 180 degrees; that her cambered section's would be alike
    reversed is an inference, as for the drag. Her model's ground effect
    multiplies it as it does her lift in flight.
  - Her pitching moment past 90 degrees is derived, not measured on her (the
    owner's decision, 2026-10-20): the section's force normal to its chord,
    Cl cos(alpha) + Cd sin(alpha), scaled as above, acting at its centre of
    pressure - half the chord back at 90 degrees, broadside, moving straight
    to three quarters at 180, where the reversed section's sharp edge leads
    and its quarter chord is her three-quarter chord. About her quarter
    chord, her aerodynamic reference, that is -(x_cp - 0.25) times the
    normal force: 0.28 nose up where she stands. The moving centre, rather
    than three quarters throughout, is this script's: NACA TN 3361
    (Critzos, Heyson and Boswinkle, 1955,
    https://ntrs.nasa.gov/citations/19930084501), figure 1, measured the NACA
    0012's quarter-chord moment through 180 degrees at 1,800,000 - about
    -0.5 at 90 to 120 degrees and -0.4 to -0.45 at 150 to 175, negative from
    the stall to 180 and antisymmetric about it - and the moving centre
    meets that within about 0.05 at every angle, where three quarters
    throughout would give -0.85 at 110, off by 0.35. Read off a graph, it is a check, not
    the figure; tail to the wind the two differ by 0.02 (0.30 against 0.28).
  - It is her whole moment past 90: her tailplane in reversed flow, and her
    elevator's term (Pitch_elevator, unchanged, the same sign either way
    round), are not modelled apart.
  - Pitch_alpha is multiplied by a table of the angle of attack that is
    exactly one within 90 degrees either way and nought from 95, so in
    flight it is unchanged to the bit; Pitch_reversed_flow is nought within
    90 and the derived moment from 95. Between 90 and 95 the one fades into
    the other.
Standing tail to 15 kt she now stands at -167 degrees, her wing pressing
her down 85 to 88 lb (the air from behind and above meets its upper
surface) and its moment 200 ft-lb nose up.

**Her drag in a beam wind from her side area** (2026-10-17): Drag_beta's
rows at plus and minus 90 degrees of sideslip. The pinned model's were 1.23
on her wing's area, 220 sq ft of drag area broadside, and with her
airframe's and gear's 0.024 on top she was pushed sideways by 171 lb standing
in 15 kt straight across, and leaned 4.4 degrees on her soft main gear. A
Cub seen from the side is not that big: her Owner's Manual's dimensions
(J3C-65 Owner's Manual, pages 11 and 12) make her side area 48.7 sq ft -
  - her fuselage 37.4 sq ft: the firewall's depth, 28 3/4 in (page 11),
    from her nose to her wing's trailing edge, then tapering to nothing at
    the tail post. Her nose is where the overall length, 22 ft 4 1/2 in
    (page 11), less the rudder's mean chord aft of the post (its area, 6.55
    sq ft, over its height, 4 ft 5 7/8 in: 17.5 in), puts it ahead of the
    tail post; the post is 2 1/2 + 187 1/2 in aft of the wing's leading edge
    (page 12, by the axle), the trailing edge 63 in (the chord, page 11);
  - her fin and rudder, 4.7 and 6.55 sq ft (page 12).
On it, a flat plate's drag broadside: S. F. Hoerner, Fluid-Dynamic Drag
(1965), Chapter III, page 3-16, figure 28 and its text: "in 3 dimensions
C_D = 1.17", which figure 28 shows holding for a rectangular plate from
square to a span ten times its height (her fuselage's is 8.7). That is 57.0
sq ft of drag area, 0.319 on her wing's, and Drag_beta at 90 degrees is
that less her airframe's and gear's 0.024: 0.295. Its rows at 15 degrees,
her flight's sideslip, are the pinned model's.

Judgements, not the manual's or Hoerner's: her cabin is taken as the
firewall's depth, though its windows stand above it up to the wing (the
manual gives no cabin height); her wheels, gear legs, struts, engine
cylinders and wing tip are not counted. Each would add; the area is the
least the manual's figures make of her. Her fuselage is fabric over a
truss with rounded decking, between a flat plate's 1.17 and a circular
cylinder's 0.6 to 0.7 (figure 28, at her Reynolds number of about 4 x 10^5,
subcritical); the flat plate is the bluffer.

Her Owner's Manual (Taxiing, page 40) has a pilot hold the ailerons into a
turn into wind "when ground winds are in excess of 15 M.P.H.", to prevent
"the wind 'picking up' a wing", and the NTSB's NYC96LA043 records a J3
taxiing in 320 at 15 gusting 20 kt whose right wing the wind lifted: a real
Cub can be rolled by a strong gusty wind across, with her ailerons left
wrong. A steady 15 kt across, with the rollout holding her wings level,
is not that, and does not.
"""

import math
import pathlib
import re
import sys

import alpha_rate
from piston_mixture import with_best_power_mixture, with_float_carburettor

ROOT = pathlib.Path(__file__).resolve().parent.parent
PINNED = ROOT / "ext" / "jsbsim"
PINNED_CUB = PINNED / "aircraft" / "J3Cub"
OUT = ROOT / "assets" / "jsbsim"

MODEL = "j3cub"
ENGINE = "engA65-8"
PROPELLER = "prop_j3cub_74in"
SYSTEM = "conventional-controls"

EMPTY_WEIGHT_LBS = 680
FUEL_LBS = 72           # 12 US gallons at 6 lb
# The propeller's pitch over its diameter, 45 in over 74, against the pinned
# table's, a 22-degree blade at three-quarters of its radius (pi x 0.75 x
# tan 22 = 0.95): its curves are drawn in on the advance ratio by the ratio.
FINER_PITCH = round((45 / 74) / 0.952, 3)
POWER_SCALE = 0.57
THRUST_SCALE = 0.69
AIRFRAME_DRAG = "0.0200"
INDUCED_DRAG = "0.0573"   # 1 / (pi x 6.94 x 0.80)
# The elevator's travel, A-691's: 34 degrees up, 29 down.
ELEVATOR_UP_RAD = "0.5934"
ELEVATOR_DOWN_RAD = "0.5061"
# Her main gear's springs, derived from her shock rings' rating (the docstring
# says how). The ring: P/N 1174, 11/16 x 7 1/2 in, 730 lb at 100% elongation -
# Aircraft Spruce catalogue p. 306, https://www.aircraftspruce.com/pdf/2025Individual/Cat25306.pdf,
# and Piper Service Memo 12; MIL-C-5651D Table III for what "test" means.
RING_TEST_LBS = 730
RING_SECTION_IN = 11 / 16
RING_INSIDE_DIAMETER_IN = 7.5
RINGS_A_STRUT = 2           # drawing 31423: two 31322 shock cords
STRANDS_A_RING = 4          # each wound twice over the two spools
# Drawing 31513 (front view, 3 in to 1 ft), measured off the Internet
# Archive's scan rendered at 100 dpi, in inches of the aeroplane, x outboard
# and y down: the vee's hinge at the fuselage fitting, the shock strut's
# outer eye at the axle and its inner eye at the cabane vee, and the tyre's
# centre plane.
GEAR_HINGE_IN = (383 * 0.04, 78 * 0.04)
STRUT_OUTER_EYE_IN = (695 * 0.04, 602 * 0.04)
STRUT_INNER_EYE_IN = (80 * 0.04, 300 * 0.04)
TYRE_CENTRE_X_IN = 920 * 0.04


def tyre_rise_per_strut_extension():
    """The tyre's rise for an inch of the shock strut's extension, the vee
    swinging about its hinge: the tyre rises by its arm from the hinge, the
    strut's outer eye moves square to its own, and the strut extends by that
    movement along itself."""
    hx, hy = GEAR_HINGE_IN
    sx, sy = STRUT_OUTER_EYE_IN
    cx, cy = STRUT_INNER_EYE_IN
    length = math.hypot(sx - cx, sy - cy)
    eye_motion = (-(sy - hy), sx - hx)
    extension = abs(eye_motion[0] * (sx - cx) + eye_motion[1] * (sy - cy)) / length
    return (TYRE_CENTRE_X_IN - hx) / extension


def gear_spring_lbs_per_ft():
    """A main wheel's spring: the strut's rings' pull for an inch of its
    extension, over the square of the tyre's rise for it, in lb/ft."""
    ring_length = math.pi * (RING_INSIDE_DIAMETER_IN + RING_SECTION_IN)
    strand_pull = RING_TEST_LBS / 2       # a ring is two strands in the test
    strut = RINGS_A_STRUT * STRANDS_A_RING * strand_pull * STRANDS_A_RING / ring_length
    return 12 * strut / tyre_rise_per_strut_extension() ** 2


# Her rings as fitted, drawing 31423 (full size, the scan at 150 ppi): the
# spools' centres 6.77 in apart, each spool 3/4 in across (112 px), the cord's
# centre on 3/4 + 11/16 in round it; a ring goes four times between the spools
# and four half turns round them.
SPOOL_GAP_IN = 6.77
SPOOL_DIAMETER_IN = 0.75
# Her tyre: Goodyear 8.00-4, 4 ply, type III (Aviation Data Book 2022, Section
# 4C): rated load and pressure, outside diameter (max), static loaded radius.
TYRE_RATED_LBS = 1100
TYRE_RATED_PSI = 24
TYRE_OUTSIDE_DIAMETER_IN = 18.0
TYRE_STATIC_LOADED_RADIUS_IN = 6.65
# Her manual's pressure (J3C-65 manual, as assets/aircraft/j3cub.aircraft).
TYRE_PSI = 15
# NACA TN 4110, equation 22: a tyre's effective carcass pressure, 0.08 of its
# rated pressure.
CARCASS_PRESSURE_OF_RATED = 0.08
# The pinned model's damping, kept.
GEAR_DAMPING_LBS_PER_FPS = 500
GEAR_REBOUND_DAMPING_LBS_PER_FPS = 850
# The table's last compression: past it JSBSim's table holds its last force.
GEAR_TABLE_LAST_FT = 2.0


def ring_preload_stretch():
    """A ring's stretch as fitted, over its free length."""
    free = math.pi * (RING_INSIDE_DIAMETER_IN + RING_SECTION_IN)
    fitted = 4 * SPOOL_GAP_IN + 2 * math.pi * (SPOOL_DIAMETER_IN + RING_SECTION_IN)
    return fitted / free - 1


def gear_preload_lbs():
    """The rings' pull as fitted, at the tyre: what a wheel carries before its
    strut extends."""
    strut = RINGS_A_STRUT * STRANDS_A_RING * (RING_TEST_LBS / 2) * ring_preload_stretch()
    return strut / tyre_rise_per_strut_extension()


def tyre_lbs_per_in():
    """Her tyre's rate at her pressure: its rated load over its rated
    deflection, scaled by (p + 0.08 p_rated) (NACA TN 4110, equation 22)."""
    rated = TYRE_RATED_LBS / (TYRE_OUTSIDE_DIAMETER_IN / 2 - TYRE_STATIC_LOADED_RADIUS_IN)
    carcass = CARCASS_PRESSURE_OF_RATED * TYRE_RATED_PSI
    return rated * (TYRE_PSI + carcass) / (TYRE_RATED_PSI + carcass)


def gear_strut_force_table():
    """A main wheel's force against its compression, (ft, lb): the tyre alone
    up to the rings' preload, then the tyre and the rings in series."""
    tyre = tyre_lbs_per_in() * 12
    gear = gear_spring_lbs_per_ft()
    preload = gear_preload_lbs()
    knee = preload / tyre
    series = 1 / (1 / tyre + 1 / gear)
    return ((0.0, 0.0), (knee, preload),
            (GEAR_TABLE_LAST_FT, preload + series * (GEAR_TABLE_LAST_FT - knee)))


def with_preloaded_main_gear(text, side, unit):
    """A main wheel's spring and dampers replaced by a strut force: the table,
    and the pinned dampers on its compression's rate."""
    names = re.findall(r"<contact type=\"\w+\" name=\"(\w+)\">", text)
    if names.index(side) != unit:
        raise SystemExit(f"make_j3cub: {side} is gear/unit[{names.index(side)}], not [{unit}]")
    rows = "\n".join(f"                  {ft:.6f}  {lbs:.1f}" for ft, lbs in gear_strut_force_table())
    rate = f"gear/unit[{unit}]/compression-velocity-fps"
    force = (
        "<strut_force>\n"
        "      <!-- glideslope: preloaded by her shock rings, in series with her tyre\n"
        "           (tools/make_j3cub.py says how). -->\n"
        "      <function>\n"
        "        <sum>\n"
        "          <product>\n"
        "            <value> -1 </value>\n"
        "            <table>\n"
        f"              <independentVar lookup=\"row\">gear/unit[{unit}]/compression-ft</independentVar>\n"
        "              <tableData>\n"
        f"{rows}\n"
        "              </tableData>\n"
        "            </table>\n"
        "          </product>\n"
        "          <ifthen>\n"
        f"            <ge> <property>{rate}</property> <value> 0.0 </value> </ge>\n"
        f"            <product> <value> -{GEAR_DAMPING_LBS_PER_FPS} </value> <property>{rate}</property> </product>\n"
        f"            <product> <value> -{GEAR_REBOUND_DAMPING_LBS_PER_FPS} </value> <property>{rate}</property> </product>\n"
        "          </ifthen>\n"
        "        </sum>\n"
        "      </function>\n"
        "    </strut_force>")
    return replace_once(
        text, r"(name=\"" + side + r"\">.*?)<spring_coeff\s+unit=\"LBS/FT\">\s*1000\.00\s*</spring_coeff>\s*"
        r"<damping_coeff\s+unit=\"LBS/FT/SEC\">\s*500\.00\s*</damping_coeff>\s*"
        r"<damping_coeff_rebound\s+unit=\"LBS/FT/SEC\">850</damping_coeff_rebound>",
        lambda m: m.group(1) + force, f"the {side} spring")


# The wing's leading edge, the type certificate's datum, in the model's frame.
LEADING_EDGE_X = -15.75
# A wing section's drag past 90 degrees of angle of attack, to 180: Sheldahl
# and Klimas, SAND80-2114 (Sandia, 1981), Table 2, the NACA 0012's, the same
# at every Reynolds number from 10,000 to 10,000,000 (at 0.7 and 1.0 million,
# hers at 15 kt, pages 20 to 22). By degrees past 90 to 180; their 90's is 1.80.
REVERSED_FLOW_CD = ((95, 1.780), (100, 1.750), (105, 1.700), (110, 1.635), (115, 1.555),
                    (120, 1.465), (125, 1.350), (130, 1.225), (135, 1.085), (140, 0.925),
                    (145, 0.755), (150, 0.575), (155, 0.420), (160, 0.320), (165, 0.230),
                    (170, 0.140), (175, 0.055), (180, 0.025))
SECTION_BROADSIDE_CD = 1.80
# Her wing's broadside drag, the pinned table's at 90 degrees, over the section's.
BROADSIDE_SCALE = 1.4091 / SECTION_BROADSIDE_CD
# Its lift past 90 degrees, the same table at a Reynolds number of 1,000,000
# (page 22; the 700,000 rows on page 21 are the same).
REVERSED_FLOW_CL = ((95, -0.070), (100, -0.220), (105, -0.370), (110, -0.510), (115, -0.625),
                    (120, -0.735), (125, -0.840), (130, -0.910), (135, -0.945), (140, -0.945),
                    (145, -0.910), (150, -0.850), (155, -0.740), (160, -0.660), (165, -0.675),
                    (170, -0.850), (175, -0.690), (180, 0.000))
# Her side area, from her Owner's Manual's dimensions (pages 11 and 12), in
# inches: the overall length, the firewall's depth, the wing's chord, the
# wing's leading edge to the axle and the axle to the tail post; the rudder's
# height; and her fin's and rudder's areas in sq ft.
OVERALL_LENGTH_IN = 22 * 12 + 4.5
FIREWALL_DEPTH_IN = 28.75
CHORD_IN = 63.0
LEADING_EDGE_TO_TAIL_POST_IN = 2.5 + 187.5
RUDDER_HEIGHT_IN = 4 * 12 + 5.875
FIN_AREA_SQFT = 4.7
RUDDER_AREA_SQFT = 6.55
# A flat plate broadside, in three dimensions: Hoerner, Fluid-Dynamic Drag
# (1965), page 3-16, figure 28.
BROADSIDE_PLATE_CD = 1.17
WING_AREA_SQFT = 178.5
AIRFRAME_AND_GEAR_CD = 0.0200 + 0.0040   # Drag_airframe and Drag_gear


def side_area_sqft():
    """Her side area from her Owner's Manual's dimensions: her fuselage the
    firewall's depth from her nose to her wing's trailing edge, then tapering
    to nothing at the tail post, and her fin and rudder."""
    rudder_chord = RUDDER_AREA_SQFT * 144 / RUDDER_HEIGHT_IN
    nose_ahead_of_leading_edge = OVERALL_LENGTH_IN - rudder_chord - LEADING_EDGE_TO_TAIL_POST_IN
    full_depth = nose_ahead_of_leading_edge + CHORD_IN
    tapering = LEADING_EDGE_TO_TAIL_POST_IN - CHORD_IN
    fuselage = FIREWALL_DEPTH_IN * (full_depth + tapering / 2) / 144
    return fuselage + FIN_AREA_SQFT + RUDDER_AREA_SQFT


def beam_drag_cd():
    """Drag_beta at 90 degrees of sideslip: her side area's drag broadside, on
    her wing's area, less the drag her airframe and gear have at any angle."""
    return BROADSIDE_PLATE_CD * side_area_sqft() / WING_AREA_SQFT - AIRFRAME_AND_GEAR_CD


def with_beam_drag_from_side_area(text):
    """`text` with its sideslip drag at 90 degrees either way from her side
    area."""
    m = re.search(r'<function name="aero/force/Drag_beta">.*?</function>', text, re.S)
    if not m:
        raise SystemExit("make_j3cub: no Drag_beta - has the pinned model changed?")
    value = f"{beam_drag_cd():.4f}"
    new = replace_once(m.group(0), r"(\n\s*-1\.57\s+)1\.2300", rf"\g<1>{value}",
                       "the sideslip drag at -90 degrees")
    new = replace_once(new, r"(\n\s+1\.57\s+)1\.2300", rf"\g<1>{value}",
                       "the sideslip drag at 90 degrees")
    return text[:m.start()] + new + text[m.end():]


def station(inches_aft_of_datum):
    return round(LEADING_EDGE_X + inches_aft_of_datum, 2)


def replace_once(text, pattern, replacement, what):
    new, n = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f"make_j3cub: could not find {what} - has the pinned model changed?")
    return new


def pointmass(name, x, z):
    return (f"   <pointmass name=\"{name}\">\n"
            f"    <weight unit=\"LBS\"> 0 </weight>\n"
            f"    <location unit=\"IN\">\n"
            f"     <x>    {x:.2f} </x>\n"
            f"     <y>     0.00 </y>\n"
            f"     <z>   {z:.2f} </z>\n"
            f"   </location>\n"
            f"  </pointmass>\n")


def with_reversed_flow_drag(text):
    """`text` with its wing's drag table carried on past 90 degrees of angle
    of attack to 180, either way, by Sheldahl and Klimas's section's curve
    scaled to meet the table's own broadside figure at 90."""
    m = re.search(r'<function name="aero/force/Drag_basic">.*?</function>', text, re.S)
    if not m:
        raise SystemExit("make_j3cub: no Drag_basic - has the pinned model changed?")
    block = m.group(0)
    broadside = "1.4091"
    scale = float(broadside) / SECTION_BROADSIDE_CD

    def rows(sign):
        out = []
        for degrees, cd in REVERSED_FLOW_CD:
            radians = sign * (3.1416 if degrees == 180 else round(degrees * 3.141592653589793 / 180, 4))
            value = f"{cd * scale:.4f}"
            out.append(f"             {radians:7.4f}   {value}   {value}\n")
        return out if sign > 0 else out[::-1]

    new = replace_once(block, r"(\n)(             -1\.5700   " + broadside + "   " + broadside + r"\n)",
                       lambda mm: mm.group(1) + "".join(rows(-1)) + mm.group(2),
                       "the wing's drag at -90 degrees")
    new = replace_once(new, r"(              1\.5700   " + broadside + "   " + broadside + r"\n)",
                       lambda mm: mm.group(1) + "".join(rows(+1)), "the wing's drag at 90 degrees")
    return text[:m.start()] + new + text[m.end():]


def reversed_flow_radians(degrees, sign):
    """The angle of attack of a row past 90 degrees, as the drag table's."""
    return sign * (3.1416 if degrees == 180 else round(degrees * math.pi / 180, 4))


def reversed_flow_cm(degrees):
    """Her pitching moment coefficient about her quarter chord at an angle of
    attack past 90 degrees (positive), derived: the section's force normal to
    its chord, scaled as her drag and lift are, acting at its centre of
    pressure, half the chord back at 90 degrees moving straight to three
    quarters at 180 (the docstring says why)."""
    cl = dict(REVERSED_FLOW_CL)[degrees]
    cd = dict(REVERSED_FLOW_CD)[degrees]
    a = degrees * math.pi / 180
    normal = BROADSIDE_SCALE * (cl * math.cos(a) + cd * math.sin(a))
    centre = 0.5 + 0.25 * (degrees - 90) / 90
    return -(centre - 0.25) * normal


def with_reversed_flow_lift(text):
    """`text` with its wing's lift table carried on past 90 degrees of angle
    of attack to 180, either way, by Sheldahl and Klimas's section's curve,
    odd in the angle, scaled as its drag is. Within 90 degrees it is as it
    was."""
    m = re.search(r'<function name="aero/force/Lift_alpha">.*?</function>', text, re.S)
    if not m:
        raise SystemExit("make_j3cub: no Lift_alpha - has the pinned model changed?")
    block = m.group(0)

    def rows(sign):
        out = []
        for degrees, cl in REVERSED_FLOW_CL:
            value = f"{round(sign * cl * BROADSIDE_SCALE, 4) + 0.0:7.4f}"
            out.append(f"             {reversed_flow_radians(degrees, sign):7.4f}  {value}  {value}\n")
        return out if sign > 0 else out[::-1]

    new = replace_once(block, r"(\n)(             -1\.5700   0\.0000   0\.0000\n)",
                       lambda mm: mm.group(1) + "".join(rows(-1)) + mm.group(2),
                       "the wing's lift at -90 degrees")
    new = replace_once(new, r"(              1\.5700   0\.0000   0\.0000\n)",
                       lambda mm: mm.group(1) + "".join(rows(+1)), "the wing's lift at 90 degrees")
    return text[:m.start()] + new + text[m.end():]


def with_reversed_flow_pitch(text):
    """`text` with its Cm-alpha term held to 90 degrees of angle of attack
    either way and gone by 95, and a term for the wing in reversed flow from
    90 degrees to 180 either way (reversed_flow_cm), odd in the angle. Within
    90 degrees Cm-alpha is multiplied by exactly one, so it is as it was."""
    m = re.search(r'<function name="aero/moment/Pitch_alpha">.*?</function>\n', text, re.S)
    if not m:
        raise SystemExit("make_j3cub: no Pitch_alpha - has the pinned model changed?")
    block = m.group(0)
    edge = reversed_flow_radians(95, 1)
    new = replace_once(
        block, r"(\n)(           <property>aero/alpha-rad</property>\n)",
        lambda mm: (mm.group(1) + mm.group(2)
                    + "           <table>\n"
                    + "            <independentVar>aero/alpha-rad</independentVar>\n"
                    + "            <tableData>\n"
                    + f"             {-edge:7.4f}   0.0\n"
                    + "             -1.5700   1.0\n"
                    + "              1.5700   1.0\n"
                    + f"             {edge:7.4f}   0.0\n"
                    + "            </tableData>\n"
                    + "           </table>\n"),
        "Cm-alpha's angle of attack")
    rows = []
    for sign in (-1, 1):
        side = [f"             {reversed_flow_radians(d, sign):7.4f}  {round(sign * reversed_flow_cm(d), 4) + 0.0:7.4f}\n"
                for d, _ in REVERSED_FLOW_CL]
        rows += side[::-1] if sign < 0 else side
        if sign < 0:
            rows += ["             -1.5700   0.0000\n", "              1.5700   0.0000\n"]
    reversed_term = (
        "    <function name=\"aero/moment/Pitch_reversed_flow\">\n"
        "       <description>Pitch moment of the wing in reversed flow, past 90 degrees of alpha"
        " (glideslope: tools/make_j3cub.py)</description>\n"
        "       <product>\n"
        "           <property>aero/qbar-psf</property>\n"
        "           <property>metrics/Sw-sqft</property>\n"
        "           <property>metrics/cbarw-ft</property>\n"
        "           <table>\n"
        "            <independentVar>aero/alpha-rad</independentVar>\n"
        "            <tableData>\n"
        + "".join(rows) +
        "            </tableData>\n"
        "           </table>\n"
        "       </product>\n"
        "    </function>\n")
    return text[:m.start()] + new + "\n" + reversed_term + text[m.end():]


def airframe():
    text = (PINNED_CUB / "J3Cub.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n  <!-- glideslope: this is JSBSim's J3Cub with the changes listed in\n"
        r"       tools/make_j3cub.py, which made it. Do not edit it by hand. -->",
        "the file header")
    text = replace_once(text, r"<emptywt unit=\"LBS\" >\s*765\.00 </emptywt>",
                        f"<emptywt unit=\"LBS\" >    {EMPTY_WEIGHT_LBS}.00 </emptywt>",
                        "the empty weight")
    text = replace_once(
        text, r"   <pointmass name=\"Pilot\">.*?</pointmass>\n",
        pointmass("Pilot", station(36), -36.22)
        + pointmass("Passenger", station(9), -36.22)
        + pointmass("Baggage", station(49), -30.00),
        "the pilot's seat")
    text = replace_once(
        text,
        r"(<tank type=\"FUEL\" number=\"0\">\s*<location unit=\"IN\">\s*<x>)\s*0\.00\s*(</x>.*?"
        r"<capacity unit=\"LBS\">)\s*75\.00\s*(</capacity>\s*<contents unit=\"LBS\">)\s*65\.00\s*(</contents>)",
        rf"\g<1> {station(-18):.2f} \g<2> {FUEL_LBS}.00 \g<3> {FUEL_LBS}.00 \4", "the tank")
    text = replace_once(text, r"<engine file=\"Continental A-65-8\">", f"<engine file=\"{ENGINE}\">",
                        "the engine")
    text = replace_once(text, r"<thruster file=\"CM7445 MCCauley\">", f"<thruster file=\"{PROPELLER}\">",
                        "the propeller")
    text = replace_once(
        text, r"(\n    <function name=\"aero/force/Drag_gear\">)",
        "\n    <function name=\"aero/force/Drag_airframe\">\n"
        "       <description>Drag of the fuselage, tail, struts and wires at zero lift</description>\n"
        "         <product>\n"
        "           <property>aero/qbar-psf</property>\n"
        "           <property>metrics/Sw-sqft</property>\n"
        f"           <value> {AIRFRAME_DRAG} </value>\n"
        "         </product>\n"
        "    </function>\\1", "the gear's drag")
    text = replace_once(
        text, r"(<description>Induced drag</description>.*?<value>)\s*0\.0485\s*(</value>)",
        rf"\g<1> {INDUCED_DRAG} \2", "the induced drag")
    text = replace_once(text, r"<system file=\"Conventional Controls\.xml\"/>",
                        f"<system file=\"{SYSTEM}.xml\"/>", "the flight controls")
    text = replace_once(text, r"(<flight_control name=\"FCS: J3Cub\">\n)\n( </flight_control>)",
                        r"\1\2", "the empty flight controls")
    for side, unit in (("LEFT_MAIN", 1), ("RIGHT_MAIN", 2)):
        text = with_preloaded_main_gear(text, side, unit)
    text = with_reversed_flow_drag(text)
    text = with_reversed_flow_lift(text)
    text = with_reversed_flow_pitch(text)
    text = with_beam_drag_from_side_area(text)
    return with_float_carburettor(text, "make_j3cub")


def engine():
    text = (PINNED_CUB / "Engines" / "Continental A-65-8.xml").read_text()
    text = replace_once(text, r"(<piston_engine name=\"[^\"]*\">)",
                        r"\1\n  <!-- glideslope: JSBSim's J3Cub engine with the changes listed in\n"
                        r"       tools/make_j3cub.py, which made it. Do not edit it by hand. -->",
                        "the engine's name")
    text = replace_once(text, r"<maxrpm>\s*2800\.0 </maxrpm>", "<maxrpm>      2300.0 </maxrpm>",
                        "the engine's rpm")
    return with_best_power_mixture(text, "make_j3cub")


def propeller():
    text = (PINNED_CUB / "Engines" / "CM7445 MCCauley.xml").read_text()
    text = replace_once(text, r"(<propeller version=\"1\.01\" name=\"[^\"]*\">)",
                        r"\1\n  <!-- glideslope: JSBSim's J3Cub propeller with the changes listed in\n"
                        r"       tools/make_j3cub.py, which made it. Do not edit it by hand. -->",
                        "the propeller's name")
    text = replace_once(text, r"<diameter unit=\"IN\">\s*75\.0\s*</diameter>",
                        "<diameter unit=\"IN\"> 74.0 </diameter>", "the propeller's diameter")
    for name in ("C_THRUST", "C_POWER"):
        m = re.search(r'<table name="' + name + r'" type="internal">\s*<tableData>\s*\n(.*?)\n\s*</tableData>',
                      text, re.S)
        if not m:
            raise SystemExit(f"make_j3cub: no {name} table - has the pinned propeller changed?")
        rows = []
        for row in m.group(1).split("\n"):
            advance, value = (float(v) for v in row.split())
            scale = POWER_SCALE if name == "C_POWER" else THRUST_SCALE
            rows.append(f"      {advance * FINER_PITCH:.3f}   {value * scale:.4f}")
        text = text[:m.start(1)] + "\n".join(rows) + text[m.end(1):]
    return text


def controls():
    text = (PINNED_CUB / "Systems" / "Conventional Controls.xml").read_text()
    text = replace_once(text, r"(<system name=\"Conventional Controls\">)",
                        r"\1\n  <!-- glideslope: JSBSim's J3Cub flight controls with the changes listed\n"
                        r"       in tools/make_j3cub.py, which made them. Do not edit them by hand. -->",
                        "the flight controls' name")
    # The elevator's travel, in its control and in its normalisation - the
    # only surface of 0.14 radians.
    for element in ("range", "domain"):
        text = replace_once(
            text, r"(<" + element + r">\s*<min>)\s*-0\.14\s*(</min>\s*<max>)\s*0\.14\s*(</max>\s*</" + element + r">)",
            rf"\g<1> -{ELEVATOR_UP_RAD} \g<2> {ELEVATOR_DOWN_RAD} \3", f"the elevator's {element}")
    return text


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": alpha_rate.with_alpha_terms_on_qbar_uw(
            alpha_rate.with_qbar_uw(airframe(), "make_j3cub"), "make_j3cub",
            {"PITCH": ["aero/moment/Pitch_alpha", "aero/moment/Pitch_propwash",
                       "aero/moment/Pitch_reversed_flow"],
             "LIFT": ["aero/force/Lift_alpha", "aero/force/Lift_propwash"],
             "DRAG": ["aero/force/Drag_basic"]}),
        OUT / "aircraft" / MODEL / "Systems" / f"{SYSTEM}.xml": controls(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / f"{PROPELLER}.xml": propeller(),
    }


def main():
    check = "--check" in sys.argv[1:]
    stale = []
    for path, text in outputs().items():
        if check:
            if not path.exists() or path.read_text() != text:
                stale.append(path.relative_to(ROOT))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            print(f"wrote {path.relative_to(ROOT)}")
    if stale:
        print("assets/jsbsim/ is not what tools/make_j3cub.py makes:", file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print("run: python3 tools/make_j3cub.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
