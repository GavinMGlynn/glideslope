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

The aircraft files JSBSim reads at run time are made from this submodule's by
`tools/make_c172p.py` and committed under `assets/jsbsim/`; see
`docs/ASSETS.md`.

## Expected, from `REQUIREMENTS.md`

- **Cesium Native** — terrain and imagery streaming, Phase 2.
- **A JSON library** — weather, Phase 3. (HTTPS is not a dependency: see
  `src/platform/http.hpp` and `REQUIREMENTS.md`.)
- **SDL_net and libsodium** — the transport, Phase 6.
- **SQLite** — server storage, Phase 6.

Each is added when the phase that needs it starts, and not before: a dependency
nothing links is still a dependency to build, pin and keep current on three
platforms.
