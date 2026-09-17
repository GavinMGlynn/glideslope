#!/usr/bin/env python3
"""make_c172p.py - glideslope's Cessna 172P, made from JSBSim's.

JSBSim's own c172p model, as pinned in ext/jsbsim, does not fly to the Cessna
172P Pilot's Operating Handbook (12 May 1981). Flown by the published-figure
checks (src/sim/figures.cpp, assets/figures/c172p.xml) at 2400 lb, it lands
out of range on five of nine:

                        stock      handbook
    static RPM          2538       2300 to 2420
    climb rate          978 fpm    700
    stall, flaps up     54.4 KCAS  51 to 52
    stall, flaps 10     51.0 KCAS  48 to 49
    stall, flaps 30     48.8 KCAS  46

and in range but not close on two more: glide 8.4:1 (9.1) and cruise
123.1 KTAS (121). With every change below it lands in range on all nine.

This script writes assets/jsbsim/ from the pinned files with those changes and
nothing else, so what glideslope flies is always the pinned model plus a list
of edits that can be read, argued with and re-made. The committed output is
checked against this script by a test; to change the aircraft, change this
script and run it.

    python3 tools/make_c172p.py           write assets/jsbsim/
    python3 tools/make_c172p.py --check   exit 1 if assets/jsbsim/ differs

The changes, and what each is for. The numbers in brackets are the checks'
measurements with every change except that one, against all of them together
(see docs/PROJECT_STATUS.md for the full table):

  Propeller (engine/prop_75in2f.xml)
    cp_factor 1.22, ct_factor 0.955
                         The propeller absorbed too little power, so the engine
                         over-revved and climbed too fast (static 2538 RPM,
                         climb 1147 fpm, against 2316 and 742).
    C_THRUST x1.35 at advance ratio <= 0.2, fading to x1.0 at 0.5
                         Lowering the RPM lowered static thrust with it, and the
                         ground roll grew (1216 ft, against 922). Take-off runs
                         at advance ratios below 0.4; climb, cruise and glide run
                         above 0.5 and are untouched by it.

  Airframe (aircraft/c172p/c172p.xml)
    CDo 0.027 -> 0.031, and Drag_due_to_alpha x0.72 in every flap column
                         At the glide's angle of attack the table gave about
                         0.092, where this wing's induced drag is nearer 0.054,
                         so the glide was too steep (7.9:1, against 9.4:1); the
                         higher zero-lift drag pays back the cruise speed the
                         smaller table would otherwise add.
    Lift_due_to_alpha, unstalled column
                         Keeps its linear slope (5.3 per radian) to 0.16 rad and
                         rounds over to a maximum of 1.68 at 0.28 rad, where the
                         original peaked at 1.47. The stall the handbook gives at
                         2400 lb needs a maximum near 1.68; the original's was
                         reached only as the elevator ran out of travel.
    Delta_lift_due_to_flap_deflection 0.20/0.30/0.35 -> 0.25/0.38/0.42
                         For the stalls with 10 and 30 degrees of flap.

The engine file (engine/eng_io320.xml) is copied unchanged.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PINNED = ROOT / "ext" / "jsbsim"
OUT = ROOT / "assets" / "jsbsim"

CP_FACTOR = 1.22
CT_FACTOR = 0.955
LOW_J_THRUST = 1.35
CDO = "0.031"
DRAG_DUE_TO_ALPHA_SCALE = 0.72
LIFT_UNSTALLED = {
    -0.09: -0.22, 0.00: 0.25, 0.09: 0.73, 0.10: 0.78, 0.12: 0.886, 0.14: 0.99,
    0.16: 1.10, 0.17: 1.15, 0.19: 1.26, 0.21: 1.37, 0.24: 1.52, 0.26: 1.61,
    0.28: 1.68, 0.30: 1.66, 0.32: 1.58, 0.34: 1.45, 0.36: 1.28,
}
FLAP_LIFT = ("0.2500", "0.3800", "0.4200")


def table_rows(text, description):
    """The span of rows in the first <tableData> after <description>."""
    m = re.search(r"<description>" + re.escape(description) +
                  r"</description>.*?<tableData>\s*\n(.*?)\n\s*</tableData>", text, re.S)
    if not m:
        raise SystemExit(f"make_c172p: no table after '{description}' - has the pinned model changed?")
    return m


def replace_once(text, pattern, replacement, what):
    new, n = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f"make_c172p: could not find {what} - has the pinned model changed?")
    return new


def propeller():
    text = (PINNED / "engine" / "prop_75in2f.xml").read_text()
    text = replace_once(
        text, r"<numblades> 2 </numblades>",
        f"<numblades> 2 </numblades>\n"
        f"  <!-- glideslope: see tools/make_c172p.py -->\n"
        f"  <cp_factor> {CP_FACTOR} </cp_factor>\n"
        f"  <ct_factor> {CT_FACTOR} </ct_factor>",
        "the propeller's blade count")
    m = re.search(r'<table name="C_THRUST" type="internal">\s*<tableData>\s*\n(.*?)\n\s*</tableData>',
                  text, re.S)
    if not m:
        raise SystemExit("make_c172p: no C_THRUST table - has the pinned propeller changed?")
    rows = []
    for row in m.group(1).split("\n"):
        j, ct = row.split()
        advance = float(j)
        if advance <= 0.2:
            k = LOW_J_THRUST
        elif advance >= 0.5:
            k = 1.0
        else:
            k = LOW_J_THRUST + (1.0 - LOW_J_THRUST) * (advance - 0.2) / 0.3
        rows.append(f"      {j}   {float(ct) * k:.4f}")
    return text[:m.start(1)] + "\n".join(rows) + text[m.end(1):]


def airframe():
    text = (PINNED / "aircraft" / "c172p" / "c172p.xml").read_text()
    text = replace_once(
        text, r"(<description>Drag_at_zero_lift</description>.*?<value>)0\.027(</value>)",
        r"\g<1>" + CDO + r"\2", "zero-lift drag")

    m = table_rows(text, "Drag_due_to_alpha")
    head, *rows = m.group(1).split("\n")
    scaled = [head]
    for row in rows:
        v = row.split()
        scaled.append("                              " +
                      "\t".join([v[0]] + [f"{float(c) * DRAG_DUE_TO_ALPHA_SCALE:.4f}" for c in v[1:]]))
    text = text[:m.start(1)] + "\n".join(scaled) + text[m.end(1):]

    m = table_rows(text, "Lift_due_to_alpha")
    head, *rows = m.group(1).split("\n")
    lifted = [head]
    for row in rows:
        alpha, _, stalled = row.split()
        a = round(float(alpha), 2)
        if a not in LIFT_UNSTALLED:
            raise SystemExit(f"make_c172p: the pinned lift table has alpha {alpha}, which this script has no value for")
        # The two columns meet where the stall hysteresis ends, so the lift does
        # not jump there.
        stalled_value = LIFT_UNSTALLED[a] if a == 0.36 else float(stalled)
        lifted.append(f"                              {float(alpha):.4f}\t{LIFT_UNSTALLED[a]:.4f}\t{stalled_value:.4f}")
    text = text[:m.start(1)] + "\n".join(lifted) + text[m.end(1):]

    text = replace_once(
        text,
        r"(<description>Delta_lift_due_to_flap_deflection</description>.*?<tableData>\s*\n"
        r"\s*0\.0000\s+0\.0000\s*\n\s*10\.0000\s+)0\.2000(\s*\n\s*20\.0000\s+)0\.3000(\s*\n\s*30\.0000\s+)0\.3500",
        r"\g<1>" + FLAP_LIFT[0] + r"\g<2>" + FLAP_LIFT[1] + r"\g<3>" + FLAP_LIFT[2],
        "the flap lift table")
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n        <!-- glideslope: this is JSBSim's c172p with the changes listed in\n"
        r"             tools/make_c172p.py, which made it. Do not edit it by hand. -->",
        "the file header")
    return text


def outputs():
    return {
        OUT / "aircraft" / "c172p" / "c172p.xml": airframe(),
        OUT / "engine" / "prop_75in2f.xml": propeller(),
        OUT / "engine" / "eng_io320.xml": (PINNED / "engine" / "eng_io320.xml").read_text(),
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
        print("assets/jsbsim/ is not what tools/make_c172p.py makes:", file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print("run: python3 tools/make_c172p.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
