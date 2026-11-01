#!/usr/bin/env python3
"""make_747_400.py - glideslope's Boeing 747-400, made from JSBSim's B747.

JSBSim's B747, as pinned in ext/jsbsim, calls itself a B747-400: its 211.5 ft
span is the -400's (not the shorter -400 Domestic's), and its four
GE CF6-80C2B1F engines of 58,000 lb are the -400 passenger's. Its figures
here are from Boeing's 747-400 Airplane Characteristics for Airport Planning
(D6-58326-1, revision F, 2024) and the FAA's type certificate data sheet
A20WE, in assets/figures/747-400.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_747_400.py           write the files
    python3 tools/make_747_400.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Thrust reversers, added (aircraft/747-400/747-400.xml)
                        The model had none, and a wet runway's landing is
                        stopped with them (AC 25-32, section 10): on
                        all four CF6-80C2s, a translating sleeve over cascades,
                        a channel deploying them on the reverse lever with
                        the weight on the wheels, reversing 0.31 of the
                        engine's thrust - see tools/airliner.py.

  Airframe (aircraft/747-400/747-400.xml)
    Weights and loading The planning document's operating empty weight,
                        394,088 lb (three classes, 400 seats), where the model
                        had 523,816 lb; its five fuel tanks together 360,225
                        lb, the -400's 53,765 US gallons, where they held
                        54,564 lb - the model's empty weight carried the rest;
                        and a payload at the centre of gravity, which the model
                        had nowhere to put.
    Nose gear spring 22,000 -> 120,000 lb/ft
                        A seventh of each main gear's: at 875,000 lb the nose
                        sank 2.7 ft and the aircraft sat 3 degrees nose down,
                        where a 747's nose oleo settles about half a foot under
                        its share of the weight.
    Drag_due_to_mach    Lock's fourth-power rise from DRAG_DIVERGENCE, as the
                        737-300's (see tools/airliner.py), where the model's
                        rose from nothing at Mach 0.79 at a tenth of a swept
                        wing's slope.
    Drag_of_windmilling_engines, added
                        As the 737-300's: a stopped CF6-80C2's 93 in fan
                        windmills, with a drag area of about 0.4 of its
                        frontal area.
    Main wheels' braking friction 0.8 -> 0.5
                        As the 737-300's: a dry runway's, where 0.8 stopped a
                        rejected take-off at two-thirds of a g.
    Main gears' springs 150,000 -> 222,000 lb/ft (2026-11-02)
                        Her ground clearances' 12 in between her empty and
                        her maximum weight (D6-58326-1, figure 2.3.1); see
                        MAIN_SPRING_PER_LEG.
    Main gear station 1554 -> 1404 (2026-11-01)
                        Boeing's 84 ft wheelbase (D6-58326-1, figure 2.2.1),
                        where the model had 96.5 ft, 19.6% of her weight on
                        her nose wheel, and an elevator that could not lift
                        it below 182 kt; see MAIN_GEAR_X_IN.
    Delta_Lift_due_to_flaps, a table (2026-11-01)
                        Her flaps' lift and lift-curve slope at flaps 20 and
                        30 from NASA CR-2144's tables IX-1 and IX-2, up to the
                        lift of Boeing's certification stall speeds (NASA
                        CR-1756, page 2.0-37), where 0.05 a degree of flap
                        had no source; see flap_lift_rows.
    Pitch_moment_due_to_elevator at Mach 0 (2026-11-01)
                        Moved so that her elevator has CR-2144's -1.40 a
                        radian at Mach 0.198; see CM_DE_AT_0_HEFFLEY.
    Ground effect, added (2026-11-01)
                        JSBSim's own kCLge on her wing's and flaps' lift, as
                        the A320's script adds it: the model had none.

  Engine (engine/CF6-80C2B1F.xml, from JSBSim's GE-CF6-80C2-B1F.xml)
    Thrust with height and speed
                        Mattingly's lapse, as the 737-300's (see
                        tools/airliner.py), where JSBSim's generic turbofan
                        table held its thrust far too well with speed and
                        height.

**The alpha-rate terms on the dynamic pressure without its sideways part**
(tools/alpha_rate.py, JSBSim issue #89): braked to a stop in a crosswind, her
angle of attack is undefined, its rate swings, and multiplied by the whole
dynamic pressure it pitched her by tens of degrees a second.

**Her pitch terms keyed to the angle of attack on the dynamic pressure without
its sideways part** (alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15):
Cmalpha. Standing or slowing in a crosswind her angle of attack, atan2(w, u),
is any angle at all - 89 to 91 degrees standing on her brakes in 15 kt across -
and on the whole dynamic pressure, which counts the crosswind, it pitched her
nose down by up to 129,000 ft-lb; on aero/qbarUW-psf, 2. Her wing is swept,
about 37.5 degrees at the quarter chord, where issue #89 did not look; simple
sweep theory (NACA Report 863; alpha_rate.py's docstring) puts the part of a
beam wind's moment that goes with the angle of attack near nothing standing, as
qbarUW does, where on qbar it is as large as the undefined angle makes it.
Standing, qbarUW is the nearer; in flight, at a few degrees of sideslip, the
two differ by its cosine squared, and the theory's pressure lies between them,
under 1% from either (alpha_rate.py).

**Her lift keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15): CLalpha. Standing on her
brakes in 15 kt straight across, her angle of attack, atan2(w, u), was -82 to
91 degrees, and on the whole dynamic pressure, which counts the crosswind, the
term lifted her by up to 2,920 lb either way; on aero/qbarUW-psf, nothing.
JSBSim issue #89's guidance, written into the FlightGear wiki's "JSBSim
Aerodynamics": QBarUW "is more appropriate for CL than the generic QBar" - a
wing's lift is made by the air in the plane of u and w. Her wing is swept,
about 37.5 degrees at the quarter chord: the sweep argument above (simple sweep
theory, NACA Report 863) holds for the lift of the same wing as for its moment
- standing, qbarUW is the nearer; in flight, at a few degrees of sideslip, the
two are under 1% apart. Her lift terms not keyed to the angle of attack
(dCLflap, dCLsb and CLde) were not this change's: they are moved below
(2026-10-23).

**Her wing's drag keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-16): CD0, her drag against the
angle of attack. Standing on her brakes in 15 kt straight across, her angle of
attack, atan2(w, u), was 0 to 92 degrees, and on the whole dynamic pressure,
which counts the crosswind, it drew her along the beam wind by up to 6,450 lb;
on aero/qbarUW-psf, nothing. The same reasoning as her lift's, above: a wing's
drag at its angle of attack is made by the air in the plane of u and w, and the
sideways part runs along its span - the drag that rolled the J-3 Cub onto her
wing standing in a beam wind (make_j3cub.py). In flight, at a few degrees of
sideslip, the two differ by its cosine squared. Her wing is swept: moved on the
same argument as her lift, simple sweep theory
(alpha_rate.with_alpha_terms_on_qbar_uw).

**Her lift terms not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-23, with_alpha_terms_on_qbar_uw for a lift with pitch rate whose
coefficient the angle picks): dCLflap, dCLsb and CLde. On the whole dynamic
pressure they counted the crosswind standing. Standing on her brakes with her
engines stopped in 15 kt straight across, a lifting control set one at a time,
they lifted her by up to 6,451 lb (flaps down); now under 0.1 lb at every
setting (the standing test in tests/unit/test_departure.cpp). JSBSim issue
#89's guidance puts the whole of CL on QBarUW; alpha_rate.py says why, for
swept surfaces too. In flight, at a few degrees of sideslip, the two differ by
its cosine squared.

**Her pitching moments not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-28): Cmde and Cmq. On the whole dynamic pressure they counted the
crosswind standing, as her lift did. Standing on her brakes with her engines
stopped in 15 kt straight across, a control set one at a time, they pitched her
by up to 53,063 ft-lb (stick back); now by 2.10 ft-lb at most (the standing test
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

import re
import sys

import alpha_rate
import side_force
import airliner
import ground
from make_a320 import GROUND_EFFECT
from airliner import OUT, PINNED

SCRIPT = "make_747_400"
# The engines with thrust reversers, by JSBSim's index: all four CF6-80C2s, a translating sleeve over cascades
# (tools/airliner.py, before reverser_channel, says what each is and where from).
REVERSERS = (0, 1, 2, 3)
MODEL = "747-400"
ENGINE = "CF6-80C2B1F"
OPERATING_EMPTY_LBS = 394088
CG_X_IN = 1327
TANK_LBS = 72045          # 360,225 lb of fuel in five tanks
DRAG_DIVERGENCE = 0.88
FAN_DIAMETER_IN = 93.0
WINDMILL_DRAG = 0.4
BRAKING_FRICTION = "0.5"
NOSE_SPRING = "120000"

# **The airframe, so that a wheels-up landing meets the runway.** JSBSim gives
# a retracted wheel no force, and this model had nothing else to touch the
# ground with: landed with its wheels up it went straight through it. The
# points are measured from this aeroplane's own visual mesh by
# tools/ground.py, which says how and why the mesh is the source.
MAXIMUM_WEIGHT_LBS = 875000  # the -400's published maximum take-off weight


# **Her flaps' lift, from NASA CR-2144** (R. K. Heffley and W. F. Jewell,
# "Aircraft Handling Qualities Data", 1972, section IX, the B-747; its data
# from Boeing's 747 simulator description D6-30643). Two of its trim points,
# at sea level and 564,000 lb on its 5,500 sq ft wing, each with its lift
# and lift-curve slope: table IX-1, landing, flaps 30 and the gear down, 131
# KTAS at 8.5 degrees, CL 1.76 and CLalpha 5.67 a radian; table IX-2, power
# approach, flaps 20, 165 KTAS at 5.7 degrees, CL 1.11 and CLalpha 5.70.
HEFFLEY_WING_SQFT = 5500.0
WING_SQFT = 5648.0            # the model's (metrics/Sw-sqft)
HEFFLEY_FLAPS = (             # flap degrees, Mach, alpha degrees, CL, CLalpha per radian
    (20, 0.249, 5.7, 1.11, 5.70),
    (30, 0.198, 8.5, 1.76, 5.67),
)
CLEAN_CL0 = 0.2               # the model's CLalpha table: 0.2 at no angle,
CLEAN_CL_STALL = 1.2          # 1.2 at its stall,
STALL_ALPHA_RAD = 0.23        # 0.23 radian,
CLEAN_CL_DEEP = 0.6           # and 0.6 at 0.6 radian
CM_ALPHA = -0.7               # the model's Cmalpha,
CL_DE = 0.2                   # CLde,
CM_DE_AT_0, CM_DE_AT_2 = -1.3, -0.325   # and Cmde at Mach 0 and 2


# **Her flapped wing's stall, from the same report's source**: NASA CR-1756
# (Hanke and Nordwall, "The Simulation of a Jumbo Jet Transport Aircraft,
# Volume II: Modeling Data", 1970), page 2.0-37, Boeing's certification stall
# speeds (D6-30643; engines idle, load factor less than one, forward centre of
# gravity, the gear up but at flaps 25 and 30), read from the chart: flaps 20,
# 134 KEAS at 700,000 lb; flaps 30, 112 KEAS at 600,000 lb. On the 5,500 sq
# ft wing those are a CL of 2.08 and 2.57. Her flapped lift keeps Heffley and
# Jewell's slope past their trim point up to that CL, its angle her flapped
# stall's, and then falls as her clean wing's does past its stall. Without it
# her flapped wing stalled at the clean wing's 0.23 radian, at a CL of 1.82
# and 2.18: 157 kt at 875,000 lb with flaps 20, where Boeing's stall speeds
# make 150, and a 747-400 flies her V2 at 181 kt (1.2 times it), and her 153
# kt approach only 1.19 times her stall at flaps 30 where the rules ask 1.3.
SEA_LEVEL_SLUGFT3 = 0.0023769
FPS_PER_KT = 1.6878099
CERTIFICATION_STALLS = {      # flap degrees: (lb, KEAS)
    20: (700000.0, 134.0),
    30: (600000.0, 112.0),
}


def flapped_stall_cl(flap):
    weight, keas = CERTIFICATION_STALLS[flap]
    v = keas * FPS_PER_KT
    return weight / (0.5 * SEA_LEVEL_SLUGFT3 * v * v * HEFFLEY_WING_SQFT)


def flap_lift_rows():
    """dCLflap's table: rows of angle of attack (radians), columns of flap
    degrees - what each flap adds to the model's clean lift so that, trimmed
    by its elevator at Heffley and Jewell's angle and Mach, she has their
    lift and lift-curve slope on her own wing, up to the certification stall
    speeds' lift, and past it falls as her clean wing does past its stall."""
    import math
    clean_slope = (CLEAN_CL_STALL - CLEAN_CL0) / STALL_ALPHA_RAD
    scale = HEFFLEY_WING_SQFT / WING_SQFT
    deep = CLEAN_CL_DEEP / CLEAN_CL_STALL

    def clean(alpha):
        if alpha <= STALL_ALPHA_RAD:
            return CLEAN_CL0 + clean_slope * alpha
        return CLEAN_CL_STALL + (CLEAN_CL_DEEP - CLEAN_CL_STALL) * (
            (alpha - STALL_ALPHA_RAD) / (0.6 - STALL_ALPHA_RAD))

    columns = [(0, None)]
    stalls = []
    for flap, mach, alpha_deg, cl, cl_alpha in HEFFLEY_FLAPS:
        cm_de = CM_DE_AT_0_HEFFLEY + (CM_DE_AT_2 - CM_DE_AT_0_HEFFLEY) * mach / 2.0
        trim_alpha = math.radians(alpha_deg)
        slope = cl_alpha * scale
        cl_max = flapped_stall_cl(flap) * scale
        stall = trim_alpha + (cl_max - cl * scale) / slope

        def total(alpha, trim_alpha=trim_alpha, cl=cl, slope=slope,
                  cl_max=cl_max, stall=stall):
            if alpha <= stall:
                return cl * scale + slope * (alpha - trim_alpha)
            return cl_max * (1.0 - (1.0 - deep) * (alpha - stall) / (0.6 - stall))

        def increment(alpha, total=total, cm_de=cm_de):
            elevator = -CM_ALPHA * alpha / cm_de       # her trim, in radians
            return total(alpha) - clean(alpha) - CL_DE * elevator

        columns.append((flap, increment))
        stalls.append(stall)
    alphas = sorted({-0.2, 0.0, STALL_ALPHA_RAD, 0.6, *(round(a, 4) for a in stalls)})
    rows = ["                              " + "\t".join(f"{c[0]}" for c in columns)]
    for alpha in alphas:
        rows.append(f"                              {alpha:.4f}\t" +
                    "\t".join("0.0000" if f is None else f"{f(alpha):.4f}"
                              for _, f in columns))
    return "\n".join(rows)


