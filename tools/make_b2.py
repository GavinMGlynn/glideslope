#!/usr/bin/env python3
"""make_b2.py - glideslope's Northrop Grumman B-2A Spirit, written from what is published.

JSBSim has no B-2. This script writes one from what is public, which is less
than for any aircraft here: the Air Force's fact sheet gives its size, its
weights, its engines' thrust, and its speed, ceiling and range in words or
round numbers; nothing of its aerodynamics is published. Its figures, in
assets/figures/b2.xml, are those three; nothing more is claimed, and every
other number here is an estimate, named below.

    python3 tools/make_b2.py           write the files
    python3 tools/make_b2.py --check   exit 1 if what is committed differs

From the Air Force's B-2 Spirit fact sheet (current as of December 2015):
span 172 ft; length 69 ft; height 17 ft; weight 160,000 lb, taken as empty;
maximum take-off weight 336,500 lb; fuel 167,000 lb; four F118-GE-100s of
17,300 lb each (GE gives 19,000; the Air Force's are taken).

Estimated, each named below with its number: the wing's area and mean
aerodynamic chord, from its span and length, a leading edge swept 33 degrees
and a small tip chord, less the saw-tooth trailing edge's cut-outs; where the
chord, centre of gravity, tanks, engines and gear are; its inertias, from its
weight and size; its lift - DATCOM's slope for its wing - and drag at zero
lift, span efficiency and drag rise (Lock's, tools/airliner.py), set to fly
the speed, ceiling and range; its engines, JSBSim's
F100-PW-229 dry and made an F118 with the F100's thrust with height and speed
(tools/fighter.py, tools/make_f15c.py), its fuel consumption set to fly the
range; and every moment's derivative, a tailless swept wing's. A flying wing
has almost no directional stability of its own: the B-2's flight controls
give it, the drag rudders at its tips working against sideslip and yaw, and so
do this model's - the yaw damper and SIDESLIP_GAIN.
"""

import math
import sys

import airliner
import fighter
import make_f15c
import written
from airliner import OUT, PINNED

SCRIPT = "make_b2"
MODEL = "b2"
ENGINE = "F118-GE-100"

SPAN_FT = 172.0
LENGTH_FT = 69.0
TIP_CHORD_FT = 7.0
SWEEP_LEADING_EDGE = 33.0
SAWTOOTH_SHARE = 0.80              # of the trapezoid left by the trailing edge's cut-outs
AREA = SAWTOOTH_SHARE * SPAN_FT * (LENGTH_FT + TIP_CHORD_FT) / 2.0
ASPECT = SPAN_FT ** 2 / AREA
TAPER = TIP_CHORD_FT / LENGTH_FT
MAC_FT = SAWTOOTH_SHARE * 2.0 / 3.0 * LENGTH_FT * (1.0 + TAPER + TAPER ** 2) / (1.0 + TAPER)
MAC_Y_FT = SPAN_FT / 6.0 * (1.0 + 2.0 * TAPER) / (1.0 + TAPER)
LEMAC_IN = 12.0 * MAC_Y_FT * math.tan(math.radians(SWEEP_LEADING_EDGE))
SWEEP_QUARTER_CHORD = 28.0


def mac(fraction):
    return LEMAC_IN + fraction * MAC_FT * 12.0


EMPTY_LBS = 160000.0
FUEL_LBS = 167000.0
THRUST = "17300.0"
TSFC = 0.95
# Radii of gyration, as fractions of the span (roll, yaw) and length (pitch):
# a flying wing's weight is spread along its span.
RADII = (0.25 * SPAN_FT, 0.30 * LENGTH_FT, 0.26 * SPAN_FT)

# Lift and drag.
LIFT_STALL_ALPHA = 0.26
SUBSONIC_ZERO_LIFT_DRAG = 0.0080
SPAN_EFFICIENCY = 0.80
DRAG_DIVERGENCE = 0.84

# A tailless swept wing's derivatives, per radian, estimated.
CM_ALPHA, CM_Q, CM_DE, CL_DE = -0.20, -2.0, -0.40, 0.60
CY_BETA, CN_BETA, CL_BETA = -0.10, 0.010, -0.12
CL_P, CN_P, CL_R, CN_R = -0.45, -0.02, 0.10, -0.03
CL_DA, CN_DR = 0.12, -0.030
# The flight controls: elevons and drag rudders, and their augmentation.
ELEVON_DEG, RUDDER_DEG = 25.0, 45.0
PITCH_DAMPER_GAIN, YAW_DAMPER_GAIN, SIDESLIP_GAIN = 1.0, 3.0, 4.0


