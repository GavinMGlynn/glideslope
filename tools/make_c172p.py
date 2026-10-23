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
    cp_factor 0.88, ct_factor 0.96
                         Sized for the engine's 160 hp (below): static 2,351
                         RPM, climb 763 fpm, cruise 119.6 KTAS. With the
                         pinned factors of 1.0 the propeller absorbs too
                         little and the engine over-revs.
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

  Engine (engine/eng_io320.xml)
    <bsfc> 0.32 removed  The handbook (section 1) rates the O-320-D2J at "160
                         rated BHP at 2700 RPM". JSBSim's piston engine makes
                         power as the fuel burnt over the bsfc, and 0.32
                         lb/hp/h - below what any petrol engine burns at full
                         rich - made 209-222 hp; without it JSBSim sizes the
                         engine to its maxhp, 160.
    MIXTURE table        The FAA's mixture curve, for JSBSim's own, which made
                         most power at 9.9 parts of air to one of fuel, found
                         by leaning high up; see tools/piston_mixture.py.

  Carburettor (aircraft/c172p/c172p.xml)
    Float carburettor    The O-320-D2J is carburetted (section 1), so its
                         mixture richens with height as the square root of the
                         density, not as the pressure; see tools/piston_mixture.py.

  Tail strike (aircraft/c172p/c172p.xml)
    TAIL_SKID -> TAIL_STRIKE
                         Her tail strikes where her airframe does: measured
                         from her visual model by tools/ground.py, 11.7
                         degrees. The source's skid sat 8 in below her drawn
                         tail and struck at 10.3.

