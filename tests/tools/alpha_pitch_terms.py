"""alpha_pitch_terms.py - every committed flight model's pitching-moment terms
keyed to the angle of attack are on the dynamic pressure without its sideways
part, or are named here with the reason they are not.

Every aircraft under assets/jsbsim/aircraft/ is read, every file of it. In its
PITCH axis, a function that reads aero/alpha-rad or aero/alpha-deg - as a
factor or a table's input - and names a dynamic pressure (any property
containing "qbar") is one of her angle-of-attack pitch terms. Those that also
multiply aero/alphadot-rad_sec are alpha_rate_terms.py's, and left to it.
Each of the rest must be in MOVED, every dynamic pressure in it
aero/qbarUW-psf (tools/alpha_rate.py's with_alpha_terms_on_qbar_uw), or in
NAMED, on the whole aero/qbar-psf, with its reason. Standing or slowing in a
crosswind her angle of attack, atan2(w, u), is any angle at all; on the whole
dynamic pressure such a term counts the crosswind (alpha_rate.py says why).

The space is stated, not read from the directory: sixteen aircraft, the
terms of each listed. An aircraft in the directory that is not listed, one
listed and missing, a term found that is in neither list, a listed term not
found, a moved term still on the whole dynamic pressure, or a named one
moved, fails.

    python3 alpha_pitch_terms.py <assets/jsbsim/aircraft>
"""
import pathlib
import re
import sys

C = "aero/coefficient/"
M = "aero/moment/"
MOVED = {
    "737-300": [C + "Cmalpha"],
    "747-400": [C + "Cmalpha"],
    "787-8": [M + "Pitch_alpha"],
    "a320": [C + "Cmalpha"],
    "a380": [C + "Cmalpha"],
    "b2": [C + "Cmalpha"],
    "c172p": [C + "Cmalpha"],
    "c182": [C + "Cmalpha"],
    "f15c": [C + "Cmalpha"],
    "f22": [C + "Cma_M", C + "CmDh_alpha"],
    "f35b": [C + "Cmalpha"],
    "j3cub": [M + "Pitch_alpha", M + "Pitch_propwash"],
    "learjet35a": [C + "Cmalpha"],
    "mosquito-fb6": [M + "pitch-alpha", M + "pitch-stall"],
    "pa28": [C + "Cmalpha"],
    "short_s23": [C + "Cm_basic"],
}
DAMPING = "pitch damping: the angle of attack only picks the coefficient of her pitch rate"
NAMED = {
    "c182": {C + "CmDe": "the elevator's term: the angle of attack only picks its "
                         "coefficient, bounded (-0.62 to -0.87), times the deflection"},
    "f22": {
        C + "CmDh": "her tailerons' increment over CmDh_alpha, their power: an elevator "
                    "term",
        C + "CmDtv": "her thrust vectoring's moment, made by her engines, not her wing",
        C + "Cmq": DAMPING,
    },
    "short_s23": {C + "Cmq": DAMPING},
}
TOTAL = 16
assert len(MOVED) == TOTAL and set(NAMED) <= set(MOVED)
ALLOWED = "aero/qbarUW-psf"
ALPHA = re.compile(r"aero/alpha-(rad|deg)\s*<")
AXIS = re.compile(r"<axis name=\"PITCH\"[^>]*>.*?</axis>", re.S)
FUNCTION = re.compile(r"<function name=\"([^\"]+)\">.*?</function>", re.S)

root = pathlib.Path(sys.argv[1])
present = {d.name for d in root.iterdir() if d.is_dir()}
bad = []
for name in sorted(present - set(MOVED)):
    bad.append(f"{name}: in the directory, but its terms are not listed")
for name in sorted(set(MOVED) - present):
    bad.append(f"{name}: listed, but not in the directory")

moved_count = named_count = 0
for name in sorted(present & set(MOVED)):
    d = root / name
    found = {}
    for f in sorted(d.rglob("*.xml")):
        for axis in AXIS.findall(f.read_text()):
            for m in FUNCTION.finditer(axis):
                fn, block = m.group(1), m.group(0)
                if not ALPHA.search(block) or "aero/alphadot-rad_sec" in block:
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
print(f"{len(present)} aircraft of {TOTAL}; {moved_count} of {want_moved} terms moved, "
      f"{named_count} of {want_named} named")
for n, terms in NAMED.items():
    for fn, why in terms.items():
        print(f"not moved: {n} {fn}: {why}")
if len(present) != TOTAL or moved_count != want_moved or named_count != want_named:
    bad.append(f"{len(present)} aircraft of {TOTAL}, {moved_count} terms moved of "
               f"{want_moved}, {named_count} named of {want_named}")
for b in bad:
    print(b, file=sys.stderr)
sys.exit(1 if bad else 0)
