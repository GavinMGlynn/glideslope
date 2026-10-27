#!/usr/bin/env python3
"""make_short_s23.py - glideslope's Short S.23 Empire flying boat, made from
JSBSim's Short_S23.

JSBSim's Short_S23, as pinned in ext/jsbsim, is Anders Gidenstam's model of
the Short S.23 Empire flying boat of 1936, with four Bristol Pegasus Xc
engines and de Havilland two-pitch airscrews, and its hull's and wing floats'
hydrodynamics - buoyancy, planing lift and water drag - computed from its 3d
model. Its figures are Short Brothers' and Flight's, in assets/figures/
short_s23.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_short_s23.py           write the files
    python3 tools/make_short_s23.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Files and names       The model is short_s23. Its own systems are in
                        aircraft/short_s23/Systems/, as the pinned model's
                        are; the three of JSBSim's shared systems it uses -
                        hydrodynamics, hydrodynamic-planing-floats and
                        sperry-a2-autopilot - are in systems/, and its engine
                        and airscrew in engine/, the engine unchanged and the
                        airscrew as below.

  Airframe (aircraft/short_s23/short_s23.xml)
    Keel skids, added   The model meets the ground only at its bow, tail and
                        wings, as the hull's hydrodynamics are what meet the
                        water; on land its hull sank into the ground until
                        its wings touched, the centre of gravity 7 ft under
                        it. Three points on the keel - the forebody's, level
                        from 4.2 m aft of the bow to the main step at 10.5 m,
                        and the afterbody's at 17.1 m - are contacts JSBSim
                        takes as wheels, which water does not bear, so that
                        they meet land and not water, with a hull's sliding
                        friction and no rolling. Their heights are scaled
                        from the side and front views in Flight's general
                        arrangement (29 October 1936, p. 440f): the floats'
                        keels 4.3 ft above the hull's, and the model's floats'
                        1.71 ft below its hydrodynamic reference point, put
                        the forebody keel 6.0 ft (1.83 m) below it; the
                        afterbody's keel rises 4.1 ft to 17.1 m aft. The wing
                        floats' keels, where the model's hydrodynamics has
                        them - 31.99 ft out, 1.71 ft below the reference
                        point and their steps 4.72 ft aft of it - are skids
                        too, so that on land it heels onto a float as it
                        does afloat, not onto a wing tip.
    The carburettors' float chambers, full
                        Each engine is fed from a quarter-pound tank, its
                        carburettor's float chamber, which the model started
                        empty and its fuel system fills once she is flying.
                        An engine started running was starved until then:
                        it made no power, so JSBSim's trim, which runs her
                        engines with time stopped, could find no throttle
                        that held her speed, and she could not be trimmed.
                        They start full, a pound in all, the pound taken
                        from her two centre tanks so that she weighs what
                        she did.
    Six declarations, removed
                        The co-pilot's and the Sperry autopilot's control
                        commands, declared again where the systems that
                        drive them declare them, which JSBSim warns of at
                        every load.

  Aerodynamics (aircraft/short_s23/Systems/datcom_aero.xml)
    Alpha limits, 0.4189 -> 0.2443 rad
                        The angles of attack JSBSim's trim searches, which
                        reached 24 degrees, past her wing's greatest lift at
                        14 (CLwbh's peak, 0.2443 rad): there her lift is
                        less again, the trim saw too little at both ends and
                        failed even where she flies, clean at the plan's
                        slowest (86 kt). Nothing else in the model reads
                        them.
    The drag, a zero-lift drag and her wing's drag due to lift
                        The model's DATCOM drag table, times 1.23 "adjusted
                        for climb time", flew it at 225 mph at 5,500 ft where
                        its maximum is 200; times 1.9, as it was until
                        2026-10-23, the drag due to lift went up with the
                        rest, an Oswald efficiency of 0.33, and heavy at
                        10,000 ft she could not hold 130 kt. Up to her
                        greatest lift (0.2443 rad) her drag is now 0.033
                        plus CL^2 / (pi e A): A, 114 ft squared over 1,500 sq
                        ft, 8.66, and e Raymer's estimate for a straight wing
                        (Aircraft Design: A Conceptual Approach, eq. 12.48),
                        1.78 (1 - 0.045 A^0.68) - 0.64 = 0.79, in the 0.7 to
                        0.85 of a conventional monoplane flaps up. Past it,
                        where the flow has separated, Viterna and Corrigan's
                        separated drag (NASA CP-2230, 1982), CDmax sin^2 a +
                        B2 cos a, CDmax = 1.11 + 0.018 A = 1.27, B2 joining
                        it to her polar at the stall; the greater of the
                        two. The DATCOM rows past the stall fell to 0.05 by
                        20 degrees: with them, held thirty seconds in the
                        stall she dived to 200 kt and was pulled out at 6.7
                        g (test_lesson.cpp's stall left thirty seconds).

    Her lift past her greatest, Viterna and Corrigan's
                        CLwbh's rows past 0.2443 rad fell to -0.2 by 24
                        degrees and held it: past the stall she had no lift
                        at all. They are now A1 sin 2a + A2 cos^2 a / sin a
                        (the same report), A1 = CDmax / 2 and A2 joining it
                        to her lift at the stall, 1.53, out to 90 degrees.
                        Joined at 20 degrees instead, after the table's own
                        steep break, she fell out of the stall held thirty
                        seconds and was pulled out at 4.6 g: seen red so.
                        With lift past the stall she mushes rather than
                        gathering speed, so her stall is measured where her
                        lift coefficient peaks, as 14 CFR 25.103(b) has it
                        (assets/figures/short_s23.xml). The zero-lift
                        drag is fitted to Flight's 200 mph at 5,500 ft: 204
                        mph, with the climb (below) at 876 ft/min against 950.

    The airscrews' efficiency, NACA Report 640's (engine/prop_deHavilland5000.xml)
                        The model's tables are JavaProp's for the coarse
                        pitch, the fine guessed 8 degrees finer and its
                        thrust times 1.25 and power times 1.1 "for higher
                        thrust at 20 knots": that made the fine pitch 0.857
                        efficient at its best, more than the coarse's 0.810.
                        Hartman and Biermann's three-blade Clark Y propeller
                        (NACA Report 640, 1938, figure 12) peaks at 0.81 at
                        15 degrees, 0.845 at 20 and 0.855 at 25; placed
                        among them by where its thrust falls to nothing, the
                        fine pitch (J 0.88) lies between 15 and 20 and the
                        coarse (J 1.25) between 20 and 25. The coarse
                        column's thrust is times 1.055, best 0.855; the fine
                        column's times 0.957 above J 0.65, faded in from
                        0.45, best 0.820 - her take-off, which lifts off
                        near J 0.45, keeps the thrust it was tuned to
                        (Gouge's take-off times are still met). **The fade
                        is a fit, not sourced - the owner's decision,
                        2026-10-23, to keep it so named**: no source gives
                        where the pinned take-off tweak should stop. Its
                        ends: J 0.45, her model's own lift-off advance ratio
                        (60 to 70 kt at 1,200 airscrew rpm on 12 ft 9 in),
                        below which her take-off keeps the thrust Cassidy's
                        13,280 lb at 20 kt was tuned for; and J 0.65, the
                        advance ratio of fine pitch at 100 kt at full power,
                        from which NACA's efficiency holds in full.

    Ground effect's two factors
                        Each a <product> of one table, which JSBSim warns of
                        at every load; the table alone.

  Engines (aircraft/short_s23/Systems/engines.xml)
    The controls' twelve declarations, removed
                        Each engine's throttle, airscrew and mixture command
                        declared again, which JSBSim's flight controls already
                        declare, and which it warns of at every load.
    The airscrews' pitch levers
                        The model's lever put the airscrew in coarse pitch at
                        1 and in fine at 0; glideslope's propeller control is
                        an rpm lever, 1 its highest rpm, which for a two-pitch
                        airscrew is fine pitch. With the model's sense, every
                        take-off was made in coarse pitch at 620 hp an engine
                        where the Pegasus gives 910, and never left the water.

    The boost control held still while time is
                        Each engine's variable datum boost control moves
                        the throttle valve by an integrator, which JSBSim
                        runs at its own step at every pass of a trim, time
                        stopped or not: the valve moved under the trim, which
                        cycled and failed at almost every height and speed
                        but 3,000 ft and 130 kt (udot -1.9e-3, 3.3e-2,
                        1.1e-2 round and round at 1,000 ft). The piston's
                        rate is now nought while simulation/dt is - while
                        time is stopped, as in a trim - and the model says
                        so to glideslope (glideslope/trim/settle-throttle-
                        loops), whose trim lets her fly a second at a time
                        and trims again until the valve stays put: her
                        boost where its datum puts it, trimmed
                        (sim::Aircraft::initialize).

  Flaps (aircraft/short_s23/Systems/flaps.xml)
    The flap selector, added
                        The model's flaps are moved by an electric motor, out
                        to 25 degrees in a minute and in in a minute and a
                        half, switched on and set running by switches nothing
                        in glideslope works. A selector now runs the motor out
                        or in until the flaps are where glideslope's flap
                        control puts them, and the motor's power switch starts
                        on; the motor, and its time, are the model's.

  Hydrodynamics (aircraft/short_s23/Systems/Short_S23-hydrodynamics.xml)
    The hull's lift, a planing hull's
                        The model's planing lift was a flat clip: 20,000 lb
                        wherever the hull was within 0.6 ft of its height
                        above 34 kt, nothing growing with immersion, cut to
                        none past 4 degrees of bank. It is Savitsky's
                        dynamic lift (Hydrodynamic Design of Planing Hulls,
                        Marine Technology, 1964), C_L0 = 0.012 lambda^0.5
                        tau^1.1 on 1/2 rho V^2 b^2, lambda the step's
                        immersion over b sin(tau), capped at 4, tau her trim
                        and the angle her sink adds; no deadrise correction,
                        and its hydrostatic term, 0.0055 lambda^2.5 tau^1.1
                        / C_V^2, left to the buoyancy table, which carries
                        the water displaced already.
                        It acts at Savitsky's centre of pressure, lambda b
                        (0.75 - 1 / (5.21 C_V^2 / lambda^2 + 2.39)) forward
                        of the step, not at the reference point where JSBSim
                        applies it: a pitch moment moves it there. Applied
                        at the reference point, 1.4 ft ahead of her centre
                        of gravity, the lift pitched her up on the step
                        against full nose-down elevator.
    The afterbody, planing
                        Past the trim at which her afterbody's keel meets
                        the water - 10.8 degrees, its rise from the step to
                        the stern - the afterbody planes on its own,
                        Savitsky's lift at its own trim acting at its own
                        centre of pressure, aft, nose down (NACA ARR 3F30,
                        Benson and Klein, 1943: a hull as two planing
                        surfaces in tandem, and the upper trim limit where
                        the afterbody meets the wake; NACA TN 923, Benson,
                        1943). The forebody's wake is not modelled.
    Heave and pitch damping
                        The water's added mass, rho (pi/8) b^2 V (strip
                        theory, as in Zarnick, DTNSRDC-78/032), damps her
                        heave, wetted in over the step's first half foot,
                        and her pitch at the step's lever.
    The buoyancy, faded out above the water
                        The model's buoyancy table gave about 1,000 lb with
                        her keel nearly 3 ft clear of the water; buoyancy is
                        the water displaced, so its force and pitch and roll
                        moments are faded out over the keel's first 0.25 ft
                        above the surface. The immersed table is its
                        author's.

**The alpha-rate terms on the dynamic pressure without its sideways part**
(tools/alpha_rate.py, JSBSim issue #89): CLad and Cmadot in datcom_aero.xml,
so that the alpha rate, undefined with the air across her, does not pitch her.

**Her pitch terms keyed to the angle of attack on the dynamic pressure without
its sideways part** (alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15):
Cm_basic, in Systems/datcom_aero.xml. Afloat or slowing on the water in a
crosswind, her angle of attack, atan2(w, u), is set by the little u and w she
has, not by the wind - 2 to 13 degrees afloat in 15 kt across - and on the
whole dynamic pressure, which counts the crosswind, it pitched her nose down by
up to 5,600 ft-lb; on aero/qbarUW-psf, at most 930. Her wing and tailplane are
not swept: the case JSBSim issue #89 tested (on the C172) and the FlightGear
wiki's "JSBSim Aerodynamics" writes up, QBarUW "more appropriate for Cm" than
the whole QBar. In flight, at a few degrees of sideslip, the two differ by its
cosine squared: on her water take-off she runs at 1 to 3 degrees of it, qbarUW
0.998 of qbar, and the nose-down Cm_basic 0.2% less let her come unstuck about
half a second sooner, her long-range run 798 yd where it was 823 (range 676 to
914). **Not moved**: Cmq, her pitch damping - the angle of attack only picks
the coefficient of her pitch rate.

**Her lift keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15): CLwbh. Afloat in 15 kt
straight across her angle of attack is defined, -3 to 14 degrees, but the air
meets her from the side as she swings: on the whole dynamic pressure CLwbh
lifted her by 290 to 2,100 lb; on aero/qbarUW-psf, 20 to 410. JSBSim issue
#89's guidance, written into the FlightGear wiki's "JSBSim Aerodynamics":
QBarUW "is more appropriate for CL than the generic QBar" - a wing's lift is
made by the air in the plane of u and w. Her wing is not swept: the case issue
#89 tested. In flight, at a few degrees of sideslip, the two differ by its
cosine squared. **Not moved** then: CLq, lift with her pitch rate, where the
angle of attack only picks its coefficient - moved below (2026-10-23). Her
lift terms not keyed to the angle of attack (CLdF3L, CLdF3R and CLDe) were not
this change's: they are moved below too.

**Her wing's drag keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-16): CD, and CdDf3L and
CdDf3R, her flaps' drag keyed to the angle of attack, in
Systems/datcom_aero.xml. Afloat in 15 kt straight across her angle of attack is
defined, -3 to 13 degrees, but the air meets her from the side as she swings:
on the whole dynamic pressure they drew her along the wind by 21 to 170 lb; on
aero/qbarUW-psf, 1.5 to 33. The same reasoning as her lift's, above: a wing's
drag at its angle of attack is made by the air in the plane of u and w, and the
sideways part runs along its span - the drag that rolled the J-3 Cub onto her
wing standing in a beam wind (make_j3cub.py). In flight, at a few degrees of
sideslip, the two differ by its cosine squared. Not moved: CdDe, her elevator's
drag, not her wing's - a table of the angle of attack and the deflection (the
angle does not multiply the deflection), under 0.00002 at no deflection at
every angle and 0.029 at most.

**Her lift terms not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-23, with_alpha_terms_on_qbar_uw for a lift with pitch rate whose
coefficient the angle picks): CLdF3L, CLdF3R, CLDe and CLq. On the whole
dynamic pressure they counted the crosswind standing. Afloat she has no brakes
and turns into the wind, so she is not measured standing across it; the
reasoning is the landplanes'. JSBSim issue #89's guidance puts the whole of CL
on QBarUW; alpha_rate.py says why, for swept surfaces too. In flight, at a few
degrees of sideslip, the two differ by its cosine squared.

**Her pitching moments not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-28): CmDe, CmDf3L and CmDf3R. **Not moved**: Cmq, her pitch damping,
whose coefficient the angle of attack picks, named as it was: moved too, she was
left thirty seconds in the stall handed over diving at 140 kt, pitch -23
degrees, and pulled 4.2 g recovering, against 1.4 g on the whole pressure
(tests/unit/test_lesson.cpp); afloat she turns into the wind, so no crosswind
stands across her. On the whole dynamic pressure they counted the crosswind
standing, as her lift did. She is not stood: afloat she has no brakes and turns
into the wind, so it is not across her; her terms are moved as every other's.
The FlightGear wiki's guidance from JSBSim issue #89 puts Cm on QBarUW;
alpha_rate.py says why, for swept surfaces too. In flight, at a few degrees of
sideslip, the two differ by its cosine squared.
"""

