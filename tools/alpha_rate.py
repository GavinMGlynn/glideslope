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
