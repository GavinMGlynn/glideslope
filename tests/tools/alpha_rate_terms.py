"""alpha_rate_terms.py - every committed flight model's alpha-rate terms are
on the dynamic pressure without its sideways part.

Every aircraft under assets/jsbsim/aircraft/ is read, every file of it. A
function that multiplies aero/alphadot-rad_sec must not also multiply the
whole dynamic pressure, aero/qbar-psf or aero/qbar-area, which counts the
crosswind (tools/alpha_rate.py says why). Each aircraft has such a term, but
those named in NONE, with the reason; an aircraft that has none and is not
named, or is named and has one, fails, and so does a count of aircraft that
differs from what the directory holds.

    python3 alpha_rate_terms.py <assets/jsbsim/aircraft>
"""
import pathlib
import re
import sys

# The B-2A's aerodynamics have no alpha-rate term at all.
NONE = {"b2": "her aerodynamics have no alpha-rate term"}

root = pathlib.Path(sys.argv[1])
aircraft = sorted(d for d in root.iterdir() if d.is_dir())
bad, covered = [], []
for d in aircraft:
    terms = 0
    for f in sorted(d.rglob("*.xml")):
        for fn in re.findall(r"<function name=\"[^\"]+\">.*?</function>", f.read_text(), flags=re.S):
            if "aero/alphadot-rad_sec" not in fn:
                continue
            terms += 1
            if "<property>aero/qbar-psf</property>" in fn or "<property>aero/qbar-area</property>" in fn:
                name = re.match(r"<function name=\"([^\"]+)\"", fn).group(1)
                bad.append(f"{d.name}: {f.relative_to(d)}: {name} is on the whole dynamic pressure")
    if terms == 0 and d.name not in NONE:
        bad.append(f"{d.name}: no alpha-rate term, and not named as having none")
    if terms and d.name in NONE:
        bad.append(f"{d.name}: named as having no alpha-rate term, but has {terms}")
    if terms:
        covered.append(f"{d.name} ({terms})")

expected = len(aircraft) - len(NONE)
print(f"{len(aircraft)} aircraft; {len(covered)} with alpha-rate terms checked, "
      f"{expected} expected: {', '.join(covered)}")
for n, why in NONE.items():
    print(f"not checked: {n}, {why}")
if len(covered) != expected:
    bad.append(f"{len(covered)} aircraft checked, {expected} expected")
for b in bad:
    print(b, file=sys.stderr)
sys.exit(1 if bad else 0)
