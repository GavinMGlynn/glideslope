"""side_force.py - a flight model's side force taken out of her plane of
symmetry standing in a beam wind: its crossflow share, which along the wind
is her drag with sideslip, left out.

**Why.** Every model here gives her forces as DRAG, SIDE and LIFT, which
JSBSim takes in its wind axes and turns into her body's with Tw2b
(FGAerodynamics, FGAuxiliary). The wind axes' y is the side force's, and in
her body it lies along (-cos a sin b, cos b, -sin a sin b), where a is her
angle of attack, atan2(w, u), and b her sideslip, atan2(v, sqrt(u^2 + w^2)).
In flight, at a few degrees of sideslip, that is across her within a few
degrees. Standing in a beam wind it is not: b goes to 90 degrees, cos b to
nothing, and the side force lies in her plane of symmetry, fore and aft or
up and down as the angle of attack - undefined standing, set by her creep -
turns it. Standing in 15 kt straight across a 737-300 on her brakes had
1,400 lb of it along her body's x axis and 7,000 ft-lb of moment about her
centre of gravity; it put the J-3 Cub over when she braked in a crosswind.
The FlightGear wiki's "JSBSim Aerodynamics" says as much of the wind axes:
the lift, drag and side split breaks down with her x axis across the
relative wind.

**What her side force is, there.** Each model's side force is a slope, CYb
times b, the force across the wind that her fin and fuselage make at small
sideslip, carried on to 90 degrees. Of the force on a body at incidence b,
the part made by the flow across it, the crossflow, goes with sin^2 b and
lies along the flow across it (H. J. Allen and E. W. Perkins, "A study of
effects of viscosity on flow over slender inclined bodies of revolution",
NACA Report 1048, 1951); standing in a beam wind her flank meets the air
broadside, and the whole force is that: along the wind, which is across her
- her drag with sideslip. Every landplane here has one in her DRAG axis
(tests/tools/side_force_across.py checks it), and the J-3 Cub's is a cited
figure: standing in 15 kt across she is pushed along the wind within 0.03
of her side area's cited drag (tests/unit/test_departure.cpp). Carrying her
side force across her body as well counted that force twice: tried, it
pushed the Cub at 0.53 of her dynamic pressure and wing area against the
figure's 0.32. So the crossflow's share of her side force, sin^2 b of it,
is left out of it, and what is left, cos^2 b of it, is her side force
across the wind as her maker gave it: all but a sliver of it in flight,
none of it standing in a beam wind. At 5 degrees of sideslip it is 0.992 of
the model's; at 85 degrees, her creep in a beam wind, 0.008 of it, and what
reaches her plane of symmetry is that much of the old leak.

**How**, in the model and not in JSBSim. JSBSim cannot mix axis systems in
one model, and moving every model to body axes would move her lift and drag
too. One term is added at the end of her SIDE axis,
`aero/force/side-crossflow`, -sin^2 b times F, F the sum of her other side
terms, left as they are and read from their properties after they are
worked that step (an axis's functions are worked in order).

A model with no SIDE axis or no side term, or a model given this twice,
fails, so that a model changed under a script is noticed.
"""

import re

CROSSFLOW = "aero/force/side-crossflow"
FUNCTION = re.compile(r"<function name=\"([^\"]+)\">(.*?)</function>", re.S)
COMMENT = re.compile(r"<!--.*?-->", re.S)
BETA = "<property>aero/beta-rad</property>"


def without_crossflow(text, script):
    """`text`, a JSBSim model in wind axes, with the crossflow's share of its
    side force, sin^2 beta, left out (above)."""
    if CROSSFLOW in text:
        raise SystemExit(f"{script}: the crossflow is out of her side force already")
    side = re.search(r"(<axis name=\"SIDE\"[^>]*>)(.*?)(\n[ \t]*</axis>)", text, re.S)
    if not side:
        raise SystemExit(f"{script}: no SIDE axis - has the model changed?")
    names = [n for n, _ in FUNCTION.findall(COMMENT.sub("", side.group(2)))]
    if not names:
        raise SystemExit(f"{script}: no side force term - has the model changed?")
    if len(names) == 1:
        reads = f"        <property>{names[0]}</property>"
    else:
        reads = ("        <sum>\n" + "\n".join(f"          <property>{n}</property>" for n in names)
                 + "\n        </sum>")
    term = f"""
    <function name="{CROSSFLOW}">
      <description>The crossflow's share of her side force, sin^2 beta of it, left out: along the wind it is her drag with sideslip, and standing in a beam wind JSBSim's wind axes would put it in her plane of symmetry (glideslope: tools/side_force.py)</description>
      <product>
        <value>-1</value>
{reads}
        <pow>
          <sin>{BETA}</sin>
          <value>2</value>
        </pow>
      </product>
    </function>"""
    return text[:side.end(2)] + term + text[side.end(2):]
