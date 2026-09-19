"""fighter.py - what the fighters' tuning scripts share.

tools/make_f15c.py and make_f22.py each make one of glideslope's fighters
from JSBSim's model, and both models' aerodynamics stopped at the speed of
sound: the F-15's lift slope fell to a quarter at Mach 1.4 and its drag at
zero lift halved, and the F-22's lift did not change with Mach at all. Both
are given here the textbook's lift and drag across the Mach range, and the
thrust of an afterburning low-bypass turbofan, with a few numbers each -
set in its script - that its published figures pin.
"""

import math
import re

# The Mach numbers the tables are made at, and the heights the engines' are.
MACHS = [0.0, 0.2, 0.4, 0.5, 0.6, 0.7, 0.8, 0.85, 0.9, 0.95, 1.0, 1.05, 1.1, 1.2, 1.3, 1.4, 1.6, 1.8,
         2.0, 2.2, 2.4, 2.6]
ENGINE_MACHS = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.2, 2.4, 2.6]
ENGINE_ALTITUDES = [-10000, 0, 10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000]


def lift_slope(mach, aspect, sweep_quarter_chord_deg):
    """A wing's lift slope, a radian: DATCOM's subsonic formula to Mach 0.9,
    supersonic linear theory, 4 / sqrt(M^2 - 1) less the tips' loss, from Mach
    1.4, and a straight line between."""
    def subsonic(m):
        beta2 = 1.0 - m * m
        tan2 = math.tan(math.radians(sweep_quarter_chord_deg)) ** 2
        return 2.0 * math.pi * aspect / (
            2.0 + math.sqrt(4.0 + aspect ** 2 * beta2 / 0.95 ** 2 * (1.0 + tan2 / beta2)))

    def supersonic(m):
        root = math.sqrt(m * m - 1.0)
        return 4.0 / root * (1.0 - 1.0 / (2.0 * aspect * root))

    m = max(mach, 0.01)
    if m <= 0.9:
        return subsonic(m)
    if m >= 1.4:
        return supersonic(m)
    return subsonic(0.9) + (supersonic(1.4) - subsonic(0.9)) * (m - 0.9) / 0.5


def zero_lift_drag(mach, subsonic, wave, decay=0.5):
    """Drag at zero lift: `subsonic` to Mach 0.85, rising smoothly by `wave` to
    Mach 1.2, and past it falling as 1.2 over the Mach to the power `decay`."""
    if mach <= 0.85:
        return subsonic
    if mach <= 1.2:
        x = (mach - 0.85) / 0.35
        return subsonic + wave * x * x * (3.0 - 2.0 * x)
    return subsonic + wave * (1.2 / mach) ** decay


def induced_drag_factor(mach, aspect, efficiency, supersonic_factor):
    """K in CD = K CL^2: 1 / (pi A e) below Mach 0.9; from Mach 1.2
    `supersonic_factor` over the supersonic lift slope - the drag of a lift
    with the leading edge's suction lost, A (M^2 - 1) / (4 A sqrt(M^2 - 1) - 2)
    (Raymer, who takes the factor as the cosine of the leading edge's sweep);
    and a straight line between."""
    subsonic = 1.0 / (math.pi * aspect * efficiency)

    def supersonic(m):
        root = math.sqrt(m * m - 1.0)
        return supersonic_factor * aspect * (m * m - 1.0) / (4.0 * aspect * root - 2.0)

    if mach <= 0.9:
        return subsonic
    if mach >= 1.2:
        return max(subsonic, supersonic(mach))
    return subsonic + (max(subsonic, supersonic(1.2)) - subsonic) * (mach - 0.9) / 0.3


def isa(altitude_ft):
    """The standard atmosphere's temperature and pressure ratios."""
    if altitude_ft < 36089.0:
        theta = 1.0 - 6.8756e-6 * altitude_ft
        return theta, theta ** 5.2559
    return 0.75189, 0.22336 * math.exp(-4.80634e-5 * (altitude_ft - 36089.0))


def thrust_lapse(mach, altitude_ft, exponent, throttle_ratio, fall, high_loss=0.0):
    """An afterburning low-bypass turbofan's thrust, as a fraction of its
    sea-level static thrust at the same setting: Mattingly's form (Mattingly,
    Heiser and Pratt, Aircraft Engine Design, 2002) - delta0, the ram
    pressure ratio, while theta0, the ram temperature ratio, is within the
    throttle ratio, and past it less by `fall` times theta0's excess over
    theta0 - with the thrust in cold air rising as theta0 to the power
    -`exponent`, where Mattingly's typical engine holds it (exponent 0); and
    above the tropopause, where the air is thin enough that the engine's
    Reynolds number costs it, `high_loss` less for each 10,000 ft."""
    theta, delta = isa(altitude_ft)
    ram = 1.0 + 0.2 * mach * mach
    theta0, delta0 = theta * ram, delta * ram ** 3.5
    thrust = delta0 * theta0 ** -exponent
    if theta0 > throttle_ratio:
        thrust *= max(0.0, 1.0 - fall * (theta0 - throttle_ratio) / theta0)
    return thrust * (1.0 - high_loss * max(0.0, altitude_ft - 36089.0) / 10000.0)


def table(engine_text, name):
    """An engine's table, `name`, as a function of Mach and altitude that
    interpolates it as JSBSim does - straight lines, held past its edges."""
    m = re.search(r'<function name="' + name + r'">\s*<table>.*?<tableData>\s*\n(.*?)\n\s*</tableData>',
                  engine_text, re.S)
    head, *rows = m.group(1).split("\n")
    columns = [float(a) for a in head.split()]
    machs = [float(row.split()[0]) for row in rows]
    values = [[float(v) for v in row.split()[1:]] for row in rows]

    def between(xs, x):
        x = min(max(x, xs[0]), xs[-1])
        i = max(j for j in range(len(xs) - 1) if xs[j] <= x) if x < xs[-1] else len(xs) - 2
        return i, (x - xs[i]) / (xs[i + 1] - xs[i])

    def at(mach, altitude):
        i, u = between(machs, mach)
        j, w = between(columns, altitude)
        row = lambda k: values[k][j] + (values[k][j + 1] - values[k][j]) * w
        return row(i) + (row(i + 1) - row(i)) * u
    return at


