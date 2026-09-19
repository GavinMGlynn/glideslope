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
    The roll stick      The stick commanded a roll rate in proportion to its
                        travel - its first tenth 22 degrees a second - and any
                        pilot's small corrections, a person's or glideslope's
                        autopilot's, overshot and rolled the aircraft over. It
                        is shaped, as fly-by-wire sticks are: a quarter as
                        sensitive at the centre, and full travel the full rate
                        (ROLL_STICK_CENTRE_GAIN).
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
"""

import re
import sys

import airliner
import fighter
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
# The roll stick's shaping: the roll rate it commands, as a fraction of the
# model's, is the stick's travel times this plus the rest times its square.
ROLL_STICK_CENTRE_GAIN = 0.25


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
    text = with_mach_lift(text)
    text = with_mach_tail(text)
    text = with_pitch_gain_schedule(text)
    text, n = re.subn(r"(<pure_gain name=\"fcs/throttle[12]\">\s*<input>fcs/t[12]-summer</input>\s*<gain>)1\.001(</gain>)",
                      r"\g<1>1\2", text)
    if n != 2:
        raise SystemExit(f"{SCRIPT}: found {n} throttle gains, not 2 - has the pinned model changed?")
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
    return replace_once(
        text,
        r"            <function name=\"aero/coefficient/CD0\">.*?<function name=\"aero/coefficient/CDmach\">.*?</function>\n",
        lambda m: fighter.drag_functions(SUBSONIC_ZERO_LIFT_DRAG, WAVE_DRAG, make_f15c.WAVE_DECAY, ASPECT,
                                         SPAN_EFFICIENCY, make_f15c.SUPERSONIC_LIFT_DRAG, SEPARATION_ALPHA),
        "the drag at zero lift, induced drag and drag rise")


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


def outputs():
    return {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "engine" / f"{ENGINE}.xml": engine(),
        OUT / "engine" / "direct.xml": (PINNED / "engine" / "direct.xml").read_text(),
    }


if __name__ == "__main__":
    sys.exit(airliner.main(outputs(), SCRIPT))
