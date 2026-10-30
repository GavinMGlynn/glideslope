# Completion plan

What is left to do and where we are up to, in phases: one short point per item,
saying what it is, how you would know it works and, for an open item, what is
missing. The detail - what was built, what was measured, what was found on the
way - is in `PROJECT_STATUS.md`. Why a feature exists is in `FEATURES.md`, and
the design decisions behind it in `REQUIREMENTS.md`.

**`[x]` means 100% of the item, and nothing less.** An open item names what is
missing. An item without a verification cannot be ticked. Tails found while
implementing something go in the phase they belong to, among its open items
(owner, 2026-10-10: there is no Later section; everything is implemented
eventually).

`[x]` done · `[ ]` not started, or **In progress** where the text says so

**The plan is finished when this returns nothing:**

```sh
sed -n '/^## Phase /,$p' docs/COMPLETION_PLAN.md | grep '^- \[ \]'
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
- [x] **A Windows job's driver download that hangs is given up and tried
      again**, where three jobs on 2026-10-28 waited out their whole 45
      minutes with no test run. *Verification: a fetch that never answers is
      given up after its time limit and tried again, then fails with its
      reason; every Windows job of its own CI run installs the driver
      through it.* Done 2026-10-28.
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
- [x] **The server** — `glideslope_server`, with a dashboard in the terminal,
      or in a window with `--window`, or neither with `--headless`.
      *Verification: every flag is tested, a player count outside 1 to 4 is
      refused, the window shows the terminal's facts and its drop button
      drops that player, `--window` with no display is refused, and without
      it the server runs where no display library is installed.* Done
      2026-09-24.
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
- [x] **Who is flying, and the controls, on screen.** The HUD always says
      whether the pilot or the AI has the aircraft, and a panel shows the
      stick, rudder, throttle, flaps and gear. *Verification: a shot of each
      case shows the right words and each control where the flight model has
      it.* Done 2026-09-25: the panel is lines in the HUD's font.
- [x] **Ride along in any AI aircraft.** Step into its cockpit, on a server,
      and watch it fly, its controls shown as the AI moves them. *Verification:
      the view and the controls panel match the server's aircraft within
      stated bounds, under injected latency, loss and jitter.* Done 2026-09-25:
      its ground speed shown, not its airspeed, which is not sent.
- [x] **Take over an AI aircraft.** The one you ride in becomes yours, and the
      one you had goes to the AI pilot; never another player's, and a server
      setting may forbid it. *Verification: a take-over under injected
      latency, loss and jitter shows no step; a request for a player's
      aircraft, or on a server that forbids it, is refused.* Done 2026-09-25:
      T in the cockpit, a step under 2 m at 200 ms.

## Phase 8 — LLM copilot

- [x] **Natural-language commands become flight plans.** *Verification: "take
      off, climb to 3,000 ft and orbit the CBD" produces a plan the autopilot
      flies.* Done 2026-09-29: Claude and ChatGPT each planned it, and each
      plan takes off and circles the CBD; CI flies both from their recorded
      answers, with no key.
- [x] **An autopilot that flies an approach and lands.** *Verification: each
      light aircraft lands within 5 m of the centreline under 300 ft/min, in
      calm air and a 10-knot crosswind.* Done 2026-09-21.
- [x] **The copilot flies with you** — a model that changes the autopilot's
      modes and plan as the flight goes, opt-in with the player's key.
      *Verification: it follows a coast as told, handles an engine failure,
      never slows the step, and replays in CI without a key.* Done
      2026-09-30, in `glideslope_cli` and on a server, where the player's own
      client asks the model and the server flies only the route it sends.
- [x] **The model never drives a control surface.** *Verification: the copilot
      can produce only a flight plan and autopilot modes, checked by the
      build: what it sees, opens and links.* Done 2026-09-25. It was first
      "at configure time", which review showed could be got round.
- [x] **A different model on each AI aircraft** - Claude, ChatGPT, or none -
      chosen per aircraft by the server or when an aircraft is handed to the
      AI, each with its owner's key. *Verification: one scenario is planned
      and flown with each provider from its recorded answers, in CI without a
      key; a provider without a key is refused, not faked.* Done 2026-10-02,
      for the server's AI aircraft, a player's own hand-over, and one left by
      a player who goes or by a take-over.
- [x] **Reinforcement-learning agents** (stretch goal). *Verification: an agent
      trained through JSBSim's gym-style wrappers lands within stated limits.*
      Done 2026-09-30: the Cessna 172P's learnt landing touches down within
      5 m of the centreline under 300 ft/min from all 27 approach starts, in
      calm air and a 10-knot crosswind either way, from a quarter to full tanks;
      `glideslope_cli land c172p --learnt` flies it.

---

## Phase 9 — Aircraft and flight models

Engines, figures, airframes, aerodynamics and stalls: each aircraft flown to what its handbook or manual publishes.

- [x] **A tail-wheel aeroplane started on the ground balances on her main
      wheels.** Started level, the J-3 Cub went over on her nose at idle on
      her brakes in her first two seconds on the CI builds, and the
      Mosquito tipped to -6 degrees; now each starts at the attitude she
      stands at. This is the first cause of the gear item "a rolling
      aeroplane near a stop can be thrown on her nose" (#195); the second,
      in a crosswind near a stop, is the item below. *Verification: every
      tail-wheel aeroplane started at rest stands on all three wheels at
      her standing attitude.* Done 2026-10-14.
- [x] **A nose-wheel aeroplane braked to a stop in a crosswind is pitched
      by her alpha-rate terms.** As she stops, the air meets her from the
      side, her angle of attack is undefined and its rate swings; multiplied
      by the whole dynamic pressure, it pitched a C172P at up to 62 deg/s
      and struck her tail. Those terms now use the dynamic pressure without
      its sideways part, as JSBSim's maintainers advise. This is the second
      cause of the gear item of #195. *Verification: every nose-wheel
      landplane braked to a stop in a 15 kt crosswind, at every brake level,
      keeps her pitch rate under 20 deg/s.* Done 2026-10-14.
- [x] **The Mosquito's alpha-rate term counts the crosswind.** Her model was
      written by hand, so the change above did not reach her; it is now made
      by a script like every other model, and her term is changed.
      *Verification: her alpha-rate term, and every other model's, uses the
      dynamic pressure without its sideways part; her figures stay in
      range.* Done 2026-10-14.
- [x] **The J-3 Cub's flight goes NaN standing on macOS release**: braked
      fully with her stick central she went over on her nose there and her
      flight blew up; now stood as a pilot stands a tail-wheel aeroplane
      (stick back, pedals half on). *Verification: the test that every
      aircraft standing on her brakes at idle stays finite and on the ground
      passes on every platform.* Done 2026-10-14: green on every platform
      in CI run 38454086267.
- [x] **The Cherokee's full-throttle speed traced and her plan's fastest
      measured again.** She makes 126.8 kt level at 3,000 ft, not 129, since
      #142 flew her full rich below 5,000 ft on the FAA's mixture curve, as
      her handbook says; her plan's fastest is now 112 kt. *Verification:
      the change traced to the commit that made it, and her figure measured
      again.* Done 2026-10-10.
- [x] **Every nose-wheel aeroplane strikes its tail where its airframe
      would.** Each strike is measured from her visual model (the Learjet's
      from her drawings) and is a contact in her flight model; five strike
      with their wheels just lifted (PROJECT_STATUS, 2026-10-13).
      *Verification: every nose-wheel aeroplane's tail strikes the runway
      where its airframe would, and is judged a strike, with every take-off,
      approach and landing test green.* Done 2026-10-13.
- [x] **The F-35B takes off in her required short distance**: 592.5 ft,
      699 without the estimated 20-degree flaperon droop (PROJECT_STATUS,
      2026-10-13). *Verification: it hovers at its
      published thrust, lands vertically, and takes off in its published
      short distance.* Done 2026-10-13.
- [x] **A pilot and an AI plan fly the F-35B between jet-borne and
      wing-borne flight**: a pilot's STOVL selector (H and N) and stick, and
      a plan's `land_vertically`, fly her transition law; and since
      2026-10-16 a player asks for a loading as she joins, so a player's
      F-35B light enough to hover is converted, hovered and landed
      vertically through a real server, her prediction agreeing
      (PROJECT_STATUS, 2026-10-14 and 2026-10-16). *Verification: she is
      flown into a hover and out again, and a pilot and an AI plan each
      convert her.* Done 2026-10-16.
- [x] **The windowed client's `--loading` is flown by a test**: alone, through
      a server's word, and on a take-over either way (to an F-35B loaded
      otherwise, and back to the one left at her loading), she begins at the
      weight her loading gives. *Verification: three window tests read her
      weight against her figures file's 37,530 lb vertical_landing loading,
      red with the client's loading call, the take-over's loading or the
      server's word removed.*
      Done 2026-10-22.
- [x] **The F-35B's pilot flies only her height in STOVL mode**: her
      throttle's speed, the stick's sideways translation and the pedals'
      heading in a hover are not the pilot's, and a plan given to the AI
      while she hovers is not flown. *Verification: a pilot flies her
      jet-borne at a chosen speed and moves her sideways in a hover, and a
      plan given her hovering converts her back and is flown.* Done
      2026-10-14: the lever asks a ground speed near the hover and an
      acceleration above it, the stick a sideways speed and the pedals a
      turn, each within 2 kt or a degree a second of what is asked, every
      direction flown; a plan given her hovering is flown (PROJECT_STATUS,
      2026-10-14).
- [x] **The F-35B flies backward at 30 kt in the hover**, the published
      figure: her pitching moment no longer jumps where flying backward puts
      her angle of attack, at 180 degrees, which had swung her past 20 kt.
      *Verification: she holds 30 kt backward within 2 kt, averaged over
      12 s, her speed swinging under 6 kt and her pitch within 15 degrees.*
- [x] **The F-35B's lift no longer steps where flying backward puts her
      angle of attack, at 180 degrees**, as it did by about 1,300 lb at
      30 kt back; reversed, her lift is lopsided and unsourced, as the owner
      accepts. *Verification: her lift either side of 180 degrees within
      a stated bound, and every figure in range.*
- [x] **Jet-borne, the F-35B's stick and pedals turn her**: the stick
      rolls her and, centred, holds her bank, as the VAAC Harrier's unified
      law does; the pedals turn her heading. *Verification: jet-borne at
      60 kt, the stick full over turns her at least 3 degrees a second,
      banked under 30, and held when centred; full pedal moves her heading
      at least 5 degrees in 10 s; her height within 15 ft.*
- [x] **A client joining while an F-35B banks jet-borne predicts her with
      the bank held**: the server tells a client given her while her law
      flies her the whole of it (`LAW_STATE`, protocol 10), and the client
      begins on it. *Verification: a client joining mid-bank predicts the
      server's held bank and agrees with it within a stated bound
      (2026-10-22, through a server: 20 degrees held, her pitch within
      0.017 degrees, the median 6 mm; without the message 2.784 degrees).*
- [x] **A player taking over or back an F-35B in STOVL mode is predicted
      from the server's law**: a plan file may name her loading, so an AI
      F-35B hovers to be taken over. *Verification: through a server, a
      client taking over or back an F-35B whose law flies her is told it,
      her pitch within half a degree.* Done 2026-10-22: taken over
      hovering 0.005 degrees, taken back hovering 0.004 and wing-borne
      with STOVL still selected 0.009. Untold: taken back hovering 3.632
      degrees, taken back wing-borne 0.306 (taken over untold not measured).
- [x] **A player who joins already selecting STOVL mode has her law seeded
      as the server's**: her prediction begins on the controls the server
      says it holds her on, and a law made again in a replay is seeded as it
      was first. *Verification: a client selecting STOVL on its very first
      input, through a real server, has its transition law seeded as the
      server's is - the same throttle integral at the step both make it
      (2026-10-21: 0.4685201574755089 on both, to the 17th digit).*
- [x] **A player's aircraft the server holds untouched is trimmed**: she
      starts trimmed level, wings and all, and holds what the trim found
      until her first input. *Verification: through a real server, a
      player's F-35B and 172P untouched for thirty seconds hold their start
      height within 100 ft (2026-10-20: 7 ft and under 1 ft).*
- [x] **The F-22A, the Mosquito and the S.23 trimmed, and the C182 with her
      propeller governed**, so a player's aircraft left untouched holds her
      height; the F-22A in pitch alone, the trim she is held on told her
      client's prediction; a trim that fails is said in the server's log; the
      S.23's boost control settled through her trim, her drag due to lift
      her wing's and her airscrews' efficiency NACA's. *Verification: every one of
      the sixteen, started as a player's and untouched for thirty seconds,
      within 20 ft of her start height (2026-10-22: within 7 ft at 3,000 ft);
      and the S.23 trimmed and held at every loading at 1,000, 5,000 and
      10,000 ft at her slowest, start and fastest speeds (2026-10-23: 27 of
      27).*
- [x] **Trimmed at every height, speed and loading a player or plan may start
      at, her levers as her book flies there** (the owner, 2026-10-23): an
      airborne start leans her mixture for the most power above her
      handbook's height and puts two-pitch airscrews in coarse in cruise,
      held so until her first input and told her client. *Verification: the
      catalogue's walk of loadings, three heights and three speeds leaves out
      only points past each aircraft's book, named with their source
      (2026-10-23: 524 of 540 trimmed, 16 listed, each failing).*
- [x] **A Learjet 35A given no loading flies inside her limits**: her
      model's own loading is now a crew of two and her wing and tip tanks
      full, at 28.1% of her chord, inside her flight manual's 14.1 to 30%,
      and she trims at 300 kt; the J-3 Cub's, which carried no one forward
      of her limit, is flown solo from the rear seat. *Verification: a walk
      of every aircraft's own loading against the envelope her books
      publish - six inside, ten named unpublished - finds the Learjet and
      the Cub inside (seen red on each), and the trim walk takes the
      Learjet's 300 kt points off its left-out list.*
- [x] **A player's first input keeps the trim the server held**: her
      client begins its inputs from every control the server held her on,
      the aileron and rudder her trim found held as roll and yaw trim, so a
      centred stick keeps her trimmed, and her prediction agrees. *Verification:
      through a real server, a predicting F-35B and C172P, touched not at
      all or nudged and centred, hold their height within 100 ft and their
      attitude within 5 degrees for thirty seconds, the prediction within its
      bounds; every aircraft's first input is what she was held on.*
- [x] **The first-input test's nudge is held to the steps the server flew
      it**: a nudged aircraft's bank and pitch bounded per step, as the server
      counts them. *Verification: within it at every load tried; red when
      never nudged and on the trims held as the stick.*
- [x] **A predicting F-35B on a starved machine is put tens of metres off**:
      a client stalled between flying and sending learnt its clocks'
      difference seconds wrong, and a server catching up its own slow start
      put a client joining it 5 to 14 steps off. Both clients now sample the
      difference from when each input was sent, and a server's start is not
      owed. *Verification: a predicting F-35B whose client is stalled 3 s is
      off by no more than four steps' travel, in both clients, with the
      server's start built slow (2026-10-28: one step; seen red at 136 m
      and 962 m).*
- [x] **A client's prediction of an F-35B in STOVL mode follows the
      server's flight-control laws**: the server tells the height asked and
      the wing-borne autopilot's pitch integral; the converting, jet-borne
      and hover laws need no telling once both change stage on the same step
      (measured within 0.012 degrees). *Verification: through a real server,
      held wing-borne two minutes, selected at any moment, including before
      the prediction has settled, the client's predicted pitch within half a
      degree of the server's at every update (2026-10-22: selected on her
      first input, 0.004 degrees over 3,000 updates).*
- [x] **A STOVL prediction flown on 0.8 of the stick is caught**: what her
      stick, pedals and lever set in her law is held against the server's at
      every step, in all four stages. Read: the stick fore-and-aft in each
      stage, and the stick sideways, pedals and lever jet-borne and
      hovering - ten cells of sixteen. **Not read: the stick sideways, the
      pedals and the lever wing-borne and converting** (six), where her
      autopilot flies her heading and speed. *Verification: a test red on that bug, comparing
      what the stick sets, not only the height; seen red in the converting
      stage too (780 of 780 steps apart).*
- [x] **A STOVL prediction's law changes stage when the server's does**: a
      replay from before the selector no longer flies with the fan's doors
      commanded open. *Verification: both laws change stage on the same
      step, and the converting stage's stick compared (2026-10-22: 48,228
      steps, none apart; 780 converting, none apart).*
- [x] **The Mosquito's model is never level after a stall warning with full
      flap and gear: her own pitching moment.** Recovered at her stall
      warning at 20,000 ft, 45 degrees of flap and the gear down, she loses
      3,095 ft and is never level again; at 140 kt she pulls 2.1 g with her
      elevator half nose-down (PROJECT_STATUS, 2026-10-10, the stall
      recovery closed by the owner's decision). *Verification: her model
      level at her lesson's recovery speed with full flap and gear, or her
      lesson flown in the configuration her Pilot's Notes give, and her
      recovery at the warning within 600 ft.* Done 2026-10-10: closed by
      flying her stall lesson at 5,000 ft, where she can level with
      everything down - recovered at her warning she loses 46 ft; at 20,000
      ft she still cannot, as her Notes' climb figure implies. It was her
      engines' power there, not her pitching moment. No period or RAF height
      for practising a stall was found; 5,000 ft is the light aeroplanes',
      and her Notes' climb with everything down is a low-level figure. A
      stall lesson begun away from its height now says so first.
- [x] **The Mosquito, recovered from a stall and flown by the AI, keeps the
      10% margin.** *Verification: her recovery and the AI's notice within
      their bounds with the 10% in hand, and unnamed.* Done 2026-10-13: 1.69 g
      and 4 kt over her stall (PROJECT_STATUS, 2026-10-13).
- [x] **The Learjet cannot be rotated early**: full back stick lifts her
      nose only near her rotation speed. *Verification: the Learjet held
      fully back from 85 percent of her rotation speed leaves the runway
      before it, and her take-off lesson calls it early.* Closed 2026-10-09
      by the owner's decision on the measured figure: she leaves at 124.1
      knots against her rotation speed of 125.3 - before it, but inside the
      five knots her lesson allows, so it does not call it early. The
      shortfall, recorded: no source ties her stabilizer's take-off setting
      to the flight model's, or gives her elevator's power.
- [x] **The F-15C's stall, held to her flight manual's speed**: at full
      aft stick she settles at 100 knots or less; her angle of attack is
      reported, not judged. *Verification: held at full aft stick, the
      F-15C settles at 100 knots or less.* Closed 2026-10-09 by the owner's
      decision on the measured figure: on NASA TN D-8052's lift she settles
      at 110.1 KCAS against the manual's 100, a 10-knot shortfall recorded -
      100 needs 21 per cent more lift and drag than any source gives her.
- [x] **The leaner richening an engine that stops while leaned has no
      test.** *Verification: an engine the leaner had leaned, stopped in
      flight, is richened and runs again.* Done 2026-10-06: at 7,000 and
      12,000 ft. Richened to full rich, as it was, at 12,000 ft it never ran
      again; it is now given back the mixture it was leaned to.
- [x] **The Cessna 172P's engine makes 209 hp from 160.** Corrected, the
      learnt landing trained on the old engine lands too hard, so the policy
      needs training again (PROJECT_STATUS, 2026-10-08). *Verification: the
      engine makes 160 hp, and its figures and the learnt landing stay
      within theirs.* Done 2026-10-09: 160.4 hp, every figure in range, and
      the policy trained again on it lands from all 160 gate corners at 190
      ft/min at worst.
- [x] **The Cessna 182S's climb falls away high up**: leaned, it reaches
      about 13,000 ft against its handbook's 18,100. Its engine and
      propeller check out; the chart needs much less drag high up than the
      model has, which is the owner's call (PROJECT_STATUS, 2026-10-09).
      *Verification: the AI
      climbs it to within 10% of 18,100 ft, and its other figures stay
      within theirs.* Done 2026-10-09: its wing's drag due to lift was a
      third too high for its shape; corrected, and the windmilling
      propeller's drag charged only in the glide, it climbs to 17,039 ft
      and all eleven figures are in range. The handbook's sea-level climb
      and ceiling cannot both be met closely by any drag polar; both are
      inside 10%, at +8% and -6%.
- [x] **The Cherokee's engine makes most power far too rich**, at 9.9 parts
      of air to one of fuel against the FAA's 12 to 13.8; the other three
      light aeroplanes are fixed (PROJECT_STATUS, 2026-10-08). On the FAA's
      curve its stall at the warning is never recovered, with #136's
      recovery too: it cannot be level at the lesson's recovery speed with
      40 degrees of flap (PROJECT_STATUS, 2026-10-09). *Verification:
      leaned for best power, each engine sits between 12 and 13.8 to 1, and
      every figure stays in range.* Done 2026-10-09: on the FAA's curve she
      leans to 12.9-13.2 to 1, and her figures are in range.
- [x] **The Cub's carburettor runs too rich to climb past about 8,000 ft.**
      It has no mixture lever, and JSBSim enriches every engine as the air
      pressure falls, where a float carburettor enriches only as the square
      root of the density; so modelled, it climbs to 12,300-15,600 ft solo
      against its manual's 14,000. *Verification: flown solo, the Cub climbs
      to within 10% of 14,000 ft.* Done 2026-10-06: a float carburettor's
      metering, and the FAA's mixture curve; solo it climbs to 13,637 ft.
- [x] **The Cherokee is leaned below 5,000 ft, where its handbook has it
      full rich.** Held full rich there, its stall recovered at 4,950 ft
      loses 328 ft against its lesson's 300; on the FAA's mixture curve it
      is not recovered at all: with 40 degrees of flap at the recovery's
      speed it sinks 190 ft/min at full power, though slower it climbs; the
      least-sink recovery of #136 does not change that - the lesson's
      recovery would have to raise the flap or ask a speed she can hold. *Verification: the Cherokee full
      rich below 5,000 ft, and its stall recovery within its lesson.* Done
      2026-10-09: the recovery raises her flaps to her handbooks' 25-degree
      climb-out setting, and she loses 131 ft of 300.
- [x] **A checklist item's band is not held against what the aeroplane can
      reach.** *Verification: every band is shown reachable, and one outside its
      lever's travel turns the test red.* Done 2026-09-21.
- [x] **Propeller and mixture levers for the pilot.** *Verification: keys and
      bindings move each lever, and the S.23's and Mosquito's rpm follow them.*
      Done 2026-09-21.
- [x] **Four visual models are drawn sunk into the ground.** *Verification:
      every model's lowest point sits within the same distance of its lowest
      wheel contact.* Done 2026-09-22.
- [x] **The F-35A becomes the F-35B.** *Verification: it flies its published
      Mach and range, its model sits on its flight model, and it rests on its
      airframe wheels up.* Done 2026-09-22.
- [x] **Stalls entered in the landing configuration.** *Verification: the
      stall is entered in the configuration its reference speed was measured
      in, and every aeroplane recovers within the lesson's height.* Done
      2026-09-23.
- [x] **The PA-28 loses five times the height the other light aircraft lose
      entering a stall.** *Verification: all four enter a stall within the same
      band.* Done 2026-09-23: it was the clean entry.
- [x] **The F-15C, F-35B and Learjet leave the ground far past their rotation
      speed.** The F-15C lifts off at 230 knots where its flight manual gives
      157. *Verification: each lifts off within ten knots of its rotation
      speed, and can be rotated early.* Done 2026-09-26: all thirteen
      landplanes, at every loading, leave within ten knots of their rotation
      speed for their weight and sooner when rotated early; the F-15C's model
      speed is itself 21 knots above its manual's.
- [x] **The Learjet ends its landing roll nose down through the runway.**
      *Verification: every aeroplane the AI lands ends its rollout upright on
      its wheels.* Done 2026-09-24: the Learjet already stopped level; three
      jets bounced off the runway and one rocked a wingtip on to it. Jets now
      land as jets are landed - nose down, spoilers out, brakes on.
- [x] **The B-2A cannot slow down on the approach.** *Verification: the
      B-2A crosses the threshold within five knots of its reference speed,
      and rises less than half a foot after it first touches.* Done
      2026-10-02: flown down with her drag rudders half open she crosses at
      124.1 knots against 124.0, and flared to her wheels she rises 0.0 ft.
      A pilot opens them by hand since the speedbrake item below.
- [x] **A pilot has no control for the speedbrakes.** No stick, throttle or
      key binding moves the speedbrake lever, so by hand the B-2A cannot open
      its drag rudders and an airliner cannot use its spoilers.
      *Verification: the speedbrake lever is moved from a stick, a throttle
      quadrant and the keyboard, and the bomber lessons' bands come back to
      what the AI flies.* Done 2026-10-07: a stick's and a quadrant's buttons,
      the quadrant's last lever, and the semicolon and apostrophe keys move
      it; the HUD shows it where there are speedbrakes; the bomber lessons'
      bands are `vref+12` and `vref+15` again, and flown by hand with the
      lever half out the B-2A stays inside, and stowed she does not.
- [x] **The F-15C's speedbrake draws no drag.** Her model moves the surface
      but its aerodynamics ignores it, so the lever does nothing in her.
      *Verification: with the lever out she slows, held level, as the
      airliners do.* Done 2026-10-07: the F-15 aerodynamic database's drag,
      31 knots slower in ten seconds from 300; and as there, it stays in
      past 15 degrees of alpha.
- [x] **The Learjet's stabilizer cannot trim her in cruise**, so a pilot
      flying by hand holds the stick forward. *Verification: the Learjet
      cruises from 250 to 350 knots with its elevator near neutral.* Done
      2026-09-30: her stabilizer's travel is her maintenance manual's.
- [x] **Every aircraft's airframe meets the ground with its wheels up.**
      *Verification: every aircraft landed wheels up rests on its airframe
      within the stated friction's distance.* Done 2026-09-22.
- [x] **The F-15C's airframe slides on the wrong friction.** *Verification: its
      wheels-up landing stops in the stated friction's distance.* Done
      2026-09-22.
- [x] **The F-15C landed wheels-up rocks from wing tip to wing tip, and on
      Windows it now breaks up.** *Verification: the F-15C rests on its
      airframe without rocking, and the wheels-up test passes on every
      platform.* Done 2026-09-26.
- [x] **The S.23's hull is not a planing hull**: its lift is a flat clip,
      its buoyancy has a tail above the water, and nothing models the
      afterbody, so she skips and porpoises on the step. *Verification: her
      hull from a cited planing model (Savitsky, with an afterbody term from
      NACA's porpoising work), her take-off figures in range, and in calm
      air she holds the step without porpoising.* Done 2026-10-14: she
      hops 0.32 ft on the step against 0.5.
- [x] **A rolling aeroplane near a stop can be thrown on her nose by her
      gear in one step.** The Mosquito tips from brake 0.65 at every
      loading (0.60 at five before her slipstream lift was defined,
      2026-10-20): her real behaviour, not changed for it (owner,
      2026-10-17), unmeasured on a real aeroplane. *Verification: a sweep at
      every loading and 0.05 of brake pins the level, both sides.* Done 2026-10-17.
- [x] **The J-3 Cub's nose is pushed down by a crosswind**: her
      angle-of-attack pitch term counts it. *Verification: that term, and
      every other model's, uses the dynamic pressure without its sideways
      part; stick central in 15 kt across her tail stays down; her figures
      stay in range.* Done 2026-10-15: all sixteen models' Cm-alpha terms
      moved, each justified; braked in 15 kt across the Cub does no worse
      than in calm air; every figure in range.
- [x] **The J-3 Cub is rolled onto her wing by a crosswind at walking
      pace**: held at 3.5 to 5 kt in 15 kt across she rolled 21 degrees and
      struck. *Verification: her lift uses the dynamic pressure without its
      sideways part, and held at every walking pace in 15 kt across she stays
      on her wheels.* Done 2026-10-15: her wing's drag counted the crosswind
      too and grew as she leaned. With both moved, held at each pace from 2
      to 5 kt for 40 s (31.7 s or more at it), then stopped, she leans at most
      4.4 degrees, every loading, either side.
- [x] **A J-3 Cub standing tail to the wind meets a reversed wing's cited
      drag.** Her wing's drag past 90 degrees of angle of attack was a flat
      plate's 1.41; it now runs on to 180 degrees by a wing section's
      measured curve (Sandia, SAND80-2114). *Verification: stood in 15 kt
      from behind, the air's force on her is within a stated bound of a
      cited figure.* Done 2026-10-16: 29 lb where it was 195, within 0.01 of
      the cited figure as her model scales it, at every loading.
- [x] **A J-3 Cub standing tail to the wind has no lift from her wing, and
      an unchecked pitching moment.** *Verification: cited figures for a
      reversed wing's lift and an aeroplane's pitching moment tail to the
      wind, and hers held to them.* Done 2026-10-21: her lift past 90
      degrees is Sandia's reversed section's, her moment derived from it
      (owner's decision); in 15 kt from behind her wing presses her down
      86 lb where it gave nothing, and 200 ft-lb of nose-up moment where
      there were 4,150, both within 0.01 of the figures.
- [x] **The J-3 Cub leans 4.4 degrees standing in 15 kt across.** Her
      sideways drag in a beam wind is the pinned model's, 1.23 on her wing's
      area, and her main gear's springs are soft; whether a real Cub leans
      so is not measured. *Verification: a cited figure for a Cub's side
      area or lean, and her lean held to it.* Done 2026-10-17: her side area
      from her Owner's Manual (48.7 sq ft) with Hoerner's flat plate
      broadside (1.17); standing in 15 kt across she is pushed 43 lb, not
      162, within 0.005 of the figure, and leans 2.2 degrees at most.
- [x] **The J-3 Cub's main gear springs are the pinned model's**, 1,000 lb
      a foot a wheel, from no source. *Verification: a cited figure for a
      Cub's gear stiffness or her static deflection, and hers held to it.*
      Done 2026-10-20: no published figure exists, so her rate is derived
      from her shock rings' rating (730 lb at 100% stretch) and Piper's gear
      drawings, at 2,093 lb a foot a wheel; both main wheels are held to it
      within 1% (owner's decision of 2026-10-20, a derived figure).
- [x] **The J-3 Cub sags about 3 in a wheel standing, where a real one
      barely does**: her gear springs have no preload, her shock rings'
      pre-stretch. *Verification: her static deflection held to a cited or
      derived figure.* Done 2026-10-22: her rings' 40% pre-stretch holds
      each strut closed up to 731 lb a wheel, so she stands on her tyres
      (Goodyear's 8.00-4 at 15 psi) and sinks 1.1 to 1.7 in, within 0.2% of
      the derived figure at every loading.
- [x] **Every model but the Cub has her lift keyed to the angle of attack
      counting the crosswind**, as their pitch terms did: standing or slowing
      in one, her lift is the beam wind's whole pressure times an undefined
      angle (the Cub's is moved, 2026-10-15). *Verification: each model's lift
      terms on the dynamic pressure without its sideways part or named with a
      reason, and every figure in range.* Done 2026-10-15: 16 terms moved in
      15 models and 3 named (CLq in the F-22A, PA-28 and S.23), as the test
      asserts; the Mosquito's slipstream lift is named in her script, which
      the test does not read. Standing in 15 kt across they lifted the
      landplanes by up to 7,260 lb, now nothing; the S.23 afloat by up to
      2,100 lb, now 20 to 410. Every figure in range.
- [x] **Every model but the Cub has her wing's drag keyed to the angle of
      attack counting the crosswind**, as her lift did. Now every model's is
      moved, 18 terms, or named, 4: standing in 15 kt across they drew the
      airliners and the B-2A along the wind by up to 11,060 lb, now nothing.
      *Verification: each model's drag terms keyed to the angle of attack on
      the dynamic pressure without its sideways part or named with a reason,
      and every figure in range.* Done 2026-10-16.
- [x] **Lift not keyed to the angle of attack counts the crosswind
      standing**: flaps, elevators and the C182's lift at no angle give her
      up to 46 lb in 15 kt across, the F-22A 280. *Verification: each such
      term moved or named with a reason, and every figure in range.* Done
      2026-10-23: all 38 such terms in the 16 models moved, lift with pitch
      rate among them, and the 4 lifts with pitch rate keyed to the angle,
      named until now, too; standing in 15 kt across with any lifting
      control set, from up to 6,450 lb to under 0.1 lb. Every figure in
      range.
- [x] **Pitching moments not keyed to the angle of attack count the
      crosswind standing**: the elevators', flaps' and pitch damping's
      moments are on the whole dynamic pressure, as their lift was;
      unmeasured. *Verification: each such term moved or named with a
      reason, her pitch standing in 15 kt across measured before and after,
      and every figure in range.* Done 2026-10-28: all 48 such terms in the
      16 models off the crosswind, and 4 of the 5 keyed to the angle named until
      now too (the S.23's pitch damping stays named); standing in 15 kt across with any control set, from up to
      199,000 ft-lb to under 21. Every figure in range.
- [x] **Her side force standing in a beam wind is turned fore and aft**:
      JSBSim's wind axes put it in her plane of symmetry, pitching her and
      putting the J-3 Cub over braking in a crosswind (figures in the
      2026-10-28 status file). *Verification: every landplane standing in
      15 kt across, her side force across her and its pitching moment
      within a stated bound, and every figure in range.* Done 2026-10-29:
      every model tapers her side force to nothing standing in a beam
      wind, where her drag with sideslip pushes her across; the air's force in her plane of symmetry fell from up to
      10,400 lb to under 19, and the Cub no longer goes over braking in a
      crosswind. Every figure in range.
- [x] **The Mosquito's slipstream lift flips with an undefined angle of
      attack**: on her brakes at +9 boost it is -2,570 or +2,050 lb as her
      creep flips the angle from -93 to +87 degrees. *Verification: standing
      at any power, calm or 15 kt across, her slipstream lift within a stated
      bound, and every figure in range.* Done 2026-10-20: read at the angle
      the air in each slipstream meets her wing, within its bound everywhere.
- [x] **The J-3 Cub braked stick central from 10 kt noses over in calm
      air**, from 0.55 of brake solo, 0.50 trainer and 0.60 gross; owner's
      decision 2026-10-20: that is her real behaviour (a Cub does go over on
      her brakes, NTSB LAX07CA166), her model unchanged. The level itself is
      unmeasured on a real Cub. *Verification: at each of her three
      loadings, braked at every 0.05 from 10 kt, she does not go over below
      the pinned level and does at or above it; seen red with the level
      pinned wrong and her brakes' grip lowered.* Done 2026-10-21.
- [x] **A C172P at full power on held brakes sat on her tail, or went over
      her nose**: her elevator worked on the whole slipstream. Now only the
      part of her tailplane the slipstream reaches is in it (her handbook's
      Figure 1-1 and momentum theory). *Verification: stick central she
      stands on three wheels at every loading, as her handbook's short-field
      take-off has her; stick full forward she never noses over; stick full
      back, measured and named unsourced.* Done 2026-10-16: stick back she
      sits on her tail only at her aft limit; every figure in range.
- [x] **The C172P's rudder works on the whole slipstream**, as her elevator
      did. *Verification: her rudder in the part of the slipstream that
      reaches her fin, from a cited figure, and every figure in range.* Done
      2026-10-20: 0.545 of her fin is in it, by her handbook's Figure 1-1;
      the learnt landing trained again on her.
- [x] **The walking-pace tests' throttle sits the C172P on her tail.**
      Holding her at 3 kt against the autobrake, the test's hand winds to full
      power with her stick back, and she rears to 11 degrees. *Verification:
      no landplane rolled at walking pace and stopped touches the runway with
      anything but her wheels.* Done 2026-10-15: every landplane is held at
      every pace from 2 to 5 kt by half a knot, dry, wet and in 15 kt across,
      then stopped by the rollout - 273 runs, none touching but her wheels.
- [x] **The macOS network checks keep time on a stalling runner** (red
      tree, worked now). The predicting client went 265 to 498 ms without a
      frame on CI's macOS: it built its aircraft in a frame as it joined and
      at every take-back. Now built ahead, a fresh one for each take-back;
      time the machine did not run it is the machine's, up to a second.
      Its own branch's nightlies fail on the controls item's bound (39, 38
      and 39 of 40, the last on its final head, the frames 39 ms apart at
      most), so its evidence is with that item on top, and the two land
      together. *Verification: the nightly's macOS network checks green in
      two runs with the controls item on top, and its PR's CI.* Done
      2026-10-22: 200 of 200 in each of two runs of 100 a check, the second
      on the final head.
- [x] **The paced prediction on CI's Windows release went 138 m off** once
      (2026-10-21), the runner not running the client for 3,503 of the 3,583
      ms between two frames: the test judges the machine. *Verification: the
      test judged on the client's own time, or a stall built into it
      explicitly and its bound held through it.* Done 2026-10-22: both
      clients count the updates across a stall their machine took apart and
      name them, and every update after is judged as before; a built 3.5 s
      stall of client and server is back within a metre 4 to 8 updates on.
- [x] **The paced prediction through a stall went 4.2 m off on CI's macOS**:
      the stall was named after the words that came through it were read.
      *Verification: a built stall with words waiting is counted right and
      holds every bound, red without the fix.* Done 2026-10-23.
- [x] **The windowed client's take-over back to the F-35B it left was
      refused on CI's Ubuntu** (2026-10-22): riding along on a timer, it
      chose from the sky before the take-over, in the number the aircraft
      left had given up. W now waits for an unanswered take-over's answer,
      the update that answers is taken in for everybody, and the test waits
      on events. *Verification: the test red with each fix removed, green
      with both under load.* Done 2026-10-23.
- [x] **The impaired checks' AI controls are shown 0.0051 to 0.0061 off**
      on CI's macOS (nightly, 2026-10-15 and 2026-10-21; 0.005 is the bound):
      the throttle drawn at 0.9949 where it was 1.0, past a step to full
      power, across a 0.29 s gap in the updates. Now held across a gap to
      what the throttle's and a hand's rates allow. *Verification: the cause
      found, and the bound held, or a stated reason to change it, in 100
      macOS runs of each impaired check.* Done 2026-10-22: 200 of 200 on
      the final head (an earlier 100's one failure, a prediction error after
      the join, was the client's build and is fixed under it).
- [x] **A predicting client on a starved machine never caught up**: it
      replayed its inputs for every update. Now only the newest replays.
      *Verification: a client made too slow keeps up within bounds, red
      without the fix.* Done 2026-10-23.
- [x] **The window client compares only each frame's newest update.** Now
      both clients compare every word they read at that word's own step. A
      word about a step not yet flown waits until that step is flown. The only
      words not compared are counted, each with its reason: those read before
      the first could be placed, as a prediction begins, and those dropped at
      a take-back, a take-over or a let-go.
      *Verification: every update it reads is compared, counted in a test.*
      Done 2026-10-24.
- [x] **A client's prediction asserted in JSBSim on a NaN after two water
      wrecks** (CI's Ubuntu, run 39883628869). Closed on a guard and a trap
      by the owner's decision of 2026-10-23, the cause unknown: a prediction
      given a NaN as an input, or left with one by a step, is let go of and
      started again from the server's word, in both clients. A NaN made
      inside one JSBSim step from finite inputs still asserts in a debug
      build; only glideslope_cli's trap names it - the window client has
      the guard alone. A prediction held on the water after
      flying again was found and fixed on the way.
      *Verification: a prediction given a NaN is let go of and each client
      flies on, red without the guard; the held prediction pinned, red
      without its fix.* Done 2026-10-23.
- [x] **The STOVL at-once tests' predicted pitch varies from run to run**:
      0.58 and 0.96 degrees on CI's Windows against the half-degree bound,
      0.20 on Linux. Fixed 2026-10-28: both laws now fly her on the trim she
      was held on, the windowed client's too (2026-10-29). In two full CI
      runs every platform's figure is under 0.033 degree, and each test's
      bound is twice its most. *Verification: both laws fly her on the same
      pitch trim, red with the server's old seeding; each at-once test's
      pitch within a stated spread on every run and platform.*
      2026-10-29: the 0.032 that failed was the step words were placed at,
      not the law; the tests now judge her pitch where she was, 0.003 at
      most measured, bound 0.010 (`docs/status`).

---

## Phase 10a — Navigator, plans and take-off

The autopilot's navigator and the plans it flies: orbits, glides, the speeds a plan may ask, the take-off, and the lessons' autopilot.

- [x] **The take-off autopilot rotates at a held rate.** The airliners and
      the B-2A do, at three degrees a second, and the light aeroplanes, the
      fighters and the Learjet at four.
      *Verification: every nose-wheel aeroplane at every loading rotates no
      faster than its stated rate on her wheels, and every take-off and
      hand-over test stays green.* Done 2026-10-14: the F-22A's flight
      controls were given a ground mode, as fly-by-wire aircraft have, and
      she comes up at 4.5 to 4.7 degrees a second; her rotation speed,
      re-measured, is 150 kt (the Air Force's one published figure: 143).
      The ground mode also engages on every landing, bounce and
      touch-and-go, which is not measured: nothing lands her yet.
- [x] **The autopilot holds a low height loosely in an airliner's take-off
      configuration.** Asked to hold 200 ft with her take-off flap and gear
      out, the A380 swings 15 ft either side of it at up to 350 ft/min (488
      when climbed to it at 500 ft/min) for minutes before it settles.
      *Verification: every airliner holding a height at her take-off flap
      settles within 100 ft/min in a minute.* Done 2026-10-13: the throttle
      no longer chases the speed a climb trades for height; all five settle
      in 12 to 32 s.
- [x] **A jet, the Mosquito or the S.23 slowed from cruise into her climb
      sinks 2 to 6.4 kt past her climb speed.** Only a light aeroplane has
      a speed floor to capture it with; the others' climb speeds are not a
      best-rate climb to hold as one. *Verification: the capture test's six
      named aeroplanes within 2 kt, unnamed.* Done 2026-10-13: every
      aeroplane now holds her climb back while her throttle comes up, as a
      light aeroplane did; all within 0.9 kt, none named, and the test now
      flies them with the gear up, which it never had.
- [x] **The B-2A's and Mosquito's slowest glides are under their plan's
      slowest.** Measured again with `glideslope_cli glide-speeds`: 169 and
      129 kt, their plan's slowest, and every other aeroplane's figure is
      what the command writes now; their glide tests are renamed, and a test
      holds every aeroplane's slowest glide to the first a glide is asked.
      *Verification: each figure is what the command writes now.*
- [x] **A plan's first leg strays further since the take-off climbs at the
      published speed.** Traced: the stray is her turn onto a leg that
      starts where she is, and a turn's size grows with the square of her
      speed, so the faster climb strays more, not less (the 172P 63 -> 80 m).
      The autopilot's own turn laws, flown at her speed, predict every
      aeroplane's stray within 2 m, and the test now holds each within 4 m.
      *Verification: the extra stray traced to its cause, and either removed
      or explained in the test.*
- [x] **The navigator turns at a waypoint only once abeam of it.** Done
      2026-10-13: it flies a waypoint by, leading the turn and flying its
      arc onto the next leg, unless the plan marks it to be flown over, at
      any angle and however short the legs, cutting a waypoint by no more
      than 278 m where she can turn that tight; the autopilot now holds any
      bank it is asked for. Every aircraft strays at most 0.063 of her turn
      radius (the Cherokee at her climb speed), against 1.1 to 1.6 before.
      *Verification: every aircraft, turning 90 degrees at a waypoint at her
      climb and cruise speeds, strays off the next leg less than a tenth of
      her turn radius at that speed and 25 degrees of bank.*
- [x] **A glide may still be asked of a jet at its approach speed.** Each
      aircraft's slowest glide is measured with its gear up, and every
      aircraft glides round unstalled at every speed it may be asked to
      glide at - the F-22A at one speed, 225 kt.
      *Verification: every aircraft glides round its tightest orbit at every
      speed a glide may be asked at without stalling, measured.*
- [x] **A jet's orbit entered from a waypoint swings 460 m off its circle.**
      Flying Claude's plan round the CBD, the 747-400 and F-22A held their
      height but strayed up to 460 m off 7 and 9 km circles, against the
      60 m a Cessna holds. *Verification: each jet flies a model's orbit
      within 60 m of its circle after the first quarter-turn, and within the
      100 m join band before it.* Done 2026-10-09: entered from a waypoint
      anywhere in or near it, a jet could swing 6 km off; the navigator now
      joins the circle only going its way round and banks onto it by a
      loiter law after ArduPilot's. Every jet, from 60 entries each, calm
      and in a 20 kt wind, holds within 10 m after the first quarter-turn,
      and Claude's two plans within 7 m.
- [x] **The B-2 and the F-22 yaw from side to side in a crosswind when
      slow.** Done 2026-10-10: flown below each plan floor as an approach is,
      every aircraft holds its sideslip within a degree (the F-35B under 181
      kt is named under "The F-35B lands on her power"). *Verification: every
      aircraft holds a heading in a 20 kt crosswind from its approach speed
      up, its sideslip within a stated bound.*
- [x] **The autopilot's stall recovery, held to what a stall lesson can
      ask of it** (decided 2026-09-30, REQUIREMENTS 4.3). *Verification:
      every aeroplane taught a stall is recovered within 2 g both handed over
      at its stall warning, within its lesson's height with no exceptions,
      and left thirty seconds in it, within a height worked out for it from
      its speed and sink.* Both checks exist, the instructor flies the
      recovery, engaging or letting it go steps no control, and the AI pilot
      notices a stall warning and recovers (the A380 and Mosquito named;
      the 747-400 and F-22A, with no published stall, not watched; the
      warning is the landing configuration's, so late clean or banked).
      The lesson's entry holds the height level to the warning, and every
      aeroplane left thirty seconds is recovered within 2 g. Closed
      2026-10-10 by the owner's decision on the measured figures, two
      short at the warning: the Learjet 35A loses 385 ft against 350 and the
      Mosquito is never level after her warning, from her own pitching
      moment (an open item in Phase 9). The S.23, short then, loses 89 ft
      against 200 since her lift past the stall was fixed (2026-10-24).
- [x] **The AI notices a stall the J-3 Cub is not in, in moderate
      turbulence.** Done 2026-10-10: a plan's slowest is raised for the
      weight flown and keeps 10 kt over the stall warning; a light aeroplane
      slowed into her climb captures it within 1.3 kt; and in gusts or
      turbulence every speed the AI climbs or holds from a plan or route,
      and the light climb floor, is half the gust factor faster (the AFH's
      turbulent-air rule), up to the plan's fastest. *Verification: every
      aeroplane cruising, climbing and at its plan's slowest in moderate
      turbulence stays 3 kt over its warning and is never noticed, none
      named, and every stall-notice test still passes.*
- [x] **A light aeroplane's take-off hands over slower than the plan's
      climb floor.** Done 2026-10-09: the take-off climbs her at her
      handbook's best-climb speed whatever she weighs, as the plan does, and
      the autopilot takes her on without pitching up, easing the take-off's
      climb down to the plan's; the worst, the 182S, 1.1 kt over. *Verification: every light aeroplane's
      first leg is climbed within 2 kt of one climb speed, from the
      hand-over.*
- [x] **On tight, slow orbits the navigator flies inside the circle.**
      *Verification: an orbit at the tightest radius allowed, at the approach
      speed, is flown within a stated distance of its circle, measured, not
      assumed.* Done 2026-09-30: every light aeroplane within 60 m, Claude's
      CBD plan within 19 m; what cannot be flown is the two tails below.
- [x] **A plan may fly a jet clean at its approach speed**, a flaps-down
      figure: round their tightest orbit at it, six jets come down to the
      ground and the A320 loses 600 ft. The planner needs a clean floor for
      each aircraft. *Verification: every aircraft holds every speed a plan
      may ask of it round its tightest orbit, measured.* Done 2026-10-02:
      each aircraft's slowest and fastest are measured and in its figures
      file; plans, the copilot's routes and plan files outside them are
      refused; all sixteen hold their height and speed round the orbit
      every 5 kt between them.
- [x] **At its fastest, a fast aircraft's tightest orbit is flown well off
      its circle.** Round 13 to 19 km circles at 300 to 360 kt the jets
      hold their height and speed but wander up to 18% of the radius off the
      circle (3.3 km, the F-15C); the light aeroplanes stay within 60 m.
      *Verification: every aircraft at the fastest a plan may ask holds its
      tightest circle within a stated distance, measured.* Done 2026-10-06:
      once round the join every jet holds within 2% of the radius and the
      rest within 60 m, on Linux and Windows; a plan's fastest now leaves 5
      kt of full-throttle speed in hand, which brought six aircraft's down.
- [x] **A model cannot plan the 747-400 or the F-22, nor a copilot route
      them.** Neither publishes an approach or a take-off speed, and a
      model's plan takes off. *Verification: a model plans each of the
      sixteen aircraft and its plan is flown.* Done 2026-10-06: their take-off
      speeds are measured from their models (the owner's decision); each of
      the sixteen is planned and routed from its own speeds, and takes off;
      Claude's plans for the two are recorded and flown round their orbits.
- [x] **In wind the Cub and the Cherokee yaw from side to side.** Holding
      a heading in a 10 kt crosswind, with no plan, the autopilot's J-3 Cub
      and PA-28 sideslip 35 degrees either way every few seconds; in calm air
      they do not. (The Cessnas did too, at cruise.) The autopilot now damps
      yaw, and their orbits in wind are flown again. *Verification: every
      light aeroplane holds a heading in a 20 kt crosswind with its sideslip
      within a stated bound.*
- [x] **Offer the learnt landing in a session.** A player's 172 on final
      is handed to it at its gate on a server and landed - L in the window
      client, which says why on its HUD when refused - and an AI 172 whose
      flight plan ends in a landing (`land`) is flown on to the final approach
      and handed to it at the gate. *Verification: an AI aircraft on a server is
      landed by it when asked, and a client's aircraft handed over at the
      gate is too.* Done 2026-10-08: the AI flying `sydney-arrival.plan` and
      the window client's 172 both landed within 5 m and 300 ft/min and
      stopped on the runway; the window client refused with its flaps moving,
      told why, and handed over by L.
- [x] **On the DEM at Sydney's 16R the runway is not flat enough to take off
      from.** *Verification: every aeroplane takes off from 16R on the DEM,
      and what the collision ground under a runway may do is measured.* Done
      2026-10-06: flattened 2026-10-01, and with the 747-400 and F-22A given
      take-off speeds, all fifteen landplanes take off from 16R and fly on to
      the orbit; the ground under seven reference runways is measured and held.
- [x] **After the take-off hands over, the Learjet 35A and the Mosquito come
      down on Botany Bay.** *Verification: every aeroplane that takes off
      from 16R flies the plan to its orbit with nothing wrecked.* Done
      2026-10-06: the plan asked both for 80 knots, below their stalls. The
      fix is the 2026-10-02 refusal of such plans; the test now flies each
      at speeds it may be planned at, and all thirteen reach the orbit.
- [x] **Runways that meet at different slopes still pull each other's
      surface.** *Verification (the owner's, 2026-10-07): no runway in the
      world pulled more than 0.1 m off where pavements overlap, and the
      overlaps' worst measured and stated.* Done 2026-10-07: the nearest
      runway now wins; off the overlaps none is pulled more than a
      millimetre; on them the worst is 0.61 m (LKMB 04/22), 262 over 0.1 m.
- [x] **The runway lessons fly at their figures' weight.** *Verification:
      every lesson flies at its figures' weight, with the circuit still flying
      its pattern.* Done 2026-09-23.
- [x] **The autopilot banks to its limit even when the aeroplane cannot sustain
      the turn.** *Verification: a light aeroplane near its ceiling holds its
      height through a 90-degree turn as it does at 3,000 ft.* Done
      2026-09-24: all four light aeroplanes, through quarter turns and full
      circles, each way, at two speeds.
- [x] **The altitude hold flies an aeroplane into the stall when asked for a
      height it cannot hold.** *Verification: a light aeroplane asked for a
      height above its ceiling gives up height, not airspeed, and never
      drops below its best-climb speed.* Done 2026-09-25, for all four light
      aeroplanes; other classes have no such floor.
- [x] **A plan flown after the take-off keeps the take-off flap out.** The
      Cherokee climbs its first leg and cruises with 25 degrees, so the
      climb gets no floor and, full rich, its speed bleeds to 52 knots
      (PROJECT_STATUS, 2026-10-09). *Verification: every light aeroplane's
      flaps are up once the plan has it, and its first leg is flown at its
      climb speed.* Done 2026-10-09: the take-off raises its flap a notch at
      a time, at its climb speed, from 50 ft for a light aeroplane and 400
      for any other, before the plan has her; all 16 aircraft checked, and
      the Cherokee's first leg strays 46 m where it strayed 151.
- [x] **Runways on the DEM**, which shows bumps a runway does not have.
      *Verification: decided in `REQUIREMENTS.md`; if smoothed, reference
      runways roll with no bump beyond a bound.* Done 2026-10-01: flattened
      for every runway the data places; reference runways within 5 cm.

---

## Phase 10b — Landing, go-around and the AI pilot

The AI's approach, flare, touchdown, roll-out and go-around, and the learnt landing.

- [ ] **The AI cannot land every aeroplane in gusts**: she now flies half
      the gust factor faster with the power on to the touch, and in moderate
      turbulence lands only two of fourteen within limits with a margin on
      eight seeds and every platform; gusts magnify each platform's floating
      point, and the 787-8, A320, C172P, C182 and Cub that pass on Linux
      fail on macOS or Windows, the 737-300 and A380 balloon, the F-15C and F-35B come down
      hard or nearly, the B-2A puts a wingtip down, the Mosquito bounces, and
      the flying boat comes down hard; at her light loading the 737-300 balloons, and the
      Mosquito at her heaviest. *Verification: every aeroplane lands light,
      heavy and in gusts with none named, and a gust on an approach flown
      well makes no go-around.*
- [x] **In a steady headwind the S.23 skips off the water after
      alighting.** The rollout now judges her lift by her airspeed in all
      air, and the thirteen landplanes roll out into 15 kt without lifting
      off. *Verification: in a steady 15 kt headwind every aeroplane rolls
      out without lifting off.* Done 2026-10-14: on her planing hull the
      S.23 rises 0.20 ft (0.23 in a 10 kt tailwind), and all fourteen are
      judged.
- [ ] **The rollout's autobrake winds up when there is nothing to slow.**
      It keeps adding brake while her deceleration falls short. Blown
      sideways at a walking pace, a C172P had her brakes wound up and was
      put on her nose. *Verification: a rollout held at low groundspeed with
      no deceleration keeps her brake bounded and her nose above -5 degrees.*
      Below 5 kt the autobrake now adds no brake past 0.3, or 0.45 at a
      creep, wet or dry (2026-10-14). **Still missing**: the walking-pace
      tests roll her at 2-5 kt under the autobrake for about 2 s at most;
      reaching the rolling bound is required from 3 kt (3.5 for the aircraft
      named, 2.5 wet on nose-wheel aeroplanes), and below that is named;
      the 0.8 allowance for an idle creep is barely exercised on the jets.
- [ ] **The Mosquito swings below walking pace.** Held at every walking
      pace with her brakes worked as the rollout's rudder bar works them, she
      turns at most 4.4 degrees after the hand-over (2026-10-15); but rolled
      up from rest in 15 kt across she swings 47 degrees off the runway below
      2 kt, where that steering brake fades to nothing. *Verification: every
      landplane rolling at walking pace stops within 30 degrees of her
      heading, in calm air and 15 kt across.*
- [ ] **The flying boat lands in gusts without skipping.** Her flare is
      flown to her hull and she touches gently, but in gusts she touches
      flat - under one degree, below NACA TN 923's lower trim limit - and
      skips 1.7 to 8 ft off the water, worse than on main (0.8 to 1.6 ft):
      the planing hull's centre of pressure and the flat touch both add to
      it. Her gust flare should put her on the water at her calm touchdown
      trim. *Verification:
      the S.23 lands in calm air and in gusts within the landplanes'
      limits.*
- [x] **The learnt landing's policy flies a C172P it was not trained on.**
      It flies her slipped, up to 18 degrees in the flare in calm air; with
      her lift's crosswind moved it lands up to 100 m shorter and 105 ft/min
      harder than it did, within its limits. *Verification: the policy trained
      again on the C172P as committed, its parity fixture from that training.*
      Done 2026-10-20: trained on from part 7's checkpoint on her with her new
      rudder (parts 8 to 10), chosen with every test's figure 5 per cent inside
      its limit; parts 1 to 7 still cannot be made again from committed scripts.
- [ ] **The learnt landing's fidelity to the C172P as she now flies**: it
      still passes every limit, but was trained before her flaps', elevator's
      and pitch-rate lift left out the crosswind (2026-10-23), and now lands
      39 to 69 m shorter than it did. Owner's decision 2026-10-24: a fidelity
      item, not a fault. *Verification: the policy trained again on the
      C172P as committed, its parity fixture from that training.*
- [ ] **The learnt landing flies its approach up to 35 kt fast**, so the
      stabilized-approach gate does not judge it. By the owner's decision of
      2026-10-10, a larger project: its flare trained as its own
      stage, or started from the approach autopilot's flare. The gate's
      judging waits on branch `learnt-landing-gate-wip`. *Verification:
      from every corner of its gate it is stabilized by 500 ft and lands.*
- [x] **A contaminated runway is flown when reported, and nothing reports
      one.** The simulation brakes on every FAA runway condition code, but a
      METAR's weather says only that rain or snow is falling, not how deep it
      lies; Europe's runway-state group, which does, is not read. Nor is a
      runway still wet after the rain stops. *Verification: a report of
      slush or ice on a runway makes it slippery to the aircraft on it.*
      Done 2026-10-14: a METAR's runway-state group, in either form, is read
      and the runway it names given the FAA code its deposit and depth make;
      a C172P braked from 40 kt stops in 78 m dry, 292 m in reported slush
      and 550 m on reported ice. Rain that has stopped keeps the runway wet
      for half an hour, an estimate. The runway list is fetched without ever
      holding up the server or a client, and the server tells every client
      its runways' conditions, so a client joining after the rain agrees
      with it. **Its reach is small**: since ICAO dropped the group from
      METARs in November 2021 few States send it - Russia's still do, every
      report; none of 20 other European and Central Asian stations sampled
      did - and the US never has.
- [ ] **A copilot or planner is not told a runway is reported
      contaminated.** Its landing there is now refused or sized by the FAA's
      factors for the code (2026-10-29), but the model is not shown the
      condition until refused: that changes every prompt, so it waits on the
      13 recordings made again. *Verification: a landing on a runway reported
      icy is refused, or sized from a published contaminated-runway figure.*
- [x] **A wet runway does not change a rolling tyre's grip sideways.**
      Wet wheels now keep the share of their dry side grip NASA measured
      (TN D-7743, checked against TN D-7351). Rolled out wet in 15 kt
      across, braking held equal, every jet slides sideways at least 1.2
      times as fast with the wet grip as with the dry. *Verification: a wet
      runway's side grip from a published source, and a crosswind roll-out
      held to it.* Done 2026-10-14: no assertion covers the light
      aeroplanes, whose soft tyres barely lose grip, or the Mosquito, who
      slides less wet; on snow, slush and ice the share is extrapolated.
- [ ] **The AI's roll-out drifts 10 to 21 m from the centreline when a 15
      kt crosswind arrives**, dry as wet. *Verification: every landplane
      rolled out as 15 kt arrives stays within 5 m of the centreline.*
- [x] **The AI lands the J-3 Cub within her published crosswind limit**,
      12 kt: one instructor's "about 12 knots" in an EAA Inspire article
      (Piper published none), accepted by the owner on 2026-10-30.
      *Verification: a published limit, and the Cub landed within it.* Done
      2026-10-29: every whole knot to 12, both sides, dry and wet - 48
      landings; she lands in 15 kt too, the ground loop gone since her lift
      left out the crosswind.
- [x] **Wet runways judged with each aircraft's own tyres and brakes.** The
      wet grip is read for each landplane's own tyre pressure and anti-skid,
      softer tyres gripping more and a light aeroplane's brakes without
      anti-skid less. *Verification: each aircraft's tyre pressure and brakes
      are sourced and used.* Done 2026-10-13: four pressures (F-22A, F-35B,
      B-2A, Mosquito) are stated estimates.
- [x] **Three overrun a regulation-length wet runway**, braked fully. The
      five airliners now have thrust reversers, used on the roll as their
      makers' manuals say, and the 787-8 and A320 stop on it; the F-15C's
      wet runway is sized from her own flight manual's wet distance, and
      she stops on it. The touches were never fast. *Verification: each
      stops on it.* Done 2026-10-14: her wet distance is read from a chart,
      to about 500 ft.
- [ ] **A pilot has no key or lever for reverse thrust**: the reverse lever
      is in the controls and on the wire, and only the AI moves it.
      *Verification: a binding moves it, as every other lever's does.*
- [ ] **The F-35B lands on her power**: her model flies the glidepath at
      19.5 degrees of incidence and its flare runs out of nose; level at
      3,000 ft below 181 kt the same incidence is past the autopilot's 15
      degrees of nose and she sinks (PROJECT_STATUS, 2026-10-10). *Verification:
      the F-35B flies her approach at her published incidence and flares
      with her throttle closing.* Owner 2026-10-09: no public approach angle
      of attack.
- [ ] **A runway too short refused for the five with no public landing
      figure, and the runways a model is shown say their length.** Since
      2026-10-13 the J-3 Cub, Mosquito, F-35B and B-2A, none published, are
      refused a runway shorter than the one their model lands in (measured,
      PROJECT_STATUS); the F-22A has no approach speed, so every landing is
      refused her, and no figure is measured for her. Still missing: the
      runway lines a model is shown do not say their landing length or what
      the aircraft needs - saying so changes every model's prompt, so all 13
      copilot recordings must be recorded again, which waits on the owner.
      *Verification: each of the five refused a runway shorter than a cited
      figure, and the model shown each runway's landing length.* Owner
      2026-10-09.
- [ ] **Learnt landings for other aircraft.** Only the Cessna 172P has one.
      *Verification: each light aircraft's policy lands within the
      autopilot's limits from the same starts, in calm air and a 10-knot
      crosswind.* Owner 2026-10-09: needs RL training per aircraft; the
      172P's is retrained first.
- [x] **A landing taken over on its roll is braked for a dry runway**: a
      wet or contaminated one, which needs more, is not known.
      *Verification: on a wet short runway an aeroplane handed over on its
      roll still stops on it.* Done 2026-10-10: rain in the session's METAR
      wets the runway, the wheels grip as the FAA's wet-runway rule says,
      and 12 of 13 landplanes stop on a wet runway 1.15 times their dry need
      (the F-35B named: she had no landing distance; all 13 since
      2026-10-13, hers measured); a session's copilot and planner refuse a runway long enough dry
      but short wet. Contaminated runways are flown when given, and a
      METAR's runway-state group gives them (done 2026-10-14, Phase 10b).
- [x] **A landing handed over in a skip is not landed.** Handed to the AI
      the moment a pilot's landing has bounced its wheels clear, even by an
      inch, she is given the plain autopilot, which never stops her (a 787
      on a wet runway ran 23 km). *Verification: a landing handed over in a
      skip is landed to a stop.* Done 2026-10-10: every landplane handed
      over with her wheels clear of the runway, within a second and three
      feet of their last contact, is landed to a stop.
- [x] **A take-off handed to the AI at lift-off is not climbed away.**
      Handed over the moment her wheels leave the runway, she is given the
      plain autopilot, which holds the height she had: 7 of 13 landplanes
      touch the runway again within 30 seconds. *Verification: every
      landplane handed over at lift-off climbs away and never touches the
      runway again.* Done 2026-10-10: handed over at any moment of the ten
      seconds after lift-off, or leaving the runway from a touch-and-go, she
      is given the take-off's own climb-out to 500 ft, and all 13 climb away
      without touching; one levelled low down is not taken for a take-off.
- [x] **The F-35B's circuit touches down two kilometres short of the
      runway**, at 165 knots, and rolls on to it. Nothing checks where along
      the runway a circuit touches. *Verification: every circuit touches down
      on the runway, past its threshold.* Done 2026-09-27: every circuit and
      every approach lesson now touches down on the runway, past its
      threshold; the approach autopilot held the F-35B's nose below what her
      glidepath needs.
- [x] **The flare starts at no more than 10 degrees of pitch**, so an
      aeroplane on the glidepath above that (the F-35B) has its nose pushed
      down at 30 ft and touches down flat and fast. *Verification: every
      aeroplane's flare begins at the attitude it flew the glidepath at, and
      its touchdown pitch is below its tail-strike attitude.* Done 2026-09-27.
- [x] **Taking an aeroplane back on its landing roll does not finish the
      landing**: it is handed the plain autopilot, which never stops it.
      *Verification: an approach taken back on the roll is landed to a stop.*
      Done 2026-09-27: every landplane, taken back at the touch, at half
      speed or after the pilot's own touch, stops on the runway upright; the
      flying boat, never still afloat, is left out.
- [x] **A landing the pilot flies with no approach given to the AI is
      still handed the plain autopilot** when the AI takes it back on the
      roll, and is never stopped. *Verification: an aeroplane landed by
      hand and taken back on the roll is landed to a stop.* Done 2026-10-06:
      every landplane so handed over stops on the runway, held to the line
      she rolls along and braked at autobrake 3.
- [x] **The AI knows no runway for a landing flown by hand**: taken over on
      the roll it holds her track and brakes at a fixed rate, not for the
      runway left. *Verification: a landing flown by hand on a short runway
      is stopped on it by the AI.* Done 2026-10-08: she is given the world's
      runway she rolls on and braked for what is left of it; every landplane
      stops on a 1,500 m runway, where six ran off it.
- [x] **An A320 taken at the touch by a pilot who lets the stick go rises
      4.1 ft after the AI takes her back**, against three for everything
      else. *Verification: every landplane taken back at the touch rises
      less than three feet after it.* Done 2026-10-06: she rises 0.8 ft,
      and her named bound is gone.
- [x] **The AI's own touchdown in the 787-8, F-15C and F-35B is a crash by
      the server's rule** (707, 976 and 883 ft/min, past the gear's 600).
      *Verification: every aeroplane the AI lands touches down within what
      its gear takes.* Done 2026-10-01: flared to the wheels, the hardest
      touches at 469 ft/min (the F-15C) and none rises half a foot after.
- [x] **Nothing bounds where along the runway a jet touches down**: they
      touch 520 to 809 m past the threshold, and an F-15C at 800 m would run
      off a 6,000 ft runway. *Verification: every aeroplane the AI lands
      touches inside a touchdown zone stated for it.* Done 2026-10-06: every
      one in both lessons touches inside the FAA's touchdown zone, the first
      3,000 ft, at 246 to 725 m.
- [x] **The F-15C balloons in her flare**, climbing at about 100 ft/min at
      five feet before settling. *Verification: no aeroplane the AI lands
      climbs in its flare.* Done 2026-10-06: the F-15C no longer did, but the
      737-300, A380 and Mosquito did; the flare now judges the sink a moment
      ahead, and none climbs in either lesson.
- [x] **The touchdown sink the AI flares to is set, not published** (200
      ft/min for jets, 40 otherwise). *Verification: each aeroplane's comes
      from a source the figures file names.* Done 2026-10-09 (owner's
      decision): the figures files cite NASA's go-around criteria study,
      whose touchdown criterion is 360 ft/min, and every aircraft's target
      is held within it.
- [x] **The AI does not go around from a balloon**: a Mosquito given back
      from a pilot's over-pulled flare zooms to fifty feet and comes down at
      958 ft/min. *Verification: every aeroplane given back in a balloon
      lands within what its gear takes or goes around.* Done 2026-10-06: the
      Mosquito goes around to 500 ft and the other twelve land.
- [x] **The F-35B climbs away on a raised go-around circuit**: over ground
      rising beside the runway, on the downwind leg at 2,900 ft she climbs
      to 6,000 ft and flies on past base. *Verification: she flies the
      rising-ground go-around and lands, as the other twelve do.* Done
      2026-10-08: at her highest pitch she could not climb at the speed
      asked, and the autopilot never asked for more; now it does, and she
      climbs to her circuit, flies every leg and lands with the other twelve.
- [x] **The circuit's rules are written twice**: the go-around's circuit
      and the circuit lessons' AI fly the same legs from separate code.
      *Verification: the circuit lessons are flown by the go-around's
      circuit, with their figures unchanged.* Done 2026-10-08: the lessons
      join it from the take-off, every aeroplane inside the same bands.
- [x] **The AI's approach speed is for the weight its figures give, not
      the weight it flies at.** The server flies each aircraft at its
      model's own weight and lands it at its unscaled reference speed: the
      B-2A's 124 kt is for 177,160 lb and her model weighs 327,000, where
      124 kt is below her stall. *Verification: every aircraft the AI lands
      flies its reference speed scaled for the weight it has, landed light
      and heavy on a server.* Done 2026-10-10: the reference speed goes as
      the square root of the weight, in the approach, the gate, the circuit
      and the go-around; all fourteen land at their models' weights and
      their figures', and a server lands a light PA-28 and a heavy B-2A and
      A380, and every brief tells the speed the AI will fly. The C172P
      flies to her learnt landing's gate at the speed it was trained at,
      and only at the weights it was trained at.

---

## Phase 11 — Traffic and separation

AI aircraft kept apart from each other and clear of the runway.

- [ ] **An AI aircraft that arrives between two layers inside 1.5 nm of
      one is under the minimum until it has turned 1.5 nm away** (39 s
      measured): nothing yet moves the layers apart to make room, and a
      handed aircraft flying a copilot's route is not measured in a run.
      *Verification: every AI aircraft a server runs, planned or not, stays a
      stated distance from every other along the whole of its route,
      measured over a whole run.*
- [ ] **The AI's go-around circuit knows no traffic**: an approach long or
      fast now goes around, but two circuits are kept apart only by the
      separation monitor's height limits. *Verification: an aeroplane that
      goes around is flown round and landed (done), one arriving long or fast
      goes around (done), and two going around together are kept apart and
      both land.*
- [x] **Nothing clears a runway for a copilot's route**: an AI aircraft
      landed now leaves the runway, one arriving at a runway not clear goes
      around, and a copilot's route may end in a landing on a runway the
      server knows (protocol 10).
      *Verification: two AI aircraft on plans ending at one runway both land,
      the second once the first has left it; and a copilot's route that ends
      in a landing is landed and leaves the runway.* Done 2026-10-08.

---

## Phase 12a — Prediction, clocks and the server's weather

A client's prediction held to the server's: correction bounds, the clocks' difference, what a state update carries, and the weather a client flies.

Found re-reading the living documents at the end of Phase 8, 2026-10-02: the lobby, the
session, the weather and the terrain dataset, the weather over hills, the blend, the gusts and
the window client in the server's weather (and a few more, in the phases below) were each
named in `PROJECT_STATUS.md` as not done, with no item here.

- [ ] **A client whose every frame is slow is not shown to stay under the
      20 m correction bound** (the window client beside other tests in a
      sanitized build, every frame 0.8 s and more: 21.4 m once; CI run
      36674576086, the slow-frames test: linux-debug 20.624 m, windows-debug
      29.491 m). The plain on-server window test on the debug presets is in
      the same regime. *Verification: the
      on-server window test with every frame held a second stays under 20 m.*
- [ ] **A server that falls behind real time puts its clients' prediction off
      by metres**: it flies each input for fewer steps than the client does.
      *Verification: a server slowed on purpose keeps a predicting client's
      error under a metre.* Both clients now fly at the server's pace
      (2026-10-08); the command-line client holds it. Still missing: the
      window client is still put right 6 to 9 m against a slowed server, and
      10 to 13 m against one keeping time, untested and unexplained; and on
      Windows one update in a hundred is off by two steps (1.1 m) - a named
      limit, its cause found (2026-10-08), not fixed. A server held up now
      flies every input from the step due when it came and catches up what it
      can (2026-10-28): stopped alone 3 s, the 95th percentile 0.5 to 1.5 m
      (was 15 m), but the worst still 4.9 m - the inputs that came in the
      stall are flown as one, which needs inputs that name their step. (A
      server's slow start is no longer owed, 2026-10-28.) The first-input
      test and the F-22A's still run alone: not yet seen on CI held up.
- [ ] **A test that fails twice on CI for the same cause has nowhere to go but
      the pull request's run**: a test labelled `nightly` is left out of every
      pull request's run and run by the nightly one, and each is named with its
      reason. *Verification: a test labelled nightly runs every night and on no
      pull request, named with its reason; the first is the held-up-unevenly
      prediction test (1 of 1 named).* Done but for the nightly half: pull
      requests leave it out (run 40770584919); owed, the first scheduled run
      after the merge showing it run.
- [ ] **On Windows the server's word is placed two steps off now and then**:
      Windows' 15.6 ms timers spread the clocks' difference, and the least of
      it moves. *Verification: on Windows debug the paced test's 99th
      percentile under a metre in ten runs.* 2026-10-29: the held-up-unevenly
      test runs nightly until this is fixed; its red there is reported here
      (CLAUDE.md: a named nightly test's red is not a red tree).
- [ ] **On a busy machine the server's word is placed a step off now and
      then**: a server held up unevenly moves the least of the clocks'
      difference, so the multi-process tests bound their 95th percentile by
      steps' travel, not centimetres. *Verification: with twice as many busy
      loops as cores beside it, the STOVL selector test's 95th percentile
      under 10 cm in ten runs.* 2026-10-16: until then that test holds it
      to four steps (3.4 m) and its median to 60 mm, from CI's figures.
      2026-10-23: CI's Windows placed the first-input test's words 5 and 18
      steps off (24.7 m), no client frame over 171 ms; that test holds its
      updates to the path flown instead. 2026-10-29: the STOVL join with its
      first datagrams lost reached a 77 mm median on Windows, along her
      path, its cause unknown; it is held to 120 mm. 2026-10-30: CI's
      Windows debug placed the STOVL at-once test's words 11 steps off
      (run 40829041494), 0.032 degree of pitch; it judges pitch where she was.
- [ ] **On a server the air has no ground's lift**: the server and its
      clients fly the weather over no ground, because the lift is too costly
      for a predicting client. *Verification: a client predicting over hills
      in a strong wind is within the same bound as over the sea, the lift
      flown on both ends.* Still missing, found 2026-10-08: the headless
      predicting client flies over no ground at all, so it needs the
      collision DEM first.
- [ ] **A client's gusts are not the server's while its clocks' difference
      settles**, a step or two out, metres in a strong gust. *Verification: a
      client predicting in gusting air stays within the steady air's bound.*
      2026-10-08: at the server's moment the client's gusts are the
      server's exactly, and a step out they are not; still missing: an
      end-to-end test that tells a wrong gust clock (the steady bound passed
      one five seconds out), and the cause of a worst of up to 4 m in a
      weather's change, where the steady air's is 0.4 m.
- [ ] **A 172 a window client joins on final leaves the gate in seconds**:
      left alone, it is 8 degrees off the runway's heading six seconds in. The
      client keeps the server's flaps and throttle but not its pitch trim,
      which no update carries; whether that is why is not yet measured.
      *Verification: a window client joining on final and touching nothing is
      still at the gate thirty seconds later.*
- [ ] **The engine-stop prediction test fails on Windows clang when the
      server is held up**: its client, pacing 1.6 to 1.7 times its own clock
      to catch up, is put right by metres and misses its speed bound - since
      before 2026-10-09, on branches and on main. *Verification: the test
      passes with the server held up for a second on purpose, on every
      platform, and a month of CI runs.* 2026-10-16: its speed bound is
      25 cm/s, from six Windows failures since 2026-10-08. 2026-10-17: the
      cause found and the test no longer depends on it (below); the month of
      CI runs is still owed.
- [x] **The engine-stop prediction test does not depend on the client's
      pacing**: it held the server's word against the step the clocks'
      difference named, which a server held up by fits and starts moves by
      tens of steps; it now holds it against where the client had flown
      nearest. *Verification: with the server stopped for up to 800 ms at a
      time it passes, and an engine left running fails it plain, stopped
      and slowed.* Done 2026-10-17.
- [ ] **A flight flies one station's weather wherever it goes**: nothing
      picks the nearest station. *Verification: a flight from one station to
      another flies the nearer's weather, changing between them with no step
      in the wind.*
- [x] **The network checks on macOS drew another aircraft metres off** (8.6 m
      at 100 ms, 8.5 m at 200 ms). *Verification: the cause found, and the
      check run a hundred times on macOS within its bound.* Done 2026-09-30:
      an aircraft guessed through late updates now comes back from where it
      was drawn; both checks passed a hundred times each on macOS.
- [x] **A client assumed the server's clock keeps real time**, and drew other
      aircraft from guesses when a slow server's clock ran behind. *Verification:
      against servers at 80%, 100% and 125% of real time, with jitter and loss,
      the client's clock stays within 20 ms ahead and 50 ms behind.* Done
      2026-09-25.
- [x] **Over a network a client's own aircraft is corrected by metres**, because
      the server does not say how far into its latest input it had flown.
      *Verification: through 200 ms with jitter and loss, the worst prediction
      error is under a metre.* Done 2026-09-27: the server says it, and the
      client places the server's word on its own clock by it. A unit test
      with built jitter and loss holds a metre on every machine (1 cm at
      200 ms); the network checks measured 0.26 and 0.39 m at 200 ms.
- [x] **A player's copilot is told whether the engine runs.** No state update
      said so, so on a server a player's copilot was told it did, and would
      not glide. *Verification: an engine stopped on a server is said to the
      player's copilot, which answers with a glide the server flies.* Done
      2026-09-30, with Phase 8's "The copilot flies with you".
- [x] **A client predicting its own aircraft does not know its engine has
      stopped.** An engine that stops on the server while the player flies is
      still run by the client's prediction, and put right correction by
      correction. *Verification: with an engine stopped under a player flying
      it, the client's corrections are as small as with it running.* Done
      2026-10-06, by the median error (14 mm after, 42 mm before); the
      update names which engine stopped (protocol version 04).
- [x] **The lobby, the session, the weather and the terrain dataset
      travel.** A joining client is told each, and every change of the
      weather; it flies the server's weather, and refuses other ground.
      *Verification: a client joining a server is told each, and its
      prediction flies in the server's weather.* Done 2026-10-06.
- [x] **A client joining while a weather blends in flies the new one whole**:
      the server sends only the newest weather, not the one it blends from,
      so until the blend ends the joining client's air is not the server's.
      *Verification: a client joining mid-blend predicts within the same
      bound as one there before it.* Done 2026-10-08: told what it blends
      from first, 0.043 m against 0.046 m.
- [x] **Nothing tests the client with the window in the server's weather.**
      *Verification: the window client on a server with a METAR says it flies
      it, and its prediction error is within the headless client's bound.*
      Done 2026-10-08: median 0.003 m, held to 0.1 m.
- [x] **A test's server let a slow client go after a few seconds' silence.**
      *Verification: a check names every short server `--timeout` in the tests
      exactly and fails on any other; watched red; the changed tests pass.*
      Done 2026-10-28.

---

## Phase 12b — Sessions, joining and the protocol

Joining, leaving, being dropped and coming back; the handshake, the reliable layer and the messages' limits.

- [ ] **A client with another control layout is not refused by name**: the
      version does not move with a message's layout before a first release,
      so a client sending 17 controls to a server reading 18 is let in and
      every input it sends is dropped as the wrong length. *Verification: a
      client with a mismatched layout is refused at the handshake, saying
      so, before the first release.*
- [ ] **The client's half of a session is written twice** - the command-line
      client's own and the shared one the window client uses - and a bug was
      found in one and not the other. Joining again, knocking and believing
      a refusal are one piece now; the rest - the handshake, reading updates,
      the stall and the goodbye - is still two. *Verification: both clients
      use one session, and every rejoin and going-back test passes through it.*
- [x] **A client the server has let go cannot come back.** Both clients now
      join again by themselves, and neither does when the operator dropped
      it. *Verification: a client stalled past the timeout joins again by
      itself - the command-line client and the one with the window.*
- [x] **Nothing tests a client going back to its old session** when a forged
      refusal made it try to join again. Both clients are now tested; the
      command-line one, back in its session, numbered its inputs from 1 again
      and the server dropped them - fixed. *Verification: a client refused by a
      forger while its session is merely quiet goes back to that session, and
      the server makes no second player - both clients.*
- [x] **A client goes back to a session already let go** when an update the
      server sent before letting it go arrives while it joins again: the
      server has admitted its new initiation, so a ghost session holds a slot
      and an aircraft until its timeout, and the client is lost for about 13 s.
      *Verification: a client whose session was let go, with the server's last
      updates held until it tries to join again, joins again without going
      back, and is admitted once.* Done 2026-10-06: it goes back only on the
      old session's answer to a knock of its own.
- [x] **The client with the window builds its flight without reading its
      socket**, so a slow build can outlast the server's timeout and be let
      go. Its session is now kept from a thread of its own whenever it is
      away - building its flight, or in any long frame. *Verification: a
      window client that takes longer than the server's timeout to build its
      flight is not let go, nor one whose frames take longer.*
- [x] **Nothing tests the client with the window refused `DROPPED`** when every
      one of the server's goodbyes was lost. *Verification: a window client
      dropped with its goodbyes lost tries to join again, is refused, and stops
      saying it was dropped.* Done 2026-10-06.
- [x] **A client cannot say it is leaving**, so a server notices only by
      the silence. *Verification: a client that leaves is let go at once.*
      Done 2026-09-26: both clients say goodbye, sealed, as they go, and the
      server lets them go at once; a goodbye from anyone else lets nobody go.
- [x] **One player on two addresses flies two aircraft**: a client that
      starts again from a new port while its old session is still live is
      given a second aircraft until the old one times out. *Verification: a
      second session for a key takes over that player's slot and aircraft,
      and a test with one key on two addresses counts one aircraft.* Done
      2026-09-29: it takes over once it has sent something sealed, which a
      replayed handshake cannot, so a replay takes nothing from a live player.
- [x] **A player who starts again on a full server waits out the timeout**:
      their new session is refused as a stranger's would be, because the
      server spares a full session the work of finding out who is asking.
      *Verification: on a server of one player, the player started again from
      a new port is flying again at once.* Done 2026-10-02: a full server
      reads an initiation as far as its key, a few times a second at most, and
      lets a player's key back in; a stranger is still refused.
- [x] **The client with the window is not tested with its updates
      reordered across a take-over**; it has the guard the command-line client
      needed. *Verification: an update from before the take-over, heard after
      it, leaves the client flying the aircraft it took.* Done 2026-10-06.
- [x] **A client opened the model a server named as a path.** *Verification:
      every aircraft's id is known by the catalogue and eight hostile ones are
      not.* Done 2026-09-25.
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
- [x] **The reliable layer believes an acknowledgement it is told.**
      *Verification: a forged acknowledgement lets go of nothing not yet
      acknowledged.* Done 2026-09-22.
- [x] **No message rejects a number that is not one.** *Verification: every
      floating-point field of every message refuses a NaN and an infinity.* Done
      2026-09-22.
- [x] **A network test's program died of a closed pipe on macOS.** A client
      that said goodbye and then printed was killed for writing to the server
      its goodbye had ended, twice on CI. *Verification: every program a test
      runs in a pipeline exits as it should when nobody reads what it writes,
      and the relay fails when it gives up with its server still running.*
      Done 2026-09-29.
- [x] **A server's refusal was lost from the take-over test now and then.**
      The relay passed on the server's words a character at a time, so a
      client's line could land in the middle of one and hide it, on Windows
      CI. *Verification: with the relay slowed on purpose the test failed
      three runs of three; passing on whole lines, it passed three of
      three.* Done
      2026-09-29.
- [x] **A client dropped by the operator did not always say so.** On CI,
      four times, the server dropped the window client and then stopped,
      and the relay in front of it stopped too, before passing on the
      server's goodbye. *Verification: with the relay slowed on purpose the
      test failed three runs of three and passes three of three now; a relay
      holding the server's last words when the server goes is seen to lose
      them before the fix and pass them on after.* Done 2026-09-30.
- [x] **A player cannot choose an aeroplane on a server**: a player flies
      what the server's plan flies. *Verification: a player asks for an
      aeroplane when joining and flies it, and every other client draws it
      as that aeroplane.* Done 2026-10-07: asking and flying it (protocol
      version 04), every client told what it is, and the window client draws
      another player's PA-28 from the PA-28's model.
- [x] **Nothing limits how often a client sends**, where REQUIREMENTS 6.2
      asks for rate limits on its inputs and requests. *Verification: a
      client sending faster than a stated rate is held to it, and
      `THREATS.md` states the rate.* Done 2026-10-06: 240 datagrams and 8
      requests a second per session.
- [x] **Nothing tests the client with the window joining a server that has
      started again** with its key from `--store`. *Verification: the window
      client, its server restarted under it, joins again and flies an
      aircraft the new server gives it.* Done 2026-10-06.

---

## Phase 12c — The window client

What the client with the window does that the command-line client does, and what it draws.

- [ ] **The client with the window does not blend corrections to its
      prediction**, as the command-line client does. *Verification:
      corrections are blended, and the largest step away from a switch is
      held to a bound, failing clearly with the blend taken out.* The blend
      is in and unit-tested (2026-09-29). Still missing: large corrections
      built through the client itself, a bound on the hand-over test seen to
      fail, and whether the older steps CI saw (6.2 and 24.5 m in the
      hand-over test, 2.77 m at a take-over) were the session's clock,
      which stepped under long, uneven frames and is now slewed (2026-10-09).
- [ ] **A livery on the aeroplane, and its control surfaces moving.**
      *Verification: a shot shows a livery, and the ailerons move with the
      stick.*
- [ ] **The aeroplane is lit by a light baked into its mesh.** *Verification:
      its lighting follows a roll with no mesh remade.*
- [ ] **The window client crashes on exit (signal 11) after giving up on an
      imagery tile.** A headless `--shot` run writes its frame, then exits
      139 once a Sentinel-2 tile is "given up, unfinished" - the Cessna 172P
      and the Learjet 35A alike; the same run with `--imagery off` exits 0
      (PROJECT_STATUS, 2026-10-10, the Learjet drawn). *Verification: a shot
      run with imagery on, whose tile is refused, exits 0.*
- [ ] **No player can fly a lesson or watch the instructor.** Lessons and
      the instructor run in the simulation and its tests alone; no client,
      server or command line starts one (PROJECT_STATUS, 2026-10-13).
      *Verification: a player chooses a lesson in the window client, the
      instructor flies it first and hands over, and the debrief is shown.*
- [x] **The Learjet 35A is drawn as nothing**: FlightGear has no Learjet
      model. *Verification: a model whose source and licence are in
      `ASSETS.md` is held to the Learjet's size and drawn in a shot.* Done
      2026-10-10: a CC-BY-4.0 Sketchfab model the owner provides; its span
      within 0.3%, its length 5.3% over and held to 6% by name; its mesh
      check skips where the owner's copy is absent, as on CI. The owner
      accepted the 6% on 2026-10-10: she ships as built, the excess being
      the artist's tail.
- [x] **The client with the window does not blend its own aircraft at a
      switch** - handed over (A on a server), taken back or taken over - as
      the network checks' model of a display does. *Verification: what it
      shows of its own aircraft, measured sixty times a second across each
      switch, steps less than 5 m at a hand-over and a take-back, and less
      than 2.5 m at a take-over, where unblended it is about 5 m - each past
      its bound with the blend taken out.* Done 2026-09-27: 1.1 m at worst.
- [x] **A weather service's answer that is not JSON ended the flight.**
      *Verification: an answer that is not JSON is fetched again, and three
      in a row are a download that failed.* Done 2026-09-25.
- [x] **Quitting during a slow or rate-limited weather refresh could hang
      the exit for about 80 s.** *Verification: quitting while the weather
      service answers 429 ends the program within a couple of seconds.* Done
      2026-09-29: within 2 s of a quit with no refresh, while refused with a
      429 and while a download never finishes, on Linux and Windows.
- [x] **The client with the window does not hand over on a server**: pressing A
      online does nothing. *Verification: the client with the window hands its
      aircraft to the AI and takes it back on a server, and what it shows does
      not step; A pressed during a take-over's round trip hands over the
      aircraft taken, or, the take-over refused, the one kept.* Done
      2026-10-07: the server says when it refuses a take-over (protocol
      version 7), and A waits for the answer.
- [x] **The display model is written twice**, in the command-line client
      and in the client with the window. *Verification: one presentation-free
      module serves both, with a unit test that builds long frames across a
      switch and bounds the step.* Done 2026-10-06.
- [x] **No in-client help lists the controls.** The keys and buttons are
      only in the README and the bindings file, not in `glideslope --help`
      or on screen. *Verification: the client shows every control's key and
      button, held to the bindings by a test.* Done 2026-10-08: F1 shows
      every key and every binding, made from the bindings file and the
      keyboard's own table, and read back off a real frame.
- [x] **The HUD's horizon line is not the horizon**: it moves a hundredth of
      the frame a degree of pitch, and the drawn terrain does not line up
      with it. *Verification: over level ground the HUD's horizon lies on the
      drawn one within a stated number of pixels, at every pitch and bank
      walked.* Done 2026-10-06: the line is the camera's own horizon; over
      the Nullarbor, at pitches of -15, 0 and 15 by banks from -60 to 60, it
      lies within 4 pixels of the horizon drawn (the Earth's curve is 1.8).
- [x] **The keyboard has no flaps, and a window client joining forgets the
      server's.** F and R now work the flaps a notch a press, and a client
      joining keeps the server's flaps and throttle. *Verification: a window
      client started on final keeps full flap, sets it from the keyboard, and
      is handed to the learnt landing.* Done 2026-10-08.
- [x] **A window client whose imagery cannot be fetched draws nothing**: on
      CI's windows-clang every EOX tile was refused (WinHTTP 12030) and the
      forged-refusal test's client wrote no frame (run 40603515425); on
      Linux, EOX resetting its connections cost a ride-along test its frame
      (run 40618052854). *Verification: with every imagery fetch failing,
      the window client still draws its frame, on the DEM's colours, and
      says why.* Done 2026-10-28.
- [x] **The wedged-imagery test hung when Cesium asked a coarse fetch
      first** (CI's macOS, run 40743161009; Windows debug, run 40686462655):
      its hold took the first fetch to be the root tile's and held the
      root's own. *Verification: with the fetches asked for out of order,
      the old hold hangs and the new one builds the wedge and gives it up;
      nothing is held for ever.* Done 2026-10-29.
- [ ] **A parent-imagery unit test died at exit on the Windows development
      machine** (`a_parent_imagery_tile_scaled_into_a_missing_childs_place_is_sound`,
      0xc0000409, once, 2026-10-29). It passed in its own checks first. On CI's
      Windows jobs it passes in 0.02 s; on that machine it took 2 to 4 s,
      and its Webroot has killed test programs before. *Verification: the
      cause named, Webroot's log read first, or the crash reproduced and
      fixed.*
- [ ] **A shot with its imagery refused drew coarser than one
      without**, twice on CI's Linux release, so its test runs nightly, not on
      pull requests. Cause not found. *Verification: the cause shown, the test
      back on pull requests, and a shot with its imagery refused drawing as
      deep as one without in 20 of 20 runs here and every CI run for a week.*

---

## Phase 13 — The copilot and planner

The language model that plans: its routes, its recordings and what it is told.

- [ ] **The copilot and planner do not know a vertical landing.** A plan
      may end in `land_vertically` since 2026-10-14 and the AI flies it in
      the F-35B, but neither model is told of it, so asked to land her on a
      pad neither can. *Verification: asked to land the F-35B vertically at
      a named point, the planner's plan ends in one and she is landed there;
      recorded and played back.*
- [ ] **The copilot marks the waypoints it means to fly over.** A plan's
      waypoint may now say it is flown over rather than by, but the copilot
      and the planner never say it, so a route's landmark is passed as far
      off as its turn cuts the corner. *Verification: asked to fly over a
      named landmark and turn there, the copilot's route marks it, and the
      aircraft passes it within 100 m; recorded and played back.*
- [ ] **The window copilot test's copilot never answered once** (Windows
      release): asked, then nothing in five minutes, the aircraft on the
      ground. *Verification: the cause named, and a month of CI runs without
      it failing.*
- [ ] **No model has yet been seen to answer a copilot's question with a
      landing**: the copilot may end a route in one since 2026-10-08, but no
      recorded live answer of the copilot's does (the planner's has, since
      2026-10-09). *Verification: a recording of a model asked to land at a
      runway nearby answers with a landing the server flies.*
- [x] **A landing on a runway too short for the aircraft is refused, for
      the ten that publish a landing distance**: the copilot's routes and
      the planner's plans are held to each one's published landing distance
      (the 172P's 1,280 ft to the A380's 6,350 ft, each document pinned) with
      its class's margin, against the runway less any displaced threshold.
      *Verification: a landing on a runway shorter than the aircraft lands
      in is refused, saying so, for each of the ten.* Done 2026-10-09; the
      six with no public figure were set aside by the owner's decision,
      now an open item in Phase 10b.
- [x] **A copilot recording breaks when two runways swap places.** Played
      back, a question matches its recording but for its numbers, so a
      flight a little different that lists two runways in the other order
      must be recorded again. *Verification: a played-back flight whose
      runways come in another order still plays.* Done 2026-10-06: the
      runways' order is disregarded with their numbers; nothing else's.
- [x] **A model planning an aircraft left to the AI is not told the plan it
      flies.** Left by a player who goes, it flies the server's plan file
      until the server's model answers, and the model is told no route is
      flown. *Verification: the model is told the plan file's waypoints still
      to fly.* Done 2026-10-06; Claude's recording made again.
- [x] **The window client's hand-over model is chosen at start**, by a flag,
      not in flight, and its refusal for want of a key is tested only on the
      headless client. *Verification: a key cycles the model in flight, and
      the window client with no key says the model is refused and its
      aircraft is held.* Done 2026-10-06: M.
- [x] **A model's flight plan cannot end in a landing**: the planner, which
      plans a flight from the ground, is now offered `land` as the copilot is,
      held to the same checks. *Verification: Claude, asked to take off from
      Sydney and land at Bankstown, answers with a plan ending there, and the
      server's AI flies it and lands it on Bankstown's 29C.* Done 2026-10-09.

---

## Phase 14a — CI and its speed

The pipeline: its length, its caches and what it costs a pull request.

- [x] **macOS's tests ran on a Python their runner did not have**: the build
      found Homebrew's newest and its test runners lacked it, so every script
      check failed at once (run 40986292025). It is pinned, as Windows' is.
      *Verification: the macOS build checks its configure found the pinned
      Python, and its test shards pass on the full run.* Done 2026-10-30.
- [ ] **vcpkg rebuilds every package when GitHub updates a runner's compiler**
      (26-28 minutes a Windows configure). *Verification: a new runner image
      costs one rebuild, saved, not one per run.* Windows restores from
      GitHub Packages, Linux and macOS from a cache keyed on the image, and a
      newer merge no longer cancels main's run before it saves. Owed: no new
      Windows compiler has come since 2026-09-30, so the rebuild is unseen.

- [ ] **A Windows configure on CI sometimes takes 30 minutes in vcpkg**,
      even after an exact hit on its binary cache. *Verification: every
      Windows configure whose vcpkg cache hit takes under 3 minutes, over a
      week of runs on main.* No rebuild since 2026-09-30; 2 of main's 152 in
      the week to 2026-10-09 took just over 3 minutes. vcpkg is now fetched
      from the runner's own copy where it has the commit. Owed: a week on
      main after that.
- [ ] **One test takes 15-22 minutes in Linux debug**: the AI aircraft
      kept 500 ft or 1.5 nm apart, so its shard runs 26 of its 30 minutes.
      *Verification: every test under 10 minutes on CI's Linux debug.* The
      ground under an aircraft is now worked out once for each place, not at
      every read (2026-10-10): this test takes 99 s on this machine's Linux
      debug, 393 s before, with every figure it prints the same. Owed: its
      time on CI, from the first run after the merge.
- [x] **A Windows test job on a newer runner image than its build fails
      every script check**: the Python the build found is not there.
      *Verification: a Windows test job on another image from its build
      passes every script check.* Done 2026-10-15: each Windows job pins one
      Python, the build checks its configure took it (run 38637622313).
- [x] **A CI run takes 90-120 minutes where its jobs need about 40**.
      *Verification: pull requests' builds restore main's ccache with most
      compiles hits, and a run's time from push to result is measured and
      stated.* The caches fit now and pull requests build warm (86-95% hits,
      2026-10-09). With several runs in flight a pull request's took 148
      minutes, its macOS jobs waiting for the account's five macOS runners;
      by the owner's decision (2026-10-09) a pull request now runs macOS
      release only, four macOS jobs instead of ten, and macOS debug's tests
      run on main after the merge and nightly. Done 2026-10-09: PR #150's run 37933227320, with
      this in place, took 46 minutes from push to result, every build
      restoring main's ccache at 79-91% hits.
- [x] **CI's actions run on Node.js 20, which GitHub has deprecated.**
      *Verification: a CI run's annotations name no action as targeting
      Node.js 20.* Done 2026-09-24.
- [x] **CI's Windows builds have no compiler cache**, so each compiles
      everything on every push (build jobs 14-17 minutes). *Verification: on
      CI, a Windows build after a saved cache hits most of its compiles, and
      its build job's time is measured cold and warm.* Done 2026-10-07:
      MSVC's builds hit 791 of 792 compiles, 4 minutes; clang-cl's 722 of
      824 with a pull request's changes, a 7-minute job (run 37561340650),
      against 17 minutes cold (run 37570543750).
- [x] **CI's cost tables do not know the plan-speed tests.** The eight
      tightest-orbit tests, renamed or new, and the one-step-past test
      (2026-10-02) count 60 s each in tests/ci_costs until a green run is
      measured with tools/ci_test_costs.py. *Verification: every test in
      the tables, from a green run.* Done 2026-10-07: every table from three
      green runs, no shard warning of an unmeasured test; the shard counts
      raised to about 15 minutes of measured work each.
- [x] **A Linux debug test shard nearly fills CI's 30-minute budget** (27 min
      43 s, 2026-09-27), mostly the circuit lessons at about 980 s each.
      *Verification: every shard's longest run on CI stays under two thirds of
      its job's limit.* Done 2026-09-29: tests are dealt to shards by their
      measured cost, a fixture with its tests, and a test checks every shard
      between them runs every test once; the worst shard on CI took 17.7 of
      30 minutes, macOS debug's 11.8.
- [x] **Rocky 9's release tests run in one job, 42 minutes** (run
      38777694219), the longest job of every run. *Verification:
      on CI every Rocky shard's tests take under 20 minutes, and every test
      runs in exactly one shard.* Done 2026-10-16: four shards, 8.9 to 12.5
      minutes each, and the shards test checks every job's dealing.
- [x] **Every push to a draft pull request runs the full matrix**, about 50
      jobs and 980 runner-minutes (run 39846357752). *Verification: a draft's
      run builds everything and tests only Linux and Windows debug, with "CI
      passed" red and "CI draft passed" green; marked ready, the full run
      goes green; both runs' runner-minutes recorded.*

---

## Phase 14b — Tests, downloads and tooling

Flaky and fragile tests, downloads that fail once, the build scripts, and the month of clean nightly runs.

- [ ] **A month of clean nightly runs.** Each of these needs only runs
      watched or counted, no code, and each is owed the count named; one that
      fails in the month has its cause found and fixed under its own item:
      - The four-player test once counted five players' aircraft (owed: the hundred Windows debug runs)
      - The command-line forger test once failing after 300 s (owed: the cause found, or the test repeated under load on every platform)
      - A take-over at 100 ms not refused once (owed: repeated nightly runs on every platform)
      - A take-over at 100 ms put off 14.9 m on macOS debug by a server held up, fixed at cause 2026-10-08: inputs flown from the step due when they came (owed: a stall built into the multi-process test - the rule has a unit test - and a month of CI runs)
      - The window client stepping over 2.5 m at a take-over on a slow machine (owed: a pass on windows-release)
      - Windows debug test programs crash on their way out on the development machine (owed: a hundred runs on the development machine)
      - The client with the window can crash on Windows as it exits (owed: a hundred argument refusals on Windows)
      - The Cesium cache still locks when the rendering tests run together (owed: nine of ten suite runs at -j4)
      - A weather request on the Windows development machine sometimes waits two minutes (owed: twenty fetches in a row within ten seconds)
      - Two window clients on one cold Cesium cache can stall for over fifteen minutes (owed: both finishing in the time of one, Linux and Windows)
      - CI fails more often than it passes, on tests that time the machine (owed: a month of runs counted)
      - The window client put right 20 to 31 m after a long frame (owed: a month of CI runs, 20 m bound unchanged)
      - The rate-limit test once took 10 of 50 requests on Windows debug, fixed at cause 2026-10-08 (owed: a month of CI runs)
      - The ground test's two clients raced on macOS, fixed at cause 2026-10-08 (owed: a month of CI runs)
      - The engine-stop prediction test failed on Windows, judged in speed since 2026-10-08; replays keep the engines' spin and the actuators since 2026-10-09 (owed: a month of CI runs)
      - The learnt policy's 160 corner landings timed out on CI's Ubuntu debug, split one test a wind 2026-10-08 (owed: a month of CI runs)
      - The prediction test at 100 ms with jitter and loss once read an interpolated control 0.0051 against its 0.005 bound, CI run 37863482425 attempt 1 (owed: a month of CI runs)
      *Verification: 30 consecutive nightly runs with none of these failing,
      or each failure's cause found and fixed under its own item.*
- [x] **A HUD test fails, rather than skipping, when the weather service
      does not answer** (Windows CI, 2026-09-25: WinHTTP 12002 from
      Open-Meteo). *Verification: with no weather to be had, the HUD tests
      that fly in live weather report themselves skipped.* Done 2026-09-25:
      skipped even where the network is required, since live weather is
      never kept. A service that refuses the request, or an answer that
      can't be read, still fails there, and so does a missing DEM.
- [x] **The tests' fixed ports lay in Linux's ephemeral range** (478xx, where
      it hands out 32768 to 60999), so a client's socket could take one before
      its test's server listened: "cannot listen on port 47853" on CI,
      2026-09-26. They now come from one block, 24700 to 24799, below every
      platform's ephemeral range. *Verification: a test walks every test's
      port, relays included, and fails on one in 32768-65535, outside the
      block, or shared; seen to fail with 47853 put back.*
- [x] **The HUD check read the horizon, crossing the rows below the HUD, as a
      line of the HUD.** *Verification: a line that does not begin at the
      HUD's margin is not judged, and an extra HUD line still is.* Done
      2026-09-25.
- [x] **`tools/windows_build.sh` does not work from a git worktree**, where
      agents work: Windows git cannot follow a worktree's `.git` file.
      *Verification: an agent's worktree builds on Windows with the script
      as it stands.* Done 2026-09-26: it fetches from the repository's
      common git directory instead of the working tree.
- [x] **The horizon can cross the HUD's own rows**, and three HUD checks
      compare those lines exactly. *Verification: a frame with the horizon
      drawn across every HUD row, built on purpose, is read and judged
      correctly.* Done 2026-09-25: the text is drawn small enough to stay
      left of the horizon; under 474 pixels wide the horizon crosses it.
- [x] **The horizon can cross the checklist's rows** too, down the top right,
      and the checklist test compares them whole. *Verification: a frame with
      the horizon drawn across every checklist row, built on purpose, is read
      correctly.* Done 2026-09-26: the checklist is drawn over a panel that
      dims what is behind it by half, as the credits are, so the horizon
      shows through it rather than crossing its letters.
- [x] **Tests that need a download failed, rather than skipping, when
      Open-Meteo answered with something that was not JSON** (Windows CI,
      2026-09-26). *Verification: with the weather service unreachable, or
      answering nothing or a page that is not JSON, the live-weather unit test
      and the HUD tests report themselves skipped; an answer that begins as
      JSON and does not parse, or is JSON but not a report, still fails.*
      Done 2026-09-27: one shared rule for the client tests' skips.
- [x] **The HUD tests failed when Open-Meteo answered 429, Too Many
      Requests** (Windows CI, 2026-09-27). *Verification: a 429 is tried
      again after the wait it asks for, never more than 10 s, and a 429 to
      every try is weather not to be had, so the HUD test skips; any other
      refusal still fails.* Done 2026-09-27.
- [x] **A headless client drawing thousands of frames runs the software
      Vulkan driver out of memory** (seen in WSL, lavapipe). *Verification: a
      headless client draws ten thousand frames, and its memory stays level.*
      Done 2026-09-26.
- [x] **On Windows a DEM tile can fail to open while another test renames a
      fresh copy into place.** Seen on CI: `cannot open ...S34_00_E151_00_DEM.tif`.
      *Verification: many processes fetching and reading one tile at once on
      Windows all read it.* Done 2026-09-25: tiles are opened sharing
      deletion; sixteen fetchers and readers at once, 200 times over, all read
      the whole tile on Windows, where before the fix 1396 of 3200 could not
      open it.
- [x] **On Windows a DEM tile can be refused as `Access is denied` while a
      second fetch replaces it** (CI, windows-clang, 1 of 3200 threads).
      *Verification: a tile, a water mask and a pinned file whose name is
      held delete-pending are each waited for and read whole on Windows, and
      the many-at-once test passes 20 times over there.* Done 2026-09-26: a
      fetched file never replaces one already in place, and a refusal that
      passes is asked again for up to 3 seconds.
- [x] **A cached tile cut short by a power cut is never fetched again.**
      Nothing flushes a download to disk before it is moved into place, and
      nothing deletes a cached file that cannot be read. *Verification: a
      cached tile cut short, or damaged, is fetched again and read whole.*
      Done 2026-09-30: downloads reach the disk before they take their name,
      and a cached tile, water mask or pinned file that cannot be read whole
      is taken away and fetched again - a tile once, not on every query.
- [x] **Two server tests counted wall-clock seconds on slow runners**: one
      counted inputs still in flight, and a late client arrived before a slow
      server was flying. *Verification: the client waits for its last input to
      be applied, and the late one for the server to say it is flying; each is
      seen to fail on a deliberate bug.* Done 2026-09-24.
- [x] **The hooks' test, run by the pre-push hook, committed into the
      repository being pushed** and set it bare. *Verification: run with git's
      hook variables naming a decoy repository, it leaves the decoy exactly as
      it was.* Done 2026-09-24.
- [x] **Windows debug programs can crash on their way out**, when Windows
      starts a thread as they exit. *Verification: a program can still
      allocate in the last call the loader makes into it, and the tests that
      crashed run hundreds of times without a crash.* Done 2026-09-24.
- [x] **The state-stream test measured the runner, not the server.**
      *Verification: every update is counted against the simulation's own
      steps, exactly, and one dropped in ten fails it.* Done 2026-09-24.
- [x] **A `--terrain ion` run can hang for ever, past its own timeout.**
      *Verification: a timed-out run is gone and leaves no cache lock.* Done
      2026-09-26: against a stand-in ion that never finishes sending, a run
      told to stop is gone within a second or two, one left alone ends when
      its frame is written, and either way the cache takes a write at once.
- [x] **Cesium ion on Windows, where a body arrives compressed unasked.**
      *Verification: a Windows machine fetches and reads ion's layer.json, and
      the weather still arrives.* Done 2026-10-01: Windows could not undo the
      kind of compression the weather service picks when offered it, so
      Windows now offers only the kind it can undo. Verified on the
      development machine, which has a token; CI has none and skips the ion
      test.
- [x] **Nothing here compiles first-party code under clang.** *Verification: a
      test compiles every source under clang and fails on an unused constant GCC
      accepts.* Done 2026-09-21.
- [x] **One download that fails once reds the tree.** *Verification: a missing
      file is tried three times before failing, and a present one is not
      fetched.* Done 2026-09-21.
- [x] **Tests that run at once share one Cesium cache.** *Verification: the
      client tests run together and no run reports a locked database.* Done
      2026-09-21.
- [x] **The geoid came from one host, and SourceForge went down.** Its
      download link served a page in place of the file and every platform
      went red. *Verification: the geoid is fetched from a GitHub copy of the
      same pinned file, with SourceForge after it, every source checked
      against the one pin; a test shows a source serving the wrong bytes or an
      error is passed over, and that the program and the tests fetch from the
      same sources.* Done 2026-10-01.
- [x] **The aircraft models' source files came only from SourceForge**,
      which served a page in place of every one on 2026-10-01. *Verification:
      with SourceForge unreachable, every file is fetched from its second
      source with its pinned bytes.* Done 2026-10-01: Software Heritage for
      60, and a release of this repository for the other 15.
- [ ] **Many airborne tests fly with the gear down without meaning to.**
      A test that hands a controller default controls lowers the gear of
      every aircraft whose gear retracts. *Verification: every airborne
      test's gear is what it states - the helper starts it as the aircraft
      starts, an audit names the tests that mean it down - and every test is
      green.*
- [ ] **The go-around beside rising ground takes 26.9 to 778.7 s on CI**
      against a 900 s timeout, and timed out once on #202's Ubuntu debug.
      Fix its step or its cost, not the timeout. *Verification: it takes
      under a stated time on every CI platform, a month of runs.*

---

## Phase 15 — The whole world, its sky and scenery, and the game shipped

Terrain and weather beyond where a flight starts, scenery to fly over, and
downloads and a server for anyone: the first goal's world, widened.

- [ ] **Terrain over the whole Earth, streamed as an aircraft flies.**
      *Verification: a Sydney-to-Melbourne flight draws terrain the whole way,
      with tiles in memory under a bound.*
- [ ] **Weather seen as it is** — cloud that drifts and has depth, towering
      cumulonimbus, a sky that blends between reports, lit haze, rain out to the
      visibility. *Verification: each shown in shots within stated tolerances.*
- [ ] **Thermals from the ground beneath them, and lee waves trapped under a
      stable layer.** *Verification: no thermal over open water on a convective
      day, and trapped lee waves at the two-layer wavelength.*
- [ ] **Signed and notarised macOS builds.** *Verification: a downloaded package
      opens with no Gatekeeper warning.*
- [ ] **One Linux download for every distribution** — an AppImage or Flatpak.
      *Verification: one file runs on a fresh Ubuntu and a fresh Rocky.*
- [ ] **A hosted public server**, if one is needed. *Verification: `server.txt`
      names a running server a client reaches with `--online`.*
- [ ] **Free buildings for the default scenery.** *Verification: a source
      recorded in `ASSETS.md`, and a shot of a city shows its buildings.*
