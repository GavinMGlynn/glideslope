#!/usr/bin/env python3
"""make_models.py - glideslope's aircraft visual models, made from FlightGear's.

JSBSim supplies flight dynamics and nothing to look at. The visual models come
from FlightGear's aircraft, whose geometry is AC3D (`.ac`), and for the A380
also 3D Studio (`.3ds`), placed by FlightGear model XML. This script fetches
the pinned files, flattens each aircraft's exterior into one mesh in
glideslope's body frame, and writes it to assets/models/<model>.mesh. The
committed output is checked against it by a test; to change a model, change
this script and run it.

    python3 tools/make_models.py --refresh   re-read the sources and re-pin them
    python3 tools/make_models.py             write the meshes from the cache
    python3 tools/make_models.py --check     exit 1 if what is committed differs

Every file read is pinned in assets/models/sources.txt by URL and SHA-256, in
the four-field form tests/cmake/fetch.cmake reads, so the test fetches exactly
what this script did. `--refresh` is the only mode that reaches the network:
it walks each aircraft's model XML, discovers the files, and rewrites that
list. The other two modes read the cache and fail if a pinned file is missing.

**Which aircraft ship a model, and which do not.** Fourteen of the sixteen do.
FlightGear has no model for the Learjet 35A, and none of the F-35A - only the
F-35B, a different airframe with a lift fan - so those two have none. Eight
state a licence in their own directory. The other six - the A380, B-2, F-15,
F-22, Mosquito and Short Empire - state none at any level, and ship on
FGAddon's project-wide requirement that its content is GPL, which is a policy
and not a grant by the author; see POLICY below, and docs/ASSETS.md, which
records for every model the terms its source states, or that it states none.

**The frames.** An AC3D file from FlightGear is authored with +X aft, +Y up
and +Z to port; this was checked against every model's published span, length
and height, which a test pins. A 3D Studio file in the same aircraft is that
frame turned a quarter circle about X, because 3D Studio puts up along +Z:
+X aft, +Y starboard, +Z up. FlightGear's model XML <offsets> are a frame
again: +x aft, +y starboard, +z up. glideslope's body frame is JSBSim's -
+x forward, +y starboard, +z down - so AC3D geometry is mapped
(x, y, z) -> (-X, -Z, -Y) and 3D Studio geometry (x, y, z) -> (-X, Y, -Z),
each a rotation and not a mirror, and an offset (x, y, z) -> (-x, y, -z).

An <offsets> in a file places everything that file contributes: its own
geometry and its <model> children alike. A320-common.xml is the proof - it
carries an offset and no geometry of its own, so the offset can only be for
its children - and the Short Empire is what it costs to get wrong: its model
XML turns the aeroplane through 180 degrees, and its engines are written in
the frame that turn leaves.

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
flat diffuse colour of its AC3D or 3D Studio material. Liveries are large, separately
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
import struct
import sys
import urllib.error
import urllib.request
from xml.etree import ElementTree

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "models"
SOURCES = OUT / "sources.txt"

# FGAddon has no releases and no tags; a Subversion revision is its version.
FGADDON_REV = "21588"
FGADDON = ("https://sourceforge.net/p/flightgear/fgaddon/"
           f"{FGADDON_REV}/tree/trunk/Aircraft")

# Six aircraft state no licence anywhere in their own directory. FGAddon's
# own requirement is that what it carries is GPL, and the project owner
# decided on 2026-09-20 that those six ship on that requirement. It is a
# policy and not a grant by the author, which is why it is written out in
# full here and quoted per model in docs/ASSETS.md.
POLICY = ("GPL, on FGAddon's project-wide requirement that its content is "
          "GPL - a policy, not a grant stated by the author")

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
    "a380": dict(
        dir="A380", entry="XML/A380.xml",
        include=("XML/Wings/wings.xml", "XML/htp.xml",
                 "XML/Wings/bathtub.xml",
                 "XML/Wings/pylon1.xml", "XML/Wings/pylon2.xml",
                 "XML/Wings/pylon3.xml", "XML/Wings/pylon4.xml",
                 "Engines/XML/engine1.xml", "Engines/XML/engine2.xml",
                 "Engines/XML/engine3.xml", "Engines/XML/engine4.xml"),
        licence=POLICY + "; no licence file at any level of the directory, "
                         "and A380-set.xml names the authors 'Ampere.K, "
                         "I.Cunningham, F.Dalvi, S.Hamilton, et al' and "
                         "states no terms",
    ),
    "f35b": dict(
        dir="F-35B", entry="Models/F-35B.xml",
        include=("Models/Engine.xml", "Models/Gear.xml"),
        objects_out=("antennas",),
        licence="GPL-3.0, the GNU GPL v3 text verbatim in License.txt",
    ),
    "b2": dict(
        dir="B-2", entry="Models/b2-spirit.xml",
        include=(),
        licence=POLICY + "; no licence file at any level, B-2-set.xml names "
                         "the author 'Markus Zojer' and states no terms, and "
                         "readme-spirit.txt is a flying guide",
    ),
    "f15c": dict(
        dir="F-15", entry="Models/F-15C.xml",
        include=(),
        # An F-15C with every station loaded is a payload FlightGear picks,
        # not the airframe: glideslope's flight model flies clean, so its
        # missiles, bombs, rails and tanks are left out, and the ladder is
        # ground equipment.
        objects_out=("aim9-", "aim120-", "aim-7-", "mk84-", "su59-",
                     "lau128-", "adu552-", "tank-", "aircraft-ladder"),
        licence=POLICY + "; no licence file at any level, and README.txt is "
                         "a feature list crediting Richard Harrison that "
                         "states no terms",
    ),
    "f22": dict(
        dir="Lockheed-Martin-FA-22A-Raptor",
        entry="Models/F-22-JSBSIM-Model-File.xml",
        include=(),
        licence=POLICY + "; no licence file at any level, and "
                         "f22-jsbsim-set.xml names the author 'Fabrizio "
                         "Fracaroli' and states no terms",
    ),
    "mosquito-fb6": dict(
        dir="mosquito", entry="Models/Mosquito-FB6.xml",
        # The airframe carries propeller hubs and no blades: the blades are
        # in pdisk.ac, beside the two discs FlightGear blurs them into as
        # the engine speeds up, which are left out.
        include=("Models/pdiskL.xml", "Models/pdiskR.xml"),
        objects_out=("slowpdisk", "fastpdisk"),
        licence=POLICY + "; no licence file at any level, and "
                         "mosquito-fbVI-set.xml names the authors 'Ludovic "
                         "Brenta, Detlef Faber.' and states no terms",
    ),
    "short_s23": dict(
        dir="Short_Empire", entry="Models/Short_Empire.xml",
        include=("Models/propeller.xml", "Models/PegasusXc/PegasusXc.xml",
                 "Models/Exterior/cowling_gills.xml"),
        objects_out=("propblur", "propdisc"),
        licence=POLICY + "; no licence file at any level, though "
                         "Short_Empire-set.xml and Models/Short_Empire.xml "
                         "each carry 'Copyright (C) 2007 - 2025 Anders "
                         "Gidenstam ... This file is licensed under the GPL "
                         "license version 2 or later', which the geometry "
                         "files do not, and AUTHORS credits the propeller "
                         "and engine models to other FlightGear aircraft "
                         "without terms",
    ),
}

# FlightGear has no model of these at all.
NO_MODEL = {
    "learjet35a": "FGAddon has no Learjet of any mark",
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


# --- 3D Studio --------------------------------------------------------------

def parse_3ds(data: bytes):
    """(materials, root object), the same shape parse_ac3d returns.

    The A380 is the one aircraft here whose exterior is not all AC3D: its
    horizontal tailplane, its four pylons and its four engines are 3D Studio
    (.3ds), and without them it is a fuselage and a wing. A .3ds is a tree of
    chunks, each a 16-bit identifier and a 32-bit length that counts its own
    six-byte header; only the chunks geometry needs are read, and the rest
    are stepped over by that length.

    A .3ds holds its vertices in world coordinates and its 0x4160 matrix is
    the object's own axis system, which a reader either ignores or applies
    and then undoes; this ignores it. It carries smoothing groups rather than
    AC3D's crease angle, and takes AC3D's default 45 degrees instead, so a
    3DS surface is smoothed by the angle between its faces like every other.
    """
    materials: list[tuple[float, float, float]] = []
    index_of: dict[str, int] = {}
    root = Object()
    for cid, at, end in _chunks(data, 0, len(data)):
        if cid != 0x4D4D:  # the file
            continue
        for cid, at, end in _chunks(data, at, end):
            if cid != 0x3D3D:  # the scene
                continue
            for cid, at, end in _chunks(data, at, end):
                if cid == 0xAFFF:  # a material
                    name, rgb = _material_3ds(data, at, end)
                    index_of.setdefault(name, len(materials))
                    materials.append(rgb)
                elif cid == 0x4000:  # a named object
                    name, at = _cstring(data, at, end)
                    obj = _mesh_3ds(data, at, end, name, index_of)
                    if obj is not None:
                        root.kids.append(obj)
    return materials, root


def _chunks(data: bytes, at: int, end: int):
    """(identifier, first byte of content, one past its last) for each chunk."""
    while at + 6 <= end:
        cid, size = struct.unpack_from("<HI", data, at)
        if size < 6 or at + size > end:
            return
        yield cid, at + 6, at + size
        at += size


def _cstring(data: bytes, at: int, end: int) -> tuple[str, int]:
    stop = data.find(b"\0", at, end)
    if stop < 0:
        return data[at:end].decode("latin-1"), end
    return data[at:stop].decode("latin-1"), stop + 1


def _material_3ds(data: bytes, at: int, end: int):
    name = ""
    rgb = None
    for cid, a, e in _chunks(data, at, end):
        if cid == 0xA000:  # its name
            name, _ = _cstring(data, a, e)
        elif cid == 0xA020 and rgb is None:  # its diffuse colour
            for cid, a, e in _chunks(data, a, e):
                if rgb is not None:
                    break
                if cid in (0x0011, 0x0012):  # three bytes
                    r, g, b = struct.unpack_from("<3B", data, a)
                    rgb = (r / 255.0, g / 255.0, b / 255.0)
                elif cid in (0x0010, 0x0013):  # three floats
                    rgb = struct.unpack_from("<3f", data, a)
    return name, rgb if rgb is not None else (0.8, 0.8, 0.8)


def _mesh_3ds(data: bytes, at: int, end: int, name: str, index_of: dict):
    obj = Object()
    obj.name = name
    faces: list[tuple[int, int, int]] = []
    groups: list[tuple[int, tuple]] = []
    for cid, at, end in _chunks(data, at, end):
        if cid != 0x4100:  # a triangle mesh
            continue
        for cid, a, e in _chunks(data, at, end):
            if cid == 0x4110:  # its vertices
                count = struct.unpack_from("<H", data, a)[0]
                obj.verts = [struct.unpack_from("<3f", data, a + 2 + i * 12)
                             for i in range(count)]
            elif cid == 0x4120:  # its faces, then which material each has
                count = struct.unpack_from("<H", data, a)[0]
                for i in range(count):
                    x, y, z, _flags = struct.unpack_from("<4H", data,
                                                         a + 2 + i * 8)
                    faces.append((x, y, z))
                for cid, a, e in _chunks(data, a + 2 + count * 8, e):
                    if cid != 0x4130:
                        continue
                    material, a = _cstring(data, a, e)
                    n = struct.unpack_from("<H", data, a)[0]
                    groups.append((index_of.get(material, -1),
                                   struct.unpack_from(f"<{n}H", data, a + 2)))
    if not obj.verts or not faces:
        return None
    material_of = [-1] * len(faces)
    for material, which in groups:
        for face in which:
            if face < len(material_of):
                material_of[face] = material
    obj.surfaces = [(material_of[i], list(face))
                    for i, face in enumerate(faces)]
    return obj


def read_geometry(data: bytes, path: str):
    """(materials, root object, its frame) from whichever format names it."""
    if path.endswith(".3ds"):
        return parse_3ds(data) + (body_from_3ds,)
    return parse_ac3d(data.decode("latin-1")) + (body_from_ac3d,)


# --- the FlightGear model XML -----------------------------------------------

def parse_xml(data: bytes):
    """The root element of a FlightGear XML file.

    A real parser, rather than searching the text for tags: comments go away
    on their own, and Nasal - which is code, and holds anything that looks
    like a tag - is CDATA, so nothing inside it can be mistaken for an
    element. libxml2 through lxml would do as well; expat is in the standard
    library and this script is to have no dependency of its own.
    """
    return ElementTree.fromstring(data)


# Nasal is code, not structure, and FlightGear does not read it as structure
# either. The A380's is not even inside CDATA, so its jetway doors are real
# <door> elements in the tree; nothing under <nasal> is looked at here.
SKIP = ("nasal",)


def _outside_models(element):
    """Every descendant no <model> encloses, in document order.

    A model XML's own geometry is the <path> outside its <model> children,
    which are aircraft of their own: read the 747-400's first <path> without
    this and its geometry is a pushback tug's.
    """
    for child in element:
        if child.tag == "model" or child.tag in SKIP:
            continue
        yield child
        yield from _outside_models(child)


def _everything(element):
    """Every descendant, in document order."""
    for child in element:
        if child.tag in SKIP:
            continue
        yield child
        yield from _everything(child)


def _child_models(element):
    """Every <model> element with no other <model> above it."""
    for child in element:
        if child.tag in SKIP:
            continue
        if child.tag == "model":
            yield child
        else:
            yield from _child_models(child)


def _first(nodes, tag):
    for node in nodes:
        if node.tag == tag:
            return node
    return None


def _text(node) -> str | None:
    return None if node is None else (node.text or "").strip()


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
    if path.endswith((".ac", ".3ds")):
        found.append((path, rot, pos))
        return
    root = parse_xml(files.get(key, spec, path))
    here = path.rsplit("/", 1)[0] if "/" in path else ""

    def resolve(p: str) -> str:
        p = p.strip()
        if p.startswith("Aircraft/"):
            # Absolute from FlightGear's root: drop "Aircraft/<dir>/".
            rest = p.split("/", 2)
            return rest[2] if len(rest) > 2 else p
        return f"{here}/{p}" if here else p

    # A file's own <offsets> place everything that file contributes - its own
    # geometry and its <model> children alike - and are read from outside
    # those children, whose own <path> and <offsets> are theirs. Reading the
    # 747-400's first <path> without that and its geometry is a pushback
    # tug's; placing the Short Empire's children without it and its four
    # engines stand six metres ahead of the bow, because its <offsets> turn
    # the aeroplane through 180 degrees and its children are written in the
    # frame that turn leaves - which its own comment says: "x/y/z ==
    # forward/left/up due to the heading offset".
    outer = list(_outside_models(root))
    offsets = _first(outer, "offsets")
    if offsets is not None:
        rot, pos = _placed(offsets, rot, pos)

    # "empty.ac" is FlightGear's placeholder for a model with no geometry.
    own = _text(_first(outer, "path"))
    if own and own.endswith((".ac", ".3ds", ".xml")) \
            and not own.endswith("empty.ac"):
        walk_model(files, key, spec, resolve(own), rot, pos, found, listing,
                   depth + 1)

    # <PropertyList include="other.xml"> is FlightGear's way of saying "this
    # file is that one": the PA-28-180's model XML is nothing else.
    for node in [root, *_everything(root)]:
        if node.tag == "PropertyList" and "include" in node.attrib:
            walk_model(files, key, spec, resolve(node.attrib["include"]), rot,
                       pos, found, listing, depth)

    allowed = spec.get("include", ())
    for block in _child_models(root):
        inside = list(_everything(block))
        child = _text(_first(inside, "path"))
        if not child:
            continue
        name = _text(_first(inside, "name")) or ""
        target = resolve(child)
        take = target in allowed or child in allowed or name in allowed
        if listing is not None:
            listing.append((depth, name, target, take))
        if not take:
            continue
        rot_child, pos_child = rot, pos
        child_offsets = _first(inside, "offsets")
        if child_offsets is not None:
            rot_child, pos_child = _placed(child_offsets, rot, pos)
        walk_model(files, key, spec, target, rot_child, pos_child, found,
                   listing, depth + 1)


def _placed(offsets, rot, pos):
    """Apply an <offsets> element, in FlightGear's x-aft/y-starboard/z-up frame."""
    inside = list(_everything(offsets))

    def number(tag):
        v = _text(_first(inside, tag))
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


