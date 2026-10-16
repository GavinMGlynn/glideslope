### CI split into builds and test shards, and a handshake that overflowed, 2026-09-23

**CI had been red since 2026-09-21**, through two sessions of pushing, and the
causes were stacked: each hid the next. libsodium's port needed autotools no
runner had; a bare `std::getenv` failed MSVC; the clang-warnings test fed
`cl`'s flags to `clang++`; two tests shared a Cesium cache; a timing threshold
tested the runner's speed; the F-15C's airframe contacts were wheels with
tyres; the belly landing idled its engines; and MSVC's debug runtime answered
every failed assert with a dialog box nobody on a runner will click, so four
tests hung until the job's limit cancelled it.

**Once asserts reached the log, the hang was a real bug**: the handshake copied
its 33-byte protocol name into a 32-byte hash. The last byte landed in the next
member and was overwritten at once, so nothing ever misbehaved and
AddressSanitizer - which guards objects, not the joins between their members -
could not see it. It copies only what fits now, which is byte for byte what the
hash held before. The larger fault underneath is a tail: Noise's BLAKE2b has a
64-byte hash, this uses one cut to 32, so the protocol is not quite the Noise
variant it is named after (`TRANSPORT.md` now says so).

**The workflow is rebuilt for time.** The Ubuntu debug "build, test" step took
53 minutes and Windows debug 90. Now every configuration builds in its own job;
ccache keeps objects between runs on Linux and macOS; a build packs what its
tests need (422 MB for Linux debug, compressed) and uploads it once; and its
tests run in shards, `ctest -I k,,n`, each a job on its own runner. A shard
cannot finish sooner than its longest test, and simulated on measured timings
sharding stopped improving at 9.9 minutes whatever the number of shards,
because one test took 592 s. **That test - every view shot in one - is seven
tests and a comparison now**, the views as the comparison's ctest fixture, so a
shard that runs the comparison runs the seven with it. Run alone the eight took
207 s. Every test also has a 900 s default timeout, so no hang can hold a job.

**Splitting the views reopened the shared cache**: seven view tests running at
once shared one Cesium cache until the view went into its name. On the first
warm run macOS builds took 30 s and Rocky 52 s, but Ubuntu took 6 to 9
minutes: GCC 14 has CMake scan every file for C++ modules, and ccache will not
cache a scanned compile, so 80% of calls missed. Ubuntu configures with the
scan off; glideslope has no modules. A macOS runner also lost DNS for
api.open-meteo.com for longer than a fetch's three tries spanned (6 s),
failing the weather and HUD tests; fetches now try five times, over 30 s.

**The next run found a real bug in a dependency.** When an imagery tile times
out, Cesium Native fills its place with the parent tile's pixels scaled up,
through stb's resizer - and the stb that vcpkg's pinned checkout carries
(stb_image_resize2 v2.10, 2024-07-29) writes a float past its decode buffer
when scaling three-channel pixels, and reads a field through a pointer it has
just freed. The sanitized macOS client stopped there in three tests. stb
fixed both in v2.11 and v2.12; `cmake/ports/stb` is vcpkg's own port at stb's
2026-08-02 commit (v2.18), pinned by SHA-512, as an overlay.
`a_parent_imagery_tile_scaled_into_a_missing_childs_place_is_sound` makes
that blit: under v2.10 AddressSanitizer stopped it with a heap overflow (seen);
under v2.18 it passes.

**The view tests shot imagery that streamed.** Each view is two launches,
with the aeroplane and without, and the outline is where the two frames
differ. A tile slow in one launch and not the other made them differ in the
terrain too: the cockpit's two shots differed with no aeroplane in either, and
the ahead, behind and orbit outlines took in terrain. They are shot with
`--imagery off` now, so the frames rest on the pinned DEM alone; the imagery
keeps its own tests. And the state-rate floor was 50 of 75 updates in three
seconds, where a debug server on a busy Windows runner sent 32: it is 10,
which still fails once a second, the thing it is there for.
The input-frame floor in the flying test was the same: 100 of 180, where the
same runner's debug client sent 97. It is 20; once a second would be six.
Its count of inputs still in flight at the end, which it allowed ten, was
seventeen on a loaded Ubuntu debug runner; it allows a second's worth, thirty.

**One shard took 19 minutes** because the test holding all seven views'
flights to one another had the seven as its ctest fixture, so whichever shard
ran it ran all seven, each two sanitized client launches. That test is gone:
the cockpit view is the fixture, and each outside view holds its own flight to
the cockpit's (seen to fail with the cockpit's account altered). A shard
running any view now pulls in one other test, not six.

**A Windows crash now prints its stack.** The virtual-joystick test
segfaulted once on Windows debug with nothing in the log but "SegFault"; it
passed the run before, and 200 runs under AddressSanitizer on Linux. Nothing
in SDL's virtual driver explains it on reading, so it is not fixed.
`no_crash_dialogs()` installs an unhandled-exception filter that prints the
exception and a symbolized stack, and the Windows packs keep each program's
PDB, so its next occurrence says where.
