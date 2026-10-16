### Cesium ion as a visual terrain provider, 2026-09-21 — item done

Phase 5b's first item. **Cesium ion draws Cesium World Terrain under Bing
Maps Aerial with its attribution, and a provider without its key says what is
missing rather than failing** - both walked by one test that takes 17 seconds
and reports itself skipped where there is no token.

**A key belongs to the user and is never in the repository.**
`platform::config_directory()` is where their settings live -
`%APPDATA%\glideslope`, `~/Library/Application Support/glideslope`, or
`$XDG_CONFIG_HOME/glideslope` - and `cesium_ion_token()` and
`google_maps_key()` read one at run time, from an environment variable first
and then a file of their own. A missing, unreadable or empty one is "none",
because a provider that needs a key says what is missing and where to put it:

    Cesium ion needs your own token: put it in cesium-ion-token in
    glideslope's config directory, or set GLIDESLOPE_CESIUM_ION_TOKEN. One is
    free from https://cesium.com/ion/

**`--terrain open|ion|google`** chooses what is drawn. The ground the aircraft
meets is the open DEM whichever is drawn, which is the rule that lets a server
and every client agree on where the ground is.

**Three things had to be built before any of it could fetch a tile.**

1. *Requests carry headers.* The accessor refused anything but a plain GET
   with no headers, and ion authorises every tile with one. `HttpRequest` now
   carries headers and all three backends send them - libcurl's `slist`,
   WinHTTP's CRLF block, NSURLSession's `setValue:forHTTPHeaderField:`. A
   name or value holding a control character is dropped rather than passed
   on, so nothing can be smuggled in by splitting a header across lines.
2. *Content encoding is the HTTP layer's own business.* ion serves its
   `layer.json` with `content-encoding: gzip` whether the client asks for it
   or not. libcurl only undoes an encoding it negotiated, so a forwarded
   `Accept-Encoding` left a body nothing could parse. libcurl now accepts
   every encoding it can undo, and a provider's own `Accept-Encoding` is not
   passed on.
3. *A token goes only to the host that issued it.* `AuthorisingAccessor` puts
   the bearer token on requests to one place and no other, so a tileset
   naming a URL elsewhere cannot make it leak.

**Cesium Native logged the token.** It says at info level which URLs it
fetched, and an ion URL carries the token in it - which would put the user's
token in whatever kept the output, a CI log included.
`gfx::log_to_standard_error()` now also quiets the log to warnings and worse.

**Two defects in Cesium Native v0.64.0, both found by the sanitized build.**
Drafts of both are ready to post upstream; neither is glideslope's to fix.

1. *A dangling reference on every ion tile load.*
   `TileLoadInput::pAssetAccessor` is a reference member
   (`const shared_ptr<IAssetAccessor>&`), and `CesiumIonTilesetLoader` passes
   a `shared_ptr` to a *derived* accessor. The conversion makes a temporary
   that is bound to the reference and destroyed at the end of the statement,
   so every later use reads it dangling - `stack-use-after-scope`. The open
   provider never meets it, because its types match and no temporary is made.
   **Worked around**: ion's asset endpoint is resolved here, with one plain
   GET, and the tiles are then an ordinary tileset with an Authorization
   header, which goes nowhere near that code. `main` upstream still has the
   same reference member, so bumping the pin would not have helped.
2. *Misaligned loads on every terrain tile.*
   `QuantizedMeshLoader::readValue` is
   `return *reinterpret_cast<const T*>(data.data() + offset)` at an arbitrary
   byte offset, and quantized mesh does not align its fields. With
   `-fno-sanitize-recover=all` that ends the process, so a sanitized build
   could not stream Cesium World Terrain at all. **Decided 2026-09-21 by the
   project owner**: the alignment check alone is off for Cesium Native's own
   targets - `glideslope_allow_misaligned` in `cmake/Sanitizers.cmake` - and
   every other check, and all first-party code, is untouched. It goes away
   when they fix it.

**Waiting for a whole Earth is not a thing that ends.** The open provider's
terrain is built here, over a region, and stops at the DEM's own spacing, so
waiting for every tile a view needs makes the same command draw the same
terrain on every machine. A streamed provider has no such end, and asking
Cesium Native for it - `updateViewGroupOffline`, which refines regardless of
screen-space error - ran past 25 minutes. A streamed provider is given a
settling instead: rounds of loading, taking up what the workers finish, until
what is drawn, how deep it goes and how many tiles the tileset holds have all
stood still for three seconds, and never more than three quarters of a
minute. **Its frame is therefore not
claimed to be the same on every machine**, because what arrives depends on
the network and on the provider; what is claimed is that it drew terrain and
that its attribution is on it. With that, ion reaches level 13 at the same
screen-space error the open provider uses, in 13 seconds.

**What it draws.** Mount Taranaki from the north-east, Cesium World Terrain
under Bing Maps Aerial, 126 tiles, the deepest at level 13 - with ion's own attribution along the bottom: the USGS, CGIAR-CSI,
Copernicus, Land Information New Zealand, data.gov.uk, Geoscience Australia,
Microsoft, Mapbox, Earthstar Geographics SIO, Maxar and Airbus DS, and the
free tier's "upgrade for commercial use".

**Both things this left were picked up the same day**, and are written up
under "Google's Photorealistic 3D Tiles" and "How far the terrain drawn is
from the terrain flown" above. Google's tiles did not draw because
`Cesium3DTilesContent::registerAllTileContentTypes()` had never been called,
so every `glTF` body was offered to the JSON tileset reader and refused at
byte offset 0 - the guess recorded here, that content was being read as the
wrong thing, was right. The two things found on the way to it and kept are
`QueryAccessor`, which carries Google's `session` and `key` onto child
requests that name neither, and reading ion's third answer shape
(`externalType: 3DTILES`, a url and no token).
