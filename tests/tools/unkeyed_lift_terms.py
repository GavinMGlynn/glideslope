"""unkeyed_lift_terms.py - every committed flight model's lift terms *not*
keyed to the angle of attack are on the dynamic pressure without its
sideways part, or are named here with the reason they are not.

alpha_lift_terms.py's sibling. Every aircraft under assets/jsbsim/aircraft/
is read, every file of it, its XML comments left out. In its LIFT axis, a
function that names a dynamic pressure (any property containing "qbar"),
does not read the angle of attack (tools/alpha_rate.py's functions_of_alpha,
alpha_lift_terms.py's) and does not multiply aero/alphadot-rad_sec
(alpha_rate_terms.py's) is one of her lift terms not keyed to the angle of
attack: a flap's, an elevator's, a stabiliser's, a spoiler's, a wing's at no
angle, or with pitch rate. Each must be in MOVED, every dynamic pressure in
it aero/qbarUW-psf - or, for the Mosquito's elevator, her tailplane's
pressure made from it, aero/function/qbarUW-tail-psf, which must itself read
aero/qbarUW-psf and not aero/qbar-psf - (tools/alpha_rate.py's
with_unkeyed_terms_on_qbar_uw), or in NAMED, on the whole aero/qbar-psf,
with its reason. Standing in a crosswind such a term on the whole dynamic
pressure counts the crosswind: the C182 was lifted by up to 46 lb, the F-22A
by 280.

The space is stated: sixteen aircraft, the terms of each listed. An aircraft
in the directory not listed, one listed and missing, a term found in neither
list, a listed term not found, a moved term still on the whole dynamic
pressure, or a named one moved, fails.

    python3 unkeyed_lift_terms.py <assets/jsbsim/aircraft>
"""
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from alpha_rate import functions_of_alpha  # noqa: E402

C = "aero/coefficient/"
F = "aero/force/"
MOVED = {
    "737-300": [C + "dCLflap", C + "CLde"],
    "747-400": [C + "dCLflap", C + "dCLsb", C + "CLde"],
    "787-8": [F + "Lift_flap", F + "Lift_speedbrake", F + "Lift_elevator"],
    "a320": [C + "CLDe"],
    "a380": [C + "CLq", C + "CLde"],
    "b2": [C + "CLde"],
    "c172p": [C + "CLq", C + "CLDf", C + "CLDe"],
    "c182": [C + "CLq", C + "CLo", C + "CLDf", C + "CLDe"],
    "f15c": [C + "CLq", C + "CLDe"],
    "f22": [C + "dCLflap", C + "dCLsb", C + "CLde"],
    "f35b": [C + "CLde", C + "CLdroop"],
    "j3cub": [F + "Lift_elevator"],
    "learjet35a": [C + "CLq", C + "CLde", C + "CLstab", C + "CLspoilers"],
    "mosquito-fb6": [F + "lift-elevator", F + "lift-pitch-rate"],
    "pa28": [C + "CLDf", C + "CLDe"],
    "short_s23": [C + "CLdF3L", C + "CLdF3R", C + "CLDe"],
}
# Every such term is moved. Lift with pitch rate (CLq) too: the lift its
# pitching adds at the tail, at an incidence of q l / V, is made by the same
# air in the plane of u and w (alpha_rate.with_unkeyed_terms_on_qbar_uw).
NAMED = {}
TOTAL = 16
assert len(MOVED) == TOTAL and set(NAMED) <= set(MOVED)
if len(sys.argv) != 2:
    raise SystemExit("usage: unkeyed_lift_terms.py <assets/jsbsim/aircraft>")
ALLOWED = {"aero/qbarUW-psf", "aero/function/qbarUW-tail-psf"}
AXIS = re.compile(r"<axis name=\"LIFT\"[^>]*>.*?</axis>", re.S)
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
    whole = "\n".join(texts)
    of_alpha = functions_of_alpha(whole)
    # A pressure made by the model must itself leave out the crosswind.
    for m in FUNCTION.finditer(whole):
        if m.group(1) in ALLOWED and ("aero/qbar-psf" in m.group(0)
                                      or "aero/qbarUW-psf" not in m.group(0)):
            bad.append(f"{name}: {m.group(1)} does not read aero/qbarUW-psf alone")
    found = {}
    for text in texts:
        for axis in AXIS.findall(text):
            for m in FUNCTION.finditer(axis):
                fn, block = m.group(1), m.group(0)
                if "aero/alphadot-rad_sec" in block or fn in of_alpha:
                    continue
                qbars = re.findall(r"<property>\s*([^<]*qbar[^<]*?)\s*</property>", block)
                if qbars:
                    found[fn] = qbars
    moved, named = set(MOVED[name]), set(NAMED.get(name, {}))
    for fn in sorted(set(found) - moved - named):
        bad.append(f"{name}: {fn}, not keyed to the angle of attack, is on {found[fn]}, "
                   f"and is neither moved nor named")
    for fn in sorted((moved | named) - set(found)):
        bad.append(f"{name}: {fn} is listed, but no such term was found")
    for fn in sorted(moved & set(found)):
        wrong = [q for q in found[fn] if q not in ALLOWED]
        if wrong:
            bad.append(f"{name}: {fn} is on {wrong}, not {sorted(ALLOWED)}")
        else:
            moved_count += 1
    for fn in sorted(named & set(found)):
        if all(q in ALLOWED for q in found[fn]):
            bad.append(f"{name}: {fn} is named as not moved, but is on {found[fn]}")
        else:
            named_count += 1

want_moved = sum(len(v) for v in MOVED.values())
want_named = sum(len(v) for v in NAMED.values())
print(f"LIFT not keyed to the angle of attack: {len(present)} aircraft of {TOTAL}; "
      f"{moved_count} of {want_moved} terms moved, {named_count} of {want_named} named")
for n, terms in NAMED.items():
    for fn, why in terms.items():
        print(f"not moved: {n} {fn}: {why}")
if len(present) != TOTAL or moved_count != want_moved or named_count != want_named:
    bad.append(f"{len(present)} aircraft of {TOTAL}, {moved_count} terms moved of "
               f"{want_moved}, {named_count} named of {want_named}")
for b in bad:
    print(b, file=sys.stderr)
sys.exit(1 if bad else 0)