import math
import pathlib
import re
import sys

import alpha_rate

ROOT = pathlib.Path(__file__).resolve().parent.parent
PINNED = ROOT / "ext" / "jsbsim"
OUT = ROOT / "assets" / "jsbsim"

PINNED_S23 = PINNED / "aircraft" / "Short_S23"
MODEL = "short_s23"
UNCHANGED_SYSTEMS = ["electrical", "fuel-system", "take-off-ap"]
DRAG_FACTOR = 1.9   # past her greatest lift alone, where the flow has separated
ZERO_LIFT_DRAG = 0.0330
# Her wing's span efficiency, Raymer's estimate for a straight wing (Aircraft
# Design: A Conceptual Approach, eq. 12.48), 1.78 (1 - 0.045 A^0.68) - 0.64,
# on her aspect ratio, 114 ft squared over 1,500 sq ft.
ASPECT_RATIO = 114.0 ** 2 / 1500.0
OSWALD = 1.78 * (1.0 - 0.045 * ASPECT_RATIO ** 0.68) - 0.64
STALL_ALPHA = 0.2443
CLWBH_FACTOR = 0.83       # the pinned model's factor on CLwbh's table
LIFT_JOIN_ALPHA = STALL_ALPHA  # Viterna and Corrigan's from her greatest lift
STALL_CL = CLWBH_FACTOR * 1.845   # CLwbh at STALL_ALPHA, clean
# NACA Report 640 (Hartman and Biermann, 1938), figure 12: a three-blade
# Clark Y propeller's efficiency against advance ratio by blade angle.
FINE_EFFICIENCY_SCALE = 0.957    # fine pitch's thrust above take-off
FINE_FADE = (0.45, 0.65)         # advance ratios over which it comes in: a fit, see above
COARSE_EFFICIENCY_SCALE = 1.055  # coarse pitch's thrust everywhere
SHARED_SYSTEMS = ["hydrodynamics", "hydrodynamic-planing-floats", "sperry-a2-autopilot"]
ENGINE_FILES = ["eng_PegasusXc", "prop_deHavilland5000"]

