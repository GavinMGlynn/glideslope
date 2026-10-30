#!/usr/bin/env python3
"""make_f35b.py - glideslope's Lockheed Martin F-35B Lightning II, written from what is published.

JSBSim has no F-35. This script writes one from what is public, which is
little: its size, weights and engine from Lockheed Martin's own documents;
its lift and drag across the Mach range as the fighters' are made
(tools/fighter.py), their numbers set to fly the published figures; and its
stability and control derivatives, which are not public, those of the nearest
fighter whose are - the F-4C, in NASA CR-2144. Its figures, in
assets/figures/f35b.xml, are its maximum Mach and its range; nothing more is
claimed.

**Her LiftSystem: the lift fan, the roll posts and the swivelling nozzle.**
The F-35B is a STOVL aeroplane, and in STOVL mode her engine's thrust is
shared between Rolls-Royce's shaft-driven LiftFan behind the cockpit, a roll
post in each wing and the three-bearing swivel module (3BSM) that turns the
engine's own jet down. Here the 3BSM is the engine's own thruster, vectored
from 0 to 95 degrees by JSBSim's thrust-vectoring property; the fan and the
two posts are JSBSim external forces, pointing up, each a fixed share of what
the engine's thruster passes, and its share taken from it: Rolls-Royce gives
the fan "more than 20,000 lbf", each post 1,950 lbf and the 3BSM 18,000 lbf,
41,900 lbf in all, and Pratt & Whitney gives the propulsion system a hover
thrust of 40,650 lbf, which is the total here, shared as Rolls-Royce shares
it. In STOVL mode the engine's throttle stops short of its afterburner -
Lockheed's own card says the vertical thrust is "without afterburner" - and
the power the fan's shaft takes from it is the engine's thrust its nozzle no
longer passes, which JSBSim's thrust reverser (a cosine on the thrust) stands
for. Converting to STOVL mode opens the fan and posts in CONVERSION_S
seconds; the nozzle swivels at Rolls-Royce's 95 degrees in 2.5 seconds.
Height is the throttle; pitch the fan's inlet vanes, which Lockheed says
modulate its thrust; roll the share of the fan air between the posts; yaw
the nozzle swivelled sideways, its thrust along its axis less by the cosine
of the swivel so that swivelling makes none. Above STOVL_LIMIT_KCAS the
simulation refuses STOVL mode. Where the fan, posts and nozzle are, how far
each control moves its thrust, and how long the conversion takes are not
published, and are estimated below, each with its number; nothing models
the ground's effect on a jet in a hover - no suckdown, fountain or hot gas
taken back into the inlet. The fan's vane box vectors its thrust aft for a
short take-off, as Lockheed's paper gives its arc. Two more are hers on the
short take-off's run: the thrust split, the engine's thrust moved between the
nozzle and the fan, the total unchanged, which pitches her (SPLIT_SHARE); and
her flaperons drooped as she converts (FLAPERON_DROOP_DEG). Her short
take-off is flown (assets/figures/f35b.xml), inside her requirement's 600 ft.

**Why the B and not the A.** FGAddon has no F-35A, so the A could have no
visual model and nothing to rest on with its wheels up. It has an F-35B, under
a verbatim GPL-3.0, whose model draws its undercarriage - which is what
tools/ground.py needs to measure an airframe. The project owner chose the
variant on 2026-09-22.

    python3 tools/make_f35b.py           write the files
    python3 tools/make_f35b.py --check   exit 1 if what is committed differs

Where each number comes from:

  Lockheed Martin, F-35B product card (C2023, 23-08442_002, PIRA AER2023060207)
    Length 51.2 ft; span 35 ft; wing area 460 sq ft; internal fuel 13,100 lb;
    the F135-PW-600 at 38,000 lb with afterburner and 26,000 lb without;
    Mach 1.6; range more than 900 nm on internal fuel; 7.0 g.
  Lockheed Martin, F-35 Lightning II Program Status and Fast Facts (April 2020,
  FG19-24749_004)
    Empty weight 32,300 lb and height 14.3 ft, which the product card does not
    give; maximum weight "60,000 lb class", which is the only maximum
    published.
  NASA CR-2144, Heffley and Jewell, Aircraft Handling Qualities Data (1972),
  section IV, the F-4C
    Every moment's derivative, from its power approach (table IV-1), non-
    dimensional and so carried to the F-35B's wing; its inertias at 38,925 lb
    (table IV-2) as radii of gyration, scaled by the root of the wings' areas.
  Rolls-Royce, "We put the vertical lift in the F-35B Lightning II" (the
  LiftSystem poster)
    The LiftFan "more than 20,000 lbf"; the roll posts "a further 1,950 lbf
    of thrust each"; the 3BSM "can rotate through 95 degrees in 2.5 seconds
    and passes 18,000 lbf of thrust".
  Pratt & Whitney, F135 Engine Fast Facts, Q4 2021 (S18016.10.21)
    The F135-PW-600's Hover Thrust Class, 40,650 lbs.
  Lockheed Martin, F-35B product card (2015, FG15-1280_002)
    "40,500 lb Vertical", "Vertical = without afterburner": the hover is
    flown without it.
  Wiegand et al., F-35 Air Vehicle Technology Overview (Lockheed Martin,
  AIAA 2018-3368)
    The fan's inlet guide vanes "provide thrust modulation"; its vane box
    directs its thrust "in an arc of 41.75-104 degrees ... at a rate of 40
    degrees per second"; the roll posts
    "produce about 10 percent of the vertical thrust" (here 9.3, Rolls-
    Royce's share); "Thrust split is defined as the ratio of main engine
    thrust over lift fan thrust, typically used to represent the propulsion
    system pitching moment applied to the aircraft", and "The 3BSM can
    vector up to 23,900 pounds of thrust at the maximum rearward thrust
    split" - the split's reach.
  Lockheed Martin, Code One, "F-35 Lightning II Flight Test Update 1" (2010)
    BF-1 converted to STOVL mode "at 200 knots", slowed to sixty and flew "a
    decelerating approach to a zero airspeed hover at 150 feet": the speed
    the transition law converts her at.
  F-35 Joint Program Office (Joe DellaVedova), "F-35B Lightning II
  completes first land-based ski jump launch", DVIDS, 24 June 2015
    "The F-35B's design allows it to automatically position the control
    surfaces and nozzles for takeoff": her flaperons drooped in STOVL mode.
  Raymer, Aircraft Design: A Conceptual Approach, DATCOM's plain flap
    The flaperons' droop's lift, at estimated effectiveness and span.
  United States Air Force, Aircraft Accident Investigation Board Report,
  F-35A T/N 12-005053, Eglin AFB, 19 May 2020 (released 30 September 2020)
    "the recommended landing attitude of 13 AOA" (page 21); her 202 KCAS
    touchdown "approximately 50 KCAS faster and 8 degrees AOA shallower than
    the on speed parameters for the MA gross weight" (page 6): the approach
    her lift is set to fly (LIFT_FACTOR).
  Lockheed Martin, Code One, "F-35 Lightning II Flight Testing At NAS
  Patuxent River" (1 December 2012), the test pilot Dan Levin
    "Landings in the A and B are similar to the F-16 in terms of speed and
    angle of attack": the A's approach taken for the B's.
  JSBSim's F100-PW-229, made the F135 as tools/make_f15c.py makes the F100-
  PW-220 (tools/fighter.py's thrust with speed and height, the F100's shape),
  its throttle ratios set to fly the maximum Mach.

**Two figures Lockheed publishes twice, differently, and which was taken.**
Internal fuel is 13,100 lb on the product card and 13,500 lb in the April 2020
Fast Facts. The product card is the later Lockheed publication and is the
B's own, so 13,100 lb is used. Thrust is 38,000/26,000 lb on the B's product
card and 40,000/25,000 lb in Fast Facts - but Fast Facts prints that same
40,000/25,000 for the A, the B and the C, labelled "uninstalled thrust
ratings", so it is a family figure and not the B's; the product card's is used.

**What is published nowhere, and so is not claimed: a service ceiling.**
Lockheed publishes none for any F-35, and the "above 50,000 feet" that the
F-35A model was held to is the Air Force's, for the A. The F-35B is held to
its maximum Mach and its range, and to no ceiling.

Estimated, as nothing published gives them, each named below with its number:
the mean aerodynamic chord and where the wing, the centre of gravity, the
tanks, the engine and the gear are; the gear's springs; the lift's curve past
its straight line; the drag's numbers; the fuel its engine burns; the
surfaces' travel; how far the flaperons droop in STOVL mode and how
far forward the thrust split goes; and the pitch and yaw dampers its fly-by-wire flight
controls stand for. The airframe's own contacts, which it rests on with its
wheels up, are measured from its visual model by tools/ground.py.

**The alpha-rate terms on the dynamic pressure without its sideways part**
(tools/alpha_rate.py, JSBSim issue #89): braked to a stop in a crosswind, her
angle of attack is undefined, its rate swings, and multiplied by the whole
dynamic pressure it pitched her by tens of degrees a second.

**Her pitch terms keyed to the angle of attack on the dynamic pressure without
its sideways part** (alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15):
Cmalpha. Standing or slowing in a crosswind her angle of attack, atan2(w, u),
is any angle at all - 74 to 103 degrees standing on her brakes in 15 kt across
- and on the whole dynamic pressure, which counts the crosswind, it pitched her
nose down by up to 940 ft-lb; on aero/qbarUW-psf, nothing. Her wing is swept,
25 degrees at the quarter chord (SWEEP_QUARTER_CHORD), where issue #89 did not
look; simple sweep theory (NACA Report 863; alpha_rate.py's docstring) puts the
part of a beam wind's moment that goes with the angle of attack near nothing
standing, as qbarUW does, where on qbar it is as large as the undefined angle
makes it. Standing, qbarUW is the nearer; in flight, at a few degrees of
sideslip, the two differ by its cosine squared, and the theory's pressure lies
between them, under 1% from either (alpha_rate.py).

**Her lift keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15): CLalpha. Standing on her
brakes in 15 kt straight across, her angle of attack, atan2(w, u), was -15 to
110 degrees, and on the whole dynamic pressure, which counts the crosswind, the
term lifted her by up to 470 lb either way; on aero/qbarUW-psf, nothing. JSBSim
issue #89's guidance, written into the FlightGear wiki's "JSBSim Aerodynamics":
QBarUW "is more appropriate for CL than the generic QBar" - a wing's lift is
made by the air in the plane of u and w. Her wing is swept, 25 degrees at the
quarter chord: the sweep argument above (simple sweep theory, NACA Report 863)
holds for the lift of the same wing as for its moment - standing, qbarUW is the
nearer; in flight, at a few degrees of sideslip, the two are under 1% apart.
Her lift terms not keyed to the angle of attack (CLde and CLdroop) were not
this change's: they are moved below (2026-10-23).

**Her wing's drag keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-16): CDseparated. CDseparated,
her drag past the flow separating, is a product of her lift coefficient and the
angle of attack; standing in 15 kt across it was under 0.1 lb on either
pressure, and it is moved as the same wing's drag keyed to the same angle, for
slowing and flight. The same reasoning as her lift's, above: a wing's drag at
its angle of attack is made by the air in the plane of u and w, and the
sideways part runs along its span - the drag that rolled the J-3 Cub onto her
wing standing in a beam wind (make_j3cub.py). In flight, at a few degrees of
sideslip, the two differ by its cosine squared. Her wing is swept: moved on the
same argument as her lift, simple sweep theory
(alpha_rate.with_alpha_terms_on_qbar_uw).

**Her Cm-alpha the same both ways round** (pitch_alpha_table, 2026-10-20): as
the F-4C's slope times the angle of attack, it jumped where flying backward
puts her angle - at 180 degrees, where atan2(w, u) goes from -180 to 180 -
from 0.098 times pi of nose-up moment to as much nose-down: 5,700 ft-lb either
way at 30 kt backward, past what the fan's share answers, and her hover law,
the fan saturated at each crossing, swung her 19 degrees in pitch and her
speed between 25 and 32 kt. An aeroplane met by the air from behind feels
the same air whichever side of 180 the angle is reckoned: the moment is a
function of where the air comes from, not of how its angle is written. So the
slope holds to 90 degrees either way, as before, and past it falls straight to
nothing at 180 - flown tail first, the slope reversed, as a weathercock blown
from behind turns. Nothing published gives her moment past 90 degrees; the
shape is this script's. Within 90 degrees it is the slope as it was.

**Her lift the same both ways round** (lift_table, 2026-10-20): the table ran
from -0.60 to 1.57 rad, and JSBSim holds a table's end value past its ends, so
past 90 degrees her lift was nothing on one side and, from -0.60 down, the
whole -0.30 rad of negative lift on the other: flown backward at 30 kt, where
atan2(w, u) crosses from 180 to -180 with each small pitch swing, it stepped
by about 1,300 lb and her height law took the step up. As her moment, above:
the air from behind is the same whichever side of 180 its angle is written.
So the table runs all the way round, the same from -90 to 90 degrees as JSBSim
read it before (from -0.60 to -90 the -0.60 value held, as it was held past
the end), straight from there to nothing at -180, and nothing from 90 to 180,
as before: continuous through 180. Made symmetric instead - straight to
nothing at -90, as the positive side falls at 90 - it moved her full-forward
dive at 300 KCAS, which passes -0.60, and a prediction's distance from the
server after a wreck from 4.2 to 9.7 cm; this way flight inside 90 degrees
is untouched. **Lopsided, and unsourced**: nothing published gives her lift
in reversed air, and this shape - nothing from 90 to 180, a straight fall from
-90 to -180 - is this script's, chosen (the owner's decision, 2026-10-21) so
that her flight inside 90 degrees either way, and her full-forward dive, are
what they were; a wing met from its trailing edge makes some lift, and the
same way either side.

**Her lift terms not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-23, with_alpha_terms_on_qbar_uw for a lift with pitch rate whose
coefficient the angle picks): CLde and CLdroop. On the whole dynamic pressure
they counted the crosswind standing. Standing on her brakes with her engines
stopped in 15 kt straight across, a lifting control set one at a time, they
lifted her by up to 37 lb (stick back); now under 0.1 lb at every setting (the
standing test in tests/unit/test_departure.cpp). JSBSim issue #89's guidance
puts the whole of CL on QBarUW; alpha_rate.py says why, for swept surfaces too.
In flight, at a few degrees of sideslip, the two differ by its cosine squared.

**Her pitching moments not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-28): Cmq, Cmde and Cmdroop. On the whole dynamic pressure they counted
the crosswind standing, as her lift did. Standing on her brakes with her engines
stopped in 15 kt straight across, a control set one at a time, they pitched her
by up to 753 ft-lb (stick forward); now by 0.00 ft-lb at most (the standing test
in tests/unit/test_departure.cpp). The FlightGear wiki's guidance from JSBSim
issue #89 puts Cm on QBarUW; alpha_rate.py says why, for swept surfaces too. In
flight, at a few degrees of sideslip, the two differ by its cosine squared.

**Her side force out of her plane of symmetry standing in a beam wind**
(side_force.with_side_force_tapered, 2026-10-29). JSBSim makes it along its
wind axes' y, which at 90 degrees of sideslip lies in her plane of symmetry at
an undefined angle of attack: standing in 15 kt across, it pushed her fore and
aft or up and down and pitched her about her centre of gravity. Every side
term (CYb and her rudder's, ailerons', roll and yaw rates') is now tapered by
cos^2 beta: kept whole at small sideslip, 0.992 at 5 degrees and 0.933 to
0.883 at a crosswind slip's 15 to 20 (a little easier to hold, within what a
linear slope knows there, her fin stalling), none broadside, where her drag
with sideslip pushes her. The taper is chosen, not a crossflow's share.
Spins and departures are not examined. side_force.py says why.
"""

