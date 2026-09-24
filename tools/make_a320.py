#!/usr/bin/env python3
"""make_a320.py - glideslope's Airbus A320, made from JSBSim's A320.

JSBSim's A320, as pinned in ext/jsbsim, calls itself an A320-200: its 1,317 sq
ft wing and 111.3 ft span are the A320's with wing-tip fences, and its
CFM56-5 engines of 25,000 lb the CFM56-5A1 of the first, the A320-211. It is
made the A320-214 here, with the 27,000 lb CFM56-5B4 on the same airframe:
the planning document's take-off chart, the one take-off figure Airbus
publishes for its CFM A320s, names only "CFM56 series" engines, and the -214
is the CFM A320 that document describes above all. Its figures
here are from Airbus's A320 Aircraft Characteristics - Airport and Maintenance
Planning (June 2024), the FAA's type certificate data sheet A28NM and
Airbus's Getting to Grips with Aircraft Performance (2002), in
assets/figures/a320.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_a320.py           write the files
    python3 tools/make_a320.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Airframe (aircraft/a320/a320.xml)
    Weights and loading The operating empty weight the planning document uses
                        in its examples, 41,244 kg (90,927 lb), where the model
                        had 111,000 lb; a payload, which the model had nowhere
                        to put; and its two fuel tanks, 47,880 lb together
                        where an A320 with CFM engines holds 24,209 L (41,896
                        lb at 0.785 kg/L), with the payload at the centre of
                        gravity. The tanks were 72 in ahead of it.
    Drag_due_to_mach, added
                        The model had no drag rise at all; at full throttle at
                        35,000 ft it flew past Mach 0.89. Lock's fourth-power
                        rise from DRAG_DIVERGENCE (see tools/airliner.py).
    Pitch_moment_due_to_alpha -4.0 -> -0.9
                        The model's pitching moment rose with angle of attack
                        at 4 a radian, on a lift slope of 5.8: a static margin
                        of 70% of the chord, where an airliner's is 10 to 20%.
                        The elevator could not raise the nose past 10 degrees,
                        and with 10 degrees of flap the aircraft would not fly
                        slower than 173 knots. Now 15%.
    Drag_due_to_alpha -> Induced_drag and Drag_due_to_flaps
                        The model's drag rose with angle of attack, by a table
                        for each flap setting, to about twice what a wing of
                        its aspect ratio makes: at V2 with 10 degrees of flap
                        its lift was 6.5 times its drag, and with an engine
                        out it could not climb at any speed. The drag is now
                        the lift's induced drag at a span efficiency of 0.8 and
                        the flaps' own, rising from the slats' LEADING_EDGE_DRAG
                        as the flaps' deflection to the power 1.5, to
                        FULL_FLAP_DRAG at 40 degrees.
    Drag_due_to_landing_gear 0.040 -> 0.015
                        More than twice JSBSim's other airliners' - its 737's
                        0.015 and 747's 0.011 - where an airliner's gear adds
                        0.015 to 0.02; the 737's.
    Ground effect on lift, added
                        The model had none. JSBSim's own, as its 737 and other
                        models have it, by the wing's height over its span.
    Lift with the take-off flaps
                        With 9 and 10 degrees of flap, configuration 1+F, the
                        model's lift rose by only 0.1 at small angles and then
                        climbed steeply near its stall: at the 10 degrees its
                        tail allows on the runway it could not lift 162,000 lb
                        off until 168 knots, and rolled on its tail. It now
                        rises at the clean wing's slope to TAKEOFF_CLMAX at 16
                        degrees and stalls there as the model did; at 73,500
                        kg its V2 is 150 knots, an A320's.
    Ground spoilers, added
                        The model's speedbrake only added drag, over two
                        seconds, and nothing took the wing's lift away on the
                        runway: an A320 touching at 145 knots at 400 ft/min
                        rode its gear's rebound four to fourteen feet back
                        into the air however its nose was lowered. An A320's
                        ground spoilers come out at touchdown, and the FAA's
                        Airplane Flying Handbook (FAA-H-8083-3C, chapter 16)
                        gives their purpose: they spoil much of the lift and
                        put the weight on the wheels. They now come out with
                        the speedbrake lever while a wheel has weight on it,
                        and take the lift down as JSBSim's own 737's ground
                        spoilers do - in 0.6 seconds, to six tenths of it.
    Drag_of_windmilling_engines, added
                        As the 737-300's: a stopped CFM56-5A's 68.3 in fan
                        windmills, with a drag area of about 0.4 of its
                        frontal area.

  Engine (engine/CFM56-5B4.xml, from JSBSim's CFM56_5.xml)
    milthrust 25,000 -> 27,000 lb
                        The CFM56-5B4's take-off rating (FAA type certificate
                        data sheet E37NE), the -214's engine.
    Thrust with height and speed
                        Mattingly's lapse, as the 737-300's (see
                        tools/airliner.py), where JSBSim's generic turbofan
                        table held its thrust far too well with speed and
                        height.
  Airframe contacts scrape instead of rolling
                        The model's nine contacts that never retract - its
                        wing tips, its engine nacelles, its nose and its tail -
                        are airframe, not wheels, but carried JSBSim's rolling
                        friction of 0.02, a tyre's. Landed with its wheels up
                        the aeroplane rolled along on its wing tips: it was
                        still doing 118 knots after three minutes and 11 km,
                        and never stopped. They now carry the scraping
                        friction, spring and damping tools/ground.py states for
                        an airframe.
"""

