#!/usr/bin/env python3
"""Make the zlib streams the DEFLATE decoder's tests decompress.

    tools/make_inflate_fixtures.py

Writes tests/data/inflate/<name>.zz: payloads compressed by zlib - Python's own
binding - with every strategy zlib has, so the decoder meets fixed and dynamic
Huffman blocks, stored blocks, runs, literal-only streams, flushes, and matches
reaching back the full 32 KB window. The payloads are not stored: they are
made from a seed by a generator that tests/unit/test_inflate.cpp repeats
exactly, and the test compares what it decompresses with what it makes.

Different zlib versions may compress the same payload to different bytes, so
there is no --check: any valid stream of the payload serves. The files record
which zlib made them in tests/data/inflate/README.md.
"""

import pathlib
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "tests" / "data" / "inflate"

PHRASE = b"glideslope flies over the Copernicus DEM at thirty metres a sample "


def payload(size: int, seed: int) -> bytes:
    """Text-like bytes with noise: the same generator as the C++ test's."""
    x = seed
    out = bytearray()
    while len(out) < size:
        x = (x * 6364136223846793005 + 1442695040888963407) % (1 << 64)
        r = x >> 33
        if r % 4 == 0:
            out.append(r & 0xFF)
        else:
            start = (r >> 8) % len(PHRASE)
            length = 1 + (r >> 16) % 40
            out += PHRASE[start:start + length]
    return bytes(out[:size])


def far(size: int, seed: int) -> bytes:
    """32000 bytes of noise, then the noise again from its start: matches 32000
    bytes back, in the last distance code. The same as the C++ test's."""
    x = seed
    noise = bytearray()
    for _ in range(32000):
        x = (x * 6364136223846793005 + 1442695040888963407) % (1 << 64)
        noise.append((x >> 33) & 0xFF)
    out = bytes(noise)
    while len(out) < size:
        out += bytes(noise)
    return out[:size]


def compress(data: bytes, level: int, strategy: int, chunks: int = 1,
             flush: int = zlib.Z_NO_FLUSH) -> bytes:
    c = zlib.compressobj(level, zlib.DEFLATED, 15, 9, strategy)
    out = bytearray()
    step = (len(data) + chunks - 1) // chunks if data else 1
    for i in range(0, max(len(data), 1), step):
        out += c.compress(data[i:i + step])
        if flush != zlib.Z_NO_FLUSH:
            out += c.flush(flush)
    out += c.flush(zlib.Z_FINISH)
    return bytes(out)


# name: (size, seed, level, strategy, chunks, flush); names starting "far" use
# far() for their payload, the rest payload().
FIXTURES = {
    "far_matches": (40000, 10, 9, zlib.Z_DEFAULT_STRATEGY, 1, zlib.Z_NO_FLUSH),
    "empty": (0, 1, 6, zlib.Z_DEFAULT_STRATEGY, 1, zlib.Z_NO_FLUSH),
    "fixed": (20000, 2, 6, zlib.Z_FIXED, 1, zlib.Z_NO_FLUSH),
    "dynamic": (120000, 3, 9, zlib.Z_DEFAULT_STRATEGY, 1, zlib.Z_NO_FLUSH),
    "fast": (50000, 4, 1, zlib.Z_DEFAULT_STRATEGY, 1, zlib.Z_NO_FLUSH),
    "huffman_only": (6000, 5, 6, zlib.Z_HUFFMAN_ONLY, 1, zlib.Z_NO_FLUSH),
    "runs": (20000, 6, 6, zlib.Z_RLE, 1, zlib.Z_NO_FLUSH),
    "stored": (3000, 7, 0, zlib.Z_DEFAULT_STRATEGY, 1, zlib.Z_NO_FLUSH),
    "sync_flushes": (30000, 8, 6, zlib.Z_DEFAULT_STRATEGY, 5, zlib.Z_SYNC_FLUSH),
    "full_flushes": (30000, 9, 6, zlib.Z_DEFAULT_STRATEGY, 5, zlib.Z_FULL_FLUSH),
}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    for name, (size, seed, level, strategy, chunks, flush) in FIXTURES.items():
        data = far(size, seed) if name.startswith("far") else payload(size, seed)
        stream = compress(data, level, strategy, chunks, flush)
        assert zlib.decompress(stream) == data
        (OUT / f"{name}.zz").write_bytes(stream)
        rows.append(f"| `{name}.zz` | {size} | {seed} | {len(stream)} |")
    readme = [
        "# DEFLATE decoder fixtures",
        "",
        "Made by `tools/make_inflate_fixtures.py` with zlib "
        f"{zlib.ZLIB_RUNTIME_VERSION}. Each is a zlib stream of the payload the",
        "script's generator makes from the seed; `tests/unit/test_inflate.cpp` makes",
        "the same payload and compares.",
        "",
        "| File | Payload bytes | Seed | Stream bytes |",
        "| --- | --- | --- | --- |",
        *rows,
        "",
    ]
    (OUT / "README.md").write_text("\n".join(readme))


if __name__ == "__main__":
    main()