import math
import re
import sys

import alpha_rate
import side_force
import airliner
import fighter
import ground
import make_f15c
import written
from airliner import OUT, PINNED

SCRIPT = "make_f35b"
MODEL = "f35b"
ENGINE = "F135-PW-600"

SPAN_FT = 35.0
AREA = 460.0
ASPECT = SPAN_FT ** 2 / AREA
MAC_FT = 1.16 * AREA / SPAN_FT     # a fighter's mean aerodynamic chord over its mean chord
SWEEP_QUARTER_CHORD = 25.0
LEMAC_IN = 300.0                   # the chord's leading edge, inches behind the nose


def mac(fraction):
    return LEMAC_IN + fraction * MAC_FT * 12.0


EMPTY_LBS = 32300.0
FUEL_LBS = 13100.0
# The F-4C's radii of gyration at 38,925 lb (CR-2144 table IV-2), scaled.
F4C_WEIGHT, F4C_AREA = 38925.0, 530.0
F4C_INERTIA = (25002.0, 122193.0, 139767.0, 2177.0)

MILITARY_THRUST = "26000.0"
MAXIMUM_THRUST = "38000.0"
MILITARY_THROTTLE_RATIO = 1.10
MAXIMUM_THROTTLE_RATIO = 1.10
# Its fuel a pound of thrust an hour, dry: not published; a low-bypass
# turbofan's, estimated.
TSFC = 0.90

