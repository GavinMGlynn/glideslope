#!/usr/bin/env python3
"""make_f22.py - glideslope's Lockheed Martin F-22A Raptor, from JSBSim's f22.

JSBSim's f22, as pinned in ext/jsbsim, is an F-22A with F119-PW-100 engines.
Its figures here are from the Department of Defense's Selected Acquisition
Report for the F-22 (December 2010) - its demonstrated supercruise, its
acceleration and its sustained load factor - and the Air Force's F-22 fact
sheet - its speed class and ceiling - in assets/figures/f22.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_f22.py           write the files
    python3 tools/make_f22.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Airframe (aircraft/f22/f22.xml)
    The control surfaces move
                        Four of the flight controls' actuators - the ailerons',
                        rudders', elevators' and thrust vectoring's - were
                        given a lag of 0, which JSBSim 1.3.1 takes as a filter
                        that never moves: every surface stayed where it
                        started, and the model could not be flown. The lag is
                        removed, as an actuator without one follows its
                        command.
    Military power at 0.99
                        The throttles reached the engines through a gain of
                        1.001, so 0.99 - military power in glideslope's
                        fighters, their afterburners lighting above it - lit
                        the afterburners. The gain is 1.
    Weights and loading The fact sheet's empty weight, 43,340 lb, where the
                        model had 43,430; its internal fuel, 18,000 lb, where
                        the model's two tanks held 18,700; and stores - the
                        six AIM-120s and two AIM-9s of the report's missile
                        load - at the centre of gravity, which the model had
                        nowhere to put.
    Lift with Mach      The model's lift did not change with Mach at all. Its
                        lift curve, whose slope at low Mach is DATCOM's for
                        its wing, is carried across the Mach range in
                        proportion to the lift slope (fighter.lift_slope), as
                        the F-15C's is.
    The tail's moment with Mach
                        Nor did its tail's pitching moment, which is carried
                        across the Mach range the same way: the tail is a wing
                        too.
    The pitch loop's gains
                        The flight controls' pitch loop, its gains scheduled on
                        calibrated airspeed alone, went into a limit cycle -
                        the stabilators beating at their rate limit, three
                        times a second - wherever the tail's moment was great:
                        in the pinned model past Mach 2.06 at 36,000 ft, and
                        with the lift above past Mach 1.85. Its gains now fall
                        in inverse proportion to the dynamic pressure times the
                        tail's effectiveness past PITCH_GAIN_PRESSURE, as a
                        fly-by-wire aircraft's are scheduled.
    The pitch loop on her wheels
                        The pitch loop commands a pitch rate through an
                        integrator that alone drives the stabilators, and on
                        her wheels, with her nose wheel holding her nose
                        down, it wound to full stabilator whatever the stick
                        asked: she came up at ten to thirteen degrees a
                        second. It is given a ground mode, as fly-by-wire
                        aircraft have one: on any wheel, the integrator is
                        washed out (PITCH_GROUND_WASHOUT_PER_S) and its gain
                        raised by as much, so that the stick commands a pitch
                        rate through a proportional loop, the stabilator in
                        proportion to the rate she is short of; off her
                        wheels both fade to the flight law over
                        PITCH_GROUND_BLEND_S. NASA TM-112208 (Franklin, 1997,
                        a lift-fan STOVL fighter's control laws) initialises
                        its rate command integrator on weight on wheels; the
                        A320's normal law flies a ground mode, stick to
                        elevator, and blends it into its flight mode over
                        five seconds after lift-off. The integrator's state is
                        the stabilator's command in both laws, so the flight
                        law takes over from where the ground law left it.
                        **It engages on every touch of a wheel, not only at
                        the take-off**: at once on a landing, a bounce or a
                        touch-and-go, and it fades out again over the same
                        five seconds after each. That is unmeasured: she has
                        no approach speed, so nothing lands her. Clear of the
                        ground and five seconds after her wheels have left
                        it, the loop is the pinned model's, so her figures in
                        the air are unchanged by it; within those five
                        seconds of a lift-off she flies the blend.
                        Held instead of washed out, the integrator would
                        leave her nothing on her wheels to lift the nose
                        with.
    The roll stick      The stick commanded a roll rate in proportion to its
                        travel - its first tenth 22 degrees a second - and any
                        pilot's small corrections, a person's or glideslope's
                        autopilot's, overshot and rolled the aircraft over. It
                        is shaped, as fly-by-wire sticks are: a quarter as
                        sensitive at the centre, and full travel the full rate
                        (ROLL_STICK_CENTRE_GAIN).
    The held trim       Her pitch trim is added to the stick, and so
                        commands a pitch rate, which her pitch loop's
                        integrator turns into elevator over time; with
                        JSBSim's time stopped while it trims, nothing moved
                        and she could not be trimmed: started for a player
                        and left alone she fell 500 ft in thirty seconds.
                        The pilot's trim is left as it is. A second input,
                        fcs/pitch-trim-held-norm, is added to the elevators'
                        command after the loop: the deflection the server
                        holds her at until her player's first input, found
                        by glideslope's trim (sim/aircraft.cpp), about which
                        the integrator works from nought.
    The throttle lags   The throttles reached the engines through a lag
                        filter of a second, which with time stopped does not
                        move either, so the trim could not find her power.
                        Each is an actuator with the same lag, which JSBSim
                        passes straight through while it trims and lags the
                        same as the filter in flight.
    The leading edge flaps' filter
                        The flaps follow her angle of attack through a lead
                        filter, (2s + 7.25) / (s + 7.25), which with time
                        stopped lagged each angle the trim tried, so that her
                        lift depended on the angles tried before and the trim
                        never settled. It is written as twice the angle less
                        its lag, the lag an actuator: the same filter in
                        flight, and the angle itself while JSBSim trims.
    Drag with Mach      The model's drag at zero lift, its lift-dependent drag
                        of 0.07 CL^2 - a span efficiency of 1.9, which no wing
                        has - and its drag rise are made the fighters' drag by
                        Mach (fighter.drag_functions): the model's own 0.0082
                        at zero lift, subsonic; a wave drag of WAVE_DRAG, which
                        the report's acceleration pins; and a span efficiency,
                        SPAN_EFFICIENCY, which its sustained load factor pins.
                        The drag's shape past Mach 1.2 is the F-15C's, which the
                        F-15C's charts pin and no published F-22 figure does.

  Engines (engine/F119-PW-100.xml, from JSBSim's F119-PW-1.xml)
    The engine's thrust The fact sheet's "35,000-pound class" with
                        afterburner, where the model had 37,000; the model's
                        military thrust, 26,950 lb, is kept, as nothing
                        published gives it.
    Thrust with speed and height
                        fighter.thrust_lapse, as the F-15C's F100: the rise in
                        cold air, the fall past the throttle ratio and the loss
                        above the tropopause are the F100's, which the F-15C's
                        charts pin; the throttle ratios themselves - how fast
                        the engine can go before its thrust falls - are the
                        F119's own, set by the report's supercruise at military
                        power and its acceleration in afterburner. The military
                        table allows for the idle thrust JSBSim adds to it.

**The alpha-rate terms on the dynamic pressure without its sideways part**
(tools/alpha_rate.py, JSBSim issue #89): braked to a stop in a crosswind, her
angle of attack is undefined, its rate swings, and multiplied by the whole
dynamic pressure it pitched her by tens of degrees a second.

**Her pitch terms keyed to the angle of attack on the dynamic pressure without
its sideways part** (alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15):
Cma_M, the change in her Cm-alpha with Mach number, and CmDh_alpha, her
Cm-alpha itself. She has no Cm-alpha term of her own: it is the column at no
deflection of CmDh, her tailerons' table of the angle of attack and their
deflection, and with_cm_alpha_split_out makes that column its own function and
leaves CmDh the deflection's increment over it - the two adding to CmDh as it
was, so that on the same pressure nothing would move. Her wing is swept, 30.9
degrees at the quarter chord, where JSBSim issue #89 did not look; simple sweep
theory (NACA Report 863; alpha_rate.py's docstring) puts the part of a beam
wind's moment that goes with the angle of attack near nothing standing, as
qbarUW does. Standing on her brakes in 15 kt across, at 89 to 90 degrees of the
angle of attack, CmDh gave 9,600 ft-lb nose down on the whole dynamic pressure;
split and moved, CmDh_alpha gives nothing and the increment at most 19 ft-lb.
Cma_M is nothing below Mach 0.6. Standing, qbarUW is the nearer; in flight, at
a few degrees of sideslip, the two differ by its cosine squared, and the
theory's pressure lies between them, under 1% from either (alpha_rate.py).
**Not moved**: CmDh, the tailerons' increment - an elevator term, its power;
CmDtv, her thrust vectoring's moment, made by her engines, not her wing; and
Cmq, her pitch damping, where the angle of attack only picks the coefficient of
her pitch rate.

**Her lift keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15): CLalpha, and CLDlef, her
leading-edge flaps' lift keyed to the angle of attack: the same wing's lift,
its leading edge drooped. Standing on her brakes in 15 kt straight across, her
angle of attack, atan2(w, u), was -175 to 120 degrees, and on the whole dynamic
pressure, which counts the crosswind, the term lifted her by up to 660 lb
either way; on aero/qbarUW-psf, nothing. JSBSim issue #89's guidance, written
into the FlightGear wiki's "JSBSim Aerodynamics": QBarUW "is more appropriate
for CL than the generic QBar" - a wing's lift is made by the air in the plane
of u and w. Her wing is swept, 30.9 degrees at the quarter chord: the sweep
argument above (simple sweep theory, NACA Report 863) holds for the lift of the
same wing as for its moment - standing, qbarUW is the nearer; in flight, at a
few degrees of sideslip, the two are under 1% apart. **Not moved** then: CLq,
lift with her pitch rate, where the angle of attack only picks its coefficient
(as Cmq above) - moved below (2026-10-23). Her lift terms not keyed to the
angle of attack (dCLflap, dCLsb and CLde) were not this change's: standing,
they left her 260 to 280 lb, and they are moved below too. Two CLDh terms of
the pinned model are commented out, and not read.

**Her wing's drag keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-16): CDseparated, and CDDlef,
her leading-edge flaps' drag keyed to the angle of attack. CDseparated, her
drag past the flow separating, is a product of her lift coefficient and the
angle of attack; standing in 15 kt across it was under 0.1 lb on either
pressure, and it is moved as the same wing's drag keyed to the same angle, for
slowing and flight. CDDlef was nothing standing, her flaps at rest. The same
reasoning as her lift's, above: a wing's drag at its angle of attack is made by
the air in the plane of u and w, and the sideways part runs along its span -
the drag that rolled the J-3 Cub onto her wing standing in a beam wind
(make_j3cub.py). In flight, at a few degrees of sideslip, the two differ by its
cosine squared. Her wing is swept: moved on the same argument as her lift,
simple sweep theory (alpha_rate.with_alpha_terms_on_qbar_uw). Not moved: CDq
and CDq_Dlef, drag with pitch rate - the angle of attack only picks the
coefficient of her pitch rate.

**Her lift terms not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-23, with_alpha_terms_on_qbar_uw for a lift with pitch rate whose
coefficient the angle picks): dCLflap, dCLsb, CLde and CLq. On the whole
dynamic pressure they counted the crosswind standing. Standing on her brakes
with her engines stopped in 15 kt straight across, a lifting control set one at
a time, they lifted her by up to 551 lb (speedbrake out; 260 to 282 with
everything neutral, her flaperons scheduled down); now under 0.1 lb at every
setting (the standing test in tests/unit/test_departure.cpp). JSBSim issue
#89's guidance puts the whole of CL on QBarUW; alpha_rate.py says why, for
swept surfaces too. In flight, at a few degrees of sideslip, the two differ by
its cosine squared.

**Her pitching moments not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-28): every one of her pitching terms is keyed to it, and three, named
until now because the angle only picks their coefficient, move
(with_alpha_terms_on_qbar_uw): CmDh, her tailerons', CmDtv and Cmq. CmDtv, her
thrust vectoring's, is a coefficient on the dynamic pressure, not her engines'
thrust: standing in calm air it is nothing on either pressure, and now in a
crosswind too. On the whole dynamic pressure they counted the crosswind
standing, as her lift did. Standing on her brakes with her engines stopped in 15
kt straight across, a control set one at a time, they pitched her by up to
13,785 ft-lb (stick back); now by 0.13 ft-lb at most (the standing test in
tests/unit/test_departure.cpp). The FlightGear wiki's guidance from JSBSim issue
#89 puts Cm on QBarUW; alpha_rate.py says why, for swept surfaces too. In
flight, at a few degrees of sideslip, the two differ by its cosine squared.

**Her side force out of her plane of symmetry standing in a beam wind**
(side_force.without_crossflow, 2026-10-29). JSBSim makes it along its wind
axes' y, which at 90 degrees of sideslip lies in her plane of symmetry at an
undefined angle of attack: standing in 15 kt across, it pushed her fore and
aft or up and down and pitched her about her centre of gravity. Its
crossflow's share, sin^2 beta, which along the wind is her drag with
sideslip, is now left out of it; standing in 15 kt across, the
air's force in her plane of symmetry is under a thousandth of her weight (the
standing tests in tests/unit/test_departure.cpp). In flight she keeps
all but sin^2 of her sideslip of it, 0.992 at 5 degrees. side_force.py says
why.
"""

