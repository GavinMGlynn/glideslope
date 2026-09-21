#!/usr/bin/env python3
"""align_models.py - where each visual model sits on the aeroplane it draws.

A visual model comes from a FlightGear aircraft and is drawn around that
aircraft's own origin; the aeroplane glideslope flies is a JSBSim flight
model, with an origin of its own. This script measures the fixed vector
between the two, per aircraft, into assets/models/alignment.txt. The
committed file is checked against it by a test; to change it, change this
script and run it.

    python3 tools/align_models.py           write assets/models/alignment.txt
    python3 tools/align_models.py --check   exit 1 if what is committed differs

**It measures the meshes, so it runs after tools/make_models.py**: change a
model and this is stale until it is run again, which its own test catches.

It reads only what is already in the repository - assets/models/<id>.mesh and
assets/jsbsim/aircraft/<id>/<id>.xml - so it needs no network and no build.
The test that holds the result reads the same numbers out of a *running*
JSBSim instead, so the two are cross-checked rather than one trusting the
other.

**The model is placed at the flight model's visual reference point.** JSBSim's
<metrics> carries a VRP for exactly this, and every aircraft here defines one.
Anchoring there rather than at the structural origin is most of the answer on
its own: it puts the Cessna 172P's wheels within 0.05 m of its flight model's,
where the structural origin had them 1.03 m out.

**The undercarriage is the contacts the aeroplane rests on.** A flight model's
<contact> points are not all wheels: the A320's include its wingtips, its nose
tip and the top of its fin, and the Short Empire's are the keels of a hull.
Nor does any field tell them apart - the A320's wingtip carries the same
rolling friction as its wheels. Geometry does: the ones it stands on are those
on the hull of the contact set seen from below, which in the body frame, where
+z is down, is the hull of their greatest z, along the span of it the centre
of gravity lies over. That picks out a tricycle's nose and mains, a
taildragger's tail and mains, the A380's nose and its four bogies, and the
flying boat's forward keel and step, and nothing else.

**The fit.** For each of those contacts, the model's own ground contact beneath
it is the lowest model point within a radius of it in plan - three per cent of
the model's length, and never less than 0.3 m. The offset is the mean of the
differences, held to zero across the centreline because the model and the
flight model are both symmetric, and walked until it settles. It is walked
from three starts - nothing at all, the model's own lowest tenth put on the
flight model's undercarriage, and a move straight down - and the one that
settles closest is taken: from a bad start the walk settles in the wrong
place, and the A380, whose model is 22.7 m from its flight model along the
fuselage, is never found from nothing.

**What is left over is recorded too.** A flight model and a visual model of the
same aeroplane do not always agree, and no placement can make them. JSBSim's
747-400 has one main leg a side, 5.5 m out; the aeroplane has two, at 3.7 m
and 11.4 m, and FlightGear's model draws both. Each line's last figure is the
worst distance left between a resting contact and the model beneath it, which
is the distance a test holds that aircraft to.
"""

from __future__ import annotations

import argparse
import math
import pathlib
import struct
import sys
from xml.etree import ElementTree

ROOT = pathlib.Path(__file__).resolve().parent.parent
MODELS = ROOT / "assets" / "models"
JSBSIM = ROOT / "assets" / "jsbsim" / "aircraft"
OUT = MODELS / "alignment.txt"

MESH_MAGIC = b"GSMESH\0"

# JSBSim's length units, to metres.
UNITS = {"IN": 0.0254, "FT": 0.3048, "M": 1.0}

# How far around a contact, in plan, to look for the model's ground beneath
# it: a share of the model's length, and never less than this.
NEAR_SHARE = 0.03
NEAR_LEAST_M = 0.30

# A contact is on the ground plane if it is within this of it.
ON_GROUND_M = 0.02


def read_mesh(path: pathlib.Path):
    """(positions, low, high) in the model's own frame, metres."""
    data = path.read_bytes()
    if data[:len(MESH_MAGIC)] != MESH_MAGIC:
        raise ValueError(f"{path} is not a mesh")
    count = struct.unpack_from("<I", data, 8)[0]
    low = list(struct.unpack_from("<3f", data, 16))
    high = list(struct.unpack_from("<3f", data, 28))
    span = [max(high[i] - low[i], 1e-6) for i in range(3)]
    at = 40
    out = []
    for i in range(count):
        q = struct.unpack_from("<3H", data, at + i * 12)
        out.append(tuple(low[k] + q[k] / 65535.0 * span[k] for k in range(3)))
    return out, low, high


