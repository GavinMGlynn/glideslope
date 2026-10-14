"""alpha_rate_terms.py - every committed flight model's alpha-rate terms are
on the dynamic pressure without its sideways part.

Every aircraft under assets/jsbsim/aircraft/ is read, every file of it. In a
function that multiplies aero/alphadot-rad_sec, every property naming a
dynamic pressure - any containing "qbar" - must be aero/qbarUW-psf, the form
tools/alpha_rate.py writes; aero/qbar-psf, aero/qbar-area or any other
spelling counts the crosswind (alpha_rate.py says why).

The space is stated, not read from the directory: the fifteen aircraft in
CHECKED each have such a term, and the B-2A, in NONE, has none. An aircraft
in the directory that is in neither, one of them missing from it, one in
CHECKED with no term or one in NONE with one, fails.

    python3 alpha_rate_terms.py <assets/jsbsim/aircraft>
"""
import pathlib
import re
import sys

CHECKED = {"737-300", "747-400", "787-8", "a320", "a380", "c172p", "c182", "f15c",
           "f22", "f35b", "j3cub", "learjet35a", "mosquito-fb6", "pa28", "short_s23"}
NONE = {"b2": "her aerodynamics have no alpha-rate term"}
TOTAL = 16
assert len(CHECKED) == 15 and len(CHECKED) + len(NONE) == TOTAL
ALLOWED = "aero/qbarUW-psf"

root = pathlib.Path(sys.argv[1])
present = {d.name for d in root.iterdir() if d.is_dir()}
bad, covered = [], []
for name in sorted(present - CHECKED - set(NONE)):
    bad.append(f"{name}: in the directory, but neither checked nor named as having no term")
for name in sorted((CHECKED | set(NONE)) - present):
    bad.append(f"{name}: expected, but not in the directory")

for name in sorted(present & (CHECKED | set(NONE))):
    d = root / name
    terms = 0
    for f in sorted(d.rglob("*.xml")):
        for fn in re.findall(r"<function name=\"[^\"]+\">.*?</function>", f.read_text(), flags=re.S):
            if "aero/alphadot-rad_sec" not in fn:
                continue
            terms += 1
            fname = re.match(r"<function name=\"([^\"]+)\"", fn).group(1)
            qbars = re.findall(r"<property>\s*([^<]*qbar[^<]*?)\s*</property>", fn)
            if not qbars:
                bad.append(f"{name}: {f.relative_to(d)}: {fname} names no dynamic pressure")
            for q in qbars:
                if q != ALLOWED:
                    bad.append(f"{name}: {f.relative_to(d)}: {fname} is on {q}, not {ALLOWED}")
    if name in CHECKED and terms == 0:
        bad.append(f"{name}: no alpha-rate term, but listed as having one")
    if name in NONE and terms:
        bad.append(f"{name}: named as having no alpha-rate term, but has {terms}")
    if name in CHECKED and terms:
        covered.append(f"{name} ({terms})")

print(f"{len(present)} aircraft of {TOTAL}; {len(covered)} of {len(CHECKED)} checked: "
      f"{', '.join(covered)}")
for n, why in NONE.items():
    print(f"not checked: {n}, {why}")
if len(present) != TOTAL or len(covered) != len(CHECKED):
    bad.append(f"{len(present)} aircraft of {TOTAL}, {len(covered)} checked of {len(CHECKED)}")
for b in bad:
    print(b, file=sys.stderr)
sys.exit(1 if bad else 0)
