#!/usr/bin/env python3
"""make_models.py - glideslope's aircraft visual models, made from FlightGear's.

JSBSim supplies flight dynamics and nothing to look at. The visual models come
from FlightGear's aircraft, whose geometry is AC3D (`.ac`) placed by FlightGear
model XML. This script fetches the pinned files, flattens each aircraft's
exterior into one mesh in glideslope's body frame, and writes it to
assets/models/<model>.mesh. The committed output is checked against it by a
test; to change a model, change this script and run it.

    python3 tools/make_models.py --refresh   re-read the sources and re-pin them
    python3 tools/make_models.py             write the meshes from the cache
    python3 tools/make_models.py --check     exit 1 if what is committed differs

Every file read is pinned in assets/models/sources.txt by URL and SHA-256, in
the four-field form tests/cmake/fetch.cmake reads, so the test fetches exactly
what this script did. `--refresh` is the only mode that reaches the network:
it walks each aircraft's model XML, discovers the files, and rewrites that
list. The other two modes read the cache and fail if a pinned file is missing.

**Which aircraft ship a model, and which do not.** FlightGear has no model for
the Learjet 35A, and none of the F-35A - only the F-35B, a different airframe
with a lift fan - so those two have none. Six more have a FlightGear model
whose directory states no licence at all: the A380, B-2, F-15, F-22, Mosquito
and Short Empire. FGAddon's own policy is that its aircraft are GPL, but a
policy is not a licence grant by the author, and docs/ASSETS.md records terms
quoted from the source. Those six are listed in UNLICENSED below with what
was looked at, and no model of them ships. That leaves eight.

**The frames.** An AC3D file from FlightGear is authored with +X aft, +Y up
and +Z to port; this was checked against every model's published span, length
and height, which a test pins. FlightGear's model XML <offsets> are a
different frame again: +x aft, +y starboard, +z up. glideslope's body frame is
JSBSim's - +x forward, +y starboard, +z down - so geometry is mapped
(x, y, z) -> (-X, -Z, -Y), a rotation and not a mirror, and an offset
(x, y, z) -> (-x, y, -z).

**What is left out of each mesh, and why.** A FlightGear model XML composes an
airframe out of parts, and most of what it composes is not the airframe: a
variant on the same model (the Cub's XML carries the PA-18 too), optional
kit - skis, floats, a bush kit - damage states, tyre smoke, spray, the
interior and its instruments, a pilot, chocks, tiedowns, a pushback tug, light
cones. FlightGear hides them with animations; this script does not interpret
animations, so following every child would weld all of it to the aeroplane.
The walk is therefore an allow-list: the entry XML's own geometry, and only
those <model> children named in `include`, at every level. Anything not named
is left out, which is why each aircraft's list is short. Interiors are left
out throughout: this ships the exterior, and nothing draws a cockpit yet.

Some of it is not in the XML at all but inside the .ac, as objects FlightGear
hides: safety cones standing under the 737's and 747's wings, the Cessna
182's chocks, pitot cover and winter kit, the 172's cowl plugs and tiedown
ropes. Those are named per aircraft in `objects_out`. The 182's cones set its
span 7.6% over the published figure, which is how they were found. Objects
whose name holds "hotspot" go everywhere: they are FlightGear's invisible
boxes for the mouse to hit, and are geometry like any other here.

**What no mesh carries.** No texture, and so no livery: a surface takes the
flat diffuse colour of its AC3D material. Liveries are large, separately
licensed and would need a texture path through the renderer. No animation:
control surfaces, gear and propellers are welded where the model has them,
gear down. Normals are smoothed within an object across faces meeting at less
than its crease angle, and are flat across sharper edges than that.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import os
import pathlib
import re
import struct
import sys
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "models"
SOURCES = OUT / "sources.txt"

# FGAddon has no releases and no tags; a Subversion revision is its version.
FGADDON_REV = "21588"
FGADDON = ("https://sourceforge.net/p/flightgear/fgaddon/"
           f"{FGADDON_REV}/tree/trunk/Aircraft")

# The aircraft that ship a model: our model name -> where it comes from.
#
#   dir      the aircraft's directory, in FGAddon or in its own repository
#   entry    the model XML the walk starts at, under that directory
#   include  the <model> children that are part of the airframe (see above);
#            a child is named by its <name>, or by its path as written
#   objects_out  AC3D object names, as prefixes, to leave out of the geometry
#   licence  what the source states, and the file it states it in
AIRCRAFT = {
    "c172p": dict(
        repo="c172p-team/c172p",
        commit="84477612bba340ab98004a10f8b28a81c18e6169",
        entry="Models/c172p.xml",
        include=(),
        objects_out=("PropellerCowlPlugs", "TieDown"),
        licence="GPL-2.0, the GNU GPL v2 text verbatim in LICENSE",
    ),
    "c182": dict(
        dir="c182s", entry="Models/c182s.xml",
        include=(),
        objects_out=("safety-cone", "chokes", "pitotcover", "winterkit", "TieDown"),
        licence="GPL-2.0, the GNU GPL v2 text verbatim in LICENSE",
    ),
    "pa28": dict(
        dir="PA28", entry="Models/PA28-161-180.xml",
        include=(),
        licence="GPL-2.0, the GNU GPL v2 text verbatim in LICENSE",
    ),
    "j3cub": dict(
        dir="J3Cub", entry="Models/J3Cub.xml",
        include=(),
        licence="GPL-3.0, the GNU GPL v3 text verbatim in copying.txt, and "
                "readme.txt: 'License: GPL (see file \"COPYING.txt\" for details)'",
    ),
    "737-300": dict(
        dir="737-300", entry="Models/737-300.xml",
        include=(),
        objects_out=("Cone",),
        licence="GPL-3.0, the GNU GPL v3 text verbatim in LICENSE.md, and "
                "README.md: 'This is the 737-300 in Progress and under GNU GPL v3.0'",
    ),
    "747-400": dict(
        dir="747-400", entry="Models/747-400.xml",
        include=("Models/747-400_fuselage.xml", "Models/747-400_gear.xml",
                 "Models/747-400_wings.xml", "Models/GE_CF6-80C2B1F.xml"),
        objects_out=("Cone",),
        licence="GPL-2.0, the GNU GPL v2 text verbatim in COPYING",
    ),
    "787-8": dict(
        dir="787-8", entry="Models/787-8.xml",
        include=(),
        licence="GPL-2.0, the GNU GPL v2 text verbatim in COPYING",
    ),
    "a320": dict(
        dir="A320-family", entry="Models/A320-200-CFM.xml",
        include=("Models/A320-common.xml", "Models/Fuselage/a320.cfm.xml",
                 "Models/Fuselage/fuselage.xml"),
        licence="GPL-2.0, the GNU GPL v2 text verbatim in LICENSE",
    ),
}

# A FlightGear model exists, but its directory states no licence: not shipped.
# The value is what was looked at, for docs/ASSETS.md to quote.
UNLICENSED = {
    "a380": ("A380", "no COPYING, LICENSE, README or other licence file at any "
                     "level of the directory; A380-set.xml names the authors "
                     "'Ampere.K, I.Cunningham, F.Dalvi, S.Hamilton, et al' and "
                     "states no terms"),
    "b2": ("B-2", "no licence file at any level; B-2-set.xml names the author "
                  "'Markus Zojer' and states no terms"),
    "f15c": ("F-15", "no licence file at any level; README.txt is a feature "
                     "list crediting Richard Harrison and states no terms"),
    "f22": ("Lockheed-Martin-FA-22A-Raptor",
            "no licence file at any level; f22-jsbsim-set.xml names the author "
            "'Fabrizio Fracaroli' and states no terms"),
    "mosquito-fb6": ("mosquito", "no licence file at any level; "
                                 "mosquito-fbVI-set.xml names the authors "
                                 "'Ludovic Brenta, Detlef Faber.' and states "
                                 "no terms"),
    "short_s23": ("Short_Empire", "no licence file at any level; AUTHORS "
                                  "credits Anders Gidenstam and the authors "
                                  "whose work the model borrows, and states "
                                  "no terms"),
}

# FlightGear has no model of these at all.
NO_MODEL = {
    "learjet35a": "FGAddon has no Learjet of any mark",
    "f35a": "FGAddon has an F-35B, a different airframe with a lift fan, and "
            "no F-35A",
}

MESH_MAGIC = b"GSMESH\0"
MESH_VERSION = 1


# --- fetching, and the pinned list ------------------------------------------

def cache_dir(given: str | None) -> pathlib.Path:
    if given:
        return pathlib.Path(given)
    env = os.environ.get("GLIDESLOPE_DOWNLOADS")
    if env:
        return pathlib.Path(env)
    return ROOT / "build" / "downloads"


def pinned_name(key: str, path: str) -> str:
    """The flat name a pinned file is cached under."""
    return f"fgmodel-{key}-" + path.replace("/", "_")


def url_for(spec: dict, path: str) -> str:
    if "repo" in spec:
        return (f"https://raw.githubusercontent.com/{spec['repo']}/"
                f"{spec['commit']}/{path}")
    return f"{FGADDON}/{spec['dir']}/{path}?format=raw"


def download(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "glideslope"})
    with urllib.request.urlopen(request, timeout=180) as response:
        return response.read()


def read_sources() -> dict[str, tuple[int, str, str]]:
    if not SOURCES.exists():
        return {}
    out = {}
    for line in SOURCES.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        name, size, sha, url = line.split()
        out[name] = (int(size), sha, url)
    return out


def write_sources(files: dict[str, tuple[int, str, str]]) -> None:
    lines = ["# The FlightGear files glideslope's visual models are made from,",
             "# each pinned by SHA-256: name, size in bytes, SHA-256, URL.",
             "# Written by tools/make_models.py --refresh; see docs/ASSETS.md",
             "# for each model's source, revision and licence."]
    for name in sorted(files):
        size, sha, url = files[name]
        lines.append(f"{name} {size} {sha} {url}")
    SOURCES.parent.mkdir(parents=True, exist_ok=True)
    SOURCES.write_text("\n".join(lines) + "\n")


class Files:
    """The pinned files, from the cache, or from the network when refreshing."""

    def __init__(self, cache: pathlib.Path, refresh: bool) -> None:
        self.cache = cache
        self.refresh = refresh
        self.pinned = read_sources()
        self.used: dict[str, tuple[int, str, str]] = {}

    def get(self, key: str, spec: dict, path: str) -> bytes:
        # A relative path in a FlightGear model XML is resolved against the
        # XML's own directory, and failing that against the aircraft's, which
        # is how "Models/Interior/..." appears inside Models/J3Cub.xml.
        candidates = [path]
        if "/" in path:
            trimmed = path.split("/", 1)[1]
            if trimmed != path:
                candidates.append(trimmed)
        last = None
        for candidate in candidates:
            try:
                return self._one(key, spec, candidate)
            except (FileNotFoundError, urllib.error.HTTPError) as e:
                last = e
        raise last

    def _one(self, key: str, spec: dict, path: str) -> bytes:
        name = pinned_name(key, path)
        url = url_for(spec, path)
        local = self.cache / name
        if local.exists():
            data = local.read_bytes()
        elif self.refresh:
            data = download(url)
            self.cache.mkdir(parents=True, exist_ok=True)
            local.write_bytes(data)
        else:
            raise FileNotFoundError(
                f"{local} is missing; fetch assets/models/sources.txt with "
                f"tests/cmake/fetch.cmake, or run with --refresh")
        sha = hashlib.sha256(data).hexdigest()
        if not self.refresh:
            want = self.pinned.get(name)
            if want is None:
                raise ValueError(f"{name} is not pinned in {SOURCES}")
            if want[1] != sha:
                raise ValueError(f"{name} is not what is pinned: {sha}, "
                                 f"not {want[1]}")
        self.used[name] = (len(data), sha, url)
        return data


# --- AC3D -------------------------------------------------------------------

class Ac3dReader:
    """A reader for the AC3D text format, enough of one for FlightGear's models.

    `data N` counts bytes and those bytes may hold newlines, so the file is
    walked by offset rather than split into lines.
    """

    def __init__(self, text: str) -> None:
        self.text = text
        self.at = 0

    def line(self) -> str | None:
        if self.at >= len(self.text):
            return None
        end = self.text.find("\n", self.at)
        if end < 0:
            end = len(self.text)
        line = self.text[self.at:end]
        self.at = end + 1
        return line

    def skip(self, n: int) -> None:
        self.at += n
        if self.at < len(self.text) and self.text[self.at] == "\n":
            self.at += 1


def _values(words: list[str], key: str, n: int) -> list[float]:
    if key not in words:
        return []
    i = words.index(key)
    return [float(w) for w in words[i + 1:i + 1 + n]]


def parse_ac3d(text: str):
    """(materials, root object). A material is (r, g, b); see `Object`."""
    if not text.startswith("AC3D"):
        raise ValueError("not an AC3D file")
    reader = Ac3dReader(text)
    reader.line()
    materials: list[tuple[float, float, float]] = []
    while True:
        line = reader.line()
        if line is None:
            raise ValueError("no OBJECT in the file")
        words = line.split()
        if not words:
            continue
        if words[0] == "MATERIAL":
            rgb = _values(words, "rgb", 3)
            materials.append((rgb[0], rgb[1], rgb[2]) if len(rgb) == 3
                             else (0.8, 0.8, 0.8))
        elif words[0] == "OBJECT":
            return materials, _read_object(reader)


class Object:
    __slots__ = ("name", "loc", "rot", "crease", "verts", "surfaces", "kids")

    def __init__(self) -> None:
        self.name = ""
        self.loc = (0.0, 0.0, 0.0)
        self.rot = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
        self.crease = 45.0
        self.verts: list[tuple[float, float, float]] = []
        # (material, [vertex index])
        self.surfaces: list[tuple[int, list[int]]] = []
        self.kids: list[Object] = []


def _read_object(reader: Ac3dReader) -> Object:
    obj = Object()
    while True:
        line = reader.line()
        if line is None:
            return obj
        words = line.split()
        if not words:
            continue
        head = words[0]
        if head == "name":
            obj.name = line.split('"')[1] if '"' in line else words[1]
        elif head == "data":
            reader.skip(int(words[1]))
        elif head == "loc":
            obj.loc = (float(words[1]), float(words[2]), float(words[3]))
        elif head == "rot":
            obj.rot = tuple(float(w) for w in words[1:10])
        elif head == "crease":
            obj.crease = float(words[1])
        elif head == "numvert":
            for _ in range(int(words[1])):
                v = reader.line().split()
                obj.verts.append((float(v[0]), float(v[1]), float(v[2])))
        elif head == "numsurf":
            for _ in range(int(words[1])):
                surface = _read_surface(reader)
                if surface is not None:
                    obj.surfaces.append(surface)
        elif head == "kids":
            for _ in range(int(words[1])):
                while True:
                    kid = reader.line()
                    if kid is None:
                        return obj
                    kw = kid.split()
                    if kw and kw[0] == "OBJECT":
                        obj.kids.append(_read_object(reader))
                        break
            return obj


def _read_surface(reader: Ac3dReader):
    material, flags, refs = -1, 0, []
    while True:
        line = reader.line()
        if line is None:
            break
        words = line.split()
        if not words:
            continue
        if words[0] == "SURF":
            flags = int(words[1], 0)
        elif words[0] == "mat":
            material = int(words[1])
        elif words[0] == "refs":
            for _ in range(int(words[1])):
                refs.append(int(reader.line().split()[0]))
            break
    # The low nibble is the type: 0 a polygon, 1 a closed line, 2 a line.
    if (flags & 0x0F) != 0 or len(refs) < 3:
        return None
    return material, refs


# --- the FlightGear model XML -----------------------------------------------

def _xml_blocks(text: str, tag: str):
    """Every <tag>...</tag> in `text`, at the top nesting level for that tag."""
    out = []
    at = 0
    open_tag, close_tag = f"<{tag}>", f"</{tag}>"
    while True:
        start = text.find(open_tag, at)
        if start < 0:
            return out
        depth, scan = 1, start + len(open_tag)
        while depth:
            nxt_open = text.find(open_tag, scan)
            nxt_close = text.find(close_tag, scan)
            if nxt_close < 0:
                return out
            if 0 <= nxt_open < nxt_close:
                depth += 1
                scan = nxt_open + len(open_tag)
            else:
                depth -= 1
                scan = nxt_close + len(close_tag)
        out.append(text[start + len(open_tag):scan - len(close_tag)])
        at = scan


def _xml_value(text: str, tag: str) -> str | None:
    start = text.find(f"<{tag}>")
    if start < 0:
        return None
    end = text.find(f"</{tag}>", start)
    if end < 0:
        return None
    return text[start + len(tag) + 2:end].strip()


def _strip_comments(text: str) -> str:
    out, at = [], 0
    while True:
        start = text.find("<!--", at)
        if start < 0:
            out.append(text[at:])
            return "".join(out)
        out.append(text[at:start])
        end = text.find("-->", start)
        if end < 0:
            return "".join(out)
        at = end + 3


def _strip_nasal(text: str) -> str:
    """Nasal is code, and can hold anything that looks like a tag."""
    for block in _xml_blocks(text, "nasal"):
        text = text.replace(block, "")
    return text


def rotation(heading_deg: float, pitch_deg: float, roll_deg: float):
    """Rz(heading) Ry(pitch) Rx(roll) in the body frame, row-major 3x3."""
    ch, sh = math.cos(math.radians(heading_deg)), math.sin(math.radians(heading_deg))
    cp, sp = math.cos(math.radians(pitch_deg)), math.sin(math.radians(pitch_deg))
    cr, sr = math.cos(math.radians(roll_deg)), math.sin(math.radians(roll_deg))
    return (
        ch * cp, ch * sp * sr - sh * cr, ch * sp * cr + sh * sr,
        sh * cp, sh * sp * sr + ch * cr, sh * sp * cr - ch * sr,
        -sp, cp * sr, cp * cr,
    )


IDENTITY = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)


def mat_mul(a, b):
    return tuple(sum(a[r * 3 + k] * b[k * 3 + c] for k in range(3))
                 for r in range(3) for c in range(3))


def mat_apply(m, v):
    return (m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
            m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
            m[6] * v[0] + m[7] * v[1] + m[8] * v[2])


def walk_model(files: Files, key: str, spec: dict, path: str, rot, pos,
               found: list, listing: list | None = None, depth: int = 0):
    """Collect (.ac path, rotation, translation) down the allowed children.

    `listing`, when given, gathers every child the walk saw and whether it was
    taken, which --list prints so that an aircraft's `include` can be written.
    """
    if path.endswith(".ac"):
        found.append((path, rot, pos))
        return
    text = _strip_nasal(_strip_comments(
        files.get(key, spec, path).decode("utf-8", "replace")))
    here = path.rsplit("/", 1)[0] if "/" in path else ""

    def resolve(p: str) -> str:
        p = p.strip()
        if p.startswith("Aircraft/"):
            # Absolute from FlightGear's root: drop "Aircraft/<dir>/".
            rest = p.split("/", 2)
            return rest[2] if len(rest) > 2 else p
        return f"{here}/{p}" if here else p

    # A model XML's own <path> is its own geometry, placed by its own
    # <offsets>; "empty.ac" is FlightGear's placeholder for none. Its
    # children's <path> and <offsets> are theirs, so they go first: the
    # 747's own geometry is named after a pushback tug's.
    outer = text
    for block in _xml_blocks(text, "model"):
        outer = outer.replace(block, "")
    own = _xml_value(outer, "path")
    if own and own.endswith((".ac", ".xml")) and not own.endswith("empty.ac"):
        rot_here, pos_here = rot, pos
        for block in _xml_blocks(outer, "offsets")[:1]:
            rot_here, pos_here = _placed(block, rot, pos)
        walk_model(files, key, spec, resolve(own), rot_here, pos_here, found,
                   listing, depth + 1)

    # <PropertyList include="other.xml"> is FlightGear's way of saying "this
    # file is that one": the PA-28-180's model XML is nothing else.
    for other in re.findall(r'<PropertyList[^>]*\binclude="([^"]+)"', text):
        walk_model(files, key, spec, resolve(other), rot, pos, found, listing,
                   depth)

    allowed = spec.get("include", ())
    for block in _xml_blocks(text, "model"):
        child = _xml_value(block, "path")
        if not child:
            continue
        name = _xml_value(block, "name") or ""
        target = resolve(child)
        take = target in allowed or child.strip() in allowed or name in allowed
        if listing is not None:
            listing.append((depth, name, target, take))
        if not take:
            continue
        rot_child, pos_child = rot, pos
        for offsets in _xml_blocks(block, "offsets")[:1]:
            rot_child, pos_child = _placed(offsets, rot, pos)
        walk_model(files, key, spec, target, rot_child, pos_child, found,
                   listing, depth + 1)


def _placed(offsets: str, rot, pos):
    """Apply an <offsets> block, in FlightGear's x-aft/y-starboard/z-up frame."""
    def number(tag):
        v = _xml_value(offsets, tag)
        return float(v) if v else 0.0

    # To the body frame: x forward, y starboard, z down.
    delta = (-number("x-m"), number("y-m"), -number("z-m"))
    turn = rotation(number("heading-deg"), number("pitch-deg"),
                    number("roll-deg"))
    moved = mat_apply(rot, delta)
    return mat_mul(rot, turn), (pos[0] + moved[0], pos[1] + moved[1],
                                pos[2] + moved[2])


