#!/usr/bin/env python3
"""make_737_300.py - glideslope's Boeing 737-300, made from JSBSim's 737.

JSBSim's 737, as pinned in ext/jsbsim, is a 737 Classic: its 94.7 ft span is
the -300, -400 and -500's, and its CFM56 engines of 20,000 lb are the
CFM56-3B1 that only the -300 and -500 had. Its figures here are the -300's,
from Boeing's 737 Airplane Characteristics for Airport Planning (D6-58325-6,
revision E, 2023) and the FAA's type certificate data sheet A16WE, in
assets/figures/737-300.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_737_300.py           write the files
    python3 tools/make_737_300.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Airframe (aircraft/737-300/737-300.xml)
    No network sockets  The model opened a telnet port, 5137, and a UDP port,
                        5139, for programs to drive it, whenever it was loaded;
                        glideslope's aircraft talk to nothing but glideslope.
    Weights and loading The planning document's operating empty weight for the
                        135,000 lb take-off weight, 72,540 lb, where the model
                        had 83,000, more than any 737 Classic's; a payload at
                        the centre of gravity, which the model had nowhere to
                        put; and its three fuel tanks at the centre of gravity
                        too. They were 120 to 160 in ahead of it, so that a
                        take-off's fuel moved it forward by a fifth of the
                        wing's chord and the elevator could not raise the nose
                        to the stall.
    Flaps in degrees    The model's flaps ran through eight equal steps of its
                        normalised position; the 737's detents are 1, 2, 5, 10,
                        15, 25, 30 and 40 degrees, and the figures name them.
                        The flaps now travel in degrees, and the normalised
                        position the lift tables read is each detent's step, as
                        it was.
    Leading edge devices
                        The 737's slats and Krueger flaps come out with flaps 1
                        and let the wing reach a greater angle before it
                        stalls; the model's wing stalled at 13 degrees whatever
                        its flaps. From flaps 1 its lift slope now carries on to
                        LEADING_EDGE_STALL_ALPHA (17 degrees); at 135,000 lb
                        with flaps 5 it stalls at 131 knots, and its take-off
                        speeds are a 737-300's, where they were 20 knots fast.
    Drag_due_to_flaps   The model's flap drag rose in proportion to the
                        detent's step, so that flaps 5, the third of eight, had
                        more than a third of full flap's; it is now the leading
                        edge devices' LEADING_EDGE_DRAG from flaps 1, and the
                        flaps' growing as their deflection to the power 1.5 to
                        the model's 0.059 at 40 degrees. With the model's, the
                        one-engine climb at V2 fell to 1.4%, short of the 2.4%
                        FAR 25 demands. LEADING_EDGE_DRAG is what the take-off
                        runway length and that climb, together, need.
    Drag_of_windmilling_engines, added
                        JSBSim's turbine gives a stopped engine neither thrust
                        nor drag; its fan windmills, with a drag area of about
                        0.4 of the CFM56-3's 60 in fan's frontal area - an
                        estimate of the kind Torenbeek's method gives.
    Drag_due_to_mach    The model's drag rose from nothing at Mach 0.79 to
                        0.023 at Mach 1.1, a tenth of a swept wing's slope past
                        its drag divergence; it is now Lock's fourth-power
                        rise from DRAG_DIVERGENCE.
    Main wheels' braking friction 0.8 -> 0.5
                        JSBSim takes a wheel's static friction as its braking
                        coefficient at full brake, anti-skid assumed; 0.8
                        stopped a rejected take-off at two-thirds of a g, where
                        a dry runway gives about half that.

  Engine (engine/CFM56-3B1.xml, from JSBSim's CFM56.xml)
    Thrust with height and speed
                        JSBSim's generic turbofan table kept 93% of the static
                        thrust at Mach 0.2 and 42% at 30,000 ft and Mach 0.8,
                        where a high-bypass engine keeps about 80% and 25%: at
                        full throttle at 35,000 ft the model flew past Mach
                        0.85. The table is now Mattingly's lapse (see
                        tools/airliner.py).
    bleed 0.04 -> 0      The planning document's take-off runway lengths are
                        for "no engine airbleed for air conditioning".

The thruster (engine/direct.xml) is copied unchanged.
"""

import re
import sys

import airliner
from airliner import OUT, PINNED

