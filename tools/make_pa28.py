#!/usr/bin/env python3
"""make_pa28.py - glideslope's Piper PA-28-180 Cherokee, made from JSBSim's pa28.

JSBSim's pa28 model, as pinned in ext/jsbsim, calls itself "PA28-180": the
constant-chord 30 ft wing of 160 sq ft, fixed gear, 40-degree flaps, and an
engine file of 180 hp at 2,700 rpm - a Cherokee 180 of 1962 to 1972 (type
certificate data sheet 2A13, section III), whose engine is the carburetted
Lycoming O-360-A3A. It was given a constant-speed propeller the Cherokee 180
never had (only the retractable Arrow has one). The figures it is held to are
the Cherokee 180 "E" Owner's Handbook's (Piper P/N 753 806, 1969, revised
1974) and the 1962 Airplane Flight Manual's, in assets/figures/pa28.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_pa28.py           write the files
    python3 tools/make_pa28.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Airframe (aircraft/pa28/pa28.xml)
    Weights and loading, from the handbook: its standard empty weight, 1,310
                         lb, where the model had 1,650; seats for the front
                         passenger and two in the back and the baggage area,
                         at the Cherokee's stations, so its 2,400 lb can be
                         loaded; two 25-gallon wing tanks, where the model had
                         one of 150 lb.
    Flaps at 10, 25 and 40 degrees
                         The Cherokee's flaps have three positions, "10, 25
                         and 40" (handbook, section II); the model's first was
                         15, and its flap tables are read at 10 instead.
    Main wheels fixed    The model's main wheels castored (max_steer 360), so
                         nothing held it straight on the ground, as JSBSim's
                         c182's did; they are fixed, and its wing tips, tail
                         and nose, which it had as wheels, are structure.
    A fixed-pitch propeller
                         The Cherokee 180's is a Sensenich M76EMMS, 76 in and
                         fixed pitch (TCDS 2A13); the model is given one, made
                         from JSBSim's fixed-pitch 75 in propeller (the base of
                         the Cessna 172P's), in engine/prop_pa28_76in.xml, with
                         the changes listed under it below.
    Gear damping         The model's struts were damped at 100 lb/ft/s (nose)
                         and 150 (mains), a tenth of the Cessna 172's at the
                         same weight; on the brakes at full throttle it bounced
                         and turned. The 172's springs and dampers.
    The stabilator: Pitch_moment_due_to_elevator_deflection -0.603 -> -1.5,
    Lift_due_to_Elevator_Deflection 0.81 -> 0.56
                         The Cherokee's tailplane is a stabilator, all of it
                         moving: about 25 sq ft on the 160 sq ft wing, with a
                         lift slope of about 4 a radian and about 2.7 chords
                         behind the centre of gravity, it gives 0.56 of lift
                         and -1.5 of pitching moment a radian - estimates of
                         the tail's size and arm, not measurements. The
                         model's moment was too weak to lift the nose at the
                         handbook's take-off speed, and its lift, for so weak a
                         moment, too great: its download at the stall took 0.2
                         off the wing's lift.
    Lift_due_to_alpha    The model's lift was 0.52 at zero incidence and rose
                         at 6 a radian to 1.91 at 18 degrees; with the
                         stabilator able to reach its peak, it stalled 7 knots
                         slow (51, against 58). Its slope is now its aspect
                         ratio's, 5.6: 4.6 a radian, from 0.30 at zero to 1.43
                         at 16 degrees; less the stabilator's download, 1.31
                         trimmed, what the flight manual's 67 mph at 2,400 lb
                         needs.
    Delta_lift_due_to_flap_deflection 0.065 / 0.016 / 0.254 -> 0.18 / 0.40 / 0.56
                         The model's flaps gave less lift at 25 degrees than
                         at 10; they rise with the flap now, to what the
                         flight manual's 57 mph stall with 40 degrees needs.
    Drag_induced         A table of angle of attack, which the lift curve's
                         change would have moved; it is now the lift squared
                         over pi, the aspect ratio and an efficiency of 0.75,
                         and, near the ground, less by McCormick's ground
                         effect, (16 h/b)^2 / (1 + (16 h/b)^2): the model had
                         ground effect on its lift but none on its drag.
    Drag_at_zero_lift 0.019 -> 0.027, Drag_due_to_landing_gear 0.030 -> 0.007
                         The gear's drag was charged by the gear's position,
                         which glideslope keeps down for fixed gear; with it,
                         the model was 25 knots slow (107, against 132). The
                         two together are now what the handbook's top speed
                         needs; the gear's share of it is an estimate.

  Propeller (engine/prop_pa28_76in.xml)
    Diameter 75 -> 76 in The Sensenich M76EMMS's.
    C_POWER x0.91 at advance ratio <= 0.3, fading to x1.0 at 0.5
                         At full throttle on the ground it turned 2,250 rpm,
                         under the type certificate's 2,275; now 2,350. The
                         climb's and cruise's rpm are unchanged.
    C_THRUST x1.25 at advance ratio <= 0.3, fading to x1.0 at 0.55
                         JSBSim's generic fixed-pitch propeller gave 427 lb of
                         static thrust, 2.7 lb a horsepower: the take-off ran
                         39% long (1,004 ft, against 720), and the climb, at an
                         advance ratio about 0.5, was 15% short (620 ft/min,
                         against 725). Now 534 lb, 3.4 lb a horsepower.

The engine (engine/engIO360C.xml) is copied unchanged: 180 hp at 2,700 rpm
is the O-360-A3A's rating.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PINNED = ROOT / "ext" / "jsbsim"
OUT = ROOT / "assets" / "jsbsim"

EMPTY_WEIGHT_LBS = 1310
TANK_LBS = 150
PROPELLER = "prop_pa28_76in"
STABILATOR = "-1.5000"
ZERO_LIFT_DRAG = "0.027"
GEAR_DRAG = "0.0070"
LIFT_DUE_TO_ALPHA = {
    -0.175: -0.52, -0.14: -0.36, -0.105: -0.19, -0.07: -0.03, -0.035: 0.14, 0.0: 0.30,
    0.035: 0.46, 0.07: 0.62, 0.105: 0.78, 0.14: 0.94, 0.175: 1.10, 0.209: 1.24,
    0.244: 1.36, 0.279: 1.43, 0.314: 1.37, 0.349: 1.22,
}
FLAP_LIFT = {0.0: 0.0, 10.0: 0.18, 25.0: 0.40, 40.0: 0.56}
STABILATOR_LIFT = "0.5600"

INDUCED_DRAG = "0.0755"   # 1 / (pi x 5.625 x 0.75)
LOW_J_POWER = 0.91
LOW_J_THRUST = 1.25
# Ground effect on induced drag, McCormick's (16 h/b)^2 / (1 + (16 h/b)^2), by
# the wing's height over its span.
GROUND_EFFECT_HEIGHTS = (0.0, 0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.6, 0.8, 1.0, 1.1)


def replace_once(text, pattern, replacement, what):
    new, n = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f"make_pa28: could not find {what} - has the pinned model changed?")
    return new


def table(text, description):
    """The first <tableData> after <description>: its span and its rows."""
    m = re.search(r"<description>" + re.escape(description) +
                  r"</description>.*?<tableData>\s*\n(.*?)\n\s*</tableData>", text, re.S)
    if not m:
        raise SystemExit(f"make_pa28: no table after '{description}' - has the pinned model changed?")
    return m


def with_rows(text, m, rows):
    return text[:m.start(1)] + "\n".join(rows) + text[m.end(1):]


def pointmass(name, x, y, z):
    return (f"        <pointmass name=\"{name}\">\n"
            f"            <weight unit=\"LBS\"> 0 </weight>\n"
            f"            <location unit=\"IN\">\n"
            f"                <x> {x} </x>\n"
            f"                <y> {y} </y>\n"
            f"                <z> {z} </z>\n"
            f"            </location>\n"
            f"        </pointmass>\n")


def tank(number, y):
    return (f"        <tank type=\"FUEL\">    <!-- Tank number {number} -->\n"
            f"            <location unit=\"IN\">\n"
            f"                <x> 95 </x>\n"
            f"                <y> {y} </y>\n"
            f"                <z> 0 </z>\n"
            f"            </location>\n"
            f"            <capacity unit=\"LBS\"> {TANK_LBS} </capacity>\n"
            f"            <contents unit=\"LBS\"> {TANK_LBS} </contents>\n"
            f"        </tank>\n")


def airframe():
    text = (PINNED / "aircraft" / "pa28" / "pa28.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n        <!-- glideslope: this is JSBSim's pa28 with the changes listed in\n"
        r"             tools/make_pa28.py, which made it. Do not edit it by hand. -->",
        "the file header")
    text = replace_once(text, r"<emptywt unit=\"LBS\"> 1650 </emptywt>",
                        f"<emptywt unit=\"LBS\"> {EMPTY_WEIGHT_LBS} </emptywt>", "the empty weight")
    # The pilot's seat is the model's own; the others at the Cherokee's
    # stations, the front seats beside it.
    text = replace_once(
        text, r"(<pointmass name=\"name\">.*?</pointmass>\n)",
        r"\1" + pointmass("Co-Pilot", 81.6, 9.6, 0) + pointmass("Left Passenger", 118.1, -9.6, 0)
        + pointmass("Right Passenger", 118.1, 9.6, 0) + pointmass("Baggage", 142.8, 0, 0),
        "the pilot's seat")
    text = text.replace("<pointmass name=\"name\">", "<pointmass name=\"Pilot\">", 1)
    text = replace_once(
        text, r"        <tank type=\"FUEL\">    <!-- Tank number 0 -->.*?</tank>\n",
        tank(0, -60) + tank(1, 60), "the tank")
    text = replace_once(text, r"(<engine file=\"engIO360C\">\s*<feed>0</feed>)",
                        r"\1\n            <feed>1</feed>", "the engine's feed")
    text = replace_once(text, r"<thruster file=\"propC8v\">", f"<thruster file=\"{PROPELLER}\">",
                        "the propeller")

    text = replace_once(
        text, r"(<setting>\s*<position>)15(</position>\s*<time>2</time>)", r"\g<1>10\2",
        "the flaps' first detent")
    for description in ("Delta_drag_due_to_flap_deflection",
                        "Delta_lift_due_to_flap_deflection",
                        "Delta_pitching_moment_due_to_flap_deflection"):
        text = replace_once(
            text, r"(<description>" + description + r"</description>.*?<tableData>.*?)15\.0000",
            r"\g<1>10.0000", f"the 15 degree row of {description}")

    m = table(text, "Lift_due_to_alpha")
    rows = []
    for row in m.group(1).split("\n"):
        alpha = round(float(row.split()[0]), 3)
        if alpha not in LIFT_DUE_TO_ALPHA:
            raise SystemExit(f"make_pa28: the pinned lift table has alpha {alpha}, which this script has no value for")
        rows.append(f"                              {row.split()[0]}\t{LIFT_DUE_TO_ALPHA[alpha]:.4f}")
    text = with_rows(text, m, rows)
    m = table(text, "Delta_lift_due_to_flap_deflection")
    rows = []
    for row in m.group(1).split("\n"):
        flap = float(row.split()[0])
        rows.append(f"                              {row.split()[0]}\t{FLAP_LIFT[flap]:.4f}")
    text = with_rows(text, m, rows)
    text = replace_once(
        text,
        r"(<description>Drag_induced</description>\s*<product>\s*<property>aero/qbar-psf</property>\s*"
        r"<property>metrics/Sw-sqft</property>\s*)<table>.*?</table>",
        r"\g<1><property>aero/cl-squared</property>\n"
        r"                    <property>aero/function/kCDge</property>\n"
        r"                    <value>" + INDUCED_DRAG + "</value>",
        "the induced drag table")
    rows = []
    for h in GROUND_EFFECT_HEIGHTS:
        x = (16 * h) ** 2
        rows.append(f"                    {h:.4f}\t{x / (1 + x):.4f}")
    text = replace_once(
        text, r"(\n        <function name=\"aero/function/kCLge\">)",
        "\n        <function name=\"aero/function/kCDge\">\n"
        "            <description>Change_in_induced_drag_due_to_ground_effect</description>\n"
        "            <table>\n"
        "                <independentVar>aero/h_b-mac-ft</independentVar>\n"
        "                <tableData>\n" + "\n".join(rows).replace("\\", "\\\\") + "\n"
        "                </tableData>\n"
        "            </table>\n"
        "        </function>\n\\1", "the ground effect on lift")

    text = replace_once(
        text, r"(<description>Pitch_moment_due_to_elevator_deflection</description>.*?<value>)-0\.6030(</value>)",
        r"\g<1>" + STABILATOR + r"\2", "the stabilator's power")
    text = replace_once(
        text, r"(<description>Lift_due_to_Elevator_Deflection</description>.*?<value>)0\.8100(</value>)",
        r"\g<1>" + STABILATOR_LIFT + r"\2", "the stabilator's lift")
    text = replace_once(
        text, r"(<description>Drag_at_zero_lift</description>.*?<value>)0\.019(</value>)",
        r"\g<1>" + ZERO_LIFT_DRAG + r"\2", "the zero-lift drag")
    text = replace_once(
        text, r"(<description>Drag_due_to_landing_gear</description>.*?<value>)0\.0300(</value>)",
        r"\g<1>" + GEAR_DRAG + r"\2", "the gear's drag")
    text = replace_once(
        text, r"(name=\"NOSE\">.*?<spring_coeff unit=\"LBS/FT\">)\s*1500\s*(</spring_coeff>\s*<damping_coeff unit=\"LBS/FT/SEC\">)\s*100\s*(</damping_coeff>)",
        r"\g<1> 1800 \g<2> 600 \3", "the nose strut")
    for side in ("LEFT_MAIN", "RIGHT_MAIN"):
        text = replace_once(
            text, r"(name=\"" + side + r"\">.*?<spring_coeff unit=\"LBS/FT\">)\s*5000\s*(</spring_coeff>\s*<damping_coeff unit=\"LBS/FT/SEC\">)\s*150\s*(</damping_coeff>)",
            r"\g<1> 5400 \g<2> 1600 \3", f"the {side} strut")
    for side in ("LEFT_MAIN", "RIGHT_MAIN"):
        text = replace_once(
            text, r"(name=\"" + side + r"\">.*?<max_steer unit=\"DEG\">) 360\.0 (</max_steer>)",
            r"\1 0.0 \2", f"the {side} wheel's steering")
    for contact in ("TAIL_SKID", "VTAIL_TOP", "VTAIL_TIP", "LEFT_TIP", "RIGHT_TIP", "NOSE_TIP"):
        text = replace_once(
            text,
            r"<contact type=\"BOGEY\" name=\"" + contact + r"\">(.*?)\s*<max_steer unit=\"DEG\"> 0\.0 </max_steer>"
            r"(\s*<brake_group> NONE </brake_group>)?(\s*<retractable>0</retractable>)?",
            r'<contact type="STRUCTURE" name="' + contact + r'">\1', f"the {contact} contact")
    return text


def propeller():
    text = (PINNED / "engine" / "prop_75in2f.xml").read_text()
    text = replace_once(text, r"<propeller name=\"[^\"]*\">",
                        "<propeller name=\"Sensenich M76EMMS, 76 in fixed pitch\">\n"
                        "  <!-- glideslope: JSBSim's prop_75in2f with the changes listed in\n"
                        "       tools/make_pa28.py, which made it. Do not edit it by hand. -->",
                        "the propeller's name")
    text = replace_once(text, r"<diameter unit=\"IN\">\s*75(\.0)?\s*</diameter>",
                        "<diameter unit=\"IN\"> 76 </diameter>", "the propeller's diameter")
    text = scaled(text, "C_THRUST", LOW_J_THRUST, 0.55)
    return scaled(text, "C_POWER", LOW_J_POWER, 0.5)


def scaled(text, name, low_advance, full_at):
    """The propeller's table `name`, times `low_advance` up to an advance ratio
    of 0.3 and fading to times 1 at `full_at`."""
    m = re.search(r'(?<!<!--)<table name="' + name + r'" type\s*=\s*"internal">\s*<tableData>\s*\n(.*?)\n\s*</tableData>',
                  text, re.S)
    if not m:
        raise SystemExit(f"make_pa28: no {name} table - has the pinned propeller changed?")
    rows = []
    for row in m.group(1).split("\n"):
        advance, value = (float(v) for v in row.split())
        k = 1.0 if advance >= full_at else \
            low_advance + (1.0 - low_advance) * max(0.0, advance - 0.3) / (full_at - 0.3)
        rows.append(f"      {row.split()[0]}   {value * k:.4f}")
    return text[:m.start(1)] + "\n".join(rows) + text[m.end(1):]


def outputs():
    return {
        OUT / "aircraft" / "pa28" / "pa28.xml": airframe(),
        OUT / "engine" / "engIO360C.xml": (PINNED / "engine" / "engIO360C.xml").read_text(),
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
        print("assets/jsbsim/ is not what tools/make_pa28.py makes:", file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print("run: python3 tools/make_pa28.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
