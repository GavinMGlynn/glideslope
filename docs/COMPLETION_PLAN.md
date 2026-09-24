# Completion plan

What is left to do and where we are up to, in phases: one short point per item,
saying what it is, how you would know it works and, for an open item, what is
missing. The detail - what was built, what was measured, what was found on the
way - is in `PROJECT_STATUS.md`. Why a feature exists is in `FEATURES.md`, and
the design decisions behind it in `REQUIREMENTS.md`.

**`[x]` means 100% of the item, and nothing less.** An open item names what is
missing. An item without a verification cannot be ticked. Tails found while
implementing something go in at the bottom the moment they are found.

`[x]` done · `[ ]` not started, or **In progress** where the text says so

**The plan is finished when this returns nothing:**

```sh
sed -n '/^## Phase /,/^## Tails/p' docs/COMPLETION_PLAN.md | grep '^- \[ \]'
```

The phases are worked in order: the flight model and state set/resume first, the
world next, and networking late because it is built on everything before it.

---

## Phase 0 — Foundations

- [x] **C++20 + CMake/Ninja build**, with presets per platform. *Verification:
      every preset configures, builds and tests on its own platform.*
- [x] **64-bit-only and compiler gates.** *Verification: a 32-bit toolchain, or
      a compiler below its floor, is refused with a message saying what is
      needed.*
- [x] **Warnings as errors in every build type**, first-party targets only.
      *Verification: a double-to-float narrowing fails the build in every build
      type on every compiler.*
- [x] **The simulation links no presentation**, checked at configure time.
      *Verification: a forbidden include in `src/sim/` fails the configure, and
      `glideslope_cli` links nothing presentational.*
- [x] **CI on Ubuntu, Rocky 9, Windows and macOS.** *Verification: every job is
      green on `main`.*
- [x] **Packaging** — a tarball for Linux and macOS, a zip for Windows.
      *Verification: CI unpacks each package on its own platform and runs
      `glideslope_cli` from it.*
- [x] **The living documents exist and are honest.** *Verification: someone who
      has not seen the code can say what works from `PROJECT_STATUS.md` alone.*

## Phase 1 — The feel

- [x] **JSBSim, pinned and built**, flying the Cessna 172. *Verification:
      `glideslope_cli` loads the Cessna and prints the figures JSBSim read.*
- [x] **A fixed 120 Hz step with an accumulator.** *Verification: the same
      flight fed its time in different-sized chunks ends in the same state.*
- [x] **Scripted flight checked against published figures** for the Cessna 172.
      *Verification: each figure lands within its tolerance of the handbook's.*
- [x] **State capture and set/resume.** *Verification: an instance restored
      mid-flight tracks the original within tolerance, in every phase of
      flight.*
- [x] **The same-machine replay hash** — `glideslope_cli selftest`.
      *Verification: the hash is identical run to run, and a one-line physics
      change moves it.*
- [x] **Cross-platform flight checks by tolerance.** *Verification: every CI
      platform agrees with the published figures, and with the others, within
      tolerance.*
- [x] **The packaged CLI flies.** *Verification: CI runs `glideslope_cli
      selftest` from every unpacked package.*

## Phase 2 — The world

- [x] **Earth-centred, Earth-fixed positions in double precision.**
      *Verification: conversions round-trip within a millimetre everywhere
      tested.*
- [x] **A camera-relative floating origin.** *Verification: a still scene far
      from the origin shows no jitter.*
- [x] **Reversed-Z depth.** *Verification: distant mountains and a nearby
      aircraft draw with no z-fighting.*
- [x] **The Copernicus DEM, read directly**, with a height query anywhere on
      Earth. *Verification: heights at surveyed airfields and coastlines match
      within the dataset's stated accuracy.*
- [x] **Collision terrain in the simulation.** *Verification: an aircraft rests
      on the DEM at sea level, at a high airfield and on a slope.*
- [x] **A window and a GPU device through SDL3** on Vulkan, D3D12 and Metal.
      *Verification: `--shot` writes a frame on every backend of every
      platform.*
- [x] **Cesium Native drawing the open-data terrain through SDL_GPU.**
      *Verification: a shot of a known region matches its reference frame.*
- [x] **Open imagery on the terrain.** *Verification: a shot shows the imagery
      and its attribution.*
- [x] **Joysticks, HOTAS and yokes.** *Verification: every axis and button of a
      virtual device reaches the controls.*
