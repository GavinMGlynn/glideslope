### The geoid, and zip archives, 2026-09-18

**What is missing first:** the geoid converts heights but nothing uses it yet:
there is still no height query. (The grid's terms were at first not found
stated; PROJ's data package records NGA's EGM2008 as public domain, quoted in
`ASSETS.md`.)

**`world/geoid.hpp`** reads GeographicLib's geoid grids in their PGM form and
interpolates bilinearly, wrapping longitude and clamping latitude at the poles.
**`world/zip.hpp`** reads stored and DEFLATE entries out of a zip archive, as
GeographicLib distributes its grids, checking each against its CRC-32; Zip64,
encryption and multi-disk archives are refused by name.
**`world/byte_source.hpp`** is where the GeoTIFF reader's byte sources went, so
the zip reader could share them.

**The tests.** CRC-32 against its published check value. A zip archive built in
the test, with stored, deflated and empty entries, reads back, and six bad ones
are each refused for their reason. A small synthetic grid interpolates exactly
as bilinear interpolation must: at samples, between columns, between rows, in
a cell, across 360 degrees, at negative longitudes, at 180 E and W, at both
poles and past them; five malformed grids are refused. **The real grid**,
`egm2008-5.zip` pinned by SHA-256, gives GeographicLib's own online GeoidEval
undulations at 14 places within the grid's stated 0.478 m - the worst is 0.118
m - including the Indian Ocean low (-106.9 m), New Guinea (+70.2 m), both
poles, and the date line from both sides.

**Watched to fail:** negative longitudes negated rather than wrapped (the
synthetic test at "a negative longitude is the same place", and Denver 27 m
out); the interpolation weights between columns swapped (New Guinea 0.55 m
out). The sanitized build also caught a real bug on the first run: a read of
zero bytes passed a null pointer to `memcpy`.
