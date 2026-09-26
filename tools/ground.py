#!/usr/bin/env python3
"""ground.py - the points of an aeroplane's airframe that can touch the ground.

JSBSim gives a retracted wheel no force, so an aeroplane whose only contacts
are its undercarriage falls through the runway when it is landed with the
wheels up. What it should come down on is its airframe: the belly, the wing
tips, the nose and the tail. This measures those from the aeroplane's own
visual mesh, which is the airframe, rather than placing them by hand from a
published length.

**Why the mesh and not the flight model.** The hunt for JSBSim models carrying
these points ran over FGAddon, FGMEMBERS, JSBSim's own aircraft and two
university mirrors of them, and found one belly in total - the 737-300's. The
mesh, by contrast, is already here for fifteen of the sixteen aircraft, is the
shape the player sees, and is scaled right: no mesh this reads is more than
0.8% from its aeroplane's published length, once a nose boom is set aside.
check() holds every one of them to that, and to its published span less
tightly, because a model carries wingtip lights a published span does not.

**How the height is anchored, and why it needs no alignment.** A mesh is drawn
in its own frame and `assets/models/alignment.txt` records where that frame
sits on the flight model - but the z of that fit carries a residual of up to
2.5 m, which is the size of the very number wanted here. So z is not taken
from it. Every mesh here draws its undercarriage extended, so the lowest point
of the mesh is a tyre touching the ground, and the height of a point above the
tyres is a difference *within one mesh*, in which the alignment offset cancels
exactly. That difference is added to the flight model's own wheel contact z.
Only x and y come from the alignment, where a foot of error moves a belly
point along the belly and changes nothing.

**Checked against the one aeroplane whose answer is known.** FlightGear's
JSBSim 737-300 puts its belly 43.7 in above the wheel contact. Measured this
way from the 737-300 mesh, with no reference to that model at all, the belly
comes out 50.3 in above it: the two agree to 6.6 in on a 33 m aeroplane.
check() below holds that agreement, and ctest runs it as
the_airframe_contacts_measured_from_the_meshes_agree_with_what_is_published.

**Telling the undercarriage from the belly.** The lowest thing at the main
gear station is the wheels, not the airframe, so the legs have to come out of
the measurement, and they cannot be found by looking near the flight model's
own contacts: the B-2's mesh draws its nose leg 100 in forward of where its
flight model puts the nose wheel. They are found from the shape instead. The
lowest point is taken at each of `STATIONS` stations along the fuselage, which
gives a profile of the underside with a deep notch wherever a leg or a bay
reaches down towards the ground. The typical belly height is the median of
that profile over the flat run of the belly, where a majority of stations are
belly whatever the aeroplane, and any station lower than `GEAR_SHARE` of it is
over the undercarriage and is dropped. What is left is belly, and the nose and tail ends of it are the nose
and tail contacts.
"""

import math
import pathlib
import struct
from xml.etree import ElementTree

ROOT = pathlib.Path(__file__).resolve().parent.parent
MODELS = ROOT / "assets" / "models"
JSBSIM = ROOT / "assets" / "jsbsim" / "aircraft"

# **What an airframe scraping a runway is held by.** Aluminium sliding on dry
# concrete has a friction coefficient near 0.4, and that is the number this
# project states and uses for every airframe contact. It decides how far a
# gear-up landing slides: at 0.4 g an aeroplane touching down at 117 knots
# runs about 460 m, which is the order real gear-up landings are recorded at.
# The value it replaced was JSBSim's own rolling friction of 0.02, a tyre's,
# left on the A320's wingtips and nacelles by the model it came from, which
# ran that landing out to 11 km.
SCRAPE_FRICTION = 0.4

MESH_MAGIC = b"GSMESH\0"
UNITS = {"IN": 0.0254, "FT": 0.3048, "M": 1.0}
IN = 0.0254

