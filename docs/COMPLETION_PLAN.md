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
- [x] **The same-machine replay hash** — `glideslope_cli selftest` flies a fixed
      input log and prints a state hash. *Verification: the hash is identical
      run to run on one build, and a deliberate one-line change to the physics
      moves it.*
- [x] **Cross-platform flight checks by tolerance.** *Verification: every CI
      platform flies the same scripted inputs and agrees with the published
      figures, and with the other platforms, within stated tolerances.*
- [x] **The packaged CLI flies.** *Verification: the
      `package` workflow runs `glideslope_cli selftest` out of every unpacked
      artifact.*

## Phase 2 — The world

- [x] **Earth-centred, Earth-fixed positions in double precision**, with
      latitude, longitude and height conversions. *Verification: conversions
      round-trip within a millimetre at the poles, the equator, the date line,
      and from below sea level to cruising altitude.*
- [x] **A camera-relative floating origin.** *Verification: a still scene
      rendered far from the origin is identical frame to frame, with no jitter.*
- [x] **Reversed-Z depth.** *Verification: a frame holding both distant
      mountains and a nearby aircraft draws with no z-fighting at either.*
- [x] **The Copernicus DEM, read directly** from its GeoTIFF files at a pinned
      version, with a height query anywhere on Earth. *Verification (amended
      2026-09-18): heights at surveyed airfields and coastlines match within
      the dataset's stated accuracy, and the version and hash are in
      `ASSETS.md`. Summits were in the original verification, and cannot be:
      a 30 m grid does not hold a peak, and the DEM lies 8 to 35 m below five
      surveyed summits; that shortfall is pinned by a test and named in
      `PROJECT_STATUS.md`.*
- [x] **Collision terrain in the simulation.** *Verification: an aircraft set
      down at sea level, at a high airfield and on a slope rests on the DEM
      surface in each.*
- [x] **A window and a GPU device through SDL3** on Vulkan, D3D12 and Metal.
      *Verification: the client writes a frame with `--shot` on every backend
      of every platform that has it.*
- [x] **Cesium Native drawing the open-data terrain through SDL_GPU** around one
      region. *Verification: a `--shot` of a known region matches a reference
      frame of it within a stated tolerance.*
- [x] **Open imagery on the terrain**, from the source `REQUIREMENTS.md`
      settles on. *Verification: a `--shot`
      shows the imagery and its attribution.*
- [x] **Joysticks, HOTAS and yokes.** *Verification: every axis and button of a
      virtual device reaches the aircraft's controls, walked by test.*
- [x] **A basic HUD** — airspeed, altitude, heading, vertical speed, attitude.
      *Verification: the numbers in a `--shot` match the simulation's state at
      that tick.*
- [x] **The client's test flags** — `--shot FILE`, `--shot-at TICK`,
      `--trace`, `--screen NAME`. *Verification: each is used by a ctest.*
- [x] **A frame rendered headless in CI and from every package.**
      *Verification: CI and the `package` workflow each upload a frame from
      every platform.*

## Phase 3 — Weather

- [x] **METARs from aviationweather.gov.** *Verification: a recorded METAR sets the surface wind, temperature and
      pressure it reports.*
- [x] **Winds aloft from Open-Meteo.** *Verification: a recorded response sets the wind at every level it
      reports, and between levels.*
- [x] **Weather in JSBSim's atmosphere, with turbulence.** *Verification: an aircraft in a steady crosswind drifts at
      the rate the wind predicts, and turbulence disturbs it within a stated
      bound when on and not at all when off.*
- [x] **Weather that changes during a flight without a jump.** *Verification: a new report blends in over a stated
      interval with no step in the wind.*

## Phase 3b — Wind that shears and gusts, and hazardous air

Beyond the steady wind of Phase 3 (`FEATURES.md`: wind that shears and gusts,
hazardous air, weather you can see, the same air for everyone). The server's
weather is authoritative, and every client's prediction must fly the same air,
so everything here is a function of position, time and the weather's shared
parameters alone - computed here, not in JSBSim's own gust and turbulence
models, whose hidden state a restored aircraft would not carry.

