"""alpha_lift_terms.py - every committed flight model's lift and drag terms
keyed to the angle of attack are on the dynamic pressure without its sideways
part, or are named here with the reason they are not.

Every aircraft under assets/jsbsim/aircraft/ is read, every file of it, its
XML comments left out (the F-22A's pinned model keeps two old CLDh terms in
one). In its LIFT or DRAG axis, the one asked for, a function that reads
aero/alpha-rad or aero/alpha-deg - as a factor or a table's input, itself or
through a function of the model that does (the Mosquito's lift-alpha reads
her lift-coefficient-wing table) - and names a dynamic pressure (any property
containing "qbar") is one of her angle-of-attack terms on that axis. Those
that also multiply aero/alphadot-rad_sec are alpha_rate_terms.py's, and left
to it. Each of the rest must be in MOVED, every dynamic pressure in it
aero/qbarUW-psf (tools/alpha_rate.py's with_alpha_terms_on_qbar_uw), or in
NAMED, on the whole aero/qbar-psf, with its reason. Standing or slowing in a
crosswind her angle of attack, atan2(w, u), is any angle at all; on the
whole dynamic pressure such a term counts the crosswind (alpha_rate.py says
why). The sibling for the PITCH axis is alpha_pitch_terms.py.

The space is stated, not read from the directory: sixteen aircraft, the
terms of each listed for each axis - an empty list where she has none (the
A320's and the PA-28's drag is not keyed to the angle of attack). An
aircraft in the directory that is not listed, one listed and missing, a term
found that is in neither list, a listed term not found, a moved term still
on the whole dynamic pressure, or a named one moved, fails.

    python3 alpha_lift_terms.py <assets/jsbsim/aircraft> LIFT|DRAG
"""
import pathlib
import re
import sys

# Which functions read the angle of attack is the make scripts' own answer
# (tools/alpha_rate.py), so that the check and what it checks agree on it.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from alpha_rate import functions_of_alpha  # noqa: E402

C = "aero/coefficient/"
F = "aero/force/"
MOVED = {
    "LIFT": {
        "737-300": [C + "CLalpha"],
        "747-400": [C + "CLalpha"],
        "787-8": [C + "CLalpha"],
        "a320": [C + "CLalpha"],
        "a380": [C + "CLalpha"],
        "b2": [C + "CLalpha"],
        "c172p": [C + "CLwbh"],
        "c182": [C + "CLalpha"],
        "f15c": [C + "CLalpha"],
        "f22": [C + "CLalpha", C + "CLDlef"],
        "f35b": [C + "CLalpha"],
        "j3cub": [F + "Lift_alpha", F + "Lift_propwash"],
        "learjet35a": [C + "CLalpha"],
        "mosquito-fb6": [F + "lift-alpha"],
        "pa28": [C + "CLwbh"],
        "short_s23": [C + "CLwbh"],
    },
    "DRAG": {
        "737-300": [C + "CD0"],
        "747-400": [C + "CD0"],
        "787-8": [F + "Drag_basic"],
        "a320": [],
        "a380": [C + "CD0"],
        "b2": [C + "CD0"],
        "c172p": [C + "CDwbh"],
        "c182": [C + "CDalpha", C + "CDDf"],
        "f15c": [C + "CDseparated"],
        "f22": [C + "CDseparated", C + "CDDlef"],
        "f35b": [C + "CDseparated"],
        "j3cub": [F + "Drag_basic"],
        "learjet35a": [C + "CD0"],
        "mosquito-fb6": [F + "drag-stall"],
        "pa28": [],
        "short_s23": [C + "CD", C + "CdDf3L", C + "CdDf3R"],
    },
}
DAMPING = "with pitch rate: the angle of attack only picks the coefficient of her pitch rate"
NAMED = {
    "LIFT": {
        "f22": {C + "CLq": "lift " + DAMPING},
        "j3cub": {F + "Lift_pitch_rate": "lift " + DAMPING},
        "pa28": {C + "CLq": "lift " + DAMPING},
        "short_s23": {C + "CLq": "lift " + DAMPING},
    },
    "DRAG": {
        "f15c": {C + "CDbeta": "drag with sideslip: keyed to the sideways air by design; the "
                               "angle of attack only picks its coefficient of it"},
        "f22": {C + "CDq": "drag " + DAMPING, C + "CDq_Dlef": "drag " + DAMPING},
        "short_s23": {C + "CdDe": "her elevator's drag, not her wing's: a table of the angle "
                                  "of attack and the deflection, under 0.00002 at no "
                                  "deflection at every angle, 0.029 at most"},
    },
}
TOTAL = 16
assert all(len(MOVED[a]) == TOTAL and set(NAMED[a]) <= set(MOVED[a]) for a in MOVED)
if len(sys.argv) != 3 or sys.argv[2] not in MOVED:
    raise SystemExit("usage: alpha_lift_terms.py <assets/jsbsim/aircraft> LIFT|DRAG")