def metrics():
    return ("    <metrics>\n"
            f"        <wingarea unit=\"FT2\"> {AREA:.0f} </wingarea>\n"
            f"        <wingspan unit=\"FT\"> {SPAN_FT:.1f} </wingspan>\n"
            f"        <chord unit=\"FT\"> {MAC_FT:.2f} </chord>\n"
            + written.location(mac(0.25), 0.0, 0.0, "        ", "AERORP")
            + written.location(120.0, -20.0, 40.0, "        ", "EYEPOINT")
            + written.location(0.0, 0.0, 0.0, "        ", "VRP") +
            "    </metrics>\n")


def mass_balance():
    slugs = EMPTY_LBS / 32.174
    ixx, iyy, izz = (slugs * k * k for k in RADII)
    return ("    <mass_balance>\n"
            f"        <ixx unit=\"SLUG*FT2\"> {ixx:.4g} </ixx>\n"
            f"        <iyy unit=\"SLUG*FT2\"> {iyy:.4g} </iyy>\n"
            f"        <izz unit=\"SLUG*FT2\"> {izz:.4g} </izz>\n"
            f"        <emptywt unit=\"LBS\"> {EMPTY_LBS:.0f} </emptywt>\n"
            + written.location(mac(0.25), 0.0, 0.0, "        ", "CG") +
            "        <pointmass name=\"Crew\">\n"
            "            <weight unit=\"LBS\"> 0 </weight>\n"
            + written.location(140.0, 0.0, 30.0, "            ") +
            "        </pointmass>\n"
            "        <pointmass name=\"Weapons\">\n"
            "            <weight unit=\"LBS\"> 0 </weight>\n"
            + written.location(mac(0.25), 0.0, -20.0, "            ") +
            "        </pointmass>\n"
            "    </mass_balance>\n")


def ground_reactions():
    weight = 336500.0
    main_x = mac(0.25) + 40.0
    nose_x = 180.0
    nose_share = (main_x - mac(0.25)) / (main_x - nose_x)
    contact = -12.0 * 11.0
    out = "    <ground_reactions>\n"
    out += written.bogey("NOSE", nose_x, 0.0, contact, nose_share * weight / 0.8, nose_share * weight / 3.0,
                         5, "NONE", "0.80")
    for side, sign in (("LEFT", -1.0), ("RIGHT", 1.0)):
        out += written.bogey(f"{side}_MAIN", main_x, sign * 240.0, contact,
                             (1.0 - nose_share) / 2.0 * weight / 0.8, (1.0 - nose_share) / 2.0 * weight / 3.0,
                             0, side, "0.50")
    return out + "    </ground_reactions>\n"


def propulsion():
    out = "    <propulsion>\n"
    for y in (-150.0, -90.0, 90.0, 150.0):
        out += (f"        <engine file=\"{ENGINE}\">\n"
                + "".join(f"            <feed>{t}</feed>\n" for t in range(4)) +
                "            <thruster file=\"direct\">\n"
                + written.location(mac(0.60), y, 0.0, "                ") +
                "            </thruster>\n"
                "        </engine>\n")
    for y in (-400.0, -200.0, 200.0, 400.0):
        out += ("        <tank type=\"FUEL\">\n"
                + written.location(mac(0.25) + abs(y) * math.tan(math.radians(SWEEP_QUARTER_CHORD)) / 4.0, y, 0.0,
                                   "            ") +
                f"            <capacity unit=\"LBS\"> {FUEL_LBS / 4.0:.0f} </capacity>\n"
                f"            <contents unit=\"LBS\"> {FUEL_LBS / 4.0:.0f} </contents>\n"
                "        </tank>\n")
    return out + "    </propulsion>\n"