# **Her main gear where Boeing puts it**: 84 ft 0 in behind her nose gear,
# the wheelbase of D6-58326-1's general dimensions (December 2002, figure
# 2.2.1, page 21), to the middle of her wing and body gear, which the model's
# one main leg a side stands for. The model had them 96.5 ft back, at station
# 1554, and 19.6% of her weight on her nose wheel at her centre of gravity
# (1327, 21% of her chord): D6-58326-1's section 7.3 has 64,000 to 93,300 lb
# on it at the most forward centre of gravity at 803,000 to 877,000 lb, 7.3
# to 11.6%, and here, aft of the most forward, it is 7.6%. Held back so far,
# her elevator could not lift her nose from the runway below 182 kt.
NOSE_GEAR_X_IN = 396
MAIN_GEAR_X_IN = NOSE_GEAR_X_IN + 84 * 12


# **Her main legs' spring, from her ground clearances.** D6-58326-1, figure
# 2.3.1 (747-400, -400 Combi, ground clearances, July 2003), gives each
# point's height at the least and the most the loadings put it at; point T,
# the fuselage just behind her body gear, the nearest to her main wheels and
# so the least moved by her attitude, is 17 ft 0 in at its most and 16 ft 0
# in at its least: 12 in between her operating empty weight, 394,088 lb, and
# her 875,000 lb maximum. The table does not name its weights; they are
# taken as the lightest and the heaviest it can mean. Her main wheels carry
# MAIN_SHARE of her weight (her centre of gravity, station 1327, 77 in ahead
# of them and 931 behind her nose wheel), so her mains, tyres and legs
# together, sink 12 in under 0.924 times 480,912 lb: 444,000 lb/ft, 222,000
# a leg for the model's two. JSBSim's B747 had 150,000 a leg, which sank her
# 20 in more at the tail strike: over-rotated on her wheels she struck at
# 11.5 degrees against her contacts' 12.7 and Boeing's 12.5 (AERO tailstrike
# article). The spring is linear from no load, where hers is an oleo
# pumped up to carry her empty weight, so it is her rate between the two
# weights, not her height standing.
OPERATING_EMPTY_LBS = 394088
MAIN_SHARE = round(931 / (MAIN_GEAR_X_IN - NOSE_GEAR_X_IN), 3)
CLEARANCE_T_FT = (17.0, 16.0)        # at her lightest and heaviest
MAIN_SPRING_PER_LEG = round(MAIN_SHARE * (MAXIMUM_WEIGHT_LBS - OPERATING_EMPTY_LBS)
                            / (CLEARANCE_T_FT[0] - CLEARANCE_T_FT[1]) / 2.0, -3)