import re
import sys

import alpha_rate
import side_force
import airliner
import fighter
import ground
import make_f15c
from airliner import OUT, PINNED

SCRIPT = "make_f22"
MODEL = "f22"
ENGINE = "F119-PW-100"
EMPTY_LBS = 43340
TANK_LBS = 9000              # 18,000 lb in two tanks
MAXIMUM_THRUST = "35000.0"
# The F119's thrust with speed and height (fighter.thrust_lapse): the F100's
# rise in cold air and loss above the tropopause, and the F119's own throttle
# ratios, past which its thrust falls as the F100's does.
MILITARY_THROTTLE_RATIO = 1.175
MAXIMUM_THROTTLE_RATIO = 1.19
# The wing: aspect ratio, 44.49^2 / 840; the sweep of its quarter chord, from
# its leading edge's 42 degrees and trailing edge's -17.
ASPECT = 44.49 ** 2 / 840.0
SWEEP_QUARTER_CHORD = 30.9
# Drag: the model's own at zero lift, subsonic; the wave drag's rise to Mach
# 1.2; and the span efficiency. The lift curve is straight to 45 degrees, where
# the model's lift stops rising and the flow is taken to leave the wing.
SUBSONIC_ZERO_LIFT_DRAG = 0.0082
WAVE_DRAG = 0.032
SPAN_EFFICIENCY = 0.62
SEPARATION_ALPHA = 0.785
# The tail's moment, in lb/sq ft of dynamic pressure times its effectiveness,
# past which the pitch loop's gains are scheduled down.
PITCH_GAIN_PRESSURE = 1000.0
# The pitch loop's ground mode: on her wheels the loop's integrator is
# washed out at this rate, per second, and its gain raised by as much, so
# that it follows the pitch rate she is short of within a tenth of a second
# and holds a stabilator in proportion to it, instead of winding to its stop
# against her nose wheel; off them both fade to the flight law's over
# PITCH_GROUND_BLEND_S.
PITCH_GROUND_WASHOUT_PER_S = 10.0
PITCH_GROUND_BLEND_S = 5.0
# The roll stick's shaping: the roll rate it commands, as a fraction of the
# model's, is the stick's travel times this plus the rest times its square.
ROLL_STICK_CENTRE_GAIN = 0.25

