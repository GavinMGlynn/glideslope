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
    Lift at high alpha, NASA's flight data
                        NASA TN D-8052 (Summary of Flight Tests to Determine
                        the Spin and Controllability Characteristics of a
                        Remotely Piloted, Large-Scale (3/8) Fighter Airplane
                        Model, 1976, figures 12 and 13) gives the lift and
                        drag the 3/8-scale F-15 drop model flew at, from -24
                        to 40 degrees, at a Reynolds number of 4 million, its
                        inlets drooped 11 degrees and blocked. From 20 to 40
                        degrees the model's lift was 6 to 11 per cent under
                        it, and its peak at 32 degrees, where the flight data
                        go on rising to 40. Its rows from 16 to 40 degrees
                        are now the flight data (RPRV_LIFT): the 57 points
                        read off figure 12 at 300 dpi, faired by a local
                        straight line weighted over 2.5 degrees, 0.89 at 16,
                        1.035 at 20, 1.13 at 24, 1.18 at 28, 1.215 at 32
                        and 1.25 at 40. They are taken at Mach 0.2
                        (RPRV_MACH) - the report flew "at low speed" and did
                        not consider Mach - and the Mach 0.5 curve the table
                        is carried from is them over Mach 0.2's share of the
                        lift slope, so every Mach column moves by the same
                        proportion as the Mach 0.2 one. Past 40 degrees,
                        where the report has nothing, the model's own rows
                        at 45 and 50 are scaled by what the flight data
                        raised its 40 degree row by, 1.11: its fall-off is
                        the only F-15 source there, and scaled it meets the
                        flight data without a step. Below 16 degrees the
                        model is kept: the drop model's lift there is up to
                        15 per cent under it (0.48 at 8 degrees against
                        0.56), and that range is held by the full-size
                        F-15's approach, which NASA TM-4604 (below) has at
                        about 10 degrees and the model flies at 10 to 11.5;
                        the drop model's inlets were blocked, its Reynolds
                        number a tenth of the full-size aeroplane's. The
                        drag (Drag with Mach, below) was not changed: past
                        the flow separating it is the drag at zero lift and
                        the lift times the tangent of alpha, and with this
                        lift that is within 0.035 of figure 13's flight
                        data from 16 to 32 degrees (0.03 under at 16, 0.02
                        over at 24 and 28, 0.03 at 32), 0.04 over at 36 and
                        0.07 at 40 - where the report's four points from 38
                        to 40 degrees scatter over 0.09. They show the drag
                        a force normal to the wing would make, the lift
                        times the tangent of alpha, with no drag at zero
                        lift on top; the model adds 0.025.
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
                        and 111; with NASA's pitching moment (below), 96.6,
                        100.9 and 105.0.
    The pitching moment and the stabilator, NASA's
                        NASA TM-4604 (Corda, Stephenson, Burcham and Curry,
                        Dynamic Ground Effects Flight Test of an F-15
                        Aircraft, 1994, page 8) gives the derivatives of the
                        NASA Dryden F-15 simulator's database at the F-15's
                        approach, 8 degrees of alpha, each a degree: the
                        pitching moment's slope with alpha, -0.0021; the
                        stabilator's lift, 0.005; and its pitching moment,
                        printed -0.00072. That last has lost a zero: the two
                        stabilator derivatives' ratio is the tail's arm in
                        chords, and -0.00072 / 0.005 puts the stabilator 2.3
                        ft behind the centre of gravity where the model's
                        horizontal tail arm is 20 ft; -0.0072 puts it 23 ft.
                        The model's were -0.0100 about the clean loading's
                        centre of gravity across 6 to 10 degrees - five times
                        as stable - 0.010 and -0.008: every degree of alpha
                        took nose-up stabilator, whose download took lift, so
                        on the manual's 160-knot approach (T.O. 1F-15A-1,
                        figure A8-1) it flew at 12 to 18 degrees with the
                        stabilator near its stop, where TM-4604's figure 5
                        has the aeroplane at about 10, and full aft stick
                        held it at 18 degrees and 151 knots.
                        Now (with_nasa_pitch) the pitching moment's table is
                        scaled, its shape kept, until its slope across 6 to
                        10 degrees (APPROACH_ALPHA_DEG) about the clean
                        loading's centre of gravity - 3.41 in ahead of the
                        aerodynamic reference point, the pilot counted - is
                        NASA's: by 0.118. That scale is the approach's, not
                        the stall's: carried to high alpha it left full aft
                        stick nothing to balance against, and the nose went
                        on rising to 80 degrees. **Past 24 degrees**
                        (HIGH_ALPHA_RAD), where the flow has left the wing,
                        the table is scaled by HIGH_ALPHA_SCALE, 0.45 - no
                        published F-15 pitching moment at high alpha was
                        found to take instead. That is about the most of the
                        pinned model's nose-down moment that lets full aft
                        stick carry the nose past the lift's peak at 32
                        degrees, as the manual's goes on past its wing rock
                        at 30 units to 45: at 0.468 or more full aft stick
                        balances below 24 degrees, and the script refuses
                        it. Full aft stick now settles at 41.9 degrees, at
                        110.1 knots with TN D-8052's lift (above; 116.6
                        before it) - not the manual's "100 knots or less",
                        which needs about a fifth more lift and drag there
                        than the model's tables give. The
                        stabilator's lift and pitching moment are NASA's.
                        On the approach she flies at 10 to 11.5 degrees with
                        a seventh of the nose-up travel; the nose wheel comes
                        off at 96.6, 100.9 and 105.0 knots at the three
                        loadings, against figure A3-6's maximum performance
                        91.5, 100.1 and 110.7 at military thrust.
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

    The speedbrake's drag
                        The model moved its speedbrake but no coefficient read
                        it, so the lever slowed nothing. Its drag is now the
                        1988 F-15 aerodynamic database's, as McDonnell's AFIT
                        thesis gives it (Investigation of the High Angle of
                        Attack Dynamics of the F-15B Using Bifurcation
                        Analysis, AFIT/GAE/ENY/90D-16, December 1990, page
                        81): CXDSPD, a drag coefficient of 0.0436 on the
                        wing's area with the speedbrake out (SPEEDBRAKE_DRAG).
                        The same page says it will not deploy past 15 degrees
                        of alpha, and the thesis's characteristics (page 75)
                        give its 31.5 sq ft and 45 degrees; so past 15 degrees
                        (SPEEDBRAKE_MOST_ALPHA_DEG) its command is taken as
                        stowed, and it comes in.

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
import written
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
# NASA TN D-8052's lift (figure 12), flown by the 3/8-scale F-15 drop model
# at a Reynolds number of 4 million, inlets drooped 11 degrees and blocked:
# faired through its flight points, read from the figure, at the lift
# table's rows from 16 to 40 degrees (radians, as the table has them). The
# report flew them "at low speed" and did not consider Mach; they are taken
# at Mach 0.2, about where she settles at full aft stick.
RPRV_MACH = 0.2
RPRV_LIFT_FROM_RAD = 0.279
RPRV_LIFT_TO_RAD = 0.698
RPRV_LIFT = {0.279: 0.89, 0.349: 1.035, 0.419: 1.13, 0.489: 1.18, 0.559: 1.215, 0.698: 1.25}
# Where the lift curve leaves its straight line, and the flow the wing.
SEPARATION_ALPHA = 0.21
# NASA TM-4604's derivatives for the F-15 on its approach, from the NASA
# Dryden F-15 simulator's database, each a degree: the pitching moment's
# slope with alpha, fitted across APPROACH_ALPHA_DEG; the stabilator's lift;
# and its pitching moment, the report's -0.00072 with the zero it dropped
# (with_nasa_pitch). The Mach the approach is flown at, whose lift curve the
# slope is taken on, and the loading whose centre of gravity it is about.
APPROACH_ALPHA_DEG = (6.0, 10.0)
NASA_CM_ALPHA_PER_DEG = -0.0021
NASA_CL_DELTA_PER_DEG = 0.005
NASA_CM_DELTA_PER_DEG = -0.0072
APPROACH_MACH = 0.2
APPROACH_LOADING = "clean"
# Past the flow leaving the wing - the pitching moment table's rows from
# HIGH_ALPHA_RAD, 24 degrees on - its table is scaled by HIGH_ALPHA_SCALE
# instead: about the most of the pinned model's nose-down moment that still
# lets full aft stick (STABILATOR_NOSE_UP_DEG) carry the nose past the
# lift's peak, as the flight manual's goes on past its wing rock to 45
# units (with_nasa_pitch, which works out the bound, 0.468).
HIGH_ALPHA_RAD = 0.40
HIGH_ALPHA_SCALE = 0.45
STABILATOR_NOSE_UP_DEG = 26.0
CHORD_IN = 15.95 * 12.0
# The speedbrake's drag coefficient, out, on the wing's area, and the alpha
# past which it does not deploy: the 1988 F-15 aerobase's CXDSPD and its
# limit, from AFIT/GAE/ENY/90D-16 page 81.
SPEEDBRAKE_DRAG = 0.0436
SPEEDBRAKE_MOST_ALPHA_DEG = 15.0
FIGURES = OUT.parent / "figures" / f"{MODEL}.xml"
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
    return with_speedbrake(with_flanks(scraping_airframe(with_mach_drag(text))))


