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

  Engine (engine/CF6-80C2B1F.xml, from JSBSim's GE-CF6-80C2-B1F.xml)
    Thrust with height and speed
                        Mattingly's lapse, as the 737-300's (see
                        tools/airliner.py), where JSBSim's generic turbofan
                        table held its thrust far too well with speed and
                        height.
"""

import re
import sys

import airliner
import ground
from airliner import OUT, PINNED

SCRIPT = "make_747_400"
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
    text = replace_once(
        text, r"(name=\"NOSE_LG\">.*?<spring_coeff unit=\"LBS/FT\">)\s*22000\s*(</spring_coeff>)",
        rf"\g<1> {NOSE_SPRING} \2", "the nose gear's spring")

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
    return text


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