- [x] **A basic HUD.** *Verification: the numbers in a shot match the simulation
      at that tick.*
- [x] **The client's test flags** — `--shot`, `--shot-at`, `--trace`,
      `--screen`. *Verification: each is used by a ctest.*
- [x] **A frame rendered headless in CI and from every package.** *Verification:
      CI uploads a frame from every platform.*

## Phase 3 — Weather

- [x] **METARs from aviationweather.gov.** *Verification: a recorded METAR sets
      the wind, temperature and pressure it reports.*
- [x] **Winds aloft from Open-Meteo.** *Verification: a recorded response sets
      the wind at every level it reports, and between them.*
- [x] **Weather in JSBSim's atmosphere, with turbulence.** *Verification:
      crosswind drift matches the wind, and turbulence is bounded when on and
      absent when off.*
- [x] **Weather that changes during a flight without a jump.** *Verification: a
      new report blends in with no step in the wind.*

## Phase 3b — Wind that shears and gusts, and hazardous air

Gusts, shear and hazardous air, computed from position, time and the weather's
shared parameters so every machine flies the same air.

- [x] **The same air on every machine.** *Verification: the same weather gives
      the same wind within 1e-9 m/s across platforms, and a restored aircraft
      meets the same gust.*
- [x] **A METAR's gusts flown.** *Verification: a gusty report gives winds
      between its mean and gust speeds; one without gusts gives none.*
- [x] **The wind near the ground as a boundary layer.** *Verification: the wind
      at 10 to 180 m matches the report, with the profile between and below.*
- [x] **Reported wind shear read** from METARs. *Verification: recorded reports
      decode correctly, and runway shear reaches the approach.*
- [x] **Microbursts.** *Verification: an approach through one meets the
      published outflow model's winds.*
- [x] **Thermals and mountain waves.** *Verification: circling in a thermal and
      crossing a ridge give the model's lift and sink.* Done 2026-09-18.
- [x] **Weather you can see** — cloud, rain and visibility. *Verification: a
      shot shows the cloud base where reported, and 3 km visibility hides
      terrain beyond it.* Done 2026-09-18.

## Phase 4 — Autopilot and navigation

- [x] **Holds for heading, altitude, airspeed and vertical speed.**
      *Verification: each captures a step change within a stated overshoot and
      settling time, in calm air and turbulence.* Done 2026-09-18.
- [x] **Waypoint following and flight plans as data.** *Verification: a plan
      loaded from a file passes every waypoint within 100 m.* Done 2026-09-18.
- [x] **The user/AI controller swap.** *Verification: swapping either way
      mid-flight causes no step, in every phase of flight.* Done 2026-09-19.
- [x] **`--autopilot`** — the AI flies this client's aircraft. *Verification: a
      ctest flies a whole plan with it.* Done 2026-09-19.

## Phase 5 — Aircraft choice

Sixteen aircraft (`REQUIREMENTS.md` 4.2), each held to published figures before
it is offered; the five JSBSim lacks are written here.

- [x] **Aircraft as data.** *Verification: an aircraft is added without a code
      change, and a test flies every one.* Done 2026-09-19.
- [x] **A Mosquito flight model.** *Verification: its pilot's notes' and trials'
      figures and handling, each within tolerance.* Done 2026-09-19.
- [x] **The light aircraft fly to their figures** — J-3 Cub, PA-28, Cessna 182.
      *Verification: every handbook figure within tolerance.* Done 2026-09-19.
- [x] **The airliners fly to their figures** — A320, 737, 747, 787-8.
      *Verification: take-off, climb, cruise Mach and ceiling within tolerance.*
      Done 2026-09-19.
- [x] **The F-15 and F-22 fly to their figures.** *Verification: maximum Mach,
      climb, ceiling and turn rate within tolerance.* Done 2026-09-19.
- [x] **An A380 flight model.** *Verification: the airliners' figures within
      tolerance.* Done 2026-09-19.
- [x] **A Learjet 35A flight model.** *Verification: its flight manual's figures
      within tolerance.* Done 2026-09-19.
- [x] **F-35B and B-2 flight models**, held only to what is published.
      *Verification: each published figure within tolerance, with its source in
      `ASSETS.md`.* Done 2026-09-19.
- [x] **The Short S.23 on water.** *Verification: it floats at its draught,
      takes off from sea and lake, and alights and rests afloat.* Done
      2026-09-20.
- [x] **Water where the DEM says it is.** *Verification: water or land as the
      mask says at reference places, and a landplane ditches on water.* Done
      2026-09-20.
