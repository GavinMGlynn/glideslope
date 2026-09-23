#!/usr/bin/env python3
"""make_f15c.py - glideslope's McDonnell Douglas F-15C Eagle, from JSBSim's f15.

JSBSim's f15, as pinned in ext/jsbsim, is an F-15C airframe - its span,
42.83 ft, and mean chord, 15.95 ft, are the F-15C's, and it seats one - with
the F100-PW-229 engines of the later F-15E. Its figures here are the F-15C's,
from the USAF's Standard Aircraft Characteristics for the F-15C (AFG 2,
volume 1, addendum 61, February 1992), for its F100-PW-220 engines, in
assets/figures/f15c.xml; no primary source gives figures for an F-15 with
the -229.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_f15c.py           write the files
    python3 tools/make_f15c.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Airframe (aircraft/f15c/f15c.xml)
    Weights and loading The Standard Aircraft Characteristics' empty weight of
                        an F-15C, 28,476 lb, where the model had 28,000; its
                        internal fuel, 13,455 lb of JP-4, where the model's
                        two tanks held 13,123; and stores - missiles, the gun's
                        ammunition and what else the F-15C's operating weight
                        carries - at the centre of gravity, which the model
                        had nowhere to put.
    Lift with Mach      The model's lift had two Mach columns, 0.5 and 1.4,
                        and fell between them to a quarter; past Mach 1.4 it
                        stayed there. Its lift curve at Mach 0.5 is now
                        carried across the Mach range in proportion to the
                        lift slope (fighter.lift_slope): DATCOM's subsonic,
                        linear theory's supersonic.
    Drag with Mach      The model's drag due to alpha and induced drag, tables
                        of alpha with two Mach columns, halved the drag at zero
                        lift past Mach 1 and made the F-15 far too fast. They
                        are now the drag at zero lift by Mach, the drag due to
                        lift, K CL^2 with K by Mach, and past SEPARATION_ALPHA,
                        where the lift curve bends, the drag of a wing whose
                        flow has left it (fighter.drag_functions). Their
                        numbers are fitted, with the engines', to the
                        Standard Aircraft Characteristics' page 6 chart of
                        specific excess power - thirty-three points across
                        30,000 to 59,000 ft and Mach 0.9 to 2.4, which the
                        model meets to 27 ft/s, near what the chart can be read
                        to - and to the figures in assets/figures/f15c.xml.
    Airframe contacts scrape instead of rolling
                        The model's six contacts that never retract - its
                        wing tips, its fin tips, its radome and its belly -
                        are airframe, not wheels, but carried a rolling
                        friction of 0.2 and a spring of 10,000 lb/ft, which
                        is four feet of give under an aeroplane of 45,713 lb.
                        Landed with its wheels up it slid 2,734 m where the
                        0.4 this project states for an airframe scraping a
                        runway gives 1,093, and settled with its centre of
                        gravity below the runway. They now carry the
                        friction, spring and damping tools/ground.py states,
                        and are STRUCTURE contacts rather than BOGEYs: left
                        a wheel, each carried a tyre, whose sideways force
                        rocked it wheels-up between belly and wing tips
                        until it was thrown into the air and came down
                        inverted through the runway.

  Engines (engine/F100-PW-220.xml, from JSBSim's F100-PW-229.xml)
    The F-15C's engine  The Standard Aircraft Characteristics' F100-PW-220,
                        uninstalled at sea level: 23,450 lb with afterburner,
                        14,370 lb military, where the model's -229 had 29,000
                        and 17,800.
    The afterburner at full throttle
                        The -229's afterburner was worked by a throttle past 1,
                        which glideslope's throttle, 0 to 1, never reaches; it
                        now lights above 0.99, as JSBSim's F-22's does, and
                        0.99 is military power.
    Thrust with speed and height
                        fighter.thrust_lapse, fitted as the drag is: Mattingly's
                        form for an afterburning turbofan, with the thrust
                        rising in cold air - the page 6 chart's subsonic excess
                        power and its manoeuvrability chart's sustained load
                        factor together need a fifth more thrust at Mach 0.9
                        high up than Mattingly's typical engine has - and a
                        loss in the thin air above the tropopause. The military
                        table allows for the idle thrust JSBSim adds to it,
                        which at height was a quarter of the military thrust.
"""

import re
import sys

import airliner
import fighter
import ground
from airliner import OUT, PINNED

