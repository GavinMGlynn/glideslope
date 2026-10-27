"""unkeyed_pitch_terms.py - every committed flight model's pitching-moment
terms *not* keyed to the angle of attack are on a dynamic pressure without
the crosswind's sideways part, or are named here with the reason they are
not.

unkeyed_lift_terms.py's sibling for the PITCH axis, as alpha_pitch_terms.py
is alpha_lift_terms.py's. Every aircraft under assets/jsbsim/aircraft/ is
read, every file of it, its XML comments left out. In its PITCH axis, a
function that names a dynamic pressure (any property containing "qbar"),
does not read the angle of attack (tools/alpha_rate.py's functions_of_alpha)
and does not multiply aero/alphadot-rad_sec (alpha_rate_terms.py's) is one of
her pitching-moment terms not keyed to the angle of attack: an elevator's, a
flap's, a stabiliser's, a radiator's or her gear's, her moment at no angle or
with Mach, or her pitch damping. Each must be in MOVED, every dynamic
pressure in it one that leaves out the sideways part of the air:
aero/qbarUW-psf (tools/alpha_rate.py's with_unkeyed_terms_on_qbar_uw); or her
tailplane's pressure made from it, aero/function/qbarUW-tail-psf (the
C172P's and the Mosquito's elevator, the free stream's and her slipstream's
part), which must read aero/qbarUW-psf and not aero/qbar-psf; or the J-3
Cub's aero/function/qbar-induced-psf, her slipstream's pressure of u and her
propeller's induced velocity alone, which must read neither aero/qbar-psf
nor any velocity but u. Or in NAMED, on the whole aero/qbar-psf, with its
reason. Standing in a crosswind such a term on the whole dynamic pressure
counts the crosswind (alpha_rate.with_unkeyed_terms_on_qbar_uw says why it
should not).

The space is stated: sixteen aircraft, the terms of each listed. An aircraft
in the directory not listed, one listed and missing, a term found in neither
list, a listed term not found, a moved term still on the whole dynamic
pressure, or a named one moved, fails.

    python3 unkeyed_pitch_terms.py <assets/jsbsim/aircraft>
"""
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from alpha_rate import functions_of_alpha  # noqa: E402

C = "aero/coefficient/"
M = "aero/moment/"
MOVED = {
    "737-300": [C + "Cmde", C + "Cmq"],
    "747-400": [C + "Cmde", C + "Cmq"],
    "787-8": [M + "Pitch_elevator", M + "Pitch_damp"],
    "a320": [C + "Cmo", C + "CmDe", C + "Cmq"],
    "a380": [C + "Cm0", C + "Cmq", C + "Cmde", C + "Cmflaps"],
    "b2": [C + "Cmq", C + "Cmde"],
    "c172p": [C + "Cmo", C + "Cmq", C + "Cmde", C + "Cmdf"],
    "c182": [C + "Cmo", C + "Cmq", C + "CmDf"],
    "f15c": [C + "CmM", C + "Cmq", C + "Cmde"],
    # Every pitching term of hers not keyed to the angle of attack is keyed to
    # it (alpha_pitch_terms.py): none here.
    "f22": [],
    "f35b": [C + "Cmq", C + "Cmde", C + "Cmdroop"],
    # Her Pitch_elevator was on her slipstream's pressure, of u alone, before
    # this check: it leaves out the crosswind already.
    "j3cub": [M + "Pitch_elevator", M + "Pitch_damp"],
    "learjet35a": [C + "Cm0", C + "Cmq", C + "Cmde", C + "Cmstab", C + "Cmflaps"],
    "mosquito-fb6": [M + "pitch-zero", M + "pitch-elevator", M + "pitch-damping",
                     M + "pitch-flaps", M + "pitch-radiators", M + "pitch-gear"],
    "pa28": [C + "CmDf", C + "Cmq", C + "Cmo", C + "Cmde"],
    "short_s23": [C + "CmDe", C + "CmDf3L", C + "CmDf3R"],
}
# Every such term leaves out the crosswind; none is named.
NAMED = {}
TOTAL = 16
assert len(MOVED) == TOTAL and set(NAMED) <= set(MOVED)
if len(sys.argv) != 2:
    raise SystemExit("usage: unkeyed_pitch_terms.py <assets/jsbsim/aircraft>")
TAIL = "aero/function/qbarUW-tail-psf"
INDUCED = "aero/function/qbar-induced-psf"
ALLOWED = {"aero/qbarUW-psf", TAIL, INDUCED}
AXIS = re.compile(r"<axis name=\"PITCH\"[^>]*>.*?</axis>", re.S)
FUNCTION = re.compile(r"<function name=\"([^\"]+)\">.*?</function>", re.S)
COMMENT = re.compile(r"<!--.*?-->", re.S)
# The velocities that count the sideways air, and its pressure.
SIDEWAYS = ("aero/qbar-psf", "velocities/v-aero-fps", "velocities/vt-fps",
            "aero/qbar-area", "velocities/vc-")


def reads_sideways(name, funcs, seen=()):
    """Whether the model's function `name` reads the sideways air, itself or
    through another of the model's functions it reads."""
    block = funcs[name]
    if any(s in block for s in SIDEWAYS):
        return True
    for other in funcs:
        if other != name and other not in seen and re.search(
                r">\s*" + re.escape(other) + r"\s*<", block):
            if reads_sideways(other, funcs, seen + (name,)):
                return True
    return False


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
    funcs = {m.group(1): m.group(0) for m in FUNCTION.finditer(whole)}
    # A pressure made by the model must itself leave out the crosswind.
    if TAIL in funcs and "aero/qbarUW-psf" not in funcs[TAIL]:
        bad.append(f"{name}: {TAIL} does not read aero/qbarUW-psf")
    for p in (TAIL, INDUCED):
        if p in funcs and reads_sideways(p, funcs):
            bad.append(f"{name}: {p} reads the sideways air")
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
        # A pressure the model makes must be made in it, and checked above.
        wrong = [q for q in found[fn]
                 if q not in ALLOWED or (q != "aero/qbarUW-psf" and q not in funcs)]
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
print(f"PITCH not keyed to the angle of attack: {len(present)} aircraft of {TOTAL}; "
      f"{moved_count} of {want_moved} terms leave out the crosswind, "
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
