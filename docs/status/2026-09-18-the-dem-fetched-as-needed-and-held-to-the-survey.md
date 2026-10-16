### The DEM, fetched as needed and held to the survey, 2026-09-18 — item done

**What is missing first:** summits. At five NGS summit stations the DEM is 8 to
35 m below the surveyed height, and even its highest sample within 90 m of each
is 5 to 21 m below: a 30 m grid of a radar surface does not hold a peak. The
plan's verification asked for summits within the stated accuracy; that is not
something a 30 m DEM can meet, and the verification is amended to say so
(`COMPLETION_PLAN.md`), with the shortfall pinned by a test. Also missing:
nothing in the simulation uses a height yet (the next item), and the DEM is not
thread-safe.

**Fetching** (`world/download.hpp`). `DownloadedTiles` keeps tiles in the cache
directory, fetching a missing one from the public bucket, checking it against
the MD5 the bucket gives as its ETag, and writing it into place only whole; a
tile that arrives damaged, missing, untagged or not at all is refused with the
reason and not kept. `fetch_pinned` does the same for a file pinned by SHA-256,
and `egm2008_geoid` uses it for the geoid grid. `world/digest.hpp` is SHA-256
and MD5, tested against FIPS 180-2's and RFC 1321's vectors and around every
padding boundary, whole and in pieces. `platform::cache_directory()` is
`GLIDESLOPE_CACHE`, or the user's cache directory on each system.

**HTTPS** (`platform/http.hpp`), through what each system trusts: WinHTTP,
NSURLSession, and on Linux the system's libcurl, loaded at run time, so nothing
is needed to build or to start and a missing libcurl is named when a download
is wanted. Recorded in `REQUIREMENTS.md`. Tested against the network: a pinned
1.1 MB file byte for byte, with its ETag equal to its MD5; GitHub's redirect to
the licence file at the commit that added it; a 404 as a response rather than an
error; a body over its limit, and a host that cannot exist, as errors.

**`glideslope_cli height LAT LON`** gives the height above sea level, the geoid,
and the height above the ellipsoid, fetching what it needs. Every package now
runs it at Sydney airport from an empty cache and must get 6.234 m: on stock
Ubuntu, which has no libcurl, it must first fail saying so, and then succeed
once libcurl is installed.

**Held to the survey** (`tests/data/dem/surveyed.txt`, sources in `ASSETS.md`).
Twelve runway ends from the FAA's data - Denver, Las Vegas, Boston, Juneau,
Anchorage, Utqiagvik, from 36 to 71 N, and so four bands of tile widths - are
within the Copernicus DEM's stated 4 m (the product handbook's "< 4m (90% linear
error)"), every one of them, worst 3.53 m, most within 2 m, and all below the
survey; five coastal waters are at 0.00 m. Twelve tiles are fetched for it, and
CI keeps them.

**Watched to fail:** the ETag check removed (the damaged tile was accepted);
heights above the ellipsoid compared with the survey instead of heights above
the geoid (Denver 20.7 m out); a wrong SHA-256 round constant, and a padding
boundary moved (both digest tests).

**Also:** AppleClang's `-Wdouble-promotion` refused a float returned as a
double in `dem.cpp` that GCC let pass; the Vulkan loader ignores
`VK_DRIVER_FILES` in an elevated process, which is what CI's Windows runner
runs, so CI now registers lavapipe in the registry instead.
