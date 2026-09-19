"""written.py - what the scripts share that write a flight model whole.

tools/make_a380.py and make_learjet35a.py each write a JSBSim model that
JSBSim does not have, from published data. The XML each needs - locations,
gear, flight control components, aerodynamic coefficients, ground effect -
is written here once; what goes in it, and where each number comes from, is
in each script.
"""


def location(x, y, z, indent, name=None):
    """A JSBSim location, in inches."""
    head = f"{indent}<location name=\"{name}\" unit=\"IN\">\n" if name else f"{indent}<location unit=\"IN\">\n"
    return (head +
            f"{indent}    <x> {x:.1f} </x>\n"
            f"{indent}    <y> {y:.1f} </y>\n"
            f"{indent}    <z> {z:.1f} </z>\n"
            f"{indent}</location>\n")


def bogey(name, x, y, z, spring, damping, steer, brake, static_friction, indent="        "):
    """A retractable wheel, at x, y, z inches: its spring and damping, the
    degrees it steers, its brake group and its static friction - a braked
    wheel's being its braking friction in JSBSim."""
    return (f"{indent}<contact type=\"BOGEY\" name=\"{name}\">\n"
            + location(x, y, z, indent + "    ") +
            f"{indent}    <static_friction> {static_friction} </static_friction>\n"
            f"{indent}    <dynamic_friction> 0.50 </dynamic_friction>\n"
            f"{indent}    <rolling_friction> 0.02 </rolling_friction>\n"
            f"{indent}    <spring_coeff unit=\"LBS/FT\"> {spring:.0f} </spring_coeff>\n"
            f"{indent}    <damping_coeff unit=\"LBS/FT/SEC\"> {damping:.0f} </damping_coeff>\n"
            f"{indent}    <max_steer unit=\"DEG\"> {steer} </max_steer>\n"
            f"{indent}    <brake_group> {brake} </brake_group>\n"
            f"{indent}    <retractable>1</retractable>\n"
            f"{indent}</contact>\n")


def structure(name, x, y, z, spring, damping, indent="        "):
    """A point of the airframe that can touch the ground, at x, y, z inches."""
    return (f"{indent}<contact type=\"STRUCTURE\" name=\"{name}\">\n"
            + location(x, y, z, indent + "    ") +
            f"{indent}    <static_friction> 1.0 </static_friction>\n"
            f"{indent}    <dynamic_friction> 1.0 </dynamic_friction>\n"
            f"{indent}    <spring_coeff unit=\"LBS/FT\"> {spring:.0f} </spring_coeff>\n"
            f"{indent}    <damping_coeff unit=\"LBS/FT/SEC\"> {damping:.0f} </damping_coeff>\n"
            f"{indent}</contact>\n")


def kinematic(name, cmd, positions, times, output, indent="            "):
    """A surface run by a lever through set positions, each reached in its
    time in seconds."""
    rows = "".join(f"{indent}        <setting>\n{indent}            <position> {p} </position>\n"
                   f"{indent}            <time> {t} </time>\n{indent}        </setting>\n"
                   for p, t in zip(positions, times))
    return (f"{indent}<kinematic name=\"{name}\">\n{indent}    <input>{cmd}</input>\n"
            f"{indent}    <traverse>\n{rows}{indent}    </traverse>\n"
            f"{indent}    <output>{output}</output>\n{indent}</kinematic>\n")


def surface(name, inputs, low, high, output, indent="            "):
    """A control surface: the sum of `inputs`, clipped to -1..1, scaled to
    `low`..`high` radians."""
    ins = "".join(f"{indent}    <input>{x}</input>\n" for x in inputs)
    return (f"{indent}<summer name=\"{name} Sum\">\n{ins}"
            f"{indent}    <clipto><min>-1</min><max>1</max></clipto>\n{indent}</summer>\n"
            f"{indent}<aerosurface_scale name=\"{name}\">\n"
            f"{indent}    <input>fcs/{name.lower().replace(' ', '-')}-sum</input>\n"
            f"{indent}    <range><min>{low:.4f}</min><max>{high:.4f}</max></range>\n"
            f"{indent}    <output>{output}</output>\n{indent}</aerosurface_scale>\n")


