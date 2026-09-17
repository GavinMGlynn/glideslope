# glideslope

A multiplayer flight simulator in C++: real flight physics and live wind, flown
over real-world terrain streamed from the internet. Fly yourself, or hand any
aircraft to an AI pilot and take it back.

> **Status: Phases 0, 1 and 3 are complete, and Phase 2, the world, is under
> way.** A Cessna 172P flies to its handbook, stands
> on the real ground anywhere on Earth, and can be flown from the keyboard or a
> joystick with a HUD, in the weather reported at an airfield now. There is no
> terrain to see yet: the frame is sky and HUD.
> [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) is the single source of
> truth for what works, with the gaps named first.

## The idea

- **Real flight dynamics** from JSBSim, in real wind from live weather reports.
- **The real Earth**, anywhere on it: open terrain data by default with no
  account, and Cesium ion or Google Photorealistic 3D Tiles with your own key.
- **Up to four players** on a server that owns every aircraft, with controls
  that still answer on the frame you move them.
- **AI pilots** that take over any aircraft and hand it back — an autopilot
  first, flight plans next, and later a copilot you talk to.

It is not a scored or competitive game, and what it deliberately does not do is
in [`docs/FEATURES.md`](docs/FEATURES.md).

## Building

```sh
git clone --recurse-submodules https://github.com/GavinMGlynn/glideslope.git
cmake --preset linux-release      # or linux-debug, macos-*, windows-*
cmake --build --preset linux-release
ctest --preset linux-release
```

What the build makes, today:

```sh
glideslope_cli figures c172p            # fly the Cessna's published figures
glideslope_cli selftest                 # a five-minute flight, and its hash
glideslope_cli height -33.9461 151.1772 # the ground's height, from the DEM
glideslope_cli weather YSSY             # the weather at Sydney airport now
glideslope --weather YSSY               # fly from over Sydney, in its weather
glideslope --screen origin              # a test scene, in a window
```

`cpack --preset linux-release` (or `macos-release`, `windows-release`) makes a
package that runs from wherever it is unpacked. Terrain data and weather are
downloaded when first needed, terrain into the user's cache directory (or
`GLIDESLOPE_CACHE`); on Linux that needs the system's libcurl, which almost
every distribution has.

## The documents

- [`docs/REQUIREMENTS.md`](docs/REQUIREMENTS.md) — the design, and every
  decision made about it.
- [`docs/FEATURES.md`](docs/FEATURES.md) — what the simulator should be.
- [`docs/COMPLETION_PLAN.md`](docs/COMPLETION_PLAN.md) — the road to done, in
  phases, each item with its verification.
- [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) — what works today.
- [`docs/ASSETS.md`](docs/ASSETS.md) — where third-party data comes from, and
  its terms.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE). Third-party data — terrain, imagery,
weather and aircraft models — is under its own terms, recorded in
[`docs/ASSETS.md`](docs/ASSETS.md).