SCRIPT = "make_f15c"
MODEL = "f15c"
# Mission I's take-off weight (Standard Aircraft Characteristics page 4),
# which is also the heaviest this model can be: its two tanks hold no more
# and it carries no stores besides the missiles counted there.
MAXIMUM_WEIGHT_LBS = 45713
AIRFRAME_CONTACTS = 6  # its wing tips, fin tips, radome and belly
ENGINE = "F100-PW-220"
EMPTY_LBS = 28476
TANK_LBS = 6727.5            # 13,455 lb of JP-4 in two tanks
MILITARY_THRUST = "14370.0"
MAXIMUM_THRUST = "23450.0"
# The F100's thrust with speed and height (fighter.thrust_lapse), at
# military power and in afterburner: how its thrust rises in cold air, the
# throttle ratio past which it falls and how fast; and its loss in the thin
# air above the tropopause.
MILITARY_EXPONENT = 1.55
MILITARY_THROTTLE_RATIO = 1.036
MILITARY_FALL = 5.0
MAXIMUM_EXPONENT = 2.07
MAXIMUM_THROTTLE_RATIO = 1.447
MAXIMUM_FALL = 1.86
HIGH_LOSS = 0.072
# The wing: aspect ratio, 42.83^2 / 608, and the sweep of its quarter chord.
ASPECT = 42.83 ** 2 / 608.0
SWEEP_QUARTER_CHORD = 38.7
# Drag (fighter.drag_functions): at zero lift, subsonic; the wave drag's rise
# to Mach 1.2 and how it falls past; the span efficiency subsonic; and the
# drag due to lift supersonic, as a fraction of the no-suction wing's.
SUBSONIC_ZERO_LIFT_DRAG = 0.025
WAVE_DRAG = 0.026
WAVE_DECAY = 1.13
SPAN_EFFICIENCY = 0.487
SUPERSONIC_LIFT_DRAG = 0.63
# Where the lift curve leaves its straight line, and the flow the wing.
SEPARATION_ALPHA = 0.21


def replace_once(text, pattern, replacement, what):
    return airliner.replace_once(text, pattern, replacement, what, SCRIPT)


def engine():
    text = (PINNED / "engine" / "F100-PW-229.xml").read_text()
    text = replace_once(text, r"(<turbine_engine name=\")F100(\">)",
                        r"\g<1>" + ENGINE + r"\2\n  <!-- glideslope: JSBSim's F100-PW-229 with the changes listed in\n"
                        r"       tools/make_f15c.py, which made it. Do not edit it by hand. -->",
                        "the engine's name")
    text = replace_once(text, r"<milthrust>\s*17800\.0\s*</milthrust>",
                        f"<milthrust>   {MILITARY_THRUST} </milthrust>", "the military thrust")
    text = replace_once(text, r"<maxthrust>\s*29000\.0\s*</maxthrust>",
                        f"<maxthrust>   {MAXIMUM_THRUST} </maxthrust>", "the maximum thrust")
    text = replace_once(text, r"<augmethod>\s*2\s*</augmethod>", "<augmethod>         1 </augmethod>",
                        "the afterburner's method")
    idle = fighter.table(text, "IdleThrust")
    for name, rows in (("IdleThrust", fighter.idle_table(idle)),
                       ("MilThrust", fighter.thrust_table(MILITARY_EXPONENT, MILITARY_THROTTLE_RATIO, MILITARY_FALL, HIGH_LOSS, idle)),
                       ("AugThrust", fighter.thrust_table(MAXIMUM_EXPONENT, MAXIMUM_THROTTLE_RATIO, MAXIMUM_FALL, HIGH_LOSS))):
        text = replace_once(
            text, r'(<function name="' + name + r'">\s*<table>.*?<tableData>\s*\n).*?(\n\s*</tableData>)',
            lambda m: m.group(1) + rows + m.group(2), f"the {name} table")
    return text