import re
import sys

import airliner
import ground
from airliner import OUT, PINNED

SCRIPT = "make_a320"
MODEL = "a320"
MAXIMUM_WEIGHT_LBS = 172000  # the -200's published maximum take-off weight
AIRFRAME_CONTACTS = 9        # its wing tips, nacelles, nose and tail
ENGINE = "CFM56-5B4"
TAKEOFF_THRUST_LBS = 27000
OPERATING_EMPTY_LBS = 90927
CG_X_IN = 672
TANK_LBS = 20948
DRAG_DIVERGENCE = 0.78
FAN_DIAMETER_IN = 68.3
WINDMILL_DRAG = 0.4
# The pitching moment's slope with angle of attack: a static margin of 15% of
# the chord on the model's lift slope, 5.8 a radian.
PITCH_STIFFNESS = "-0.9000"
# Drag: the model's 0.016 at zero lift; the lift's induced drag, CL^2 over pi,
# the aspect ratio (9.41) and a span efficiency of 0.8; and the flaps' - the
# slats' LEADING_EDGE_DRAG from the first detent, the flaps' growing as their
# deflection to the power 1.5 to FULL_FLAP_DRAG at 40 degrees.
INDUCED_DRAG = 0.0423
# With the take-off flaps, 9 and 10 degrees, the lift rises at the clean
# wing's slope, 5.33 a radian, to TAKEOFF_CLMAX at 16 degrees, where it stalls
# as the model's did.
TAKEOFF_CLMAX = 2.3
GEAR_DRAG = "0.0150"
TAKEOFF_STALL_ALPHA = 0.28
LIFT_SLOPE = (0.73 - 0.25) / 0.09
# JSBSim's ground effect on lift, as its 737 and other models have it, by the
# wing's height over its span.
GROUND_EFFECT = """        <function name="aero/function/kCLge">
            <description>Change_in_lift_due_to_ground_effect</description>
            <table>
                <independentVar>aero/h_b-mac-ft</independentVar>
                <tableData>
                    0.0000	1.2030
                    0.1000	1.1270
                    0.1500	1.0900
                    0.2000	1.0730
                    0.3000	1.0460
                    0.4000	1.0280
                    0.5000	1.0190
                    0.6000	1.0130
                    0.7000	1.0080
                    0.8000	1.0060
                    0.9000	1.0030
                    1.0000	1.0020
                    1.1000	1.0000
                </tableData>
            </table>
        </function>
"""
# The ground spoilers: out with the speedbrake lever while a wheel has weight
# on it, in JSBSim's 737's 0.6 seconds, and taking the lift off as its do - to
# six tenths of it by a tenth of their travel.
GROUND_SPOILERS = """        <channel name="Ground Spoilers">
            <fcs_function name="Ground Spoilers Armed">
                <function>
                    <product>
                        <property>fcs/speedbrake-cmd-norm</property>
                        <property>gear/wow</property>
                    </product>
                </function>
                <output>fcs/ground-spoiler-cmd-norm</output>
            </fcs_function>
            <kinematic name="Ground Spoilers">
                <input>fcs/ground-spoiler-cmd-norm</input>
                <traverse>
                    <setting>
                        <position>0</position>
                        <time>0</time>
                    </setting>
                    <setting>
                        <position>1</position>
                        <time>0.6</time>
                    </setting>
                </traverse>
                <output>fcs/ground-spoiler-pos-norm</output>
            </kinematic>
        </channel>
"""
GROUND_SPOILER_LIFT = """        <function name="aero/function/kCLsp">
            <description>Change_in_lift_due_to_ground_spoilers</description>
            <table>
                <independentVar>fcs/ground-spoiler-pos-norm</independentVar>
                <tableData>
                    0.0000	1.0
                    0.1000	0.6
                </tableData>
            </table>
        </function>
"""
FLAP_DETENTS_DEG = [0, 1, 2, 5, 10, 15, 25, 30, 40]
LEADING_EDGE_DRAG = 0.007
FULL_FLAP_DRAG = 0.060


