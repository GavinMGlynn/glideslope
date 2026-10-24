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
"""

import re
import sys

import alpha_rate
import airliner
import ground
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
    # The tail strike, measured from her visual model by tools/ground.py.
    return ground.with_tail_strike(
        text, MODEL, MAXIMUM_WEIGHT_LBS, script=SCRIPT)


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airliner.with_reversers(alpha_rate.with_unkeyed_terms_on_qbar_uw(
            alpha_rate.with_alpha_terms_on_qbar_uw(
            alpha_rate.with_qbar_uw(airframe(), "make_747_400"), "make_747_400",
            {"PITCH": ["aero/coefficient/Cmalpha"],
             "LIFT": ["aero/coefficient/CLalpha"],
             "DRAG": ["aero/coefficient/CD0"]}), "make_747_400",
            {"LIFT": ["aero/coefficient/dCLflap",
             "aero/coefficient/dCLsb",
             "aero/coefficient/CLde"]}), REVERSERS, SCRIPT),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
