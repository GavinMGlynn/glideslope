# Completion plan

The road to done, in phases. **This is the map, not the territory** — one line
per item, saying what it is and how you would know it works. What any of it is
actually made of lives in `PROJECT_STATUS.md`, which is the file to read when
you want detail. Why a feature exists at all lives in `FEATURES.md`, and the
design decisions behind it in `REQUIREMENTS.md`.

**`[x]` means 100% of the item, and nothing less.** An item with any claimed
behaviour unimplemented stays `[ ]`, with the missing part named in its text.
Splitting an item to tick the easy half is not allowed.

Every item names its verification. An item without one cannot be ticked. Where
a verification says a deliberate bug "turns it red", the bug was introduced, the
test was watched to fail, and the bug was reverted. Tails found while
implementing something go in at the bottom the moment they are found.

`[x]` done · `[ ]` not started, or **In progress** where the text says so

**The plan is finished when this returns nothing:**

```sh
sed -n '/^## Phase /,/^## Tails/p' docs/COMPLETION_PLAN.md | grep '^- \[ \]'
```

The phase order is not arbitrary. The flight model and the state set/resume
wrapper come before any renderer or network, because they answer the two
questions that could sink the project cheaply: does it feel right, and can a
client predict an aircraft the server owns. The world comes next because the
Cesium-to-SDL_GPU glue is the biggest rendering risk. Networking comes late
because it is built on everything before it.

---

## Phase 0 — Foundations

- [x] **C++20 + CMake/Ninja build**, presets per platform with matching test
      presets. *Verification: every preset configures, builds and tests on its
      own platform.*
- [x] **64-bit-only and compiler gates.** *Verification: a 32-bit toolchain, and
      each supported compiler one version below its floor, are refused with a
      message naming what was found and what is needed; the configure line names
      the resolved platform and compiler.*
- [x] **Warnings as errors in every build type**, first-party targets only.
      *Verification: a double-to-float narrowing fails the build in every build
      type on every compiler, and a sign conversion does on GCC and Clang.*
- [x] **The simulation links no presentation**, checked at configure time.
      *Verification: every forbidden include placed in `src/sim/` fails the
      configure naming the file and the line, linking SDL into the simulation
      fails it too, and `glideslope_cli` links the simulation and nothing
      presentational.*
- [x] **CI on Ubuntu, Rocky 9, Windows and macOS**, building and testing every
      preset. *Verification: every job green on `main`.*
- [x] **Packaging** — a tarball for Linux and macOS and a zip for Windows, each
      running in place. *Verification: the `package` workflow unpacks each
      artifact into a different directory on its own platform and runs
      `glideslope_cli` out of the unpacked copy.*
- [x] **The living documents exist and are honest** — `CLAUDE.md`, this plan,
      `FEATURES.md`, `PROJECT_STATUS.md`, `TRANSPORT.md`, `THREATS.md`,
      `ASSETS.md`, `GUIDE.md`, `RELEASES.md` and `ext/README.md`.
      *Verification: someone who has not seen the code can say what works from
      `PROJECT_STATUS.md` alone.*

## Phase 1 — The feel

- [x] **JSBSim, pinned and built**, linked by the simulation, with the Cessna
      172 loaded from its model files. *Verification:
      `glideslope_cli` loads the Cessna 172 and prints the figures JSBSim read
      from its files, and the layering check still passes.*
- [x] **A fixed 120 Hz step with an accumulator.** *Verification: the same
      scripted flight fed its time in small, large and uneven chunks ends in the
      same state.*
- [x] **Scripted flight checked against published figures** — static RPM,
      take-off roll, climb rate, cruise speed, glide ratio, stall speeds, and a
      turn rate against the physics of a coordinated turn, since the handbook
      publishes none. **In progress:** all in range on Linux; CI proves the
      other platforms. *Verification: each figure lands within its stated
      tolerance of the Cessna 172's published number.*
- [x] **State capture and set/resume** — a full aircraft state written into a
      fresh instance, which flies on from it. *Verification: an instance
      restored mid-flight tracks the original within a stated tolerance, from a
      capture taken in every phase of flight the scripted flights cover.*
- [ ] **The same-machine replay hash** — `glideslope_cli selftest` flies a fixed
      input log and prints a state hash. **In progress:** proved on Linux; CI
      proves the other platforms. *Verification: the hash is identical
      run to run on one build, and a deliberate one-line change to the physics
      moves it.*
- [ ] **Cross-platform flight checks by tolerance.** *Verification: every CI
      platform flies the same scripted inputs and agrees with the published
      figures, and with the other platforms, within stated tolerances.*
- [ ] **The packaged CLI flies.** *Verification: the `package` workflow runs
      `glideslope_cli selftest` out of every unpacked artifact.*

## Phase 2 — The world

- [ ] **Earth-centred, Earth-fixed positions in double precision**, with
      latitude, longitude and height conversions. *Verification: conversions
      round-trip within a millimetre at the poles, the equator, the date line,
      and from below sea level to cruising altitude.*
