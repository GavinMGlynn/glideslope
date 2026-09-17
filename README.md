# glideslope

A multiplayer flight simulator in C++: real flight physics and live wind, flown
over real-world terrain streamed from the internet. Fly yourself, or hand any
aircraft to an AI pilot and take it back.

> **Status: Phase 0 — foundations. Nothing flies yet.**
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
