### The geoid is fetched from a source that serves the pinned file, 2026-10-01 — tail done; main made green

**What is not done first.** The aircraft models' source files
(`assets/models/sources.txt`, FlightGear's fgaddon) come only from SourceForge,
which served the same 859-byte page for every one of them on 2026-10-01:
`the_files_the_visual_models_are_made_from_arrive_with_their_pinned_hashes`
fails on a machine without them, and CI has them only from its downloads
cache. That is a tail in `COMPLETION_PLAN.md`. `fetch.cmake` now takes more
than one URL a line, so the fix is a second source per line once one serving
the same bytes is found.

**What broke.** Every test needing the EGM2008 geoid failed on every platform
(CI run 36818460007, and main's run at 2026-10-01T05:07Z):
`https://sourceforge.net/projects/geographiclib/files/geoids-distrib/egm2008-5.zip/download`
"arrived with SHA-256 f188236c..., not the pinned 408f05e0...". Fetched by
hand, it is a 302 to `https://sourceforge.net/#!/projects/...` and an 859-byte
HTML page saying "the Sourceforge site is currently in Disaster Recovery
mode". `downloads.sourceforge.net` answered 522; every `*.dl.sourceforge.net`
mirror tried redirected back to it or did not resolve; the Internet Archive
was offline too.

**The source.** deck.gl's data repository on GitHub holds GeographicLib's
`egm2008-5.zip`, committed once (`f0b91db8`, 2020-12-21). Fetched at that
commit it is 16,773,259 bytes with SHA-256
`408f05e0c04a9f2e17b9ea2d27123f936e9dea60128bb3411a272f8ddbe318dd` - the pinned
file byte for byte, so the pin is unchanged. The URL names the commit, so the
repository changing its branch does not change what is served.

**More than one source.** `world::fetch_pinned` takes a list of URLs as well
as one: each is tried in order, and one that cannot be reached, answers with an
error or serves anything but the pinned bytes is passed over; nothing but the
pinned file is ever kept, and when none serves it the refusal names every
source and why. `world::egm2008_geoid_file()` gives the geoid's name, pin and
sources - GitHub first, SourceForge second. `tests/cmake/fetch.cmake` reads any
further URLs on a line of `files.txt` and tries them the same way, three
attempts each, and a wrong hash still fails the run if no URL serves the
right one. **CI's cache cannot keep a bad file**: neither the program nor
`fetch.cmake` puts a file in place before its hash matches, and a cached file
is checked against its pin each time it is read.

**Verification.**
- `a_pinned_file_is_fetched_from_the_first_of_its_sources_that_serves_its_pinned_bytes`:
  a fake fetch serves an interstitial page, a 404, the file, and a fourth
  source never asked; the first three are asked in order and the file kept.
  With no source serving it the refusal names the page's source ("not the
  pinned") and the 404's, and nothing is kept; no sources at all is refused.
  Seen to fail with only the first source tried.
- `the_geoids_sources_and_pin_are_the_ones_the_tests_fetch`: the program's
  name, pin and sources, in order, are `files.txt`'s line for the geoid. Seen
  to fail with one of the program's URLs changed.
- With the real network: `the_files_the_tests_download_arrive_with_their_pinned_hashes`
  fetches the geoid from GitHub into an empty directory, and
  `the_egm2008_grid_gives_geographiclibs_undulations_within_its_stated_error`
  passes on it; `glideslope_cli height -33.9461 151.1772` with an empty
  `GLIDESLOPE_CACHE` fetched it through the program's own path (SHA-256
  `408f05e0...`) and gave 6.234 m above the sea, the geoid 22.079 m above the
  ellipsoid, at Sydney Airport.