# Lift: DATCOM's slope for its wing, straight to STRAIGHT_ALPHA, then rounding
# to its greatest at STALL_ALPHA; drag (fighter.drag_functions), the numbers
# set to fly the figures, the shape past Mach 1.2 the F-15C's.
#
# **WAVE_DRAG is what makes this aeroplane the B and not the A.** The F-35A
# model reached its published Mach 1.6 on 43,000 lb of thrust at 0.066. The
# F-35B has 38,000 lb and is 3,000 lb heavier empty, and at 0.066 it reached
# only Mach 1.35. The wave drag is the number set to fly the figure, as this
# script's docstring says of the drag: measured at 0.060 it gives Mach 1.48,
# at 0.056 Mach 1.56, at 0.052 Mach 1.64, and at 0.054 Mach 1.61 against the
# published 1.6. The range figure does not move with it - the range flight is
# level at Mach 0.8, where there is no wave drag - so the two are set
# independently.
STRAIGHT_ALPHA = 0.30
STALL_ALPHA = 0.55
# **Her lift over her wing's** (2026-10-30, "The F-35B lands on her power"):
# DATCOM's slope is the trapezoidal wing's alone, and the F-35's wide lifting
# body, chines and leading-edge root, which it leaves out, are not published.
# On the wing's alone her model flew the glidepath at 19.5 degrees and 159
# knots, past the 13 the F-35A is landed at (the Air Force's board on the
# F-35A lost at Eglin on 19 May 2020: "the recommended landing attitude of 13
# AOA", on speed about 50 KCAS under the 202 she touched at, so about 152;
# its report, pages 21 and 6), which Lockheed's test pilot Dan Levin says the
# B lands like ("Landings in the A and B are similar to the F-16 in terms of
# speed and angle of attack", Code One, 1 December 2012). **The factor is
# fitted to the accident report's figure**, as the drag's numbers are fitted
# to the published Mach: 13 degrees at 152 KCAS at her combat loading
# (assets/figures/f35b.xml's approach). It is not derived, and the 13 degrees
# is its input, not a check of it. **What was looked for, to derive it
# instead**: Raymer's wing-body method (Aircraft Design, 12.4.1) multiplies
# DATCOM's slope by the exposed wing's share of the reference area and a
# fuselage lift factor, 1.07 (1 + d/b)^2, and holds their product under 1 -
# so on its own it cannot raise her lift at all, and the rest would be the
# chines' and the wing root's vortex lift, for which no number for the F-35,
# or a chined forebody like hers, was found. Lockheed's own lift curve
# (Parsons, Eckstein and Azevedo, "F-35 Aerodynamic Performance
# Verification", AIAA 2018-3679, figure 11) is drawn without numbers on its
# lift axis; and no F-35 stall or minimum speed is published - she is flown
# to 50 degrees of angle of attack and has no stall in the usual sense - so
# her measured 97 KCAS stall has nothing to be checked against. It scales the
# whole table, every Mach, so her stall, take-offs and climb move with it
# and are measured again; her drag is on the lift's square, so at a given
# lift - her cruise, her top speed - it moves nothing.
LIFT_FACTOR = 1.6
SUBSONIC_ZERO_LIFT_DRAG = 0.020
WAVE_DRAG = 0.054
SPAN_EFFICIENCY = 0.70
SEPARATION_ALPHA = 0.35

# The F-4C's derivatives, power approach (CR-2144 table IV-1), per radian.
CM_ALPHA, CM_ADOT, CM_Q, CM_DE, CL_DE = -0.098, -0.95, -2.0, -0.322, 0.24
CY_BETA, CN_BETA, CL_BETA = -0.655, 0.199, -0.156
CL_P, CN_P, CL_R, CN_R = -0.272, -0.013, 0.205, -0.320
CY_DA, CN_DA, CL_DA = -0.0355, -0.0041, 0.057
CY_DR, CN_DR, CL_DR = 0.124, -0.072, -0.0009
# The flight controls: stabilator, flaperons and rudders' travel, and the
# pitch and yaw dampers - the fraction of full travel per radian a second.
STABILATOR_DEG, FLAPERON_DEG, RUDDER_DEG = 25.0, 25.0, 30.0
PITCH_DAMPER_GAIN, YAW_DAMPER_GAIN = 1.0, 1.0


