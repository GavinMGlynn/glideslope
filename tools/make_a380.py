#!/usr/bin/env python3
"""make_a380.py - glideslope's Airbus A380-841, written from published data.

JSBSim has no A380. This script writes one: the airframe from Airbus's and
the certifying authorities' documents, its aerodynamics from the one
aircraft of its kind whose stability derivatives are published - the Boeing
747, in NASA CR-2144 - and its engines from JSBSim's Trent 900 made a Trent
970-84. Its figures are Airbus's A380 Aircraft Characteristics - Airport and
Maintenance Planning (revision 20, December 2025) and the EASA and FAA type
certificate data sheets, in assets/figures/a380.xml.

    python3 tools/make_a380.py           write the files
    python3 tools/make_a380.py --check   exit 1 if what is committed differs

Where each number comes from:

  Airbus, A380 Aircraft Characteristics (the AC), revision 20, 1 December 2025
    Span 79.75 m and length 72.73 m (figure 2-2-0-991-001, sheet 1); the wing
    tip's chord, 3.98 m, and its leading edge, 46.97 m behind the nose (sheet
    2); the gear - nose gear 4.97 m behind the nose, the wing and body gears
    28.605 m and 31.881 m behind it, their centres 12.456 m and 5.264 m apart,
    their axles 1.70 m and 1.53 m apart (figure 7-2-0-991-003); the height of
    the fuselage datum line, 5.15 m above the ground at the maximum ramp weight
    (figure 2-3-0-991-001: 7.20 m jacked, the doors 2.05 m lower on the gear);
    the engines' inlets 22.23 m and 29.94 m behind the nose, the inboard and
    outboard pairs 29.6 m and 51.4 m apart; the fuel, 253,983 kg; and the
    operating empty weight, 285,000 kg, which the AC does not give: its
    payload-range chart (figure 3-2-1-991-001) for the 575 t variant carries a
    structural payload of 84 t to the zero-fuel weight of 369 t.
  Airbus, A380 Facts and Figures, February 2022
    The wing's reference area, 845 sq m, and the sweep of its quarter chord,
    33.5 degrees. Its mean aerodynamic chord, which no source gives, is the
    trapezoid's of that area, span and tip chord: 11.97 m, its leading edge
    29.03 m behind the nose.
  FAA type certificate data sheet A58NM, revision 11
    The datum - 7.33 m ahead of the nose, 7.00 m below the fuselage datum line
    - which is this model's origin; the control surfaces' travel: ailerons 20
    degrees down and 30 up, elevators 20 down and 30 up, rudders 30 each way.
  Airbus, A380 Flight Deck and Systems Briefing for Pilots (STL 945.1380/05,
  issue 3, 2009 - Airbus's, but found only as text on a third-party site)
    The slats and flaps: 20 and 8 degrees in CONF 1+F, 20 and 17 in 2, 23 and
    26 in 3, 23 and 32 in FULL; the ailerons droop 5 degrees with them.
  EASA type certificate data sheet E.012, Trent 900, issue 12
    The Trent 970-84's take-off thrust, 334.29 kN (75,152 lb), and its fan,
    2.95 m (116 in).
  ICAO Aircraft Engine Emissions Databank, version 32
    The Trent 970-84's bypass ratio, 8.45.
  NASA CR-2144, Heffley and Jewell, Aircraft Handling Qualities Data (1972),
  section IX, the Boeing 747
    Every stability and control derivative - the lift's slope and the lift of
    pitch rate and elevator, the pitching moments, the side force, rolling and
    yawing moments - from its power approach configuration (table IX-2), non-
    dimensional and so carried to the A380's wing; and its inertias (table
    IX-3), as radii of gyration scaled by the A380's span and length. The 747
    is the nearest aircraft of the A380's kind whose derivatives are published:
    four engines under a swept wing of much the same aspect ratio.

What no source gives, and is set here, each named below with its number:
the drag at zero lift and the span efficiency, the take-off flaps' lift and
drag and the stall, and the drag rise's Mach, all set to fly the figures; the
full flaps' lift, set so that the AC's final approach speed, 138 knots at
395,000 kg (3-5-0), is 1.23 times the stall and 5 knots - Airbus's usual
margins - with the stall at 108 knots; the pitching moment at zero lift, set
to trim with little elevator; the yaw damper, which the A380 has and the
747's derivatives need; and the gear's springs, the roll spoilers' share,
and the weights' and tanks' positions, estimates.
"""