SCRIPT = "make_737_300"
MODEL = "737-300"
ENGINE = "CFM56-3B1"
OPERATING_EMPTY_LBS = 72540
EMPTY_CG_X_IN = 639
FLAP_DETENTS_DEG = [0, 1, 2, 5, 10, 15, 25, 30, 40]
DRAG_DIVERGENCE = 0.79
LEADING_EDGE_STALL_ALPHA = 0.30
LEADING_EDGE_DRAG = 0.007
FULL_FLAP_DRAG = 0.059
FAN_DIAMETER_IN = 60.0
WINDMILL_DRAG = 0.4
BRAKING_FRICTION = "0.50"


def replace_once(text, pattern, replacement, what):
    return airliner.replace_once(text, pattern, replacement, what, SCRIPT)


def lift_rows(stall_alpha):
    """The model's lift curve, flaps up, and with the leading edge devices out
    from flaps 1: the same slope, 4.35 a radian, to `stall_alpha`."""
    clean = [(-0.20, -0.68), (0.00, 0.20), (0.23, 1.20), (0.46, 0.20)]

    def interpolate(curve, alpha):
        for (a0, c0), (a1, c1) in zip(curve, curve[1:]):
            if a0 <= alpha <= a1:
                return c0 + (c1 - c0) * (alpha - a0) / (a1 - a0)
        raise ValueError(alpha)

    slope = (1.20 - 0.20) / 0.23
    peak = 0.20 + slope * stall_alpha
    slats = [(-0.20, -0.68), (0.00, 0.20), (stall_alpha, peak), (stall_alpha + 0.23, peak - 1.0)]
    alphas = sorted({a for a, _ in clean} | {a for a, _ in slats})
    rows = ["                                        0.0\t1.0"]
    for alpha in alphas:
        c = interpolate(clean, alpha) if alpha <= 0.46 else 0.20
        s = interpolate(slats, alpha) if alpha <= slats[-1][0] else slats[-1][1]
        rows.append(f"                              {alpha:.3f}\t{c:.4f}\t{s:.4f}")
    return "\n".join(rows)


def flap_drag(deg):
    if deg == 0:
        return 0.0
    return LEADING_EDGE_DRAG + (FULL_FLAP_DRAG - LEADING_EDGE_DRAG) * (deg / 40.0) ** 1.5


def engine():
    text = (PINNED / "engine" / "CFM56.xml").read_text()
    text = replace_once(text, r"(<turbine_engine name=\"CFM56\">)",
                        r"\1\n  <!-- glideslope: JSBSim's CFM56 with the changes listed in\n"
                        r"       tools/make_737_300.py, which made it. Do not edit it by hand. -->",
                        "the engine's name")
    text = replace_once(text, r"<bleed>\s*0\.04\s*</bleed>", "<bleed>           0.00 </bleed>", "the bleed")
    return airliner.with_mattingly_thrust(text, SCRIPT)


