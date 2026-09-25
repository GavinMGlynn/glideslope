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
- [x] **Lessons** — take-off, the circuit, climbs and descents, turns, stalls,
      approach and landing — for each class of aircraft. *Verification: flown to
      the book every stage passes; flown with a stated fault, the debrief names
      that fault and no other.* Done 2026-09-24. The 747-400 and F-22A are
      taught turns alone, having no stall or climbing speed.
- [x] **The instructor demonstrates, then hands over.** *Verification: the AI
      pilot flies each lesson within its limits and hands the controls over and
      back with no step.* Done 2026-09-23.

## Phase 6 — Client and server

- [x] **The transport** — gearstick's, with its own magic value, written up in
      `TRANSPORT.md`. *Verification: a client written from `TRANSPORT.md` alone
      completes a session, and a gearstick client is refused.* Done 2026-09-24.
- [x] **Reliable delivery** for the session's messages. *Verification: every
      message arrives exactly once and in order under injected loss.* Done
      2026-09-22.
- [x] **The server** — `glideslope_server`, its flags, and a dashboard in the
      terminal by default or in an SDL window with `--window`: who is
      connected, their ping and traffic, what is flying, a log of who came and
      went, and a drop button per player. `--headless` runs with neither, as a
      cloud host does. *Verification: every flag is tested, a player count
      outside 1 to 4 is refused, the window shows the same facts as the
      terminal, its drop button drops that player, `--window` with no display
      is refused, and without `--window` it runs where no display library is
      installed.* Done 2026-09-24.
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
- [x] **Collisions resolved on the server**, mid-air and with the ground: a
      crashed aircraft is a wreck for a few seconds, then flies again from the
      start. *Verification: two aircraft on a collision course collide on the
      server, every client reading the state is told, and both fly again; a
      hard landing wrecks and a good one does not.* Done 2026-09-24.
- [x] **Every network parser fuzzed** under sanitizers. *Verification: the seed
      corpus goes through every parser in CI.* Done 2026-09-22.
- [x] **The server's test flags.** *Verification: each is used by a ctest.*
      Done 2026-09-24.
- [x] **Network checks in CI** with injected latency, loss and jitter.
      *Verification: prediction, correction, interpolation and the player limit
      all within their bounds.* Done 2026-09-24: at 100 and 200 ms, with jitter
      and loss.
- [x] **`THREATS.md` written.** *Verification: every message the server accepts
      is named with its defence.* Done 2026-09-22.
- [x] **Deployment** — a systemd unit and a Dockerfile under `deploy/`.
      *Verification: a server started from each accepts a client.* Done
      2026-09-24.
- [x] **`--online`** through a one-line `server.txt`. *Verification: a client
      started with `--online` reaches the server it names.* Done 2026-09-22.
- [x] **Four machines in one sky.** *Verification: four clients on different
      systems and an AI Cessna fly together, extra clients are refused, and
      prediction error stays within bound.* Done 2026-09-25: Windows and Linux,
      by `tools/four_machines.sh`.

## Phase 7 — User/AI controller swap across the network

- [x] **Player to AI.** *Verification: the aircraft flies on with no step, and
      the client interpolates it like any other.* Done 2026-09-25.
- [x] **AI to player.** *Verification: the client resumes prediction from the
      next full state, with no step.* Done 2026-09-25.
- [x] **A player disconnecting** — the aircraft removed or handed to an AI, by
      setting. *Verification: both settings tested.* Done 2026-09-22.
- [x] **AI traffic with nobody connected.** *Verification: AI keeps flying on an
      empty server, and a client joining later finds it mid-flight.* Done
      2026-09-22.
- [x] **Controller-swap continuity in the network checks.** *Verification: swaps
      under injected latency, loss and jitter stay within bound.* Done
      2026-09-25: at 100 and 200 ms.
- [ ] **Who is flying, and the controls, on screen.** The HUD always says
      whether the pilot or the AI has the aircraft, and a panel shows the
      stick, rudder, throttle, flaps and gear. *Verification: a shot of each
      case shows the right words and each control where the flight model has
      it.*
- [ ] **Ride along in any AI aircraft.** Step into its cockpit, on a server,
      and watch it fly, its controls shown as the AI moves them. *Verification:
      the view and the controls panel match the server's aircraft within
      stated bounds, under injected latency, loss and jitter.*
- [ ] **Take over an AI aircraft.** The one you ride in becomes yours, and the
      one you had goes to the AI pilot; never another player's, and a server
      setting may forbid it. *Verification: a take-over under injected
      latency, loss and jitter shows no step; a request for a player's
      aircraft, or on a server that forbids it, is refused.*

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
- [ ] **A different model on each AI aircraft** - Claude, ChatGPT, or none -
      chosen per aircraft by the server or when an aircraft is handed to the
      AI, each with its owner's key. *Verification: one scenario is planned
      and flown with each provider from its recorded answers, in CI without a
      key; a provider without a key is refused, not faked.*
- [ ] **Reinforcement-learning agents** (stretch goal). *Verification: an agent
      trained through JSBSim's gym-style wrappers lands within stated limits.*

---

## Tails

Found while implementing something else. Added when found, not when remembered.

- [ ] **Windows debug test programs crash on their way out on the development
      machine**, in a thread Windows starts after exit - about one run in five
      of the message tests, on `main` as well (2026-09-25). CI's Windows
      runners have not shown it since it was fixed there. *Verification: the
      message tests run a hundred times on the development machine without a
      crash.*

