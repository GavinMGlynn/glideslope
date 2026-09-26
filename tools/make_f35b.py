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

**What this is not: the lift fan is not modelled.** The F-35B is a STOVL
aeroplane, and its shaft-driven LiftFan, three-bearing swivel nozzle and roll
posts are what make it one. None of that is here. This model flies the B's
wing, weights, engine and published conventional figures, and takes off and
lands on a runway like any other fighter. Hovering, vertical landing and short
take-off are named in COMPLETION_PLAN.md as work not done, not quietly left
out.

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
surfaces' travel; and the pitch and yaw dampers its fly-by-wire flight
controls stand for. The airframe's own contacts, which it rests on with its
wheels up, are measured from its visual model by tools/ground.py.
"""

import math
import re
import sys

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
    return out + "    </ground_reactions>\n"


def propulsion():
    return ("    <propulsion>\n"
            f"        <engine file=\"{ENGINE}\">\n"
            "            <feed>0</feed>\n"
            "            <feed>1</feed>\n"
            "            <thruster file=\"direct\">\n"
            + written.location(560.0, 0.0, 0.0, "                ") +
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
    return out + "        </channel>\n    </flight_control>\n"


def lift_table(indent):
    """The lift by angle of attack and Mach: at each Mach the slope of the
    wing's (fighter.lift_slope), straight to STRAIGHT_ALPHA, rounding to its
    greatest at STALL_ALPHA and falling past it."""
    alphas = [-0.60, -STRAIGHT_ALPHA, 0.0, STRAIGHT_ALPHA, (STRAIGHT_ALPHA + STALL_ALPHA) / 2.0, STALL_ALPHA,
              0.80, 1.20, 1.57]

    def at_low_mach(a, slope):
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
            f"{at_low_mach(a, fighter.lift_slope(m, ASPECT, SWEEP_QUARTER_CHORD)):.4f}" for m in fighter.MACHS) + "\n"
    return (f"{indent}<table>\n"
            f"{indent}    <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
            f"{indent}    <independentVar lookup=\"column\">velocities/mach</independentVar>\n"
            f"{indent}    <tableData>\n{head}{rows}{indent}    </tableData>\n{indent}</table>\n")


def aerodynamics():
    t = "                    "
    w = written
    q, s, b, c = "aero/qbar-psf", "metrics/Sw-sqft", "metrics/bw-ft", "metrics/cbarw-ft"
    lift = [
        w.coefficient("CLalpha", "Lift_due_to_alpha", [q, s, "aero/function/kCLge", lift_table(t)]),
        w.coefficient("CLde", "Lift_due_to_stabilator", [q, s, "fcs/elevator-pos-rad", CL_DE]),
    ]
    drag = [fighter.drag_functions(SUBSONIC_ZERO_LIFT_DRAG, WAVE_DRAG, make_f15c.WAVE_DECAY, ASPECT,
                                   SPAN_EFFICIENCY, make_f15c.SUPERSONIC_LIFT_DRAG, SEPARATION_ALPHA),
            w.coefficient("CDgear", "Drag_due_to_gear", [q, s, "gear/gear-pos-norm", 0.02]),
            w.coefficient("CDbeta", "Drag_due_to_sideslip", [q, s, "aero/mag-beta-rad", 0.25]),
            w.coefficient("CDde", "Drag_due_to_stabilator", [q, s, "fcs/mag-elevator-pos-rad", 0.03])]
    side = [
        w.coefficient("CYb", "Side_force_due_to_beta", [q, s, "aero/beta-rad", CY_BETA]),
        w.coefficient("CYda", "Side_force_due_to_flaperons", [q, s, "fcs/left-aileron-pos-rad", 2.0 * CY_DA]),
        w.coefficient("CYdr", "Side_force_due_to_rudders", [q, s, "fcs/rudder-pos-rad", CY_DR]),
    ]
    roll = [
        w.coefficient("Clb", "Roll_moment_due_to_beta", [q, s, b, "aero/beta-rad", CL_BETA]),
        w.coefficient("Clp", "Roll_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CL_P]),
        w.coefficient("Clr", "Roll_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CL_R]),
        w.coefficient("Clda", "Roll_moment_due_to_flaperons", [q, s, b, "fcs/left-aileron-pos-rad", 2.0 * CL_DA]),
        w.coefficient("Cldr", "Roll_moment_due_to_rudders", [q, s, b, "fcs/rudder-pos-rad", CL_DR]),
    ]
    pitch = [
        w.coefficient("Cmalpha", "Pitch_moment_due_to_alpha", [q, s, c, "aero/alpha-rad", CM_ALPHA]),
        w.coefficient("Cmq", "Pitch_moment_due_to_pitch_rate", [q, s, c, "aero/ci2vel", "velocities/q-aero-rad_sec", CM_Q]),
        w.coefficient("Cmadot", "Pitch_moment_due_to_alpha_rate", [q, s, c, "aero/ci2vel", "aero/alphadot-rad_sec", CM_ADOT]),
        w.coefficient("Cmde", "Pitch_moment_due_to_stabilator", [q, s, c, "fcs/elevator-pos-rad", CM_DE]),
    ]
    yaw = [
        w.coefficient("Cnb", "Yaw_moment_due_to_beta", [q, s, b, "aero/beta-rad", CN_BETA]),
        w.coefficient("Cnp", "Yaw_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CN_P]),
        w.coefficient("Cnr", "Yaw_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CN_R]),
        w.coefficient("Cnda", "Yaw_moment_due_to_flaperons", [q, s, b, "fcs/left-aileron-pos-rad", 2.0 * CN_DA]),
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
            + metrics() + mass_balance() + ground_reactions() + propulsion() + flight_control()
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
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
