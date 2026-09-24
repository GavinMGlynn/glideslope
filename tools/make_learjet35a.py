#!/usr/bin/env python3
"""make_learjet35a.py - glideslope's Gates Learjet 35A, written from published data.

JSBSim has no Learjet. This script writes one: the airframe from its FAA-
approved flight manual and type certificate; its aerodynamics from NASA's
measurements of its forebear, the Learjet 23 - a full-scale wind tunnel
test and a flight identification - and Gates Learjet's own drag analysis of
the Learjet 25; and its engines from JSBSim's Tay 620 made a TFE731-2. Its
figures are its flight manual's, in assets/figures/learjet35a.xml.

    python3 tools/make_learjet35a.py           write the files
    python3 tools/make_learjet35a.py --check   exit 1 if what is committed differs

Where each number comes from:

  Gates Learjet 35A/36A Airplane Flight Manual, FM-108, change 23 (the AFM)
    The model's frame is the AFM's: fuselage stations, butt and water lines,
    in inches. The mean aerodynamic chord, 82.75 in, its leading edge at
    station 362.17 (figure 3, WB-4, and A10CE); the gear, the nose wheel at
    station 153.63 and the mains at 396.83, their axles at water lines -17.98
    and -16.72 at rest (WB-4); the fuel: 357 gallons in the tip tanks at
    station 385.6, 374 in the wings at 385.8 and 200 in the fuselage at 440.2
    (WB-17 and A10CE), at 6.7 lb a gallon; the flaps' settings, 8 and 20
    degrees for take-off and 40 for landing (5-1); the yaw damper (1-14B);
    and the horizontal stabilizer's take-off setting for the centre of
    gravity, 7.6 degrees nose up at 5% of the chord to 7.2 at 20% and 5.0 at
    28% and aft (figure 2-2, TAKEOFF TRIM - CG FUNCTION), which is also the
    most nose-up setting it names.
    Its stall speeds (figure 5-11) set the lift's maximum at each flap
    setting, which the figures then fly.
  FAA type certificate data sheet A10CE, revision 67
    The TFE731-2-2B's take-off thrust, 3,500 lb; the controls' travel:
    elevator 16 degrees up and 15 down, ailerons 18 each way, rudder 30,
    spoilers 40.
  NTSB, WPR22FA068 (Learjet 35A N880Z), operational factors factual report
    That 35A's basic empty weight, 10,173 lb, which the AFM leaves for each
    aircraft's records; and its figure 2, the span between the tip tanks'
    centres, 38 ft 1 in.
  NASA TN D-6573, Soderman and Aiken, Full-Scale Wind-Tunnel Tests of a
  Small Unpowered Jet Aircraft with a T-Tail (1971) - a Learjet 23
    The wing's planform - its area 0.9655 of its span times its mean chord,
    which with the 35A's span and chord makes the 35A's area, 253.5 sq ft,
    which no 35A document gives; the flaps' lift, 0.015 a degree at 20 and
    0.013 at 38; the full spoiler's 80% more drag; and, per radian, the
    lateral and directional derivatives it measured: the rolling moment of
    sideslip, -0.097 at no incidence to -0.149 at 14 degrees, and the yawing
    moment, 0.143 to 0.166 (figure 30); each aileron's rolling moment, 0.080
    (figure 25); the rudder's yawing moment, -0.0745 (figure 27).
  and, from its figure 7 - the same elevator settings at tail incidences of
    0.4 and -7 degrees, flaps up - the stabilizer's pitching moment, 0.0368 a
    degree, 2.11 a radian: the pitching moment at no incidence goes from 0.051
    to 0.323 as the stabilizer goes from 0.4 to -7. The stabilizer's travel
    nose down is the tunnel aircraft's, 0.4 degrees.
  NASA TN D-7647, Wingrove's identification of a Lear Jet's longitudinal
  coefficients from flight (1974), table II
    The lift slope, 5.12; the lift and pitching moment at no incidence, 0.111
    and 0.066; the pitching moments of incidence, -0.810, elevator, -1.036,
    and pitch rate, -16.46; the elevator's lift, 0.342.
  Ross and Neal, of Gates Learjet, Learjet Model 25 Drag Analysis, in the
  NASA/Industry/University General Aviation Drag Reduction Workshop (1975),
  figure 6
    The drag at zero lift, 0.0210 at Mach 0.60, and the drag due to lift: the
    induced drag with a span efficiency of 1, and the profile drag's rise with
    lift, which together make one of 0.906.
  USAF C-21A fact sheet
    Mach 0.81, 461 knots, at 41,000 ft.

Estimated, as no source gives them, each named below with its number: the
empty aircraft's centre of gravity, its inertias, the positions of the
engines and the payload, the damping in roll and yaw and the side force, the
engines' thrust with height and speed, the drag rise's Mach, the gear's
springs and the wheels' size, and the fan's, for a stopped engine's drag.
"""