- [ ] **The client with the window does not hand over on a server**: pressing A
      online does nothing. *Verification: the client with the window hands its
      aircraft to the AI and takes it back on a server, and what it shows does
      not step.*

- [ ] **A headless client drawing thousands of frames runs the software
      Vulkan driver out of memory** (seen in WSL, lavapipe). *Verification: a
      headless client draws ten thousand frames, and its memory stays level.*
- [ ] **A B-2 left mushing for half a minute cannot be recovered by the
      autopilot's stall recovery**, and falls 20,000 ft. *Verification: every
      aeroplane stalled and left for thirty seconds is recovered within its
      lesson's height.*
- [x] **A client assumed the server's clock keeps real time**, and drew other
      aircraft from guesses when a slow server's clock ran behind. *Verification:
      against servers at 80%, 100% and 125% of real time, with jitter and loss,
      the client's clock stays within 20 ms ahead and 50 ms behind.* Done
      2026-09-25.
- [ ] **On Windows a DEM tile can fail to open while another test renames a
      fresh copy into place.** Seen on CI: `cannot open ...S34_00_E151_00_DEM.tif`.
      *Verification: many processes fetching and reading one tile at once on
      Windows all read it.*
- [ ] **Over a network a client's own aircraft is corrected by metres**, because
      the server does not say how far into its latest input it had flown.
      *Verification: through 200 ms with jitter and loss, the worst prediction
      error is under a metre.*
- [x] **Two server tests counted wall-clock seconds on slow runners**: one
      counted inputs still in flight, and a late client arrived before a slow
      server was flying. *Verification: the client waits for its last input to
      be applied, and the late one for the server to say it is flying; each is
      seen to fail on a deliberate bug.* Done 2026-09-24.
- [x] **The hooks' test, run by the pre-push hook, committed into the
      repository being pushed** and set it bare. *Verification: run with git's
      hook variables naming a decoy repository, it leaves the decoy exactly as
      it was.* Done 2026-09-24.
- [x] **CI's actions run on Node.js 20, which GitHub has deprecated.**
      *Verification: a CI run's annotations name no action as targeting
      Node.js 20.* Done 2026-09-24.
- [x] **Windows debug programs can crash on their way out**, when Windows
      starts a thread as they exit. *Verification: a program can still
      allocate in the last call the loader makes into it, and the tests that
      crashed run hundreds of times without a crash.* Done 2026-09-24.
- [x] **The state-stream test measured the runner, not the server.**
      *Verification: every update is counted against the simulation's own
      steps, exactly, and one dropped in ten fails it.* Done 2026-09-24.
- [x] **The handshake is not quite the Noise protocol it is named after**, so a
      standard Noise client cannot complete it. *Verification: the handshake
      completes against an independent Noise implementation.* Done 2026-09-24:
      both ends match the `cacophony` implementation's known-answer vector byte
      for byte.
- [x] **Two players can be given the same aircraft number.** A slot is a key's
      rank, so a player whose key sorts first moves everyone after them; the
      server numbers a player's aircraft by the slot it had on arrival, and a
      later player can be handed a number already flying. *Verification: four
      clients joining in the reverse of their keys' order each fly their own
      aircraft, under four different numbers.* Done 2026-09-24.
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
- [x] **The Learjet ends its landing roll nose down through the runway.**
      *Verification: every aeroplane the AI lands ends its rollout upright on
      its wheels.* Done 2026-09-24: the Learjet already stopped level; three
      jets bounced off the runway and one rocked a wingtip on to it. Jets now
      land as jets are landed - nose down, spoilers out, brakes on.
- [ ] **The F-35B's circuit touches down two kilometres short of the
      runway**, at 165 knots, and rolls on to it. Nothing checks where along
      the runway a circuit touches. *Verification: every circuit touches down
      on the runway, past its threshold.*
- [ ] **Taking an aeroplane back on its landing roll does not finish the
      landing**: it is handed the plain autopilot, which never stops it.
      *Verification: an approach taken back on the roll is landed to a stop.*
- [ ] **The B-2A cannot slow down on the approach.** With nothing to add drag
      it crosses the threshold fourteen knots fast with its throttles shut,
      and floats nearly two feet off the runway after it touches.
      *Verification: the B-2A crosses the threshold within five knots of its
      reference speed.*
- [ ] **A `--terrain ion` run can hang for ever, past its own timeout.**
      *Verification: a timed-out run is gone and leaves no cache lock.*
- [ ] **A published stall speed for the F-15C**, from its flight manual. The
      fighters' approach and stall lessons fly a stall measured on the model,
      which gives a 196-knot reference speed. *Verification: the F-15C stalls
      near its published speed, and its approach is flown at the manual's.*
- [x] **The autopilot banks to its limit even when the aeroplane cannot sustain
      the turn.** *Verification: a light aeroplane near its ceiling holds its
      height through a 90-degree turn as it does at 3,000 ft.* Done
      2026-09-24: all four light aeroplanes, through quarter turns and full
      circles, each way, at two speeds.
- [ ] **The altitude hold flies an aeroplane into the stall when asked for a
      height it cannot hold.** Above its ceiling it keeps pitching up at full
      throttle until the Cessna is at 46 knots and sinking. *Verification: a
      light aeroplane asked for a height above its ceiling gives up height, not
      airspeed, and never drops below its best-climb speed.*
- [ ] **The AI never leans the mixture**, so on the autopilot a light
      aeroplane's ceiling is about 8,500 ft - the Cessna 172P's handbook gives
      13,000. *Verification: the AI climbs each light aeroplane to within its
      handbook's tolerance of its published service ceiling.*
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
