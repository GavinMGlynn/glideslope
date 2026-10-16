### Open imagery on the terrain, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** the imagery is 2016's - EOX's later mosaics are not
open - and 10 m a pixel at its finest, so a runway is a grey stripe with no
markings. It is fetched from EOX's service as the view needs it, so the first
look at a place waits for it, and without the network there is none. How
sharp the imagery is has no check: the reference frame the test holds it to
chooses its own imagery level, and a level set too coarse matched it better.

**The source is settled** (`REQUIREMENTS.md`, section 9): EOX's Sentinel-2
cloudless mosaic of 2016 - the whole Earth, cloud-free, 10 m a pixel, under CC
BY 4.0, served in latitude and longitude as a Web Map Tile Service (terms and
credit in `ASSETS.md`).

**Drawn by Cesium Native as a raster overlay.** The terrain tileset carries a
Web Map Tile Service overlay (`gfx::open_imagery()`); Cesium Native works out
each terrain tile's texture coordinates, chooses and fetches the imagery tiles,
and the glue decodes each into a texture on the GPU - with mipmaps - and puts
it on the terrain tiles it covers, with the scale and offset that take a
tile's coordinates into its part of the image. The renderer's meshes now carry
texture coordinates and its shader samples a texture, white for a mesh with
none. With imagery the terrain's vertices carry only the sun's light on their
slope, and the imagery is drawn times it; `--imagery off` tints by height as
before. Everything fetched goes through Cesium Native's SQLite cache in the
cache directory, kept as long as the service's caching allows - a week.

**Credited on screen.** The imagery's credit, EOX's text, is drawn along the
bottom of every frame with imagery, after the DEM's notice; the font gained
`:` and `/` for its address. `README.md` carries it too.

**The tests.**
- `a_shot_of_mount_taranaki_drapes_the_open_imagery_where_it_belongs_on_<driver>`
  shoots the same view as the terrain test at 640x480 with imagery, and reads
  both credits back out of the frame. Its reference frame is the DEM ray-cast
  from the same eye, each hit coloured by the slope's light times the imagery
  there, fetched from the service at the level whose pixel is the ground the
  frame's pixel covers - independent of Cesium Native, the textures and the GPU.
  As drawn: the colour 1.86 of 255 out on average and 7 at the 95th percentile;
  and, blurring both by 4 pixels and moving the frame up to 3 pixels each way,
  the best match is where it was drawn. The tolerances are 3.0 and 12, and the
  best match within a pixel.
- The DEM test now shoots with `--imagery off`; the HUD test flies over imagery
  and reads three credits.

**Watched to fail:** the imagery upside down (the colour 4.90 out, 20 at the
95th percentile); drawn without the slope's light (5.42 out, and the best match
two pixels down). **Not caught**, and stated: the imagery moved east by 2% of an
imagery tile - some 40 m, about a pixel here - and a coarser imagery level.

**Also:** attaching an imagery tile, the resources Cesium Native hands over
are the imagery tile's, not the terrain tile's; taking them for the terrain
tile's overran them, which the sanitized build caught on the first run.
Windows CI's first build of the dependencies failed in Draco: vcpkg builds
deep under its root, and under the user's AppData the paths passed the 260
characters MSVC can open. vcpkg now lives in `C:\gs-vcpkg\<12 characters of
its commit>` on Windows.
