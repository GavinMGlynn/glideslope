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


ALPHA = re.compile(r"aero/alpha-(rad|deg)\s*<")
FUNCTION = re.compile(r"<function name=\"([^\"]+)\">.*?</function>", re.S)
COMMENT = re.compile(r"<!--.*?-->", re.S)


def functions_of_alpha(text):
    """The names of the functions in `text` that read the angle of attack,
    directly or through another such function as a property (the Mosquito's
    lift-alpha reads aero/function/lift-coefficient-wing, her table of the
    angle of attack and flap); commented-out functions are not read."""
    funcs = {m.group(1): m.group(0) for m in FUNCTION.finditer(COMMENT.sub("", text))}
    found = {n for n, b in funcs.items() if ALPHA.search(b)}
    grown = True
    while grown:
        grown = False
        for n, b in funcs.items():
            if n not in found and any(reads(b, a) for a in found if a != n):
                found.add(n)
                grown = True
    return found


def reads(block, prop):
    """Whether `block` reads the property `prop`, as a factor or a table's input."""
    return re.search(r">\s*" + re.escape(prop) + r"\s*<", block) is not None


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
    its cosine squared. **A wing's drag at its angle of attack** (2026-10-16)
    is moved on the same reasoning: made by the same air in the plane of u
    and w, while the sideways part runs along the span - on the whole
    dynamic pressure the J-3 Cub's Drag_basic, a flat plate's 1.41 at 90
    degrees, rolled her onto her wing standing in a beam wind.

    **A swept wing** (2026-10-15), where issue #89 did not look. Simple sweep
    theory (R. T. Jones, "Wing Plan Forms for High-Speed Flight", NACA
    Report 863, 1947): the pressures on a yawed wing are set by the air's
    component normal to its leading edge alone. A beam wind V on a wing swept
    by L meets each half at V sin L normal to its edge - not nothing, as on a
    straight wing - but at an incidence set by w over V sin L, not by
    atan2(w, u), and standing w is near nothing: the part of her moment that
    goes with the angle of attack is near nothing too (of order rho V sin L w,
    not rho V^2 times any angle). One half meets that flow leading edge first
    and the other trailing edge first; the theory says less of a section met
    backwards, but its force too goes with w and is near nothing standing.
    On aero/qbar-psf, Cm-alpha times atan2(w, u) times the beam wind's whole
    pressure is a moment of any size the undefined angle gives; on
    aero/qbarUW-psf it is of order rho (u^2 + w^2), near nothing. **Standing,
    then, neither is exact and qbarUW is the nearer, at any sweep.** **In
    flight it is not so plain**: at sideslip b the theory's mean over both
    halves is cos^2 b + tan^2 L sin^2 b of the pressure, between qbarUW's
    cos^2 b and qbar's 1 - nearer qbarUW below 35.3 degrees of sweep, midway
    there, nearer qbar above it, and qbar's own at 45. At 5 degrees of
    sideslip that is 0.994 at 25 degrees of sweep, 0.996 at 35, 0.999 at 42,
    against 0.992 and 1: under 1% either way, which no figure shows. Whether
    a wing's sweep is taken at its quarter chord or its leading edge changes
    nothing standing, and in flight only which of two choices under 1% apart
    is the nearer; every wing here is swept 45 degrees or less on either. So
    a swept wing's terms keyed to the angle of attack may be moved too, each
    script saying why; terms not keyed to it are left.

    **Chosen, not swept up.** Which terms move is the script's to name and
    justify: a pitch-damping or elevator term keyed to alpha, or a swept
    wing's, is not moved because it reads alpha. Each named function has its
    `aero/qbar-psf` replaced by `aero/qbarUW-psf`, or its `aero/qbar-area` by
    `aero/qbarUW-psf` times `metrics/Sw-sqft`. It fails - so that a model
    changed under the script is noticed - if a named axis is missing, if a
    named function is not in its axis, does not read `aero/alpha-rad` or
    `aero/alpha-deg` - itself, or through a function of the model that does
    (functions_of_alpha) - or has no `aero/qbar-psf` or `aero/qbar-area`.
    """
    of_alpha = functions_of_alpha(text)
    for axis_name, names in terms.items():
        wanted = set(names)
        changed = set()

        def fix(m):
            name, block = m.group(1), m.group(0)
            if name not in wanted:
                return block
            if not ALPHA.search(block) and not any(reads(block, a) for a in of_alpha if a != name):
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
