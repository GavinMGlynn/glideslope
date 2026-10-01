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
    The centre of gravity over its main wheels
                        The model's centre of gravity was 50 in ahead of its
                        main wheels and 90 in above the ground: seen from the
                        ground, the wheels were 29 degrees behind it, where
                        Raymer (Aircraft Design: A Conceptual Approach,
                        "tipback angle") has them 15. So much of its weight sat
                        on the nose wheel that the stabilator could not lift it
                        until 149 knots with the stick fully back, where the
                        F-15's flight manual (T.O. 1F-15A-1, figure A3-6,
                        maximum performance take-off, military thrust) has it
                        off at 91 at the same weight, 36,946 lb - and the take-
                        off autopilot's half stick not until 220. The centre of
                        gravity, with the fuel, the stores and the aerodynamic
                        reference point that the pitching moments are taken
                        about, is moved aft together until the wheels are 15
                        degrees behind it (TIP_BACK_DEG), 25 in; the wheels
                        stay where the visual model draws them. Nothing
                        changes in the air - the centre of gravity keeps its
                        place against the aerodynamics - and on the ground the
                        nose wheel came off at 98 knots at 36,946 lb, 103 at
                        41,286 and 107 at 45,713, against the manual's 91, 100
                        and 111; with NASA's pitching moment (below), 92, 96
                        and 100.
    The pitching moment and the stabilator's lift, NASA's
                        NASA TM-4604 (Corda, Stephenson, Burcham and Curry,
                        Dynamic Ground Effects Flight Test of an F-15
                        Aircraft, 1994, page 8) gives the derivatives of the
                        NASA Dryden F-15 simulator's database at the F-15's
                        approach, 8 degrees of alpha: the pitching moment's
                        slope with alpha, -0.0021 a degree, and the
                        stabilator's lift, 0.005 a degree. The model's were
                        -0.0098 about its centre of gravity - near five times
                        as stable - and 0.010: every degree of alpha took
                        nose-up stabilator, whose download took lift, so on the
                        manual's 160-knot approach (T.O. 1F-15A-1, figure
                        A8-1) it flew at 12 to 18 degrees with the stabilator
                        near its stop, where TM-4604's figure 5 has the
                        aeroplane at about 10, and it ran out of stabilator,
                        and "stalled", at 18 degrees and 151 knots, where the
                        manual's settles at 45 units at full aft stick. The
                        pitching moment's table is scaled, its shape kept,
                        until its slope at 8 degrees about the centre of
                        gravity - the lift acting at the aerodynamic reference
                        point 2.24 in behind it counted - is NASA's
                        (with_nasa_pitch), by 0.146; the stabilator's lift is
                        NASA's. Its pitching moment with the stabilator,
                        -0.008 a degree, is the model's: TM-4604 prints
                        -0.00072, which with its own -0.0021 would trim no
                        more than nine degrees of alpha with all the
                        stabilator's 26 degrees, where the manual has the nose
                        at 45 units with full aft stick - a tenth of the
                        model's, and taken here as a misprint of it. Now the
                        approach is flown at 10.8 degrees with a tenth of the
                        nose-up travel; the nose wheel comes off at 92, 96 and
                        100 knots at the three loadings, against figure A3-6's
                        maximum performance 91.5, 100.1 and 110.7 at military
                        thrust.
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
    Airframe contacts either side of the keel
                        Still it rested on a line - its radome and its belly,
                        both on the centreline, with its wing tips 47 in above
                        the belly - and slid rolling ten degrees from tip to
                        tip. Once its centre of gravity moved aft (above),
                        that rocking grew on every Windows build until it
                        rolled over and went 35 ft through the runway. Four
                        contacts on the underside, fore and aft on each side
                        of the keel, 70 in out - under the intakes and the
                        engines - are measured from the visual model by
                        tools/ground.py (ground.flanks), 49 in above the
                        wheels' contact, and it rests on them.

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

import math
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
# NASA TM-4604's derivatives for the F-15 on its approach, from the NASA
# Dryden F-15 simulator's database: at 8 degrees of alpha, the pitching
# moment's slope with alpha and the stabilator's lift, each a degree; and
# the Mach the approach is flown at, whose lift curve the slope is taken on.
NASA_ALPHA_DEG = 8.0
NASA_CM_ALPHA_PER_DEG = -0.0021
NASA_CL_DELTA_PER_DEG = 0.005
APPROACH_MACH = 0.2
# The aerodynamic reference point, 2.24 in aft of the centre of gravity in
# the pinned model, and kept there (over_its_wheels); the mean chord, in.
AERORP_AFT_OF_CG_IN = -234.15 - -236.39
CHORD_IN = 15.95 * 12.0
# The main wheels this far behind the centre of gravity, seen from the ground
# (Raymer's tipback angle); the model's own centre of gravity, aerodynamic
# reference point and main wheels, inches, which it is worked from.
TIP_BACK_DEG = 15.0
PINNED_CG = (-236.39, 4.5)
PINNED_AERORP_X = -234.15
MAIN_WHEELS = (-187.0, -85.4)


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
    text = with_nasa_pitch(with_mach_lift(over_its_wheels(text)))
    return with_flanks(scraping_airframe(with_mach_drag(text)))