def _triplet(element) -> tuple[float, float, float]:
    scale = UNITS[element.get("unit", "IN").upper()]
    return tuple(float(element.find(a).text) * scale for a in ("x", "y", "z"))


def read_flight_model(model: str):
    """(contacts, centre of gravity, wingspan), in the body frame about the VRP.

    The structural frame JSBSim's <location> uses is +x aft, +y starboard,
    +z up; the body frame is +x forward, +y starboard, +z down.
    """
    root = ElementTree.fromstring((JSBSIM / model / f"{model}.xml").read_bytes())
    metrics = root.find("metrics")
    vrp = _triplet(metrics.find("./location[@name='VRP']"))

    def about_vrp(p):
        return (-(p[0] - vrp[0]), p[1] - vrp[1], -(p[2] - vrp[2]))

    contacts = [(c.get("name"), about_vrp(_triplet(c.find("location"))))
                for c in root.find("ground_reactions").findall("contact")]
    cg = about_vrp(_triplet(root.find("mass_balance")
                            .find("./location[@name='CG']")))
    span = metrics.find("wingspan")
    span_m = float(span.text) * UNITS[span.get("unit", "FT").upper()]
    return contacts, cg, span_m


def resting_on(contacts, cg_x: float) -> list[int]:
    """Which contacts the aeroplane stands on. See the docstring."""
    points = sorted((c[1][0], c[1][2], i) for i, c in enumerate(contacts))
    hull: list[tuple[float, float, int]] = []
    for p in points:
        while len(hull) >= 2:
            (x0, z0, _), (x1, z1, _) = hull[-2], hull[-1]
            # Keep the hull of greatest z: drop a point above the line.
            if (x1 - x0) * (p[1] - z0) - (z1 - z0) * (p[0] - x0) >= 0.0:
                hull.pop()
            else:
                break
        hull.append(p)
    if len(hull) < 2:
        return [p[2] for p in points]
    span = None
    for a, b in zip(hull, hull[1:]):
        if a[0] - 1e-9 <= cg_x <= b[0] + 1e-9:
            span = (a, b)
            break
    if span is None:
        # The weight is off the end of the hull - a flight model whose
        # contacts do not straddle its centre of gravity. The nearest span
        # is the one it would tip onto.
        span = (hull[0], hull[1]) if cg_x < hull[0][0] else (hull[-2], hull[-1])
    (x0, z0, _), (x1, z1, _) = span
    out = []
    for i, contact in enumerate(contacts):
        x, z = contact[1][0], contact[1][2]
        t = (x - x0) / (x1 - x0) if x1 != x0 else 0.0
        if abs(z - (z0 + t * (z1 - z0))) < ON_GROUND_M:
            out.append(i)
    return out


def _beneath(points, target, radius):
    """The lowest model point within `radius` of `target` in plan."""
    best = None
    for p in points:
        if abs(p[0] - target[0]) < radius and abs(p[1] - target[1]) < radius:
            if best is None or p[2] > best[2]:
                best = p
    if best is not None:
        return best
    # Nothing under it at all: the nearest point in space, so the walk has
    # somewhere to step from.
    return min(points, key=lambda p: sum((p[k] - target[k]) ** 2
                                         for k in range(3)))


def _walk(points, targets, start, radius):
    """(offset, what each target is left from the model) from one start."""
    offset = [start[0], 0.0, start[2]]
    matched = []
    for _ in range(40):
        matched = [_beneath(points, [targets[i][k] - offset[k] for k in range(3)],
                            radius)
                   for i in range(len(targets))]
        moved = [sum(targets[i][k] - matched[i][k] for i in range(len(targets)))
                 / len(targets) for k in range(3)]
        moved[1] = 0.0
        if max(abs(moved[k] - offset[k]) for k in range(3)) < 1e-5:
            offset = moved
            break
        offset = moved
    left = [math.dist([targets[i][k] - offset[k] for k in range(3)], matched[i])
            for i in range(len(targets))]
    return offset, left


