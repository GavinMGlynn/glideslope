#!/usr/bin/env python3
"""make_j3cub.py - glideslope's Piper J-3 Cub, made from JSBSim's J3Cub.

JSBSim's J3Cub, as pinned in ext/jsbsim, is an Aeromatic model of the J3C-65
with a Continental A-65-8 and a McCauley metal propeller, its aerodynamics
from a Cranfield thesis on a Cub (Du, 2011). The figures it is held to are the
Owner's Manual for the J3C-65's, Piper's booklet "How to Fly a Piper Cub"
(1945) and the type certificate's, FAA Aircraft Specification A-691, in
assets/figures/j3cub.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_j3cub.py           write the files
    python3 tools/make_j3cub.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Files and names       The pinned model keeps its engine, propeller and
                        flight controls in files whose names have spaces, in
                        its own Engines/ and Systems/ directories; here they
                        are engine/engA65-8.xml, engine/prop_j3cub_74in.xml
                        and aircraft/j3cub/Systems/conventional-controls.xml,
                        and the model is j3cub.

  Airframe (aircraft/j3cub/j3cub.xml)
    Weights and loading, from the type certificate: the booklet's empty weight,
                        680 lb, where the model had 765; the two seats, at +9
                        and +36 in from the wing's leading edge, and 20 lb of
                        baggage at +49, where the model had one seat; the
                        12-gallon tank at -18, where the model's was at the
                        aerodynamic reference point. The model's origin is its
                        aerodynamic reference, the wing's quarter chord, so the
                        leading edge is 15.75 in ahead of it - which puts the
                        main axle, as the manual has it, 2 1/2 in (here 3.15)
                        from the leading edge.

    The elevator's travel, 8 degrees each way -> 34 up and 29 down
                        A-691's (in Systems/conventional-controls.xml). With
                        the model's the elevator could not raise the nose to
                        the wing's stall: the stick fully back, at the gross
                        weight, it flew on at 41 knots, where with the travel
                        widened it stalls at 36.
    Drag_airframe 0.020, added
                        The model's drag at zero lift was its wing section's
                        (the USA-35B's, 0.019) and its gear's, 0.004: nothing
                        for the fuselage, tail, lift struts and wires, and it
                        glided at 13.5 to 1, against the booklet's 10.
    Drag_induced 0.0485 -> 0.0573
                        The model's was a span efficiency of 0.95, a wing's
                        alone; 0.80 is a light aircraft's.

  Engine (engine/engA65-8.xml)
    maxrpm 2800 -> 2300 The A-65-8's limit, "all operations, 2300 r.p.m.
                        (65 hp)" (A-691, item 311C(1)). JSBSim's piston engine
                        makes its rated power at maxrpm, so the model's made
                        its 65 hp only at 500 rpm past the Cub's limit.

  Propeller (engine/prop_j3cub_74in.xml)
    Diameter 75 -> 74 in The McCauley 1A90CF or 1B90CM, A-691's propeller item 2,
                        is "not over 74 inches".
    The tables          The model's propeller tables are JSBSim's generic
                        fixed-pitch ones, the same numbers as the Cessna
                        172P's 75 in propeller's, coarse enough to stop
                        thrusting only at an advance ratio of 1.17: on the
                        Cub's 65 hp it turned 1,590 rpm static, against the
                        type certificate's 1,950 to 2,250. Its name, CM7445,
                        reads as 74 in by 45 in of pitch, and its advance
                        ratios are drawn in by that pitch over the generic
                        blade's (22 degrees at three-quarters of its radius,
                        pi x 0.75 x tan 22 = 0.95), 0.64; its power by 0.57,
                        for 2,060 rpm static; and its thrust by 0.69, for the
                        manual's climb and cruise - the generic curve's
                        efficiency, 0.85 at best, now 0.66.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PINNED = ROOT / "ext" / "jsbsim"
PINNED_CUB = PINNED / "aircraft" / "J3Cub"
OUT = ROOT / "assets" / "jsbsim"

MODEL = "j3cub"
ENGINE = "engA65-8"
PROPELLER = "prop_j3cub_74in"
SYSTEM = "conventional-controls"

EMPTY_WEIGHT_LBS = 680
FUEL_LBS = 72           # 12 US gallons at 6 lb
# The propeller's pitch over its diameter, 45 in over 74, against the pinned
# table's, a 22-degree blade at three-quarters of its radius (pi x 0.75 x
# tan 22 = 0.95): its curves are drawn in on the advance ratio by the ratio.
FINER_PITCH = round((45 / 74) / 0.952, 3)
POWER_SCALE = 0.57
THRUST_SCALE = 0.69
AIRFRAME_DRAG = "0.0200"
INDUCED_DRAG = "0.0573"   # 1 / (pi x 6.94 x 0.80)
# The elevator's travel, A-691's: 34 degrees up, 29 down.
ELEVATOR_UP_RAD = "0.5934"
ELEVATOR_DOWN_RAD = "0.5061"
# The wing's leading edge, the type certificate's datum, in the model's frame.
LEADING_EDGE_X = -15.75


def station(inches_aft_of_datum):
    return round(LEADING_EDGE_X + inches_aft_of_datum, 2)


def replace_once(text, pattern, replacement, what):
    new, n = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f"make_j3cub: could not find {what} - has the pinned model changed?")
    return new


def pointmass(name, x, z):
    return (f"   <pointmass name=\"{name}\">\n"
            f"    <weight unit=\"LBS\"> 0 </weight>\n"
            f"    <location unit=\"IN\">\n"
            f"     <x>    {x:.2f} </x>\n"
            f"     <y>     0.00 </y>\n"
            f"     <z>   {z:.2f} </z>\n"
            f"   </location>\n"
            f"  </pointmass>\n")


def airframe():
    text = (PINNED_CUB / "J3Cub.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n  <!-- glideslope: this is JSBSim's J3Cub with the changes listed in\n"
        r"       tools/make_j3cub.py, which made it. Do not edit it by hand. -->",
        "the file header")
    text = replace_once(text, r"<emptywt unit=\"LBS\" >\s*765\.00 </emptywt>",
                        f"<emptywt unit=\"LBS\" >    {EMPTY_WEIGHT_LBS}.00 </emptywt>",
                        "the empty weight")
    text = replace_once(
        text, r"   <pointmass name=\"Pilot\">.*?</pointmass>\n",
        pointmass("Pilot", station(36), -36.22)
        + pointmass("Passenger", station(9), -36.22)
        + pointmass("Baggage", station(49), -30.00),
        "the pilot's seat")
    text = replace_once(
        text,
        r"(<tank type=\"FUEL\" number=\"0\">\s*<location unit=\"IN\">\s*<x>)\s*0\.00\s*(</x>.*?"
        r"<capacity unit=\"LBS\">)\s*75\.00\s*(</capacity>\s*<contents unit=\"LBS\">)\s*65\.00\s*(</contents>)",
        rf"\g<1> {station(-18):.2f} \g<2> {FUEL_LBS}.00 \g<3> {FUEL_LBS}.00 \4", "the tank")
    text = replace_once(text, r"<engine file=\"Continental A-65-8\">", f"<engine file=\"{ENGINE}\">",
                        "the engine")
    text = replace_once(text, r"<thruster file=\"CM7445 MCCauley\">", f"<thruster file=\"{PROPELLER}\">",
                        "the propeller")
    text = replace_once(
        text, r"(\n    <function name=\"aero/force/Drag_gear\">)",
        "\n    <function name=\"aero/force/Drag_airframe\">\n"
        "       <description>Drag of the fuselage, tail, struts and wires at zero lift</description>\n"
        "         <product>\n"
        "           <property>aero/qbar-psf</property>\n"
        "           <property>metrics/Sw-sqft</property>\n"
        f"           <value> {AIRFRAME_DRAG} </value>\n"
        "         </product>\n"
        "    </function>\\1", "the gear's drag")
    text = replace_once(
        text, r"(<description>Induced drag</description>.*?<value>)\s*0\.0485\s*(</value>)",
        rf"\g<1> {INDUCED_DRAG} \2", "the induced drag")
    text = replace_once(text, r"<system file=\"Conventional Controls\.xml\"/>",
                        f"<system file=\"{SYSTEM}.xml\"/>", "the flight controls")
    return text


def engine():
    text = (PINNED_CUB / "Engines" / "Continental A-65-8.xml").read_text()
    text = replace_once(text, r"(<piston_engine name=\"[^\"]*\">)",
                        r"\1\n  <!-- glideslope: JSBSim's J3Cub engine with the changes listed in\n"
                        r"       tools/make_j3cub.py, which made it. Do not edit it by hand. -->",
                        "the engine's name")
    return replace_once(text, r"<maxrpm>\s*2800\.0 </maxrpm>", "<maxrpm>      2300.0 </maxrpm>",
                        "the engine's rpm")


def propeller():
    text = (PINNED_CUB / "Engines" / "CM7445 MCCauley.xml").read_text()
    text = replace_once(text, r"(<propeller version=\"1\.01\" name=\"[^\"]*\">)",
                        r"\1\n  <!-- glideslope: JSBSim's J3Cub propeller with the changes listed in\n"
                        r"       tools/make_j3cub.py, which made it. Do not edit it by hand. -->",
                        "the propeller's name")
    text = replace_once(text, r"<diameter unit=\"IN\">\s*75\.0\s*</diameter>",
                        "<diameter unit=\"IN\"> 74.0 </diameter>", "the propeller's diameter")
    for name in ("C_THRUST", "C_POWER"):
        m = re.search(r'<table name="' + name + r'" type="internal">\s*<tableData>\s*\n(.*?)\n\s*</tableData>',
                      text, re.S)
        if not m:
            raise SystemExit(f"make_j3cub: no {name} table - has the pinned propeller changed?")
        rows = []
        for row in m.group(1).split("\n"):
            advance, value = (float(v) for v in row.split())
            scale = POWER_SCALE if name == "C_POWER" else THRUST_SCALE
            rows.append(f"      {advance * FINER_PITCH:.3f}   {value * scale:.4f}")
        text = text[:m.start(1)] + "\n".join(rows) + text[m.end(1):]
    return text


def controls():
    text = (PINNED_CUB / "Systems" / "Conventional Controls.xml").read_text()
    text = replace_once(text, r"(<system name=\"Conventional Controls\">)",
                        r"\1\n  <!-- glideslope: JSBSim's J3Cub flight controls with the changes listed\n"
                        r"       in tools/make_j3cub.py, which made them. Do not edit them by hand. -->",
                        "the flight controls' name")
    # The elevator's travel, in its control and in its normalisation - the
    # only surface of 0.14 radians.
    for element in ("range", "domain"):
        text = replace_once(
            text, r"(<" + element + r">\s*<min>)\s*-0\.14\s*(</min>\s*<max>)\s*0\.14\s*(</max>\s*</" + element + r">)",
            rf"\g<1> -{ELEVATOR_UP_RAD} \g<2> {ELEVATOR_DOWN_RAD} \3", f"the elevator's {element}")
    return text


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "aircraft" / MODEL / "Systems" / f"{SYSTEM}.xml": controls(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / f"{PROPELLER}.xml": propeller(),
    }


def main():
    check = "--check" in sys.argv[1:]
    stale = []
    for path, text in outputs().items():
        if check:
            if not path.exists() or path.read_text() != text:
                stale.append(path.relative_to(ROOT))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            print(f"wrote {path.relative_to(ROOT)}")
    if stale:
        print("assets/jsbsim/ is not what tools/make_j3cub.py makes:", file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print("run: python3 tools/make_j3cub.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