def replace_once(text, pattern, replacement, what):
    return airliner.replace_once(text, pattern, replacement, what, SCRIPT)


def engine():
    text = (PINNED / "engine" / "CFM56_5.xml").read_text()
    text = replace_once(text, r"(<turbine_engine name=\"CFM56_5\">)",
                        r"\1\n  <!-- glideslope: JSBSim's CFM56_5 with the changes listed in\n"
                        r"       tools/make_a320.py, which made it. Do not edit it by hand. -->",
                        "the engine's name")
    text = replace_once(text, r"<milthrust>\s*25000\.0\s*</milthrust>",
                        f"<milthrust> {TAKEOFF_THRUST_LBS}.0 </milthrust>", "the thrust")
    return airliner.with_mattingly_thrust(text, SCRIPT)


def scraping_airframe(text):
    """The contacts that never retract are airframe, so they scrape.

    JSBSim tells a wheel from a wing tip by nothing but what the model says,
    and this one said its wing tips were wheels: the same 0.02 rolling
    friction as its tyres. What never retracts is airframe, and is given the
    friction and the stiffness tools/ground.py states for one.
    """
    spring, damping = ground.stiffness(MAXIMUM_WEIGHT_LBS)
    fields = ((r"static_friction", f"{ground.SCRAPE_FRICTION}", ""),
              (r"dynamic_friction", f"{ground.SCRAPE_FRICTION}", ""),
              (r"rolling_friction", f"{ground.SCRAPE_FRICTION}", ""),
              (r"spring_coeff", f"{spring:.0f}", ' unit="LBS/FT"'),
              (r"damping_coeff", f"{damping:.0f}", ' unit="LBS/FT/SEC"'))

    def one(match):
        body = match.group(0)
        for tag, value, unit in fields:
            body, hits = re.subn(rf"<{tag}[^>]*>[^<]*</{tag}>",
                                 f"<{tag}{unit}> {value} </{tag}>", body)
            if hits != 1:
                raise SystemExit(f"{SCRIPT}: a contact has {hits} {tag}s, not one"
                                 " - has the pinned model changed?")
        return body

    text, n = re.subn(
        r"<contact type=\"BOGEY\"(?:(?!</contact>).)*?<retractable>0</retractable>"
        r"(?:(?!</contact>).)*?</contact>", one, text, flags=re.S)
    if n != AIRFRAME_CONTACTS:
        raise SystemExit(f"{SCRIPT}: found {n} contacts that never retract, not "
                         f"{AIRFRAME_CONTACTS} - has the pinned model changed?")
    return text