import math
import sys

import airliner
from airliner import OUT, PINNED

SCRIPT = "make_a380"
MODEL = "a380"
ENGINE = "Trent970"

M = 39.3701                  # inches in a metre
FT_PER_M = 3.28084
LB_PER_KG = 2.20462

# The datum, the model's origin: 7.33 m ahead of the nose, 7.00 m below the
# fuselage datum line (A58NM). x aft, z up, in inches.
NOSE_X = 7.3302
FDL_Z = 7.000
GROUND_Z = FDL_Z - 5.15      # the ground at the maximum ramp weight

# The wing: 845 sq m, 79.75 m, the tip's chord 3.98 m and its leading edge
# 46.97 m behind the nose; the trapezoid's root chord, taper, leading edge
# sweep, mean aerodynamic chord and its leading edge.
AREA = 845.0
SPAN = 79.75
TIP_CHORD = 3.98
ROOT_CHORD = 2.0 * AREA / SPAN - TIP_CHORD
TAPER = TIP_CHORD / ROOT_CHORD
ASPECT = SPAN ** 2 / AREA
TAN_LE = math.tan(math.radians(33.5)) + (1.0 - TAPER) / (ASPECT * (1.0 + TAPER))
ROOT_LE = 46.97 - SPAN / 2.0 * TAN_LE
MAC = 2.0 / 3.0 * ROOT_CHORD * (1.0 + TAPER + TAPER ** 2) / (1.0 + TAPER)
MAC_Y = SPAN / 6.0 * (1.0 + 2.0 * TAPER) / (1.0 + TAPER)
MAC_LE = ROOT_LE + MAC_Y * TAN_LE


def mac_x(fraction):
    """The datum's x, in metres, of a fraction of the mean aerodynamic chord."""
    return NOSE_X + MAC_LE + fraction * MAC


# Weights: the operating empty weight, its centre of gravity at 39% of the
# mean aerodynamic chord, as the AC's heavy loadings have theirs (37.5 to
# 41%); the payload at 38%.
EMPTY_KG = 285000.0
EMPTY_CG = 0.39
PAYLOAD_CG = 0.38
# The 747's radii of gyration at 564,032 lb (CR-2144 table IX-3), scaled by the
# span (roll) and length (pitch), and yaw as the root of the two's squares, as
# the 747's is to within 1%; its product of inertia in proportion to yaw.
RADII_747_FT = (28.46, 42.92, 50.89)
LENGTH_747_FT = 231.83       # Boeing D6-58326-1, 747-400
SPAN_747_FT = 195.68         # CR-2144 figure IX-2
IXZ_OVER_IZZ = 0.0195
# The tanks: 253,983 kg, in an inner and an outer pair (the AC's eleven tanks,
# the trim tank's fuel among the wings').
FUEL_KG = 253983.0
INNER_SHARE = 0.607
INNER_Y, OUTER_Y = 8.0, 20.0  # metres out from the centreline

# The engines: Trent 970-84, 75,152 lb; bypass ratio 8.45.
THRUST_LB = 75152.0
BYPASS = 8.45
FAN_IN = 116.0
WINDMILL_DRAG = 0.4

# Aerodynamics set here (see the docstring).
ZERO_LIFT_DRAG = 0.0165
SPAN_EFFICIENCY = 0.85
ZERO_ALPHA_LIFT = 0.25
LIFT_SLOPE = 5.70            # the 747's, table IX-2
CLEAN_STALL_ALPHA = 0.19
SLATS_STALL_ALPHA = 0.27
FLAP_DEGREES = [0, 8, 17, 26, 32]
FLAP_LIFT = [0.0, 0.18, 0.38, 0.62, 0.745]
FLAP_DRAG = [0.0, 0.004, 0.010, 0.030, 0.055]
SLAT_DRAG = 0.004
GEAR_DRAG = 0.012
DRAG_DIVERGENCE = 0.88
PITCH_ZERO_LIFT = 0.02
FLAP_PITCH = -0.10           # at full flap
BRAKING_FRICTION = "0.50"
# The yaw damper's rudder, as a fraction of its travel, per radian a second.
YAW_DAMPER_GAIN = 2.0