import math
import sys

import airliner
import fighter
import written
from airliner import OUT, PINNED

SCRIPT = "make_learjet35a"
MODEL = "learjet35a"
ENGINE = "TFE731-2"

# The wing: the span between the tip tanks' centres, the mean aerodynamic
# chord and its leading edge (inches), and the area its planform makes.
SPAN_FT = 38.0 + 1.0 / 12.0
MAC_IN = 82.75
LEMAC = 362.17
AREA = 0.9655 * SPAN_FT * MAC_IN / 12.0
ASPECT = SPAN_FT ** 2 / AREA


def mac(fraction):
    return LEMAC + fraction * MAC_IN


# Weights: the NTSB's basic empty weight, its centre of gravity estimated at
# 32% of the chord - the AFM warns it may be aft of the flight limit, 30%;
# the payload in the cabin, at station 300, its middle.
EMPTY_LBS = 10173.0
EMPTY_CG = 0.32
PAYLOAD_STATION = 300.0
# Inertias: the radii of gyration of Calspan's Learjet 25 model (AFIT thesis
# ADA424733, table 1 - the one published, if of low confidence), roll's
# scaled by the 35A's span over the 25's, TN D-6573's 34.1 ft, and yaw as the
# root of roll's and pitch's squares, as the 25's is to within 4%.
RADII_25_FT = (6.90, 7.39)
# The fuel, at 6.7 lb a gallon.
TIP_LBS = 357.0 / 2.0 * 6.7
WING_LBS = 374.0 / 2.0 * 6.7
FUSELAGE_LBS = 200.0 * 6.7

# The engines: TFE731-2, 3,500 lb; their thrust with speed and height
# (fighter.thrust_lapse) - Mattingly's form, set to fly the cruise figure.
THRUST_LB = 3500.0
LAPSE_EXPONENT = 0.0
LAPSE_THROTTLE_RATIO = 1.0
LAPSE_FALL = 3.8
ENGINE_STATION = 470.0
ENGINE_BUTT = 42.0
ENGINE_WATER = 22.0
FAN_IN = 27.3

# The lift: at no incidence and its slope (TN D-7647); the flaps' lift by
# TN D-6573's per-degree effectiveness; and each setting's greatest, from the
# AFM's stall speeds at 18,300 lb - 130, 116.5, 112 and 105.5 knots.
ZERO_ALPHA_LIFT = 0.111
LIFT_SLOPE = 5.12
FLAP_DEGREES = [0, 8, 20, 40]
FLAP_LIFT = [0.0, 0.015 * 8, 0.015 * 20, 0.013 * 40]
MAX_LIFT = [1.262, 1.571, 1.700, 1.916]
# The drag: at zero lift, the 25's; the span efficiency; the flaps', gear's and
# drag rise's, set here.
ZERO_LIFT_DRAG = 0.0210
SPAN_EFFICIENCY = 0.906
FLAP_DRAG = [0.0, 0.010, 0.030, 0.080]
GEAR_DRAG = 0.020
DRAG_DIVERGENCE = 0.77
# Pitch: TN D-7647's, per radian; the pitching moment of the flaps set here.
CL_DE, CL_Q = 0.342, 7.0     # the lift of pitch rate: the tunnel's tail's
CM_ZERO, CM_ALPHA, CM_DE, CM_Q, CM_ADOT = 0.066, -0.810, -1.036, -16.46, -5.0
FLAP_PITCH = -0.03           # at 40 degrees
# **The trimmable horizontal stabilizer.** The pitch trim moves it, not the
# elevator: the 35A trims by its stabilizer, and is set for take-off by it
# (the AFM's figure 2-2). Its pitching moment a radian (TN D-6573 figure 7),
# its lift by the elevator's ratio of lift to moment - the same tail at the
# same arm - and its travel: nose up to the AFM's greatest take-off setting,
# nose down to the tunnel aircraft's. Its rate is estimated: its whole travel
# in sixteen seconds, half a degree a second.
#
# **Why it is here.** With the pitch trim on the elevator, and no take-off
# setting, the elevator alone had to lift the nose wheel, and could not until
# 132 knots with the stick fully back - its rotation speed is 125 - so she
# left the runway at 152 by the book, and pulled early no sooner. Set, she
# leaves at 129 by the book and at 114 pulled early.
CM_STAB = -2.11
CL_STAB = CL_DE * CM_STAB / CM_DE
STAB_NOSE_UP_DEG, STAB_NOSE_DOWN_DEG = 7.6, 0.4
STAB_TRAVEL_S = 16.0
# The AFM's figure 2-2: the take-off setting, degrees nose up, by the centre
# of gravity's place along the chord.
TAKEOFF_TRIM = [(0.05, 7.6), (0.20, 7.2), (0.28, 5.0), (0.30, 5.0)]
# Lateral: TN D-6573's; the damping and side force estimated (see above).
CL_BETA = [(0.0, -0.097), (0.14, -0.120), (0.244, -0.149)]
CN_BETA = [(0.0, 0.143), (0.14, 0.143), (0.244, 0.166)]
CL_DA = 0.080                # a side's, per radian of it
CN_DR = -0.0745
CY_BETA, CY_DR, CL_DR = -0.76, 0.14, 0.020
CL_P, CL_R, CN_P, CN_R = -0.50, 0.10, -0.03, -0.18
YAW_DAMPER_GAIN = 1.0
BRAKING_FRICTION = "0.50"