# **Her LiftSystem** (the docstring). Rolls-Royce's thrusts, shared out of
# Pratt & Whitney's hover thrust.
HOVER_THRUST_LBF = 40650.0
LIFT_FAN_LBF, ROLL_POST_LBF, NOZZLE_LBF = 20000.0, 1950.0, 18000.0
LIFT_SYSTEM_LBF = LIFT_FAN_LBF + 2.0 * ROLL_POST_LBF + NOZZLE_LBF
NOZZLE_MAX_DEG = 95.0
NOZZLE_RATE_DEG_S = 95.0 / 2.5
# The fan's vane box (Wiegand et al.): its thrust "directed in an arc of
# 41.75-104 degrees", measured here from straight aft, "at a rate of 40
# degrees per second"; 90 is straight down.
FAN_MIN_DEG, FAN_MAX_DEG, FAN_RATE_DEG_S = 41.75, 104.0, 40.0
# The throttle in STOVL mode, short of the afterburner: JSBSim's turbine
# lights it above 0.99.
STOVL_THROTTLE = 0.99
# **The fastest she converts at, or flies converted**: the LiftFan "must
# operate in STOVL conditions which result in a 288 mph crosswind to the
# LiftFan intake" (Rolls-Royce's poster) - 250 kt, taken as her airspeed
# with the fan's doors open. Above it Aircraft::set_lift_system refuses.
STOVL_LIMIT_KCAS = 288.0 / 1.150779
# **The speed she converts at, either way** (sim/transition.hpp): Lockheed
# Martin's Code One, "F-35 Lightning II Flight Test Update 1" (2010): BF-1's
# lead STOVL pilot "initiates conversion to STOVL mode at 200 knots", on her
# first hover and her first vertical landing, and her third STOVL flight
# flew "airborne conversions at 200 knots and 5,000 feet". The speed she
# converts back at is not given - on her first STOVL flight she slowed to 180
# knots "before accelerating and converting back to conventional flight
# mode" - and is taken as the same.
STOVL_CONVERT_KCAS = 200.0
# **How the transition law flies her between them** (sim/transition.hpp),
# estimated, as nothing published gives them: how fast she slows to a hover
# and gathers speed out of one jet-borne, knots a second - 2 takes her from
# 200 knots to a hover in 100 s, Code One giving the sequence and the speeds
# but not the time; the attitude she gathers speed toward by her conversion
# speed, low so that her jets carry more of her and the throttle that holds
# her height gives her thrust enough (at 8 degrees she stuck at 195 KCAS, at
# 11.5 at 166); and the least height above the ground she converts at either
# way. Code One's conversions were flown at 5,000 ft, and at 200 knots before
# a decelerating approach to a hover at 150 ft; no least height is published.
# 100 ft is ten times the most the transitions measured here may sink (10 ft,
# their tests' bound) and five times what the conversion balloons her (about
# 20 ft): her conversion's height excursions are kept well clear of the
# ground.
TRANSITION_SLOW_KT_S = 2.0
TRANSITION_GATHER_KT_S = 3.0
TRANSITION_GATHERING_PITCH_DEG = 4.0
STOVL_CONVERT_LEAST_FT = 100.0
# **Her unified control laws' inceptors in STOVL mode** (sim/transition.hpp,
# Transition::Inceptors). Walker, Fuller and Wurth (AIAA 2013-4243, its
# abstract) give the left-hand inceptor longitudinal acceleration; Graham
# Tomlinson ("Test Flying the Joint Strike Fighter", 2011) "the throttle
# commands acceleration and deceleration", and his and Guy Norris's
# (Aviation Week, 3 October 2011) translational rate command "in the hover
# allows the pilot to make small positional corrections ... and will then
# bring the aircraft to a standstill if the pilot releases the controls";
# under it "the aircraft's speed over the ground is proportional to control
# displacement" (Elan Head, The Air Current, 14 March 2024). John Farley,
# of the VAAC Harrier her laws were flown on first ("A V/STOL Flight Control
# Journey", c. 2006): "either side of the aft detent gives the pilot a closed
# loop control of ground speed up to 30 kt forwards or backwards ... the
# rudder pedals command yaw rate"; and Chris Kjelgaard (2012) has the F-35B
# in STOVL mode fly backwards at 30 knots. So: the lever's detent a quarter
# of its travel from its aft stop (estimated: "aft" is all Farley gives),
# 30 kt of ground speed either way of it, the stick's sideways a ground speed
# to 20 kt (estimated: a forum's quotation of the VAAC's full lateral input,
# not traced to its source) and the pedals a turn to 10 degrees a second
# (estimated: none is published).
STOVL_LEVER_DETENT = 0.25
# **The most she is flown into a hover at**, lb: her hover thrust less 2 per
# cent for the lift system's control (estimated: none is published), so
# 39,837 lb - over her combat loading's 39,750, at which she hovers. Heavier,
# the transition law holds her wing-borne at her conversion speed in STOVL
# mode: at her plan weight, 45,400 lb, slowed to a hover she sank at 4,369
# ft/min into the ground (PROJECT_STATUS, 2026-10-14).
STOVL_HOVER_MOST_LBS = HOVER_THRUST_LBF * 0.98
STOVL_TRC_KT = 30.0
# **Backward, 30 kt**, as Farley and Kjelgaard (above) give. Held at 20 from
# 2026-10-14 while the model swung 10 degrees in pitch flown back at 30; the
# swing was her Cm-alpha's jump at 180 degrees (pitch_alpha_table), and with
# it gone she holds 30 back (docs/status/2026-10-20-f35b-backward-hover-30kt.md).
STOVL_TRC_AFT_KT = 30.0
STOVL_TRC_SIDE_KT = 20.0
STOVL_TURN_DEG_S = 10.0
# Her wing's lift slope at her conversion speed (about Mach 0.3 at sea
# level), a radian, the slope her lift table is written with (lift_table):
# from which the transition law reckons the attitude she flies wing-borne at
# her conversion speed for what she weighs.
TRANSITION_MACH = 0.3
# Estimated, as nothing published gives them: the fan's centre, behind the
# cockpit; the posts, as far out in the wings as their structure allows;
# the time the doors take to open and the fan to clutch in; how far the fan's
# vanes move its thrust either way of its share; how much of one post's air
# can be sent to the other; and how far the nozzle swivels sideways.
LIFT_FAN_X_IN = 200.0
ROLL_POST_Y_IN = 115.0
CONVERSION_S = 8.0
FAN_MODULATION = 0.10
POST_SHIFT = 0.8
NOZZLE_YAW_DEG = 12.0

# **The thrust split, her pitch control on the short take-off's run.**
# Wiegand et al.: "Thrust split is defined as the ratio of main engine thrust
# over lift fan thrust, typically used to represent the propulsion system
# pitching moment applied to the aircraft", and "The 3BSM can vector up to
# 23,900 pounds of thrust at the maximum rearward thrust split" - against
# Rolls-Royce's 18,000 at the hover's, so the split moves up to 5,900 lbf,
# 0.328 of the nozzle's share, from the fan to the nozzle. How far forward
# it goes is not published; the same, estimated. The total does not change:
# what the nozzle gives up the fan gains (lift_system).
SPLIT_SHARE = (23900.0 - NOZZLE_LBF) / NOZZLE_LBF

# **The flaperons drooped in STOVL mode.** Her flight controls set her
# surfaces and nozzles for a short take-off themselves (the F-35 Joint
# Program Office's release on her first ski-jump launch, 24 June 2015: "The
# F-35B's design allows it to automatically position the control surfaces
# and nozzles for takeoff"). How far the
# flaperons droop is not published: an estimate, short of their 25 degrees
# of travel so that they still roll her. Their lift is DATCOM's plain flap's
# (Raymer, Aircraft Design, the flap's lift increment): the wing's slope,
# the flap's effectiveness at a quarter of the chord deflected 20 degrees
# (0.45 in theory, 0.7 of it past 15 degrees), and the share of the wing
# behind the flaperons, estimated; its centre of pressure half the chord
# back, a quarter behind the aerodynamic centre; and its drag DATCOM's
# plain flap's at that deflection, estimated.
FLAPERON_DROOP_DEG = 20.0
FLAP_EFFECTIVENESS = 0.45 * 0.7
FLAPPED_WING_SHARE = 0.5
FLAP_DRAG = 0.015


def metrics():
    return ("    <metrics>\n"
            f"        <wingarea unit=\"FT2\"> {AREA:.1f} </wingarea>\n"
            f"        <wingspan unit=\"FT\"> {SPAN_FT:.2f} </wingspan>\n"
            f"        <chord unit=\"FT\"> {MAC_FT:.2f} </chord>\n"
            + written.location(mac(0.25), 0.0, 0.0, "        ", "AERORP")
            + written.location(110.0, 0.0, 30.0, "        ", "EYEPOINT")
            + written.location(0.0, 0.0, 0.0, "        ", "VRP") +
            "    </metrics>\n")


def mass_balance():
    scale = math.sqrt(AREA / F4C_AREA)
    f4c_slugs = F4C_WEIGHT / 32.174
    radii = [math.sqrt(i / f4c_slugs) * scale for i in F4C_INERTIA[:3]]
    slugs = EMPTY_LBS / 32.174
    ixx, iyy, izz = (slugs * k * k for k in radii)
    return ("    <mass_balance>\n"
            f"        <ixx unit=\"SLUG*FT2\"> {ixx:.0f} </ixx>\n"
            f"        <iyy unit=\"SLUG*FT2\"> {iyy:.0f} </iyy>\n"
            f"        <izz unit=\"SLUG*FT2\"> {izz:.0f} </izz>\n"
            f"        <ixz unit=\"SLUG*FT2\"> {izz * F4C_INERTIA[3] / F4C_INERTIA[2]:.0f} </ixz>\n"
            f"        <emptywt unit=\"LBS\"> {EMPTY_LBS:.0f} </emptywt>\n"
            + written.location(mac(0.25), 0.0, 0.0, "        ", "CG") +
            "        <pointmass name=\"Pilot\">\n"
            "            <weight unit=\"LBS\"> 0 </weight>\n"
            + written.location(140.0, 0.0, 25.0, "            ") +
            "        </pointmass>\n"
            "        <pointmass name=\"Weapons\">\n"
            "            <weight unit=\"LBS\"> 0 </weight>\n"
            + written.location(mac(0.25), 0.0, -20.0, "            ") +
            "        </pointmass>\n"
            "    </mass_balance>\n")


