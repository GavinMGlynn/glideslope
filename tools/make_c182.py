#!/usr/bin/env python3
"""make_c182.py - glideslope's Cessna 182S, made from JSBSim's c182.

JSBSim's c182 model, as pinned in ext/jsbsim (its README.c182: a 182S, type
certificate approved 1996), is its c172p with a 230 hp Lycoming IO-540-AB1A5
and a constant-speed propeller: the same inertias, geometry and centre of
gravity. The figures it is held to are the Cessna Model 182S Skylane
Information Manual's (P/N 182SIM, 1997, the Pilot's Operating Handbook of 3
February 1997 with revision 4), in assets/figures/c182.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_c182.py           write the files
    python3 tools/make_c182.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Airframe (aircraft/c182/c182.xml)
    Weights and loading, from the handbook: the standard empty weight, 1,925
                         lb where the model had 1,700; seats for the front
                         passenger and two in the back and the baggage area
                         (the model had only the pilot's), so the handbook's
                         3,100 lb can be loaded; each tank 46 gallons, 276 lb
                         of fuel, where the model had 300 lb.
    Flaps to 38 degrees  The 182S's flaps travel to FULL, 38 degrees (type
                         certificate data sheet 3A13), with detents at 10 and
                         20; the model's stopped at 30.
    Main wheels fixed    The model's main wheels castored (max_steer 360), so
                         on the ground nothing held the aircraft straight: at
                         full throttle it turned circles, and lightly loaded
                         its state went to NaN. They are fixed, as the 172's
                         are, and the tail skid and wing tips, which it had as
                         wheels, are structure, as the 172's are.

    Lift_due_to_alpha    Keeps its slope to 0.14 rad and rises to a maximum of
                         1.41 at 0.28 rad, where the original peaked at 1.16:
                         with the lift at zero alpha, 0.307, a maximum lift
                         coefficient of 1.72, what the handbook's stalling
                         speeds at 3,100 lb need; the original stalled 5 knots
                         fast in every configuration.
    Flaps to 38 degrees in the flap tables
                         The lift, drag and pitching moment the model gives
                         for flap stop at 30 degrees; each carries on to 38 at
                         its slope from 20 to 30, and the lift at 38 is 0.44
                         (from 0.35 at 30) for the stall with full flap.
    Drag_due_to_Elevator_Deflection 0.06 -> 0.02
                         The model trims with the elevator, 7.5 degrees down
                         at full speed, and charged it a fifth of all the drag
                         there. The elevator's own profile drag over that
                         deflection is a seventh of that; with the tailplane's
                         induced drag from the load it carries in trim, 0.02.
    Drag_due_to_alpha x1.15
                         The glide was too flat (9.9:1, against 8.9). The
                         handbook's glide is flown with the propeller
                         windmilling, and in JSBSim the propeller of an engine
                         that has stopped stops too - its tables end where it
                         would begin to windmill - so the drag at the glide's
                         angle of attack stands in for the windmilling
                         propeller's, as the 172's does (tools/make_c172p.py).
    A stopped engine's friction
                         JSBSim charges a running engine its friction (its
                         mean effective pressure, about 50 hp at 2,400 rpm
                         for this one) but not a stopped one. A channel
                         charges the stopped engine the same, by rpm, so that
                         an engine that fails in flight stops as it should.

  Propeller (engine/prop_81in2v.xml)
    C_THRUST x1.12 at advance ratio <= 0.3, fading to x1.0 at 0.8
                         The take-off ran 13% long (897 ft, against 795):
                         the static thrust was 3.3 lb a horsepower. The climb
                         (advance ratio about 0.5) gains 4%; cruise and top
                         speed run above 0.85 and are untouched.

The engine file (engine/engIO540AB1A5.xml) is copied unchanged.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PINNED = ROOT / "ext" / "jsbsim"
OUT = ROOT / "assets" / "jsbsim"

EMPTY_WEIGHT_LBS = 1925
TANK_LBS = 276
FULL_FLAPS_DEG = 38
LIFT_DUE_TO_ALPHA = {
    -0.09: -0.527, 0.00: -0.057, 0.14: 0.713, 0.21: 1.07, 0.24: 1.22, 0.26: 1.32,
    0.28: 1.41, 0.30: 1.39, 0.32: 1.31, 0.34: 1.17, 0.36: 0.85,
}
FULL_FLAP_LIFT = 0.44
ELEVATOR_DRAG = "0.02"
DRAG_DUE_TO_ALPHA_SCALE = 1.15
LOW_J_THRUST = 1.12


def replace_once(text, pattern, replacement, what):
    new, n = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f"make_c182: could not find {what} - has the pinned model changed?")
    return new


def table(text, description):
    """The first <tableData> after <description>: its span and its rows."""
    m = re.search(r"<description>" + re.escape(description) +
                  r"</description>.*?<tableData>\s*\n(.*?)\n\s*</tableData>", text, re.S)
    if not m:
        raise SystemExit(f"make_c182: no table after '{description}' - has the pinned model changed?")
    return m


def with_rows(text, m, rows):
    return text[:m.start(1)] + "\n".join(rows) + text[m.end(1):]


def extend_flap_rows(text, description):
    """A table of one flap column: a row for 38 degrees at the slope from 20
    to 30."""
    m = table(text, description)
    rows = m.group(1).split("\n")
    values = {float(r.split()[0]): float(r.split()[1]) for r in rows}
    v38 = values[30.0] + 0.8 * (values[30.0] - values[20.0])
    return with_rows(text, m, rows + [f"                              38.0000\t{v38:.4f}"])


def pointmass(name, x, y, z):
    return (f"        <pointmass name=\"{name}\">\n"
            f"            <weight unit=\"LBS\"> 0 </weight>\n"
            f"            <location unit=\"IN\">\n"
            f"                <x> {x} </x>\n"
            f"                <y> {y} </y>\n"
            f"                <z> {z} </z>\n"
            f"            </location>\n"
            f"        </pointmass>\n")


def airframe():
    text = (PINNED / "aircraft" / "c182" / "c182.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n        <!-- glideslope: this is JSBSim's c182 with the changes listed in\n"
        r"             tools/make_c182.py, which made it. Do not edit it by hand. -->",
        "the file header")
    text = replace_once(text, r"<emptywt unit=\"LBS\"> 1700 </emptywt>",
                        f"<emptywt unit=\"LBS\"> {EMPTY_WEIGHT_LBS} </emptywt>",
                        "the empty weight")
    # The pilot's seat is the model's own; the others are where the 172's are.
    text = replace_once(
        text, r"(<pointmass name=\"name\">.*?</pointmass>\n)",
        r"\1" + pointmass("Co-Pilot", 36, 14, 24) + pointmass("Left Passenger", 70, -14, 24)
        + pointmass("Right Passenger", 70, 14, 24) + pointmass("Baggage", 95, 0, 24),
        "the pilot's seat")
    text = text.replace("<pointmass name=\"name\">", "<pointmass name=\"Pilot\">", 1)
    for tank in range(2):
        text = replace_once(
            text, r"(<!-- Tank number " + str(tank) + r" -->.*?)<capacity unit=\"LBS\"> 300 </capacity>",
            r'\g<1><capacity unit="LBS"> ' + str(TANK_LBS) + r" </capacity>",
            f"tank {tank}'s capacity")
    text = replace_once(
        text,
        r"(<setting>\s*<position>20</position>\s*<time>1</time>\s*</setting>\s*<setting>\s*<position>)30(</position>)",
        r"\g<1>" + str(FULL_FLAPS_DEG) + r"\2", "the flaps' last detent")
    text = replace_once(
        text,
        r"(<input>fcs/flap-pos-deg</input>\s*<domain>\s*<min>0</min>\s*<max>)30(</max>)",
        r"\g<1>" + str(FULL_FLAPS_DEG) + r"\2", "the flap position normaliser")
    for side in ("LEFT_MAIN", "RIGHT_MAIN"):
        text = replace_once(
            text, r"(name=\"" + side + r"\">.*?<max_steer unit=\"DEG\">) 360\.0 (</max_steer>)",
            r"\1 0.0 \2", f"the {side} wheel's steering")
    for skid in ("TAIL_SKID", "LEFT_TIP", "RIGHT_TIP"):
        text = replace_once(
            text,
            r"<contact type=\"BOGEY\" name=\"" + skid + r"\">(.*?)\s*<max_steer unit=\"DEG\"> 0\.0 </max_steer>"
            r"\s*<brake_group> NONE </brake_group>\s*<retractable>0</retractable>",
            r'<contact type="STRUCTURE" name="' + skid + r'">\1', f"the {skid} contact")

    m = table(text, "Lift_due_to_alpha")
    rows = []
    for row in m.group(1).split("\n"):
        alpha = round(float(row.split()[0]), 2)
        if alpha not in LIFT_DUE_TO_ALPHA:
            raise SystemExit(f"make_c182: the pinned lift table has alpha {alpha}, which this script has no value for")
        rows.append(f"                              {float(row.split()[0]):.4f}\t{LIFT_DUE_TO_ALPHA[alpha]:.4f}")
    text = with_rows(text, m, rows)

    m = table(text, "Lift_due_to_flap_deflection")
    rows = m.group(1).split("\n")
    text = with_rows(text, m, rows + [f"                              38.0000\t{FULL_FLAP_LIFT:.4f}"])
    text = extend_flap_rows(text, "Pitch_moment_due_to_flap_deflection")

    m = table(text, "Drag_due_to_flap_deflection")
    head, *rows = m.group(1).split("\n")
    extended = [head + "\t38.0000"]
    for row in rows:
        v = row.split()
        v38 = float(v[4]) + 0.8 * (float(v[4]) - float(v[3]))
        extended.append(row + f"\t{v38:.4f}")
    text = with_rows(text, m, extended)

    m = table(text, "Drag_due_to_alpha")
    rows = []
    for row in m.group(1).split("\n"):
        alpha, cd = row.split()
        rows.append(f"                              {alpha}\t{float(cd) * DRAG_DUE_TO_ALPHA_SCALE:.4f}")
    text = with_rows(text, m, rows)

    text = replace_once(
        text, r"(<description>Drag_due_to_Elevator_Deflection</description>.*?<value>)0\.06(</value>)",
        r"\g<1>" + ELEVATOR_DRAG + r"\2", "the elevator's drag")
    text = replace_once(text, r"(\n)(    </flight_control>)", r"\1" + STOPPED_ENGINE_FRICTION + r"\2",
                        "the end of the flight controls")
    return text


# JSBSim's friction mean effective pressure, (18,400 x mean piston speed in m/s
# + 46,500) Pa, over this engine's 540 cubic inches and JSBSim's default
# 4.375 in stroke (the engine file gives none), a quarter of the revolutions:
# 6.740e-6 rpm^2 + 0.0045984 rpm horsepower. 1.5 hp is JSBSim's own static
# friction, which the channel replaces.
STOPPED_ENGINE_FRICTION = """        <channel name="Stopped engine friction">
            <!-- glideslope: see tools/make_c182.py -->
            <fcs_function name="fcs/engine-friction-hp">
                <function>
                    <sum>
                        <value>1.5</value>
                        <product>
                            <difference>
                                <value>1</value>
                                <property>propulsion/engine[0]/set-running</property>
                            </difference>
                            <property>propulsion/engine[0]/engine-rpm</property>
                            <sum>
                                <product>
                                    <value>0.000006740</value>
                                    <property>propulsion/engine[0]/engine-rpm</property>
                                </product>
                                <value>0.0045984</value>
                            </sum>
                        </product>
                    </sum>
                </function>
                <output>propulsion/engine[0]/friction-hp</output>
            </fcs_function>
        </channel>
