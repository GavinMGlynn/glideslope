"""cub_gear_springs.py - the committed J-3 Cub's main wheels are sprung at the
rate her shock rings' rating makes, within 1%.

The figure is derived here from its sources, not read from
tools/make_j3cub.py, whose docstring cites them:

  - Aircraft Spruce catalogue p. 306 and Piper Service Memo 12: the J-3's
    ring P/N 1174, 11/16 x 7 1/2 in, 730 lb test, four to an aeroplane.
  - MIL-C-5651D Table III: a Type II ring's test load is its pull at 100%
    elongation, twice a Type I cord's of the same section - two strands.
  - Piper drawing 31423: two rings a strut, each wound twice, four strands.
  - Piper drawing 31513: the tyre rises 1.61 in an inch of the strut's
    extension (measured off the drawing; make_j3cub.py shows the geometry).

The space is her ground contacts with a spring, five: both main wheels are
checked; the tail wheel (a leaf spring, not the rings) and the two wing tips
(structure, not gear) are named as left out. A contact in the model that is
in neither, or one of them missing, fails.

    python3 cub_gear_springs.py <assets/jsbsim/aircraft/j3cub/j3cub.xml>
"""
import math
import re
import sys

STRAND_PULL_AT_100_PERCENT_LBS = 730 / 2
RING_LENGTH_IN = math.pi * (7.5 + 11 / 16)
STRANDS_A_STRUT = 2 * 4
STRANDS_A_RING = 4
TYRE_RISE_PER_STRUT_INCH = 1.61
DERIVED_LBS_PER_FT = (12 * STRANDS_A_STRUT * STRAND_PULL_AT_100_PERCENT_LBS * STRANDS_A_RING
                      / RING_LENGTH_IN / TYRE_RISE_PER_STRUT_INCH ** 2)
TOLERANCE = 0.01

CHECKED = {"LEFT_MAIN", "RIGHT_MAIN"}
LEFT_OUT = {"TAIL": "her tail wheel is on a leaf spring, not the shock rings",
            "LEFT_WING": "a wing tip, structure and not gear",
            "RIGHT_WING": "a wing tip, structure and not gear"}
TOTAL = 5
assert len(CHECKED) + len(LEFT_OUT) == TOTAL

text = open(sys.argv[1]).read()
contacts = dict(re.findall(
    r"<contact type=\"\w+\" name=\"(\w+)\">.*?<spring_coeff\s+unit=\"LBS/FT\">\s*([\d.]+)\s*</spring_coeff>",
    text, flags=re.S))
bad = []
for name in sorted(set(contacts) - CHECKED - set(LEFT_OUT)):
    bad.append(f"{name}: a sprung contact neither checked nor named as left out")
for name in sorted((CHECKED | set(LEFT_OUT)) - set(contacts)):
    bad.append(f"{name}: expected, but not in the model")
covered = 0
for name in sorted(CHECKED & set(contacts)):
    spring = float(contacts[name])
    off = spring / DERIVED_LBS_PER_FT - 1
    print(f"{name}: {spring:.0f} lb/ft against the derived {DERIVED_LBS_PER_FT:.0f} ({off:+.1%})")
    if abs(off) > TOLERANCE:
        bad.append(f"{name}: {spring:.0f} lb/ft is {off:+.1%} from the derived {DERIVED_LBS_PER_FT:.0f}")
    covered += 1
for name, why in LEFT_OUT.items():
    print(f"{name}: left out - {why}")
print(f"{covered} of {len(CHECKED)} main wheels checked; {len(LEFT_OUT)} of {TOTAL} contacts left out, named")
if covered != len(CHECKED):
    bad.append(f"covered {covered} main wheels, expected {len(CHECKED)}")
for b in bad:
    print("FAIL", b)
sys.exit(1 if bad else 0)
