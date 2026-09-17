#!/usr/bin/env python3
"""Make assets/dem/coverage.txt: which 1-degree cells of the Copernicus DEM
have tiles, at 30 m and at 90 m.

    tools/make_dem_coverage.py --lists DIR [--check]

DIR holds each bucket's tile list, as tests/data/downloads/files.txt fetches
them: copernicus-dem-30m-tileList.txt and copernicus-dem-90m-tileList.txt. Their
SHA-256 are pinned below; a list that differs is refused, because a different
list is a different dataset.

The program needs to know, for any point on Earth, whether a 30 m tile covers
it, or only a 90 m one, or neither - which is the sea, whose height the DEM
gives as zero - without asking the network. 25 cells, around Armenia and
Azerbaijan, are in the 90 m set and not the public 30 m one.

--check compares what it would write with the committed file and exits 1 if
they differ.
"""

import argparse
import hashlib
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "dem" / "coverage.txt"

LISTS = {
    "glo30": ("copernicus-dem-30m-tileList.txt",
              "10604e3052c98a09e9216f1a8f0a555a04148419757575f783d4937fd44316dc", "10"),
    "glo90": ("copernicus-dem-90m-tileList.txt",
              "e5a5efe088e70506bc1007d22006bdcb09b0ec03177b62f9652363c13f49ed97", "30"),
}

NAME = re.compile(r"^Copernicus_DSM_COG_(\d\d)_([NS])(\d\d)_00_([EW])(\d\d\d)_00_DEM$")


def cells(path: pathlib.Path, sha256: str, resolution: str) -> set:
    data = path.read_bytes()
    got = hashlib.sha256(data).hexdigest()
    if got != sha256:
        sys.exit(f"{path.name}: SHA-256 {got}, not the pinned {sha256}")
    out = set()
    for line in data.decode().split():
        m = NAME.match(line)
        if not m or m.group(1) != resolution:
            sys.exit(f"{path.name}: unexpected tile name {line!r}")
        lat = int(m.group(3)) * (1 if m.group(2) == "N" else -1)
        lon = int(m.group(5)) * (1 if m.group(4) == "E" else -1)
        out.add((lat, lon))
    return out


def render(glo30: set, glo90: set) -> str:
    lines = [
        "# Which 1-degree cells of the Copernicus DEM have tiles. Made by",
        "# tools/make_dem_coverage.py from each bucket's tileList.txt:",
        f"#   copernicus-dem-30m SHA-256 {LISTS['glo30'][1]}",
        f"#   copernicus-dem-90m SHA-256 {LISTS['glo90'][1]}",
        "# One line per band of latitude, north first, named by the tile naming's",
        "# latitude (N89 is 89 N to 90 N; S01 is 1 S to the equator), then 360 cells",
        "# from 180 W eastwards: 2 has a 30 m tile (and a 90 m one), 1 only a 90 m",
        "# tile, 0 neither - the sea.",
    ]
    for lat in range(89, -91, -1):
        name = f"{'N' if lat >= 0 else 'S'}{abs(lat):02d}"
        row = []
        for lon in range(-180, 180):
            row.append("2" if (lat, lon) in glo30 else "1" if (lat, lon) in glo90 else "0")
        lines.append(f"{name} {''.join(row)}")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lists", required=True, type=pathlib.Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    glo30 = cells(args.lists / LISTS["glo30"][0], LISTS["glo30"][1], LISTS["glo30"][2])
    glo90 = cells(args.lists / LISTS["glo90"][0], LISTS["glo90"][1], LISTS["glo90"][2])
    if not glo30 <= glo90:
        sys.exit("a 30 m tile has no 90 m tile, which this format cannot say")
    text = render(glo30, glo90)
    if args.check:
        if OUT.read_text() != text:
            print(f"{OUT} is not what the tile lists make; run {sys.argv[0]} --lists DIR")
            sys.exit(1)
        print(f"{OUT} matches: {len(glo30)} tiles at 30 m, {len(glo90)} at 90 m")
        return
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text)
    print(f"wrote {OUT}: {len(glo30)} tiles at 30 m, {len(glo90)} at 90 m")


if __name__ == "__main__":
    main()
