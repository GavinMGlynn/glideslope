"""piston_mixture.py - the light aeroplanes' mixture curve, shared by their make scripts.

JSBSim's piston engine (FGPiston::doEnginePower) makes its power as the fuel
it burns, over a specific fuel consumption, times an efficiency read from its
MIXTURE table by the ratio of fuel to air. Its own table is flat from 0.078 to
0.088 and falls slowly richer, so the power - fuel times efficiency - goes on
rising as the mixture richens past the flat, and peaks at 0.101, 9.9 parts of
air to one of fuel: 6.6% more than full rich gives at sea level (11.3 to 1),
found by leaning high up, which no engine has.

The FAA gives the shape a petrol engine's power has (Aviation Maintenance
Technician Handbook - Powerplant, FAA-H-8083-32, volume 1, chapter 2, page
2-4): best power at "approximately 12 parts of air to 1 part of gasoline",
the power "essentially constant" from 0.0725 to 0.080 fuel/air (13.8 to 12.5
to 1), falling "gradually at first, then more rapidly" richer, and the rich
limit of combustion at 8 to 1 (0.125, also JSBSim's own). Lean of best power
the power falls slowly to the chemically correct 14.7 to 1, where it is a few
percent down, and then fast, to nothing at the lean limit near 20 to 1
(0.050, again JSBSim's own).

`POWER` is that shape, as a fraction of best power. The table written is the
efficiency that gives it, POWER over the fuel/air ratio, scaled to be 1 at
JSBSim's full rich at sea level (fuel/air 0.0884) - where its own table is 1
too - so the engine's rated power, full rich at sea level, is unchanged by
it, and what changes is the power leaned and the power full rich up high.
Best power is a gentle peak at 13.1 to 1 (0.0765) rather than a flat, so the
leaner (src/sim/leaner.hpp), which feels for a peak, settles on one.
"""

import re

# Fuel/air ratio, and the power there as a fraction of best power.
POWER = [
    (0.0500, 0.000),
    (0.0526, 0.350),
    (0.0556, 0.620),
    (0.0588, 0.800),
    (0.0625, 0.900),
    (0.0680, 0.965),
    (0.0725, 0.993),
    (0.0765, 1.000),
    (0.0800, 0.996),
    (0.0884, 0.980),
    (0.1000, 0.940),
    (0.1100, 0.860),
    (0.1176, 0.700),
    (0.1215, 0.400),
    (0.1250, 0.000),
]

# JSBSim's full rich at sea level: 1.3 times the chemically correct 1/14.7.
FULL_RICH_SEA_LEVEL = 1.3 / 14.7


def efficiency_table():
    """The MIXTURE table: (fuel/air, efficiency) rows."""
    scale = FULL_RICH_SEA_LEVEL / dict(POWER)[0.0884]
    return [(ratio, 0.0 if power == 0.0 else scale * power / ratio) for ratio, power in POWER]


def with_best_power_mixture(engine_text, script):
    """`engine_text`, a JSBSim piston engine file, with the MIXTURE table above."""
    rows = "\n".join(f"      {ratio:.4f}  {eff:.4f}" for ratio, eff in efficiency_table())
    table = (f"  <!-- glideslope: the mixture's power shape, from the FAA's\n"
             f"       FAA-H-8083-32; see tools/piston_mixture.py -->\n"
             f"  <table name=\"MIXTURE\" type=\"internal\">\n"
             f"    <tableData>\n{rows}\n    </tableData>\n"
             f"  </table>\n</piston_engine>")
    new, n = re.subn(r"</piston_engine>", table, engine_text, count=1)
    if n != 1 or "name=\"MIXTURE\"" in engine_text:
        raise SystemExit(f"{script}: no piston engine to give a mixture table to, "
                         "or it has one - has the pinned engine changed?")
    return new


# **A float carburettor's metering.** JSBSim meters a piston engine's fuel as
# the mixture lever times the sea-level pressure over the ambient
# (FGPiston::doFuelFlow), which richens the mixture as the pressure ratio,
# delta, falls: at 5,000 ft 20% richer, at 10,000 ft 45%. A float
# carburettor meters fuel and air by the one venturi's depression, so its
# mixture richens only as the square root of the density ratio, sigma (the
# FAA, FAA-H-8083-32, page 2-3: carburettors "run richer at altitude ...
# because of the decreased density"): at 5,000 ft 9%, at 10,000 ft 18%. The
# lever as JSBSim reads it is therefore the pilot's times delta over the
# square root of sigma; a channel writes it after JSBSim copies the lever.
FLOAT_CARBURETTOR = """{indent}<channel name="Float carburettor">
{indent}    <!-- glideslope: see tools/piston_mixture.py -->
{indent}    <fcs_function name="fcs/carburettor-mixture">
{indent}        <function>
{indent}            <product>
{indent}                <property>fcs/mixture-cmd-norm[0]</property>
{indent}                <property>atmosphere/delta</property>
{indent}                <pow>
{indent}                    <property>atmosphere/sigma</property>
{indent}                    <value>-0.5</value>
{indent}                </pow>
{indent}            </product>
{indent}        </function>
{indent}        <output>fcs/mixture-pos-norm[0]</output>
{indent}    </fcs_function>
{indent}</channel>
"""


def with_float_carburettor(aircraft_text, script):
    """`aircraft_text`, a JSBSim aircraft file, with the carburettor channel
    last in its flight controls."""
    m = re.search(r"\n( *)</flight_control>", aircraft_text)
    if not m or "Float carburettor" in aircraft_text:
        raise SystemExit(f"{script}: no flight controls to give a carburettor to, "
                         "or it has one - has the pinned model changed?")
    indent = m.group(1) + "    "
    return (aircraft_text[:m.start() + 1] + FLOAT_CARBURETTOR.format(indent=indent)
            + aircraft_text[m.start() + 1:])