- [x] **The same air on every machine.**
      *Verification (amended 2026-09-18:
      the first said bit-identical on every platform, which floating point
      across compilers cannot promise): two weathers built from the same
      report and parameters give identical conditions on one machine, and
      within 1e-9 m/s of each other across every platform, compared in CI at a
      thousand positions and times; and an aircraft restored in the middle of a
      gust meets the same wind as one that was not, for as long as the two are
      together - their winds then differing only as the air does between where
      each is, with nothing of the air lost in the restore.*
- [x] **A METAR's gusts flown**, and its turbulence judged from the gusts'
      spread. *Verification: a recorded gusty METAR gives winds between its
      mean and gust speeds, reaching the gust within a stated tolerance over a
      stated time; a report without gusts gives none; the same flight twice is
      the same.*
- [x] **The wind near the ground as a boundary layer**, from Open-Meteo's 10,
      80, 120 and 180 m winds and a logarithmic profile below them.
      *Verification: a recorded response sets the wind at each of those
      heights to what it reports, and between them to the profile within a
      stated bound; a 3-degree approach flies down through the shear the
      profile gives.*
- [x] **Reported wind shear read** - `WS RWY`, `WS ALL RWY`, and the `WSHFT`
      and `PK WND` remarks. *Verification: recorded METARs carrying each decode
      to what they report, and a report of shear on a runway gives its approach
      the shear the model states.*
- [x] **Microbursts**, placed by the weather's parameters. *Verification: an
      aircraft on a 3-degree approach through a microburst of stated strength
      meets the headwind, downdraught and tailwind the published outflow model
      gives, at every point within a stated tolerance.*
- [x] **Thermals and mountain waves**, from the terrain and the winds aloft.
      *Verification: over a thermal an aircraft
      circling at a stated speed climbs at the rate the model gives; across a
      ridge with the wind over it, the lift upwind and the sink in the lee are
      within stated bounds of the model's.* Stated, 2026-09-18: a Cessna
      gliding round a thermal at 65 KCAS in a 45-degree bank climbs as it
      would in still air at each height and faster by the model's updraught
      along its path, within 2% of the updraught; the
      thermals are Allen's updraft model (NASA/TM-2006-213477) - his figure
      10's speeds within 0.05 m/s, and in each thermal's prime his MATLAB to
      1e-12 m/s; and the terrain's lift is linear theory's - within 4% of the
      strongest over two ridges in stable air, against the theory integrated
      independently, and within 2% of potential flow's with no stability - so
      the air rises upwind of a ridge and sinks in its lee.
- [x] **Weather you can see** - cloud from the METAR's layers, rain, and
      visibility - once the world is drawn (Phase 2). *Verification: a `--shot` at a station reporting a broken layer at
      1,500 ft shows the cloud base there within a stated tolerance, and a
      reported visibility of 3 km hides terrain beyond it.* Stated,
      2026-09-18: under broken cloud at 1,500 ft over Hawera, where the deck
      is thick, from 30 m below its base the frame looking up is cloud and
      looking down the ground, and from 30 m above it a whiteout; where it has
      a gap, from 30 m above the base, clear sky - the base within 30 m (100
      ft), the base computed by the test from the DEM's ground and 1,500 ft.
      Through 3,000 m of mist, the frame matches the DEM ray-cast faded by
      Koschmieder's law within the tinted terrain's tolerances, ground at 3 km
      or beyond within 8 of the haze's colour at the 99th percentile, ground
      within 1 km at least 15 from it on average. And heavy rain shows streaks
      the dry frame does not.

## Phase 4 — Autopilot and navigation