def with_flanks(text):
    """Four airframe contacts either side of the keel, measured from the
    visual model by tools/ground.py (ground.flanks)."""
    return replace_once(text, r"(\n)(    </ground_reactions>)",
                        lambda m: m.group(1) + ground.contact_elements(
                            ground.flanks(MODEL), MAXIMUM_WEIGHT_LBS) + m.group(2),
                        "the end of the ground reactions")


def over_its_wheels(text):
    """The centre of gravity moved aft, with everything placed at it and the
    aerodynamic reference point, until the main wheels are TIP_BACK_DEG
    behind it."""
    for side in ("LEFT", "RIGHT"):
        m = re.search(rf'name="MLG_{side}">\s*<location unit="IN">\s*<x>\s*([-0-9.]+)\s*</x>'
                      r'\s*<y>[^<]*</y>\s*<z>\s*([-0-9.]+)\s*</z>', text)
        if not m or (float(m.group(1)), float(m.group(2))) != MAIN_WHEELS:
            raise SystemExit(f"{SCRIPT}: the main wheels have moved - has the pinned model changed?")
    arm = (PINNED_CG[1] - MAIN_WHEELS[1]) * math.tan(math.radians(TIP_BACK_DEG))
    aft = MAIN_WHEELS[0] - arm - PINNED_CG[0]
    # The centre of gravity, the stores and the two tanks are all at it.
    text, n = re.subn(rf"<x>\s*{PINNED_CG[0]}\s*</x>", f"<x> {PINNED_CG[0] + aft:.2f} </x>", text)
    if n != 4:
        raise SystemExit(f"{SCRIPT}: found {n} things at the centre of gravity, not 4"
                         " - has the pinned model changed?")
    return replace_once(
        text, rf'(<location name="AERORP" unit="IN">\s*<x>)\s*{PINNED_AERORP_X}\s*(</x>)',
        rf"\g<1> {PINNED_AERORP_X + aft:.2f} \2", "the aerodynamic reference point")


def table_rows(text, description):
    """The rows of the table under the coefficient with this description."""
    m = re.search(rf"(<description>{description}</description>.*?<tableData>\s*\n)(.*?)(\n\s*</tableData>)",
                  text, re.S)
    if not m:
        raise SystemExit(f"{SCRIPT}: no {description} table - has the pinned model changed?")
    return m


def between(rows, x):
    """Linear interpolation in rows of (x, y)."""
    for (x0, y0), (x1, y1) in zip(rows, rows[1:]):
        if x0 <= x <= x1:
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    raise SystemExit(f"{SCRIPT}: {x} is outside the table")


def pitch_slope(cm_rows, lift_rows, alpha):
    """d(Cm)/d(alpha), per radian, about the centre of gravity: the
    pitching moment's table, which JSBSim multiplies by alpha, and the lift
    acting at the aerodynamic reference point, AERORP_AFT_OF_CG behind it."""
    h = 1e-4

    def cm(a):
        return a * between(cm_rows, a)

    table = (cm(alpha + h) - cm(alpha - h)) / (2 * h)
    lift = (between(lift_rows, alpha + h) - between(lift_rows, alpha - h)) / (2 * h)
    return table - lift * AERORP_AFT_OF_CG_IN / CHORD_IN


def with_nasa_pitch(text):
    """The pitching moment with alpha scaled, and the stabilator's lift set,
    to NASA TM-4604's derivatives at its approach (NASA_ALPHA_DEG)."""
    lift = table_rows(text, "Lift_due_to_alpha")
    head, *rows = lift.group(2).split("\n")
    column = head.split().index(f"{APPROACH_MACH:.2f}") + 1
    lift_rows = [(float(r.split()[0]), float(r.split()[column])) for r in rows]

    m = table_rows(text, "Pitch_moment_due_to_alpha")
    cm_rows = [tuple(float(v) for v in r.split()) for r in m.group(2).split("\n")]
    alpha = math.radians(NASA_ALPHA_DEG)
    target = math.degrees(NASA_CM_ALPHA_PER_DEG)
    lift_part = -(between(lift_rows, alpha + 1e-4) - between(lift_rows, alpha - 1e-4)) / 2e-4 \
        * AERORP_AFT_OF_CG_IN / CHORD_IN
    table_part = pitch_slope(cm_rows, lift_rows, alpha) - lift_part
    scale = (target - lift_part) / table_part
    if not 0.0 < scale < 1.0:
        raise SystemExit(f"{SCRIPT}: the pitching moment would be scaled by {scale:.3f}")
    scaled = [(a, t * scale) for a, t in cm_rows]
    if abs(pitch_slope(scaled, lift_rows, alpha) - target) > 1e-6:
        raise SystemExit(f"{SCRIPT}: the scaled pitching moment misses NASA's slope")
    indent = re.match(r"\s*", m.group(2)).group(0)
    body = "\n".join(f"{indent}{a:.4f}\t{t:.4f}" for a, t in scaled)
    text = text[:m.start(2)] + body + text[m.end(2):]
    return replace_once(
        text,
        r"(<description>Lift_due_to_Elevator_Deflection</description>.*?<value>)\s*0\.5730\s*(</value>)",
        rf"\g<1>{math.degrees(NASA_CL_DELTA_PER_DEG):.4f}\2", "the stabilator's lift")


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
