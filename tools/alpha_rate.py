"""alpha_rate.py - the alpha-rate terms of a flight model, and the terms a
script names that are keyed to the angle of attack, on the dynamic pressure
without its sideways part.

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

`with_alpha_terms_on_qbar_uw` does the same for the terms keyed to the angle
of attack itself that a script names, function by function (below).
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
FUNCTION = re.compile(r"<function name=\"([^\"]+)\">.*?</function>", re.S)


def axis_pattern(name):
    return re.compile(r"<axis name=\"" + name + r"\"[^>]*>.*?</axis>", re.S)


def with_alpha_terms_on_qbar_uw(text, script, terms):
    """`text`, a JSBSim model, with the terms keyed to the angle of attack
    that `terms` names - {axis: [function name, ...]} - on aero/qbarUW-psf.

    **Why.** The alpha-rate fault in a second place: standing or slowing in a
    crosswind her angle of attack, atan2(w, u), is any angle at all - the
    J-3 Cub's was 22 to 60 degrees standing in 15 kt across - and a term that
    multiplies it, or looks it up, on the whole dynamic pressure counts the
    crosswind in it: the Cub's Cm-alpha term pushed her nose down by up to
    1,500 ft-lb against the ~1,080 her weight holds her tail down with, and
    she went over standing on her brakes. The pitching moment and lift of a
    wing and tailplane that are not swept are made by the air in the plane of
    u and w; the sideways part makes none. JSBSim's guidance, from issue #89
    (Coconnier's panel-method comparison: on an unswept wing the lift is
    0.5 rho (U^2 + W^2) S CL), written into the FlightGear wiki's "JSBSim
    Aerodynamics" (dany93, 2019): QBarUW "is more appropriate for Cm" and for
    CL than the generic QBar - guidance tested on the C172, swept wings not
    investigated. In flight, at a few degrees of sideslip, the two differ by
    its cosine squared.

    **Chosen, not swept up.** Which terms move is the script's to name and
    justify: a pitch-damping or elevator term keyed to alpha, or a swept
    wing's, is not moved because it reads alpha. Each named function has its
    `aero/qbar-psf` replaced by `aero/qbarUW-psf`, or its `aero/qbar-area` by
    `aero/qbarUW-psf` times `metrics/Sw-sqft`. It fails - so that a model
    changed under the script is noticed - if a named axis is missing, if a
    named function is not in its axis, does not read `aero/alpha-rad` or
    `aero/alpha-deg`, or has no `aero/qbar-psf` or `aero/qbar-area`.
    """
    for axis_name, names in terms.items():
        wanted = set(names)
        changed = set()

        def fix(m):
            name, block = m.group(1), m.group(0)
            if name not in wanted:
                return block
            if not ALPHA.search(block):
                raise SystemExit(f"{script}: {name} does not read the angle of attack - "
                                 f"has the model changed?")
            new = block.replace("<property>aero/qbar-area</property>",
                                "<property>aero/qbarUW-psf</property>\n"
                                "                    <property>metrics/Sw-sqft</property>")
            new = new.replace("<property>aero/qbar-psf</property>",
                              "<property>aero/qbarUW-psf</property>")
            if new == block:
                raise SystemExit(f"{script}: {name} has no aero/qbar-psf or aero/qbar-area "
                                 f"to replace - has the model changed?")
            changed.add(name)
            return new

        pattern = axis_pattern(axis_name)
        if not pattern.search(text):
            raise SystemExit(f"{script}: no {axis_name} axis - has the model changed?")
        text = pattern.sub(lambda m: FUNCTION.sub(fix, m.group(0)), text)
        if changed != wanted:
            raise SystemExit(f"{script}: in {axis_name}, {sorted(wanted - changed)} not found - "
                             f"has the model changed?")
    return text