# The keel, in the model's frame (metres: x aft of the bow, z up), scaled from
# Flight's general arrangement; see above.
FOREBODY_KEEL_Z = -1.83
KEEL = [("KEEL_FORWARD", 4.2, 0.0, FOREBODY_KEEL_Z), ("KEEL_STEP", 10.5, 0.0, FOREBODY_KEEL_Z),
        ("KEEL_AFT", 17.1, 0.0, FOREBODY_KEEL_Z + 1.26)]
# The wing floats' keels at their steps, from the model's hydrodynamics
# (Short_S23.xml's hydrodynamic-planing-floats, and its reference point at
# 341.73 in aft of the bow): feet converted to metres.
FLOAT_X = 341.73 * 0.0254 + 4.72 * 0.3048
FLOAT_Y = 31.99 * 0.3048
FLOAT_Z = -1.71 * 0.3048
FLOATS = [("LEFT_FLOAT_KEEL", FLOAT_X, -FLOAT_Y, FLOAT_Z), ("RIGHT_FLOAT_KEEL", FLOAT_X, FLOAT_Y, FLOAT_Z)]
# Stiff enough that the heaviest boat, 45,000 lb, settles half a foot on its
# keel; a hull's sliding friction.
KEEL_SPRING_LBS_FT = 45000
KEEL_DAMPING_LBS_FT_SEC = 9000
KEEL_FRICTION = 0.5