"""


def low_advance_thrust(advance):
    if advance <= 0.3:
        return LOW_J_THRUST
    if advance >= 0.8:
        return 1.0
    return LOW_J_THRUST + (1.0 - LOW_J_THRUST) * (advance - 0.3) / 0.5


def propeller():
    text = (PINNED / "engine" / "prop_81in2v.xml").read_text()
    text = replace_once(text, r"(<propeller name=\"[^\"]*\">)",
                        r"\1\n  <!-- glideslope: C_THRUST changed by tools/make_c182.py -->",
                        "the propeller's name")
    m = re.search(r'<table name="C_THRUST" type="internal">\s*<tableData>\s*\n(.*?)\n\s*</tableData>',
                  text, re.S)
    if not m:
        raise SystemExit("make_c182: no C_THRUST table - has the pinned propeller changed?")
    head, *rows = m.group(1).split("\n")
    scaled = [head]
    for row in rows:
        v = row.split()
        k = low_advance_thrust(float(v[0]))
        scaled.append(f"      {v[0]}  " + "  ".join(f"{float(c) * k:.4f}" for c in v[1:]))
    text = text[:m.start(1)] + "\n".join(scaled) + text[m.end(1):]
    return text


def outputs():
    return {
        OUT / "aircraft" / "c182" / "c182.xml": airframe(),
        OUT / "engine" / "engIO540AB1A5.xml": (PINNED / "engine" / "engIO540AB1A5.xml").read_text(),
        OUT / "engine" / "prop_81in2v.xml": propeller(),
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
        print("assets/jsbsim/ is not what tools/make_c182.py makes:", file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print("run: python3 tools/make_c182.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