def metrics():
    return ("    <metrics>\n"
            f"        <wingarea unit=\"FT2\"> {AREA:.1f} </wingarea>\n"
            f"        <wingspan unit=\"FT\"> {SPAN_FT:.2f} </wingspan>\n"
            f"        <chord unit=\"FT\"> {MAC_IN / 12.0:.3f} </chord>\n"
            + written.location(mac(0.25), 0.0, -5.0, "        ", "AERORP")
            + written.location(150.0, -12.0, 30.0, "        ", "EYEPOINT")
            + written.location(86.75, 0.0, 0.0, "        ", "VRP") +
            "    </metrics>\n")


def mass_balance():
    slugs = EMPTY_LBS / 32.174
    kx = RADII_25_FT[0] * SPAN_FT / 34.1
    ky = RADII_25_FT[1]
    kz = math.hypot(kx, ky)
    ixx, iyy, izz = (slugs * k * k for k in (kx, ky, kz))
    return ("    <mass_balance>\n"
            f"        <ixx unit=\"SLUG*FT2\"> {ixx:.0f} </ixx>\n"
            f"        <iyy unit=\"SLUG*FT2\"> {iyy:.0f} </iyy>\n"
            f"        <izz unit=\"SLUG*FT2\"> {izz:.0f} </izz>\n"
            f"        <ixz unit=\"SLUG*FT2\"> {0.05 * izz:.0f} </ixz>\n"
            f"        <emptywt unit=\"LBS\"> {EMPTY_LBS:.0f} </emptywt>\n"
            + written.location(mac(EMPTY_CG), 0.0, 0.0, "        ", "CG") +
            "        <pointmass name=\"Payload\">\n"
            "            <weight unit=\"LBS\"> 0 </weight>\n"
            + written.location(PAYLOAD_STATION, 0.0, 0.0, "            ") +
            "        </pointmass>\n"
            "    </mass_balance>\n")


def ground_reactions():
    # The wheels' contact 9 in below their axles, and 2.5 in lower with the
    # struts extended; the springs compressing that under the weight each
    # carries at 18,300 lb, and damped near critical; the nose wheel steering
    # 5 degrees with the pedals, as JSBSim's airliners' does.
    weight = 18300.0
    nose_share = (396.83 - mac(0.24)) / (396.83 - 153.63)
    out = "    <ground_reactions>\n"
    out += written.bogey("NOSE", 153.63, 0.0, -17.98 - 9.0 - 2.5, nose_share * weight / 0.21,
                         nose_share * weight / 0.8, 5, "NONE", "0.80")
    for side, sign in (("LEFT", -1.0), ("RIGHT", 1.0)):
        out += written.bogey(f"{side}_MAIN", 396.83, sign * 49.5, -16.72 - 9.0 - 2.5,
                             (1.0 - nose_share) / 2.0 * weight / 0.21, (1.0 - nose_share) / 2.0 * weight / 0.8,
                             0, side, BRAKING_FRICTION)
    for name, y in (("LEFT_TIP_TANK", -SPAN_FT * 6.0), ("RIGHT_TIP_TANK", SPAN_FT * 6.0)):
        out += written.structure(name, 385.6, y, -8.0, weight, weight / 5.0)
    return out + "    </ground_reactions>\n"


