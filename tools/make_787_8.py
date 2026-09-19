#!/usr/bin/env python3
"""make_787_8.py - glideslope's Boeing 787-8, made from JSBSim's 787-8.

JSBSim's 787-8, as pinned in ext/jsbsim, is the 787-8 with Rolls-Royce Trent
1000 engines of 66,500 lb - between the Trent 1000-H's 63,897 and the -A's
69,194 (FAA type certificate data sheet T00021SE), the thrust of the
"typical" rating Boeing's charts give the -8. Its figures here are from
Boeing's 787 Airplane Characteristics for Airport Planning (D6-58333,
revision O, 2023) and T00021SE, in assets/figures/787-8.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_787_8.py           write the files
    python3 tools/make_787_8.py --check   exit 1 if what is committed differs

The model's own weights are kept: its empty weight, 239,200 lb (the planning
document gives no operating empty weight), its seats and holds, where the
figures put the payload in proportion to the model's own, and its tanks,
which hold what the planning document says, 101,343 kg, within 0.3%.

The changes, and what each is for:

  Airframe (aircraft/787-8/787-8.xml)
    Lift_due_to_alpha   The model's lift rose at 5.75 a radian only to 2
                        degrees, then at 3.25, to 1.34 at its stall, and its
                        flaps took nothing of the slats the 787's leading edge
                        runs out with them; with 5 degrees of flap at 502,500 lb
                        it stalled at 181 knots, its V2 was 228 and it could
                        not climb at 2.4% on one engine. Its lift now rises at
                        LIFT_SLOPE, a wing of its aspect ratio's, to the clean
                        stall at CLEAN_STALL_ALPHA, and with the leading edge
                        out, from 1 degree of flap, on to
                        LEADING_EDGE_STALL_ALPHA, what the take-off figures
                        need.
    Span 192 -> 197.25 ft, Induced drag 0.04 -> INDUCED_DRAG
                        The span the planning document gives, 197 ft 3 in, and
                        the induced drag of a span efficiency of 0.8 at the
                        aspect ratio that makes, 11.1; the model's 0.04 was 0.76
                        on its own span, and its drag at zero lift, rising with
                        the angle of attack from 0.015 to 0.026 at 15 degrees,
                        counted some of it again: that is now held at 0.015 to
                        the stall and rises past it. With the model's, the
                        climb with an engine out was 1.7% at best.
    Drag due to flaps   The model's rose in proportion to the flaps'
                        deflection; it is now the slats' LEADING_EDGE_DRAG from
                        1 degree and the flaps' growing as their deflection to
                        the power 1.5, to the model's own 0.0875 at 35 degrees.
    The rudder's command clipped at 1, not 0.25
                        The pilot's rudder was clipped at a quarter of its
                        command before it was scaled to the rudder's 14
                        degrees, so the pedals moved it 3.6 degrees and only
                        the yaw damper could move it further: with an engine
                        out at V2 the rudder was at its stop and the aircraft
                        flew 5.7 degrees sideways, its drag a seventh more.
    Main wheels' braking friction 0.4 -> 0.5
                        The dry runway's the other airliners have.
    Drag due to mach    Lock's fourth-power rise from DRAG_DIVERGENCE, as the
                        737-300's (see tools/airliner.py), where the model's
                        rose from 0.001 at Mach 0.79 at a tenth of a swept
                        wing's slope.
    Drag_of_windmilling_engines, added
                        As the 737-300's: a stopped Trent 1000's 112 in fan
                        windmills, with a drag area of about 0.4 of its
                        frontal area.

  Engine (engine/Trent1000.xml, from JSBSim's trent_1000.xml)
    Thrust with height and speed
                        Mattingly's lapse, as the 737-300's (see
                        tools/airliner.py), where JSBSim's generic turbofan
                        table held its thrust far too well with speed and
                        height.
    bleed 0.04 -> 0      The 787 takes no bleed air from its engines: its cabin
                        is pressurised by electric compressors.
"""

