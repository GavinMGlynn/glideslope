"""airliner.py - what the airliners' tuning scripts share.

tools/make_737_300.py, make_a320.py, make_747_400.py and make_787_8.py each
make one of glideslope's airliners from JSBSim's model, and each changes the
same few things the same way: the drag rise past the critical Mach, the
engines' thrust with height and speed, and a stopped engine's windmilling
drag. The physics of each, and where it comes from, is here once.
"""

import math
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PINNED = ROOT / "ext" / "jsbsim"
OUT = ROOT / "assets" / "jsbsim"


def replace_once(text, pattern, replacement, what, script):
    new, n = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f"{script}: could not find {what} - has the pinned model changed?")
    return new


def without_sockets(text, script):
    """The model's text, checked to open no network socket once its comments
    are set aside."""
    if re.search(r"<input\s+port=|type=\"SOCKET\"", re.sub(r"<!--.*?-->", "", text, flags=re.S)):
        raise SystemExit(f"{script}: the model opens a network socket")
    return text


def mach_drag_rows(divergence, indent="                              "):
    """Lock's drag rise: nothing to the critical Mach, 20 (M - Mcrit)^4 past
    it, the critical Mach 0.108 below the drag divergence Mach - where the
    rise's slope reaches 0.1."""
    critical = divergence - (0.1 / 80.0) ** (1.0 / 3.0)
    rows = [(0.0, 0.0), (round(critical, 3), 0.0)]
    mach = round(critical, 2) + 0.01
    while mach <= 1.0001:
        rows.append((round(mach, 2), 20.0 * (mach - critical) ** 4))
        mach += 0.01
    return "\n".join(f"{indent}{m:.3f}\t{d:.5f}" for m, d in rows)


def delta(altitude_ft):
    """The standard atmosphere's pressure ratio."""
    if altitude_ft < 36089.0:
        return (1.0 - 6.8756e-6 * altitude_ft) ** 5.2559
    return 0.22336 * math.exp(-4.80634e-5 * (altitude_ft - 36089.0))


def mattingly(mach, altitude_ft):
    """Mattingly's maximum thrust of a high-bypass turbofan, as a fraction of
    its sea-level static thrust: delta0 (1 - 0.49 sqrt(M)) (Mattingly, Heiser
    and Pratt, Aircraft Engine Design, 2002, for a throttle ratio of 1)."""
    return delta(altitude_ft) * (1.0 + 0.2 * mach * mach) ** 3.5 * (1.0 - 0.49 * math.sqrt(mach))


def with_mattingly_thrust(engine_text, script):
    """The engine's MilThrust table, its rows and columns kept, refilled with
    Mattingly's lapse (nothing past Mach 1)."""
    m = re.search(r'(<function name="MilThrust">\s*<table>.*?<tableData>\s*\n)(.*?)(\n\s*</tableData>)',
                  engine_text, re.S)
    if not m:
        raise SystemExit(f"{script}: no MilThrust table - has the pinned engine changed?")
    head, *rows = m.group(2).split("\n")
    altitudes = [float(a) for a in head.split()]
    lapsed = [head]
    for row in rows:
        mach = float(row.split()[0])
        values = [mattingly(mach, a) if mach <= 1.0 else 0.0 for a in altitudes]
        lapsed.append(f"     {mach:.1f}  " + "  ".join(f"{v:.4f}" for v in values))
    return engine_text[:m.start(2)] + "\n".join(lapsed) + engine_text[m.end(2):]


def windmill_function(engines, fan_diameter_in, coefficient, indent="            "):
    """A drag function for the engines that are not running: each one's fan
    windmills, with a drag area of `coefficient` times its frontal area.
    JSBSim's turbine gives a stopped engine no thrust and no drag."""
    area = math.pi * (fan_diameter_in / 24.0) ** 2
    lines = [f'<function name="aero/coefficient/CDwindmill">',
             "    <description>Drag_of_windmilling_engines</description>",
             "    <product>",
             "        <property>aero/qbar-psf</property>",
             f"        <value>{area * coefficient:.3f}</value>",
             "        <sum>"]
    lines += [f"            <difference><value>1</value>"
              f"<property>propulsion/engine[{i}]/set-running</property></difference>" for i in range(engines)]
    lines += ["        </sum>", "    </product>", "</function>"]
    return "\n".join(indent + line for line in lines)