- [x] **The HUD for fast aircraft** — Mach and flight level. *Verification: a
      jet's shot matches its state at that tick.* Done 2026-09-20.
- [x] **Visual models from FlightGear aircraft**, each licence checked.
      *Verification: `ASSETS.md` records every shipped model, and a model
      without an entry fails a test.* Done 2026-09-20.
- [x] **A visual model put where its aeroplane is.** *Verification: each model's
      wheels and extremities sit on its flight model's within stated distances.*
      Done 2026-09-21.
- [x] **An aircraft chosen at start.** *Verification: every aircraft can be
      chosen, takes off, and passes its figures.* Done 2026-09-19.
- [x] **Views: the cockpit, outside, and a free orbit.** *Verification: each
      view's shot draws the model where its camera puts it, and switching views
      changes nothing in the flight.* Done 2026-09-21.

## Phase 5b — Terrain providers

- [x] **Cesium ion as a visual provider** with the user's own token.
      *Verification: with a token it draws with attribution; without one it says
      why and its tests skip.* Done 2026-09-21.
- [x] **Google Photorealistic 3D Tiles** with the user's key or ion token.
      *Verification: the same checks as Cesium ion, through both ways in.* Done
      2026-09-22.
- [x] **Attribution on screen for whichever provider is active.** *Verification:
      every provider, in every state, draws its attribution.* Done 2026-09-21.
- [x] **A measured visual-to-collision terrain mismatch.** *Verification: each
      provider's bound at reference airfields is stated and held by a test.*
      Done 2026-09-21.

## Phase 5c — Learning to fly

Checklists and lessons, built on the AI pilot and on aircraft as data. A lesson
ends in a debrief, never a score.

- [x] **Checklists as part of each aircraft's data**, for every phase of flight.
      *Verification: every aircraft has one for each phase, and every item is
      checkable or marked the pilot's to confirm.* Done 2026-09-23.
- [x] **Checklists on screen, ticking themselves.** *Verification: flown by the
      book, items tick as they are done; with the flaps left up, that item is
      flagged.* Done 2026-09-21.
- [ ] **Lessons** — take-off, the circuit, climbs and descents, turns, stalls,
      approach and landing — for each class of aircraft. *Verification: flown to
      the book every stage passes; flown with a stated fault, the debrief names
      that fault and no other.* **In progress.** Missing: the circuit for the
      airliners, fighters, bomber and business jet; the seaplane's take-off
      and circuit, on water. The 747-400 and F-22A are taught turns
      alone.
- [x] **The instructor demonstrates, then hands over.** *Verification: the AI
      pilot flies each lesson within its limits and hands the controls over and
      back with no step.* Done 2026-09-23.

## Phase 6 — Client and server

- [ ] **The transport** — gearstick's, with its own magic value, written up in
      `TRANSPORT.md`. *Verification: a client written from `TRANSPORT.md` alone
      completes a session, and a gearstick client is refused.* **In progress.**
      Missing: the verification itself — no client written from the document
      alone, and no gearstick client to refuse.
- [x] **Reliable delivery** for the session's messages. *Verification: every
      message arrives exactly once and in order under injected loss.* Done
      2026-09-22.
- [ ] **The server** — `glideslope_server`, its flags and a dashboard.
      *Verification: every flag is tested, and a player count outside 1 to 4 is
      refused.* **In progress.** Missing: the dashboard is in the terminal, with
      no ping, traffic or drop control; `REQUIREMENTS.md` 6.6 asks for a window.
- [x] **Lobby, identity and slot assignment.** *Verification: slots come out the
      same whatever order players connect in.* Done 2026-09-22.
- [x] **The server flies every aircraft**, anywhere on Earth. *Verification:
      aircraft on opposite sides of the world fly in one session, each over its
      own terrain.* Done 2026-09-22.
- [x] **Server-run AI aircraft**, default 4. *Verification: a server runs the
      number it is given, and four when given none.* Done 2026-09-22.
- [x] **Inputs streamed with redundancy.** *Verification: no input frame is lost
      under injected loss.* Done 2026-09-22.
- [x] **Client prediction and reconciliation.** *Verification: prediction error
      and corrections stay within bounds at 100 and 200 ms of latency.* Done
      2026-09-22.
- [x] **Other aircraft interpolated 100 ms in the past.** *Verification:
      interpolation error stays within its bound under loss and jitter.* Done
      2026-09-22.