def measure(model_id: str):
    """What one aircraft's line says, and the contacts it was fitted to."""
    points, low, high = read_mesh(MODELS / f"{model_id}.mesh")
    contacts, cg, _span = read_flight_model(model_id)
    on = resting_on(contacts, cg[0])
    targets = [contacts[i][1] for i in on]
    radius = max(NEAR_SHARE * (high[0] - low[0]), NEAR_LEAST_M)

    lowest = [p for p in points if p[2] >= high[2] - 0.10 * (high[2] - low[2])]
    middle = [sum(p[k] for p in lowest) / len(lowest) for k in range(3)]
    theirs = [sum(t[k] for t in targets) / len(targets) for k in range(3)]
    starts = [
        (0.0, 0.0, 0.0),
        tuple(theirs[k] - middle[k] for k in range(3)),
        (0.0, 0.0, max(t[2] for t in targets) - high[2]),
    ]

    best = None
    for start in starts:
        offset, left = _walk(points, targets, start, radius)
        score = sum(left) / len(left)
        if best is None or score < best[0]:
            best = (score, offset, max(left))
    _, offset, _fitted_worst = best

    # **The height is anchored, not fitted.** A model drawn with its
    # undercarriage down has a tyre as its lowest point, and an aeroplane
    # standing on a runway has every wheel on the ground together, so that
    # point belongs at its lowest wheel contact. There is nothing to fit.
    #
    # Fitting it let the walk trade height against length, because sinking a
    # model brings its contacts nearer the mesh on average when the two
    # disagree about where the undercarriage is. That drew the 747-400 2.15 m
    # into the ground, the B-2 1.67, the F-22 0.86 and the Mosquito 0.69 -
    # the 747-400's model has four bogies where its flight model has three
    # legs, so no fit of three to four can be right, and the walk bought a
    # smaller average by burying it. Only x is fitted now; y is nought by
    # symmetry, and z is this.
    offset = [offset[0], 0.0, max(t[2] for t in targets) - high[2]]
    matched = [_beneath(points, [targets[i][k] - offset[k] for k in range(3)],
                        radius)
               for i in range(len(targets))]
    worst = max(math.dist([targets[i][k] - offset[k] for k in range(3)],
                          matched[i])
                for i in range(len(targets)))

    # The contacts the aeroplane does not rest on are its shape, not its
    # undercarriage: a wingtip, a tailcone, a radome, a propeller tip. They
    # are not fitted to - the undercarriage is what has to meet the ground -
    # but how far the model lies from them is what says the two models are
    # of the same aeroplane, so it is measured and recorded.
    elsewhere = 0.0
    others = [c for i, c in enumerate(contacts) if i not in set(on)]
    for _name, target in others:
        q = [target[k] - offset[k] for k in range(3)]
        elsewhere = max(elsewhere, min(math.dist(p, q) for p in points))
    return (offset, len(on), worst, len(others), elsewhere,
            [contacts[i][0] for i in on])


def shipped() -> list[str]:
    return sorted(p.stem for p in MODELS.glob("*.mesh"))


def written() -> str:
    lines = [
        "# Where each visual model sits on the aeroplane it draws, measured by",
        "# tools/align_models.py; see its docstring for how, and docs/ASSETS.md",
        "# for what it means. A model is drawn at its flight model's visual",
        "# reference point, moved by x, y and z metres in the body frame - +x",
        "# forward, +y starboard, +z down.",
        "#",
        "# `wheels` is how many contacts the aeroplane rests on and `on` the",
        "# worst distance left between one of them and the model beneath it;",
        "# `shape` is how many contacts describe it elsewhere - a wingtip, a",
        "# tailcone, a radome - and `at` the worst distance to the model there.",
        "# Those two distances are what each aircraft is held to.",
        "#",
        "# aircraft  x  y  z  wheels  on  shape  at   (metres)",
    ]
    for model_id in shipped():
        offset, wheels, on, shape, at, _which = measure(model_id)
        lines.append(f"{model_id} {offset[0]:.3f} {offset[1]:.3f} "
                     f"{offset[2]:.3f} {wheels} {on:.3f} {shape} {at:.3f}")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="exit 1 if what is committed differs")
    parser.add_argument("--verbose", action="store_true",
                        help="print what each aircraft was fitted to")
    args = parser.parse_args()

    if args.verbose:
        for model_id in shipped():
            offset, wheels, on, shape, at, which = measure(model_id)
            print(f"{model_id:14s} ({offset[0]:+7.2f},{offset[1]:+6.2f},"
                  f"{offset[2]:+6.2f}) m; {wheels} resting, {on:.2f} m left; "
                  f"{shape} elsewhere, {at:.2f} m; on {', '.join(which)}")
        return 0

    text = written()
    if args.check:
        if not OUT.exists() or OUT.read_text() != text:
            print(f"{OUT.relative_to(ROOT)} is not what tools/align_models.py "
                  f"makes; run: python3 tools/align_models.py", file=sys.stderr)
            return 1
        return 0
    OUT.write_text(text)
    print(f"wrote {OUT.relative_to(ROOT)}: {len(shipped())} aircraft")
    return 0


if __name__ == "__main__":
    sys.exit(main())