def propulsion():
    out = "    <propulsion>\n"
    for y in (-ENGINE_BUTT, ENGINE_BUTT):
        out += (f"        <engine file=\"{ENGINE}\">\n"
                + "".join(f"            <feed>{t}</feed>\n" for t in range(5)) +
                "            <thruster file=\"direct\">\n"
                + written.location(ENGINE_STATION, y, ENGINE_WATER, "                ") +
                "            </thruster>\n"
                "        </engine>\n")
    tanks = [(-SPAN_FT * 6.0, 385.6, TIP_LBS), (SPAN_FT * 6.0, 385.6, TIP_LBS),
             (-90.0, 385.8, WING_LBS), (90.0, 385.8, WING_LBS), (0.0, 440.2, FUSELAGE_LBS)]
    for y, x, lbs in tanks:
        out += ("        <tank type=\"FUEL\">\n"
                + written.location(x, y, -4.0, "            ") +
                f"            <capacity unit=\"LBS\"> {lbs:.0f} </capacity>\n"
                f"            <contents unit=\"LBS\"> {lbs:.0f} </contents>\n"
                "        </tank>\n")
    return out + "    </propulsion>\n"


def flight_control():
    rad = math.radians
    out = "    <flight_control name=\"Learjet 35A\">\n        <channel name=\"Controls\">\n"
    out += written.surface("Elevator", ["fcs/elevator-cmd-norm"], -rad(16), rad(15), "fcs/elevator-pos-rad")
    # The stabilizer, run by the pitch trim at its own rate; the pitch trim's
    # -1 is its full nose-up travel, as the elevator's command's is.
    out += written.kinematic("Stabilizer Trim", "fcs/pitch-trim-cmd-norm", [-1, 1], [0, STAB_TRAVEL_S],
                             "fcs/stabilizer-trim-norm")
    out += ("            <aerosurface_scale name=\"Stabilizer\">\n"
            "                <input>fcs/stabilizer-trim-norm</input>\n"
            f"                <range><min>{-rad(STAB_NOSE_UP_DEG):.4f}</min><max>{rad(STAB_NOSE_DOWN_DEG):.4f}</max></range>\n"
            "                <output>fcs/stabilizer-pos-rad</output>\n"
            "            </aerosurface_scale>\n")
    # The AFM's take-off setting for where the centre of gravity is, as a
    # pitch trim - positive nose up, as glideslope's controls have it - for
    # whoever sets it before take-off.
    rows = "".join(f"                            {mac(f):.2f}\t{deg / STAB_NOSE_UP_DEG:.4f}\n"
                   for f, deg in TAKEOFF_TRIM)
    out += ("            <fcs_function name=\"Pitch Trim Takeoff Norm\">\n"
            "                <function>\n"
            "                    <table>\n"
            "                        <independentVar>inertia/cg-x-in</independentVar>\n"
            f"                        <tableData>\n{rows}                        </tableData>\n"
            "                    </table>\n"
            "                </function>\n"
            "            </fcs_function>\n")
    out += written.surface("Aileron", ["fcs/aileron-cmd-norm", "fcs/roll-trim-cmd-norm"], -rad(18), rad(18),
                           "fcs/left-aileron-pos-rad")
    out += ("            <pure_gain name=\"Right Aileron\">\n"
            "                <input>-fcs/left-aileron-pos-rad</input>\n"
            "                <gain>1</gain>\n"
            "                <output>fcs/right-aileron-pos-rad</output>\n"
            "            </pure_gain>\n")
    out += written.yaw_damper(YAW_DAMPER_GAIN)
    out += written.surface("Rudder", ["fcs/rudder-cmd-norm", "fcs/yaw-trim-cmd-norm", "fcs/yaw-damper"], -rad(30),
                           rad(30), "fcs/rudder-pos-rad")
    out += written.kinematic("Flaps", "fcs/flap-cmd-norm", FLAP_DEGREES, [0, 3, 4, 6], "fcs/flap-pos-deg")
    out += written.kinematic("Gear", "gear/gear-cmd-norm", [0, 1], [0, 5], "gear/gear-pos-norm")
    out += written.kinematic("Spoilers", "fcs/speedbrake-cmd-norm", [0, 1], [0, 2], "fcs/speedbrake-pos-norm")
    return out + "        </channel>\n    </flight_control>\n"


