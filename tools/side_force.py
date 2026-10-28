"""side_force.py - a flight model's side force taken out of her plane of
symmetry standing in a beam wind: tapered by cos^2 of her sideslip, so that
broadside only her drag with sideslip pushes her, along the wind.

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
sideslip, carried on to 90 degrees. Standing in a beam wind her flank meets
the air broadside, and the force on it is along the wind, which is across
her: her drag with sideslip. Every landplane here has one in her DRAG axis
(tests/tools/side_force_across.py checks that it is there, not its size at
90 degrees), and the J-3 Cub's is a cited figure: standing in 15 kt across
she is pushed along the wind within 0.03 of her side area's cited drag
(tests/unit/test_departure.cpp). A side force there as well counts the
broadside push twice, in the wrong direction. Carrying it across her body
instead was tried: it pushed the Cub at 0.53 of her dynamic pressure and
wing area against the figure's 0.32.

**A taper, cos^2 b.** So her side force is tapered: what is kept is cos^2 b
of it, sin^2 b left out - all of it as her maker gave it at small sideslip,
none of it standing in a beam wind. The factor is a taper chosen to end at
nothing at 90 degrees and to leave small sideslip all but untouched; it is
not the crossflow's share of anything. (Allen and Perkins' crossflow, NACA
Report 1048, 1951, adds a normal force going with sin^2 of the incidence to
a body's potential-flow force; it takes none out. It says only that
broadside the force is the crossflow's, along the flow across her, which
here is her drag with sideslip.) Every side term is tapered alike - CYb and
her rudder's, ailerons', roll rate's and yaw rate's (CYdr, CYda, CYp, CYr) -
which at small sideslip changes nothing that matters. What is kept:
- at 5 degrees of sideslip, 0.992;
- at 15 to 20 degrees, a crosswind slip, 0.933 to 0.883: such a slip is a
  little easier to hold, within the uncertainty of a slope carried that far
  (her fin stalls there, which no linear CYb knows);
- at 85 degrees, her creep in a beam wind, 0.008, and that much of the old
  leak reaches her plane of symmetry;
- at 90 degrees, none.
Spins and departures, where sideslip is large in flight, are not examined.

**How**, in the model and not in JSBSim. JSBSim cannot mix axis systems in
one model, and moving every model to body axes would move her lift and drag
too. One term is added at the end of her SIDE axis,
`aero/force/side-taper`, -sin^2 b times F, F the sum of her other side
terms, left as they are and read from their properties after they are
worked that step (an axis's functions are worked in order).

A model with no SIDE axis or no side term, or a model given this twice,
fails, so that a model changed under a script is noticed.
"""

import re

TAPER = "aero/force/side-taper"
FUNCTION = re.compile(r"<function name=\"([^\"]+)\">(.*?)</function>", re.S)
COMMENT = re.compile(r"<!--.*?-->", re.S)
BETA = "<property>aero/beta-rad</property>"


def with_side_force_tapered(text, script):
    """`text`, a JSBSim model in wind axes, with its side force tapered by
    cos^2 beta: sin^2 beta of it left out (above)."""
    if TAPER in text:
        raise SystemExit(f"{script}: her side force is tapered already")
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
    <function name="{TAPER}">
      <description>Her side force tapered by cos^2 beta, sin^2 beta of it left out: standing in a beam wind JSBSim's wind axes would put it in her plane of symmetry, and broadside her drag with sideslip pushes her (glideslope: tools/side_force.py)</description>
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