# **The main wheels 15 degrees behind the centre of gravity, seen from the
# ground** - Raymer's tipback angle (Aircraft Design: A Conceptual Approach),
# the least that keeps a tail-heavy aeroplane off its tail, and so the most
# weight that is left on the nose wheel. They were an estimated 30 in behind
# it, 23 degrees, which put so much weight on the nose wheel that the
# stabilator could not lift it until 183 knots with the stick fully back,
# against a rotation speed of 141. The F-15C's flight manual is the check on
# the rule: the same 15 degrees brings its nose wheel off within 7 knots of
# the manual's (tools/make_f15c.py). **Nothing checks it on the F-35B
# itself**: no F-35 figure gives where its wheels are or when its nose wheel
# comes off, so this is the rule, borrowed, and not a measurement.
TIP_BACK_DEG = 15.0
WHEEL_Z = -70.0                    # the wheels' contact, below the centre of gravity


def ground_reactions():
    weight = 60000.0  # Fast Facts: the B is "60,000 lb class"
    main_x = mac(0.25) - WHEEL_Z * math.tan(math.radians(TIP_BACK_DEG))
    nose_x = 150.0
    nose_share = (main_x - mac(0.25)) / (main_x - nose_x)
    out = "    <ground_reactions>\n"
    out += written.bogey("NOSE", nose_x, 0.0, WHEEL_Z, nose_share * weight / 0.5, nose_share * weight / 2.0,
                         5, "NONE", "0.80")
    for side, sign in (("LEFT", -1.0), ("RIGHT", 1.0)):
        out += written.bogey(f"{side}_MAIN", main_x, sign * 70.0, WHEEL_Z, (1.0 - nose_share) / 2.0 * weight / 0.5,
                             (1.0 - nose_share) / 2.0 * weight / 2.0, 0, side, "0.50")
    # The airframe's own contacts, so that it has something to land on with
    # its wheels up; measured from its visual model by tools/ground.py.
    out += ground.contacts(MODEL, weight, wheel_z=WHEEL_Z)
    # The tail strike, measured from her visual model by tools/ground.py.
    return ground.with_tail_strike(out + "    </ground_reactions>\n", MODEL, weight, script=SCRIPT)


def propulsion():
    return ("    <propulsion>\n"
            f"        <engine file=\"{ENGINE}\">\n"
            "            <feed>0</feed>\n"
            "            <feed>1</feed>\n"
            "            <thruster file=\"direct\">\n"
            + written.location(nozzle_x(), 0.0, 0.0, "                ") +
            "            </thruster>\n"
            "        </engine>\n"
            + "".join("        <tank type=\"FUEL\">\n"
                      + written.location(mac(0.25), y, 0.0, "            ") +
                      f"            <capacity unit=\"LBS\"> {FUEL_LBS / 2.0:.0f} </capacity>\n"
                      f"            <contents unit=\"LBS\"> {FUEL_LBS / 2.0:.0f} </contents>\n"
                      "        </tank>\n" for y in (-40.0, 40.0)) +
            "    </propulsion>\n")


def flight_control():
    rad = math.radians
    out = "    <flight_control name=\"F-35B\">\n        <channel name=\"Controls\">\n"
    out += ("            <pure_gain name=\"Pitch Damper\">\n"
            "                <input>velocities/q-aero-rad_sec</input>\n"
            f"                <gain>{PITCH_DAMPER_GAIN}</gain>\n"
            "                <clipto><min>-0.3</min><max>0.3</max></clipto>\n"
            "            </pure_gain>\n")
    out += written.surface("Elevator", ["fcs/elevator-cmd-norm", "fcs/pitch-trim-cmd-norm", "fcs/pitch-damper"],
                           -rad(STABILATOR_DEG), rad(STABILATOR_DEG), "fcs/elevator-pos-rad")
    out += written.surface("Aileron", ["fcs/aileron-cmd-norm", "fcs/roll-trim-cmd-norm"], -rad(FLAPERON_DEG),
                           rad(FLAPERON_DEG), "fcs/left-aileron-pos-rad")
    out += ("            <pure_gain name=\"Right Aileron\">\n"
            "                <input>-fcs/left-aileron-pos-rad</input>\n"
            "                <gain>1</gain>\n"
            "                <output>fcs/right-aileron-pos-rad</output>\n"
            "            </pure_gain>\n")
    out += written.yaw_damper(YAW_DAMPER_GAIN)
    out += written.surface("Rudder", ["fcs/rudder-cmd-norm", "fcs/yaw-trim-cmd-norm", "fcs/yaw-damper"],
                           -rad(RUDDER_DEG), rad(RUDDER_DEG), "fcs/rudder-pos-rad")
    out += written.kinematic("Gear", "gear/gear-cmd-norm", [0, 1], [0, 5], "gear/gear-pos-norm")
    return out + "        </channel>\n" + lift_system() + "    </flight_control>\n"


def cg_x():
    """The empty aeroplane's centre of gravity, where the posts are and what
    the fan and the nozzle balance about."""
    return mac(0.25)


def nozzle_x():
    """**The 3BSM's exit, where the fan's share balances it** about the
    centre of gravity, the one place that leaves her hovering with her pitch
    control at its middle. Not published."""
    return cg_x() + LIFT_FAN_LBF * (cg_x() - LIFT_FAN_X_IN) / NOZZLE_LBF


def stovl_thrust_lbf():
    """The engine's thrust at sea level, standing, at STOVL_THROTTLE, as
    JSBSim's turbine gives it: idle + (military - idle) x N2 squared, the
    military table being 1 there (fighter.thrust_table)."""
    idle = fighter.table((PINNED / "engine" / "F100-PW-229.xml").read_text(), "IdleThrust")(0.0, 0.0)
    return float(MILITARY_THRUST) * (idle + (1.0 - idle) * STOVL_THROTTLE ** 2)


def nozzle_share():
    """What the nozzle passes in a hover as a fraction of the engine's thrust
    at STOVL_THROTTLE: its share of the hover thrust, over that thrust."""
    return HOVER_THRUST_LBF * NOZZLE_LBF / LIFT_SYSTEM_LBF / stovl_thrust_lbf()


def external_reactions():
    """The lift fan and the roll posts, pointing up, and the nozzle's sideways
    part when it swivels; the LiftSystem channel sets their magnitudes."""
    def force(name, x, y, direction):
        dx, dy, dz = direction
        return (f"        <force name=\"{name}\" frame=\"BODY\">\n"
                + written.location(x, y, 0.0, "            ") +
                "            <direction>\n"
                f"                <x> {dx} </x>\n                <y> {dy} </y>\n                <z> {dz} </z>\n"
                "            </direction>\n"
                "        </force>\n")
    return ("    <external_reactions>\n"
            + force("lift-fan", LIFT_FAN_X_IN, 0.0, (0, 0, -1))
            + force("left-roll-post", cg_x(), -ROLL_POST_Y_IN, (0, 0, -1))
            + force("right-roll-post", cg_x(), ROLL_POST_Y_IN, (0, 0, -1))
            + force("nozzle-side", nozzle_x(), 0.0, (0, -1, 0))
            + "    </external_reactions>\n")