# The 747's derivatives, power approach (CR-2144 table IX-2), per radian.
CL_Q, CL_DE = 5.4, 0.338
CM_ALPHA, CM_ADOT, CM_Q, CM_DE = -1.26, -3.2, -20.8, -1.34
CY_BETA, CY_DR = -0.96, 0.175
CL_BETA, CL_P, CL_R, CL_DR = -0.221, -0.45, 0.101, 0.007
CN_BETA, CN_P, CN_R, CN_DR = 0.150, -0.121, -0.30, -0.109
# Its ailerons', per radian of the right and left together: the A380's roll
# spoilers besides, at a third more (set here); a side's is twice.
CL_DA, CN_DA = 0.0461 * 1.33, 0.0064


def inches(metres):
    return metres * M


def location(x, y, z, indent):
    return (f"{indent}<location unit=\"IN\">\n"
            f"{indent}    <x> {inches(x):.1f} </x>\n"
            f"{indent}    <y> {inches(y):.1f} </y>\n"
            f"{indent}    <z> {inches(z):.1f} </z>\n"
            f"{indent}</location>\n")


def metrics():
    i = "        "
    return ("    <metrics>\n"
            f"        <wingarea unit=\"FT2\"> {AREA * FT_PER_M ** 2:.1f} </wingarea>\n"
            f"        <wingspan unit=\"FT\"> {SPAN * FT_PER_M:.2f} </wingspan>\n"
            f"        <chord unit=\"FT\"> {MAC * FT_PER_M:.2f} </chord>\n"
            "        <location name=\"AERORP\" unit=\"IN\">\n"
            f"            <x> {inches(mac_x(0.25)):.1f} </x>\n"
            "            <y> 0 </y>\n"
            f"            <z> {inches(FDL_Z - 2.5):.1f} </z>\n"
            "        </location>\n"
            "        <location name=\"EYEPOINT\" unit=\"IN\">\n"
            f"            <x> {inches(NOSE_X + 5.0):.1f} </x>\n"
            "            <y> -20 </y>\n"
            f"            <z> {inches(FDL_Z + 1.0):.1f} </z>\n"
            "        </location>\n"
            "        <location name=\"VRP\" unit=\"IN\">\n"
            f"            <x> {inches(NOSE_X):.1f} </x>\n"
            "            <y> 0 </y>\n"
            f"            <z> {inches(FDL_Z):.1f} </z>\n"
            "        </location>\n"
            "    </metrics>\n")


def mass_balance():
    mass_slug = EMPTY_KG * LB_PER_KG / 32.174
    kx = RADII_747_FT[0] * SPAN * FT_PER_M / SPAN_747_FT
    ky = RADII_747_FT[1] * 72.73 * FT_PER_M / LENGTH_747_FT
    kz = math.hypot(kx, ky)
    ixx, iyy, izz = (mass_slug * k * k for k in (kx, ky, kz))
    i = "        "
    return ("    <mass_balance>\n"
            f"        <ixx unit=\"SLUG*FT2\"> {ixx:.4g} </ixx>\n"
            f"        <iyy unit=\"SLUG*FT2\"> {iyy:.4g} </iyy>\n"
            f"        <izz unit=\"SLUG*FT2\"> {izz:.4g} </izz>\n"
            f"        <ixz unit=\"SLUG*FT2\"> {IXZ_OVER_IZZ * izz:.4g} </ixz>\n"
            f"        <emptywt unit=\"LBS\"> {EMPTY_KG * LB_PER_KG:.0f} </emptywt>\n"
            "        <location name=\"CG\" unit=\"IN\">\n"
            f"            <x> {inches(mac_x(EMPTY_CG)):.1f} </x>\n"
            "            <y> 0 </y>\n"
            f"            <z> {inches(FDL_Z - 0.8):.1f} </z>\n"
            "        </location>\n"
            "        <pointmass name=\"Payload\">\n"
            "            <weight unit=\"LBS\"> 0 </weight>\n"
            + location(mac_x(PAYLOAD_CG), 0.0, FDL_Z, "            ") +
            "        </pointmass>\n"
            "    </mass_balance>\n")