def airframe():
    text = (PINNED / "aircraft" / "A320" / "A320.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n      <!-- glideslope: this is JSBSim's A320 with the changes listed in\n"
        r"           tools/make_a320.py, which made it. Do not edit it by hand. -->",
        "the file header")
    airliner.without_sockets(text, SCRIPT)
    text = replace_once(text, r"<emptywt unit=\"LBS\">\s*111000\s*</emptywt>",
                        f"<emptywt unit=\"LBS\"> {OPERATING_EMPTY_LBS} </emptywt>", "the empty weight")
    text = replace_once(
        text, r"(            <z> -40 </z>\n        </location>\n)(    </mass_balance>)",
        r'\1        <pointmass name="Payload">' "\n"
        r'            <weight unit="LBS"> 0 </weight>' "\n"
        r'            <location unit="IN">' "\n"
        rf'                <x> {CG_X_IN} </x>' "\n"
        r'                <y> 0 </y>' "\n"
        r'                <z> -40 </z>' "\n"
        r'            </location>' "\n"
        r'        </pointmass>' "\n" r"\2", "the mass balance")
    for number in (0, 1):
        text = replace_once(
            text,
            r"(<tank type=\"FUEL\">    <!-- Tank number " + str(number) + r" -->\s*<location unit=\"IN\">\s*<x>)\s*600\s*(</x>"
            r".*?<capacity unit=\"LBS\">)\s*23940\s*(</capacity>\s*<contents unit=\"LBS\">)\s*15000\s*(</contents>)",
            rf"\g<1> {CG_X_IN} \g<2> {TANK_LBS} \g<3> {TANK_LBS} \4", f"tank {number}")
    text = replace_once(text, r"<engine file=\"CFM56_5\">(.*?)<engine file=\"CFM56_5\">",
                        rf'<engine file="{ENGINE}">\1<engine file="{ENGINE}">', "the engines")
    text = replace_once(
        text, r"(<description>Pitch_moment_due_to_alpha</description>.*?<value>)-4\.0000(</value>)",
        r"\g<1>" + PITCH_STIFFNESS + r"\2", "the pitch stiffness")
    m = __import__("re").search(r"(<description>Lift_due_to_alpha</description>\s*<product>\s*"
                                r"<property>aero/qbar-psf</property>\s*<property>metrics/Sw-sqft</property>\s*)"
                                r"(<table>.*?<tableData>\s*\n)(.*?)(\n\s*</tableData>)", text, __import__("re").S)
    if not m:
        raise SystemExit(f"{SCRIPT}: no lift table - has the pinned model changed?")
    head, *rows = m.group(3).split("\n")
    if head.split() != ["0.0000", "1.0000", "9.0000", "10.0000", "40.0000"]:
        raise SystemExit(f"{SCRIPT}: the lift table's flap columns have changed")
    reshaped = [head]
    zero_lift = TAKEOFF_CLMAX - LIFT_SLOPE * TAKEOFF_STALL_ALPHA
    for row in rows:
        values = [float(v) for v in row.split()]
        alpha = values[0]
        if alpha <= TAKEOFF_STALL_ALPHA + 1e-9:
            values[3] = values[4] = zero_lift + LIFT_SLOPE * alpha
        reshaped.append("                              " + "\t".join(f"{v:.4f}" for v in values))
    text = (text[:m.start(1)] + m.group(1) + "<property>aero/function/kCLge</property>\n                      "
            + "<property>aero/function/kCLsp</property>\n                      "
            + m.group(2) + "\n".join(reshaped) + m.group(4) + text[m.end(4):])
    text = replace_once(
        text, r"(<description>Drag_due_to_landing_gear</description>.*?<value>)0\.0400(</value>)",
        r"\g<1>" + GEAR_DRAG + r"\2", "the gear's drag")
    text = replace_once(text, r"(<aerodynamics>\n)",
                        lambda mm: mm.group(1) + "\n" + GROUND_EFFECT + GROUND_SPOILER_LIFT, "the aerodynamics")
    text = replace_once(text, r"(\n    </flight_control>)", lambda mm: "\n" + GROUND_SPOILERS.rstrip("\n") + mm.group(1),
                        "the end of the flight controls")

    flap_rows = "\n".join(
        f"                            {deg}\t{0.0 if deg == 0 else LEADING_EDGE_DRAG + (FULL_FLAP_DRAG - LEADING_EDGE_DRAG) * (deg / 40.0) ** 1.5:.4f}"
        for deg in FLAP_DETENTS_DEG)
    text = replace_once(
        text, r"            <function name=\"aero/coefficient/CDalpha\">.*?</function>\n",
        "            <function name=\"aero/coefficient/CDi\">\n"
        "                <description>Induced_drag</description>\n"
        "                <product>\n"
        "                    <property>aero/qbar-psf</property>\n"
        "                    <property>metrics/Sw-sqft</property>\n"
        "                    <property>aero/cl-squared</property>\n"
        f"                    <value>{INDUCED_DRAG}</value>\n"
        "                </product>\n"
        "            </function>\n"
        "            <function name=\"aero/coefficient/CDflap\">\n"
        "                <description>Drag_due_to_flaps</description>\n"
        "                <product>\n"
        "                    <property>aero/qbar-psf</property>\n"
        "                    <property>metrics/Sw-sqft</property>\n"
        "                    <table>\n"
        "                        <independentVar>fcs/flap-pos-deg</independentVar>\n"
        "                        <tableData>\n" + flap_rows + "\n"
        "                        </tableData>\n"
        "                    </table>\n"
        "                </product>\n"
        "            </function>\n", "the drag due to alpha")
    mach = ("            <function name=\"aero/coefficient/CDmach\">\n"
            "                <description>Drag_due_to_mach</description>\n"
            "                <product>\n"
            "                    <property>aero/qbar-psf</property>\n"
            "                    <property>metrics/Sw-sqft</property>\n"
            "                    <table>\n"
            "                        <independentVar>velocities/mach</independentVar>\n"
            "                        <tableData>\n"
            + airliner.mach_drag_rows(DRAG_DIVERGENCE, "                            ") + "\n"
            "                        </tableData>\n"
            "                    </table>\n"
            "                </product>\n"
            "            </function>\n")
    text = replace_once(text, r"(<axis name=\"DRAG\">\n)",
                        lambda m: m.group(1) + mach
                        + airliner.windmill_function(2, FAN_DIAMETER_IN, WINDMILL_DRAG) + "\n", "the drag axis")
    return scraping_airframe(text)


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