# **Her elevator's power at an approach's Mach, from the same report**:
# Cmdelta_e -1.40 a radian in table IX-1 (Mach 0.198) and -1.34 in table
# IX-2 (Mach 0.249), on its 27.31 ft chord, the model's. The model's falls in
# a line from -1.3 at Mach 0 to -0.325 at Mach 2 - -1.20 at 0.198; its Mach 0
# figure is moved to the one the line through Heffley and Jewell's first
# point and the model's Mach 2 figure has there, so that her elevator has
# -1.40 at 0.198 and -1.37 at 0.249.
CM_DE_AT_0_HEFFLEY = round(-1.40 + (CM_DE_AT_2 - -1.40) * -0.198 / (2.0 - 0.198), 4)


def replace_once(text, pattern, replacement, what):
    return airliner.replace_once(text, pattern, replacement, what, SCRIPT)


def engine():
    text = (PINNED / "engine" / "GE-CF6-80C2-B1F.xml").read_text()
    text = replace_once(text, r"(<turbine_engine name=\"GE-CF6-80C2-B1F\">)",
                        r"\1\n  <!-- glideslope: JSBSim's GE-CF6-80C2-B1F with the changes listed in\n"
                        r"       tools/make_747_400.py, which made it. Do not edit it by hand. -->",
                        "the engine's name")
    return airliner.with_mattingly_thrust(text, SCRIPT)