def yaw_damper(gain, indent="            "):
    """The rudder against the yaw rate, washed out over two seconds so that a
    steady turn is left alone, `gain` of its travel a radian a second and a
    fifth of its travel at most; its output fcs/yaw-damper."""
    return (f"{indent}<washout_filter name=\"Yaw Rate Washout\">\n"
            f"{indent}    <input>velocities/r-aero-rad_sec</input>\n"
            f"{indent}    <c1>0.5</c1>\n"
            f"{indent}</washout_filter>\n"
            f"{indent}<pure_gain name=\"Yaw Damper\">\n"
            f"{indent}    <input>fcs/yaw-rate-washout</input>\n"
            f"{indent}    <gain>{gain}</gain>\n"
            f"{indent}    <clipto><min>-0.2</min><max>0.2</max></clipto>\n"
            f"{indent}</pure_gain>\n")


def table1(var, rows, indent):
    """A JSBSim table of one variable."""
    body = "".join(f"{indent}        {a}\t{b}\n" for a, b in rows)
    return (f"{indent}<table>\n{indent}    <independentVar>{var}</independentVar>\n"
            f"{indent}    <tableData>\n{body}{indent}    </tableData>\n{indent}</table>\n")


def coefficient(name, description, factors, indent="            "):
    """A JSBSim aerodynamic function: the product of `factors`, each a
    property name, a number, or a ready-made element."""
    parts = []
    for f in factors:
        if isinstance(f, (int, float)):
            parts.append(f"{indent}        <value>{f:.5g}</value>\n")
        elif f.lstrip().startswith("<"):
            parts.append(f)
        else:
            parts.append(f"{indent}        <property>{f}</property>\n")
    return (f"{indent}<function name=\"aero/coefficient/{name}\">\n"
            f"{indent}    <description>{description}</description>\n"
            f"{indent}    <product>\n" + "".join(parts) +
            f"{indent}    </product>\n{indent}</function>\n")


def ground_effect():
    """The lift's and drag's change near the ground by height over span:
    Aeromatic's tables, as JSBSim's airliners carry."""
    return ("        <function name=\"aero/function/kCLge\">\n"
            "            <description>Change_in_lift_due_to_ground_effect</description>\n"
            + table1("aero/h_b-mac-ft", [(0.0, 1.203), (0.1, 1.127), (0.15, 1.090), (0.2, 1.073),
                                         (0.3, 1.046), (0.4, 1.055), (0.5, 1.019), (0.6, 1.013),
                                         (0.7, 1.008), (0.8, 1.006), (0.9, 1.003), (1.0, 1.002),
                                         (1.1, 1.0)], "            ") +
            "        </function>\n"
            "        <function name=\"aero/function/kCDge\">\n"
            "            <description>Change_in_drag_due_to_ground_effect</description>\n"
            + table1("aero/h_b-mac-ft", [(0.0, 0.480), (0.1, 0.515), (0.15, 0.629), (0.2, 0.709),
                                         (0.3, 0.815), (0.4, 0.882), (0.5, 0.928), (0.6, 0.962),
                                         (0.7, 0.988), (0.8, 1.0), (0.9, 1.0), (1.0, 1.0), (1.1, 1.0)],
                                         "            ") +
            "        </function>\n")


def axes(axes_functions):
    """The aerodynamics element: ground effect and each axis's functions,
    given as (axis name, [function text, ...]) in order."""
    out = "    <aerodynamics>\n" + ground_effect()
    for name, fs in axes_functions:
        out += f"        <axis name=\"{name}\">\n" + "".join(fs) + "        </axis>\n"
    return out + "    </aerodynamics>\n"