# **The airframe, so that a wheels-up landing meets the runway.** JSBSim gives
# a retracted wheel no force, and this model had nothing else to touch the
# ground with: landed with its wheels up it went straight through it. The
# points are measured from this aeroplane's own visual mesh by
# tools/ground.py, which says how and why the mesh is the source.
MAXIMUM_WEIGHT_LBS = 83500  # the F-22A's published maximum take-off weight


def replace_once(text, pattern, replacement, what):
    return airliner.replace_once(text, pattern, replacement, what, SCRIPT)


def engine():
    text = (PINNED / "engine" / "F119-PW-1.xml").read_text()
    text = replace_once(text, r"(<turbine_engine name=\"F119-PW-100\">)",
                        r"\1\n  <!-- glideslope: JSBSim's F119-PW-1 with the changes listed in\n"
                        r"       tools/make_f22.py, which made it. Do not edit it by hand. -->",
                        "the engine's name")
    text = replace_once(text, r"<maxthrust>\s*37000\.0\s*</maxthrust>",
                        f"<maxthrust>   {MAXIMUM_THRUST} </maxthrust>", "the maximum thrust")
    idle = fighter.table(text, "IdleThrust")
    f15 = make_f15c
    for name, rows in (("IdleThrust", fighter.idle_table(idle)),
                       ("MilThrust", fighter.thrust_table(f15.MILITARY_EXPONENT, MILITARY_THROTTLE_RATIO,
                                                          f15.MILITARY_FALL, f15.HIGH_LOSS, idle)),
                       ("AugThrust", fighter.thrust_table(f15.MAXIMUM_EXPONENT, MAXIMUM_THROTTLE_RATIO,
                                                          f15.MAXIMUM_FALL, f15.HIGH_LOSS))):
        text = replace_once(
            text, r'(<function name="' + name + r'">\s*<table>.*?<tableData>\s*\n).*?(\n\s*</tableData>)',
            lambda m: m.group(1) + rows + m.group(2), f"the {name} table")
    return text


