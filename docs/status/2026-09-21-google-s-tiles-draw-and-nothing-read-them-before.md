### Google's tiles draw, and nothing read them before, 2026-09-21

**Google's Photorealistic 3D Tiles draw, photographs and all, with Google's
attribution**, and their surface is 10.1 m from the ground flown at worst.
Phase 5b's Google item is still not ticked, because the other way in to them
- a Google Maps Platform key used directly - has no key here to prove it
with. Phase 5b's attribution item is ticked: every provider, in both its
states, is now walked by a test.

**Nothing read a tile's content until its readers were registered.** Cesium
Native keeps a table of converters - glTF, B3DM, PNTS, composite - looked up
by the first bytes of what arrives, and `registerAllTileContentTypes()` fills
it. Nothing called it. A tileset whose tiles are glTF therefore loaded every
one of them and drew none: not knowing what a body was, Cesium fell back to
reading it as an external tileset, and 441 perfectly good glTF binaries came
back as "Error when parsing JSON content, error code Invalid value. at byte
offset 0".

It had never mattered. The open provider builds its glTF here, and Cesium
ion's quantized mesh has a reader of its own; Google's is the first content
that arrives as glTF and has to be recognised. Two guesses were wrong before
this one - that the body was still compressed, and that it was an
authorisation failure - and what settled it was printing the status and first
four bytes of every response: 548 of them, all 200, 441 beginning `glTF`, and
441 parse errors.

**Two real faults were fixed on the way**, neither of them the cause:

- A bearer token of ours went onto requests for anything Cesium ion points at
  without a token of its own, which is how Google's asset was asked for.
- Google's root carries a key and no session, and the child tiles it names
  carry a session and no key, so neither "it already has a query" nor "it
  already has a session" says whether the key is there. Taking either for an
  answer is a 403. Each parameter is now merged on its own.

**They draw their own pictures now.** Photorealistic 3D Tiles carry their
textures inside their glTF, and the only textures this renderer uploaded were
imagery draped as raster overlays - which is what the open provider and
Cesium ion use, so nothing had needed it. A primitive's base-colour texture
is now followed to its image, decoded once per tile, uploaded with the tile
and freed with it, and its TEXCOORD_0 read where no imagery is draped over
it. A textured surface starts white, because the shader draws the vertex
colour times the texture.

**And they refine.** The settling that tells a streamed provider when it has
arrived watched what was drawn and how deep it went. Google's tiles are not a
quadtree, so their depth reads 0 throughout, and what is drawn can sit still
for a moment part-way down - so it stopped at 19 tiles of smooth green. It
now also watches how many tiles the tileset holds, which keeps rising while
it is still refining: 452 tiles drawn of 795 held, and Mount Taranaki is a
snow-capped cone with the coast behind it.

**What is left: the other way in.** A Google Maps Platform key used directly,
rather than through Cesium ion, is written and has never been run, because
there is no such key on this machine. `--terrain google` takes it in
preference to the ion token when one is there, so the test covers whichever
the machine has; until one machine has a Google key, half of "through both
ways in" is unproven and the item stays open.

**Verification run.** 282 of 282 tests pass locally at `-j4`, in 1067 s.
`the_google_terrain_draws_with_its_attribution_or_says_why_not_on_vulkan`
takes 38 s and `the_google_terrain_is_within_its_stated_distance_of_the_ground_flown_on_vulkan`
40 s; the second was watched to fail with its bound tightened to 5 m. The
settling change is what let Phase 5b's first item be ticked as well:
`the_ion_terrain_draws_with_its_attribution_or_says_why_not_on_vulkan` now
takes 23 s, where waiting for a whole Earth had run past 25 minutes.