def flight_control():
    rad = math.radians
    out = "    <flight_control name=\"B-2A\">\n        <channel name=\"Controls\">\n"
    out += ("            <pure_gain name=\"Pitch Damper\">\n"
            "                <input>velocities/q-aero-rad_sec</input>\n"
            f"                <gain>{PITCH_DAMPER_GAIN}</gain>\n"
            "                <clipto><min>-0.3</min><max>0.3</max></clipto>\n"
            "            </pure_gain>\n")
    out += written.surface("Elevator", ["fcs/elevator-cmd-norm", "fcs/pitch-trim-cmd-norm", "fcs/pitch-damper"],
                           -rad(ELEVON_DEG), rad(ELEVON_DEG), "fcs/elevator-pos-rad")
    out += written.surface("Aileron", ["fcs/aileron-cmd-norm", "fcs/roll-trim-cmd-norm"], -rad(ELEVON_DEG),
                           rad(ELEVON_DEG), "fcs/left-aileron-pos-rad")
    out += ("            <pure_gain name=\"Right Aileron\">\n"
            "                <input>-fcs/left-aileron-pos-rad</input>\n"
            "                <gain>1</gain>\n"
            "                <output>fcs/right-aileron-pos-rad</output>\n"
            "            </pure_gain>\n")
    out += written.yaw_damper(YAW_DAMPER_GAIN)
    out += ("            <pure_gain name=\"Sideslip Feedback\">\n"
            "                <input>aero/beta-rad</input>\n"
            f"                <gain>{-SIDESLIP_GAIN}</gain>\n"
            "                <clipto><min>-0.5</min><max>0.5</max></clipto>\n"
            "            </pure_gain>\n")
    out += written.surface("Rudder", ["fcs/rudder-cmd-norm", "fcs/yaw-trim-cmd-norm", "fcs/yaw-damper",
                                      "fcs/sideslip-feedback"], -rad(RUDDER_DEG), rad(RUDDER_DEG),
                           "fcs/rudder-pos-rad")
    out += written.kinematic("Gear", "gear/gear-cmd-norm", [0, 1], [0, 8], "gear/gear-pos-norm")
    return out + "        </channel>\n    </flight_control>\n"


def lift_table(indent):
    alphas = [-0.40, 0.0, LIFT_STALL_ALPHA, LIFT_STALL_ALPHA + 0.12, 0.80, 1.57]

    def at_low_mach(a, slope):
        peak = slope * LIFT_STALL_ALPHA
        points = [(-0.40, -slope * 0.40), (0.0, 0.0), (LIFT_STALL_ALPHA, peak),
                  (LIFT_STALL_ALPHA + 0.12, 0.8 * peak), (0.80, 0.6 * peak), (1.57, 0.0)]
        for (a0, c0), (a1, c1) in zip(points, points[1:]):
            if a0 <= a <= a1:
                return c0 + (c1 - c0) * (a - a0) / (a1 - a0)
        return 0.0

    head = f"{indent}            \t" + "\t".join(f"{m:.2f}" for m in fighter.MACHS[:12]) + "\n"
    rows = "".join(f"{indent}        {a:.4f}\t" + "\t".join(
        f"{at_low_mach(a, fighter.lift_slope(m, ASPECT, SWEEP_QUARTER_CHORD)):.4f}" for m in fighter.MACHS[:12]) + "\n"
        for a in alphas)
    return (f"{indent}<table>\n"
            f"{indent}    <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
            f"{indent}    <independentVar lookup=\"column\">velocities/mach</independentVar>\n"
            f"{indent}    <tableData>\n{head}{rows}{indent}    </tableData>\n{indent}</table>\n")