# --- the mesh ---------------------------------------------------------------

def body_from_ac3d(v):
    """AC3D's +X aft, +Y up, +Z port to the body frame's forward, starboard, down."""
    return (-v[0], -v[2], -v[1])


def hidden(name: str, out_of: tuple) -> bool:
    """An object FlightGear would not draw here: see the docstring."""
    if "hotspot" in name.lower():
        return True
    return any(name.startswith(prefix) for prefix in out_of)


def triangles_of(root: Object, materials, rot, pos, out_of=()):
    """(colour, (a, b, c)) for every triangle, in the body frame."""
    out = []

    def walk(obj: Object, loc, spin):
        if hidden(obj.name, out_of):
            return
        here_spin = _ac_mul(spin, obj.rot)
        here_loc = _ac_add(loc, _ac_apply(spin, obj.loc))
        placed = [_ac_add(here_loc, _ac_apply(here_spin, v)) for v in obj.verts]
        body = [mat_apply(rot, body_from_ac3d(p)) for p in placed]
        body = [(b[0] + pos[0], b[1] + pos[1], b[2] + pos[2]) for b in body]
        for material, refs in obj.surfaces:
            colour = materials[material] if 0 <= material < len(materials) \
                else (0.8, 0.8, 0.8)
            for i in range(1, len(refs) - 1):
                tri = (body[refs[0]], body[refs[i]], body[refs[i + 1]])
                out.append((colour, obj.crease, tri))
        for kid in obj.kids:
            walk(kid, here_loc, here_spin)

    walk(root, (0.0, 0.0, 0.0), IDENTITY)
    return out