def lift_system():
    """The LiftSystem channel. `fcs/stovl-cmd-norm` 1 converts to STOVL mode
    and 0 back; `fcs/nozzle-cmd-deg` swivels the nozzle down;
    `fcs/lift-fan-cmd-norm`, -1 to 1, moves the fan's thrust either side of
    its share (pitch, 1 nose up: the fan is ahead of the centre of gravity); `fcs/roll-post-cmd-norm` sends the posts'
    air to one side (roll, 1 right wing down); `fcs/nozzle-yaw-cmd-norm`
    swivels the nozzle sideways (yaw, 1 nose right). In conventional flight
    every one of them is nothing: the forces are zero and the nozzle is
    straight."""
    i = "            "
    thrust = "propulsion/engine/thrust-lbs"

    def fn(name, body, output):
        return (f"{i}<fcs_function name=\"{name}\">\n{i}    <function>\n{body}"
                f"{i}    </function>\n{i}    <output>{output}</output>\n{i}</fcs_function>\n")

    base = "fcs/lift-system-base-lbf"

    def share(name, lbf, gain, control, output):
        return fn(name, f"{i}        <product>\n{i}            <property>{base}</property>\n"
                        f"{i}            <property>fcs/stovl-pos-norm</property>\n"
                        f"{i}            <value>{lbf / NOZZLE_LBF:.6f}</value>\n"
                        f"{i}            <sum><value>1</value><product><value>{gain}</value>"
                        f"<property>{control}</property></product></sum>\n{i}        </product>\n", output)

    out = "".join(f"        <property value=\"0\">fcs/{name}</property>\n" for name in
                  ("stovl-cmd-norm", "nozzle-cmd-deg", "lift-fan-cmd-norm", "roll-post-cmd-norm",
                   "nozzle-yaw-cmd-norm", "thrust-split-cmd-norm"))
    out += "        <property value=\"90\">fcs/fan-cmd-deg</property>\n"
    # What the simulation reads of her lift system: the throttle's top in
    # STOVL mode (sim/hover.cpp), and the fastest she may convert at
    # (Aircraft::set_lift_system).
    out += f"        <property value=\"{STOVL_THROTTLE}\">fcs/stovl-throttle-max</property>\n"
    out += f"        <property value=\"{STOVL_LIMIT_KCAS:.0f}\">fcs/stovl-limit-kcas</property>\n"
    # And the speed the transition law converts her at (sim/transition.hpp).
    out += f"        <property value=\"{STOVL_CONVERT_KCAS:.0f}\">fcs/stovl-convert-kcas</property>\n"
    # And the rest of what it reads of her: the nozzle's and the fan's vane
    # box's travel, its estimated rates, attitude and least height, and her
    # lift slope at her conversion speed, her body's with her wing's
    # (LIFT_FACTOR).
    for name, value in (("nozzle-max-deg", f"{NOZZLE_MAX_DEG:.1f}"), ("fan-min-deg", f"{FAN_MIN_DEG:.2f}"),
                        ("fan-max-deg", f"{FAN_MAX_DEG:.1f}"),
                        ("transition-slow-kt-per-s", f"{TRANSITION_SLOW_KT_S:.1f}"),
                        ("transition-gather-kt-per-s", f"{TRANSITION_GATHER_KT_S:.1f}"),
                        ("transition-gathering-pitch-deg", f"{TRANSITION_GATHERING_PITCH_DEG:.1f}"),
                        ("stovl-convert-least-ft", f"{STOVL_CONVERT_LEAST_FT:.0f}"),
                        ("stovl-lever-detent", f"{STOVL_LEVER_DETENT:.2f}"),
                        ("stovl-hover-most-lbs", f"{STOVL_HOVER_MOST_LBS:.0f}"),
                        ("stovl-trc-kt", f"{STOVL_TRC_KT:.1f}"),
                        ("stovl-trc-aft-kt", f"{STOVL_TRC_AFT_KT:.1f}"),
                        ("stovl-trc-side-kt", f"{STOVL_TRC_SIDE_KT:.1f}"),
                        ("stovl-turn-degps", f"{STOVL_TURN_DEG_S:.1f}"),
                        ("transition-lift-slope-per-rad",
                         f"{LIFT_FACTOR * fighter.lift_slope(TRANSITION_MACH, ASPECT, SWEEP_QUARTER_CHORD):.4f}")):
        out += f"        <property value=\"{value}\">fcs/{name}</property>\n"
    out += "        <channel name=\"LiftSystem\">\n"
    out += written.kinematic("Conversion", "fcs/stovl-cmd-norm", [0, 1], [0, CONVERSION_S], "fcs/stovl-pos-norm")
    out += fn("STOVL Throttle",
              f"{i}        <min>\n{i}            <property>fcs/throttle-cmd-norm</property>\n"
              f"{i}            <difference><value>1</value><product><value>{1.0 - STOVL_THROTTLE:.2f}</value>"
              f"<property>fcs/stovl-pos-norm</property></product></difference>\n{i}        </min>\n",
              "fcs/throttle-pos-norm")
    # The nozzle swivels only as far as the conversion has gone.
    out += fn("Nozzle Demand",
              f"{i}        <product>\n{i}            <property>fcs/nozzle-cmd-deg</property>\n"
              f"{i}            <property>fcs/stovl-pos-norm</property>\n{i}        </product>\n",
              "fcs/nozzle-demand-deg")
    out += (f"{i}<actuator name=\"Nozzle\">\n{i}    <input>fcs/nozzle-demand-deg</input>\n"
            f"{i}    <rate_limit>{NOZZLE_RATE_DEG_S:.1f}</rate_limit>\n"
            f"{i}    <clipto><min>0</min><max>{NOZZLE_MAX_DEG:.1f}</max></clipto>\n"
            f"{i}    <output>fcs/nozzle-pos-deg</output>\n{i}</actuator>\n")
    out += fn("Nozzle Vector",
              f"{i}        <product>\n{i}            <property>fcs/nozzle-pos-deg</property>\n"
              f"{i}            <value>{math.pi / 180.0:.12f}</value>\n{i}        </product>\n",
              "propulsion/engine/pitch-angle-rad")
    for name in ("lift-fan", "roll-post", "nozzle-yaw", "thrust-split"):
        out += (f"{i}<pure_gain name=\"{name} limit\">\n{i}    <input>fcs/{name}-cmd-norm</input>\n"
                f"{i}    <gain>1</gain>\n{i}    <clipto><min>-1</min><max>1</max></clipto>\n"
                f"{i}    <output>fcs/{name}-pos-norm</output>\n{i}</pure_gain>\n")
    # The thrust split (SPLIT_SHARE): the fraction of the nozzle's share sent
    # to the fan, as far as the conversion has gone - and sent back no further
    # than leaves the nozzle passing all the engine makes, which early in the
    # conversion, the split fully aft, it would pass: 1 - 1 / (the nozzle's
    # share at the split's middle) at the least.
    middle = (f"<difference><value>1</value><product><value>{1.0 - nozzle_share():.6f}</value>"
              f"<property>fcs/stovl-pos-norm</property></product></difference>")
    out += fn("Thrust Split",
              f"{i}        <max>\n{i}        <product>\n{i}            <value>{SPLIT_SHARE:.6f}</value>\n"
              f"{i}            <property>fcs/stovl-pos-norm</property>\n"
              f"{i}            <property>fcs/thrust-split-pos-norm</property>\n{i}        </product>\n"
              f"{i}        <difference><value>1</value><quotient><value>1</value>{middle}</quotient></difference>\n"
              f"{i}        </max>\n",
              "fcs/thrust-split-share")
    # What the nozzle would pass this step with the split at its middle,
    # which each share is reckoned from: the engine's own thrust - the
    # nozzle's of the step before over the reverser's cosine it had then -
    # times this step's nozzle share and swivel, so that the fan and posts
    # follow the conversion and the split in the step they move, and a step
    # behind the engine alone.
    yaw = math.radians(NOZZLE_YAW_DEG)
    out += fn("Lift System Base",
              f"{i}        <product>\n{i}            <quotient><property>{thrust}</property>"
              f"<cos><property>propulsion/engine/reverser-angle-rad</property></cos></quotient>\n"
              f"{i}            {middle}\n"
              f"{i}            <cos><product><value>{yaw:.6f}</value>"
              f"<property>fcs/nozzle-yaw-pos-norm</property></product></cos>\n{i}        </product>\n",
              "fcs/lift-system-base-lbf")
    # The power the fan's shaft and the posts take: the nozzle passes only its
    # share, less what the split sends the fan, as JSBSim's reverser takes
    # the cosine of its angle off the thrust; and swivelled sideways, only the
    # cosine of the swivel of that along its axis, the sine going to the
    # nozzle's sideways force - so that swivelling it makes no thrust.
    # Its cosine is held to -1..1 as well, so that rounding can never make
    # the angle NaN.
    out += fn("Shaft Power",
              f"{i}        <acos>\n{i}        <max><value>-1</value><min><value>1</value>\n{i}            <product>\n"
              f"{i}                <difference><value>1</value><product>"
              f"<value>{1.0 - nozzle_share():.6f}</value><property>fcs/stovl-pos-norm</property>"
              f"</product></difference>\n"
              f"{i}                <difference><value>1</value><property>fcs/thrust-split-share</property>"
              f"</difference>\n"
              f"{i}                <cos><product><value>{yaw:.6f}</value>"
              f"<property>fcs/nozzle-yaw-pos-norm</property></product></cos>\n"
              f"{i}            </product>\n{i}        </min></max>\n{i}        </acos>\n",
              "propulsion/engine/reverser-angle-rad")
    # The fan's thrust vectored by its vane box, forward and up.
    out += (f"{i}<actuator name=\"Fan Vanes\">\n{i}    <input>fcs/fan-cmd-deg</input>\n"
            f"{i}    <rate_limit>{FAN_RATE_DEG_S:.1f}</rate_limit>\n"
            f"{i}    <clipto><min>{FAN_MIN_DEG:.2f}</min><max>{FAN_MAX_DEG:.1f}</max></clipto>\n"
            f"{i}    <output>fcs/fan-pos-deg</output>\n{i}</actuator>\n")
    for axis, op, sign in (("x", "cos", "<value>1</value>"), ("z", "sin", "<value>-1</value>")):
        out += fn(f"Fan {axis.upper()}",
                  f"{i}        <product>\n{i}            {sign}<{op}><product><property>fcs/fan-pos-deg</property>"
                  f"<value>{math.pi / 180.0:.12f}</value></product></{op}>\n{i}        </product>\n",
                  f"external_reactions/lift-fan/{axis}")
    # The fan: its share, moved by its vanes, and what the split sends it.
    out += fn("Lift Fan",
              f"{i}        <sum>\n{i}        <product>\n{i}            <property>{base}</property>\n"
              f"{i}            <property>fcs/stovl-pos-norm</property>\n"
              f"{i}            <value>{LIFT_FAN_LBF / NOZZLE_LBF:.6f}</value>\n"
              f"{i}            <sum><value>1</value><product><value>{FAN_MODULATION}</value>"
              f"<property>fcs/lift-fan-pos-norm</property></product></sum>\n{i}        </product>\n"
              f"{i}        <product>\n{i}            <property>{base}</property>\n"
              f"{i}            <property>fcs/thrust-split-share</property>\n{i}        </product>\n"
              f"{i}        </sum>\n",
              "external_reactions/lift-fan/magnitude")
    out += share("Left Roll Post", ROLL_POST_LBF, POST_SHIFT, "fcs/roll-post-pos-norm",
                 "external_reactions/left-roll-post/magnitude")
    out += share("Right Roll Post", ROLL_POST_LBF, -POST_SHIFT, "fcs/roll-post-pos-norm",
                 "external_reactions/right-roll-post/magnitude")
    # The flaperons drooped as far as the conversion has gone; and each
    # flaperon's droop and roll together held to its travel, the same
    # FLAPERON_DEG the roll alone is (flight_control), so that drooped she
    # rolls on what is left. What the wing sees is their mean (the droop) and
    # half their difference (the roll); with no droop, exactly the roll.
    out += fn("Flaperon Droop",
              f"{i}        <product>\n{i}            <value>{FLAPERON_DROOP_DEG:.1f}</value>\n"
              f"{i}            <property>fcs/stovl-pos-norm</property>\n{i}        </product>\n",
              "fcs/flaperon-droop-deg")
    travel = f"{math.radians(FLAPERON_DEG):.4f}"
    for side in ("left", "right"):
        out += (f"{i}<fcs_function name=\"{side.capitalize()} Flaperon\">\n{i}    <function>\n"
                f"{i}        <sum>\n{i}            <product><property>fcs/flaperon-droop-deg</property>"
                f"<value>{math.pi / 180.0:.12f}</value></product>\n"
                f"{i}            <property>fcs/{side}-aileron-pos-rad</property>\n{i}        </sum>\n"
                f"{i}    </function>\n{i}    <clipto><min>-{travel}</min><max>{travel}</max></clipto>\n"
                f"{i}    <output>fcs/{side}-flaperon-rad</output>\n{i}</fcs_function>\n")
    for name, op, output in (("Flaperon Mean", "sum", "fcs/flaperon-droop-rad"),
                             ("Flaperon Roll", "difference", "fcs/flaperon-roll-rad")):
        out += fn(name,
                  f"{i}        <product><value>0.5</value><{op}><property>fcs/left-flaperon-rad</property>"
                  f"<property>fcs/right-flaperon-rad</property></{op}></product>\n",
                  output)
    out += fn("Nozzle Side",
              f"{i}        <product>\n{i}            <property>{thrust}</property>\n"
              f"{i}            <property>fcs/stovl-pos-norm</property>\n"
              f"{i}            <tan><product><value>{math.radians(NOZZLE_YAW_DEG):.6f}</value>"
              f"<property>fcs/nozzle-yaw-pos-norm</property></product></tan>\n{i}        </product>\n",
              "external_reactions/nozzle-side/magnitude")
    return out + "        </channel>\n"