def body_from_3ds(v):
    """3D Studio's +X aft, +Y starboard, +Z up, to the same body frame.

    AC3D is authored with up along +Y and 3D Studio with up along +Z, so a
    .3ds sitting in the same aircraft as a .ac is the AC3D frame turned a
    quarter circle about X - the turn that keeps up pointing up, since the
    other one would put it underground. It shows: read the A380's tailplane
    as if it were AC3D and it is 2 m across and 30 m thick.
    """
    return (-v[0], v[1], -v[2])


def hidden(name: str, out_of: tuple) -> bool:
    """An object FlightGear would not draw here: see the docstring."""
    if "hotspot" in name.lower():
        return True
    return any(name.startswith(prefix) for prefix in out_of)


def triangles_of(root: Object, materials, rot, pos, out_of=(),
                 to_body=body_from_ac3d):
    """(colour, (a, b, c)) for every triangle, in the body frame."""
    out = []

    def walk(obj: Object, loc, spin):
        if hidden(obj.name, out_of):
            return
        here_spin = _ac_mul(spin, obj.rot)
        here_loc = _ac_add(loc, _ac_apply(spin, obj.loc))
        placed = [_ac_add(here_loc, _ac_apply(here_spin, v)) for v in obj.verts]
        body = [mat_apply(rot, to_body(p)) for p in placed]
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
        materials, root, to_body = read_geometry(files.get(key, spec, path),
                                                 path)
        tris += triangles_of(root, materials, rot, pos,
                             spec.get("objects_out", ()), to_body)
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
                  f"{len(found)} geometry file(s)")

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