**Her elevator in the part of the slipstream that reaches it** (2026-10-16):
Cmde. The pinned model puts her elevator's moment on aero/function/qbar-induced-
psf, the dynamic pressure of the fully grown slipstream, u + 2v (v the
propeller's induced velocity), as if the whole tailplane sat in it. Standing at
full power that is the static thrust over the disc area, 16.8 psf, on all of her
elevator: stick full back +7,860 ft-lb, which reared her onto her tail strike at
11.2 degrees on held brakes at every loading, and stick full forward -6,200
ft-lb, which lifted her main wheels and put her over her nose. Momentum theory
(Glauert; McCormick, "Aerodynamics, Aeronautics, and Flight Mechanics", on the
propeller's momentum theory) has the far slipstream at twice the induced
velocity with half the disc's area, so its diameter is the propeller's over
root two. The handbook's Figure 1-1 (Three View, page 1-2) gives the propeller,
75 in, and the horizontal tail's span, 11 ft 4 in: the slipstream, 53.0 in
across, covers 0.390 of that span. Her elevator now works on the free stream's
dynamic pressure plus that fraction of the slipstream's excess over it,
aero/function/qbar-tail-psf. Assumed, not measured: the slipstream meets the
tailplane centred and uncontracted further, and the tailcone takes nothing out
of it. In flight her induced velocity is small beside her airspeed and the two
nearly agree; at a standstill her elevator has 0.39 of what it had. The selftest
flies her, and its hash moved with this.

**Her rudder in the part of the slipstream that reaches her fin** (2026-10-20):
Cndr. The pinned model puts her rudder's moment on the same fully grown
slipstream, as if her whole fin sat in it. The slipstream is the elevator's,
53.0 in across about her thrust line; her tailcone, not her fin, is below that
line at her tail. The handbook's Figure 1-1 (Three View, page 1-2, the 1985
printing in docs/ASSETS.md), its side view scaled by its own 8 ft 9 1/2 in
height, has her thrust line (the spinner's centre) 52.9 in above the ground, her
fin's root on the tailcone 50.7 in and its tip, below the beacon, 103.4 in: the
slipstream's upper half reaches 79.4 in, and covers 0.545 of the fin's 52.7 in
span. Her rudder now works on the free stream's dynamic pressure plus that
fraction of the slipstream's excess over it, aero/function/qbar-fin-psf.
Assumed, not measured: the slipstream runs back level with her thrust line,
unswirled and uncontracted further, and the heights are read off the drawing to
about half an inch, about 0.01 of the share. At a standstill her rudder has
0.55 of what it had; in flight the two nearly agree. The selftest flies her
with her rudder central, where the term is nothing, and its hash does not move
with this (b8ffab0bc34fc89b before and after, linux-release).

**The alpha-rate terms on the dynamic pressure without its sideways part**
(tools/alpha_rate.py, JSBSim issue #89): braked to a stop in a crosswind, her
angle of attack is undefined, its rate swings, and multiplied by the whole
dynamic pressure it pitched her by tens of degrees a second.

**Her pitch terms keyed to the angle of attack on the dynamic pressure without
its sideways part** (alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15):
Cmalpha. Standing or slowing in a crosswind her angle of attack, atan2(w, u),
is any angle at all - 11 to 113 degrees standing on her brakes in 15 kt across
- and on the whole dynamic pressure, which counts the crosswind, it pitched her
nose down by up to 2,420 ft-lb, and in 15 kt from the right she pitched at up
to 22 deg/s; on aero/qbarUW-psf, the air's moment is at most 260 ft-lb nose
down, and she pitches at under 1 deg/s. Her wing and tailplane are not swept:
the case JSBSim issue #89 tested (on the C172) and the FlightGear wiki's
"JSBSim Aerodynamics" writes up, QBarUW "more appropriate for Cm" than the
whole QBar. In flight, at a few degrees of sideslip, the two differ by its
cosine squared. The selftest flies her, and its hash moves with this.

**Her lift keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15): CLwbh, her wing, body and
tail's lift against the angle of attack. Standing on her brakes in 15 kt
straight across, her angle of attack, atan2(w, u), was -49 to 131 degrees, and
on the whole dynamic pressure, which counts the crosswind, the term lifted her
by up to 240 lb either way; on aero/qbarUW-psf, nothing. Her pitch moved 1.7
degrees and at up to 1.8 deg/s over those fourteen seconds standing; now 0.1
degrees and 0.3 deg/s. JSBSim issue #89's guidance, written into the FlightGear
wiki's "JSBSim Aerodynamics": QBarUW "is more appropriate for CL than the
generic QBar" - a wing's lift is made by the air in the plane of u and w. Her
wing is not swept: the case issue #89 tested. In flight, at a few degrees of
sideslip, the two differ by its cosine squared. Her lift terms not keyed to the
angle of attack (CLDf, CLDe and CLq) are not this change's, and stay on
aero/qbar-psf. The selftest flies her, and its hash moves with this. The
learnt landing flies her slipped - in calm air 8 to 10 degrees on the approach
and up to 17.7 in the flare, where qbarUW is 0.91 of qbar - so it lands
shorter on her now, and harder in a crosswind - in calm air it sinks 4 to 10
ft/min less (docs/PROJECT_STATUS.md, 2026-10-15): her lift there is the
crossflow's, the policy is not trained again, and its parity fixture is flown
again (tools/rl/export.py --parity-only).

**Her wing's drag keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-16): CDwbh, her wing, body and
tail's drag against the angle of attack and her flaps. Standing on her brakes
in 15 kt straight across, her angle of attack, atan2(w, u), was -51 to 134
degrees, and on the whole dynamic pressure, which counts the crosswind, it drew
her along the beam wind by up to 11 lb; on aero/qbarUW-psf, nothing. The same
reasoning as her lift's, above: a wing's drag at its angle of attack is made by
the air in the plane of u and w, and the sideways part runs along its span -
the drag that rolled the J-3 Cub onto her wing standing in a beam wind
(make_j3cub.py). In flight, at a few degrees of sideslip, the two differ by its
cosine squared. The selftest flies her, and its hash moves with this; the
learnt landing's parity fixture is flown again (tools/rl/export.py
--parity-only).

**Her lift terms not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-23, with_alpha_terms_on_qbar_uw for a lift with pitch rate whose
coefficient the angle picks): CLDf, CLDe and CLq. On the whole dynamic pressure
they counted the crosswind standing. Standing on her brakes with her engines
stopped in 15 kt straight across, a lifting control set one at a time, they
lifted her by up to 61 lb (flaps down); now under 0.1 lb at every setting (the
standing test in tests/unit/test_departure.cpp). JSBSim issue #89's guidance
puts the whole of CL on QBarUW; alpha_rate.py says why, for swept surfaces too.
In flight, at a few degrees of sideslip, the two differ by its cosine squared.
"""

import pathlib
import re
import sys

import alpha_rate
import ground
from piston_mixture import with_best_power_mixture, with_float_carburettor

ROOT = pathlib.Path(__file__).resolve().parent.parent
PINNED = ROOT / "ext" / "jsbsim"
OUT = ROOT / "assets" / "jsbsim"
SCRIPT = "make_c172p"
MODEL = "c172p"
MAXIMUM_WEIGHT_LBS = 2400  # the 172P's maximum weight, its handbook's

CP_FACTOR = 0.88
CT_FACTOR = 0.96
LOW_J_THRUST = 1.35
CDO = "0.031"
DRAG_DUE_TO_ALPHA_SCALE = 0.72
LIFT_UNSTALLED = {
    -0.09: -0.22, 0.00: 0.25, 0.09: 0.73, 0.10: 0.78, 0.12: 0.886, 0.14: 0.99,
    0.16: 1.10, 0.17: 1.15, 0.19: 1.26, 0.21: 1.37, 0.24: 1.52, 0.26: 1.61,
    0.28: 1.68, 0.30: 1.66, 0.32: 1.58, 0.34: 1.45, 0.36: 1.28,
}
FLAP_LIFT = ("0.2500", "0.3800", "0.4200")
# Her handbook's Figure 1-1 (Three View): the propeller's diameter, and the
# horizontal tail's span, 11 ft 4 in.
PROPELLER_DIAMETER_IN = 75.0
TAIL_SPAN_IN = 136.0
# Momentum theory: the far slipstream has half the disc's area.
SLIPSTREAM_DIAMETER_IN = PROPELLER_DIAMETER_IN / 2.0 ** 0.5
TAIL_IN_SLIPSTREAM = round(SLIPSTREAM_DIAMETER_IN / TAIL_SPAN_IN, 3)
# Her handbook's Figure 1-1, its side view scaled by its own 8 ft 9 1/2 in
# height: inches above the ground of her thrust line (the spinner's centre), her
# fin's root on the tailcone, and its tip below the beacon.
THRUST_LINE_IN = 52.9
FIN_ROOT_IN = 50.7
FIN_TIP_IN = 103.4
# The slipstream's upper half, above the thrust line, over the fin's span.
FIN_IN_SLIPSTREAM = round(
    (THRUST_LINE_IN + SLIPSTREAM_DIAMETER_IN / 2.0 - FIN_ROOT_IN) / (FIN_TIP_IN - FIN_ROOT_IN), 3)


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
    # Her elevator in the part of the slipstream that reaches it (docstring).
    text = replace_once(
        text,
        r'(<function name="aero/function/qbar-induced-psf">.*?</function>\n)',
        r"\1\n"
        r' <function name="aero/function/qbar-tail-psf">' "\n"
        r"  <description> q bar at the tailplane: the free stream's, and the part of the"
        "\n"
        r"   slipstream's excess that reaches it (glideslope: tools/make_c172p.py) </description>"
        "\n"
        r"  <sum>" "\n"
        r"   <property>aero/qbar-psf</property>" "\n"
        r"   <product>" "\n"
        f"    <value>{TAIL_IN_SLIPSTREAM:.3f}</value>\n"
        r"    <difference>" "\n"
        r"     <property>aero/function/qbar-induced-psf</property>" "\n"
        r"     <property>aero/qbar-psf</property>" "\n"
        r"    </difference>" "\n"
        r"   </product>" "\n"
        r"  </sum>" "\n"
        r" </function>" "\n",
        "the induced dynamic pressure's function")
    text = replace_once(
        text,
        r"(<description>Pitch_moment_due_to_elevator_deflection</description>\s*<product>\s*"
        r"<property>)aero/function/qbar-induced-psf(</property>)",
        r"\1aero/function/qbar-tail-psf\2", "the elevator's pitch term")
    # Her rudder in the part of the slipstream that reaches her fin (docstring).
    text = replace_once(
        text,
        r'(<function name="aero/function/qbar-tail-psf">.*?</function>\n)',
        r"\1\n"
        r' <function name="aero/function/qbar-fin-psf">' "\n"
        r"  <description> q bar at the fin: the free stream's, and the part of the"
        "\n"
        r"   slipstream's excess that reaches it (glideslope: tools/make_c172p.py) </description>"
        "\n"
        r"  <sum>" "\n"
        r"   <property>aero/qbar-psf</property>" "\n"
        r"   <product>" "\n"
        f"    <value>{FIN_IN_SLIPSTREAM:.3f}</value>\n"
        r"    <difference>" "\n"
        r"     <property>aero/function/qbar-induced-psf</property>" "\n"
        r"     <property>aero/qbar-psf</property>" "\n"
        r"    </difference>" "\n"
        r"   </product>" "\n"
        r"  </sum>" "\n"
        r" </function>" "\n",
        "the tailplane's dynamic pressure's function")
    text = replace_once(
        text,
        r"(<description>Yaw_moment_due_to_rudder</description>\s*<product>\s*"
        r"<property>)aero/function/qbar-induced-psf(</property>)",
        r"\1aero/function/qbar-fin-psf\2", "the rudder's yaw term")
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n        <!-- glideslope: this is JSBSim's c172p with the changes listed in\n"
        r"             tools/make_c172p.py, which made it. Do not edit it by hand. -->",
        "the file header")
    # The tail strike, measured from her visual model by tools/ground.py,
    # in place of the source model's tail skid, 8 in below her drawn tail,
    # which struck 1.4 degrees before it would.
    return ground.with_tail_strike(
        with_float_carburettor(text, "make_c172p"), MODEL, MAXIMUM_WEIGHT_LBS,
        replacing=("TAIL_SKID",), script=SCRIPT)


def engine():
    text = (PINNED / "engine" / "eng_io320.xml").read_text()
    text = replace_once(text, r"\n\s*<bsfc>\s*0\.32\s*</bsfc>", "", "the engine's bsfc")
    return with_best_power_mixture(text, "make_c172p")


def outputs():
    return {
        OUT / "aircraft" / "c172p" / "c172p.xml": alpha_rate.with_unkeyed_terms_on_qbar_uw(
            alpha_rate.with_alpha_terms_on_qbar_uw(
            alpha_rate.with_qbar_uw(airframe(), "make_c172p"), "make_c172p",
            {"PITCH": ["aero/coefficient/Cmalpha"],
             "LIFT": ["aero/coefficient/CLwbh"],
             "DRAG": ["aero/coefficient/CDwbh"]}), "make_c172p",
            {"LIFT": ["aero/coefficient/CLq",
             "aero/coefficient/CLDf",
             "aero/coefficient/CLDe"]}),
        OUT / "engine" / "prop_75in2f.xml": propeller(),
        OUT / "engine" / "eng_io320.xml": engine(),
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