import sys

import airliner
from airliner import OUT, PINNED

SCRIPT = "make_787_8"
MODEL = "787-8"
ENGINE = "Trent1000"
DRAG_DIVERGENCE = 0.86
BRAKING_FRICTION = "0.50"
FAN_DIAMETER_IN = 112.0
WINDMILL_DRAG = 0.4
# The lift: the model's 0.34 at no incidence, rising at LIFT_SLOPE - a wing of
# its aspect ratio, 10.5 - to the clean wing's stall at CLEAN_STALL_ALPHA, and
# with the leading edge devices out, from 1 degree of flap, on to
# LEADING_EDGE_STALL_ALPHA.
ZERO_ALPHA_LIFT = 0.34
LIFT_SLOPE = 5.5
CLEAN_STALL_ALPHA = 0.20
LEADING_EDGE_STALL_ALPHA = 0.30
# The span the planning document gives, 197 ft 3 in, where the model had 192;
# and the induced drag of a span efficiency of 0.8 at the aspect ratio that
# makes, 11.1, where the model's 0.04 was 0.76 on its own span and the drag at
# zero lift, rising with the angle of attack, counted some of it again.
WINGSPAN_FT = 197.25
WING_AREA_SQFT = 3501.7984
INDUCED_DRAG = 1.0 / (3.14159265 * WINGSPAN_FT ** 2 / WING_AREA_SQFT * 0.8)
# The drag at zero lift by angle of attack: the model's 0.015, held to the
# stall - the span efficiency carries the lift's drag - and rising past it.
ZERO_LIFT_DRAG = [(-1.57, 1.400), (-0.26, 0.020), (0.0, 0.015), (0.20, 0.016), (0.30, 0.030), (1.57, 1.600)]
# The flaps' drag: the slats' from 1 degree, and the flaps' growing as their
# deflection to the power 1.5, to the model's own 0.0875 at 35 degrees.
FLAP_DEGREES = [0, 1, 5, 10, 15, 20, 25, 30, 35]
LEADING_EDGE_DRAG = 0.005
FULL_FLAP_DRAG = 0.0875


def replace_once(text, pattern, replacement, what):
    return airliner.replace_once(text, pattern, replacement, what, SCRIPT)


def lift_rows():
    def curve(stall):
        peak = ZERO_ALPHA_LIFT + LIFT_SLOPE * stall
        return [(-0.20, ZERO_ALPHA_LIFT - LIFT_SLOPE * 0.20), (0.0, ZERO_ALPHA_LIFT), (stall, peak),
                (stall + 0.15, peak - 0.35), (0.60, 0.80)]

    def at(points, alpha):
        for (a0, c0), (a1, c1) in zip(points, points[1:]):
            if a0 <= alpha <= a1:
                return c0 + (c1 - c0) * (alpha - a0) / (a1 - a0)
        return points[-1][1]

    clean, slats = curve(CLEAN_STALL_ALPHA), curve(LEADING_EDGE_STALL_ALPHA)
    alphas = sorted({a for a, _ in clean} | {a for a, _ in slats})
    rows = ["                                        0.0\t1.0"]
    rows += [f"                              {a:.3f}\t{at(clean, a):.4f}\t{at(slats, a):.4f}" for a in alphas]
    return "\n".join(rows)


def engine():
    text = (PINNED / "engine" / "trent_1000.xml").read_text()
    text = replace_once(text, r"(<turbine_engine name=\"trent_1000\">)",
                        r"\1\n  <!-- glideslope: JSBSim's trent_1000 with the changes listed in\n"
                        r"       tools/make_787_8.py, which made it. Do not edit it by hand. -->",
                        "the engine's name")
    text = replace_once(text, r"<bleed>\s*0\.04\s*</bleed>", "<bleed>           0.00 </bleed>", "the bleed")
    return airliner.with_mattingly_thrust(text, SCRIPT)