def scraping_airframe(text):
    """The contacts that never retract are airframe, so they scrape.

    JSBSim tells a wheel from a wing tip by nothing but what the model says,
    and this one said its wing tips were wheels. What never retracts is
    airframe, and is given the friction and the stiffness tools/ground.py
    states for one.
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
        # **And they are airframe in kind, not only in friction.** A BOGEY is
        # a wheel: JSBSim gives it a tyre, which makes a sideways force from
        # the angle it slips at. Given a scraping friction but left a wheel,
        # the F-15C landed wheels-up rocked between its belly and its wing
        # tips, each tip kicked sideways by its tyre, until the rocking threw
        # it 180 ft into the air and it came down inverted through the
        # runway - on macOS, and on Linux for a start a tenth of a knot
        # different. A STRUCTURE contact is a point that scrapes, which is
        # what every other aeroplane's airframe is made of (tools/ground.py).
        name = re.search(r'name="([^"]+)"', body).group(1)
        where = {axis: re.search(rf"<{axis}>\s*([-0-9.]+)\s*</{axis}>", body).group(1)
                 for axis in ("x", "y", "z")}
        indent = "        "
        return (f'<contact type="STRUCTURE" name="{name}">\n'
                f'{indent}    <location unit="IN">\n'
                f'{indent}        <x> {where["x"]} </x>\n'
                f'{indent}        <y> {where["y"]} </y>\n'
                f'{indent}        <z> {where["z"]} </z>\n'
                f'{indent}    </location>\n'
                f'{indent}    <static_friction> {ground.SCRAPE_FRICTION} </static_friction>\n'
                f'{indent}    <dynamic_friction> {ground.SCRAPE_FRICTION} </dynamic_friction>\n'
                f'{indent}    <spring_coeff unit="LBS/FT"> {spring:.0f} </spring_coeff>\n'
                f'{indent}    <damping_coeff unit="LBS/FT/SEC"> {damping:.0f} </damping_coeff>\n'
                f'{indent}</contact>')

    text, n = re.subn(
        r"<contact type=\"BOGEY\"(?:(?!</contact>).)*?<retractable>0</retractable>"
        r"(?:(?!</contact>).)*?</contact>", one, text, flags=re.S)
    if n != AIRFRAME_CONTACTS:
        raise SystemExit(f"{SCRIPT}: found {n} contacts that never retract, not "
                         f"{AIRFRAME_CONTACTS} - has the pinned model changed?")
    return text


def airframe():
    text = (PINNED / "aircraft" / "f15" / "f15.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n        <!-- glideslope: this is JSBSim's f15 with the changes listed in\n"
        r"             tools/make_f15c.py, which made it. Do not edit it by hand. -->",
        "the file header")
    airliner.without_sockets(text, SCRIPT)
    text = replace_once(text, r"<emptywt unit=\"LBS\">\s*28000\s*</emptywt>",
                        f"<emptywt unit=\"LBS\"> {EMPTY_LBS} </emptywt>", "the empty weight")
    text = replace_once(
        text, r"(        </pointmass>\n)(    </mass_balance>)",
        r'\1        <pointmass name="Stores">' "\n"
        r'            <weight unit="LBS"> 0 </weight>' "\n"
        r'            <location unit="IN">' "\n"
        r'                <x> -236.39 </x>' "\n"
        r'                <y> 0 </y>' "\n"
        r'                <z> 4.5 </z>' "\n"
        r'            </location>' "\n"
        r'        </pointmass>' "\n" r"\2", "the mass balance")
    text, n = re.subn(r"(<capacity unit=\"LBS\">)\s*6561\.5\s*(</capacity>)", rf"\g<1> {TANK_LBS} \2", text)
    if n != 2:
        raise SystemExit(f"{SCRIPT}: found {n} fuel tanks, not 2 - has the pinned model changed?")
    text, n = re.subn(r"<engine file=\"F100-PW-229\">", f'<engine file="{ENGINE}">', text)
    if n != 2:
        raise SystemExit(f"{SCRIPT}: found {n} engines, not 2 - has the pinned model changed?")
    text = with_mach_lift(text)
    return scraping_airframe(with_mach_drag(text))


def with_mach_lift(text):
    """The model's lift curve at Mach 0.5, carried across the Mach range in
    proportion to the lift slope (fighter.lift_slope)."""
    m = re.search(r"(<description>Lift_due_to_alpha</description>.*?<tableData>\s*\n)(.*?)(\n\s*</tableData>)", text, re.S)
    if not m:
        raise SystemExit(f"{SCRIPT}: no lift table - has the pinned model changed?")
    head, *rows = m.group(2).split("\n")
    if head.split() != ["0.5000", "1.4000"]:
        raise SystemExit(f"{SCRIPT}: the lift table's Mach columns have changed")
    base = fighter.lift_slope(0.5, ASPECT, SWEEP_QUARTER_CHORD)
    scale = [fighter.lift_slope(mach, ASPECT, SWEEP_QUARTER_CHORD) / base for mach in fighter.MACHS]
    lines = ["                              " + "\t".join(f"{mach:.2f}" for mach in fighter.MACHS)]
    for row in rows:
        alpha, low = (float(v) for v in row.split()[:2])
        lines.append(f"                              {alpha:.4f}\t" + "\t".join(f"{low * k:.4f}" for k in scale))
    return text[:m.start(2)] + "\n".join(lines) + text[m.end(2):]


def with_mach_drag(text):
    """The drag due to alpha and the induced drag, each a table of alpha, made
    the drag at zero lift by Mach, the drag due to lift by Mach, and the drag
    past the flow separating (fighter.drag_functions)."""
    return replace_once(
        text,
        r"            <function name=\"aero/coefficient/CDalpha\">.*?<function name=\"aero/coefficient/CDi\">.*?</function>\n",
        lambda m: fighter.drag_functions(SUBSONIC_ZERO_LIFT_DRAG, WAVE_DRAG, WAVE_DECAY, ASPECT, SPAN_EFFICIENCY,
                                         SUPERSONIC_LIFT_DRAG, SEPARATION_ALPHA),
        "the drag due to alpha and induced drag")


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
