#!/usr/bin/env python3
"""Make assets/runways/strips.csv: every runway the collision ground flattens.

    tools/make_runway_strips.py --runways FILE [--check]

FILE is OurAirports' runways.csv at its pinned commit, as
tests/data/downloads/files.txt fetches it (ourairports-runways.csv); its
SHA-256 is pinned below, and a file that differs is refused, because a
different file is different ground.

**Why a made file.** The ground under every runway is made from these strips
(src/world/runway_ground.hpp), on the server and in every client's
prediction, so they belong to the build: a program never fetches the ground,
and two builds with the same strips make the same ground. The planner still
reads the whole runways.csv, fetched, for the runway ends a plan names.

**What is kept**, one line a runway, in the file's order:
  - left out: a closed runway; a helipad (either end's ident beginning H, as
    world::read_runways has one); one on water (its surface beginning WAT or
    holding WATER, in any case); one with neither end placed;
  - both ends placed: their places as the file gives them;
  - one end placed, with its true heading and the runway's length: the other
    is that length along that heading on a sphere of the Earth's mean radius
    (6371008.8 m) - for a runway's few kilometres centimetres from the
    ellipsoid's answer; with no heading or length it is left out;
  - each end's elevation in feet, empty where the file has none;
  - the width in metres, from width_ft, empty where the file has none.
Places are written to 6 decimal places of a degree (about 0.1 m, the file's own
precision), trailing zeros dropped; elevations as the file has them; widths to
0.1 m.

--check compares what it would write with the committed file and exits 1 if
they differ.
"""

import argparse
import csv
import hashlib
import io
import math
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "runways" / "strips.csv"
PINNED = "ae9a7661f230731cb4fef3a291991cd440f8a68593f41d773092798fc6ec9a8c"
MEAN_RADIUS_M = 6371008.8


def number(text: str):
    try:
        v = float(text)
    except ValueError:
        return None
    return v if math.isfinite(v) else None


def placed(lat, lon) -> bool:
    return lat is not None and lon is not None and -90 <= lat <= 90 and -180 <= lon <= 180


def other_end(lat, lon, heading, length_m):
    d = length_m / MEAN_RADIUS_M
    p1, l1, h = math.radians(lat), math.radians(lon), math.radians(heading)
    p2 = math.asin(math.sin(p1) * math.cos(d) + math.cos(p1) * math.sin(d) * math.cos(h))
    l2 = l1 + math.atan2(math.sin(h) * math.sin(d) * math.cos(p1),
                         math.cos(d) - math.sin(p1) * math.sin(p2))
    lon2 = math.degrees(l2)
    lon2 = math.remainder(lon2, 360.0)
    return math.degrees(p2), lon2


def degrees(v: float) -> str:
    text = f"{v:.6f}".rstrip("0").rstrip(".")
    return "0" if text in ("-0", "") else text


def elevation(text: str) -> str:
    v = number(text)
    return "" if v is None else f"{v:g}"


def strips(text: str):
    out = []
    for r in csv.DictReader(io.StringIO(text)):
        airport, le, he = r["airport_ident"], r["le_ident"], r["he_ident"]
        surface = r["surface"].upper()
        if (r["closed"] == "1" or not airport or le.startswith("H") or he.startswith("H")
                or surface.startswith("WAT") or "WATER" in surface):
            continue
        le_lat, le_lon = number(r["le_latitude_deg"]), number(r["le_longitude_deg"])
        he_lat, he_lon = number(r["he_latitude_deg"]), number(r["he_longitude_deg"])
        length_ft = number(r["length_ft"])
        length_m = None if length_ft is None else length_ft * 0.3048
        has_le, has_he = placed(le_lat, le_lon), placed(he_lat, he_lon)
        if has_le and not has_he or has_he and not has_le:
            at = "le_" if has_le else "he_"
            heading = number(r[at + "heading_degT"])
            if heading is None or not 0 <= heading <= 360 or length_m is None or length_m <= 0:
                continue
            if has_le:
                he_lat, he_lon = other_end(le_lat, le_lon, heading, length_m)
            else:
                le_lat, le_lon = other_end(he_lat, he_lon, heading, length_m)
        elif not has_le:
            continue
        width_ft = number(r["width_ft"])
        width = "" if width_ft is None else f"{width_ft * 0.3048:.1f}"
        out.append([airport, le, he, degrees(le_lat), degrees(le_lon),
                    elevation(r["le_elevation_ft"]), degrees(he_lat), degrees(he_lon),
                    elevation(r["he_elevation_ft"]), width])
    return out


def render(rows) -> str:
    buffer = io.StringIO()
    buffer.write("# Every runway the collision ground flattens (src/world/runway_ground.hpp).\n")
    buffer.write("# Made by tools/make_runway_strips.py from OurAirports' runways.csv,\n")
    buffer.write(f"# SHA-256 {PINNED}; see its docstring for what is kept.\n")
    writer = csv.writer(buffer, lineterminator="\n")
    writer.writerow(["airport", "le_ident", "he_ident", "le_latitude_deg", "le_longitude_deg",
                     "le_elevation_ft", "he_latitude_deg", "he_longitude_deg",
                     "he_elevation_ft", "width_m"])
    writer.writerows(rows)
    return buffer.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runways", required=True, type=pathlib.Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = args.runways.read_bytes()
    got = hashlib.sha256(data).hexdigest()
    if got != PINNED:
        sys.exit(f"{args.runways}: SHA-256 {got}, not the pinned {PINNED}")
    rows = strips(data.decode("utf-8"))
    text = render(rows)
    if args.check:
        if not OUT.exists() or OUT.read_text(encoding="utf-8") != text:
            print(f"{OUT} is not what the runways make; run {sys.argv[0]} --runways FILE")
            sys.exit(1)
        print(f"{OUT} matches: {len(rows)} runways")
        return
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {OUT}: {len(rows)} runways, {len(text.encode())} bytes")


if __name__ == "__main__":
    main()