def airframe():
    text = (PINNED / "aircraft" / "f22" / "f22.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n        <!-- glideslope: this is JSBSim's f22 with the changes listed in\n"
        r"             tools/make_f22.py, which made it. Do not edit it by hand. -->",
        "the file header")
    airliner.without_sockets(text, SCRIPT)
    for actuator in ("aileron-act", "rudder-act", "elevator-act", "tvc-act"):
        text = replace_once(
            text, r"(<actuator name=\"fcs/" + actuator + r"\">(?:(?!</actuator>).)*?)\n\s*<lag>\s*0(?:\.0+)?\s*</lag>",
            r"\1", f"the {actuator} lag")
    text = replace_once(text, r"<emptywt unit=\"LBS\">\s*43430\s*</emptywt>",
                        f"<emptywt unit=\"LBS\"> {EMPTY_LBS} </emptywt>", "the empty weight")
    text = replace_once(
        text, r"(        </pointmass>\n)(    </mass_balance>)",
        r'\1        <pointmass name="Stores">' "\n"
        r'            <weight unit="LBS"> 0 </weight>' "\n"
        r'            <location unit="IN">' "\n"
        r'                <x> 445.7 </x>' "\n"
        r'                <y> 0 </y>' "\n"
        r'                <z> -18.6 </z>' "\n"
        r'            </location>' "\n"
        r'        </pointmass>' "\n" r"\2", "the mass balance")
    for tag in ("capacity", "contents"):
        text, n = re.subn(rf"(<{tag} unit=\"LBS\">)\s*9350\s*(</{tag}>)", rf"\g<1> {TANK_LBS} \2", text)
        if n != 2:
            raise SystemExit(f"{SCRIPT}: found {n} tank {tag}s, not 2 - has the pinned model changed?")
    text, n = re.subn(r"<engine file=\"F119-PW-1\">", f'<engine file="{ENGINE}">', text)
    if n != 2:
        raise SystemExit(f"{SCRIPT}: found {n} engines, not 2 - has the pinned model changed?")

    # The airframe's own contacts, so that the aeroplane has something to
    # land on with its wheels up. See MAXIMUM_WEIGHT_LBS above.
    points = ground.contacts(MODEL, MAXIMUM_WEIGHT_LBS)
    text = replace_once(text, r"(\n)(    </ground_reactions>)",
                        "\n\n" + points.replace("\\", "\\\\") + r"\2",
                        "the airframe's contacts")

    text = with_mach_lift(text)
    text = with_mach_tail(text)
    text = with_pitch_gain_schedule(text)
    text = with_pitch_ground_mode(text)
    text, n = re.subn(r"(<pure_gain name=\"fcs/throttle[12]\">\s*<input>fcs/t[12]-summer</input>\s*<gain>)1\.001(</gain>)",
                      r"\g<1>1\2", text)
    if n != 2:
        raise SystemExit(f"{SCRIPT}: found {n} throttle gains, not 2 - has the pinned model changed?")
    text = with_trimmable_pitch_and_power(text)
    text = replace_once(
        text,
        r"(            <!-- Stick filter -->\s*<lag_filter name=\"fcs/roll-cmd-filter\">\s*<input>)fcs/aileron-cmd-limiter(</input>)",
        lambda m: '            <fcs_function name="fcs/roll-stick-shaped">\n'
        "                <!-- glideslope: tools/make_f22.py -->\n"
        "                <function>\n"
        "                    <product>\n"
        "                        <property>fcs/aileron-cmd-limiter</property>\n"
        "                        <sum>\n"
        f"                            <value>{ROLL_STICK_CENTRE_GAIN}</value>\n"
        "                            <product>\n"
        f"                                <value>{1.0 - ROLL_STICK_CENTRE_GAIN}</value>\n"
        "                                <pow><property>fcs/aileron-cmd-limiter</property><value>2</value></pow>\n"
        "                            </product>\n"
        "                        </sum>\n"
        "                    </product>\n"
        "                </function>\n"
        "            </fcs_function>\n" + m.group(1) + "fcs/roll-stick-shaped" + m.group(2),
        "the roll stick")
    text = replace_once(
        text,
        r"            <function name=\"aero/coefficient/CD0\">.*?<function name=\"aero/coefficient/CDmach\">.*?</function>\n",
        lambda m: fighter.drag_functions(SUBSONIC_ZERO_LIFT_DRAG, WAVE_DRAG, make_f15c.WAVE_DECAY, ASPECT,
                                         SPAN_EFFICIENCY, make_f15c.SUPERSONIC_LIFT_DRAG, SEPARATION_ALPHA),
        "the drag at zero lift, induced drag and drag rise")
    # The tail strike, measured from her visual model by tools/ground.py.
    return ground.with_tail_strike(text, MODEL, MAXIMUM_WEIGHT_LBS, script=SCRIPT)