def bogey(name, x, y, z, spring, damping, steer, brake, indent="        "):
    return (f"{indent}<contact type=\"BOGEY\" name=\"{name}\">\n"
            + location(x, y, z, indent + "    ") +
            f"{indent}    <static_friction> {BRAKING_FRICTION if brake != 'NONE' else '0.80'} </static_friction>\n"
            f"{indent}    <dynamic_friction> 0.50 </dynamic_friction>\n"
            f"{indent}    <rolling_friction> 0.02 </rolling_friction>\n"
            f"{indent}    <spring_coeff unit=\"LBS/FT\"> {spring:.0f} </spring_coeff>\n"
            f"{indent}    <damping_coeff unit=\"LBS/FT/SEC\"> {damping:.0f} </damping_coeff>\n"
            f"{indent}    <max_steer unit=\"DEG\"> {steer} </max_steer>\n"
            f"{indent}    <brake_group> {brake} </brake_group>\n"
            f"{indent}    <retractable>1</retractable>\n"
            f"{indent}</contact>\n")


def ground_reactions():
    # The springs: each gear compressing about a foot under its share of 575 t -
    # the nose's 6%, the wing gears' 38%, the body gears' 56% - and damped at
    # about a fifth of critical for the aircraft's mass on them.
    weight = 575000.0 * LB_PER_KG
    contact_z = GROUND_Z - 0.30
    nose_x = NOSE_X + 4.97
    wing_x = nose_x + 28.605 + 0.85
    body_x = nose_x + 31.881 + 1.53
    out = "    <ground_reactions>\n"
    # The nose wheel steers 5 degrees with the pedals, as JSBSim's airliners'
    # does; the tiller's 70 is for taxiing, which glideslope's controls lack.
    out += bogey("NOSE", nose_x, 0.0, contact_z, 0.06 * weight, 0.012 * weight, 5, "NONE")
    for side, sign in (("LEFT", -1.0), ("RIGHT", 1.0)):
        out += bogey(f"{side}_WING", wing_x, sign * 12.456 / 2.0, contact_z, 0.19 * weight,
                     0.04 * weight, 0, side)
        out += bogey(f"{side}_BODY", body_x, sign * 5.264 / 2.0, contact_z, 0.28 * weight,
                     0.06 * weight, 0, side)
    # The wing tips and the nacelles, where the AC's ground clearances put
    # them at the maximum ramp weight (figure 2-3-0-991-001): W1, 7.55 m; N1
    # and N2, 1.05 and 1.90 m, the inboard nacelles taken as the lower. The
    # tail's clearance in rotation the AC does not give, and it has none.
    for name, x, y, z in (("LEFT_WING_TIP", NOSE_X + 50.0, -SPAN / 2.0, GROUND_Z + 7.55),
                          ("RIGHT_WING_TIP", NOSE_X + 50.0, SPAN / 2.0, GROUND_Z + 7.55),
                          ("LEFT_OUTBOARD_ENGINE", NOSE_X + 29.94, -25.7, GROUND_Z + 1.90),
                          ("RIGHT_OUTBOARD_ENGINE", NOSE_X + 29.94, 25.7, GROUND_Z + 1.90),
                          ("LEFT_INBOARD_ENGINE", NOSE_X + 22.23, -14.8, GROUND_Z + 1.05),
                          ("RIGHT_INBOARD_ENGINE", NOSE_X + 22.23, 14.8, GROUND_Z + 1.05)):
        out += (f"        <contact type=\"STRUCTURE\" name=\"{name}\">\n"
                + location(x, y, z, "            ") +
                "            <static_friction> 1.0 </static_friction>\n"
                "            <dynamic_friction> 1.0 </dynamic_friction>\n"
                f"            <spring_coeff unit=\"LBS/FT\"> {0.5 * weight:.0f} </spring_coeff>\n"
                f"            <damping_coeff unit=\"LBS/FT/SEC\"> {0.1 * weight:.0f} </damping_coeff>\n"
                "        </contact>\n")
    return out + "    </ground_reactions>\n"