- [ ] **Collisions resolved on the server**, mid-air and with the ground.
      *Verification: two aircraft on a collision course collide on the server,
      and every client shows it.*
- [x] **Every network parser fuzzed** under sanitizers. *Verification: the seed
      corpus goes through every parser in CI.* Done 2026-09-22.
- [ ] **The server's test flags.** *Verification: each is used by a ctest.* **In
      progress.** Missing: `--window-dump`, `--window-shot` and
      `--window-press`, which wait on the dashboard being a window.
- [ ] **Network checks in CI** with injected latency, loss and jitter.
      *Verification: prediction, correction, interpolation and the player limit
      all within their bounds.*
- [x] **`THREATS.md` written.** *Verification: every message the server accepts
      is named with its defence.* Done 2026-09-22.
- [ ] **Deployment** — a systemd unit and a Dockerfile under `deploy/`.
      *Verification: a server started from each accepts a client.* **In
      progress.** Missing: the Dockerfile has never been built, as no Docker
      daemon is reachable here.
- [x] **`--online`** through a one-line `server.txt`. *Verification: a client
      started with `--online` reaches the server it names.* Done 2026-09-22.
- [ ] **Four machines in one sky.** *Verification: four clients on different
      systems and an AI Cessna fly together, extra clients are refused, and
      prediction error stays within bound.*

## Phase 7 — User/AI controller swap across the network

- [ ] **Player to AI.** *Verification: the aircraft flies on with no step, and
      the client interpolates it like any other.*
- [ ] **AI to player.** *Verification: the client resumes prediction from the
      next full state, with no step.*
- [x] **A player disconnecting** — the aircraft removed or handed to an AI, by
      setting. *Verification: both settings tested.* Done 2026-09-22.
- [x] **AI traffic with nobody connected.** *Verification: AI keeps flying on an
      empty server, and a client joining later finds it mid-flight.* Done
      2026-09-22.
- [ ] **Controller-swap continuity in the network checks.** *Verification: swaps
      under injected latency, loss and jitter stay within bound.*

## Phase 8 — LLM copilot

- [ ] **Natural-language commands become flight plans.** *Verification: "take
      off, climb to 3,000 ft and orbit the CBD" produces a plan the autopilot
      flies.*
- [x] **An autopilot that flies an approach and lands.** *Verification: each
      light aircraft lands within 5 m of the centreline under 300 ft/min, in
      calm air and a 10-knot crosswind.* Done 2026-09-21.
- [ ] **The copilot flies with you** — a model that changes the autopilot's
      modes and plan as the flight goes, opt-in with the player's key.
      *Verification: it follows a coast as told, handles an engine failure,
      never slows the step, and replays in CI without a key.*
- [ ] **The model never drives a control surface.** *Verification: the copilot
      can produce only a flight plan and autopilot modes, checked at configure
      time.*
- [ ] **Reinforcement-learning agents** (stretch goal). *Verification: an agent
      trained through JSBSim's gym-style wrappers lands within stated limits.*

---

## Tails

Found while implementing something else. Added when found, not when remembered.

- [ ] **The handshake is not quite the Noise protocol it is named after**, so a
      standard Noise client cannot complete it. *Verification: the handshake
      completes against an independent Noise implementation.*
- [x] **Stalls entered in the landing configuration.** *Verification: the
      stall is entered in the configuration its reference speed was measured
      in, and every aeroplane recovers within the lesson's height.* Done
      2026-09-23.
- [x] **The runway lessons fly at their figures' weight.** *Verification:
      every lesson flies at its figures' weight, with the circuit still flying
      its pattern.* Done 2026-09-23.
- [x] **The PA-28 loses five times the height the other light aircraft lose
      entering a stall.** *Verification: all four enter a stall within the same
      band.* Done 2026-09-23: it was the clean entry.
- [ ] **The F-15C, F-35B and Learjet leave the ground far past their rotation
      speed.** The F-15C lifts off at 230 knots where its flight manual gives
      157. *Verification: each lifts off within ten knots of its rotation
      speed, and can be rotated early.*
- [ ] **The Learjet ends its landing roll nose down through the runway.**
      *Verification: every aeroplane the AI lands ends its rollout upright on
      its wheels.*
- [ ] **A `--terrain ion` run can hang for ever, past its own timeout.**
      *Verification: a timed-out run is gone and leaves no cache lock.*
- [ ] **A published stall speed for the F-15C**, from its flight manual, to give
      the fighters approach and stall lessons. *Verification: the F-15C stalls
      near its published speed, and its class gains both lessons.*
