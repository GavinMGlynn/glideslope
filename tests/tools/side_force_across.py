"""side_force_across.py - every committed flight model's side force is
tapered to nothing in a beam wind, and every landplane has a drag with
sideslip to push her across.

Every aircraft under assets/jsbsim/aircraft/ is read, every file of it, its
XML comments left out. Its forces are in wind axes (DRAG, SIDE, LIFT; no X,
Y, Z, AXIAL or NORMAL axis). Its SIDE axis ends with tools/side_force.py's
`aero/force/side-taper`: -1 times the sum of the properties of every
other term of that axis, each read once (a sum of one is the term alone),
times sin^2 beta, so that cos^2 beta of her side force is kept. Standing in
a beam wind JSBSim's wind axes put her side force in her plane of symmetry,
and broadside what pushes her is her drag with sideslip, along the wind
(side_force.py says why). So every landplane
must have that drag: a term of her DRAG axis that reads aero/beta-rad,
aero/beta-deg or their magnitudes. The flying boat has none and is named: afloat she turns into
the wind, so no beam wind stands across her.

The space is stated: sixteen aircraft, the side terms of each listed, and
the drag with sideslip of each. An aircraft in the directory not listed,
one listed and missing, a side term not listed or listed and not found, the
added term missing or not as above, or a drag with sideslip not as listed,
fails.

    python3 side_force_across.py <assets/jsbsim/aircraft>
"""
import pathlib
import re
import sys

C = "aero/coefficient/"
F = "aero/force/"
SIDE = {
    "737-300": [C + "CYb"],
    "747-400": [C + "CYb"],
    "787-8": [F + "Side_beta"],
    "a320": [C + "CYb"],
    "a380": [C + "CYb", C + "CYdr"],
    "b2": [C + "CYb"],
    "c172p": [C + "CYb", C + "CYda", C + "CYdr", C + "CYp", C + "CYr"],
    "c182": [C + "CYb", C + "CYp", C + "CYr", C + "CYDa", C + "CYDr"],
    "f15c": [C + "CYb", C + "CYda", C + "CYdr"],
    "f22": [C + "CYb", C + "CYb_M", C + "CYbdot", C + "CYDa", C + "CYDr", C + "CYp", C + "CYr"],
    "f35b": [C + "CYb", C + "CYda", C + "CYdr"],
    "j3cub": [F + "Side_beta", F + "Side_roll_rate", F + "Side_yaw_rate", F + "Side_rudder"],
    "learjet35a": [C + "CYb", C + "CYdr"],
    "mosquito-fb6": [F + "side-sideslip", F + "side-rudder"],
    "pa28": [C + "CYb", C + "CYda", C + "CYdr", C + "CYp", C + "CYr"],
    "short_s23": [C + "Cyb", C + "Cyp", C + "Cyr", C + "CyDr", C + "CyDa"],
}
SIDESLIP_DRAG = {
    "737-300": [C + "CDbeta"],
    "747-400": [C + "CDbeta"],
    "787-8": [F + "Drag_beta"],
    "a320": [C + "CDbeta"],
    "a380": [C + "CDbeta"],
    "b2": [C + "CDbeta"],
    "c172p": [C + "CDbeta"],
    "c182": [C + "CDbeta"],
    "f15c": [C + "CDbeta"],
    "f22": [C + "CDbeta"],
    "f35b": [C + "CDbeta"],
    "j3cub": [F + "Drag_beta"],
    "learjet35a": [C + "CDbeta"],
    "mosquito-fb6": [F + "drag-sideslip"],
    "pa28": [C + "CDbeta"],
    # Named: a flying boat; afloat she turns into the wind.
    "short_s23": [],
}
TOTAL = 16
assert len(SIDE) == TOTAL and set(SIDESLIP_DRAG) == set(SIDE)
if len(sys.argv) != 2:
    raise SystemExit("usage: side_force_across.py <assets/jsbsim/aircraft>")
TAPER = "aero/force/side-taper"
BETA = "<property>aero/beta-rad</property>"
FUNCTION = re.compile(r"<function name=\"([^\"]+)\">(.*?)</function>", re.S)
COMMENT = re.compile(r"<!--.*?-->", re.S)
DESCRIPTION = re.compile(r"<description>.*?</description>", re.S)


def axis(text, name):
    found = re.findall(r"<axis name=\"" + name + r"\"[^>]*>(.*?)</axis>", text, re.S)
    return found[0] if len(found) == 1 else None


def squeezed(s):
    return re.sub(r"\s+", "", s)


root = pathlib.Path(sys.argv[1])
present = {d.name for d in root.iterdir() if d.is_dir()}
bad = []
for name in sorted(present - set(SIDE)):
    bad.append(f"{name}: in the directory, but its side terms are not listed")
for name in sorted(set(SIDE) - present):
    bad.append(f"{name}: listed, but not in the directory")

left_out = terms_seen = drags_seen = 0
for name in sorted(present & set(SIDE)):
    text = "\n".join(COMMENT.sub("", f.read_text()) for f in sorted((root / name).rglob("*.xml")))
    if re.search(r"<axis name=\"(X|Y|Z|AXIAL|NORMAL)\"", text):
        bad.append(f"{name}: forces not in wind axes")
        continue
    side, drag = axis(text, "SIDE"), axis(text, "DRAG")
    if side is None or drag is None:
        bad.append(f"{name}: not one SIDE axis and one DRAG axis")
        continue
    drags = [n for n, b in FUNCTION.findall(drag) if re.search(r">\s*aero/(mag-)?beta-(rad|deg)\s*<", b)]
    if drags != SIDESLIP_DRAG[name]:
        bad.append(f"{name}: drag with sideslip {drags}, listed {SIDESLIP_DRAG[name]}")
    else:
        drags_seen += len(drags)
    funcs = FUNCTION.findall(side)
    names = [n for n, _ in funcs if n != TAPER]
    if names != SIDE[name]:
        bad.append(f"{name}: side terms {names}, listed {SIDE[name]}")
        continue
    terms_seen += len(names)
    if [n for n, _ in funcs].count(TAPER) != 1 or funcs[-1][0] != TAPER:
        bad.append(f"{name}: SIDE does not end with one {TAPER}")
        continue
    reads = "".join(f"<property>{n}</property>" for n in names)
    if len(names) > 1:
        reads = f"<sum>{reads}</sum>"
    want = squeezed(f"<product><value>-1</value>{reads}<pow><sin>{BETA}</sin><value>2</value>"
                    "</pow></product>")
    if squeezed(DESCRIPTION.sub("", funcs[-1][1])) != want:
        bad.append(f"{name}: {TAPER} is not -sin^2 beta times her side terms")
        continue
    left_out += 1

want_terms = sum(len(v) for v in SIDE.values())
want_drags = sum(len(v) for v in SIDESLIP_DRAG.values())
print(f"side force tapered: {len(present)} aircraft of {TOTAL}; {left_out} tapered, {terms_seen} of {want_terms} side terms; {drags_seen} of {want_drags} drags with "
      f"sideslip")
print("named: short_s23 has no drag with sideslip: a flying boat, afloat she turns into the wind")
if (len(present) != TOTAL or left_out != TOTAL or terms_seen != want_terms
        or drags_seen != want_drags):
    bad.append(f"{len(present)} aircraft of {TOTAL}, {left_out} leave it out, "
               f"{terms_seen} side terms of {want_terms}, {drags_seen} drags of {want_drags}")
for b in bad:
    print(b, file=sys.stderr)
sys.exit(1 if bad else 0)