- [x] **Holds for heading, altitude, airspeed and vertical speed.**
      *Verification: each hold captures a step change
      within a stated overshoot and settling time, in calm air and in
      turbulence.* Stated, 2026-09-18, for the Cessna at 4,000 ft and 100
      KCAS: in calm air, heading 0 to 90 degrees overshooting at most 3 and
      within 2 for good by 50 s; altitude up 500 ft, at most 20 ft over and
      within 20 by 80 s; airspeed up 10 kt, at most 2 over and within 2 by
      15 s; climb 0 to 500 ft/min, at most 100 over and within 50 by 15 s. In
      moderate turbulence, on ten-second averages: heading within 5 degrees by
      40 s, overshooting at most 5; altitude within 50 ft by 70 s; climb
      within 150 ft/min by 30 s, overshooting at most 200; airspeed at most 3
      kt over, within 5 kt by 20 s and never more than 10 off after a minute.
      And engaging the autopilot moves no control more than a hundredth of its
      travel in a step.
- [x] **Waypoint following and flight plans as data.** *Verification: a plan loaded from a file passes every
      waypoint within a stated distance.* Stated, 2026-09-18: the Cessna flies
      `assets/plans/sydney-harbour.plan` - four waypoints over 45 km, with
      turns of 90 and 135 degrees and climbs and descents of 500 ft - past
      every waypoint in order, within 100 m of each and within 50 ft of its
      altitude, in calm air and in a 20 kt wind across and against it.
- [x] **The user/AI controller swap.** *Verification: swapping mid-flight in either direction causes no step in
      any control or in the aircraft's state, in every phase of flight.*
      Stated, 2026-09-19: in the takeoff roll, the climb, the cruise, a
      30-degree turn, the descent and a flapped approach, handing the Cessna to
      the AI moves no control more than 0.01 of its travel in a step; handing it
      back moves none faster than a pilot's hand - full travel in a second -
      until the controls meet the pilot's, within two seconds, and they are the
      pilot's from then; and over the three seconds after either, the load
      factor changes by no more than 0.05 g in a step.
- [x] **`--autopilot`** — the AI flies this client's aircraft. *Verification: a ctest flies a plan with it.* Stated,
      2026-09-19: the client, headless on every driver, flies
      `--plan sydney-harbour` from the plan's start and prints each waypoint
      as its AI passes it - every one passed in order, within 100 m and 50 ft
      of its altitude - and the plan flown to its end.

## Phase 5 — Aircraft choice

The roster (`REQUIREMENTS.md`, section 4.2): light aircraft - the Piper J-3
Cub, Cessna 172P, Piper PA-28 and Cessna 182; a seaplane, the Short S.23 Empire
flying boat; from the Second World War, the de Havilland Mosquito; a business
jet, the Learjet 35A; the airliners Airbus A320 and A380 and Boeing 737, 747
and 787; the fighters F-15 Eagle, F-22 Raptor and F-35A Lightning II; and a
bomber, the B-2 Spirit. JSBSim ships flight models for all but the A380, the
Learjet, the Mosquito, the F-35A and the B-2, and those it ships are of uneven
quality: each is held to published figures before it is offered, as the Cessna
172P was. The five it does not ship are written here, from published data.

- [x] **Aircraft as data.** *Verification: an
      aircraft is added without a code change, and a test walks every aircraft
      the data holds.* Stated, 2026-09-19: a copy of the data with one
      `.aircraft` file more holds one aircraft more, as its file describes,
      which loads and flies; and every aircraft the data holds loads and, on
      the autopilot, holds 3,000 ft within 100 ft and its catalogue airspeed
      within 5 kt for a minute.
- [x] **A Mosquito flight model**, written here from its pilot's notes and
      published trials - the first aircraft after the Cessna, at the project
      owner's asking. *Verification: maximum speed at altitude, climb, stall
      and take-off figures from those sources, each inside its tolerance, and
      its handling - the swing on take-off, the single-engine safety speed -
      as the pilot's notes describe it.* Stated, 2026-09-19, CI run 35409102752:
      the FB Mk VI flies all fourteen of its figures inside their ranges on
      every platform - HX809's level speeds at four heights within 0.8%,
      HJ679's climbs and time to 20,000 ft within 7.5%, the Pilot's Notes'
      stalls within 3 knots, the B Mk IV's take-off over 50 ft 5.5% long,
      the swing to port checked by a 2.6 lb/sq in lead on the port throttle,
      the two safety speeds 5 and 8 knots from the Notes', and the
      single-engine ceiling 12,480 ft against their 12,000.