def replace_once(text, pattern, replacement, what):
    new, n = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f"make_short_s23: could not find {what} - has the pinned model changed?")
    return new


def without_sockets(text):
    if re.search(r"<input\s+port=|type=\"SOCKET\"", re.sub(r"<!--.*?-->", "", text, flags=re.S)):
        raise SystemExit("make_short_s23: the model opens a network socket")
    return text


def keel_skid(name, x, y, z):
    return (f"  <contact type=\"BOGEY\" name=\"{name}\">\n"
            f"   <location unit=\"M\">\n"
            f"    <x> {x:.2f} </x>\n"
            f"    <y> {y:.2f} </y>\n"
            f"    <z> {z:.2f} </z>\n"
            f"   </location>\n"
            f"   <static_friction>  {KEEL_FRICTION:.2f} </static_friction>\n"
            f"   <dynamic_friction> {KEEL_FRICTION:.2f} </dynamic_friction>\n"
            f"   <rolling_friction> {KEEL_FRICTION:.2f} </rolling_friction>\n"
            f"   <spring_coeff unit=\"LBS/FT\">      {KEEL_SPRING_LBS_FT} </spring_coeff>\n"
            f"   <damping_coeff unit=\"LBS/FT/SEC\"> {KEEL_DAMPING_LBS_FT_SEC} </damping_coeff>\n"
            f"   <max_steer unit=\"DEG\"> 0 </max_steer>\n"
            f"   <brake_group> NONE </brake_group>\n"
            f"   <retractable> 0 </retractable>\n"
            f"  </contact>\n")


def airframe():
    text = (PINNED_S23 / "Short_S23.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n  <!-- glideslope: this is JSBSim's Short_S23 with the changes listed in\n"
        r"       tools/make_short_s23.py, which made it. Do not edit it by hand. -->",
        "the file header")
    without_sockets(text)
    for owner in ("fcs/copilot", "sperry-autopilot"):
        text = replace_once(
            text, rf"\n(  <property value=\"0\.0\">{owner}/(aileron|elevator|rudder)-cmd-norm</property>\n){{3}}",
            "\n", f"the {owner} commands' declarations")
    text, n = re.subn(
        r"(<!-- Engine feeder tanks \(carburettor float chambers\)\. -->.*?</propulsion>)",
        lambda m: re.sub(r"(<capacity unit=\"LBS\"> 0\.25 </capacity> <!-- Guess -->\n\s*<contents unit=\"LBS\">) 0\.00 ",
                         r"\1 0.25 ", m.group(1)),
        text, count=1, flags=re.S)
    if n != 1 or text.count('<contents unit="LBS"> 0.25 </contents>') != 4:
        raise SystemExit("make_short_s23: could not find the four float chambers - has the pinned model changed?")
    text, n = re.subn(r"(<capacity unit=\"LBS\"> 2356\.9 </capacity>\n\s*<contents unit=\"LBS\">) 2352\.0 ",
                      r"\1 2351.5 ", text)
    if n != 2:
        raise SystemExit("make_short_s23: could not find the two centre tanks - has the pinned model changed?")
    skids = ("  <!-- glideslope: the keel, which meets land and not water; see\n"
             "       tools/make_short_s23.py. -->\n"
             + "".join(keel_skid(name, x, y, z) for name, x, y, z in KEEL + FLOATS))
    text = replace_once(text, r"(\n </ground_reactions>)", lambda m: "\n" + skids.rstrip("\n") + m.group(1),
                        "the end of the ground reactions")
    return text


def aerodynamics():
    text = (PINNED_S23 / "Systems" / "datcom_aero.xml").read_text()
    text = replace_once(text, r"(<max>)   \.4189    (</max>)", r"\1   .2443    \2", "the alpha limits")
    for what in ("lift", "drag"):
        text = replace_once(
            text,
            rf"(<function name=\"aero/function/ground-effect-factor-{what}\">\s*<description>[^<]*</description>\n)"
            r"\s*<product>\n(\s*<table>.*?</table>\n)\s*</product>\n",
            r"\1\2", f"ground effect's {what} factor")
    return with_drag_polar(with_separated_lift(text))


def with_separated_lift(text):
    """`text` with CLwbh's rows past her greatest lift (LIFT_JOIN_ALPHA)
    Viterna and Corrigan's separated lift (NASA CP-2230, 1982): A1 sin 2a +
    A2 cos^2 a / sin a, A1 = CDmax / 2, A2 making it the table's own lift
    there, out to 90 degrees. The pinned rows fell to -0.2 by 24 degrees and
    held it beyond."""
    m = re.search(r"(<function name=\"aero/coefficient/CLwbh\">.*?<tableData>\n)(.*?)(\n\s*</tableData>)",
                  text, re.S)
    if not m:
        raise SystemExit("make_short_s23: no CLwbh table - has the pinned model changed?")
    rows = [line.split() for line in m.group(2).strip().splitlines()]
    cd_max = 1.11 + 0.018 * ASPECT_RATIO
    a1 = cd_max / 2.0
    join_cl = CLWBH_FACTOR * next(float(r[1]) for r in rows
                                  if abs(float(r[0]) - LIFT_JOIN_ALPHA) < 1e-4)
    a2 = ((join_cl - cd_max * math.sin(LIFT_JOIN_ALPHA) * math.cos(LIFT_JOIN_ALPHA))
          * math.sin(LIFT_JOIN_ALPHA) / math.cos(LIFT_JOIN_ALPHA) ** 2)
    kept = [f"                       {r[0]:>10}     {r[1]}" for r in rows
            if float(r[0]) <= LIFT_JOIN_ALPHA + 1e-4]
    added = []
    for i in range(1, 17):
        a = LIFT_JOIN_ALPHA + i * (math.pi / 2 - LIFT_JOIN_ALPHA) / 16
        cl = a1 * math.sin(2 * a) + a2 * math.cos(a) ** 2 / math.sin(a)
        added.append(f"                       {a:10.4f}     {cl / CLWBH_FACTOR:.4f}")
    return text[:m.start()] + m.group(1) + "\n".join(kept + added) + m.group(3) + text[m.end():]