HELD_TRIM = "fcs/pitch-trim-held-norm"  # glideslope's held trim, after the pitch loop
LEF_LAG = 7.25  # the leading edge flaps' filter's pole, per second, the model's


def with_trimmable_pitch_and_power(text):
    """A held trim on the elevators' command after the pitch loop, and the
    throttle lags and the flaps' filter actuators, so that a trim moves them:
    see "The held trim", "The throttle lags" and "The leading edge flaps'
    filter" above."""
    text = replace_once(
        text, r"(    <flight_control name=\"F-22 FC\">\n)",
        lambda m: m.group(1) + "        <!-- glideslope: tools/make_f22.py - the held trim -->\n"
        f'        <property value="0">{HELD_TRIM}</property>\n', "the flight controls")
    text = replace_once(
        text,
        r"(            <actuator name=\"fcs/elevator-act\">\s*<input>)\s*fcs/el-reg-scale\s*(</input>)",
        lambda m: '            <summer name="fcs/elevator-trimmed">\n'
        "                <!-- glideslope: tools/make_f22.py - the held trim -->\n"
        "                <input>fcs/el-reg-scale</input>\n"
        f"                <input>{HELD_TRIM}</input>\n"
        "            </summer>\n\n" + m.group(1) + "fcs/elevator-trimmed" + m.group(2),
        "the elevators' actuator")
    text, n = re.subn(
        r"<lag_filter name=\"(fcs/throttle1?-filter)\">(\s*<input>fcs/throttle-cmd-norm(?:\[1\])?</input>\s*)"
        r"<c1>1</c1>(\s*)</lag_filter>",
        r'<actuator name="\1">\2<lag>1</lag>\3</actuator>', text)
    if n != 2:
        raise SystemExit(f"{SCRIPT}: found {n} throttle filters, not 2 - has the pinned model changed?")
    # (2s + 7.25) / (s + 7.25) is 2 - 7.25 / (s + 7.25): twice the input less
    # its lag, which the bilinear transform JSBSim uses for both keeps the same.
    text = replace_once(
        text,
        r"            <lead_lag_filter name=\"fcs/lef-alpha-filter\">\s*<input>fcs/lef-alpha-gain</input>\s*"
        r"<c1> 2 </c1>\s*<c2> 7\.25 </c2>\s*<c3> 1 </c3>\s*<c4> 7\.25 </c4>\s*</lead_lag_filter>",
        lambda m: '            <actuator name="fcs/lef-alpha-lag">\n'
        "                <!-- glideslope: tools/make_f22.py - the filter, trimmable -->\n"
        "                <input>fcs/lef-alpha-gain</input>\n"
        f"                <lag>{LEF_LAG}</lag>\n"
        "            </actuator>\n"
        '            <fcs_function name="fcs/lef-alpha-filter">\n'
        "                <function>\n"
        "                    <difference>\n"
        "                        <product><value>2</value><property>fcs/lef-alpha-gain</property></product>\n"
        "                        <property>fcs/lef-alpha-lag</property>\n"
        "                    </difference>\n"
        "                </function>\n"
        "            </fcs_function>",
        "the leading edge flaps' filter")
    return text