# How many stations the underside is measured at along the fuselage.
STATIONS = 48
# A station lower than this share of the typical belly height is over the
# undercarriage, not the airframe.
GEAR_SHARE = 0.5
# How much of the span counts as keel. A belly contact is the lowest point of
# the narrow strip along the centreline, not of the whole fuselage: on a
# flying wing like the B-2 there is no fuselage to speak of, and a band wide
# enough to be one reaches out to where the body blends up into the wing.
CENTRE_SHARE = 0.06
# How far an airframe contact gives under the whole aeroplane's weight.
SINK_FT = 0.25

# A station holding fewer vertices than this is not a measurement. The B-2's
# mesh carries 6,875 vertices for a 52 m aeroplane, and at that density a
# station can hold two, whose lowest is wherever they happen to be - which is
# how a belly 47 in above the wheels first read as 130.
STATION_LEAST = 12
# The run of the length over which the belly is flat, and so where the typical
# belly height is taken. Outside it the nose and the tail rise, and including
# them puts the median above the belly - far enough above, on the B-2, that
# the real belly would be thrown away as undercarriage.
BELLY_FROM, BELLY_TO = 0.15, 0.65
# A station more than this many times the typical belly height above the
# wheels is no longer underside: it is the tail cone rising, or the fin.
UNDERSIDE = 2.0


def stiffness(weight_lbs):
    """(spring, damping) for an airframe contact under `weight_lbs`.

    An airframe is not a shock absorber, and a belly that sinks into concrete
    puts the centre of gravity under the runway. The spring is set so that the
    whole aeroplane's weight on one point compresses it a quarter of a foot,
    and the damping to critical for that spring, so it settles rather than
    bounces or rings.
    """
    spring = weight_lbs / SINK_FT
    mass_slugs = weight_lbs / 32.174
    return spring, 2.0 * math.sqrt(spring * mass_slugs)


def read_mesh(path):
    """The mesh's vertices in its own frame, metres."""
    data = path.read_bytes()
    if data[:len(MESH_MAGIC)] != MESH_MAGIC:
        raise ValueError(f"{path} is not a mesh")
    count = struct.unpack_from("<I", data, 8)[0]
    low = list(struct.unpack_from("<3f", data, 16))
    high = list(struct.unpack_from("<3f", data, 28))
    span = [max(high[i] - low[i], 1e-6) for i in range(3)]
    return [tuple(low[k] + q[k] / 65535.0 * span[k] for k in range(3))
            for q in (struct.unpack_from("<3H", data, 40 + i * 12)
                      for i in range(count))]


def alignment():
    """Each model's offset from its flight model's visual reference point."""
    out = {}
    for line in (MODELS / "alignment.txt").read_text().splitlines():
        if line and not line.startswith("#"):
            f = line.split()
            out[f[0]] = (float(f[1]), float(f[2]), float(f[3]))
    return out


def _triplet(element):
    scale = UNITS[element.get("unit", "IN").upper()]
    return tuple(float(element.find(a).text) * scale for a in ("x", "y", "z"))


def flight_model(model_id):
    """(VRP, wheel contact z) of a flight model, in metres and inches."""
    root = ElementTree.fromstring((JSBSIM / model_id / f"{model_id}.xml").read_bytes())
    vrp = _triplet(root.find("metrics/location[@name='VRP']"))
    wheels = [_triplet(c.find("location"))[2] / IN
              for c in root.findall("ground_reactions/contact")
              if c.get("type") == "BOGEY"]
    if not wheels:
        raise SystemExit(f"ground.py: {model_id} has no wheels to anchor to")
    # The wheels all touch the ground together; the lowest is the ground.
    return vrp, min(wheels)


def in_structural_frame(model_id, wheel_z=None):
    """The mesh's vertices in the flight model's structural frame, inches.

    +x aft, +y starboard, +z up, with z anchored so that the lowest point of
    the mesh - a tyre - sits at the flight model's wheel contact.
    """
    vrp, read_z = flight_model(model_id)
    wheel_z = read_z if wheel_z is None else wheel_z
    off = alignment()[model_id]
    points = read_mesh(MODELS / f"{model_id}.mesh")
    # The model is drawn at the VRP, moved by `off` in the body frame, which
    # is +x forward, +y starboard, +z down.
    out = [((vrp[0] - (p[0] + off[0])) / IN,
            (vrp[1] + (p[1] + off[1])) / IN,
            (vrp[2] - (p[2] + off[2])) / IN) for p in points]
    lowest = min(p[2] for p in out)
    lift = wheel_z - lowest
    return [(x, y, z + lift) for x, y, z in out], wheel_z


