### Reading the Copernicus DEM's GeoTIFF tiles, 2026-09-18

**What is missing first:** there is no height query yet. Nothing converts the
DEM's heights, which are above the EGM2008 geoid, to heights above the WGS84
ellipsoid; nothing fetches tiles at run time; nothing knows which tile covers a
point, or interpolates between samples. What exists reads a tile's samples.

**DEFLATE, written here** (`world/inflate.hpp`): zlib streams and raw DEFLATE,
with the Adler-32 checked and a limit on how much a stream may decompress to.
It is not zlib because the renderer's dependencies will bring their own, and
two static zlibs in one program are one set of symbols too many. It is tested
against ten streams Python's zlib made (`tools/make_inflate_fixtures.py`) with
every strategy zlib has - fixed and dynamic codes, stored blocks, runs,
literals only, sync and full flushes, and matches 32,000 bytes back - and
against hand-built streams: stored blocks of the largest size, a failed
checksum, every length of three streams cut short, a decompression limit one
byte too small, eight kinds of malformed header and block, and 2,000
deterministic corruptions under the sanitizers.

**GeoTIFF** (`world/geotiff.hpp`): single-channel float32 rasters on WGS84
latitude and longitude, in tiles or strips, either byte order, uncompressed or
DEFLATE, with or without the floating-point predictor, sample grids of points
or areas, the GDAL no-data value, and overviews. Anything else - BigTIFF,
integer samples, projected coordinates, a transformation matrix - is refused
by name. Tests write GeoTIFFs in all 32 combinations of those layouts and read
every sample back, and write 14 files it must refuse, each for its stated
reason.

**The real tile.** `Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif`, pinned by
SHA-256, decodes to exactly the heights a separate decoder gave - Python, its
zlib, and the predictor written again from TIFF Technical Note 3 - at 15
samples across its internal tiles, including tile edges and the sea. Its
sample (0, 0) is exactly at 151 E, 33 S: the Copernicus grid's samples are
points on whole arc-seconds, and each tile's south and east edges belong to
its neighbours.

**Downloads for tests** (`tests/cmake/fetch.cmake`): the files the tests need,
listed with their SHA-256 in `tests/data/downloads/files.txt`, fetched once
before the tests that read them. Without the network those tests are skipped;
with `GLIDESLOPE_REQUIRE_NETWORK` set, as CI sets it, that fails. A file that
arrives with the wrong hash always fails and is deleted. CI keeps the
downloads between runs. The unit-test harness can now skip a test (exit 77).
The DEM's source, pinned file and licence, with its notices quoted, are in
`ASSETS.md`.

**Watched to fail:** a distance code's extra-bits entry one short (the far
fixture referred back before its start); the stored-block length check
removed (the malformed test caught it); the predictor's running sum removed
(the layouts test, and the real tile at sample (0, 0): 198.016 for 274.590); the
half-sample shift for area grids reversed (the layouts test); a wrong pinned
hash (the fetch failed and left nothing behind). The first run of the
truncation test took 277 s: zeros past the end of a stream decoded as literals
up to the limit. The decoder now refuses input read more than four bytes past
its end.