def _ac_mul(a, b):
    # AC3D writes its 3x3 column-major.
    return tuple(sum(a[k * 3 + r] * b[c * 3 + k] for k in range(3))
                 for c in range(3) for r in range(3))


def _ac_apply(m, v):
    return (m[0] * v[0] + m[3] * v[1] + m[6] * v[2],
            m[1] * v[0] + m[4] * v[1] + m[7] * v[2],
            m[2] * v[0] + m[5] * v[1] + m[8] * v[2])


def _ac_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def face_normal(tri):
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = tri
    ux, uy, uz = bx - ax, by - ay, bz - az
    vx, vy, vz = cx - ax, cy - ay, cz - az
    nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    length = math.sqrt(nx * nx + ny * ny + nz * nz)
    if length < 1e-12:
        return None
    return (nx / length, ny / length, nz / length)


def build_mesh(tris):
    """Vertices (position, normal, colour) and indices, welded where smooth."""
    faces = []
    for colour, crease, tri in tris:
        normal = face_normal(tri)
        if normal is not None:
            faces.append((colour, crease, tri, normal))

    # The normals at a place, to smooth across faces that meet gently.
    at_place: dict[tuple, list[tuple[float, float, float]]] = {}
    for _colour, _crease, tri, normal in faces:
        for p in tri:
            at_place.setdefault(_round(p), []).append(normal)

    vertices: list[tuple] = []
    indices: list[int] = []
    index_of: dict[tuple, int] = {}
    for colour, crease, tri, normal in faces:
        limit = math.cos(math.radians(max(crease, 1.0)))
        for p in tri:
            near = at_place[_round(p)]
            sx = sy = sz = 0.0
            for n in near:
                if n[0] * normal[0] + n[1] * normal[1] + n[2] * normal[2] >= limit:
                    sx, sy, sz = sx + n[0], sy + n[1], sz + n[2]
            length = math.sqrt(sx * sx + sy * sy + sz * sz)
            smooth = (sx / length, sy / length, sz / length) if length > 1e-9 \
                else normal
            key = (_round(p), _round(smooth, 3), tuple(round(c, 3) for c in colour))
            got = index_of.get(key)
            if got is None:
                got = len(vertices)
                index_of[key] = got
                vertices.append((p, smooth, colour))
            indices.append(got)
    return vertices, indices