- [x] **The light aircraft fly to their figures** - the J-3 Cub, PA-28 and
      Cessna 182, from JSBSim's models. *Verification: each lands inside its
      tolerance on every figure recorded from its handbook, as the Cessna 172P
      does.* Stated, 2026-09-19, CI run 35417893114: the Cessna 182S flies all
      nine of its handbook's figures inside their ranges on every platform,
      the PA-28-180 all seven and the J-3 Cub all five - the 182S's ground
      roll 793 ft against 795, the PA-28's 747 against 720, the Cub's cruise
      64.6 knots against 63.4, and every stall within 2 knots of its
      handbook's; the Cub's stall flown with the Trainer's two people, its
      manual naming no weight.
- [x] **The airliners fly to their figures** - the A320, 737, 747 and 787-8,
      from JSBSim's models. *Verification: each lands inside its tolerance on
      figures from its manufacturer's airport-planning document and its
      type-certificate data sheet: take-off distance at maximum weight, climb,
      cruise Mach and ceiling.* Stated, 2026-09-19, CI run 35423458464: the
      A320, 737-300, 747-400 and 787-8 fly all four of their figures inside
      their ranges on every platform - take-off runway lengths at maximum
      weight, flown as FAR 25 defines them, of 6,200, 7,790, 11,270 and
      10,880 ft against their documents' 5,850, 8,400, 10,500 and 10,100;
      engine-out climbs of 3.7, 2.4, 4.0 and 2.6% against FAR 25.121(b)'s 2.4
      and 3.0; cruise at Mach 0.84, 0.78, 0.91 and 0.91 at 35,000 ft; and each
      still climbing at its type certificate's ceiling. The A320 is the -214,
      with the CFM56-5B4 its model lacked; the climb is FAR 25's floor, as
      neither manufacturer publishes a climb rate.
- [x] **The F-15 and F-22 fly to their figures**, from JSBSim's models.
      *Verification: maximum Mach at altitude, climb rate, service ceiling and
      sustained turn rate, each inside its tolerance of published figures.*
      Stated, 2026-09-19, CI run 35430601204: the F-15C flies all six of the
      Air Force's Standard Aircraft Characteristics figures inside their
      ranges on every platform - Mach 2.40 at 45,000 ft against 2.39, sea-level
      climbs of 15,300 and 56,900 ft/min against 15,250 and 55,960, service and
      combat ceilings of 46,900 and 57,600 ft against 46,750 and 56,100, and a
      sustained turn of 7.82 degrees a second against 7.87 - and the F-22A
      all five of the Selected Acquisition Report's and the fact sheet's:
      supercruise at Mach 1.77 against 1.76, Mach 0.8 to 1.5 in 52.7 seconds
      against 52.4, 7.35 degrees a second (3.7 g), Mach 2.15 at 40,000 ft, and
      still climbing at 50,000 ft. The F-15C's thrust and drag are fitted to
      its chart of excess power; the F-22's are pinned by five figures only.
- [x] **An A380 flight model**, written here from published data: JSBSim has
      none. *Verification: as the airliners.* Stated, 2026-09-19, CI run
      35435906751: the A380-841, written from Airbus's A380 Aircraft
      Characteristics, its type certificates and the Boeing 747's published
      derivatives, flies the airliners' four figures inside their ranges on
      every platform - 9,300 ft of take-off field at 575,000 kg against the
      AC's 9,734, 4.2% with an engine out against JAR 25's 3.0, Mach 0.91 level
      at 35,000 ft, and still climbing at its 43,000 ft maximum operating
      altitude.