def propulsion():
    tanks = [(-INNER_Y, INNER_SHARE / 2.0), (INNER_Y, INNER_SHARE / 2.0),
             (-OUTER_Y, (1.0 - INNER_SHARE) / 2.0), (OUTER_Y, (1.0 - INNER_SHARE) / 2.0)]
    out = "    <propulsion>\n"
    # Outboard left, inboard left, inboard right, outboard right: 0 to 3.
    for y, inlet in ((-25.7, 29.94), (-14.8, 22.23), (14.8, 22.23), (25.7, 29.94)):
        out += (f"        <engine file=\"{ENGINE}\">\n"
                + "".join(f"            <feed>{t}</feed>\n" for t in range(4)) +
                "            <thruster file=\"direct\">\n"
                + location(NOSE_X + inlet + 3.0, y, FDL_Z - 4.3 if abs(y) < 20 else FDL_Z - 4.0,
                           "                ") +
                "            </thruster>\n"
                "        </engine>\n")
    for y, share in tanks:
        # Each tank at 40% of the local chord, on the swept wing.
        chord = ROOT_CHORD * (1.0 - (1.0 - TAPER) * abs(y) / (SPAN / 2.0))
        x = NOSE_X + ROOT_LE + abs(y) * TAN_LE + 0.4 * chord
        lbs = FUEL_KG * share * LB_PER_KG
        out += ("        <tank type=\"FUEL\">\n"
                + location(x, y, FDL_Z - 2.7, "            ") +
                f"            <capacity unit=\"LBS\"> {lbs:.0f} </capacity>\n"
                f"            <contents unit=\"LBS\"> {lbs:.0f} </contents>\n"
                "        </tank>\n")
    return out + "    </propulsion>\n"


def kinematic(name, cmd, positions, times, output, indent="            "):
    rows = "".join(f"{indent}        <setting>\n{indent}            <position> {p} </position>\n"
                   f"{indent}            <time> {t} </time>\n{indent}        </setting>\n"
                   for p, t in zip(positions, times))
    return (f"{indent}<kinematic name=\"{name}\">\n{indent}    <input>{cmd}</input>\n"
            f"{indent}    <traverse>\n{rows}{indent}    </traverse>\n"
            f"{indent}    <output>{output}</output>\n{indent}</kinematic>\n")


def surface(name, inputs, low, high, output, indent="            "):
    ins = "".join(f"{indent}    <input>{x}</input>\n" for x in inputs)
    return (f"{indent}<summer name=\"{name} Sum\">\n{ins}"
            f"{indent}    <clipto><min>-1</min><max>1</max></clipto>\n{indent}</summer>\n"
            f"{indent}<aerosurface_scale name=\"{name}\">\n"
            f"{indent}    <input>fcs/{name.lower().replace(' ', '-')}-sum</input>\n"
            f"{indent}    <range><min>{low:.4f}</min><max>{high:.4f}</max></range>\n"
            f"{indent}    <output>{output}</output>\n{indent}</aerosurface_scale>\n")


def flight_control():
    rad = math.radians
    out = "    <flight_control name=\"A380\">\n        <channel name=\"Controls\">\n"
    # Elevators 30 degrees up, 20 down; ailerons 30 up, 20 down, each side's
    # the mirror of the other's, and drooping 5 degrees with the flaps; rudders
    # 30 each way (A58NM). The stick's range is taken as symmetric about the
    # smaller travel's side where the certificate's is not.
    out += surface("Elevator", ["fcs/elevator-cmd-norm", "fcs/pitch-trim-cmd-norm"], -rad(30), rad(20),
                   "fcs/elevator-pos-rad")
    out += surface("Aileron", ["fcs/aileron-cmd-norm", "fcs/roll-trim-cmd-norm"], -rad(20), rad(20),
                   "fcs/left-aileron-pos-rad")
    out += ("            <pure_gain name=\"Right Aileron\">\n"
            "                <input>-fcs/left-aileron-pos-rad</input>\n"
            "                <gain>1</gain>\n"
            "                <output>fcs/right-aileron-pos-rad</output>\n"
            "            </pure_gain>\n")
    # A yaw damper, as the A380's flight controls have and JSBSim's 747 does:
    # the rudder against the yaw rate, washed out over two seconds so that a
    # steady turn is left alone, and given a fifth of its travel. Without it the
    # 747's derivatives leave the Dutch roll damped by a fifth a swing.
    out += ("            <washout_filter name=\"Yaw Rate Washout\">\n"
            "                <input>velocities/r-aero-rad_sec</input>\n"
            "                <c1>0.5</c1>\n"
            "            </washout_filter>\n"
            "            <pure_gain name=\"Yaw Damper\">\n"
            "                <input>fcs/yaw-rate-washout</input>\n"
            f"                <gain>{YAW_DAMPER_GAIN}</gain>\n"
            "                <clipto><min>-0.2</min><max>0.2</max></clipto>\n"
            "            </pure_gain>\n")
    out += surface("Rudder", ["fcs/rudder-cmd-norm", "fcs/yaw-trim-cmd-norm", "fcs/yaw-damper"], -rad(30),
                   rad(30), "fcs/rudder-pos-rad")
    # The flaps' lever: 0, 1+F, 2, 3 and FULL, its command a fraction of FULL's
    # 32 degrees; each setting's travel taking about 8 seconds.
    out += kinematic("Flaps", "fcs/flap-cmd-norm", FLAP_DEGREES, [0, 8, 8, 8, 6], "fcs/flap-pos-deg")
    out += ("            <aerosurface_scale name=\"Flap Position Normalizer\">\n"
            "                <input>fcs/flap-pos-deg</input>\n"
            "                <domain><min>0</min><max>32</max></domain>\n"
            "                <range><min>0</min><max>1</max></range>\n"
            "                <output>fcs/flap-pos-norm</output>\n"
            "            </aerosurface_scale>\n")
    out += kinematic("Gear", "gear/gear-cmd-norm", [0, 1], [0, 8], "gear/gear-pos-norm")
    out += kinematic("Speedbrake", "fcs/speedbrake-cmd-norm", [0, 1], [0, 2], "fcs/speedbrake-pos-norm")
    out += kinematic("Spoilers", "fcs/spoiler-cmd-norm", [0, 1], [0, 2], "fcs/spoiler-pos-norm")
    return out + "        </channel>\n    </flight_control>\n"