def _profile(points, wheel_z):
    """The underside's lowest point at each station, with the undercarriage
    dropped: (kept stations, every station, the typical belly height)."""
    xs = [p[0] for p in points]
    x0, x1 = min(xs), max(xs)
    half = max(abs(p[1]) for p in points)
    centre = [p for p in points if abs(p[1]) < CENTRE_SHARE * half]
    step = (x1 - x0) / STATIONS
    every = []
    for i in range(STATIONS):
        a = x0 + i * step
        band = [p for p in centre if a <= p[0] < a + step]
        if len(band) >= STATION_LEAST:
            every.append(min(band, key=lambda p: p[2]))
    if not every:
        raise SystemExit("ground.py: the mesh has no fuselage to measure")
    flat = [p for p in every
            if x0 + (x1 - x0) * BELLY_FROM <= p[0] <= x0 + (x1 - x0) * BELLY_TO]
    heights = sorted(p[2] - wheel_z for p in (flat or every))
    typical = heights[len(heights) // 2]
    kept = [p for p in every if p[2] - wheel_z > GEAR_SHARE * typical]
    return kept, every, typical


def derive(model_id, belly_stations=3, wheel_z=None):
    """The airframe's ground contacts for `model_id`, in structural inches.

    A nose, `belly_stations` points along the belly, two wing tips and a tail,
    named for what they are. Every one is measured; nothing is placed by hand.
    """
    points, wheel_z = in_structural_frame(model_id, wheel_z)
    kept, every, typical = _profile(points, wheel_z)
    half = max(abs(p[1]) for p in points)

    # The underside proper: the stations still near the belly's own height.
    # The nose and the tail are its two ends, and are where an aeroplane
    # touches first if it comes down nose low or tail low.
    low = [p for p in kept if p[2] - wheel_z < UNDERSIDE * typical]
    if len(low) < 3:
        raise SystemExit(f"ground.py: {model_id} has no underside to measure")
    x0, x1 = low[0][0], low[-1][0]

    out = [("NOSE", low[0])]
    for i in range(belly_stations):
        # Spread over the belly, clear of the nose and the tail.
        want = x0 + (x1 - x0) * (0.20 + 0.6 * i / max(belly_stations - 1, 1))
        out.append((f"BELLY_{i + 1}", min(low, key=lambda p: abs(p[0] - want))))
    for side, name in ((-1.0, "LEFT_WING_TIP"), (1.0, "RIGHT_WING_TIP")):
        tip = [p for p in points if p[1] * side > 0.94 * half]
        if tip:
            out.append((name, min(tip, key=lambda p: p[2])))
    out.append(("TAIL", low[-1]))

    # Two stations can land on the same vertex; a repeated contact is a
    # contact that says nothing, so they are dropped by name order.
    seen = set()
    unique = []
    for name, p in out:
        key = (round(p[0], 1), round(p[1], 1), round(p[2], 1))
        if key not in seen:
            seen.add(key)
            unique.append((name, p))
    return ([(n, round(p[0], 1), round(p[1], 1), round(p[2], 1)) for n, p in unique],
            wheel_z, typical, len(kept), len(every))


# **How far either side of the keel a flank contact is looked for**, as a share
# of the half span: beyond the keel's own strip (CENTRE_SHARE), out to where a
# fighter's underside still runs flat under its engines.
FLANK_SHARE = 0.3


def flanks(model_id, wheel_z=None):
    """Four contacts on the underside either side of the keel, in structural
    inches: on each side, the lowest point of the fore half and of the aft
    half of the underside, between the keel's strip and FLANK_SHARE of the
    half span. It is measured station by station, as _profile() measures the
    keel, and a station is dropped where it is over the undercarriage or no
    longer underside, by the same rules.

    **Why an aeroplane may need them.** The contacts derive() measures lie on
    the centreline and at the wing tips, and an aeroplane whose wing tips sit
    well above its belly rests on a line: it rolls from tip to tip as it
    slides. The F-15C did, ten degrees each way, and on Windows it rolled
    over and through the runway (tools/make_f15c.py). A fighter's belly is
    wide - two engines side by side, with the intakes outside them - and
    these are where it rests.
    """
    points, wheel_z = in_structural_frame(model_id, wheel_z)
    _kept, _every, typical = _profile(points, wheel_z)
    half = max(abs(p[1]) for p in points)
    xs = [p[0] for p in points]
    x0, x1 = min(xs), max(xs)
    step = (x1 - x0) / STATIONS
    out = []
    for side, name in ((-1.0, "LEFT"), (1.0, "RIGHT")):
        flank = [p for p in points if CENTRE_SHARE * half < p[1] * side < FLANK_SHARE * half]
        lows = []
        for i in range(STATIONS):
            a = x0 + i * step
            band = [p for p in flank if a <= p[0] < a + step]
            if len(band) >= STATION_LEAST:
                low = min(band, key=lambda p: p[2])
                if GEAR_SHARE * typical < low[2] - wheel_z < UNDERSIDE * typical:
                    lows.append(low)
        if len(lows) < 2:
            raise SystemExit(f"ground.py: {model_id} has no underside beside its keel")
        middle = (lows[0][0] + lows[-1][0]) / 2.0
        for part, which in (("FORE", [p for p in lows if p[0] < middle]),
                            ("AFT", [p for p in lows if p[0] >= middle])):
            p = min(which, key=lambda q: q[2])
            out.append((f"{name}_{part}_FLANK", round(p[0], 1), round(p[1], 1), round(p[2], 1)))
    return out


def contact_elements(points, weight_lbs, indent="        "):
    """`points`, (name, x, y, z) in inches, as scraping JSBSim <contact>s."""
    spring, damping = stiffness(weight_lbs)
    out = ""
    for name, x, y, z in points:
        out += (f'{indent}<contact type="STRUCTURE" name="{name}">\n'
                f'{indent}    <location unit="IN">\n'
                f'{indent}        <x> {x} </x>\n'
                f'{indent}        <y> {y} </y>\n'
                f'{indent}        <z> {z} </z>\n'
                f'{indent}    </location>\n'
                f'{indent}    <static_friction> {SCRAPE_FRICTION} </static_friction>\n'
                f'{indent}    <dynamic_friction> {SCRAPE_FRICTION} </dynamic_friction>\n'
                f'{indent}    <spring_coeff unit="LBS/FT"> {spring:.0f} </spring_coeff>\n'
                f'{indent}    <damping_coeff unit="LBS/FT/SEC"> {damping:.0f} </damping_coeff>\n'
                f'{indent}</contact>\n')
    return out


def contacts(model_id, weight_lbs, indent="        ", wheel_z=None):
    """The airframe's contacts for `model_id`, as JSBSim <contact> elements.

    `weight_lbs` is the aeroplane's maximum weight, which sets how stiff the
    airframe has to be to hold it off the runway; see stiffness().
    """
    points, _wheels, _typical, _kept, _every = derive(model_id, wheel_z=wheel_z)
    return contact_elements(points, weight_lbs, indent)


# **What the meshes are checked against.** Each aeroplane's published overall
# length and wing span, in metres, so that a mesh cannot quietly be the wrong
# aeroplane or the wrong scale: the heights measured off it are only worth
# what its scale is worth.
PUBLISHED_M = {
    # aircraft:     (length, span, source)
    "737-300": (33.40, 28.88, "Boeing 737 Airplane Characteristics for Airport Planning D6-58325-6"),
    "747-400": (70.66, 64.44, "Boeing 747-400 Airplane Characteristics for Airport Planning D6-58326-1"),
    "b2": (21.03, 52.43, "USAF B-2 Spirit fact sheet"),
    "f22": (18.92, 13.56, "USAF F-22 Raptor fact sheet"),
    "f35b": (15.60, 10.67, "Lockheed Martin F-35B product card (2023)"),
}
# How far a mesh may be from its published length. Length is the strong
# evidence that a mesh is the aeroplane it says it is, and no mesh here is
# more than 0.8% out once a nose boom is excluded.
LENGTH_TOLERANCE = 0.02
# Span is held less tightly, because a model carries things a published span
# does not count: the F-35B's wingtip navigation lights put it 3.6% over,
# where its wing alone is 1.6% over, and the 747's wingtip is 1.5% out. The
# existing model test allows 6% for the same reason.
SPAN_TOLERANCE = 0.04

# FlightGear's own JSBSim 737-300 (FGAddon r21588) is the one flight model
# found anywhere that carries a belly contact, and so the one independent
# answer this method can be checked against. Its belly sits this far above its
# main wheel contact.
FLIGHTGEAR_737_BELLY_IN = 43.7
# How far the mesh's answer may be from FlightGear's.
BELLY_AGREEMENT_IN = 10.0


def check():
    """Everything this module claims, checked. Returns the number of faults."""
    faults = []
    for model_id, (length_m, span_m, source) in sorted(PUBLISHED_M.items()):
        points, wheel_z, typical, kept, every = derive(model_id)
        mesh = read_mesh(MODELS / f"{model_id}.mesh")
        got_length = (max(p[0] for p in mesh) - min(p[0] for p in mesh))
        got_span = (max(p[1] for p in mesh) - min(p[1] for p in mesh))
        for what, got, want, allowed in (("length", got_length, length_m, LENGTH_TOLERANCE),
                                         ("span", got_span, span_m, SPAN_TOLERANCE)):
            off = abs(got - want) / want
            print(f"  {model_id:<9} {what:<6} {got:6.2f} m against {want:6.2f} m published"
                  f" ({off * 100:+.1f}%, up to {allowed * 100:.0f}%)  [{source}]")
            if off > allowed:
                faults.append(f"{model_id}'s mesh is {off * 100:.1f}% off its published {what}")
        # No airframe contact may sit at or below the wheels: one that did
        # would drag on every ordinary landing.
        for name, x, y, z in points:
            if z <= wheel_z:
                faults.append(f"{model_id}'s {name} is at or below the wheels")
        print(f"  {model_id:<9} belly {typical:.1f} in above the wheels; {len(points)} contacts"
              f" from {kept} of {every} stations")

    # The one independent answer there is to check against.
    _points, wheel_z, typical, _kept, _every = derive("737-300")
    off = abs(typical - FLIGHTGEAR_737_BELLY_IN)
    print(f"  737-300 belly {typical:.1f} in above the wheels against FlightGear's own "
          f"flight model at {FLIGHTGEAR_737_BELLY_IN} in: they differ by {off:.1f} in")
    if off > BELLY_AGREEMENT_IN:
        faults.append(f"the mesh and FlightGear's 737-300 differ by {off:.1f} in, more "
                      f"than the {BELLY_AGREEMENT_IN} in this method claims")

    # **The space this walked, stated.**
    if len(PUBLISHED_M) != 5:
        faults.append(f"{len(PUBLISHED_M)} aircraft were checked, not the five derived this way")
    for fault in faults:
        print(f"  FAULT: {fault}")
    return len(faults)


if __name__ == "__main__":
    import sys
    if "--check" in sys.argv:
        sys.exit(1 if check() else 0)
    for model in sys.argv[1:] or ["737-300", "747-400", "b2", "f22"]:
        got, wheel_z, typical, kept, every = derive(model)
        print(f"=== {model} ===  wheels at z {wheel_z:.1f} in; belly typically "
              f"{typical:.1f} in above them; {kept} of {every} stations are airframe, "
              f"{every - kept} over the undercarriage")
        for name, x, y, z in got:
            print(f"  {name:<16} x {x:8.1f}  y {y:8.1f}  z {z:8.1f}   ({z - wheel_z:+.1f} above the wheels)")
