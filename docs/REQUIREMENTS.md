# Requirements

A multiplayer flight simulator with real flight physics and live wind, flown
over real-world terrain streamed from the internet. Players can fly themselves
or hand any aircraft to an AI pilot.

This document captures the decisions made so far and the reasoning behind them.
Items marked **Open** are not yet decided.

---

## 1. Goals

- Real flight dynamics (6DOF), including wind and turbulence.
- Real terrain and imagery for anywhere on Earth, streamed from online sources.
- A choice of aircraft types.
- User and AI flying modes, switchable per aircraft at runtime.
- Networked multiplayer, client/server only.
- Not a scored or competitive game: no rollback, replay verification or
  lap-time checking.

## 2. Platforms and toolchain

- **Language:** C++ throughout — client, server, CLI and tools. JSBSim and
  Cesium Native are C++, so there is no C/C++ boundary to maintain.
- **Targets (64-bit only):** Linux x86_64 (Red Hat and Debian families),
  Windows x64, macOS (arm64; x86_64 optional).
- **Build:** CMake with presets per platform in `-debug` and `-release`,
  following the conventions of `GavinMGlynn/gearstick`.
- **Dependencies:** pinned git submodules under `ext/`, as in gearstick. vcpkg
  is an option for Cesium Native's dependency tree.
- **RHEL note:** use `gcc-toolset-14` (or later) for a modern compiler on
  RHEL 9.
- **Linux packaging:** an AppImage or Flatpak covers both distribution families
  with one build.
- **macOS:** code signing and notarisation are required for distribution.
- **CI:** GitHub Actions matrix across Ubuntu, Rocky 9, Windows and macOS.

### 2.1 Development environment

**Primary: native Windows 11** for all 3D rendering work.

- Real GPU driver with full Vulkan and D3D12, so SDL_GPU runs at full speed on
  either backend. Test both from one machine by switching backends with SDL's
  GPU driver hint.
- Graphics debugging: RenderDoc, PIX (D3D12), the GPU vendor's tools (NVIDIA
  Nsight or AMD equivalent), and the Vulkan validation layers.
- MSVC or clang-cl with the project's CMake presets. This is also the Windows
  target, so it is tested directly.

**Secondary: WSL on Windows 11 running Rocky Linux 10** for Linux builds and
headless work.

- GCC builds, `ctest`, and headless `glideslope_cli` and `glideslope_server`
  runs. Most simulation and networking work can happen here.
- **Not for 3D rendering.** WSL's GPU path translates Vulkan to D3D12 through
  Mesa; it is incomplete and slower, and Rocky's Mesa packages may not include
  the needed driver. Verify before relying on it.
- Keep source on the Linux filesystem, not under `/mnt/c`, or builds are very
  slow.

**Not recommended: VMware on Windows 11 running Rocky Linux 10.**