def with_drag_polar(text):
    """`text` with its DATCOM drag table, times 1.23, made a zero-lift drag and
    a drag due to lift from her wing's span efficiency - the table, times
    DRAG_FACTOR, kept past her greatest lift."""
    m = re.search(r"(<value>)1\.23(</value>) <!-- Adjusted for climb time\. -->\n(\s*<table>\s*"
                  r"<independentVar lookup=\"row\">aero/alpha-rad</independentVar>\s*<tableData>\n)"
                  r"(.*?)(\n\s*</tableData>)", text, re.S)
    if not m:
        raise SystemExit("make_short_s23: no drag table - has the pinned model changed?")
    rows = [line.split() for line in m.group(4).strip().splitlines()]
    k = 1.0 / (math.pi * OSWALD * ASPECT_RATIO)
    # Past her greatest lift, Viterna and Corrigan's separated drag (NASA
    # CP-2230, 1982): CDmax sin^2 a + B2 cos a, CDmax = 1.11 + 0.018 A, B2
    # making it her polar's drag at the stall.
    stall_cd = ZERO_LIFT_DRAG + k * STALL_CL ** 2
    cd_max = 1.11 + 0.018 * ASPECT_RATIO
    b2 = (stall_cd - cd_max * math.sin(STALL_ALPHA) ** 2) / math.cos(STALL_ALPHA)
    alphas = [float(a) for a, _ in rows if float(a) <= STALL_ALPHA]
    alphas += [STALL_ALPHA + i * (math.pi / 2 - STALL_ALPHA) / 16 for i in range(1, 17)]
    separated = "\n".join(
        f"                       {a:8.4f}  "
        f"{(cd_max * math.sin(a) ** 2 + b2 * math.cos(a)) if a >= STALL_ALPHA else 0.0:.5f}"
        for a in alphas)
    polar = (
        f"<value>1.0</value> <!-- glideslope: a zero-lift drag and a drag due to lift, and the\n"
        f"                    DATCOM table past her greatest lift; see tools/make_short_s23.py. -->\n"
        f"               <max>\n"
        f"                  <sum>\n"
        f"                     <value>{ZERO_LIFT_DRAG:.4f}</value>\n"
        f"                     <product>\n"
        f"                        <value>{k:.5f}</value> <!-- 1 / (pi e A), e = {OSWALD:.3f}, A = {ASPECT_RATIO:.2f} -->\n"
        f"                        <property>aero/cl-squared</property>\n"
        f"                     </product>\n"
        f"                  </sum>\n"
        f"                  <product>\n"
        f"                     <value>{DRAG_FACTOR}</value>\n"
        f"                     <table>\n"
        f"                        <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
        f"                        <tableData>\n{separated}\n"
        f"                        </tableData>\n"
        f"                     </table>\n"
        f"                  </product>\n"
        f"               </max>")
    end = text.index("</table>", m.end()) + len("</table>")
    return text[:m.start()] + polar + text[end:]


def propeller():
    """The airscrew with its efficiency as NACA Report 640's, figure 12: the
    coarse column's thrust times COARSE_EFFICIENCY_SCALE, the fine column's
    times FINE_EFFICIENCY_SCALE from the end of the take-off, faded in over
    FINE_FADE."""
    text = (PINNED / "engine" / "prop_deHavilland5000.xml").read_text()
    m = re.search(r"(<table name=\"C_THRUST\" type=\"internal\">\s*<tableData>\s*20\.0\s+28\.0\n)"
                  r"(.*?)(\n\s*</tableData>)", text, re.S)
    if not m:
        raise SystemExit("make_short_s23: no thrust table - has the pinned airscrew changed?")
    lo, hi = FINE_FADE

    def fine_scale(j):
        f = min(1.0, max(0.0, (j - lo) / (hi - lo)))
        return 1.0 + f * (FINE_EFFICIENCY_SCALE - 1.0)

    rows = []
    for line in m.group(2).strip().splitlines():
        j, fine, coarse = (float(v) for v in line.split())
        rows.append(f"   {j:.5f}  {fine * fine_scale(j):8.5f}  {coarse * COARSE_EFFICIENCY_SCALE:8.5f}")
    return (text[:m.start()] + m.group(1) + "\n".join(rows) + m.group(3) + text[m.end():]).replace(
        "<propeller name=\"deHavilland5000\">",
        "<propeller name=\"deHavilland5000\">\n <!-- glideslope: the thrust tables' efficiency as NACA\n"
        "      Report 640's, figure 12; see tools/make_short_s23.py. -->", 1)


# The planing hull (see "Hydrodynamics" above). Feet, from the keel skids and
# the model's hydrodynamic reference point (HRP): the step 1.83 m below it and
# 10.5 m aft of the bow, the HRP 341.73 in aft of it.
STEP_BELOW_HRP_FT = -FOREBODY_KEEL_Z / 0.3048
STEP_AFT_OF_HRP_FT = (10.5 - 341.73 * 0.0254) / 0.3048
# Savitsky's dynamic lift coefficient, C_L0 = 0.012 lambda^0.5 tau^1.1.
SAVITSKY_DYNAMIC = 0.012
# The added mass's heave damping, rho (pi / 8) b^2 V, full once the step is
# this deep; and its lever in pitch, the step's distance aft of the centre of
# gravity (9.12 m), squared.
FULLY_WETTED_FT = 0.5
STEP_LEVER_FT = (10.5 - 9.12) / 0.3048

