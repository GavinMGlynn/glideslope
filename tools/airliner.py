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