def airframe():
    text = (PINNED / "aircraft" / "B747" / "B747.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n      <!-- glideslope: this is JSBSim's B747 with the changes listed in\n"
        r"           tools/make_747_400.py, which made it. Do not edit it by hand. -->",
        "the file header")
    airliner.without_sockets(text, SCRIPT)
    text = replace_once(text, r"<emptywt unit=\"LBS\">\s*523816\s*</emptywt>",
                        f"<emptywt unit=\"LBS\"> {OPERATING_EMPTY_LBS} </emptywt>", "the empty weight")
    text = replace_once(
        text, r"(            <z> -24 </z>\n        </location>\n)(    </mass_balance>)",
        r'\1        <pointmass name="Payload">' "\n"
        r'            <weight unit="LBS"> 0 </weight>' "\n"
        r'            <location unit="IN">' "\n"
        rf'                <x> {CG_X_IN} </x>' "\n"
        r'                <y> 0 </y>' "\n"
        r'                <z> -24 </z>' "\n"
        r'            </location>' "\n"
        r'        </pointmass>' "\n" r"\2", "the mass balance")
    text, n = re.subn(r"(<capacity unit=\"LBS\">)\s*10912\.8\s*(</capacity>\s*<contents unit=\"LBS\">)\s*5456\.4\s*(</contents>)",
                      rf"\g<1> {TANK_LBS} \g<2> {TANK_LBS} \3", text)
    if n != 5:
        raise SystemExit(f"{SCRIPT}: found {n} fuel tanks, not 5 - has the pinned model changed?")
    text, n = re.subn(r"<engine file=\"GE-CF6-80C2-B1F\">", f'<engine file="{ENGINE}">', text)
    if n != 4:
        raise SystemExit(f"{SCRIPT}: found {n} engines, not 4 - has the pinned model changed?")
    for side in ("LEFT", "RIGHT"):
        text = replace_once(
            text, r"(name=\"" + side + r"_MLG\">.*?<static_friction>)\s*0\.8\s*(</static_friction>)",
            rf"\g<1> {BRAKING_FRICTION} \2", f"the {side.lower()} main gear's friction")
    # An assertion, not a change: the file's nose gear already stands at
    # NOSE_GEAR_X_IN, and this fails if it ever stops.
    text = replace_once(
        text, r"(name=\"NOSE_LG\">.*?<spring_coeff unit=\"LBS/FT\">)\s*22000\s*(</spring_coeff>)",
        rf"\g<1> {NOSE_SPRING} \2", "the nose gear's spring")

    for side in ("LEFT", "RIGHT"):
        text = replace_once(
            text, r"(name=\"" + side + r"_MLG\">\s*<location unit=\"IN\">\s*<x>)\s*1554\s*(</x>)",
            rf"\g<1> {MAIN_GEAR_X_IN} \2", f"the {side.lower()} main gear's station")
        text = replace_once(
            text, r"(name=\"" + side + r"_MLG\">.*?<spring_coeff unit=\"LBS/FT\">)\s*150000\s*(</spring_coeff>)",
            rf"\g<1> {MAIN_SPRING_PER_LEG:.0f} \2", f"the {side.lower()} main gear's spring")
    text = replace_once(
        text, r"(name=\"NOSE_LG\">\s*<location unit=\"IN\">\s*<x>)\s*" + str(NOSE_GEAR_X_IN) + r"\s*(</x>)",
        r"\g<1> " + str(NOSE_GEAR_X_IN) + r" \2", "the nose gear's station")
    # The airframe's own contacts, so that the aeroplane has something to
    # land on with its wheels up. See MAXIMUM_WEIGHT_LBS above.
    points = ground.contacts(MODEL, MAXIMUM_WEIGHT_LBS)
    text = replace_once(text, r"(\n)(    </ground_reactions>)",
                        "\n\n" + points.replace("\\", "\\\\") + r"\2",
                        "the airframe's contacts")

    text = replace_once(
        text,
        r"(<description>Drag_due_to_mach</description>.*?<tableData>\n).*?(\n\s*</tableData>)",
        lambda mm: mm.group(1) + airliner.mach_drag_rows(DRAG_DIVERGENCE) + mm.group(2), "the Mach drag")
    text = replace_once(text, r"(\n\s*<function name=\"aero/coefficient/CDgear\">)",
                        lambda mm: "\n" + airliner.windmill_function(4, FAN_DIAMETER_IN, WINDMILL_DRAG) + "\n"
                        + mm.group(1), "the gear drag function")
    text = replace_once(
        text,
        r"(<description>Delta_Lift_due_to_flaps</description>\s*<product>\s*<property>aero/qbar-psf</property>\s*"
        r"<property>metrics/Sw-sqft</property>\s*)<property>fcs/flap-pos-deg</property>\s*<value>0\.05</value>",
        lambda mm: mm.group(1) + "<table>\n"
        "                          <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
        "                          <independentVar lookup=\"column\">fcs/flap-pos-deg</independentVar>\n"
        "                          <tableData>\n" + flap_lift_rows() + "\n"
        "                          </tableData>\n"
        "                      </table>", "the flaps' lift")
    text = replace_once(
        text,
        r"(<description>Pitch_moment_due_to_elevator</description>.*?<tableData>\s*0\.0000\s*)-1\.3000",
        lambda mm: mm.group(1) + f"{CM_DE_AT_0_HEFFLEY:.4f}", "the elevator's pitch")
    # Her wing's lift in ground effect, JSBSim's own, as its 737 has it and
    # the A320's script adds it (GROUND_EFFECT, tools/make_a320.py): the
    # model had none.
    text = replace_once(text, r"(<aerodynamics>\n)",
                        lambda mm: mm.group(1) + "\n" + GROUND_EFFECT, "the aerodynamics")
    for name in ("CLalpha", "dCLflap"):
        text = replace_once(
            text,
            r"(<function name=\"aero/coefficient/" + name + r"\">.*?<property>metrics/Sw-sqft</property>\n)",
            lambda mm: mm.group(1) + "                    <property>aero/function/kCLge</property>\n",
            f"{name}'s ground effect")
    # The tail strike, measured from her visual model by tools/ground.py.
    return ground.with_tail_strike(
        text, MODEL, MAXIMUM_WEIGHT_LBS, script=SCRIPT)


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": side_force.with_side_force_tapered(airliner.with_reversers(alpha_rate.with_unkeyed_terms_on_qbar_uw(
            alpha_rate.with_alpha_terms_on_qbar_uw(
            alpha_rate.with_qbar_uw(airframe(), "make_747_400"), "make_747_400",
            {"PITCH": ["aero/coefficient/Cmalpha"],
             "LIFT": ["aero/coefficient/CLalpha", "aero/coefficient/dCLflap"],
             "DRAG": ["aero/coefficient/CD0"]}), "make_747_400",
            {"LIFT": ["aero/coefficient/dCLsb",
             "aero/coefficient/CLde"],
             "PITCH": ["aero/coefficient/Cmde",
             "aero/coefficient/Cmq"]}), REVERSERS, SCRIPT), "make_747_400"),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