PLANING_LIFT = f"""  <fcs_function name="hydro/hull-lift-lbs">
   <function>
    <description>Lift due to hull moving in water</description>
    <!-- glideslope: Savitsky's planing lift, smooth in the step's immersion,
         in place of the pinned model's step to 20,000 lb; see
         tools/make_short_s23.py. -->
    <product>
     <value>{SAVITSKY_DYNAMIC}</value>
     <pow>
      <property>hydro/glideslope/forebody-wetted-length-beams</property>
      <value>0.5</value>
     </pow>
     <pow>
      <property>hydro/glideslope/trim-deg</property>
      <value>1.1</value>
     </pow>
     <value>0.5</value>
     <property>hydro/glideslope/rho-slug_ft3</property>
     <property>hydro/v-fps</property>
     <property>hydro/v-fps</property>
     <property>hydro/hull-beam-ft2</property>
    </product>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/rho-slug_ft3">
   <function>
    <quotient>
     <property>hydro/environment/rho-lbs_ft3</property>
     <value>32.174</value>
    </quotient>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/step-immersion-ft">
   <description>How deep the main step is below the water.</description>
   <function>
    <max>
     <value>0.0</value>
     <difference>
      <sum>
       <product>
        <value>{STEP_BELOW_HRP_FT:.3f}</value>
        <cos><product><property>hydro/hull/pitch-deg</property><value>0.0174533</value></product></cos>
       </product>
       <product>
        <value>{STEP_AFT_OF_HRP_FT:.3f}</value>
        <sin><product><property>hydro/hull/pitch-deg</property><value>0.0174533</value></product></sin>
       </product>
      </sum>
      <property>hydro/hull/height-agl-ft</property>
     </difference>
    </max>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/trim-deg">
   <description>The planing bottom's incidence to the water's flow: the
    hull's trim and the angle her sink adds to it.</description>
   <function>
    <max>
     <value>0.5</value>
     <min>
      <value>20.0</value>
      <sum>
       <property>hydro/hull/pitch-deg</property>
       <product>
        <value>57.29578</value>
        <atan2>
         <property>velocities/v-down-fps</property>
         <max><value>1.0</value><property>hydro/v-fps</property></max>
        </atan2>
       </product>
      </sum>
     </min>
    </max>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/forebody-wetted-length-beams">
   <description>The keel's wetted length aft of the step's waterline, in
    beams: its immersion over the sine of the trim.</description>
   <function>
    <min>
     <value>4.0</value>
     <quotient>
      <property>hydro/glideslope/step-immersion-ft</property>
      <product>
       <property>hydro/hull-beam-ft</property>
       <sin><product><property>hydro/glideslope/trim-deg</property><value>0.0174533</value></product></sin>
      </product>
     </quotient>
    </min>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/added-mass-damping-lbs_fps">
   <description>The heave damping of the water's added mass, rho (pi/8) b^2 V,
    wetted in over the step's first {FULLY_WETTED_FT} ft.</description>
   <function>
    <product>
     <value>0.392699</value>
     <property>hydro/glideslope/rho-slug_ft3</property>
     <property>hydro/hull-beam-ft2</property>
     <property>hydro/v-fps</property>
     <min>
      <value>1.0</value>
      <quotient>
       <property>hydro/glideslope/step-immersion-ft</property>
       <value>{FULLY_WETTED_FT}</value>
      </quotient>
     </min>
    </product>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/heave-damping-lbs">
   <function>
    <product>
     <property>hydro/glideslope/added-mass-damping-lbs_fps</property>
     <property>velocities/v-down-fps</property>
    </product>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/step-pitch-damping-lbsft">
   <function>
    <product>
     <value>{-STEP_LEVER_FT ** 2:.2f}</value>
     <property>hydro/glideslope/added-mass-damping-lbs_fps</property>
     <property>velocities/thetadot-rad_sec</property>
    </product>
   </function>
  </fcs_function>
"""


# **Buoyancy is the water her hull displaces**, so none once her keel is
# clear of it. The pinned model's table, from its author's Gerris runs, gives
# about 1,000 lb with the hull's reference point 7 ft up and her keel near 3 ft
# clear of the water, and none only from 9 ft. Its force and its pitch and
# roll moments are kept where the keel is in the water and faded out over the keel's first
# BUOYANCY_FADE_FT above it.
BUOYANCY_FADE_FT = 0.25
HRP_X_M = 341.73 * 0.0254


def keel_depth_ft(name, x, z):
    """A keel point's depth below the reference point at the hull's pitch, as
    a JSBSim expression: below cos(pitch) + aft sin(pitch), feet."""
    below = -z / 0.3048
    aft = (x - HRP_X_M) / 0.3048
    pitch = "<product><property>hydro/hull/pitch-deg</property><value>0.0174533</value></product>"
    return (f"      <sum><!-- {name} -->\n"
            f"       <product><value>{below:.3f}</value><cos>{pitch}</cos></product>\n"
            f"       <product><value>{aft:.3f}</value><sin>{pitch}</sin></product>\n"
            f"      </sum>\n")


HULL_WET = (f"""  <fcs_function name="hydro/glideslope/keel-immersion-ft">
   <description>glideslope: how deep the lowest point of the keel is in the
    water, negative where it is clear; see tools/make_short_s23.py.</description>
   <function>
    <difference>
     <max>
""" + "".join(keel_depth_ft(name, x, z) for name, x, _, z in KEEL) + f"""     </max>
     <property>hydro/hull/height-agl-ft</property>
    </difference>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/hull-wet-norm">
   <description>glideslope: 1 with the keel in the water, falling to 0 as it
    rises {BUOYANCY_FADE_FT} ft clear of it.</description>
   <function>
    <max>
     <value>0.0</value>
     <min>
      <value>1.0</value>
      <sum>
       <value>1.0</value>
       <quotient>
        <property>hydro/glideslope/keel-immersion-ft</property>
        <value>{BUOYANCY_FADE_FT}</value>
       </quotient>
      </sum>
     </min>
    </max>
   </function>
  </fcs_function>

""")


def pressure_centre(body):
    """Savitsky's centre of pressure of a planing surface, forward of its
    trailing edge, lambda b (0.75 - 1 / (5.21 C_V^2 / lambda^2 + 2.39)), as a
    JSBSim function; and the pitch moment of its lift about the reference
    point, where JSBSim applies the water's forces."""
    lam = f"hydro/glideslope/{body}-wetted-length-beams"
    return f"""  <fcs_function name="hydro/glideslope/{body}-pressure-centre-ft">
   <description>Savitsky's centre of pressure, forward of the trailing edge:
    lambda b (0.75 - 1 / (5.21 C_V^2 / lambda^2 + 2.39)).</description>
   <function>
    <product>
     <property>{lam}</property>
     <property>hydro/hull-beam-ft</property>
     <difference>
      <value>0.75</value>
      <quotient>
       <value>1.0</value>
       <sum>
        <value>2.39</value>
        <quotient>
         <product>
          <value>5.21</value>
          <property>hydro/coefficients/C_V</property>
          <property>hydro/coefficients/C_V</property>
         </product>
         <max>
          <value>0.0001</value>
          <product>
           <property>{lam}</property>
           <property>{lam}</property>
          </product>
         </max>
        </quotient>
       </sum>
      </quotient>
     </difference>
    </product>
   </function>
  </fcs_function>
"""