def table1(var, rows, indent):
    body = "".join(f"{indent}        {a}\t{b}\n" for a, b in rows)
    return (f"{indent}<table>\n{indent}    <independentVar>{var}</independentVar>\n"
            f"{indent}    <tableData>\n{body}{indent}    </tableData>\n{indent}</table>\n")


def coefficient(name, description, factors, indent="            "):
    """A JSBSim aerodynamic function: the product of `factors`, each a
    property name, a number, or a ready-made element."""
    parts = []
    for f in factors:
        if isinstance(f, (int, float)):
            parts.append(f"{indent}        <value>{f:.5g}</value>\n")
        elif f.lstrip().startswith("<"):
            parts.append(f)
        else:
            parts.append(f"{indent}        <property>{f}</property>\n")
    return (f"{indent}<function name=\"aero/coefficient/{name}\">\n"
            f"{indent}    <description>{description}</description>\n"
            f"{indent}    <product>\n" + "".join(parts) +
            f"{indent}    </product>\n{indent}</function>\n")


def lift_table(indent):
    """The lift by angle of attack, and by the flaps: the zero-alpha lift and
    the 747's slope to the clean stall, the slats' - out from CONF 1+F - to
    theirs, and past each a fall; each flap setting's lift added."""
    def curve(stall, extra):
        peak = ZERO_ALPHA_LIFT + extra + LIFT_SLOPE * stall
        return [(-0.20, ZERO_ALPHA_LIFT + extra - LIFT_SLOPE * 0.20), (0.0, ZERO_ALPHA_LIFT + extra),
                (stall, peak), (stall + 0.12, peak - 0.40), (0.60, 0.70 + extra), (1.57, 0.0)]

    def at(points, alpha):
        for (a0, c0), (a1, c1) in zip(points, points[1:]):
            if a0 <= alpha <= a1:
                return c0 + (c1 - c0) * (alpha - a0) / (a1 - a0)
        return points[-1][1]

    curves = [curve(CLEAN_STALL_ALPHA if deg == 0 else SLATS_STALL_ALPHA - 0.02 * (lift / FLAP_LIFT[-1]), lift)
              for deg, lift in zip(FLAP_DEGREES, FLAP_LIFT)]
    alphas = sorted({round(a, 4) for c in curves for a, _ in c} | {-1.57})
    head = f"{indent}            \t" + "\t".join(str(d) for d in FLAP_DEGREES) + "\n"
    rows = "".join(f"{indent}        {a:.4f}\t" + "\t".join(f"{at(c, a):.4f}" for c in curves) + "\n"
                   for a in alphas)
    return (f"{indent}<table>\n"
            f"{indent}    <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
            f"{indent}    <independentVar lookup=\"column\">fcs/flap-pos-deg</independentVar>\n"
            f"{indent}    <tableData>\n{head}{rows}{indent}    </tableData>\n{indent}</table>\n")


