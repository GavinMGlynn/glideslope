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

The aircraft files JSBSim reads at run time are made from this submodule's by
`tools/make_c172p.py` and committed under `assets/jsbsim/`; see
`docs/ASSETS.md`.

## Expected, from `REQUIREMENTS.md`

- **SDL3** — window, input and GPU through SDL_GPU, Phase 2.
- **Cesium Native** — terrain and imagery streaming, Phase 2.
- **libcurl and a JSON library** — weather, Phase 3.
- **SDL_net and libsodium** — the transport, Phase 6.
- **SQLite** — server storage, Phase 6.

Each is added when the phase that needs it starts, and not before: a dependency
nothing links is still a dependency to build, pin and keep current on three
platforms.