# **The afterbody planes too, once her trim passes its keel's angle.** NACA's
# porpoising work treats a hull as two planing surfaces in tandem, the
# afterbody's keel inclined to the forebody's (Benson and Klein, NACA ARR
# 3F30, 1943: models with the afterbody's keel at 7 degrees to the
# forebody's), and the upper trim limit - high-angle porpoising, "in which
# both the forebody and the afterbody are involved" - as where her trim
# brings the afterbody into the water; Benson (NACA TN 923, 1943) puts the
# danger of skipping where the trim reaches that of the afterbody's keel
# horizontal. Hers rises 1.26 m over the 6.6 m from the step to the stern
# (the keel skids above), 10.8 degrees. Past it the afterbody is a planing
# surface of its own, by the same Savitsky lift as the forebody's at its own
# trim - hers less that angle - and its wetted length from the stern forward,
# its immersion over the sine of that trim, capped at its length; its lift
# acts at Savitsky's centre of pressure, l_p = lambda b (0.75 - 1 / (5.21
# C_V^2 / lambda^2 + 2.39)) forward of its trailing edge, the stern, and so
# pitches her nose down. Not modelled: the forebody's wake, which lowers the
# water the afterbody meets (so this wets it a little early), and her
# afterbody's narrowing; its beam is taken as the hull's.
STERN_X_M, _, STERN_Z_M = KEEL[2][1], KEEL[2][2], KEEL[2][3]
AFTERBODY_ANGLE_DEG = math.degrees(math.atan2(STERN_Z_M - FOREBODY_KEEL_Z, STERN_X_M - KEEL[1][1]))
AFTERBODY_LENGTH_FT = math.hypot(STERN_X_M - KEEL[1][1], STERN_Z_M - FOREBODY_KEEL_Z) / 0.3048
STERN_AFT_OF_HRP_FT = (STERN_X_M - 341.73 * 0.0254) / 0.3048
DEG = "<value>0.0174533</value>"

AFTERBODY = (f"""
  <fcs_function name="hydro/glideslope/afterbody-trim-deg">
   <description>The afterbody keel's incidence to the water: the planing
    trim less its angle to the forebody keel, {AFTERBODY_ANGLE_DEG:.2f} degrees.</description>
   <function>
    <max>
     <value>0.0</value>
     <difference>
      <property>hydro/glideslope/trim-deg</property>
      <value>{AFTERBODY_ANGLE_DEG:.3f}</value>
     </difference>
    </max>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/stern-immersion-ft">
   <description>How deep the afterbody keel's aft end is below the water.</description>
   <function>
    <max>
     <value>0.0</value>
     <difference>
""" + keel_depth_ft("KEEL_AFT", STERN_X_M, STERN_Z_M).replace("      ", "      ", 1) + f"""      <property>hydro/hull/height-agl-ft</property>
     </difference>
    </max>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/afterbody-wetted-length-beams">
   <description>The afterbody keel's wetted length forward of the stern, in
    beams, no longer than the afterbody ({AFTERBODY_LENGTH_FT:.2f} ft).</description>
   <function>
    <max>
     <value>0.01</value>
     <min>
      <quotient>
       <value>{AFTERBODY_LENGTH_FT:.3f}</value>
       <property>hydro/hull-beam-ft</property>
      </quotient>
      <quotient>
       <property>hydro/glideslope/stern-immersion-ft</property>
       <product>
        <property>hydro/hull-beam-ft</property>
        <max>
         <value>0.001</value>
         <sin><product><property>hydro/glideslope/afterbody-trim-deg</property>{DEG}</product></sin>
        </max>
       </product>
      </quotient>
     </min>
    </max>
   </function>
  </fcs_function>

  <fcs_function name="hydro/glideslope/afterbody-lift-lbs">
   <description>The afterbody's planing lift, Savitsky's at its own trim.</description>
   <function>
    <product>
     <value>{SAVITSKY_DYNAMIC}</value>
     <pow>
      <property>hydro/glideslope/afterbody-wetted-length-beams</property>
      <value>0.5</value>
     </pow>
     <pow>
      <property>hydro/glideslope/afterbody-trim-deg</property>
      <value>1.1</value>
     </pow>
     <value>0.5</value>
     <property>hydro/glideslope/rho-slug_ft3</property>
     <property>hydro/v-fps</property>
     <property>hydro/v-fps</property>
     <property>hydro/hull-beam-ft2</property>
     <min>
      <value>1.0</value>
      <quotient>
       <property>hydro/glideslope/stern-immersion-ft</property>
       <value>0.001</value>
      </quotient>
     </min>
    </product>
   </function>
  </fcs_function>

""" + pressure_centre("afterbody") + f"""
  <fcs_function name="hydro/glideslope/afterbody-pitch-moment-lbsft">
   <description>The afterbody's lift, acting at its centre of pressure aft
    of the reference point, where the water's forces are applied: nose down.</description>
   <function>
    <product>
     <value>-1.0</value>
     <property>hydro/glideslope/afterbody-lift-lbs</property>
     <difference>
      <value>{STERN_AFT_OF_HRP_FT:.3f}</value>
      <property>hydro/glideslope/afterbody-pressure-centre-ft</property>
     </difference>
     <cos><product><property>hydro/hull/pitch-deg</property>{DEG}</product></cos>
    </product>
   </function>
  </fcs_function>
""")

FOREBODY_MOMENT = pressure_centre("forebody") + f"""
  <fcs_function name="hydro/glideslope/forebody-pitch-moment-lbsft">
   <description>The forebody's planing lift acts at its centre of pressure,
    forward of the step, not at the reference point where JSBSim applies
    it: the moment that moves it there.</description>
   <function>
    <product>
     <value>-1.0</value>
     <property>hydro/hull-lift-lbs</property>
     <difference>
      <value>{STEP_AFT_OF_HRP_FT:.3f}</value>
      <property>hydro/glideslope/forebody-pressure-centre-ft</property>
     </difference>
     <cos><product><property>hydro/hull/pitch-deg</property>{DEG}</product></cos>
    </product>
   </function>
  </fcs_function>
"""