def with_speedbrake(text):
    """The speedbrake's drag, which the model had none of, and its command
    taken as stowed past the alpha it does not deploy at."""
    text = replace_once(
        text,
        r"(            <kinematic name=\"Speedbrake Control\">\n                <input>)fcs/speedbrake-cmd-norm(</input>)",
        lambda m: ('            <switch name="fcs/speedbrake-allowed-norm">\n'
                   '                <default value="0"/>\n'
                   '                <test value="fcs/speedbrake-cmd-norm">\n'
                   f'                    aero/alpha-deg le {SPEEDBRAKE_MOST_ALPHA_DEG:.1f}\n'
                   '                </test>\n'
                   '            </switch>\n'
                   + m.group(1) + "fcs/speedbrake-allowed-norm" + m.group(2)),
        "the speedbrake's command")
    return replace_once(
        text,
        r"(            <function name=\"aero/coefficient/CDDe\">.*?</function>\n)",
        lambda m: m.group(1) + written.coefficient(
            "CDsb", "Drag_due_to_speedbrake",
            ["aero/qbar-psf", "metrics/Sw-sqft", "fcs/speedbrake-pos-norm", SPEEDBRAKE_DRAG]),
        "the drag due to the stabilator")


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


def x_of(text, pattern, what):
    m = re.search(pattern + r'\s*<location unit="IN">\s*<x>\s*([-0-9.]+)\s*</x>', text)
    if not m:
        raise SystemExit(f"{SCRIPT}: no {what} - has the pinned model changed?")
    return float(m.group(1))


