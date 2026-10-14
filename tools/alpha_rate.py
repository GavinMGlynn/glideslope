"""alpha_rate.py - the alpha-rate terms of a flight model on the dynamic
pressure without its sideways part.

**Why.** JSBSim's angle of attack is atan2(w, u) and its rate is worked from
u, w and their rates (FGAuxiliary). Where the air meets her from the side -
an aeroplane slowing to a stop in a crosswind, her sideslip going to 90
degrees - u and w go to nothing, the angle of attack is undefined and its
rate swings by hundreds of degrees a second with the wind steady. A term that
multiplies that rate by the whole dynamic pressure, `aero/qbar-psf`, which
counts the crosswind in it, then pitches her by tens of degrees a second: on
the CI builds a C172P braked to a stop in a 15 kt crosswind pitched at 38 to
62 deg/s and struck her tail, the F-15C, 787-8 and F-22 likewise.

JSBSim's own diagnosis, and this remedy - the dynamic pressure of u and w
alone, `aero/qbarUW-psf`, in the terms that multiply the alpha rate, since the
sideways part of the air makes no lift on an unswept wing - are Bertrand
Coconnier's in JSBSim issue #89, "Pitch oscillations at rest, Brakes ON, with
pure crosswind" (https://github.com/JSBSim-Team/jsbsim/issues/89), adopted
by the FlightGear C172P (https://github.com/c172p-team/c172p/issues/1297).
In flight, where the sideslip is a few degrees, the two pressures differ by
its cosine squared.

Every function of a model whose product names `aero/alphadot-rad_sec` has
its `aero/qbar-psf` replaced by `aero/qbarUW-psf`, or its `aero/qbar-area`
by `aero/qbarUW-psf` times `metrics/Sw-sqft`. A model with no such term
fails, so that a script that applies this does not do so to nothing.
"""

import re

ALPHA_RATE = "aero/alphadot-rad_sec"


def with_qbar_uw(text, script):
    """`text`, a JSBSim model, with its alpha-rate terms on aero/qbarUW-psf."""
    changed = 0

    def fix(m):
        nonlocal changed
        block = m.group(0)
        if ALPHA_RATE not in block:
            return block
        new = block.replace("<property>aero/qbar-area</property>",
                            "<property>aero/qbarUW-psf</property>\n"
                            "                    <property>metrics/Sw-sqft</property>")
        new = new.replace("<property>aero/qbar-psf</property>",
                          "<property>aero/qbarUW-psf</property>")
        if new == block:
            raise SystemExit(f"{script}: an alpha-rate term with no aero/qbar-psf or "
                             f"aero/qbar-area to replace - has the model changed?")
        changed += 1
        return new

    text = re.sub(r"<function name=\"[^\"]+\">.*?</function>", fix, text, flags=re.S)
    if changed == 0:
        raise SystemExit(f"{script}: no alpha-rate term found - has the model changed?")
    return text


ALPHA = re.compile(r"aero/alpha-(rad|deg)<")
def axis_pattern(name):
    return re.compile(r"<axis name=\"" + name + r"\"[^>]*>.*?</axis>", re.S)


def with_pitch_alpha_on_qbar_uw(text, script, axes=("PITCH",)):
    """`text`, a JSBSim model, with every pitching-moment term keyed to the
    angle of attack on aero/qbarUW-psf.

    **Why.** The same singularity in a second place: standing or slowing in a
    crosswind her angle of attack, atan2(w, u), is any angle at all - the
    J-3 Cub's was 22 to 60 degrees standing in 15 kt across - and a pitching
    moment that multiplies it, or looks it up, on the whole dynamic pressure
    counts the crosswind in it: the Cub's Cm-alpha term pushed her nose down
    by up to 1,500 ft-lb against the ~1,080 her weight holds her tail down
    with, and she went over standing on her brakes. The pitching moment of a
    wing and tailplane that are not swept is made by the air in the plane of
    u and w; the sideways part makes none. JSBSim's guidance, from issue #89
    (https://github.com/JSBSim-Team/jsbsim/issues/89, Coconnier and dany93,
    2019, written into the FlightGear wiki's "JSBSim Aerodynamics": "Using
    QBarUW is more appropriate for Cm than the generic QBar"). In flight,
    with a few degrees of sideslip, the two differ by its cosine squared.

    The same holds for her lift (bcoconni's panel-method comparison in issue
    #89: on an unswept wing the lift is 0.5 rho (U^2 + W^2) S CL), and a
    script may name the LIFT axis too in `axes`. Every function inside each
    of `axes` (the PITCH axis alone, unless named) whose product reads
    `aero/alpha-rad` or `aero/alpha-deg` - as a factor or as a table's
    input - has its
    `aero/qbar-psf` replaced by `aero/qbarUW-psf`, or its `aero/qbar-area` by
    `aero/qbarUW-psf` times `metrics/Sw-sqft`. A model with no such term
    fails, so that a script that applies this does not do so to nothing.
    """
    changed = 0

    def fix(m):
        nonlocal changed
        block = m.group(0)
        if not ALPHA.search(block):
            return block
        new = block.replace("<property>aero/qbar-area</property>",
                            "<property>aero/qbarUW-psf</property>\n"
                            "                    <property>metrics/Sw-sqft</property>")
        new = new.replace("<property>aero/qbar-psf</property>",
                          "<property>aero/qbarUW-psf</property>")
        if new != block:
            changed += 1
        return new

    def axis(m):
        return re.sub(r"<function name=\"[^\"]+\">.*?</function>", fix, m.group(0), flags=re.S)

    for name in axes:
        text = axis_pattern(name).sub(axis, text)
    if changed == 0:
        raise SystemExit(f"{script}: no pitching-moment term keyed to alpha on the whole "
                         f"dynamic pressure - has the model changed?")
    return text