- [ ] **A camera-relative floating origin.** *Verification: a still scene
      rendered far from the origin is identical frame to frame, with no jitter.*
- [ ] **Reversed-Z depth.** *Verification: a frame holding both distant
      mountains and a nearby aircraft draws with no z-fighting at either.*
- [ ] **The Copernicus DEM, read directly** from its GeoTIFF files at a pinned
      version, with a height query anywhere on Earth. *Verification: heights at
      surveyed points — summits, coastlines, airfields — match within the
      dataset's stated accuracy, and the version and hash are in `ASSETS.md`.*
- [ ] **Collision terrain in the simulation.** *Verification: an aircraft set
      down at sea level, at a high airfield and on a slope rests on the DEM
      surface in each.*
- [ ] **A window and a GPU device through SDL3** on Vulkan, D3D12 and Metal.
      *Verification: the client writes a frame with `--shot` on every backend
      of every platform that has it.*
- [ ] **Cesium Native drawing the open-data terrain through SDL_GPU** around one
      region. *Verification: a `--shot` of a known region matches a reference
      frame of it within a stated tolerance.*
- [ ] **Open imagery on the terrain**, from the source `REQUIREMENTS.md`
      settles on. *Verification: a `--shot` shows the imagery and its
      attribution.*
- [ ] **Joysticks, HOTAS and yokes.** *Verification: every axis and button of a
      virtual device reaches the aircraft's controls, walked by test.*
- [ ] **A basic HUD** — airspeed, altitude, heading, vertical speed, attitude.
      *Verification: the numbers in a `--shot` match the simulation's state at
      that tick.*
- [ ] **The client's test flags** — `--shot FILE`, `--shot-at TICK`,
      `--trace`, `--screen NAME`. *Verification: each is used by a ctest.*
- [ ] **A frame rendered headless in CI and from every package.**
      *Verification: CI and the `package` workflow each upload a frame from
      every platform.*

## Phase 3 — Weather

- [ ] **METARs from aviationweather.gov.** *Verification: a recorded METAR sets
      the surface wind, temperature and pressure it reports.*
- [ ] **Winds aloft from Open-Meteo.** *Verification: a recorded response sets
      the wind at every level it reports, and between levels.*
- [ ] **Weather in JSBSim's atmosphere, with turbulence.** *Verification: an
      aircraft in a steady crosswind drifts at the rate the wind predicts, and
      turbulence disturbs it within a stated bound when on and not at all when
      off.*
- [ ] **Weather that changes during a flight without a jump.** *Verification: a
      new report blends in over a stated interval with no step in the wind.*

## Phase 4 — Autopilot and navigation

- [ ] **Holds for heading, altitude, airspeed and vertical speed.**
      *Verification: each hold captures a step change within a stated overshoot
      and settling time, in calm air and in turbulence.*
- [ ] **Waypoint following and flight plans as data.** *Verification: a plan
      loaded from a file passes every waypoint within a stated distance.*
- [ ] **The user/AI controller swap.** *Verification: swapping mid-flight in
      either direction causes no step in any control or in the aircraft's
      state, in every phase of flight.*
- [ ] **`--autopilot`** — the AI flies this client's aircraft.
      *Verification: a ctest flies a plan with it.*

## Phase 5 — Aircraft choice

- [ ] **Aircraft as data.** *Verification: an aircraft is added without a code
      change, and a test walks every aircraft the data holds.*
- [ ] **Visual models from FlightGear aircraft**, each licence checked.
      *Verification: `ASSETS.md` names the source, commit and licence of every
      model that ships, and a model without an entry fails a test.*
- [ ] **An aircraft chosen at start.** *Verification: every aircraft can be
      chosen, takes off, and passes its published-figure checks.*

## Phase 5b — Terrain providers

- [ ] **Cesium ion as a visual provider** with the user's own token.
      *Verification: with a token, a `--shot` draws Cesium World Terrain with
      its attribution; without one, the provider says why it is unavailable and
      its tests report themselves skipped.*
- [ ] **Google Photorealistic 3D Tiles** with the user's own Google Maps
      Platform key or Cesium ion token. *Verification: the same checks as
      Cesium ion, through both ways in.*
- [ ] **Attribution on screen for whichever provider is active.**
      *Verification: every provider, in every state, draws its attribution,
      walked by test.*
- [ ] **A measured visual-to-collision terrain mismatch.** *Verification: the
      bound for each provider at a set of reference airfields is stated in
      `PROJECT_STATUS.md`, with how it was measured.*

## Phase 6 — Client and server

- [ ] **The transport** — gearstick's, with its own magic value, written up
      byte for byte in `TRANSPORT.md`. *Verification: a client written from
      `TRANSPORT.md` alone completes a session, and a gearstick client is
      refused cleanly.*
- [ ] **Reliable delivery** for lobby, session, weather, aircraft definitions,
      terrain dataset and controller-swap messages. *Verification: every one
      arrives exactly once and in order under injected loss.*