def thrust_table(exponent, throttle_ratio, fall, high_loss=0.0, idle=None, indent="    "):
    """A JSBSim thrust table, Mach by density altitude, of `thrust_lapse`.
    JSBSim's turbine at military power gives the idle thrust and the military
    table's share of what is left, idle + (1 - idle) x table: given the idle
    table, as a function (`table`), the military table is made so that the
    two together are the lapse."""
    def cell(m, a):
        lapse = thrust_lapse(m, a, exponent, throttle_ratio, fall, high_loss)
        if idle is None:
            return lapse
        return (lapse - idle(m, a)) / (1.0 - idle(m, a))
    rows = [indent + "     " + "  ".join(f"{a:>7d}" for a in ENGINE_ALTITUDES)]
    for m in ENGINE_MACHS:
        rows.append(indent + f"{m:4.1f}  " + "  ".join(f"{cell(m, a):7.4f}" for a in ENGINE_ALTITUDES))
    return "\n".join(rows)


def idle_table(idle, indent="    "):
    """The idle table, `idle` (a function, from `table`), on the military
    table's rows and columns, so that JSBSim interpolates both alike."""
    rows = [indent + "     " + "  ".join(f"{a:>7d}" for a in ENGINE_ALTITUDES)]
    for m in ENGINE_MACHS:
        rows.append(indent + f"{m:4.1f}  " + "  ".join(f"{idle(m, a):7.4f}" for a in ENGINE_ALTITUDES))
    return "\n".join(rows)



def mach_table(f, indent):
    """A JSBSim table of f(Mach) at MACHS."""
    rows = "\n".join(f"{indent}          {m:.2f}\t{f(m):.5f}" for m in MACHS)
    return (f"{indent}<table>\n"
            f"{indent}    <independentVar>velocities/mach</independentVar>\n"
            f"{indent}    <tableData>\n{rows}\n"
            f"{indent}    </tableData>\n"
            f"{indent}</table>\n")


def drag_functions(subsonic, wave, decay, aspect, efficiency, supersonic_factor, separation_alpha,
                   indent="            "):
    """JSBSim's drag functions for a fighter: the drag at zero lift by Mach
    (`zero_lift_drag`); the drag due to lift, K by Mach times the lift
    squared (`induced_drag_factor`); and past `separation_alpha`, where the
    flow leaves the wing and the leading edge's suction goes with it, that
    drag giving way over ten degrees to the lift times the tangent of the
    angle of attack - the backward part of a force normal to the wing - held
    past 60 degrees, where the lift tables stop meaning much."""
    i = indent
    zero = lambda m: zero_lift_drag(m, subsonic, wave, decay)
    factor = lambda m: induced_drag_factor(m, aspect, efficiency, supersonic_factor)
    a0, a1 = separation_alpha, separation_alpha + math.radians(10.0)
    blend = "\n".join(f"{i}                  {a:.4f}\t{w:.1f}" for a, w in
                      ((-1.5708, 1.0), (-a1, 1.0), (-a0, 0.0), (a0, 0.0), (a1, 1.0), (1.5708, 1.0)))
    return (
        f'{i}<function name="aero/coefficient/CD0">\n'
        f"{i}    <description>Drag_at_zero_lift</description>\n"
        f"{i}    <product>\n"
        f"{i}        <property>aero/qbar-psf</property>\n"
        f"{i}        <property>metrics/Sw-sqft</property>\n"
        + mach_table(zero, i + "        ") +
        f"{i}    </product>\n"
        f"{i}</function>\n"
        f'{i}<function name="aero/coefficient/CDi">\n'
        f"{i}    <description>Drag_due_to_lift</description>\n"
        f"{i}    <product>\n"
        f"{i}        <property>aero/qbar-psf</property>\n"
        f"{i}        <property>metrics/Sw-sqft</property>\n"
        f"{i}        <property>aero/function/kCDge</property>\n"
        f"{i}        <property>aero/cl-squared</property>\n"
        + mach_table(factor, i + "        ") +
        f"{i}    </product>\n"
        f"{i}</function>\n"
        f'{i}<function name="aero/coefficient/CDseparated">\n'
        f"{i}    <description>Drag_past_the_flow_separating</description>\n"
        f"{i}    <product>\n"
        f"{i}        <property>aero/qbar-psf</property>\n"
        f"{i}        <property>metrics/Sw-sqft</property>\n"
        f"{i}        <table>\n"
        f"{i}            <independentVar>aero/alpha-rad</independentVar>\n"
        f"{i}            <tableData>\n{blend}\n"
        f"{i}            </tableData>\n"
        f"{i}        </table>\n"
        f"{i}        <difference>\n"
        f"{i}            <product>\n"
        f"{i}                <pow><property>aero/cl-squared</property><value>0.5</value></pow>\n"
        f"{i}                <min><abs><tan><property>aero/alpha-rad</property></tan></abs>"
        f"<value>1.732</value></min>\n"
        f"{i}            </product>\n"
        f"{i}            <product>\n"
        f"{i}                <property>aero/cl-squared</property>\n"
        + mach_table(factor, i + "                ") +
        f"{i}            </product>\n"
        f"{i}        </difference>\n"
        f"{i}    </product>\n"
        f"{i}</function>\n")
