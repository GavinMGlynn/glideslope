# Third-party dependencies

Every dependency is a pinned git submodule in this directory:
nothing here is this project's code, nothing is modified in place, and nothing
is redistributed by this repository — a clone gets URLs and commit SHAs, not
sources. Non-git sources, such as the SQLite amalgamation, are pinned by URL
plus SHA-256 instead.

Each dependency gets a row in this table in the commit that adds it: upstream,
pinned tag or SHA, role, and licence.

| Submodule | Upstream | Pinned at | Role | Licence |
| --- | --- | --- | --- | --- |
| `jsbsim` | JSBSim-Team/jsbsim | `v1.3.1` (`3b25f25`) | **The flight model.** Linked by the simulation; built from its `src/` only | LGPL-2.1 |
| `sdl` | libsdl-org/SDL | `release-3.4.16` (`fa2c02b`) | **The window, input and GPU.** Linked by the presentation only; `cmake/Layering.cmake` refuses it in the simulation | Zlib |
| `glslang` | KhronosGroup/glslang | `16.6.0` (`e1b562a8`) | **Shaders, GLSL to SPIR-V.** Linked by the build's shader compiler only; nothing shipped | BSD-3-Clause and others (see its `LICENSE.txt`) |
| `spirv-cross` | KhronosGroup/SPIRV-Cross | `vulkan-sdk-1.4.357.0` (`6c09849f`) | **Shaders, SPIR-V to MSL and HLSL.** Linked by the build's shader compiler only; nothing shipped | Apache-2.0 |
| `cesium-native` | CesiumGS/cesium-native | `v0.64.0` (`80a22ff`) | **Terrain tiles: selection, loading, caching, glTF.** Linked by the presentation only (`glideslope_gfx`) | Apache-2.0 |

### jsbsim

`cmake/Jsbsim.cmake` builds JSBSim's `src/` directory rather than its top-level
project, because the top level also runs Doxygen, builds a Python module,
queries `lsb_release` and includes CPack with its own names, all of which would
leak into this build. `src/` needs a handful of variables from above it, which
are set inside a `block()`; the version among them is read from JSBSim's own
`CMakeLists.txt`, not written here.

It is linked statically. JSBSim is LGPL-2.1, which permits that for a program
whose own source is available to rebuild against a modified JSBSim, and this
project's is: it is GPL-3.0-or-later, in this repository. Its headers are
included as system headers, so this project's warning set does not fire inside
them; the sanitizers are applied to it in the sanitized presets, because a
program half-instrumented by AddressSanitizer reports errors that are not there.

JSBSim bundles two libraries in its source tree: **expat** (MIT), which parses
its XML, and **GeographicLib** (MIT). Their licence texts, and JSBSim's, are
installed into `licenses/` in every package. Building expat is why the project
enables C as well as C++.

### sdl

`cmake/Sdl.cmake` builds SDL statically, with its tests, examples and install
rules off. SDL_GPU is the renderer's only graphics API: Vulkan, Direct3D 12 or
Metal, chosen by SDL or by `--gpu-driver`.

**Building it needs the system's development packages.** On Linux that is
SDL's own list in `docs/README-linux.md` at the pinned tag;
`.github/workflows/ci.yml` installs it for Ubuntu and for Rocky 9, and that is
the copy that is tested.
SDL fails its configure without XTEST and XScrnSaver rather than building
without them; on RHEL-family systems the development packages come from EPEL
and CRB. Running the frame tests needs a Vulkan driver: CI uses Mesa's lavapipe,
which draws on the CPU.

### glslang and spirv-cross

`cmake/Shaders.cmake` builds their libraries, and `tools/shaderc/main.cpp` links
them into `glideslope_shaderc`, which runs during the build to turn each GLSL
shader under `src/gfx/shaders/` into SPIR-V, MSL and - on Windows, through the
system's D3DCompile - DXBC, and checks it against SDL_GPU's resource layout.
The program links none of their code; what it carries is the compiled shaders,
which are glideslope's. So no package carries their licences.

glslang is built without its optimizer, which would need SPIRV-Tools as well,
and without its HLSL front end: the shaders are GLSL. Neither library, nor the
compiler, is sanitized in the sanitized presets. They are a build step, and a
leak report from glslang would fail the build without saying anything about
glideslope.

### cesium-native

`cmake/CesiumNative.cmake` adds it with its tests, clang-tidy, curl and install
rules off; only the libraries the client links - `Cesium3DTilesSelection` and
what it needs - are built. Its own warnings are left as warnings, not errors:
its code is not this project's to fix, and not every compiler here is one it is
tested with. It is sanitized with the rest in the sanitized presets, as JSBSim
is. The glue that draws its tiles is `src/gfx/terrain_tiles.cpp`.

**Its dependencies come from vcpkg**, not from submodules: thirty libraries -
Abseil, S2, OpenSSL, Draco, KTX, libwebp, libjpeg-turbo, SQLite, spdlog and the
rest - which `vcpkg.json` lists, the same as Cesium Native's own manifest less
curl (the configure refuses the two lists parting). `cmake/Vcpkg.cmake` fetches
vcpkg at the commit Cesium Native v0.64.0 is built against,
`56bb2411609227288b70117ead2c47585ba07713`, into the user's cache directory -
on Windows `C:\gs-vcpkg`, short because MSVC cannot open a path of more than
260 characters and vcpkg builds deep under its root - rather than this tree - it is a tool, not a dependency - and installs the
packages after the platform gate has accepted the compiler. The ports, and so
every dependency's version, are that commit's - but for stb, which
`cmake/ports/stb` takes at `2c980bb59875b0d32144a71867fbdebb2f77cd20`
(2026-08-02, pinned by SHA-512): that commit's stb had a resizer that wrote
past its buffer, and Cesium Native resizes imagery. How each platform builds them is
`cmake/triplets/`, whose README says why.

**The first configure builds all thirty**, which takes most of an hour; vcpkg
keeps what it built in its binary cache (`~/.cache/vcpkg/archives`, or
`VCPKG_DEFAULT_BINARY_CACHE`), and every configure after that, in any build
directory, unpacks it in seconds. CI keeps that cache between runs. Building
them needs, on Linux, Perl with `IPC::Cmd` (OpenSSL's build), NASM
(libjpeg-turbo's), make, and autoconf, autoconf-archive, automake and libtool
(libsodium's); on macOS, NASM and those four; on Windows, nothing: vcpkg
fetches what it needs, and builds libsodium with MSBuild. **The autotools went
missing from CI for two days** without anyone noticing, because every other
package was being unpacked from the binary cache: libsodium, added for the
handshake, was the first thing CI actually had to build.

**Every package carries their licences**: Cesium Native's as
`licenses/CesiumNative.txt`, and each vcpkg package's copyright file as
`licenses/vcpkg/<package>.txt`.

The aircraft files JSBSim reads at run time are made from this submodule's by
`tools/make_c172p.py` and committed under `assets/jsbsim/`; see
`docs/ASSETS.md`.

## Expected, from `REQUIREMENTS.md`

- **SDL_net and libsodium** — the transport, Phase 6.
- **SQLite** — server storage, Phase 6.

Two expected dependencies turned out not to be: HTTPS goes through each
system's own client (`src/platform/http.hpp`), and the weather's JSON is read by
a parser written here (`src/world/json.hpp`); both decisions are recorded in
`REQUIREMENTS.md`.

Each is added when the phase that needs it starts, and not before: a dependency
nothing links is still a dependency to build, pin and keep current on three
platforms.