def hydrodynamics():
    text = (PINNED_S23 / "Systems" / "Short_S23-hydrodynamics.xml").read_text()
    text = replace_once(text, r"(  <fcs_function name=\"hydro/buoyancy-lbs\">)",
                        lambda m: HULL_WET + m.group(1), "the hull's buoyancy")
    for name in ("buoyancy-lbs", "buoyancy-pitch-moment-lbsft", "buoyancy-roll-moment-lbsft"):
        text = replace_once(text, rf"(<fcs_function name=\"hydro/{name}\">.*?<product>\n.*?</value>\n)",
                            r"\1     <property>hydro/glideslope/hull-wet-norm</property>\n",
                            f"the hull's {name}")
    text = replace_once(text, r"  <fcs_function name=\"hydro/hull-lift-lbs\">.*?</fcs_function>\n",
                        lambda m: PLANING_LIFT + FOREBODY_MOMENT + AFTERBODY, "the hull's lift")
    text = replace_once(text, r"(      <property>hydro/damping-lift-lbs</property>\n)",
                        r"\1      <property>hydro/glideslope/heave-damping-lbs</property>\n"
                        r"      <property>hydro/glideslope/afterbody-lift-lbs</property>\n",
                        "the hull's lifts' sum")
    return replace_once(text, r"(   <input> hydro/hull-pitch-moment-lbsft </input>\n)",
                        r"\1   <input> hydro/glideslope/step-pitch-damping-lbsft </input>\n"
                        r"   <input> hydro/glideslope/forebody-pitch-moment-lbsft </input>\n"
                        r"   <input> hydro/glideslope/afterbody-pitch-moment-lbsft </input>\n",
                        "the pitch moments' sum")


def engines():
    text = (PINNED_S23 / "Systems" / "engines.xml").read_text()
    for control in ("throttle", "advance", "mixture"):
        text = replace_once(
            text, rf"\n( <property value=\"0\.0\">fcs/{control}-cmd-norm\[\d\]</property>\n){{4}}", "",
            f"the {control} commands' declarations")
    for i in range(4):
        text = replace_once(
            text,
            rf"(<switch name=\"fcs/propeller/blade-angle-deg\[{i}\]\">\s*<default value=\")20\.0(\"/>\s*"
            rf"<test value=\")28\.0(\">\s*fcs/advance-cmd-norm\[{i}\]) GT 0\.5",
            r"\g<1>28.0\g<2>20.0\g<3> GT 0.5",
            f"engine {i}'s pitch lever")
    rate = "     <property>fcs/boost/control-rate-factor</property>\n"
    if text.count(rate) != 4:
        raise SystemExit("make_short_s23: could not find the four boost controls - has the pinned model changed?")
    text = text.replace(rate, rate + "     <gt>\n      <property>simulation/dt</property>\n"
                        "      <value>0.0</value>\n     </gt>\n")
    text = replace_once(
        text, r"( <property value=\"0\.500\">fcs/boost/control-range-norm</property>\n)",
        r"\1 <!-- glideslope: the boost controls hold still while time is stopped;\n"
        r"      a trim settles them by flying. See tools/make_short_s23.py. -->\n"
        r' <property value="1">glideslope/trim/settle-throttle-loops</property>\n',
        "the boost control's range")
    return replace_once(
        text, r"( <documentation>\n   The propeller pitch control only had the settings FINE and COARSE.\n)",
        r"\1   glideslope: the lever in FINE at 1, glideslope's highest rpm, and COARSE\n"
        r"   at 0; see tools/make_short_s23.py.\n",
        "the pitch control's documentation")


SELECTOR = """ <channel name="The flap selector">
  <!-- glideslope: the pilot's flap control, fcs/flap-cmd-norm, runs the motor
       out or in until the flaps are where it puts them; see
       tools/make_short_s23.py. -->
  <summer name="fcs/flap/selector-error-norm">
   <input>fcs/flap-cmd-norm</input>
   <input>-fcs/flap-pos-norm</input>
  </summer>
  <switch name="fcs/flap/selector-motor-cmd-norm">
   <default value="0.0"/>
   <test value="1.0">
    fcs/flap/selector-error-norm GT 0.01
   </test>
   <test value="-1.0">
    fcs/flap/selector-error-norm LT -0.01
   </test>
   <output>fcs/flap-motor-cmd-norm</output>
  </switch>
 </channel>

"""


def flaps():
    text = (PINNED_S23 / "Systems" / "flaps.xml").read_text()
    text = replace_once(text, r"<property value=\"0\.0\">fcs/flap-power-cmd-norm</property>",
                        "<property value=\"1.0\">fcs/flap-power-cmd-norm</property>",
                        "the flap motor's power switch")
    return replace_once(text, r"( <channel name=\"Flap indicator lights\">)",
                        lambda m: SELECTOR + m.group(1), "the flap indicator lights")


def outputs():
    out = {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "aircraft" / MODEL / "Systems" / "datcom_aero.xml":
            alpha_rate.with_unkeyed_terms_on_qbar_uw(alpha_rate.with_alpha_terms_on_qbar_uw(
                alpha_rate.with_qbar_uw(aerodynamics(), "make_short_s23"), "make_short_s23",
                {"PITCH": ["aero/coefficient/Cm_basic"],
                 "LIFT": ["aero/coefficient/CLwbh", "aero/coefficient/CLq"],
                 "DRAG": ["aero/coefficient/CD",
                          "aero/coefficient/CdDf3L",
                          "aero/coefficient/CdDf3R"]}), "make_short_s23",
            {"LIFT": ["aero/coefficient/CLdF3L",
             "aero/coefficient/CLdF3R",
             "aero/coefficient/CLDe"],
             "PITCH": ["aero/coefficient/CmDe",
             "aero/coefficient/CmDf3L",
             "aero/coefficient/CmDf3R"]}),
        OUT / "aircraft" / MODEL / "Systems" / "engines.xml": engines(),
        OUT / "aircraft" / MODEL / "Systems" / "flaps.xml": flaps(),
        OUT / "aircraft" / MODEL / "Systems" / "Short_S23-hydrodynamics.xml": hydrodynamics(),
    }
    for name in UNCHANGED_SYSTEMS:
        out[OUT / "aircraft" / MODEL / "Systems" / f"{name}.xml"] = \
            (PINNED_S23 / "Systems" / f"{name}.xml").read_text()
    for name in SHARED_SYSTEMS:
        out[OUT / "systems" / f"{name}.xml"] = (PINNED / "systems" / f"{name}.xml").read_text()
    for name in ENGINE_FILES:
        out[OUT / "engine" / f"{name}.xml"] = (PINNED / "engine" / f"{name}.xml").read_text()
    out[OUT / "engine" / "prop_deHavilland5000.xml"] = propeller()
    return out


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
        print("assets/jsbsim/ is not what tools/make_short_s23.py makes:", file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print("run: python3 tools/make_short_s23.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
