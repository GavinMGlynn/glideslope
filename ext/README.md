# Third-party dependencies

**Nothing is pinned here yet.** Phase 0 builds only this project's own code.

When dependencies arrive, every one is a pinned git submodule in this directory:
nothing here is this project's code, nothing is modified in place, and nothing
is redistributed by this repository — a clone gets URLs and commit SHAs, not
sources. Non-git sources, such as the SQLite amalgamation, are pinned by URL
plus SHA-256 instead.

Each dependency gets a row in this table in the commit that adds it: upstream,
pinned tag or SHA, role, and licence.

| Submodule | Upstream | Pinned at | Role | Licence |
| --- | --- | --- | --- | --- |

## Expected, from `REQUIREMENTS.md`

- **JSBSim** — flight dynamics, Phase 1.
- **SDL3** — window, input and GPU through SDL_GPU, Phase 2.
- **Cesium Native** — terrain and imagery streaming, Phase 2.
- **libcurl and a JSON library** — weather, Phase 3.
- **SDL_net and libsodium** — the transport, Phase 6.
- **SQLite** — server storage, Phase 6.

Each is added when the phase that needs it starts, and not before: a dependency
nothing links is still a dependency to build, pin and keep current on three
platforms.