def main(outputs, script):
    """Writes each of `outputs`, {path: text}, or with --check exits 1 if what
    is committed differs."""
    check = "--check" in sys.argv[1:]
    stale = []
    for path, text in outputs.items():
        if check:
            if not path.exists() or path.read_text() != text:
                stale.append(path.relative_to(ROOT))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            print(f"wrote {path.relative_to(ROOT)}")
    if stale:
        print(f"assets/jsbsim/ is not what tools/{script}.py makes:", file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print(f"run: python3 tools/{script}.py", file=sys.stderr)
        return 1
    return 0


# **Thrust reversers.** Every airliner here reverses only her fan's air: the
# CFM56-3 (737-300), CF6-80C2 (747-400) and Trent family (787-8, A380) by a
# translating sleeve over cascades, the A320's CFM56-5B by pivoting doors -
# "In the case of sliding sleeve and pivoting blocker door types of thrust
# reverser, only the direction of the bypassed air is reversed, while the
# direction of core airflow is unchanged" (Aircraft Commerce no. 92, 2014,
# "Thrust reverser repair process & economics", p. 48; docs/ASSETS.md). The
# A380's are on her inboard engines only (her Aircraft Characteristics,
# 2-12-0: "It is only installed on the aircraft inboard position nacelles").
#
# **How much they reverse**: a cascade reverser's overall effectiveness - its
# reverse force over the engine's forward thrust at the same setting, the core
# still pushing forward - is "about 0.31, which is typical of cascade thrust
# reversers" (NASA TM-2000-210300, Asbury and Yetter, 2000, p. 21, the
# baseline cascade at a bypass ratio of 9). The pivoting doors are given the
# same: no source to hand gives theirs, and they too turn the fan's air alone.
# JSBSim's direct thruster scales its thrust by the cosine of its reverser
# angle, so the sleeve's full travel is the angle whose cosine is -0.31.
REVERSER_EFFECTIVENESS = 0.31
# **The sleeves' travel, stowed to deployed**: two seconds, this project's
# estimate - no document to hand gives a type's. With the engines' own spool
# from idle to full throttle it puts each airliner at nine tenths of full
# reverse 7.2 to 7.8 seconds after the lever (PROJECT_STATUS, 2026-10-14),
# against the A340's five on the TSB's flight data (A05H0002, 1.16: "the N1
# parameters of the four engines stabilized at maximum reverse approximately
# five seconds later"): a real engine's maximum reverse is a lower N1 than
# its full throttle, and is reached sooner.
REVERSER_TRAVEL_S = 2.0
# **The reverse lever**: 0 stowed; anything more deploys the reversers, with the
# weight on the wheels (the air-ground interlock every type has); 0.1 is the
# idle reverse detent; from there to 1, idle to maximum reverse power. Its
# power is held at idle until the sleeves are fully out - "thrust reverser
# interlocks that prevent the pilot from applying reverse thrust until the
# reverser is deployed" (AC 25-32, 10.1.4) - and while it is out of stowed the
# forward throttles of the engines that reverse are idle, as the levers'
# interlock holds them.
REVERSE_IDLE_DETENT = 0.1


def reverser_channel(engines, indent="        "):
    """The reverse lever, `fcs/reverse-cmd-norm`, and a flight control
    channel deploying the thrust reversers of `engines`
    (JSBSim's indices) on it and
    driving their engines' power from it while they are out."""
    i = indent
    angle = math.acos(-REVERSER_EFFECTIVENESS)
    out = (f"{i}<!-- glideslope: the reverse lever, stowed. -->\n"
           f"{i}<property value=\"0\">fcs/reverse-cmd-norm</property>\n"
           f"{i}<channel name=\"Thrust Reversers\">\n"
           f"{i}    <!-- glideslope: tools/airliner.py, reverser_channel. The reverse lever\n"
           f"{i}         fcs/reverse-cmd-norm: 0 stowed, {REVERSE_IDLE_DETENT} idle reverse, 1 maximum. -->\n"
           f"{i}    <switch name=\"fcs/reverser-cmd-norm\">\n"
           f"{i}        <default value=\"0\"/>\n"
           f"{i}        <test logic=\"AND\" value=\"1\">\n"
           f"{i}            fcs/reverse-cmd-norm gt 0\n"
           f"{i}            gear/wow eq 1\n"
           f"{i}        </test>\n"
           f"{i}    </switch>\n"
           f"{i}    <kinematic name=\"fcs/reverser-pos-norm\">\n"
           f"{i}        <input>fcs/reverser-cmd-norm</input>\n"
           f"{i}        <traverse>\n"
           f"{i}            <setting><position>0</position><time>0</time></setting>\n"
           f"{i}            <setting><position>1</position><time>{REVERSER_TRAVEL_S:g}</time></setting>\n"
           f"{i}        </traverse>\n"
           f"{i}    </kinematic>\n"
           f"{i}    <fcs_function name=\"fcs/reverser-angle-rad\">\n"
           f"{i}        <function>\n"
           f"{i}            <product>\n"
           f"{i}                <property>fcs/reverser-pos-norm</property>\n"
           f"{i}                <value>{angle:.4f}</value>\n"
           f"{i}            </product>\n"
           f"{i}        </function>\n")
    for e in engines:
        out += f"{i}        <output>propulsion/engine[{e}]/reverser-angle-rad</output>\n"
    out += (f"{i}    </fcs_function>\n"
            f"{i}    <switch name=\"fcs/reverser-unlocked\">\n"
            f"{i}        <default value=\"0\"/>\n"
            f"{i}        <test value=\"1\">\n"
            f"{i}            fcs/reverser-pos-norm ge 0.999\n"
            f"{i}        </test>\n"
            f"{i}    </switch>\n"
            f"{i}    <fcs_function name=\"fcs/reverse-power-norm\">\n"
            f"{i}        <function>\n"
            f"{i}            <product>\n"
            f"{i}                <property>fcs/reverser-unlocked</property>\n"
            f"{i}                <max>\n"
            f"{i}                    <value>0</value>\n"
            f"{i}                    <quotient>\n"
            f"{i}                        <difference>\n"
            f"{i}                            <min>\n"
            f"{i}                                <property>fcs/reverse-cmd-norm</property>\n"
            f"{i}                                <value>1</value>\n"
            f"{i}                            </min>\n"
            f"{i}                            <value>{REVERSE_IDLE_DETENT:g}</value>\n"
            f"{i}                        </difference>\n"
            f"{i}                        <value>{1.0 - REVERSE_IDLE_DETENT:g}</value>\n"
            f"{i}                    </quotient>\n"
            f"{i}                </max>\n"
            f"{i}            </product>\n"
            f"{i}        </function>\n"
            f"{i}    </fcs_function>\n")
    for e in engines:
        out += (f"{i}    <switch name=\"fcs/reverse-throttle-{e}\">\n"
                f"{i}        <default value=\"fcs/throttle-cmd-norm[{e}]\"/>\n"
                f"{i}        <test value=\"fcs/reverse-power-norm\">\n"
                f"{i}            fcs/reverser-cmd-norm gt 0\n"
                f"{i}        </test>\n"
                f"{i}        <output>fcs/throttle-pos-norm[{e}]</output>\n"
                f"{i}    </switch>\n")
    return out + f"{i}</channel>\n"


def with_reversers(text, engines, script):
    """The airframe `text` with thrust reversers on `engines`, their channel
    the flight controls' last."""
    return replace_once(text, r"(\n)([ \t]*</flight_control>)",
                        lambda mm: mm.group(1) + reverser_channel(engines) + mm.group(2),
                        "the end of the flight controls", script)