def aerorp_aft_of_cg_in(text):
    """How far the aerodynamic reference point is behind the centre of
    gravity of APPROACH_LOADING, in: the empty aeroplane's, the stores and
    the fuel all at one point (over_its_wheels), and the pilot forward."""
    figures = FIGURES.read_text()
    m = re.search(rf'<loading name="{APPROACH_LOADING}" total_lbs="([0-9.]+)">(.*?)</loading>',
                  figures, re.S)
    if not m:
        raise SystemExit(f"{SCRIPT}: {FIGURES} has no {APPROACH_LOADING} loading")
    total = float(m.group(1))
    pilot = float(re.search(r'<pointmass index="0" lbs="([0-9.]+)"/>', m.group(2)).group(1))
    cg = float(re.search(r'<location name="CG" unit="IN">\s*<x>\s*([-0-9.]+)', text).group(1))
    pilot_x = x_of(text, r'<pointmass name="Pilot">\s*<weight unit="LBS">[^<]*</weight>', "pilot")
    aerorp = float(re.search(r'<location name="AERORP" unit="IN">\s*<x>\s*([-0-9.]+)', text).group(1))
    return aerorp - (cg + pilot * (pilot_x - cg) / total)


def cm_about_cg(cm_rows, lift_rows, arm_in, alpha):
    """The pitching moment about the centre of gravity: the table, which
    JSBSim multiplies by alpha, and the lift acting at the aerodynamic
    reference point arm_in behind it."""
    return alpha * between(cm_rows, alpha) - between(lift_rows, alpha) * arm_in / CHORD_IN


def approach_slope(cm_rows, lift_rows, arm_in):
    """The pitching moment's slope across APPROACH_ALPHA_DEG, a degree."""
    low, high = APPROACH_ALPHA_DEG
    return (cm_about_cg(cm_rows, lift_rows, arm_in, math.radians(high)) -
            cm_about_cg(cm_rows, lift_rows, arm_in, math.radians(low))) / (high - low)