def airframe():
    text = (PINNED / "aircraft" / "737" / "737.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n        <!-- glideslope: this is JSBSim's 737 with the changes listed in\n"
        r"             tools/make_737_300.py, which made it. Do not edit it by hand. -->",
        "the file header")
    text = replace_once(text, r"\n <!-- this is the telnet interface -->\n <input port=\"5137\" />\n", "\n",
                        "the telnet port")
    text = replace_once(text, r"\n <!-- 6 properties input, \+ 1 for the time stamp -->\n <input port=\"5139\".*?</input>\n",
                        "\n", "the UDP input port")
    airliner.without_sockets(text, SCRIPT)

    text = replace_once(text, r"<emptywt unit=\"LBS\">\s*83000 </emptywt>",
                        f"<emptywt unit=\"LBS\">      {OPERATING_EMPTY_LBS} </emptywt>", "the empty weight")
    text = replace_once(
        text, r"(            <z> -40 </z>\n        </location>\n)(    </mass_balance>)",
        r'\1        <pointmass name="Payload">' "\n"
        r'            <weight unit="LBS"> 0 </weight>' "\n"
        r'            <location unit="IN">' "\n"
        rf'                <x> {EMPTY_CG_X_IN} </x>' "\n"
        r'                <y>   0 </y>' "\n"
        r'                <z> -40 </z>' "\n"
        r'            </location>' "\n"
        r'        </pointmass>' "\n" r"\2", "the mass balance")
    text, n = re.subn(r"(<tank type=\"FUEL\"><!-- (?:Left wing|Right wing|Center) tank -->\s*<location unit=\"IN\">\s*<x>)\s*\d+\s*(</x>)",
                      rf"\g<1> {EMPTY_CG_X_IN} \2", text)
    if n != 3:
        raise SystemExit(f"{SCRIPT}: found {n} fuel tanks, not 3 - has the pinned model changed?")
    text = replace_once(text, r"<engine file=\"CFM56\">(.*?)<engine file=\"CFM56\">",
                        rf'<engine file="{ENGINE}">\1<engine file="{ENGINE}">', "the engines")
    for side in ("Left", "Right"):
        text = replace_once(
            text, r"(<contact name=\"" + side + r" Main Gear\" type=\"BOGEY\">.*?<static_friction>)\s*0\.80\s*(</static_friction>)",
            rf"\g<1> {BRAKING_FRICTION} \2", f"the {side.lower()} main gear's friction")

    positions = iter(FLAP_DETENTS_DEG)
    m = re.search(r"<kinematic name=\"Flaps Control\">.*?</kinematic>", text, re.S)
    if not m:
        raise SystemExit(f"{SCRIPT}: no flap kinematic - has the pinned model changed?")
    kinematic = re.sub(r"<position>[0-9.]+</position>", lambda _: f"<position>{next(positions)}</position>",
                       m.group(0))
    kinematic = kinematic.replace("<output>fcs/flap-pos-norm</output>", "<output>fcs/flap-pos-deg</output>")
    steps = len(FLAP_DETENTS_DEG) - 1
    table = "\n".join(f"                            {deg}\t{i / steps:.4f}" for i, deg in enumerate(FLAP_DETENTS_DEG))
    kinematic += ("\n\n            <fcs_function name=\"Flap Position Normalized\">\n"
                  "                <function>\n"
                  "                    <table>\n"
                  "                        <independentVar>fcs/flap-pos-deg</independentVar>\n"
                  "                        <tableData>\n" + table + "\n"
                  "                        </tableData>\n"
                  "                    </table>\n"
                  "                </function>\n"
                  "                <output>fcs/flap-pos-norm</output>\n"
                  "            </fcs_function>")
    text = text[:m.start()] + kinematic + text[m.end():]

    text = replace_once(
        text,
        r"(<description>Lift_due_to_alpha</description>.*?)<table>\s*<independentVar>aero/alpha-rad</independentVar>"
        r"\s*<tableData>.*?</tableData>\s*</table>",
        lambda mm: mm.group(1) + "<table>\n"
        "                          <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
        "                          <independentVar lookup=\"column\">fcs/flap-pos-deg</independentVar>\n"
        "                          <tableData>\n" + lift_rows(LEADING_EDGE_STALL_ALPHA) + "\n"
        "                          </tableData>\n"
        "                      </table>", "the lift curve")
    rows = "\n".join(f"                              {deg}\t{flap_drag(deg):.4f}" for deg in FLAP_DETENTS_DEG)
    text = replace_once(
        text,
        r"(<description>Drag_due_to_flaps</description>\s*<product>\s*<property>aero/qbar-psf</property>\s*"
        r"<property>metrics/Sw-sqft</property>\s*)<property>fcs/flap-pos-norm</property>\s*<value>[0-9.]+</value>",
        lambda mm: mm.group(1) + "<table>\n"
        "                          <independentVar>fcs/flap-pos-deg</independentVar>\n"
        "                          <tableData>\n" + rows + "\n"
        "                          </tableData>\n"
        "                      </table>", "the flap drag")
    text = replace_once(
        text,
        r"(<description>Drag_due_to_mach</description>.*?<tableData>\n).*?(\n\s*</tableData>)",
        lambda mm: mm.group(1) + airliner.mach_drag_rows(DRAG_DIVERGENCE) + mm.group(2), "the Mach drag")
    text = replace_once(text, r"(\n\s*<function name=\"aero/coefficient/CDflap\">)",
                        lambda mm: "\n" + airliner.windmill_function(2, FAN_DIAMETER_IN, WINDMILL_DRAG) + "\n"
                        + mm.group(1), "the flap drag function")
    return text


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