- [ ] **The autopilot banks to its limit even when the aeroplane cannot sustain
      the turn.** *Verification: a light aeroplane near its ceiling holds its
      height through a 90-degree turn as it does at 3,000 ft.*
- [ ] **Cesium ion on Windows, where a body arrives compressed unasked.**
      *Verification: a Windows machine fetches and reads ion's layer.json, and
      the weather still arrives.* Missing: why asking for compression times out
      the send on Windows CI.
- [ ] **A livery on the aeroplane, and its control surfaces moving.**
      *Verification: a shot shows a livery, and the ailerons move with the
      stick.*
- [ ] **The aeroplane is lit by a light baked into its mesh.** *Verification:
      its lighting follows a roll with no mesh remade.*
- [x] **A checklist item's band is not held against what the aeroplane can
      reach.** *Verification: every band is shown reachable, and one outside its
      lever's travel turns the test red.* Done 2026-09-21.
- [x] **Nothing here compiles first-party code under clang.** *Verification: a
      test compiles every source under clang and fails on an unused constant GCC
      accepts.* Done 2026-09-21.
- [x] **One download that fails once reds the tree.** *Verification: a missing
      file is tried three times before failing, and a present one is not
      fetched.* Done 2026-09-21.
- [x] **Tests that run at once share one Cesium cache.** *Verification: the
      client tests run together and no run reports a locked database.* Done
      2026-09-21.
- [ ] **Runways on the DEM**, which shows bumps a runway does not have.
      *Verification: decided in `REQUIREMENTS.md`; if smoothed, reference
      runways roll with no bump beyond a bound.*
- [ ] **Free buildings for the default scenery.** *Verification: a source
      recorded in `ASSETS.md`, and a shot of a city shows its buildings.*
- [ ] **Signed and notarised macOS builds.** *Verification: a downloaded package
      opens with no Gatekeeper warning.*
- [ ] **One Linux download for every distribution** — an AppImage or Flatpak.
      *Verification: one file runs on a fresh Ubuntu and a fresh Rocky.*
- [ ] **A hosted public server**, if one is needed. *Verification: `server.txt`
      names a running server a client reaches with `--online`.*
- [ ] **Terrain over the whole Earth, streamed as an aircraft flies.**
      *Verification: a Sydney-to-Melbourne flight draws terrain the whole way,
      with tiles in memory under a bound.*
- [ ] **Thermals from the ground beneath them, and lee waves trapped under a
      stable layer.** *Verification: no thermal over open water on a convective
      day, and trapped lee waves at the two-layer wavelength.*
- [ ] **Weather seen as it is** — cloud that drifts and has depth, towering
      cumulonimbus, a sky that blends between reports, lit haze, rain out to the
      visibility. *Verification: each shown in shots within stated tolerances.*
- [x] **Every aircraft's airframe meets the ground with its wheels up.**
      *Verification: every aircraft landed wheels up rests on its airframe
      within the stated friction's distance.* Done 2026-09-22.
- [x] **Propeller and mixture levers for the pilot.** *Verification: keys and
      bindings move each lever, and the S.23's and Mosquito's rpm follow them.*
      Done 2026-09-21.
- [x] **Four visual models are drawn sunk into the ground.** *Verification:
      every model's lowest point sits within the same distance of its lowest
      wheel contact.* Done 2026-09-22.
- [x] **The F-15C's airframe slides on the wrong friction.** *Verification: its
      wheels-up landing stops in the stated friction's distance.* Done
      2026-09-22.
- [x] **The F-35A becomes the F-35B.** *Verification: it flies its published
      Mach and range, its model sits on its flight model, and it rests on its
      airframe wheels up.* Done 2026-09-22.
- [ ] **The F-35B cannot hover, land vertically or take off short**: its lift
      fan is not modelled. *Verification: it hovers at its published thrust,
      lands vertically, and takes off in its published short distance.*
- [ ] **The Cesium cache still locks when the rendering tests run together.**
      *Verification: the whole suite at `-j4` reports no locked cache, ten times
      over.*
- [x] **The reliable layer believes an acknowledgement it is told.**
      *Verification: a forged acknowledgement lets go of nothing not yet
      acknowledged.* Done 2026-09-22.
- [x] **No message rejects a number that is not one.** *Verification: every
      floating-point field of every message refuses a NaN and an infinity.* Done
      2026-09-22.