def with_mach_lift(text):
    """The model's lift curve, a table of alpha, made a table of alpha and
    Mach: at each Mach in proportion to the lift slope (fighter.lift_slope),
    the model's own at Mach 0.5."""
    m = re.search(r"(<description>Lift_due_to_alpha</description>.*?)<table>\s*"
                  r"<independentVar>aero/alpha-rad</independentVar>\s*<tableData>\s*\n(.*?)\n\s*</tableData>\s*</table>",
                  text, re.S)
    if not m:
        raise SystemExit(f"{SCRIPT}: no lift table - has the pinned model changed?")
    base = fighter.lift_slope(0.5, ASPECT, SWEEP_QUARTER_CHORD)
    scale = [fighter.lift_slope(mach, ASPECT, SWEEP_QUARTER_CHORD) / base for mach in fighter.MACHS]
    pad = "                             "
    lines = [pad + "        \t" + "\t".join(f"{mach:.2f}" for mach in fighter.MACHS)]
    for row in m.group(2).strip().split("\n"):
        alpha, lift = (float(v) for v in row.split())
        lines.append(pad + f"{alpha:.4f}\t" + "\t".join(f"{lift * k:.4f}" for k in scale))
    table = ("<table>\n"
             "                             <independentVar lookup=\"row\">aero/alpha-rad</independentVar>\n"
             "                             <independentVar lookup=\"column\">velocities/mach</independentVar>\n"
             "                             <tableData>\n" + "\n".join(lines) + "\n"
             "                             </tableData>\n"
             "                        </table>")
    return text[:m.start()] + m.group(1) + table + text[m.end():]


def with_mach_tail(text):
    """The tail's pitching moment, which did not change with Mach, carried
    across the Mach range in proportion to the lift slope, as the wing's lift
    is: the tail is a wing too."""
    ratio = tail_ratio_rows("                               ")
    return replace_once(
        text,
        r"(<description>Pitch_moment_due_to_horizontal_tail_deflection</description>\s*<product>\s*)",
        lambda m: m.group(1) + "<table>\n"
        "                           <independentVar>velocities/mach</independentVar>\n"
        "                           <tableData>\n" + ratio + "\n"
        "                           </tableData>\n"
        "                         </table>\n                       ", "the tail's pitching moment")