def lift_table(indent):
    """The lift by angle of attack and Mach: at each Mach the slope of the
    wing's (fighter.lift_slope), straight to STRAIGHT_ALPHA, rounding to its
    greatest at STALL_ALPHA and falling past it; held from -0.60 to -90
    degrees, as JSBSim held it before, then straight to nothing at -180;
    nothing from 90 to 180 (the docstring says why)."""
    alphas = [-math.pi, -1.57, -0.60, -STRAIGHT_ALPHA, 0.0, STRAIGHT_ALPHA, (STRAIGHT_ALPHA + STALL_ALPHA) / 2.0,
              STALL_ALPHA, 0.80, 1.20, 1.57, math.pi]

    def at_low_mach(a, slope):
        if a >= 1.57 or a <= -math.pi:
            return 0.0
        if a <= STRAIGHT_ALPHA:
            return slope * max(a, -STRAIGHT_ALPHA) if a >= -STRAIGHT_ALPHA else -slope * STRAIGHT_ALPHA
        straight = slope * STRAIGHT_ALPHA
        peak = straight + 0.5 * slope * (STALL_ALPHA - STRAIGHT_ALPHA)
        if a <= STALL_ALPHA:
            x = (a - STRAIGHT_ALPHA) / (STALL_ALPHA - STRAIGHT_ALPHA)
            return straight + (peak - straight) * (2.0 * x - x * x)
        return peak * max(0.0, math.cos(a)) / math.cos(STALL_ALPHA) if a < 1.57 else 0.0

    head = f"{indent}            \t" + "\t".join(f"{m:.2f}" for m in fighter.MACHS) + "\n"
    rows = ""
    for a in alphas:
        rows += f"{indent}        {a:.4f}\t" + "\t".join(
            f"{at_low_mach(a, LIFT_FACTOR * fighter.lift_slope(m, ASPECT, SWEEP_QUARTER_CHORD)):.4f}"
            for m in fighter.MACHS) + "\n"
    return (f"{indent}<table>\n"
            f"{indent}    <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
            f"{indent}    <independentVar lookup=\"column\">velocities/mach</independentVar>\n"
            f"{indent}    <tableData>\n{head}{rows}{indent}    </tableData>\n{indent}</table>\n")


def pitch_alpha_table(indent):
    """Cm-alpha by the angle of attack, all the way round: CM_ALPHA times it
    to 90 degrees either way, then straight to nothing at 180, so that it is
    the same at -180 and 180 (the docstring says why)."""
    alphas = [-math.pi, -math.pi / 2.0, math.pi / 2.0, math.pi]
    rows = "".join(f"{indent}        {a:.6f}\t{CM_ALPHA * math.copysign(min(abs(a), math.pi - abs(a)), a) + 0.0:.6f}\n"
                   for a in alphas)
    return (f"{indent}<table>\n"
            f"{indent}    <independentVar>aero/alpha-rad</independentVar>\n"
            f"{indent}    <tableData>\n{rows}{indent}    </tableData>\n{indent}</table>\n")


def flap_lift_per_deg():
    """The flaperons' droop's lift a degree (FLAPERON_DROOP_DEG): DATCOM's
    plain flap, at low speed."""
    return (fighter.lift_slope(0.2, ASPECT, SWEEP_QUARTER_CHORD) * FLAP_EFFECTIVENESS * FLAPPED_WING_SHARE
            * math.pi / 180.0)