def lift_table(indent):
    """The lift by angle of attack and flaps: the slope from the lift at no
    incidence and each setting's flap lift, to its greatest, and past it a
    fall."""
    curves = []
    for lift, peak in zip(FLAP_LIFT, MAX_LIFT):
        base = ZERO_ALPHA_LIFT + lift
        stall = (peak - base) / LIFT_SLOPE
        curves.append([(-0.30, base - LIFT_SLOPE * 0.30), (0.0, base), (stall, peak), (stall + 0.10, peak - 0.35),
                       (0.60, 0.80), (1.57, 0.0)])

    def at(points, alpha):
        for (a0, c0), (a1, c1) in zip(points, points[1:]):
            if a0 <= alpha <= a1:
                return c0 + (c1 - c0) * (alpha - a0) / (a1 - a0)
        return points[-1][1]

    alphas = sorted({round(a, 4) for c in curves for a, _ in c} | {-1.57})
    head = f"{indent}            \t" + "\t".join(str(d) for d in FLAP_DEGREES) + "\n"
    rows = "".join(f"{indent}        {a:.4f}\t" + "\t".join(f"{at(c, a):.4f}" for c in curves) + "\n"
                   for a in alphas)
    return (f"{indent}<table>\n"
            f"{indent}    <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
            f"{indent}    <independentVar lookup=\"column\">fcs/flap-pos-deg</independentVar>\n"
            f"{indent}    <tableData>\n{head}{rows}{indent}    </tableData>\n{indent}</table>\n")