def airframe():
    text = (PINNED / "aircraft" / "787-8" / "787-8.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n  <!-- glideslope: this is JSBSim's 787-8 with the changes listed in\n"
        r"       tools/make_787_8.py, which made it. Do not edit it by hand. -->",
        "the file header")
    airliner.without_sockets(text, SCRIPT)
    text, n = __import__("re").subn(r"<engine file=\"trent_1000\">", f'<engine file="{ENGINE}">', text)
    if n != 2:
        raise SystemExit(f"{SCRIPT}: found {n} engines, not 2 - has the pinned model changed?")
    text = replace_once(
        text,
        r"(<description>Lift_due_to_alpha</description>.*?<property>aero/function/kCLge</property>\s*)<table>.*?</table>",
        lambda mm: mm.group(1) + "<table>\n"
        "                          <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
        "                          <independentVar lookup=\"column\">fcs/flap-pos-deg</independentVar>\n"
        "                          <tableData>\n" + lift_rows() + "\n"
        "                          </tableData>\n"
        "                      </table>", "the lift curve")
    text = replace_once(text, r"<wingspan  unit=\"FT\" >  192 </wingspan> <!-- Wikipedia -->",
                        f"<wingspan  unit=\"FT\" >  {WINGSPAN_FT} </wingspan>", "the span")
    text = replace_once(
        text,
        r"(<description>Drag at zero lift</description>.*?<tableData>\n).*?(\n\s*</tableData>)",
        lambda mm: mm.group(1) + "\n".join(f"             {a:.2f}\t{d:.3f}" for a, d in ZERO_LIFT_DRAG)
        + mm.group(2), "the drag at zero lift")
    text = replace_once(
        text, r"(<description>Induced drag</description>.*?<value>)\s*0\.04\s*(</value>)",
        rf"\g<1>{INDUCED_DRAG:.5f}\2", "the induced drag")
    rows = "\n".join(
        f"                {deg}\t{0.0 if deg == 0 else LEADING_EDGE_DRAG + (FULL_FLAP_DRAG - LEADING_EDGE_DRAG) * (deg / 35.0) ** 1.5:.4f}"
        for deg in FLAP_DEGREES)
    text = replace_once(
        text,
        r"(<description>Drag due to flaps</description>\s*<product>\s*<property>aero/qbar-psf</property>\s*"
        r"<property>metrics/Sw-sqft</property>\s*)<property>fcs/flap-pos-deg</property>\s*<value> 0\.0025 </value>",
        lambda mm: mm.group(1) + "<table>\n"
        "              <independentVar>fcs/flap-pos-deg</independentVar>\n"
        "              <tableData>\n" + rows + "\n"
        "              </tableData>\n"
        "           </table>", "the flap drag")
    for side in ("LEFT", "RIGHT"):
        text = replace_once(
            text, r"(name=\"" + side + r"_MAIN\">.*?<static_friction>)\s*0\.40\s*(</static_friction>)",
            rf"\g<1> {BRAKING_FRICTION} \2", f"the {side.lower()} main gear's friction")
    text = replace_once(
        text, r"(<summer name=\"Rudder Command Sum\">.*?<clipto>\s*<min>)\s*-0\.25\s*(</min>\s*<max>)\s*0\.25\s*(</max>)",
        r"\g<1> -1 \g<2> 1 \3", "the rudder command's clip")
    text = replace_once(
        text,
        r"(<description>Drag due to mach</description>.*?<tableData>\n).*?(\n\s*</tableData>)",
        lambda mm: mm.group(1) + airliner.mach_drag_rows(DRAG_DIVERGENCE, "                ") + mm.group(2),
        "the Mach drag")
    text = replace_once(text, r"(\n    <function name=\"aero/force/Drag_flap\">)",
                        lambda mm: "\n" + airliner.windmill_function(2, FAN_DIAMETER_IN, WINDMILL_DRAG, "    ")
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