def aerodynamics():
    t = "                    "
    w = written
    q, s, b, c = "aero/qbar-psf", "metrics/Sw-sqft", "metrics/bw-ft", "metrics/cbarw-ft"
    lift = [
        w.coefficient("CLalpha", "Lift_due_to_alpha", [q, s, "aero/function/kCLge", lift_table(t)]),
        w.coefficient("CLde", "Lift_due_to_stabilator", [q, s, "fcs/elevator-pos-rad", CL_DE]),
        w.coefficient("CLdroop", "Lift_due_to_flaperon_droop", [q, s, "fcs/flaperon-droop-rad",
                                                                 round(flap_lift_per_deg() * 180.0 / math.pi, 6)]),
    ]
    drag = [fighter.drag_functions(SUBSONIC_ZERO_LIFT_DRAG, WAVE_DRAG, make_f15c.WAVE_DECAY, ASPECT,
                                   SPAN_EFFICIENCY, make_f15c.SUPERSONIC_LIFT_DRAG, SEPARATION_ALPHA),
            w.coefficient("CDgear", "Drag_due_to_gear", [q, s, "gear/gear-pos-norm", 0.02]),
            w.coefficient("CDbeta", "Drag_due_to_sideslip", [q, s, "aero/mag-beta-rad", 0.25]),
            w.coefficient("CDde", "Drag_due_to_stabilator", [q, s, "fcs/mag-elevator-pos-rad", 0.03]),
            w.coefficient("CDdroop", "Drag_due_to_flaperon_droop", [q, s, "fcs/flaperon-droop-rad",
                                                                    FLAP_DRAG / math.radians(FLAPERON_DROOP_DEG)])]
    side = [
        w.coefficient("CYb", "Side_force_due_to_beta", [q, s, "aero/beta-rad", CY_BETA]),
        w.coefficient("CYda", "Side_force_due_to_flaperons", [q, s, "fcs/flaperon-roll-rad", 2.0 * CY_DA]),
        w.coefficient("CYdr", "Side_force_due_to_rudders", [q, s, "fcs/rudder-pos-rad", CY_DR]),
    ]
    roll = [
        w.coefficient("Clb", "Roll_moment_due_to_beta", [q, s, b, "aero/beta-rad", CL_BETA]),
        w.coefficient("Clp", "Roll_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CL_P]),
        w.coefficient("Clr", "Roll_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CL_R]),
        w.coefficient("Clda", "Roll_moment_due_to_flaperons", [q, s, b, "fcs/flaperon-roll-rad", 2.0 * CL_DA]),
        w.coefficient("Cldr", "Roll_moment_due_to_rudders", [q, s, b, "fcs/rudder-pos-rad", CL_DR]),
    ]
    pitch = [
        w.coefficient("Cmalpha", "Pitch_moment_due_to_alpha", [q, s, c, pitch_alpha_table(t)]),
        w.coefficient("Cmq", "Pitch_moment_due_to_pitch_rate", [q, s, c, "aero/ci2vel", "velocities/q-aero-rad_sec", CM_Q]),
        w.coefficient("Cmadot", "Pitch_moment_due_to_alpha_rate", [q, s, c, "aero/ci2vel", "aero/alphadot-rad_sec", CM_ADOT]),
        w.coefficient("Cmde", "Pitch_moment_due_to_stabilator", [q, s, c, "fcs/elevator-pos-rad", CM_DE]),
        w.coefficient("Cmdroop", "Pitch_moment_due_to_flaperon_droop", [q, s, c, "fcs/flaperon-droop-rad",
                                                                         round(-0.25 * flap_lift_per_deg() * 180.0 / math.pi, 6)]),
    ]
    yaw = [
        w.coefficient("Cnb", "Yaw_moment_due_to_beta", [q, s, b, "aero/beta-rad", CN_BETA]),
        w.coefficient("Cnp", "Yaw_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CN_P]),
        w.coefficient("Cnr", "Yaw_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CN_R]),
        w.coefficient("Cnda", "Yaw_moment_due_to_flaperons", [q, s, b, "fcs/flaperon-roll-rad", 2.0 * CN_DA]),
        w.coefficient("Cndr", "Yaw_moment_due_to_rudders", [q, s, b, "fcs/rudder-pos-rad", CN_DR]),
    ]
    return w.axes((("LIFT", lift), ("DRAG", drag), ("SIDE", side), ("ROLL", roll), ("PITCH", pitch), ("YAW", yaw)))


def airframe():
    return ("<?xml version=\"1.0\"?>\n"
            "<fdm_config name=\"Lockheed Martin F-35B\" version=\"2.0\" release=\"BETA\">\n"
            "    <fileheader>\n"
            "        <!-- glideslope: written by tools/make_f35b.py, which says where each number\n"
            "             comes from. Do not edit it by hand. -->\n"
            "        <author>glideslope</author>\n"
            "        <filecreationdate>2026-09-19</filecreationdate>\n"
            "        <description>Lockheed Martin F-35B Lightning II, F135-PW-600</description>\n"
            "    </fileheader>\n"
            + metrics() + mass_balance() + ground_reactions() + external_reactions() + propulsion() + flight_control()
            + aerodynamics() + "</fdm_config>\n")


def engine():
    text = (PINNED / "engine" / "F100-PW-229.xml").read_text()
    text = airliner.replace_once(text, r"(<turbine_engine name=\")F100(\">)",
                                 r"\g<1>" + ENGINE + r"\2\n  <!-- glideslope: JSBSim's F100-PW-229 made the F-35B's F135\n"
                                 r"       by tools/make_f35b.py, which made it. Do not edit it by hand. -->",
                                 "the engine's name", SCRIPT)
    text = airliner.replace_once(text, r"<milthrust>\s*17800\.0\s*</milthrust>",
                                 f"<milthrust>   {MILITARY_THRUST} </milthrust>", "the military thrust", SCRIPT)
    text = airliner.replace_once(text, r"<maxthrust>\s*29000\.0\s*</maxthrust>",
                                 f"<maxthrust>   {MAXIMUM_THRUST} </maxthrust>", "the maximum thrust", SCRIPT)
    text = airliner.replace_once(text, r"<augmethod>\s*2\s*</augmethod>", "<augmethod>         1 </augmethod>",
                                 "the afterburner's method", SCRIPT)
    text = airliner.replace_once(text, r"<tsfc>\s*0\.74\s*</tsfc>", f"<tsfc>            {TSFC:.2f} </tsfc>",
                                 "the fuel consumption", SCRIPT)
    idle = fighter.table(text, "IdleThrust")
    f15 = make_f15c
    for name, rows in (("IdleThrust", fighter.idle_table(idle)),
                       ("MilThrust", fighter.thrust_table(f15.MILITARY_EXPONENT, MILITARY_THROTTLE_RATIO,
                                                          f15.MILITARY_FALL, f15.HIGH_LOSS, idle)),
                       ("AugThrust", fighter.thrust_table(f15.MAXIMUM_EXPONENT, MAXIMUM_THROTTLE_RATIO,
                                                          f15.MAXIMUM_FALL, f15.HIGH_LOSS))):
        text = airliner.replace_once(
            text, r'(<function name="' + name + r'">\s*<table>.*?<tableData>\s*\n).*?(\n\s*</tableData>)',
            lambda m: m.group(1) + rows + m.group(2), f"the {name} table", SCRIPT)
    return text


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": side_force.with_side_force_tapered(alpha_rate.with_unkeyed_terms_on_qbar_uw(
            alpha_rate.with_alpha_terms_on_qbar_uw(
            alpha_rate.with_qbar_uw(airframe(), "make_f35b"), "make_f35b",
            {"PITCH": ["aero/coefficient/Cmalpha"],
             "LIFT": ["aero/coefficient/CLalpha"],
             "DRAG": ["aero/coefficient/CDseparated"]}), "make_f35b",
            {"LIFT": ["aero/coefficient/CLde",
             "aero/coefficient/CLdroop"],
             "PITCH": ["aero/coefficient/Cmq",
             "aero/coefficient/Cmde",
             "aero/coefficient/Cmdroop"]}), "make_f35b"),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