def aerodynamics():
    t = "                    "
    w = written
    q, s, b, c = "aero/qbar-psf", "metrics/Sw-sqft", "metrics/bw-ft", "metrics/cbarw-ft"
    induced = 1.0 / (math.pi * ASPECT * SPAN_EFFICIENCY)
    mach_rows = [tuple(r.split("\t")) for r in airliner.mach_drag_rows(DRAG_DIVERGENCE, "").split("\n")]
    lift = [
        w.coefficient("CLalpha", "Lift_due_to_alpha_and_flaps", [q, s, "aero/function/kCLge", lift_table(t)]),
        w.coefficient("CLq", "Lift_due_to_pitch_rate", [q, s, "aero/ci2vel", "velocities/q-aero-rad_sec", CL_Q]),
        w.coefficient("CLde", "Lift_due_to_elevator", [q, s, "fcs/elevator-pos-rad", CL_DE]),
        w.coefficient("CLstab", "Lift_due_to_stabilizer", [q, s, "fcs/stabilizer-pos-rad", CL_STAB]),
        w.coefficient("CLspoilers", "Lift_lost_to_spoilers", [q, s, "fcs/speedbrake-pos-norm", -0.15]),
    ]
    drag = [
        w.coefficient("CD0", "Drag_at_zero_lift", [q, s, w.table1("aero/alpha-rad", [
            (-1.57, 1.5), (-0.26, 0.05), (0.0, ZERO_LIFT_DRAG), (0.22, ZERO_LIFT_DRAG), (0.35, 0.10),
            (1.57, 1.6)], t)]),
        w.coefficient("CDi", "Induced_drag", [q, s, "aero/cl-squared", "aero/function/kCDge", induced]),
        w.coefficient("CDflaps", "Drag_due_to_flaps", [q, s, w.table1("fcs/flap-pos-deg",
                                                                      list(zip(FLAP_DEGREES, FLAP_DRAG)), t)]),
        w.coefficient("CDgear", "Drag_due_to_gear", [q, s, "gear/gear-pos-norm", GEAR_DRAG]),
        w.coefficient("CDmach", "Drag_due_to_mach", [q, s, w.table1("velocities/mach", mach_rows, t)]),
        w.coefficient("CDspoilers", "Drag_due_to_spoilers", [q, s, "fcs/speedbrake-pos-norm", 0.8 * ZERO_LIFT_DRAG]),
        w.coefficient("CDbeta", "Drag_due_to_sideslip", [q, s, "aero/mag-beta-rad", 0.25]),
        w.coefficient("CDde", "Drag_due_to_elevator", [q, s, "fcs/mag-elevator-pos-rad", 0.03]),
    ]
    side = [
        w.coefficient("CYb", "Side_force_due_to_beta", [q, s, "aero/beta-rad", CY_BETA]),
        w.coefficient("CYdr", "Side_force_due_to_rudder", [q, s, "fcs/rudder-pos-rad", CY_DR]),
    ]
    roll = [
        w.coefficient("Clb", "Roll_moment_due_to_beta", [q, s, b, "aero/beta-rad", w.table1("aero/alpha-rad", CL_BETA, t)]),
        w.coefficient("Clp", "Roll_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CL_P]),
        w.coefficient("Clr", "Roll_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CL_R]),
        w.coefficient("Clda", "Roll_moment_due_to_ailerons", [q, s, b, "fcs/left-aileron-pos-rad", 2.0 * CL_DA]),
        w.coefficient("Cldr", "Roll_moment_due_to_rudder", [q, s, b, "fcs/rudder-pos-rad", CL_DR]),
    ]
    pitch = [
        w.coefficient("Cm0", "Pitch_moment_at_zero_lift", [q, s, c, CM_ZERO]),
        w.coefficient("Cmalpha", "Pitch_moment_due_to_alpha", [q, s, c, "aero/alpha-rad", CM_ALPHA]),
        w.coefficient("Cmq", "Pitch_moment_due_to_pitch_rate", [q, s, c, "aero/ci2vel", "velocities/q-aero-rad_sec", CM_Q]),
        w.coefficient("Cmadot", "Pitch_moment_due_to_alpha_rate", [q, s, c, "aero/ci2vel", "aero/alphadot-rad_sec", CM_ADOT]),
        w.coefficient("Cmde", "Pitch_moment_due_to_elevator", [q, s, c, "fcs/elevator-pos-rad", CM_DE]),
        w.coefficient("Cmstab", "Pitch_moment_due_to_stabilizer", [q, s, c, "fcs/stabilizer-pos-rad", CM_STAB]),
        w.coefficient("Cmflaps", "Pitch_moment_due_to_flaps", [q, s, c, w.table1("fcs/flap-pos-deg", [
            (d, FLAP_PITCH * d / 40.0) for d in FLAP_DEGREES], t)]),
    ]
    yaw = [
        w.coefficient("Cnb", "Yaw_moment_due_to_beta", [q, s, b, "aero/beta-rad", w.table1("aero/alpha-rad", CN_BETA, t)]),
        w.coefficient("Cnp", "Yaw_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CN_P]),
        w.coefficient("Cnr", "Yaw_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CN_R]),
        w.coefficient("Cndr", "Yaw_moment_due_to_rudder", [q, s, b, "fcs/rudder-pos-rad", CN_DR]),
    ]
    windmill = airliner.windmill_function(2, FAN_IN, 0.4, "            ") + "\n"
    return w.axes((("LIFT", lift), ("DRAG", drag + [windmill]), ("SIDE", side), ("ROLL", roll),
                   ("PITCH", pitch), ("YAW", yaw)))


def airframe():
    return ("<?xml version=\"1.0\"?>\n"
            "<fdm_config name=\"Gates Learjet 35A\" version=\"2.0\" release=\"BETA\">\n"
            "    <fileheader>\n"
            "        <!-- glideslope: written by tools/make_learjet35a.py, which says where each\n"
            "             number comes from. Do not edit it by hand. -->\n"
            "        <author>glideslope</author>\n"
            "        <filecreationdate>2026-09-19</filecreationdate>\n"
            "        <description>Gates Learjet 35A, TFE731-2-2B</description>\n"
            "    </fileheader>\n"
            + metrics() + mass_balance() + ground_reactions() + propulsion() + flight_control()
            + aerodynamics() + "</fdm_config>\n")


def engine():
    text = (PINNED / "engine" / "Tay-620.xml").read_text()
    text = airliner.replace_once(text, r"<turbine_engine name=\"Tay-620\">",
                                 f"<turbine_engine name=\"{ENGINE}\">\n"
                                 "  <!-- glideslope: JSBSim's Tay-620 made the Learjet 35A's TFE731-2 by\n"
                                 "       tools/make_learjet35a.py, which made it. Do not edit it by hand. -->",
                                 "the engine's name", SCRIPT)
    text = airliner.replace_once(text, r"<milthrust>\s*13847\.7\s*</milthrust>",
                                 f"<milthrust> {THRUST_LB:.0f} </milthrust>", "the thrust", SCRIPT)
    idle = fighter.table(text, "IdleThrust")
    for name, rows in (("IdleThrust", fighter.idle_table(idle)),
                       ("MilThrust", fighter.thrust_table(LAPSE_EXPONENT, LAPSE_THROTTLE_RATIO, LAPSE_FALL,
                                                          0.0, idle))):
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