- [x] **A Learjet 35A flight model**, written here from published data:
      JSBSim has none. *Verification: as the airliners, from its flight
      manual's figures.* Stated, 2026-09-19, CI run 35442214130: the Gates Learjet
      35A, written from its FAA-approved flight manual, its type certificate
      and NASA's measurements of the Learjet 23, flies seven figures inside
      their tolerances on every platform - 5,150 ft of take-off field at
      18,300 lb against the manual's 5,300, 7.6% with an engine out against
      FAR 25's 2.4, Mach 0.82 level at 41,000 ft against the Air Force's 0.81,
      still climbing at 45,000 ft, and its stalls with flaps up, 8 and 40
      within two knots of the manual's.
- [x] **F-35A and B-2 flight models**, written here from what is published.
      Much of their performance is not public: they are held to what is -
      maximum speed, ceiling, and range where it is given - and nothing more
      is claimed. *Verification: each published figure inside its tolerance,
      with every source named in `ASSETS.md`, and `PROJECT_STATUS.md` saying
      which of the aircraft's behaviour no figure pins.* Stated, 2026-09-19,
      CI run 35442214130: on every platform the F-35A flies Mach 1.62 against its
      published 1.6, still climbs above 50,000 ft, and ranges 1,830 nm on
      internal fuel against more than 1,200; the B-2A flies Mach 0.89 level
      at 40,000 ft, "high subsonic", still climbs at its 50,000 ft ceiling,
      and ranges 6,100 nm against about 6,000. Every source is in `ASSETS.md`,
      and `PROJECT_STATUS.md` says what no figure pins: nearly all of it.
- [x] **The Short S.23 on water**, from JSBSim's model and its hydrodynamics.
      *Verification: it floats at rest at its published draught, takes off from
      the sea and from a lake within its published distance, and alights on
      water and comes to rest afloat.* Stated, 2026-09-20, CI run 35472220036:
      on every platform the 1936 Empire flying boat floats at 3.7 ft at the
      main step against the 4.0 ± 0.5 ft scaled off Flight's general
      arrangement, takes off from calm water in 825 yd against Gouge's 795,
      and from the Tasman Sea off Sydney and from Lake Macquarie on the DEM in
      819 and 820 yd; it flies 202 mph at 5,500 ft against a published 200 and
      climbs 912 ft/min at sea level against 950; and it alights on water and
      comes to rest afloat with its engines stopped.
- [x] **Water where the DEM says it is** - the sea, and lakes and rivers from
      the DEM's water body mask - for the seaplane to alight on and landplanes
      not to. *Verification: at reference lakes, rivers and coasts the ground
      under an aircraft is water or land as the mask says, and a landplane that
      alights on water does not roll out on it.* Stated, 2026-09-20, CI run
      35449051367: at eleven places around Sydney - the Tasman Sea off Bondi and
      Maroubra, Lake Macquarie, Tuggerah Lake, Sydney Harbour, Botany Bay,
      Broken Bay, and four on land - the ground under a Cessna flown over each
      is water or land as the Copernicus mask says, its pinned tile read as an
      independent decoder reads it; and all fifteen aircraft set down on water
      ditch where they meet it, no wheel taking weight, where on land they roll
      on, on every platform.
- [x] **The HUD for fast aircraft** - Mach and flight level where they apply.
      *Verification: the numbers in a `--shot` of a jet at altitude match its
      state at that tick, as for the Cessna.* Stated, 2026-09-20, CI run
      35449051367: on every platform and GPU driver a shot of the A320 at 11,000 m
      over Sydney shows MACH and FL lines that match its traced state at that
      tick, read back out of the frame, and a Cessna's shows neither.
- [x] **Visual models from FlightGear aircraft**, each licence checked.
      *Verification: `ASSETS.md` names the source, commit and licence of every
      model that ships, and a model without an entry fails a test.* Stated,
      2026-09-20: fourteen of the sixteen aircraft ship a visual model, made
      from FlightGear's by `tools/make_models.py` and recorded in `ASSETS.md`
      with their source, revision and licence - eight on the terms their own
      directory states, six on FGAddon's project-wide GPL requirement, which
      the project owner decided on 2026-09-20 they ship on and which each
      entry names as a policy rather than a grant. The Learjet 35A and the
      F-35A ship none: FlightGear has no model of either, and `ASSETS.md`
      says so. Every model is held by tests to its published length and span
      and to facing the way it flies, and the committed meshes are checked
      against what the script makes. Nothing draws any of them yet - that is
      the views item below.