- [ ] **The server** — `glideslope_server`, with `--headless`, `--players N`,
      `--port`, `--store FILE`, `--key HEX` and `--timeout`, and a dashboard
      otherwise. *Verification: every flag is exercised by a test, and a player
      count outside 1 to 4 is refused.*
- [ ] **Lobby, identity and slot assignment.** *Verification: slots are assigned
      by the server, the same whatever order players connect in.*
- [ ] **The server flies every aircraft**, at 120 Hz against the collision
      terrain, loading terrain around each aircraft anywhere on Earth.
      *Verification: aircraft on opposite sides of the world fly in one session,
      each over its own terrain.*
- [ ] **Server-run AI aircraft**, how many a server setting, default 4.
      *Verification: a server runs the number it is given, and four when given
      none.*
- [ ] **Inputs streamed with redundancy.** *Verification: no input frame is lost
      under injected loss.*
- [ ] **Client prediction and reconciliation.** *Verification: prediction error
      and correction size stay within stated bounds at 100 ms and 200 ms of
      simulated latency.*
- [ ] **Other aircraft interpolated 100 ms in the past**, extrapolating when
      packets are late. *Verification: interpolation error stays within a stated
      bound under loss and jitter.*
- [ ] **Collisions resolved on the server**, mid-air and with the ground.
      *Verification: two aircraft put on a collision course collide on the
      server, and every client shows it.*
- [ ] **Every network parser fuzzed** under sanitizers. *Verification: the seed
      corpus goes through every parser in CI.*
- [ ] **The server's test flags** — `--seconds N`, `--plain`, `--window-dump`,
      `--window-shot`, `--window-press`. *Verification: each is used by a
      ctest.*
- [ ] **Network checks in CI** with injected latency, loss and jitter.
      *Verification: prediction error, correction size, interpolation error and
      the `--players` limit all checked against their stated bounds.*
- [ ] **`THREATS.md` written.** *Verification: every message the server accepts
      is named in it with its defence, or with why it needs none.*
- [ ] **Deployment** — a systemd unit and a Dockerfile under `deploy/`.
      *Verification: a server started from each accepts a client.*
- [ ] **`--online`** through a one-line `server.txt`. *Verification: a client
      started with `--online` reaches the server `server.txt` names.*
- [ ] **Four machines in one sky.** *Verification: four clients on different
      operating systems and one server-run AI Cessna fly together; a fifth
      client is refused; `--players 2` refuses a third; all are visible on the
      server dashboard; and each client's prediction error at 100 ms and 200 ms
      of simulated latency is within its stated bound.*

## Phase 7 — User/AI controller swap across the network

- [ ] **Player to AI.** *Verification: the aircraft flies on with no step, and
      the client stops predicting it and interpolates it like any other.*
- [ ] **AI to player.** *Verification: the client resumes prediction from the
      next full state, with no step.*
- [ ] **A player disconnecting**, the aircraft removed or handed to an AI by
      session setting. *Verification: both settings, tested.*
- [ ] **AI traffic with nobody connected.** *Verification: AI aircraft keep
      flying on an empty server, and a client that joins later finds them
      mid-flight.*
- [ ] **Controller-swap continuity in the network checks.** *Verification:
      swaps under injected latency, loss and jitter stay within the stated
      bound.*

## Phase 8 — LLM copilot

- [ ] **Natural-language commands become flight plans**, planned off the
      simulation thread and flown by the autopilot. *Verification: "take off,
      climb to 3,000 ft and orbit the CBD" produces a plan that the autopilot
      flies.*
- [ ] **The model never drives a control surface.** *Verification: the copilot
      can produce a flight plan and nothing else, checked at configure time.*
- [ ] **Reinforcement-learning agents** (stretch goal). *Verification: an agent
      trained through JSBSim's gym-style wrappers lands an aircraft within
      stated limits.*

---

## Tails

Found while implementing something else. Added when found, not when remembered.

- [ ] **Runways on the DEM.** *(Found writing `REQUIREMENTS.md`; open there.)*
      The terrain data shows a runway with bumps it does not have.
      *Verification: decided in `REQUIREMENTS.md`, and if runways are smoothed,
      an aircraft rolls the length of reference runways with no bump beyond a
      stated bound.*
- [ ] **Free buildings for the default scenery.** *(Found writing
      `REQUIREMENTS.md`; open there.)* *Verification: a source is decided and
      recorded in `ASSETS.md`, and a `--shot` of a city shows its buildings.*
- [ ] **Signed and notarised macOS builds.** *(Found planning Phase 0.)*
      *Verification: a downloaded macOS package opens on a Mac with no
      Gatekeeper warning.*
- [ ] **One Linux download for every distribution** — an AppImage or Flatpak.
      *(Found planning Phase 0.)* *Verification: one file runs on a fresh
      Ubuntu and a fresh Rocky install.*
- [ ] **A hosted public server**, on the project owner's AWS, if one is
      needed. *(Found planning Phase 6.)* *Verification: `server.txt` names a
      running server that a client reaches with `--online`.*