def _round(v, places=6):
    return (round(v[0], places), round(v[1], places), round(v[2], places))


def write_mesh(vertices, indices) -> bytes:
    """The committed form: see src/gfx/model.hpp, which reads it.

    Positions are quantised to 16 bits over the model's own bounding box - a
    millimetre on a 70 m aeroplane - because these are the largest files the
    repository carries and a float32 would double them for precision no eye
    can use. Normals are signed bytes, colours unsigned.
    """
    lo = [min(v[0][i] for v in vertices) for i in range(3)]
    hi = [max(v[0][i] for v in vertices) for i in range(3)]
    span = [max(hi[i] - lo[i], 1e-6) for i in range(3)]
    out = bytearray()
    out += MESH_MAGIC
    out += struct.pack("<B", MESH_VERSION)
    out += struct.pack("<II", len(vertices), len(indices))
    out += struct.pack("<6f", lo[0], lo[1], lo[2], hi[0], hi[1], hi[2])
    for position, normal, colour in vertices:
        for i in range(3):
            q = int(round((position[i] - lo[i]) / span[i] * 65535.0))
            out += struct.pack("<H", min(max(q, 0), 65535))
        for i in range(3):
            q = int(round(normal[i] * 127.0))
            out += struct.pack("<b", min(max(q, -127), 127))
        for i in range(3):
            q = int(round(colour[i] * 255.0))
            out += struct.pack("<B", min(max(q, 0), 255))
    for i in indices:
        out += struct.pack("<I", i)
    return bytes(out)