- [ ] **A visual model put where its aeroplane is.** Each model is in its
      FlightGear aircraft's own frame, whose origin is that aircraft's FDM
      datum and not always glideslope's: the Cub's and 747's wheels are
      within 0.14 m of their JSBSim contact points, the Cessna 172P's 1.03 m
      out. *(Found 2026-09-20 making the models.)* *Verification: for every
      aircraft that has a model, the model's wheels sit on the ground within
      a stated distance when the aeroplane is standing on it, and its nose,
      wingtips and fin are where the flight model's geometry says.*
- [x] **An aircraft chosen at start.** *Verification: every aircraft can be
      chosen, takes off, and passes its published-figure checks.* Stated,
      2026-09-19, CI run 35442214130: `--aircraft` flies any of the fifteen the
      data holds, in the air or `--on-ground`, and refuses one it does not,
      naming those it does; a test stands each on a runway and every one
      climbs through 200 ft; each passes its published-figure checks; and in
      the client on every platform the F-22 chosen flies at its start's 300
      knots and a Cessna on the ground at Sydney stands at rest on the DEM.
- [ ] **Views: the cockpit, and outside from ahead, behind, left, right and
      above, and a free orbit**, switched by a key and chosen with `--view`.
      *(Asked for 2026-09-18; needs the visual models above.)* *Verification:
      a `--shot` from each view at a fixed tick draws the aircraft's model
      where that view's camera puts it - its outline within a stated number of
      pixels of the model projected independently from the same camera - and
      the cockpit view's eye is the pilot's; switching views steps nothing in
      the flight.*

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

## Phase 5c — Learning to fly

Checklists and lessons (`FEATURES.md`, learning to fly). They build on the AI
pilot (Phase 4), which demonstrates, and on aircraft as data (Phase 5), which
checklists are part of. A lesson ends in a debrief, never a score
(`FEATURES.md`, deliberately not).

- [ ] **Checklists as part of each aircraft's data**, for every phase of flight,
      from its handbook or pilot's notes in this project's own words, each
      source recorded in `ASSETS.md`. *Verification: every aircraft in the
      roster has a checklist for each phase of flight, and every item either
      names a state of the aircraft that shows it done or is marked the pilot's
      to confirm; a test walks them all.*
- [ ] **Checklists on screen, ticking themselves.** *Verification: the test
      pilot flies the Cessna's before-take-off, take-off and climb by the book,
      and every item the aircraft can see ticks at the tick its state first
      shows it done; flown with the flaps left up, that item stays unticked and
      is flagged.*
- [ ] **Lessons** - take-off, the circuit, climbs and descents, turns, stalls,
      approach and landing - for each class of aircraft. *Verification: each
      lesson flown by the AI pilot to the book passes every stage; flown with a
      stated fault - rotating early, an approach too fast, no flap - the
      debrief names that fault and no other.*
- [ ] **The instructor demonstrates, then hands over.** *Verification: for
      each lesson the AI pilot flies the demonstration within the lesson's own
      limits, hands the controls to the player with no step in any control, and
      takes them back on request the same way.*

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
- [ ] **An autopilot that flies an approach and lands** - down a glidepath to
      a runway given by its threshold, heading and elevation, then a flare, a
      touchdown and the brakes; what the copilot below needs to bring an
      aircraft home. *(Added 2026-09-19 at the project owner's asking.)*
      *Verification: from 5 nm out, in calm air and in a 10-knot crosswind,
      each light aircraft is flown down a 3-degree glidepath and lands within
      5 m of the centreline, sinking under 300 ft/min, and stops on the
      runway.*
- [ ] **The copilot flies with you.** It reads the aircraft's state and, every
      few seconds and off the simulation thread, changes the autopilot's modes
      and the flight plan as the flight goes - the pilot in command of the
      autopilot, never of a control surface, which a model answering in
      seconds could not hold at the 120 Hz a control loop needs. Opt-in, with
      the player's own key. *(Added 2026-09-19 at the project owner's asking.)*
      *Verification: told "hold this heading to the coast, then follow it
      north at 2,000 ft", it does, its track within a stated distance of the
      coastline; after an engine failure it slows to the best-glide speed
      within 10 s and turns for the runway its plan names, if it can reach
      it; the step rate does not fall while it thinks; its responses are
      recorded, so CI replays the flight without a key; and without a key the
      simulator runs as before, with no copilot.*