- The virtual GPU has weak or no hardware Vulkan, so rendering usually falls
  back to lavapipe (Mesa's software Vulkan). Acceptable for a one-frame `--shot`
  test, unusable for flying.
- Only useful if a full Rocky desktop, systemd or the server's window is needed;
  WSL covers build and headless work more cheaply.

**Gaps none of these cover:**

- **Linux rendering on real GPU drivers:** a physical Linux machine or
  dual-boot partition for periodic checks. In CI, lavapipe renders the headless
  screenshot tests.
- **macOS Metal:** a Mac, preferably Apple silicon.

## 3. Core libraries

| Area | Choice | Notes |
|---|---|---|
| Window, input, GPU | **SDL3** with the **SDL_GPU** API | One codebase over Vulkan, Metal and D3D12. Avoid OpenGL: deprecated on macOS and capped at 4.1. |
| Joysticks, HOTAS, yokes | SDL3 gamepad/joystick | Cross-platform. |
| Flight dynamics | **JSBSim** (C++) | One `FGFDMExec` instance per aircraft. Ships models from a Cessna 172 upward; includes atmosphere and Dryden turbulence. LGPL: link dynamically or keep changes open. |
| Terrain and imagery | **Cesium Native** (C++, Apache 2.0) | Streams 3D Tiles and quantized-mesh terrain, handles LOD and caching, and yields glTF meshes. The project writes the SDL_GPU upload and draw glue. Renders all three terrain providers (section 4.1). |
| Weather | libcurl + JSON library | METARs from aviationweather.gov; winds aloft from Open-Meteo. Fed into JSBSim's atmosphere. |
| Networking | **SDL_net** + **libsodium** | Same pairing as gearstick. Authenticated, encrypted datagrams. |
| Server storage | SQLite | Accounts, aircraft definitions, session config. |
| Maths | GLM (or equivalent) | Double precision for world positions. |

## 4. World and simulation architecture

- **Coordinates:** double-precision Earth-centred, Earth-fixed (ECEF) world
  positions.
- **Rendering precision:** camera-relative floating origin, converting to float
  only relative to the camera. This avoids geometry jitter at planetary scale.
  Reversed-Z depth keeps distant terrain from z-fighting, which the floating
  origin alone does not fix.
- **Physics timestep:** fixed rate (JSBSim default 120 Hz), decoupled from
  rendering. Rendering interpolates between physics states.
- **Aircraft interface:** every aircraft is a JSBSim instance plus a
  **controller**. A controller is either human input or an AI pilot, so
  switching user and AI mode is a controller swap.
- **Terrain queries:** ground height and collision come from the **collision
  terrain** (section 4.1), never from the visual mesh, so the server and every
  client's prediction agree on where the ground is.

### 4.1 Terrain providers

All three sources are supported, behind one provider interface. The user chooses
the visual provider in settings.

| Provider | Role | Access |
|---|---|---|
| **Open data:** Copernicus DEM + OpenStreetMap buildings + open imagery | **Default.** Works with no account. Also the source of the collision terrain. | Built in; no key. |
| **Cesium ion** | Optional visual provider (Cesium World Terrain and imagery). | The user supplies their own ion access token. |
| **Google Photorealistic 3D Tiles** | Optional visual provider. | The user supplies their own Google Maps Platform API key, or reaches the tiles through their own Cesium ion access token. |

- **No keys or tokens ship with the project.** Commercial providers are opt-in
  and each user is bound by that provider's terms, including attribution and
  caching rules. Attribution is displayed on screen for whichever provider is
  active.
- **Collision terrain is always the open DEM**, at a fixed dataset version and
  resolution, identified by hash. The server uses it for ground contact and
  collision; clients use the same data for predicted ground contact.
- **The project reads Copernicus DEM's GeoTIFF files itself.** Nothing is
  converted to tiles or hosted. Cesium Native only streams quantized-mesh
  terrain and 3D Tiles, and the server and client prediction need DEM heights
  regardless, so one reader serves both collision and drawing.
- **Visual and collision terrain can disagree** (for example, wheels on a Google
  mesh sitting slightly above or below the DEM surface, or buildings present
  visually but not in collision). This is accepted and documented; a measured
  bound for the mismatch belongs in `PROJECT_STATUS.md`.
- Provenance, dataset versions and licence terms for each source are recorded
  in `docs/ASSETS.md`.

### 4.2 Aircraft roster

Decided up front (2026-09-18), so that what the world, the weather, the HUD and
the network must carry is known before they are built. Flight models are
JSBSim's where JSBSim ships one, and are held to published figures before they
are offered; where it ships none, one is written here from published data.

| Class | Aircraft | Flight model |
|---|---|---|
| Light aircraft | Piper J-3 Cub, Cessna 172P, Piper PA-28, Cessna 182 | JSBSim's |
| Seaplane | Short S.23 Empire flying boat | JSBSim's, with its hydrodynamics |
| Second World War | de Havilland Mosquito - the first after the Cessna | Written here |
| Business jet | Learjet 35A | Written here |
| Airliners | Airbus A320, Boeing 737, 747, 787-8 | JSBSim's |
| Airliners | Airbus A380 | Written here |
| Fighters | F-15 Eagle, F-22 Raptor | JSBSim's |
| Fighter | F-35B Lightning II | Written here |
| Bomber | B-2 Spirit | Written here |

JSBSim's other models - among them the P-51D, B-17G, DHC-6, Global 5000, MD-11
and Concorde - are candidates, not commitments.

What the roster asks of everything else:

- **Speed and height.** From a Cub at 70 kt to an F-22 above Mach 1.5 and the
  F-15 at 65,000 ft. Terrain must be drawn over the whole Earth as fast as a
  jet crosses it, not around one region; the atmosphere and winds aloft must
  reach the stratosphere (Open-Meteo's 30 hPa, about 24 km, does); the HUD must
  give Mach and flight level where they apply.
- **Water.** The seaplane alights on the sea, lakes and rivers, so the ground
  under an aircraft must say where water is - the DEM's water body mask - and
  a landplane must not roll out on it.
- **Published figures.** Airliners are held to their manufacturers'
  airport-planning documents and type-certificate data sheets; light aircraft
  and the Mosquito to their handbooks and pilot's notes. **Much of the F-35B's
  and B-2's performance is not public**: they are held to what is published and
  no more, and `PROJECT_STATUS.md` says which of their behaviour no figure
  pins.
- **Visual models** come from FlightGear's aircraft where one exists, each
  licence checked (section 9).
- **The network** carries aircraft far faster than a Cessna: at Mach 0.8 an
  aircraft moves 27 m in the 100 ms other aircraft are interpolated behind.

### 4.3 Checklists and lessons

Decided 2026-09-18: the simulator teaches flying. Every aircraft carries
checklists for each phase of flight, as data beside its flight model, taken
from its handbook or pilot's notes and written in this project's own words.
An item names the state of the aircraft that shows it done - flaps set, mixture
rich, gear down, a speed reached - so it ticks itself; one the simulation cannot
see (a passenger briefing, a look out) is the pilot's to confirm. Lessons teach
take-off, the circuit, climbs, turns, stalls, approach and landing for each
class of aircraft, as a sequence of stages each with what to do and what to
watch; the AI pilot (section 5) demonstrates a lesson and hands over, and a
debrief says what to do differently. There is no score (`FEATURES.md`).

### Suggested layout (gearstick-style)

```
src/sim/        JSBSim wrapper, aircraft, controllers, autopilot, AI pilot
src/world/      coordinates, floating origin, terrain streaming and queries, weather
src/gfx/        SDL_GPU renderer, Cesium Native glue, HUD
src/net/        protocol, packet encode/decode, interpolation
src/platform/   paths, input, sockets
src/frontend/   one main per executable: client/, server/, cli/
ext/            pinned submodules
docs/
```

Keep `src/sim/` free of rendering and windowing so a headless tool and the
server can link it without SDL's video subsystem.

## 5. AI flying modes

Layered, each built on the one below:

1. **Classical autopilot:** PID controllers for heading, altitude, airspeed and
   vertical speed.
2. **Navigation:** waypoint following and flight-plan execution on top of the
   autopilot.
3. **LLM copilot or planner (later):** natural-language commands such as "take
   off, climb to 3,000 ft and orbit the CBD" are turned into a flight plan that
   layer 2 executes. Runs off the simulation thread. **The LLM plans; the
   controllers fly.** The model never drives control surfaces directly. Later
   it stays in the loop: reading the aircraft's state and changing the
   autopilot's modes and the plan every few seconds as the flight goes - pilot
   in command of the autopilot, as a pilot is. A model answering in seconds
   cannot hold an aircraft at the 120 Hz a control loop needs, so its output
   is modes and plans, never control positions. It needs an autopilot that
   can fly an approach and land, and the player's own key.
4. **Reinforcement learning (stretch goal):** JSBSim has Python gym-style
   wrappers for training landing or aerobatic agents.

## 6. Networking

### 6.1 Topology

- **Client/server only. No peer-to-peer.** Every client holds one authenticated
  libsodium channel to the server; clients never talk to each other.
- No NAT traversal or relay fallback is needed.
- **No rollback, no deterministic simulation, no server verification.** The sim
  is not scored.

### 6.2 Authority

- **The server is authoritative for every aircraft**, player and AI. It runs one
  JSBSim instance per aircraft at 120 Hz against the collision terrain.
- **Clients send inputs, not state:** stick, rudder, throttle, mixture, flaps,
  gear, brakes and switches, each input frame tagged with a sequence number.
  Inputs are sent redundantly (the last several frames in each packet) so a
  lost datagram does not lose input.
- **Clients predict their own aircraft** by running JSBSim locally on the same
  inputs, so controls respond on the frame they are pressed.
- **Reconciliation:** the server's state updates include the last input sequence
  number it applied. The client resets its local JSBSim to that server state,
  re-applies its unacknowledged inputs, and compares the result with what it
  displayed. Small errors are blended out over a short interval; large errors
  snap.
- **Divergence is expected, not a bug.** JSBSim is floating point and client and
  server may be different platforms, so prediction and server will drift
  slightly. Reconciliation keeps it bounded; the bound is measured and stated
  (section 8.3).
- **Key technical risk:** reconciliation needs to set a JSBSim instance to a
  full aircraft state (position, attitude, velocities, angular rates, engine and
  propeller state, control positions) and resume cleanly. JSBSim has no single
  snapshot/restore call, so this wrapper is built and proved in Phase 1, before
  any networking.
- Cheating by reporting false state is not possible, because clients never
  report state. Input validation still applies (rate limits, ranges).

### 6.3 Server responsibilities

The server is the meeting point, the owner of shared state and the simulation
host:

- Lobby, identity and **slot assignment**. The server decides who is which
  player, not whoever connected first.
- **Session clock** shared by all clients, used for input timing and
  interpolation.
- **Simulation of all aircraft** at 120 Hz.
- **Weather:** the server's weather is authoritative. It is sent to clients so
  their prediction flies in the same wind; turbulence differences are corrected
  by reconciliation.
- **Collision terrain:** aircraft can fly anywhere on Earth, so the server loads
  the open DEM around each aircraft as it moves, and tells clients which dataset
  version and hash to use.
- **Aircraft definitions:** which aircraft types exist and which type each
  player is flying.
- **Replication:** each client gets its own aircraft with acknowledgement data
  and every other aircraft, all at 20–30 Hz. With at most four players,
  interest management is not needed.
- **Session size:** up to **four players**, set per server with `--players N`.

### 6.4 Replication

**Client → server, every input frame (batched, e.g. 60 Hz):**

- Input sequence number and session timestamp
- The last several input frames, for redundancy

**Server → client, 20–30 times per second:**

- For the client's **own aircraft:** full reconciliation state (last applied
  input sequence, position, orientation, velocities, angular rates, engine and
  control state).
- For **other aircraft:** session timestamp, ECEF position, orientation
  quaternion (quantised, smallest-three), linear and angular velocity, and
  visual state (control surfaces, gear, flaps, lights, engine RPM). Roughly
  60–80 bytes each after quantisation.

**Clients render other aircraft** about **100 ms in the past**, interpolating
between snapshots, and extrapolate from velocity when packets are late, blending
back when the next snapshot arrives.

**Collisions** (mid-air and ground) are resolved on the server. Clients may show
predicted ground contact for their own aircraft; the server's result wins.

### 6.5 AI aircraft

- AI aircraft are server aircraft with an AI pilot controller instead of a
  client's inputs. They are replicated like any other aircraft.
- AI traffic persists with no clients connected.
- **Player → AI:** the server swaps the aircraft's controller from the client's
  input stream to an AI pilot. No state is transferred, because the server
  already owns it; the client stops predicting and interpolates its former
  aircraft like any other.
- **AI → player:** the server swaps back and the client resumes prediction from
  the next full state.
- **Player disconnects:** the server either removes the aircraft or hands it to
  an AI pilot, by session setting.

### 6.6 Server shape (following gearstick)

- Separate binary, `glideslope_server`.
- Fixed 120 Hz simulation step for all aircraft; replication at 20–30 Hz.
- `--headless` for no window; otherwise a live dashboard with connected clients,
  ping, traffic and a drop control.
- `--players N`: how many players the server accepts, 1 to 4 (default 4). A
  fifth connection is refused with a message saying the session is full.
- `--port`, `--store FILE` (SQLite), `--key HEX` (server secret, minted once and
  stored if not given), `--timeout`.
- Deployment: systemd unit and Dockerfile under `deploy/`.
- Clients connect with `--server HOST PORT --server-key HEX`, or `--online`
  using a one-line `server.txt` naming the default server's host, port and
  public key.

### 6.7 Transport (reuse gearstick's)

- Reuse gearstick's transport as specified in its `docs/TRANSPORT.md`:
  `Noise_IK_25519_ChaChaPoly_BLAKE2s` over UDP, libsodium primitives, a sequence
  number and replay window per message.
- One suite, no negotiation. The client knows the server's static key out of
  band (`--server-key`, printed by the server at startup).
- Same six-byte envelope shape (magic, version, type); every protocol message
  travels as a plaintext payload inside a `SEALED` datagram. Use a new magic
  value so a gearstick client and a flight sim server reject each other cleanly.
- Write this project's own `docs/TRANSPORT.md` byte for byte, including what the
  transport does **not** claim, so a third party could write a client from the
  document alone.
- Inputs (redundant) and state updates are unreliable (latest wins). Lobby,
  session, weather, aircraft definitions, terrain dataset and controller-swap
  messages need a small reliable-delivery layer on top.

## 7. Phases

These are the starting phases for `docs/COMPLETION_PLAN.md` (see section 8).
Each item there gets a one- or two-sentence description and a plain-English
verification.

- **Phase 0 — Foundations:** CMake/Ninja presets per platform with matching test
  presets; 64-bit gate; warnings as errors; layering checks at configure time;
  CI on Ubuntu, Rocky, Windows and macOS; packaging; the living documents exist
  and are honest.
- **Phase 1 — The feel:** one JSBSim Cessna 172 in `glideslope_cli` with no
  window, a fixed 120 Hz step with an accumulator, scripted-input flight checked
  against known aircraft figures. Also the **state set/resume wrapper**
  reconciliation depends on: set an instance to a captured mid-flight state,
  resume, and track the original within a stated tolerance. Answers whether the
  flight model feels right, and whether prediction is feasible, before any
  renderer or network exists.
- **Phase 2 — The world:** ECEF coordinates, floating origin, reversed-Z depth,
  open-data collision terrain read from Copernicus DEM with height query, Cesium
  Native drawing the open-data provider through SDL_GPU around one region,
  joystick input, basic HUD. The Cesium-to-SDL_GPU glue is the biggest
  rendering risk.
- **Phase 3 — Weather:** METAR and winds aloft into JSBSim's atmosphere.
- **Phase 3b — Wind that shears and gusts, and hazardous air:** gusts flown, the
  boundary layer, reported shear, microbursts, thermals and mountain waves, and
  weather drawn - each a function of position, time and the weather's shared
  parameters, so the server and every client fly the same air.
- **Phase 4 — Autopilot and navigation:** PID layer, waypoint following, user/AI
  controller swap.
- **Phase 5 — Aircraft choice:** multiple aircraft types as data, selectable at
  start: the roster of section 4.2.
- **Phase 5c — Learning to fly:** checklists for every aircraft, ticking
  themselves from the aircraft's state, and lessons the AI pilot demonstrates
  and debriefs (section 4.3).
- **Phase 5b — Terrain providers:** Cesium ion and Google Photorealistic 3D
  Tiles as opt-in visual providers with user-supplied keys, on-screen
  attribution, and a measured visual-to-collision terrain mismatch.
- **Phase 6 — Client and server:** transport, server binary, lobby, server
  simulation of all aircraft, input streaming, client prediction and
  reconciliation, interpolation of other aircraft. Verification: four clients on
  different operating systems plus one server-run AI Cessna flying together, a
  fifth client refused, `--players 2` refusing a third, all three visible on the
  server dashboard, with each client's prediction error at 100 ms and 200 ms
  simulated latency within its stated bound.
- **Phase 7 — User/AI controller swap** across the network, including player
  disconnect.
- **Phase 8 — LLM copilot**, then RL agents as a stretch goal.
- **Tails:** found work, added at the bottom the moment it is found.

## 8. Development and documentation strategy (from gearstick)

Adopted from `GavinMGlynn/gearstick`. Where a gearstick rule depends on its
deterministic integer simulation, the adaptation for this project is stated.

### 8.1 The living documents

| File | What it is | What it must not contain |
|---|---|---|
| `CLAUDE.md` | Working conventions: the shape of the project, discipline rules, project specifics, layout. | Status or progress. |
| `docs/FEATURES.md` | The menu, at the altitude of "what would the player notice". Each entry tagged `CORE`, `WANTED`, `CANDIDATE`, `DONE` or `OUT`. Rejected ideas stay, with their reasons, so they are not re-proposed. Ends with "Deliberately not" and "Open questions". | Any implementation: no data structures, formats or function names. |
| `docs/COMPLETION_PLAN.md` | The road to done, in phases. One line per item saying what it is and how you would know it works, plus a Tails section for found work. A user document: keep it a summary. | Implementation detail, test-name inventories, design rationale. |
| `docs/PROJECT_STATUS.md` | The single source of truth for what works today, gaps named first. Where detail and rationale live. Newest first. | Claims of "working" for anything partial. |
| `docs/TRANSPORT.md` | The wire protocol, byte for byte, and what it does not claim. | — |
| `docs/THREATS.md` | What is defended, from whom, how, what is deliberately not defended, and the order of work. | — |
| `docs/ASSETS.md` | Provenance and licence rules for aircraft models, terrain, imagery, weather data, fonts and sound. | — |
| `docs/GUIDE.md` | The player's guide: the first ten minutes as a path to walk, every control, how to report a bug. | — |
| `docs/RELEASES.md` | What a download is, how it was built, what the OS will say about it. | — |
| `ext/README.md` | Every submodule: upstream, pinned tag or SHA, role, licence. | — |

**Rules that bind them:**

- Every commit that lands a plan item updates `PROJECT_STATUS.md` (what now
  works, with its verification) and `COMPLETION_PLAN.md` (tick the item, add
  any tails) **in the same commit**.
- Re-read both in full at every phase boundary.
- `[x]` means 100% of the item. An item with anything unimplemented stays `[ ]`
  with the missing part named in its text. Splitting an item to tick the easy
  half is not allowed.
- An item without a named verification cannot be ticked.
- The plan is finished when this returns nothing:

  ```
  sed -n '/^## Phase /,/^## Tails/p' docs/COMPLETION_PLAN.md | grep '^- \[ \]'
  ```

### 8.2 Discipline

- **The simulation links no presentation.** `src/sim/` may not include SDL
  video, input or audio, nor `gfx/`, `ui/`, `platform/` or a frontend. Checked
  at configure time by `cmake/Layering.cmake`, not by review. `glideslope_cli`
  linking the simulation and nothing presentational is the proof. The server
  links the same simulation to run AI aircraft.
- **The simulation steps at a fixed rate; the presentation does not.** Physics
  runs in fixed 120 Hz steps; frames interpolate between states. Physics per
  frame is not allowed.
- **Warnings are errors in every build type**, first-party targets only; `ext/`
  is untouched. Include `-Wconversion` and `-Wsign-conversion`: double-to-float
  narrowing at the floating origin is exactly the kind of silent bug they catch.
- **Verify on the real output:** a frame written by `--shot`, a `ctest` run,
  flight numbers from `glideslope_cli`. Not a proxy, not "it should work now".
- **Content is data, not code.** Aircraft, sessions, regions and AI flight plans
  live in files. A hard-coded aircraft is a prototype and must be replaced
  before anything is built on it.
- **One item at a time, landing with its test.** Keep `ctest` green; a red tree
  stops everything. Commit each finished item and push.
- **Name tests as sentences stating the fact they pin**, e.g.
  `a_cessna_at_full_power_climbs_near_its_published_rate`.
- **A test nobody has seen fail is not trusted.** Where a verification says a
  deliberate bug turns it red, the bug was introduced, the failure watched, and
  the bug reverted.
- **A test that only sometimes tests its rule is worse than none.** Build the
  situation explicitly.
- **Cover every scenario, not a sample.** Every control, every aircraft, every
  setting, every state a screen can be in. If exhaustive looks too slow, fix the
  step, not the claim.
- **Coverage is asserted, not believed.** A test that walks a space states its
  size and how much it covered, and fails when they differ. Exclusions are named
  in the test with their reason.
- **Never describe a partial module as working.** Report the missing part first:
  "terrain streams and draws; nothing collides with it yet", never "terrain
  works".
- **Prefer no dependency to a small one.** Pin every dependency: submodules
  under `ext/` by tag or SHA; non-git sources (e.g. SQLite amalgamation,
  datasets) by URL plus SHA-256.

### 8.3 Adapted: the tripwire without determinism

Gearstick's golden replay compares one state hash across all platforms. That
depends on integer-only physics and does **not** carry over, because JSBSim is
floating point.

The replacement:

- **Same-machine replay hash:** `glideslope_cli selftest` flies a fixed input
  log and prints a state hash. On one build on one machine it must be stable
  run to run. A change that moves it is deliberate, noted in
  `PROJECT_STATUS.md` with the reason.
- **Cross-platform flight checks by tolerance:** CI flies the same scripted
  inputs on every platform and checks outcomes against published aircraft
  figures and each other within stated tolerances (climb rate, stall speed,
  glide ratio, turn rate, position after N minutes).
- **Network checks:** a headless server plus scripted clients in CI, with
  injected latency, loss and jitter, verify prediction error and correction
  size, interpolation error, controller-swap continuity and the `--players`
  limit against stated bounds.

### 8.4 CI and tooling (as gearstick)

- Workflows: `ci.yml` (build, test, headless smoke on Debian-family Linux,
  Rocky, Windows, macOS arm64), `package.yml` (build artifacts, unpack
  elsewhere, run `glideslope_cli selftest` and render one frame from the
  unpacked copy).
- `cmake/`: `Platform.cmake`, `CompilerWarnings.cmake`, `Layering.cmake`,
  `Libsodium.cmake`, `Sqlite.cmake`, fuzzer and sanitizer support.
- Tooling flags on the client for tests and screenshots: `--shot FILE`,
  `--shot-at TICK`, `--trace`, `--screen NAME`, `--autopilot` (the AI flies this
  client's aircraft, like gearstick's `--autodrive`).
- Server flags for tests: `--seconds N`, `--plain`, `--window-dump`,
  `--window-shot`, `--window-press`.

---

## 9. Decisions

**Closed in the original brief:**

- **Language:** C++ throughout (no C core).
- **Authority:** server-authoritative for all aircraft, with client-side
  prediction and reconciliation (section 6.2).
- **Terrain:** open data by default and for collision; Cesium ion and Google
  Photorealistic 3D Tiles as opt-in visual providers with user-supplied keys
  (section 4.1).
- **Session size:** up to four players, configurable per server with
  `--players N` (1–4, default 4). Interest management is dropped: every client
  receives every aircraft at full replication rate.

**Closed 2026-09-17:**

- **Licence: GPL-3.0-or-later.** Matches gearstick, so gearstick's transport and
  server code can be reused without relicensing questions. Compatible with
  JSBSim (LGPL-2.1) and Cesium Native (Apache 2.0). Keeps derivatives open. As
  sole copyright holder the author can still dual-license later. Third-party
  data terms (Google, Cesium ion, Copernicus, OSM) are separate from the code
  licence and recorded in `docs/ASSETS.md`.
- **Server-run AI aircraft:** how many a session runs is a server setting,
  default 4. Each is a JSBSim instance at 120 Hz on the server.
- **Open-data terrain delivery:** the project reads Copernicus DEM's GeoTIFF
  files itself; no tile hosting (section 4.1).
- **Session scope: the whole world.** Aircraft can fly anywhere on Earth, so the
  server loads collision terrain around each aircraft as it moves (section 6.3).
- **Depth precision:** reversed-Z depth, built in Phase 2 (section 4).
- **Aircraft visual models:** JSBSim supplies flight dynamics only. Visual
  models come from FlightGear's aircraft, which are mostly GPL; each model's
  licence is checked and recorded in `docs/ASSETS.md`.
- **Google Photorealistic 3D Tiles** can be reached with a Google Maps Platform
  key or through a Cesium ion token (section 4.1).
- **No Git LFS.** Dependencies and datasets are pinned as section 8.2 describes.
- **Hosting:** the project owner's AWS hosting runs the default public server,
  if one is needed.
- **Test hardware:** the project owner has Windows, macOS and Linux machines for
  rendering checks and Phase 6 verification.
- **macOS signing:** the project owner has an Apple Developer account for
  signing and notarising macOS builds.
- **Rocky version in CI: Rocky 9**, the oldest supported, with `gcc-toolset-14`
  or later as the RHEL note says. Rocky 10 is covered by the WSL development
  environment.
- **Open-Meteo terms:** its free API is for non-commercial use only. That fits a
  free GPL project; the terms are recorded in `docs/ASSETS.md`.
- **Executable names:** `glideslope` (the client), `glideslope_cli` and
  `glideslope_server`, following gearstick's pattern of naming them after the
  project. The original brief used `flightsim_*` as example names.

- **Shaders: GLSL, compiled at build time for every backend.** SDL_GPU takes
  SPIR-V on Vulkan, DXBC or DXIL on Direct3D 12 and MSL on Metal. Each shader
  is written once in GLSL, to SDL_GPU's SPIR-V resource layout; glslang makes
  the SPIR-V, SPIRV-Cross the MSL and HLSL, and on Windows D3DCompile turns the
  HLSL into Shader Model 5.1 DXBC. DXC is not used: it is LLVM-sized, and
  SDL_GPU accepts DXBC. The build's shader compiler checks every resource
  against SDL_GPU's documented layout (`cmake/Shaders.cmake`).

- **HTTPS through the operating system, not a bundled libcurl.** Terrain now,
  and weather later, are fetched through WinHTTP on Windows, NSURLSession on
  macOS, and on Linux the system's libcurl loaded at run time
  (`src/platform/http.hpp`). Each uses the system's certificate store and proxy
  settings and is updated with it; nothing is built or shipped for it, and the
  program starts without libcurl and says what is missing if a download is
  wanted. Section 3's "libcurl + JSON library" for weather becomes this and a
  JSON reader written here (`src/world/json.hpp`): the weather needs only to
  read small documents, strictly to RFC 8259, which is short to write and to
  test, and the project prefers no dependency to a small one.

- **Cesium Native's dependencies through vcpkg** (section 2 left it open).
  Cesium Native needs thirty libraries, and vcpkg is how Cesium Native itself
  builds them on every platform; as submodules they would be thirty builds to
  write and keep. vcpkg is pinned to the commit Cesium Native's release uses and
  fetched outside the tree (`cmake/Vcpkg.cmake`); `vcpkg.json` lists the
  packages and a configure-time check holds it to Cesium Native's own list. The
  cost is a first build of most of an hour, which vcpkg's binary cache, kept by
  CI, pays once.

- **The open imagery: EOX's Sentinel-2 cloudless mosaic of 2016** (settled
  2026-09-18; section 4.1 asked for open imagery needing no account). A
  cloud-free mosaic of the whole Earth from the Copernicus Sentinel-2
  satellites at 10 m a pixel, served by EOX as a Web Map Tile Service in
  latitude and longitude, which Cesium Native drapes on the terrain as a
  raster overlay. The 2016 layer is under CC BY 4.0 - the later years are
  non-commercial - and EOX invites its endpoints to be used directly in an
  application; its credit is shown on screen (`docs/ASSETS.md`). NASA's GIBS
  Blue Marble, public domain, was the other candidate: at 500 m a pixel it
  cannot show a runway.

**Open:**

- **Runways on the DEM** (under discussion). Copernicus DEM is a radar-measured
  surface model (it includes trees and buildings) with a sample every 30 m and
  a few metres of vertical error, so runways come out with bumps and ramps that
  are not really there. Proposed: give paved runways a smooth surface that
  follows their surveyed slope between the two thresholds, from OurAirports
  data, and keep the DEM everywhere else.
- **Buildings:** how OpenStreetMap buildings arrive without a Cesium ion
  token.