# --- making them ------------------------------------------------------------

def make(files: Files, key: str, spec: dict, listing: list | None = None):
    """The mesh bytes, and what went into them."""
    found: list = []
    walk_model(files, key, spec, spec["entry"], IDENTITY, (0.0, 0.0, 0.0),
               found, listing)
    if not found:
        if listing is None:
            raise ValueError(f"{key}: the walk found no .ac geometry")
        return b"", 0, 0, found
    tris = []
    for path, rot, pos in found:
        text = files.get(key, spec, path).decode("latin-1")
        materials, root = parse_ac3d(text)
        tris += triangles_of(root, materials, rot, pos,
                             spec.get("objects_out", ()))
    vertices, indices = build_mesh(tris)
    return write_mesh(vertices, indices), len(vertices), len(indices) // 3, found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="exit 1 if what is committed differs")
    parser.add_argument("--refresh", action="store_true",
                        help="reach the network and re-pin the sources")
    parser.add_argument("--cache", help="where the pinned files are kept")
    parser.add_argument("--only", help="one model, for working on it")
    parser.add_argument("--list", action="store_true",
                        help="print the <model> children the walk saw, and "
                             "whether `include` took them")
    args = parser.parse_args()

    files = Files(cache_dir(args.cache), args.refresh)
    wanted = {args.only: AIRCRAFT[args.only]} if args.only else AIRCRAFT
    stale = []
    for key, spec in wanted.items():
        listing: list | None = [] if args.list else None
        try:
            data, verts, tris, found = make(files, key, spec, listing)
        except FileNotFoundError as e:
            print(f"{key}: {e}", file=sys.stderr)
            return 77
        if listing is not None:
            print(f"== {key}: {spec['entry']}")
            for depth, name, target, take in listing:
                print(f"  {'  ' * depth}{'[x]' if take else '[ ]'} "
                      f"{name or '-'}: {target}")
            print(f"  geometry taken: {[f[0] for f in found]}")
            continue
        path = OUT / f"{key}.mesh"
        if args.check:
            if not path.exists() or path.read_bytes() != data:
                stale.append(path.relative_to(ROOT))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            print(f"wrote {path.relative_to(ROOT)}: {tris} triangles, "
                  f"{verts} vertices, {len(data) / 1024:.0f} KiB, from "
                  f"{len(found)} .ac file(s)")

    if args.refresh and not args.only:
        write_sources(files.used)
        print(f"pinned {len(files.used)} files in "
              f"{SOURCES.relative_to(ROOT)}")
    if stale:
        print("assets/models/ is not what tools/make_models.py makes:",
              file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print("run: python3 tools/make_models.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
