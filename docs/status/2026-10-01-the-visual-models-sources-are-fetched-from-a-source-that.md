### The visual models' sources are fetched from a source that serves their pinned files, 2026-10-01 — tail done

**Every file has a source outside SourceForge.** Software Heritage holds 60
of the 75. The other fifteen - the 787-8's `787-8.ac` and `787-8.xml`; the
A320's `A320-200-CFM.xml`, `A320-common.xml`, `Fuselage/a320.cfm.xml`,
`Fuselage/fuselage.xml`, `Fuselage/res/A320-216.ac` and
`Fuselage/res/CFM56.ac`; the A380's `XML/A380.xml`; the c182's `c182s.xml`;
the F-15's `f15c.ac` and `f15c.xml`; and the Mosquito's `Mosquito-FB6.xml`,
`pdiskL.xml` and `pdiskR.xml` - are published by this project, as the owner
approved on 2026-10-01: unmodified, each under its cached name, as assets of
the release `model-sources-r21588`
(<https://github.com/GavinMGlynn/glideslope/releases/tag/model-sources-r21588>),
with a `SHA256SUMS` and the three licence texts their directories carry (all
GNU GPL v2). Each was checked against its pin before it was staged, and the
release was made in the owner's session after checking `SHA256SUMS`.

**`--refresh` and Software Heritage's limit.** `tools/make_models.py
--refresh` lists the release second for an FGAddon file it serves with the
pinned bytes, and Software Heritage second otherwise, when it holds them. It
asks Software Heritage only about bytes neither answers for already - a file
the pinned list has it serving, or the release holds - because asking about
all 75 on every refresh spent the 120 requests an hour it allows: a refresh
on 2026-10-01 failed on its 429 three times running.

**What broke.** Every line of `assets/models/sources.txt` named one URL,
`https://sourceforge.net/p/flightgear/fgaddon/21588/tree/trunk/Aircraft/...?format=raw`
(the c172p's two named GitHub), and on 2026-10-01 SourceForge's web pages were
in "Disaster Recovery mode", answering each with a 302 to the same 859-byte
HTML page. `the_files_the_visual_models_are_made_from_arrive_with_their_pinned_hashes`
failed on any machine without them, and CI passed only from its downloads
cache.

**The sources.** FGAddon has no git mirror: FlightGear's GitLab group carries
none, and GitHub's `FGMEMBERS/FGADDON_mirror` is empty since 2016.
- *SourceForge's Subversion server* - `svn.code.sf.net`, a different service
  from the web pages - serves the repository over HTTP with `p` as the peg
  revision:
  `https://svn.code.sf.net/p/flightgear/fgaddon/trunk/Aircraft/<path>?p=21588`.
  All 73 FGAddon files fetched from it on 2026-10-01 matched their pins.
  A path that climbs (`XML/../Models/a380.ac`) is given resolved.
- *Software Heritage* serves a file by its own SHA-256 at
  `https://archive.softwareheritage.org/api/1/content/sha256:<pin>/raw/`, so it
  can only ever serve the pinned bytes. It holds 58 of the 73 FGAddon files and
  both of the c172p's; all 60 fetched from it matched. Its copy of FGAddon is a
  partial visit of 2022-02-03 and every visit since failed, which is why the
  15 changed after it are missing. Anonymous use allows 120 requests an hour,
  so a fetch from empty, 60 files at most, is within it.
- The aircraft's own repositories were checked for the missing 15 by git blob
  id: Zaretto's `fg-aircraft` (F-15), `FGDATA/IDG-A32X` and
  `legoboyvdlp/A320-family`, `IskenderWang/787-family`, `HHS81/c182s` - none
  holds the same bytes.

**The change.** Each line of `sources.txt` now lists, in order: the
Subversion server (or for the c172p, GitHub at its pinned commit); then
Software Heritage where it holds the file, or this project's release for the
fifteen it does not; and SourceForge's web view last. The pins are unchanged.
`tests/cmake/fetch.cmake` already tried several URLs a line (the geoid's
change); `tools/make_models.py` did not - it read exactly four fields - so it
now reads and writes any number of URLs. `--refresh` takes a file already
pinned only with its pinned bytes, from any of its URLs, so an outage page
never reaches the cache; a refusal such as Software Heritage's 429 stops it,
naming the URL, rather than being taken for a file that is not there and
sending the model walk to another path. CI's downloads cache is keyed on
`sources.txt` as well as `files.txt`.

**Verification.**
- `the_files_the_visual_models_are_made_from_arrive_with_their_pinned_hashes`,
  run as `fetch.cmake` with `GLIDESLOPE_REQUIRE_NETWORK=1` into an empty
  directory: all 75 fetched from their first URL, none failed, exit 0.
- Before the release, the same with the Subversion host renamed to one that
  does not resolve, for three files: the two Software Heritage holds came from
  it; the F-15's `f15c.xml`, which it does not, fell through to SourceForge's
  web view, got the 859-byte page and failed the run "fetched, but not what
  was pinned" - the gap the release closes, seen.
- `the_committed_visual_models_are_what_their_script_writes`
  (`tools/make_models.py --check`) with the new `sources.txt`: the old script
  fails, "ValueError: too many values to unpack (expected 4)"; the new passes.
- `tools/make_models.py --refresh`, over the 75 files fetched above, asking
  Software Heritage of each: it wrote `sources.txt` and every mesh byte for
  byte as committed.
- With the release made, `--refresh` added its URL to exactly the fifteen
  lines (60 keep Software Heritage), every mesh unchanged, and `--check`
  passes.
- `fetch.cmake` with `GLIDESLOPE_REQUIRE_NETWORK=1` into an empty directory,
  for the fifteen, with both SourceForge hosts renamed to ones that do not
  resolve: all fifteen came from the release with their pinned hashes, exit 0.
- The same for all 75, both SourceForge hosts unresolvable (run in two parts
  over one directory, the first stopped by a time limit, the second finding
  its 52 files present by hash): 58 came from Software Heritage, 15 from the
  release and the c172p's 2 from its GitHub, none with a wrong hash, exit 0.
- `a_model_source_refresh_keeps_only_pinned_bytes_and_reports_a_refusal_as_one`
  (`tests/tools/model_sources_refresh.py`, a stand-in network, 4 cases
  counted): Software Heritage's 429 stops the refresh naming it; a 503 is not
  taken for a missing path (one candidate tried, not two); with the Subversion
  server down, the web view's outage page is passed over and the pinned bytes
  come from the release; with no source serving them, nothing is cached.
  Against the script before these fixes all 4 fail; skipped without Python.