def tail_ratio_rows(indent):
    return "\n".join(f"{indent}{mach:.2f}\t"
                     f"{fighter.lift_slope(mach, ASPECT, SWEEP_QUARTER_CHORD) / fighter.lift_slope(0.5, ASPECT, SWEEP_QUARTER_CHORD):.4f}"
                     for mach in fighter.MACHS)


def with_pitch_gain_schedule(text):
    """The pitch loop's gains, scheduled on calibrated airspeed alone, made
    smaller where the tail's moment - the dynamic pressure times its
    effectiveness - passes PITCH_GAIN_PRESSURE, in inverse proportion."""
    schedule = (
        '            <fcs_function name="fcs/pitch-gain-schedule">\n'
        "                <!-- glideslope: tools/make_f22.py -->\n"
        "                <function>\n"
        "                    <min>\n"
        "                        <value>1</value>\n"
        "                        <quotient>\n"
        f"                            <value>{PITCH_GAIN_PRESSURE:.1f}</value>\n"
        "                            <product>\n"
        "                                <property>aero/qbar-psf</property>\n"
        "                                <table>\n"
        "                                    <independentVar>velocities/mach</independentVar>\n"
        "                                    <tableData>\n" + tail_ratio_rows("                                        ") + "\n"
        "                                    </tableData>\n"
        "                                </table>\n"
        "                            </product>\n"
        "                        </quotient>\n"
        "                    </min>\n"
        "                </function>\n"
        "            </fcs_function>\n")
    return replace_once(
        text,
        r"(            <!-- LQR Tracker Integral elevator pitch rate controller-->\s*"
        r"<fcs_function name=\"fcs/el-pitch-cmd\">\s*<function>\s*)<sum>(.*?)</sum>(\s*</function>)",
        lambda m: schedule + m.group(1) + "<product>\n                    <property>fcs/pitch-gain-schedule</property>\n"
        "                    <sum>" + m.group(2) + "</sum>\n                    </product>" + m.group(3),
        "the pitch loop's gains")


def with_pitch_ground_mode(text):
    """The pitch loop's ground mode: its integrator washed out on her wheels,
    so that her stick commands a pitch rate through a proportional loop
    there, and the washout faded out over PITCH_GROUND_BLEND_S once she is
    off them. The integrator's state is the stabilator's command in both
    laws, so the flight law takes over from where the ground law left it."""
    mode = (
        '            <!-- glideslope: tools/make_f22.py - the ground mode -->\n'
        '            <switch name="fcs/pitch-ground-wow">\n'
        '                <default value="0"/>\n'
        '                <test value="1">\n'
        '                    gear/wow eq 1\n'
        '                </test>\n'
        '            </switch>\n'
        '\n'
        '            <actuator name="fcs/pitch-ground-mode">\n'
        '                <input>fcs/pitch-ground-wow</input>\n'
        f'                <rate_limit sense="decr"> {1.0 / PITCH_GROUND_BLEND_S:g} </rate_limit>\n'
        '            </actuator>\n'
        '\n'
        '            <fcs_function name="fcs/pitch-ground-washout">\n'
        '                <function>\n'
        '                    <product>\n'
        '                        <property>fcs/pitch-ground-mode</property>\n'
        '                        <property>fcs/pitch-rate-integrator</property>\n'
        '                    </product>\n'
        '                </function>\n'
        '            </fcs_function>\n'
        '\n'
        '            <fcs_function name="fcs/pitch-integrator-gain">\n'
        '                <function>\n'
        '                    <sum>\n'
        '                        <value>1</value>\n'
        '                        <product>\n'
        f'                            <value>{PITCH_GROUND_WASHOUT_PER_S - 1.0:g}</value>\n'
        '                            <property>fcs/pitch-ground-mode</property>\n'
        '                        </product>\n'
        '                    </sum>\n'
        '                </function>\n'
        '            </fcs_function>\n'
        '\n')
    text = replace_once(text, r"(            <deadband name=\"fcs/pitch-windup-trigger\">)",
                        lambda m: mode + m.group(1), "the pitch loop's windup trigger")
    text = replace_once(
        text,
        r"(<summer name=\"fcs/pitch-rate-error\">\s*<input>fcs/q-gear-down-limiter</input>\s*"
        r"<input>-velocities/q-aero-rad_sec</input>\s*<input>-fcs/pitch-windup-trigger</input>\n)",
        lambda m: m.group(1) + "                <input>-fcs/pitch-ground-washout</input>\n",
        "the pitch loop's rate error")
    return replace_once(
        text,
        r"(<integrator name=\"fcs/pitch-rate-integrator\">\s*<!--trigger>fcs/pitch-int-trigger</trigger-->\s*"
        r"<input>fcs/pitch-rate-error</input>\s*<c1>) 1 (</c1>)",
        r"\1fcs/pitch-integrator-gain\2", "the pitch loop's integrator's gain")