def aerodynamics():
    t = "                    "
    w = written
    q, s, b, c = "aero/qbar-psf", "metrics/Sw-sqft", "metrics/bw-ft", "metrics/cbarw-ft"
    induced = 1.0 / (math.pi * ASPECT * SPAN_EFFICIENCY)
    mach_rows = [tuple(r.split("\t")) for r in airliner.mach_drag_rows(DRAG_DIVERGENCE, "").split("\n")]
    lift = [
        w.coefficient("CLalpha", "Lift_due_to_alpha", [q, s, "aero/function/kCLge", lift_table(t)]),
        w.coefficient("CLde", "Lift_due_to_elevons", [q, s, "fcs/elevator-pos-rad", CL_DE]),
    ]
    drag = [
        w.coefficient("CD0", "Drag_at_zero_lift", [q, s, w.table1("aero/alpha-rad", [
            (-1.57, 1.3), (-0.26, 0.05), (0.0, SUBSONIC_ZERO_LIFT_DRAG), (0.20, SUBSONIC_ZERO_LIFT_DRAG),
            (0.40, 0.15), (1.57, 1.4)], t)]),
        w.coefficient("CDi", "Induced_drag", [q, s, "aero/cl-squared", "aero/function/kCDge", induced]),
        w.coefficient("CDmach", "Drag_due_to_mach", [q, s, w.table1("velocities/mach", mach_rows, t)]),
        w.coefficient("CDgear", "Drag_due_to_gear", [q, s, "gear/gear-pos-norm", 0.006]),
        w.coefficient("CDrudders", "Drag_of_the_drag_rudders", [q, s, "fcs/mag-rudder-pos-rad", 0.02]),
        w.coefficient("CDbeta", "Drag_due_to_sideslip", [q, s, "aero/mag-beta-rad", 0.10]),
        w.coefficient("CDde", "Drag_due_to_elevons", [q, s, "fcs/mag-elevator-pos-rad", 0.01]),
    ]
    side = [w.coefficient("CYb", "Side_force_due_to_beta", [q, s, "aero/beta-rad", CY_BETA])]
    roll = [
        w.coefficient("Clb", "Roll_moment_due_to_beta", [q, s, b, "aero/beta-rad", CL_BETA]),
        w.coefficient("Clp", "Roll_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CL_P]),
        w.coefficient("Clr", "Roll_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CL_R]),
        w.coefficient("Clda", "Roll_moment_due_to_elevons", [q, s, b, "fcs/left-aileron-pos-rad", 2.0 * CL_DA]),
    ]
    pitch = [
        w.coefficient("Cmalpha", "Pitch_moment_due_to_alpha", [q, s, c, "aero/alpha-rad", CM_ALPHA]),
        w.coefficient("Cmq", "Pitch_moment_due_to_pitch_rate", [q, s, c, "aero/ci2vel", "velocities/q-aero-rad_sec", CM_Q]),
        w.coefficient("Cmde", "Pitch_moment_due_to_elevons", [q, s, c, "fcs/elevator-pos-rad", CM_DE]),
    ]
    yaw = [
        w.coefficient("Cnb", "Yaw_moment_due_to_beta", [q, s, b, "aero/beta-rad", CN_BETA]),
        w.coefficient("Cnp", "Yaw_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CN_P]),
        w.coefficient("Cnr", "Yaw_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CN_R]),
        w.coefficient("Cndr", "Yaw_moment_due_to_drag_rudders", [q, s, b, "fcs/rudder-pos-rad", CN_DR]),
    ]
    return w.axes((("LIFT", lift), ("DRAG", drag), ("SIDE", side), ("ROLL", roll), ("PITCH", pitch), ("YAW", yaw)))


def airframe():
    return ("<?xml version=\"1.0\"?>\n"
            "<fdm_config name=\"Northrop Grumman B-2A\" version=\"2.0\" release=\"BETA\">\n"
            "    <fileheader>\n"
            "        <!-- glideslope: written by tools/make_b2.py, which says where each number\n"
            "             comes from. Do not edit it by hand. -->\n"
            "        <author>glideslope</author>\n"
            "        <filecreationdate>2026-09-19</filecreationdate>\n"
            "        <description>Northrop Grumman B-2A Spirit, F118-GE-100</description>\n"
            "    </fileheader>\n"
            + metrics() + mass_balance() + ground_reactions() + propulsion() + flight_control()
            + aerodynamics() + "</fdm_config>\n")


def engine():
    text = (PINNED / "engine" / "F100-PW-229.xml").read_text()
    text = airliner.replace_once(text, r"(<turbine_engine name=\")F100(\">)",
                                 r"\g<1>" + ENGINE + r"\2\n  <!-- glideslope: JSBSim's F100-PW-229 made the B-2's F118\n"
                                 r"       by tools/make_b2.py, which made it. Do not edit it by hand. -->",
                                 "the engine's name", SCRIPT)
    text = airliner.replace_once(text, r"<milthrust>\s*17800\.0\s*</milthrust>",
                                 f"<milthrust>   {THRUST} </milthrust>", "the thrust", SCRIPT)
    text = airliner.replace_once(text, r"<maxthrust>\s*29000\.0\s*</maxthrust>",
                                 f"<maxthrust>   {THRUST} </maxthrust>", "the maximum thrust", SCRIPT)
    text = airliner.replace_once(text, r"<augmented>\s*1\s*</augmented>", "<augmented>         0 </augmented>",
                                 "the afterburner", SCRIPT)
    text = airliner.replace_once(text, r"<tsfc>\s*0\.74\s*</tsfc>", f"<tsfc>            {TSFC:.2f} </tsfc>",
                                 "the fuel consumption", SCRIPT)
    idle = fighter.table(text, "IdleThrust")
    f15 = make_f15c
    text = airliner.replace_once(
        text, r'(<function name="MilThrust">\s*<table>.*?<tableData>\s*\n).*?(\n\s*</tableData>)',
        lambda m: m.group(1) + fighter.thrust_table(f15.MILITARY_EXPONENT, f15.MILITARY_THROTTLE_RATIO,
                                                    f15.MILITARY_FALL, f15.HIGH_LOSS, idle) + m.group(2),
        "the MilThrust table", SCRIPT)
    return airliner.replace_once(
        text, r'(<function name="IdleThrust">\s*<table>.*?<tableData>\s*\n).*?(\n\s*</tableData>)',
        lambda m: m.group(1) + fighter.idle_table(idle) + m.group(2), "the IdleThrust table", SCRIPT)


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