def with_nasa_pitch(text):
    """The pitching moment with alpha scaled to NASA TM-4604's slope at the
    approach, and past HIGH_ALPHA_RAD by HIGH_ALPHA_SCALE; the stabilator's
    lift and pitching moment NASA's."""
    lift = table_rows(text, "Lift_due_to_alpha")
    head, *rows = lift.group(2).split("\n")
    column = head.split().index(f"{APPROACH_MACH:.2f}") + 1
    lift_rows = [(float(r.split()[0]), float(r.split()[column])) for r in rows]
    arm = aerorp_aft_of_cg_in(text)

    m = table_rows(text, "Pitch_moment_due_to_alpha")
    cm_rows = [tuple(float(v) for v in r.split()) for r in m.group(2).split("\n")]
    # The lift's part of the slope does not scale with the table; solve for
    # the scale that brings the two together to NASA's.
    lift_only = approach_slope([(a, 0.0) for a, _ in cm_rows], lift_rows, arm)
    table_only = approach_slope(cm_rows, [(a, 0.0) for a, _ in lift_rows], arm)
    scale = (NASA_CM_ALPHA_PER_DEG - lift_only) / table_only
    if not 0.0 < scale < HIGH_ALPHA_SCALE:
        raise SystemExit(f"{SCRIPT}: the pitching moment would be scaled by {scale:.3f}")
    # Full aft stick's moment, and the scale past HIGH_ALPHA_RAD at which it
    # would balance at that row: any more and the stick trims below it.
    if not re.search(r"<range>\s*<min>-26</min>\s*<max>15</max>\s*</range>", text):
        raise SystemExit(f"{SCRIPT}: the stabilator's travel has changed")
    full_aft = NASA_CM_DELTA_PER_DEG * -STABILATOR_NOSE_UP_DEG
    row = next(a for a, _ in cm_rows if a >= HIGH_ALPHA_RAD)
    bound = (full_aft - between(lift_rows, row) * arm / CHORD_IN) / -(row * between(cm_rows, row))
    if not HIGH_ALPHA_SCALE < bound:
        raise SystemExit(f"{SCRIPT}: full aft stick would trim below {row} rad at a scale "
                         f"over {bound:.3f}")
    scaled = [(a, t * (HIGH_ALPHA_SCALE if a >= HIGH_ALPHA_RAD else scale)) for a, t in cm_rows]
    if abs(approach_slope(scaled, lift_rows, arm) - NASA_CM_ALPHA_PER_DEG) > 1e-9:
        raise SystemExit(f"{SCRIPT}: the scaled pitching moment misses NASA's slope")
    indent = re.match(r"\s*", m.group(2)).group(0)
    body = "\n".join(f"{indent}{a:.4f}\t{t:.4f}" for a, t in scaled)
    text = text[:m.start(2)] + body + text[m.end(2):]
    text = replace_once(
        text,
        r"(<description>Pitch_moment_due_to_elevator_deflection</description>.*?<value>)\s*-0\.4580\s*(</value>)",
        rf"\g<1>{math.degrees(NASA_CM_DELTA_PER_DEG):.4f}\2", "the stabilator's pitching moment")
    return replace_once(
        text,
        r"(<description>Lift_due_to_Elevator_Deflection</description>.*?<value>)\s*0\.5730\s*(</value>)",
        rf"\g<1>{math.degrees(NASA_CL_DELTA_PER_DEG):.4f}\2", "the stabilator's lift")


def with_mach_lift(text):
    """The model's lift curve at Mach 0.5, from 16 to 40 degrees TN D-8052's
    flight data (RPRV_LIFT, taken at RPRV_MACH) and past 40 its own scaled
    to meet them, carried across the Mach range in proportion to the lift
    slope (fighter.lift_slope)."""
    m = re.search(r"(<description>Lift_due_to_alpha</description>.*?<tableData>\s*\n)(.*?)(\n\s*</tableData>)", text, re.S)
    if not m:
        raise SystemExit(f"{SCRIPT}: no lift table - has the pinned model changed?")
    head, *rows = m.group(2).split("\n")
    if head.split() != ["0.5000", "1.4000"]:
        raise SystemExit(f"{SCRIPT}: the lift table's Mach columns have changed")
    base = fighter.lift_slope(0.5, ASPECT, SWEEP_QUARTER_CHORD)
    scale = [fighter.lift_slope(mach, ASPECT, SWEEP_QUARTER_CHORD) / base for mach in fighter.MACHS]
    # The flight data are at RPRV_MACH; the Mach 0.5 curve that gives them
    # there is theirs over that column's share of the lift slope.
    at_rprv = fighter.lift_slope(RPRV_MACH, ASPECT, SWEEP_QUARTER_CHORD) / base
    pinned = [tuple(float(v) for v in row.split()[:2]) for row in rows]
    alphas = [round(a, 4) for a, _ in pinned]
    if sorted(RPRV_LIFT) != [a for a in alphas if RPRV_LIFT_FROM_RAD <= a <= RPRV_LIFT_TO_RAD]:
        raise SystemExit(f"{SCRIPT}: the lift table's rows from {RPRV_LIFT_FROM_RAD} to "
                         f"{RPRV_LIFT_TO_RAD} rad are not TN D-8052's - has the pinned model changed?")
    # Past the flight data's end, the model's own fall-off, scaled by what
    # the flight data raised it by at their last row.
    beyond = RPRV_LIFT[RPRV_LIFT_TO_RAD] / (dict(zip(alphas, (low for _, low in pinned)))[RPRV_LIFT_TO_RAD] * at_rprv)
    lines = ["                              " + "\t".join(f"{mach:.2f}" for mach in fighter.MACHS)]
    for alpha, low in pinned:
        key = round(alpha, 4)
        if key in RPRV_LIFT:
            low = RPRV_LIFT[key] / at_rprv
        elif key > RPRV_LIFT_TO_RAD:
            low *= beyond
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