def aerodynamics():
    i = "            "
    t = i + "        "
    ground = ("        <function name=\"aero/function/kCLge\">\n"
              "            <description>Change_in_lift_due_to_ground_effect</description>\n"
              + table1("aero/h_b-mac-ft", [(0.0, 1.203), (0.1, 1.127), (0.15, 1.090), (0.2, 1.073),
                                           (0.3, 1.046), (0.4, 1.055), (0.5, 1.019), (0.6, 1.013),
                                           (0.7, 1.008), (0.8, 1.006), (0.9, 1.003), (1.0, 1.002),
                                           (1.1, 1.0)], "            ") +
              "        </function>\n"
              "        <function name=\"aero/function/kCDge\">\n"
              "            <description>Change_in_drag_due_to_ground_effect</description>\n"
              + table1("aero/h_b-mac-ft", [(0.0, 0.480), (0.1, 0.515), (0.15, 0.629), (0.2, 0.709),
                                           (0.3, 0.815), (0.4, 0.882), (0.5, 0.928), (0.6, 0.962),
                                           (0.7, 0.988), (0.8, 1.0), (0.9, 1.0), (1.0, 1.0), (1.1, 1.0)],
                                           "            ") +
              "        </function>\n")
    induced = 1.0 / (math.pi * ASPECT * SPAN_EFFICIENCY)
    flap_drag = [(d, (SLAT_DRAG if d > 0 else 0.0) + f) for d, f in zip(FLAP_DEGREES, FLAP_DRAG)]
    flap_pitch = [(d, FLAP_PITCH * d / FLAP_DEGREES[-1]) for d in FLAP_DEGREES]
    q, s = "aero/qbar-psf", "metrics/Sw-sqft"
    lift = [
        coefficient("CLalpha", "Lift_due_to_alpha_and_flaps", [q, s, "aero/function/kCLge", lift_table(t)]),
        coefficient("CLq", "Lift_due_to_pitch_rate", [q, s, "aero/ci2vel", "velocities/q-aero-rad_sec", CL_Q]),
        coefficient("CLde", "Lift_due_to_elevator", [q, s, "fcs/elevator-pos-rad", CL_DE]),
    ]
    drag = [
        coefficient("CD0", "Drag_at_zero_lift", [q, s, table1("aero/alpha-rad", [
            (-1.57, 1.5), (-0.26, 0.04), (0.0, ZERO_LIFT_DRAG), (CLEAN_STALL_ALPHA, ZERO_LIFT_DRAG),
            (SLATS_STALL_ALPHA + 0.05, 0.08), (1.57, 1.6)], t)]),
        coefficient("CDi", "Induced_drag", [q, s, "aero/cl-squared", "aero/function/kCDge", induced]),
        coefficient("CDflaps", "Drag_due_to_slats_and_flaps", [q, s, table1("fcs/flap-pos-deg", flap_drag, t)]),
        coefficient("CDgear", "Drag_due_to_gear", [q, s, "gear/gear-pos-norm", GEAR_DRAG]),
        coefficient("CDmach", "Drag_due_to_mach", [q, s, table1("velocities/mach", [
            tuple(r.split("\t")) for r in airliner.mach_drag_rows(DRAG_DIVERGENCE, "").split("\n")], t)]),
        coefficient("CDspeedbrake", "Drag_due_to_speedbrakes", [q, s, "fcs/speedbrake-pos-norm", 0.020]),
        coefficient("CDspoilers", "Drag_due_to_ground_spoilers", [q, s, "fcs/spoiler-pos-norm", 0.040]),
        coefficient("CDbeta", "Drag_due_to_sideslip", [q, s, "aero/mag-beta-rad", 0.25]),
        coefficient("CDde", "Drag_due_to_elevator", [q, s, "fcs/mag-elevator-pos-rad", 0.03]),
    ]
    side = [
        coefficient("CYb", "Side_force_due_to_beta", [q, s, "aero/beta-rad", CY_BETA]),
        coefficient("CYdr", "Side_force_due_to_rudder", [q, s, "fcs/rudder-pos-rad", CY_DR]),
    ]
    b = "metrics/bw-ft"
    roll = [
        coefficient("Clb", "Roll_moment_due_to_beta", [q, s, b, "aero/beta-rad", CL_BETA]),
        coefficient("Clp", "Roll_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CL_P]),
        coefficient("Clr", "Roll_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CL_R]),
        coefficient("Clda", "Roll_moment_due_to_ailerons", [q, s, b, "fcs/left-aileron-pos-rad", 2.0 * CL_DA]),
        coefficient("Cldr", "Roll_moment_due_to_rudder", [q, s, b, "fcs/rudder-pos-rad", CL_DR]),
    ]
    c = "metrics/cbarw-ft"
    pitch = [
        coefficient("Cm0", "Pitch_moment_at_zero_lift", [q, s, c, PITCH_ZERO_LIFT]),
        coefficient("Cmalpha", "Pitch_moment_due_to_alpha", [q, s, c, "aero/alpha-rad", CM_ALPHA]),
        coefficient("Cmq", "Pitch_moment_due_to_pitch_rate", [q, s, c, "aero/ci2vel", "velocities/q-aero-rad_sec", CM_Q]),
        coefficient("Cmadot", "Pitch_moment_due_to_alpha_rate", [q, s, c, "aero/ci2vel", "aero/alphadot-rad_sec", CM_ADOT]),
        coefficient("Cmde", "Pitch_moment_due_to_elevator", [q, s, c, "fcs/elevator-pos-rad", CM_DE]),
        coefficient("Cmflaps", "Pitch_moment_due_to_flaps", [q, s, c, table1("fcs/flap-pos-deg", flap_pitch, t)]),
    ]
    yaw = [
        coefficient("Cnb", "Yaw_moment_due_to_beta", [q, s, b, "aero/beta-rad", CN_BETA]),
        coefficient("Cnp", "Yaw_moment_due_to_roll_rate", [q, s, b, "aero/bi2vel", "velocities/p-aero-rad_sec", CN_P]),
        coefficient("Cnr", "Yaw_moment_due_to_yaw_rate", [q, s, b, "aero/bi2vel", "velocities/r-aero-rad_sec", CN_R]),
        coefficient("Cnda", "Yaw_moment_due_to_ailerons", [q, s, b, "fcs/left-aileron-pos-rad", 2.0 * CN_DA]),
        coefficient("Cndr", "Yaw_moment_due_to_rudder", [q, s, b, "fcs/rudder-pos-rad", CN_DR]),
    ]
    windmill = airliner.windmill_function(4, FAN_IN, WINDMILL_DRAG, "            ") + "\n"
    out = "    <aerodynamics>\n" + ground
    for name, fs in (("LIFT", lift), ("DRAG", drag + [windmill]), ("SIDE", side), ("ROLL", roll),
                     ("PITCH", pitch), ("YAW", yaw)):
        out += f"        <axis name=\"{name}\">\n" + "".join(fs) + "        </axis>\n"
    return out + "    </aerodynamics>\n"


def airframe():
    return ("<?xml version=\"1.0\"?>\n"
            "<fdm_config name=\"Airbus A380-841\" version=\"2.0\" release=\"BETA\">\n"
            "    <fileheader>\n"
            "        <!-- glideslope: written by tools/make_a380.py, which says where each number\n"
            "             comes from. Do not edit it by hand. -->\n"
            "        <author>glideslope</author>\n"
            "        <filecreationdate>2026-09-19</filecreationdate>\n"
            "        <description>Airbus A380-841, Trent 970-84</description>\n"
            "    </fileheader>\n"
            + metrics() + mass_balance() + ground_reactions() + propulsion() + flight_control()
            + aerodynamics() + "</fdm_config>\n")


def engine():
    text = (PINNED / "engine" / "TRENT-900.xml").read_text()
    text = airliner.replace_once(text, r"<turbine_engine name=\"TRENT-900\">",
                                 f"<turbine_engine name=\"{ENGINE}\">\n"
                                 "  <!-- glideslope: JSBSim's TRENT-900 made the A380's Trent 970-84 by\n"
                                 "       tools/make_a380.py, which made it. Do not edit it by hand. -->",
                                 "the engine's name", SCRIPT)
    text = airliner.replace_once(text, r"<milthrust>\s*80000\s*</milthrust>",
                                 f"<milthrust> {THRUST_LB:.0f} </milthrust>", "the thrust", SCRIPT)
    text = airliner.replace_once(text, r"<bypassratio>\s*8\.0\s*</bypassratio>",
                                 f"<bypassratio> {BYPASS} </bypassratio>", "the bypass ratio", SCRIPT)
    return airliner.with_mattingly_thrust(text, SCRIPT)


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