WHICH = sys.argv[2]
MOVED, NAMED = MOVED[WHICH], NAMED[WHICH]
ALLOWED = "aero/qbarUW-psf"
AXIS = re.compile(r"<axis name=\"" + WHICH + r"\"[^>]*>.*?</axis>", re.S)
FUNCTION = re.compile(r"<function name=\"([^\"]+)\">.*?</function>", re.S)
COMMENT = re.compile(r"<!--.*?-->", re.S)



root = pathlib.Path(sys.argv[1])
present = {d.name for d in root.iterdir() if d.is_dir()}
bad = []
for name in sorted(present - set(MOVED)):
    bad.append(f"{name}: in the directory, but its terms are not listed")
for name in sorted(set(MOVED) - present):
    bad.append(f"{name}: listed, but not in the directory")

moved_count = named_count = 0
for name in sorted(present & set(MOVED)):
    texts = [COMMENT.sub("", f.read_text()) for f in sorted((root / name).rglob("*.xml"))]
    of_alpha = functions_of_alpha("\n".join(texts))
    found = {}
    for text in texts:
        for axis in AXIS.findall(text):
            for m in FUNCTION.finditer(axis):
                fn, block = m.group(1), m.group(0)
                if "aero/alphadot-rad_sec" in block:
                    continue
                if fn not in of_alpha:
                    continue
                qbars = re.findall(r"<property>\s*([^<]*qbar[^<]*?)\s*</property>", block)
                if qbars:
                    found[fn] = qbars
    moved, named = set(MOVED[name]), set(NAMED.get(name, {}))
    for fn in sorted(set(found) - moved - named):
        bad.append(f"{name}: {fn} reads the angle of attack on {found[fn]}, "
                   f"and is neither moved nor named")
    for fn in sorted((moved | named) - set(found)):
        bad.append(f"{name}: {fn} is listed, but no such term was found")
    for fn in sorted(moved & set(found)):
        wrong = [q for q in found[fn] if q != ALLOWED]
        if wrong:
            bad.append(f"{name}: {fn} is on {wrong}, not {ALLOWED}")
        else:
            moved_count += 1
    for fn in sorted(named & set(found)):
        if all(q == ALLOWED for q in found[fn]):
            bad.append(f"{name}: {fn} is named as not moved, but is on {ALLOWED}")
        else:
            named_count += 1

want_moved = sum(len(v) for v in MOVED.values())
want_named = sum(len(v) for v in NAMED.values())
print(f"{WHICH}: {len(present)} aircraft of {TOTAL}; {moved_count} of {want_moved} terms "
      f"moved, {named_count} of {want_named} named")
for n, terms in NAMED.items():
    for fn, why in terms.items():
        print(f"not moved: {n} {fn}: {why}")
if len(present) != TOTAL or moved_count != want_moved or named_count != want_named:
    bad.append(f"{len(present)} aircraft of {TOTAL}, {moved_count} terms moved of "
               f"{want_moved}, {named_count} named of {want_named}")
for b in bad:
    print(b, file=sys.stderr)
sys.exit(1 if bad else 0)