def with_cm_alpha_split_out(text):
    """`text` with CmDh's column at no deflection - her Cm-alpha, for she has
    no other - its own function, CmDh_alpha, and CmDh the deflection's
    increment over it: the two add to CmDh as it was, term for term, so on
    the same dynamic pressure nothing moves. The docstring says why."""
    m = re.search(r"( *)<function name=\"aero/coefficient/CmDh\">.*?</function>\n", text, re.S)
    if not m:
        raise SystemExit(f"{SCRIPT}: no CmDh - has the pinned model changed?")
    block, indent = m.group(0), m.group(1)
    tables = re.findall(r"<table>.*?</table>", block, re.S)
    if len(tables) != 2 or "lookup=\"column\">fcs/elevator-pos-rad" not in tables[1]:
        raise SystemExit(f"{SCRIPT}: CmDh is not a Mach table and an alpha-by-deflection table - "
                         f"has the pinned model changed?")
    # Nothing else in it, so that a factor added to the pinned model is not
    # dropped from the two functions this writes in its place.
    properties = re.findall(r"<property>\s*([^<]*?)\s*</property>", block)
    values = re.findall(r"<value>\s*([^<]*?)\s*</value>", block)
    rest = re.sub(r"<table>.*?</table>|<property>.*?</property>|<value>.*?</value>|"
                  r"<description>.*?</description>|</?product>|</?function[^>]*>", "", block,
                  flags=re.S)
    if (properties != ["aero/qbar-psf", "metrics/Sw-sqft", "metrics/cbarw-ft"]
            or values != ["1.5"] or rest.strip()):
        raise SystemExit(f"{SCRIPT}: CmDh is not qbar, Sw, cbarw, its two tables and 1.5 - "
                         f"has the pinned model changed? (properties {properties}, values "
                         f"{values}, else {rest.strip()[:80]!r})")
    mach, table = tables
    rows = [r.split() for r in re.search(r"<tableData>(.*?)</tableData>", table, re.S)
            .group(1).strip().splitlines()]
    if [float(x) for x in rows[0]] != [-0.5236, 0.0, 0.5236]:
        raise SystemExit(f"{SCRIPT}: CmDh's deflections are not -0.5236, 0 and 0.5236 - "
                         f"has the pinned model changed?")
    column = "".join(f"                               {r[0]}  {r[2]}\n" for r in rows[1:])
    alpha_table = ("<table>\n"
                   "                           <independentVar>aero/alpha-rad</independentVar>\n"
                   "                           <tableData>\n" + column +
                   "                           </tableData>\n"
                   "                         </table>")
    common = (f"                       {mach}\n"
              "                       <property>aero/qbar-psf</property>\n"
              "                       <property>metrics/Sw-sqft</property>\n"
              "                       <property>metrics/cbarw-ft</property>\n")
    split = (
        f'{indent}<function name="aero/coefficient/CmDh_alpha">\n'
        "                   <description>Pitch_moment_due_to_alpha: CmDh's column at no "
        "deflection (glideslope)</description>\n"
        "                   <product>\n" + common +
        f"                       {alpha_table}\n"
        "            <value>1.5</value>\n"
        "                   </product>\n"
        "               </function>\n"
        f'{indent}<function name="aero/coefficient/CmDh">\n'
        "                   <description>Pitch_moment_due_to_horizontal_tail_deflection: "
        "over CmDh_alpha (glideslope)</description>\n"
        "                   <product>\n" + common +
        "                       <difference>\n"
        f"                         {table}\n"
        f"                         {alpha_table}\n"
        "                       </difference>\n"
        "            <value>1.5</value>\n"
        "                   </product>\n"
        "               </function>\n")
    return text[:m.start()] + split + text[m.end():]


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": side_force.without_crossflow(alpha_rate.with_unkeyed_terms_on_qbar_uw(
            alpha_rate.with_alpha_terms_on_qbar_uw(
            with_cm_alpha_split_out(alpha_rate.with_qbar_uw(airframe(), "make_f22")), "make_f22",
            {"PITCH": ["aero/coefficient/Cma_M", "aero/coefficient/CmDh_alpha",
                       "aero/coefficient/CmDh", "aero/coefficient/CmDtv",
                       "aero/coefficient/Cmq"],
             "LIFT": ["aero/coefficient/CLalpha", "aero/coefficient/CLDlef", "aero/coefficient/CLq"],
             "DRAG": ["aero/coefficient/CDseparated", "aero/coefficient/CDDlef"]}), "make_f22",
            {"LIFT": ["aero/coefficient/dCLflap",
             "aero/coefficient/dCLsb",
             "aero/coefficient/CLde"]}), "make_f22"),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