- [ ] **The model never drives a control surface.** *Verification: the copilot
      can produce a flight plan and the autopilot's modes and nothing else,
      checked at configure time.*
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
- [ ] **Terrain over the whole Earth, streamed as an aircraft flies.** *(Found
      adding jets to the roster: a region three degrees across is crossed in
      minutes at Mach 0.8, and "anywhere on Earth" is a `CORE` feature with no
      item.)* *Verification: a flight from Sydney to Melbourne at cruise draws
      terrain under it the whole way, and the tiles in memory stay under a
      stated bound.*
- [ ] **Thermals from the ground beneath them, and lee waves trapped under a
      stable layer.** *(Found building thermals and mountain waves.)* The
      thermals rise alike over the sea, a lake and a sunlit slope, from one
      station's report; the waves are linear theory in one wind and one
      stability, so they rise away rather than lie trapped in a ridge's lee,
      and there is no rotor. *Verification: on a convective day no thermal
      rises over open water; and where the Scorer parameter falls with height,
      the lee of a ridge holds waves at the wavelength two-layer theory
      gives.*
- [ ] **Weather seen as it is.** *(Found building weather you can see; made a
      tail at the project owner's asking, 2026-09-19.)* The cloud drawn is
      flat sheets over a disc 60 km across the station, and does not drift
      with the wind; cumulonimbus is a deck 6 km deep, not a tower; a new
      report makes the sky again, so its cloud jumps rather than blends; the
      haze is one colour, lit by nothing; and rain and snow fall only within
      20 m of the eye. *Verification: frames a minute apart show the cloud
      moved as far as the wind at its height carries it, within a stated
      tolerance; a deck has depth - flown into, the frame is inside cloud, and
      from above its top is seen; a reported cumulonimbus is drawn as a tower
      taller than it is wide; a new report's sky blends in over a stated time,
      no frame between changing more than a stated fraction of its pixels;
      the haze is brighter toward the sun than away from it; and rain is
      drawn out to the reported visibility.*
- [ ] **Every aircraft's airframe meets the ground with its wheels up.**
      *(Found making water, 2026-09-20.)* The 737-300, 747-400, A320, B-2A,
      F-15C, F-22A and F-35A have no structure contact points, and JSBSim
      gives a retracted wheel no force: landed with its wheels up, the 737
      passes through the runway, 700 ft under it half a minute later. Their
      fuselages', nacelles' and wing tips' heights on the ground are in the
      airliners' planning documents and, for the F-22A, the Air Force's
      rescue manual (T.O. 00-105E-9); the others' are not published.
      *Verification: every aircraft landed on a runway with its wheels up
      comes to rest on its airframe, its centre of gravity above the ground,
      in a distance a stated friction gives.*
- [ ] **Propeller and mixture levers for the pilot.** *(Found making the
      Short S.23, 2026-09-20.)* The controls have a propeller lever and a
      mixture, which the figure flights set, but no key or binding works
      them: a pilot flies every aircraft full rich with its propeller at its
      highest rpm - the Mosquito's constant-speed airscrews at their highest, and the
      S.23's two-pitch ones in fine pitch, with its mixture through the gate
      to take-off boost, so that at full throttle in the air its engines turn
      3,185 rpm and give 1,185 hp where the Pegasus is rated at 2,600 and 920.
      *Verification: a binding and keys move each lever; the S.23 flown in
      coarse pitch at +2 1/2 lb from the controls turns its engines within
      their rated rpm at its top speed, and the Mosquito's rpm follows its
      lever.*
