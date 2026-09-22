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
and 787; the fighters F-15 Eagle, F-22 Raptor and F-35B Lightning II; and a
bomber, the B-2 Spirit. JSBSim ships flight models for all but the A380, the
Learjet, the Mosquito, the F-35B and the B-2, and those it ships are of uneven
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
- [x] **F-35B and B-2 flight models**, written here from what is published.
      Much of their performance is not public: they are held to what is -
      maximum speed, ceiling, and range where it is given - and nothing more
      is claimed. *Verification: each published figure inside its tolerance,
      with every source named in `ASSETS.md`, and `PROJECT_STATUS.md` saying
      which of the aircraft's behaviour no figure pins.* Stated, 2026-09-19,
      CI run 35442214130 for the F-35A, which flew Mach 1.62 against its
      published 1.6 and ranged 1,830 nm against more than 1,200. **That
      aeroplane is now the F-35B** (see the tails): it flies Mach 1.609
      against the same published 1.6 and ranges 1,372 nm against more than
      900, locally, and is held to no ceiling because none is published for
      any F-35; the B-2A flies Mach 0.89 level
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
      entry names as a policy rather than a grant. The Learjet 35A ships
      none: FlightGear has no Learjet of any mark, and `ASSETS.md` says so. Every model is held by tests to its published length and span
      and to facing the way it flies, and the committed meshes are checked
      against what the script makes. Nothing draws any of them yet - that is
      the views item below.
- [x] **A visual model put where its aeroplane is.** *(Found 2026-09-20
      making the models.)* *Verification: for every aircraft that has a
      model, the model's wheels sit on the ground within a stated distance
      when the aeroplane is standing on it, and its nose, wingtips and fin
      are where the flight model's geometry says.* Stated, 2026-09-21: a
      model is drawn at its flight model's visual reference point, moved by
      an offset `tools/align_models.py` measures per aircraft into
      `assets/models/alignment.txt` by putting the model's undercarriage on
      the flight model's. Thirteen aircraft are stood on the ground in
      JSBSim and the model under every wheel taking weight is on the ground
      within that aircraft's own stated distance; the flying boat floats
      rather than stands, and is held to its keels instead. All 97 contacts
      the fourteen flight models have - wheels, wingtips, tailcones, a
      radome, propeller tips - are within their stated distance of the
      model, and every model's span is within 6% of its flight model's, the
      PA-28 excepted and named because FlightGear's is a different mark.
      Four flight models describe nothing but their undercarriage, so their
      span is all the shape they can be held to; the test names them.
- [x] **An aircraft chosen at start.** *Verification: every aircraft can be
      chosen, takes off, and passes its published-figure checks.* Stated,
      2026-09-19, CI run 35442214130: `--aircraft` flies any of the fifteen the
      data holds, in the air or `--on-ground`, and refuses one it does not,
      naming those it does; a test stands each on a runway and every one
      climbs through 200 ft; each passes its published-figure checks; and in
      the client on every platform the F-22 chosen flies at its start's 300
      knots and a Cessna on the ground at Sydney stands at rest on the DEM.
- [x] **Views: the cockpit, and outside from ahead, behind, left, right and
      above, and a free orbit**, switched by a key and chosen with `--view`.
      *(Asked for 2026-09-18.)* *Verification: a `--shot` from each view at a
      fixed tick draws the aircraft's model where that view's camera puts it -
      its outline within a stated number of pixels of the model projected
      independently from the same camera - and the cockpit view's eye is the
      pilot's; switching views steps nothing in the flight.* Stated,
      2026-09-21: the aeroplane is drawn, and `--view` chooses where it is
      seen from, V steps round them. Each of the six outside views is shot
      twice, with the aeroplane and without, and the pixels that differ -
      its outline exactly - are held within two pixels of the model projected
      from the same camera; the cockpit draws no aeroplane, because the models
      have no interior, and its two shots are identical. The cockpit's eye is
      the flight model's own eyepoint, to the millimetre, for every aircraft
      that has a model. Every view traces the same flight to the same state.
      The light on the aeroplane is baked into the mesh, so it is made again
      as the aeroplane banks; a livery and moving control surfaces are tails.

## Phase 5b — Terrain providers

- [x] **Cesium ion as a visual provider** with the user's own token.
      *Verification: with a token, a `--shot` draws Cesium World Terrain with
      its attribution; without one, the provider says why it is unavailable and
      its tests report themselves skipped.* Done, 2026-09-21: `--terrain ion`
      draws Cesium World Terrain under Bing Maps Aerial with ion's own
      attribution, and without a token it says what is missing and where to
      put it. One test walks both halves in 23 seconds and reports itself
      skipped where there is no token. Waiting for every tile a whole-Earth
      tileset refines to had run past 25 minutes, which is why this item
      waited; a streamed provider is now given a bounded settling instead,
      and its frame is not claimed to be the same on every machine.
      `PROJECT_STATUS.md` says what was built to get there, and names two
      defects in Cesium Native it had to work around.
- [ ] **Google Photorealistic 3D Tiles** with the user's own Google Maps
      Platform key or Cesium ion token. *Verification: the same checks as
      Cesium ion, through both ways in.* Begun 2026-09-21 and not finished.
      Through a Cesium ion token they draw with their own photographs on
      them, refined - 452 tiles over Mount Taranaki - with Google's
      attribution on screen, and a test holds them to drawing and to their
      attribution. **Still to do: the other way in.** A Google Maps Platform
      key used directly is written and has never been run, because there is
      no such key on any machine here; `--terrain google` takes one in
      preference to the ion token when it is there, so the test covers
      whichever a machine has. Half of "through both ways in" is therefore
      unproven, and a key is all it needs.

- [x] **Attribution on screen for whichever provider is active.**
      *Verification: every provider, in every state, draws its attribution,
      walked by test.* Stated, 2026-09-21: whichever provider is drawing, its
      attribution is along the bottom of the frame - the open data's notices
      are its own, and a streamed provider's come from what it says about
      itself and from Cesium Native as its tiles load.
      `tests/cmake/terrain_provider.cmake` takes the provider as a parameter
      and walks all three, in both their states: with the user's key, where
      it counts the credit text on the frame, and with the config directory
      pointed at an empty place, where the provider must say what is missing
      and where to put it rather than fail. A provider whose key this machine
      has not got reports itself skipped.

- [x] **A measured visual-to-collision terrain mismatch.** *Verification: the
      bound for each provider at a set of reference airfields is stated in
      `PROJECT_STATUS.md`, with how it was measured.* Stated, 2026-09-21:
      `--mismatch` samples the drawn surface at the twelve surveyed runway
      ends of six airfields and prints it against the DEM the aircraft meets.
      **The open provider is 0.18 m from it at worst, Cesium ion 10.2 m and
      Google 10.1 m**, and a test holds each to that. All three are worst at
      Barrow, where the Arctic coast is thinly surveyed by anyone.

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
      to confirm; a test walks them all.* Begun 2026-09-21 and not finished.
      All sixteen aircraft have all nine phases, 144 checklists in all, and
      seven tests walk them: every item either names a state its own aircraft
      really has or is the pilot's to confirm, every band is one its
      aeroplane's controls can reach, and a file that is wrong is refused
      where it is wrong. **Three of the sixteen are now written from the
      aeroplane's own handbook** - the Cessna 172P's and 182S's Pilot's
      Operating Handbooks and the PA-28-180's Owner's Handbook, each read
      from the copies `ASSETS.md` records, their items and figures kept in
      the handbook's own order. **Still to do: the other thirteen.** The
      J-3 Cub's manual survives only as scans with no text in either copy
      recorded; no flight manual is public for the B-2A, the F-22A or the
      F-35B; and the airliners' and business jet's operating manuals are
      their operators' and are not published. Those thirteen follow the
      ordinary practice for the type with figures from this project's own
      `assets/figures/<id>.xml`, and `ASSETS.md` says which is which rather
      than naming a source that was not used.
- [x] **Checklists on screen, ticking themselves.** *Verification: the test
      pilot flies the Cessna's before-take-off, take-off and climb by the book,
      and every item the aircraft can see ticks at the tick its state first
      shows it done; flown with the flaps left up, that item stays unticked and
      is flagged.* Done, 2026-09-21: `--checklist PHASE` puts a phase's list
      down the right of the screen, each item marked ticked or still to do,
      and it ticks itself as the aeroplane flies. An item ticks at the first
      tick its state shows it done and stays ticked, because a checklist
      records that a thing was done, not that it is still true. Flown by the
      book, every item of the taxi, take-off and climb lists that the
      aeroplane can see is ticked; flown with the flaps up, the landing
      list's flaps item never ticks and is flagged as outstanding. A frame
      test reads the block back off a shot and holds every line to what the
      flight said it drew. **The verification names a "before-take-off"
      phase; there is none.** The nine phases are `FEATURES.md`'s, which go
      from before start to taxi to take-off, so taxi is what was flown in its
      place.
- [ ] **Lessons** - take-off, the circuit, climbs and descents, turns, stalls,
      approach and landing - for each class of aircraft. *Verification: each
      lesson flown by the AI pilot to the book passes every stage; flown with a
      stated fault - rotating early, an approach too fast, no flap - the
      debrief names that fault and no other.* **Not started, and it cannot be
      until the AI pilot can fly what the lessons teach.** Found 2026-09-21,
      reading what the AI pilot does: it holds a heading, an altitude, a
      vertical speed and a speed, and follows waypoints in the cruise. It
      cannot take off - the client refuses `--on-ground` with `--autopilot`,
      saying so - it cannot fly an approach or land, it never touches the
      flaps, the gear, the brakes or the trim, and its gains are the Cessna's
      and will not suit a jet. So a take-off lesson, a circuit, a stall and a
      landing have no pilot to fly them. **The approach-and-landing item in
      Phase 8 below is the largest piece of it, and comes before this one
      although it is written after.** The classes of aircraft are also not
      data: `REQUIREMENTS.md` names seven in prose and no aircraft file says
      which it is. Two pieces of it are now built and tested, for the light
      aircraft: an autopilot that flies an approach and lands (Phase 8's own
      item, ticked below), and one that takes off - each light aircraft holds
      the centreline within 1.4 m and is airborne within a third of its
      handbook's ground roll. Still missing: the circuit, the stalls, the
      turns, the lesson and debrief layer itself, and all of it for the other
      six classes of aircraft.
      **The reference speeds a lesson needs are published for the light
      aircraft and not for the rest**, which was looked into on 2026-09-21
      rather than assumed. Only six of the sixteen publish a stall speed and
      six a climb speed. The airliners' and business jet's operating manuals
      are their operators' and are not published; the A320 family's stall
      speed is not a tabulated figure at all, being defined as VS1g behind a
      low-speed protection the crew cannot override, with operating speeds
      referenced to that; and no flight manual is public for the B-2A, F-22A
      or F-35B. **There is a way through that does not invent figures**:
      `figures.cpp` already has a `stall_speed` flight that can measure any
      model, so a lesson's reference speed could be measured from the
      aeroplane it teaches rather than published for it. That is a decision
      about what a lesson's figures mean, and it is the project owner's to
      make, so it is written here rather than taken.
- [ ] **The instructor demonstrates, then hands over.** *Verification: for
      each lesson the AI pilot flies the demonstration within the lesson's own
      limits, hands the controls to the player with no step in any control, and
      takes them back on request the same way.*

## Phase 6 — Client and server

- [ ] **The transport** — gearstick's, with its own magic value, written up
      byte for byte in `TRANSPORT.md`. *Verification: a client written from
      `TRANSPORT.md` alone completes a session, and a gearstick client is
      refused cleanly.* Begun 2026-09-21 and not finished. The envelope and
      the encoding are built, tested and written up: six bytes of magic,
      version and type, this project's own magic `GLDS`, little-endian
      fixed-width integers, and a double as its IEEE-754 bits. A reader that
      cannot run off the end of a datagram, walked against every truncation
      and every single-byte change there is. `TRANSPORT.md` says all of it
      byte for byte, says what the transport does not claim, and says what is
      not built - and a test holds the document and the code to each other.
      **The handshake is built, 2026-09-22**: `Noise_IK` over X25519,
      ChaCha20-Poly1305 and BLAKE2b, on libsodium. Two honest ends agree
      crosswise and each learns who the other is; eight runs with the same
      keys give eight different sessions; a client told the wrong server key
      gets nowhere; **25,755 one-byte changes and 101 truncations of an
      initiation, and 190 changes to an answer, are all refused**; and a
      replayed initiation makes a session the replayer cannot read, which is
      work done for a stranger and is named in `THREATS.md` rather than
      defended. **The suite is BLAKE2b where `REQUIREMENTS.md` 6.7 says
      BLAKE2s, and that is a decision taken without the project owner**:
      libsodium has no BLAKE2s at all, BLAKE2b is a hash the Noise
      specification defines, and the alternative was hand-written
      cryptography in a project that has none. It is one line to reverse.
      **What is not claimed: that it matches the specification.** There are no
      published test vectors for this suite here, so what the tests show is
      that two honest ends agree and that nothing else does.
      **The sealing is built, 2026-09-22.** A `SEALED` body is its sequence
      number and ChaCha20-Poly1305 ciphertext under the handshake's keys; the
      number is both the nonce and the additional data, and a replay window
      of 64 refuses anything already opened and anything further behind than
      that. Held over **all 720 orders of arrival** of six datagrams, each
      delivered once and twice; 7,650 one-byte changes and 30 truncations
      refused; both replay tests watched failing with the window taken out.
      A client and a server complete a whole session over a real loopback
      socket and seal both ways.
      **The state stream is built, 2026-09-22.** A server says where every
      aircraft is 25 times a second inside a sealed datagram - Earth-centred
      double positions, because the whole world is in play and there is no
      session origin to be near - and a client opens it and reads the
      positions back out. Held end to end over a real socket, with the
      aircraft held to the place the flight plan starts over and to the
      500 ft the server stacks them apart, so a packet of the right size full
      of the wrong place fails.
      **Still to do, and it is the verification itself.** Neither half has
      been done: no client has been written from `TRANSPORT.md` alone by
      somebody who did not write the code, and there is no gearstick client
      here to be refused. Until both, this item does not tick. A client also
      cannot yet fly: its inputs reach no aircraft.
      **How libsodium gets in was looked into on 2026-09-21 rather than
      assumed**, and it is not as simple as a submodule: libsodium ships a
      `Findsodium.cmake` and no CMake build of its own - autotools on Unix,
      Visual Studio solutions on Windows - so a submodule under `ext/` would
      need a CMakeLists written here for somebody else's library. vcpkg is
      the way this project already builds thirty libraries and has a
      libsodium port, **but `cmake/CesiumNative.cmake` refuses to configure
      unless `vcpkg.json` lists exactly what Cesium Native's own manifest
      lists, less curl**, so adding anything of this project's own fails the
      build at once. That guard exists so upgrading Cesium Native cannot
      quietly drop a dependency, and it keeps that purpose if it is changed
      to hold that theirs is a subset of ours and that the difference is a
      declared list of this project's own. That change comes with libsodium,
      not before it.
- [x] **Reliable delivery** for lobby, session, weather, aircraft definitions,
      terrain dataset and controller-swap messages. *Verification: every one
      arrives exactly once and in order under injected loss.* Done,
      2026-09-22. The layer numbers each message, repeats it until it is
      acknowledged, hands them up in order and throws away a duplicate. The
      six messages the item names now exist: each begins with a byte saying
      which kind it is, and `TRANSPORT.md` writes every field of every one of
      them out byte for byte, with a test holding the document and the code
      to each other. **The weather is sent as its raw METAR** rather than as
      twenty re-encoded fields, so two ends cannot read it differently. All
      six were put through the layer under **every pattern of loss over
      twelve datagrams - all 4,096** - and in each one every message arrived
      exactly once, in order, and read back as the message it was written
      from; the worst pattern took 21 datagrams.
- [ ] **The server** — `glideslope_server`, with `--headless`, `--players N`,
      `--port`, `--store FILE`, `--key HEX` and `--timeout`, and a dashboard
      otherwise — **still to do: the dashboard is a terminal one, and
      `REQUIREMENTS.md` 6.6 asks for a window showing each client's ping and
      traffic with a control to drop them.** *Verification: every flag is
      exercised by a test, and a player count outside 1 to 4 is refused.*
      **The flags are done, 2026-09-22.** The binary
      binds its port, admits clients through the handshake and draws a
      dashboard that refreshes each second with a row per slot; `--headless`
      prints its settings and runs without one. Sixteen tests exercise every
      flag, both ends of the 1 to 4 range and both sides of them, a key that
      is the wrong length and a key that is not hexadecimal, a port that is
      not one, a flag with nothing after it and an option it does not know.
      **Every flag now drives something, and two that used to drive nothing
      are watched doing it.** `--store` is an SQLite file that keeps the
      server's key, so a client given that key out of band still finds the
      same server after a restart; all four ways a server comes by a key are
      walked and counted. `--timeout` lets go a client that has gone quiet
      and gives its slot back, watched with a real client connecting and
      exiting while the server runs. **What is left is the dashboard itself:**
      it is drawn in the terminal rather than in a window, it shows a row per
      slot but no ping and no traffic for any of them, and there is no way to
      drop a client from it. It says so on screen rather than looking as
      though it works.
- [x] **Lobby, identity and slot assignment.** *Verification: slots are assigned
      by the server, the same whatever order players connect in.* Done,
      2026-09-22. A player is known by a key, out of band - the static public
      key the handshake will use - and **a slot is that key's rank among
      those present**, not the order they arrived in, which is what
      `REQUIREMENTS.md` 6.5 asks for: "the server decides who is which
      player, not whoever connected first". So the assignment is a function
      of who is in the session, and connecting the same people in any order
      gives each the same slot. **Walked exhaustively**: every non-empty
      subset of four players and every order each could arrive in - fifteen
      subsets, 64 orders - all agreeing, with the four test keys deliberately
      shuffled against their names so that assigning by arrival or by name
      would fail. A session is full at its player count and the next is
      refused; asking twice is one player; a count outside 1 to 4 is held to
      it. The server's dashboard now draws its rows from the session itself,
      so what is on screen is what a `LOBBY` would carry. **What a slot being
      a property of the set costs, said plainly**: somebody joining can move
      somebody already in, which is why the lobby is sent whole and a client
      is told its slot rather than remembering it.
- [x] **The server flies every aircraft**, at 120 Hz against the collision
      terrain, loading terrain around each aircraft anywhere on Earth.
      *Verification: aircraft on opposite sides of the world fly in one
      session, each over its own terrain.* Done, 2026-09-22. `--fly ID@LAT,LON`
      gives the server an aeroplane and where it starts, up to the four a
      session holds, and `--seconds N` stops it after a set time so that a
      test can watch a whole run. One DEM serves them all and caches tiles as
      it goes, so two aircraft on opposite sides of the world each pull their
      own; every one is stepped on the one thread, because a DEM is not
      thread-safe. **Two aircraft, Sydney and Denver, flew 2.003 s in 240
      steps - 120 Hz exactly - and found ground at 77 ft and 5,183 ft above
      the ellipsoid**, which is Sydney a few feet above the sea and Denver a
      mile up, each held to a band that place really has. **The test was
      watched failing**: a server given one flat terrain reports Denver's
      ground at 0 ft and is told it is not over its own terrain.
- [x] **Server-run AI aircraft**, how many a server setting, default 4.
      *Verification: a server runs the number it is given, and four when given
      none.* Done, 2026-09-22. `--ai N` says how many, 0 to 16, and four when
      it is not given. They fly a flight plan - `--plan FILE`, or the data's
      `plans/sydney-harbour.plan` - through the same `sim::Controller` handed
      to the AI that a player's aircraft would be, so an AI aeroplane on the
      server is an aeroplane with its controller set to the AI and nothing
      else. They are stacked 500 ft apart, because one plan is what there is
      to fly and four aeroplanes in one piece of sky would hide that rather
      than say it. **The count is read back out of the server** - it reports
      how many it ran and a line per aeroplane flying - so a server that
      printed a number and made a different number would fail. **Watched
      failing**: a server that ignores `--ai` and always makes four is told
      "asked for 2 AI aircraft and it ran 4".
- [x] **Inputs streamed with redundancy.** *Verification: no input frame is
      lost under injected loss.* Done, 2026-09-22. Inputs are not sent
      reliably, they are sent repeatedly: a lost input frame is worth nothing
      a moment later, so every packet carries the last four instead.
      **Stated exactly**, because "no loss ever" is not true of anything: a
      frame rides in the packet of its own number and the three after it, so
      it arrives if and only if one of those four arrives. That is held frame
      by frame over **every pattern of loss across twelve packets - all
      4,096** - of which 1,490 lose nothing at all and 2,606 lose at least
      one. Each control goes as a 16-bit fraction of -1 to 1, with both ends
      exact and a step of 3e-5, and **the client must fly what it sent rather
      than what its stick said**, so that its prediction and the server run
      the same numbers. **Watched failing**: a receiver that keeps only the
      newest frame is told "frame 1 did not arrive but a packet carrying it
      did get through".
- [x] **Client prediction and reconciliation.** *Verification: prediction error
      and correction size stay within stated bounds at 100 ms and 200 ms of
      simulated latency.* Done, 2026-09-22. The client flies its own JSBSim on
      its own inputs so the controls answer on the frame they are pressed, and
      keeps every input the server has not acknowledged. When the server's
      state arrives it puts the aircraft back to it and flies forward again
      through the rest. A correction of more than 20 m is snapped rather than
      hidden, because pretending smoothly to be somewhere wrong is worse than
      a visible jump. **The key technical risk named in `REQUIREMENTS.md` -
      setting a JSBSim instance to a given state - was already retired**:
      `capture` and `restore` have their own tests. Measured on one machine,
      the worst correction is **1.5 mm at 100 ms and 3.5 mm at 200 ms**, held
      to 0.1 m: thirty times the worst seen, so the bound would catch a
      reconciliation beginning to go wrong rather than merely being wide
      enough to pass. **It is a bound for one machine and says so**; across
      platforms it will be larger, and that is section 8.3's measurement. A
      second test stops a reconciliation that does nothing from passing: a
      client flown hard over on inputs the server never saw drifts 61 m, and
      is put back to within a rounding error.
- [x] **Other aircraft interpolated 100 ms in the past**, extrapolating when
      packets are late. *Verification: interpolation error stays within a
      stated bound under loss and jitter.* Done, 2026-09-22. An aircraft is
      shown between the two snapshots straddling the moment 100 ms ago;
      when none has arrived it is carried on from its last velocity, for at
      most half a second, and when one does arrive the difference is taken up
      over a quarter of a second rather than jumped. Angles go the short way
      round the compass. **The bound is two metres, and it was reasoned
      before it was measured**: carrying on straight through half a second of
      a rate-one turn at 200 m/s leaves the arc by about 1.3 m. Measured over
      **every pattern of loss across twelve snapshots - all 4,096** - the
      worst is **1.309 m**, at the pattern that loses every snapshot but the
      first; under jitter of up to 80 ms, which puts snapshots out of order,
      it is 9 mm; with nothing lost at all it is 2.9 mm.
- [ ] **Collisions resolved on the server**, mid-air and with the ground.
      *Verification: two aircraft put on a collision course collide on the
      server, and every client shows it.*
- [x] **Every network parser fuzzed** under sanitizers. *Verification: the
      seed corpus goes through every parser in CI.* Done, 2026-09-22. Eleven
      parsers - the envelope, all seven reliable messages, the kind byte, the
      reliable layer and the input receiver - against eighteen seeds: every
      datagram and message this project writes, and five things it never
      would, among them an empty datagram, an HTTP request and 1,232 bytes of
      one value. **Every seed goes through every parser**, not only the one
      that wrote it, because a datagram arrives before anybody knows what it
      is and a parser only ever shown its own output has not been tested.
      Each is also cut to every length, changed a byte at a time at six
      values, and mutated from a fixed seed so a failure can be had again:
      **188,617 reads, none out of bounds**. The build already compiles with
      `-fsanitize=address,undefined -fno-sanitize-recover=all`, so this is
      under sanitizers by construction, and ctest runs it in CI with the
      rest. The corpus is written out so it can be handed to libFuzzer or AFL
      rather than only living inside the test. **Watched failing**: a reader
      that takes the first byte of a body without checking there is one is
      caught on the empty seed.
- [ ] **The server's test flags** — `--seconds N`, `--plain`, `--window-dump`,
      `--window-shot`, `--window-press` — **still to do: the three
      `--window-` flags, which wait on the dashboard being a window.**
      *Verification: each is used by a ctest.* Two of the five are done,
      2026-09-22: `--seconds N` stops the server after a set time, which
      every server test uses to watch a whole run, and `--plain` draws the
      dashboard without the escape codes that clear the screen, so a test can
      read its rows. A test also walks every flag the usage text prints and
      fails if the parser has never heard of one — `--plain` was documented
      and silently ignored for an hour, and passed both of its own tests,
      because neither looked at what it did.
- [ ] **Network checks in CI** with injected latency, loss and jitter.
      *Verification: prediction error, correction size, interpolation error and
      the `--players` limit all checked against their stated bounds.*
- [x] **`THREATS.md` written.** *Verification: every message the server accepts
      is named in it with its defence, or with why it needs none.* Done,
      2026-09-22. It leads with what is missing - **the server accepts
      nothing yet**, because there is no handshake and no sealing - and marks
      every defence built, unwired or not built, so that nothing in it reads
      as protection that is presently doing anything. It then names all four
      datagram types and all seven reliable messages with their defences,
      citing the limits the code actually enforces, and says which messages a
      server would accept from a client at all: only `CONTROLLER_SWAP`, the
      other six being server-to-client. Amplification, replay, an
      unauthenticated datagram, exhaustion of the reliable layer's queues, a
      client claiming a slot or a player count, and a wrong terrain dataset
      each have a section. **Writing it found six disagreements between the
      code and the documents**, four of which were fixed the same day - among
      them a `WEATHER` message that at its limits could never have been sent
      - and two of which are items below. A test holds every message kind the
      code knows to being named in the document.
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
- [x] **A player disconnecting**, the aircraft removed or handed to an AI by
      session setting. *Verification: both settings, tested.* Done,
      2026-09-22. `--on-leave remove` takes the aircraft out of the sky and
      `--on-leave ai` hands it to an AI pilot flying the server's plan;
      `remove` is the default. A player leaves by going quiet - there is no
      goodbye message and a crashed client could not send one - so the
      `--timeout` sweep is what notices. Both settings are walked in one test
      and counted, with a real client connecting and stopping, and under `ai`
      the aircraft must have **moved** from where its owner left it, since an
      AI pilot that flew nothing would pass a check that only counted
      aeroplanes. An aircraft handed over is renumbered, because a player's
      aircraft is numbered by their slot and that slot is about to be given
      to somebody else.
- [x] **AI traffic with nobody connected.** *Verification: AI aircraft keep
      flying on an empty server, and a client that joins later finds them
      mid-flight.* Done, 2026-09-22. Both halves are held by one test: the
      server flies two AI Cessnas with nobody connected, and a client that
      waits five seconds before connecting finds **a clock reading 4.2 s and
      both aircraft away from where the flight plan starts**. Its own
      aircraft, made the moment it joined, is still on the start point and is
      the control in that measurement - if everything in the packet had
      moved, the test would be measuring the plan's start and not the passage
      of time. A server that stepped its aircraft only while somebody was
      connected fails it.
- [ ] **Controller-swap continuity in the network checks.** *Verification:
      swaps under injected latency, loss and jitter stay within the stated
      bound.*

## Phase 8 — LLM copilot

- [ ] **Natural-language commands become flight plans**, planned off the
      simulation thread and flown by the autopilot. *Verification: "take off,
      climb to 3,000 ft and orbit the CBD" produces a plan that the autopilot
      flies.*
- [x] **An autopilot that flies an approach and lands** - down a glidepath to
      a runway given by its threshold, heading and elevation, then a flare, a
      touchdown and the brakes; what the copilot below needs to bring an
      aircraft home. *(Added 2026-09-19 at the project owner's asking.)*
      *Verification: from 5 nm out, in calm air and in a 10-knot crosswind,
      each light aircraft is flown down a 3-degree glidepath and lands within
      5 m of the centreline, sinking under 300 ft/min, and stops on the
      runway.* Done, 2026-09-21, **out of order and on purpose**: Phase 5c's
      lessons cannot be flown until something can land, so this was brought
      forward. All four light aircraft fly the approach in calm air and in a
      ten-knot crosswind and land: worst touchdown 255 ft/min, worst 3.99 m
      from the centreline, each stopped within 722 m of a 3,000 m runway.
      The approach speed is not a number written here - it is a third above
      each aeroplane's own published landing stall, read from
      `assets/figures/`, and an aircraft that publishes no stall speed is
      refused rather than given a guess.
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

- [ ] **Cesium ion on Windows, where a body arrives compressed unasked.**
      *(Found 2026-09-21 turning `WINHTTP_OPTION_DECOMPRESSION` on and
      watching CI.)* *Verification: a Windows machine fetches Cesium ion's
      layer.json and reads it, and the weather still arrives.* **The cause
      was found on 2026-09-22 by reading Microsoft's documentation instead of
      guessing again.** The first attempt turned the option on, every
      Open-Meteo fetch failed on all three Windows jobs, and it was turned
      off with "why is not known" written beside it. Why: the body was read
      by asking `WinHttpQueryDataAvailable` how much was there and stopping
      when it said none - and its own documentation says not to use that
      answer to decide a response has ended, because not all servers
      terminate one properly. The read loop now takes fixed chunks until a
      read returns no bytes, which is what that documentation asks for and is
      right whether anything is being decompressed or not - **but it was not
      the cause, and CI said so.** Turned on a second time with the loop
      fixed, every Open-Meteo fetch failed again, and this time with a real
      WinHTTP code rather than a stale one: **12002, ERROR_WINHTTP_TIMEOUT,
      at `WinHttpSendRequest`** - raised before a single byte of body is
      read, which is earlier than the loop runs. So asking for a compressed
      body from that host, from those runners, times out the send, and why is
      still not known. The option is off again and the loop fix stays, being
      right on its own account. The symptom is now a known code at a known
      call rather than "why is not known"; the ion half needs a token no CI
      job has.

- [ ] **A livery on the aeroplane, and its control surfaces moving.** A model
      ships no texture, so a surface takes the flat diffuse colour of its
      AC3D or 3D Studio material, and no control surface, propeller or
      undercarriage moves: they are welded where the model has them, gear
      down. *(Found 2026-09-21 drawing the aeroplane.)* *Verification: a
      livery is on the aeroplane in a shot, and a shot with the stick over
      shows the ailerons moved.*
- [ ] **The aeroplane is lit by a light baked into its mesh.** The mesh shader
      has no normals, so the light is worked into each vertex's colour and
      the mesh made again when the aeroplane has banked far enough to see -
      five degrees. A normal in the vertex would light it on the GPU and
      make that unnecessary, at the cost of a normal on every terrain vertex
      too. *(Found 2026-09-21 drawing the aeroplane.)* *Verification: the
      aeroplane's lighting follows it through a roll with no mesh remade.*

- [x] **A checklist item's band is not held against what the aeroplane can
      reach.** The tests prove the property exists and that the aircraft's own
      flight model names it, but not that its value can ever fall inside the
      item's band: a flap band of 33 to 35 degrees on a type whose flaps stop
      at 32 would never tick, and nothing would say so. Each aircraft's bands
      were flown and checked by hand when they were written, which is not the
      same as a test keeping them right. *(Found 2026-09-21 writing the
      checklists.)* *Verification: every checked item's band is shown
      reachable by driving that aircraft's own controls, and an item put
      outside its aeroplane's travel turns the test red.* Done, 2026-09-21: a
      lever is held to its own travel, and where a lever has got to is
      measured by working it with the aeroplane standing still. States of the
      flight - a speed, a height, an engine's speed - are counted and left
      alone, because what they can reach is the whole envelope. Watched to
      fail with the Cessna's landing flaps put at 33 to 35 degrees: "its
      levers only move it between 0 and 30".

- [x] **Nothing here compiles first-party code under clang.** GCC does not
      warn about an unused constant at namespace scope in C++ - not under
      `-Wall -Wextra`, and not under `-Wunused-const-variable` at any level,
      which it honours for C alone - so the class reaches CI and reds macOS
      and Windows clang-cl together, as it did on 2026-09-21. Both Linux
      builds are GCC, so no local build can catch it, and checking by hand
      before a push is review, which this project does not rely on.
      *(Found 2026-09-21 reading a red macOS build.)* *Verification: a test
      compiles every first-party source under clang with the project's own
      warnings, reports itself skipped where clang is not installed, and
      turns red for an unused constant that GCC accepts.* Done, 2026-09-21:
      `tests/cmake/clang_warnings.cmake` re-runs the compile commands CMake
      already exports, under clang, syntax only, with the same flags the
      build used and the unused-constant warning added - 93 first-party
      sources in 100 seconds. Watched both ways with a constant nobody uses
      put into `checklist.cpp`: GCC built it without a word, and the test
      failed with "unused variable 'nobody_uses_this'
      [-Werror,-Wunused-const-variable]".

- [x] **One download that fails once reds the tree.** CI sets
      `GLIDESLOPE_REQUIRE_NETWORK`, so a file that cannot be fetched is a
      failure there rather than a skip - deliberately, because CI is what
      proves the pinned files are still fetchable. But `tests/cmake/fetch.cmake`
      tries each file once, so a third-party host being briefly unwell fails
      the build: SourceForge did exactly that on 2026-09-21 and Rocky 9 went
      red on a commit that had nothing to do with it. *(Found 2026-09-21
      reading a red CI run.)* *Verification: a file that never arrives is
      tried three times, says so each time, and then fails; a file already
      present is not fetched at all.* Done, 2026-09-21: each file is tried
      three times with three seconds between, and a wrong hash still fails at
      once rather than being retried - the source changed, and trying again
      would not change it back.

- [x] **Tests that run at once share one Cesium cache.** Up to four client
      tests run together against a single `cesium-cache.sqlite`, with nothing
      serialising them; SQLite refuses the write and Cesium Native logs
      "database is locked". Nothing is lost but the caching, so a tile is
      fetched again. *(Found 2026-09-21 fixing the log that cut a trace line
      in half.)* *Verification: the client tests run together and no run
      reports a locked database.* **The mechanism was read out of Cesium
      Native on 2026-09-22 rather than guessed at.** It is not readers
      blocking writers: `SqliteCache` already turns on WAL, which handles
      that. It is that the connection is opened and **no busy timeout is ever
      set** - there is no `sqlite3_busy_timeout` or `sqlite3_busy_handler` in
      the file - so SQLite's default of zero applies and a second writer
      fails on its first attempt instead of waiting a moment. That is theirs
      to fix and is drafted as a third issue in `docs/cesium-issues.md`. Done
      here, 2026-09-21, by stopping the tests sharing one file:
      `GLIDESLOPE_CESIUM_CACHE` names it, `tests/cmake/client.cmake` gives
      each test a name of its own under the same downloads directory - so
      each still finds its own cache next run - and the shared leak check
      fails any run that reports a locked database, which is the verification
      enforced from one place for all thirteen client tests. It costs
      nothing: 323 tests in 973 s against 977 to 1,006 before, the writes now
      succeeding rather than failing and the tile being fetched again.
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
- [x] **Every aircraft's airframe meets the ground with its wheels up.**
      *(Found making water, 2026-09-20.)* JSBSim gives a retracted wheel no
      force, so an aeroplane whose only contacts are its undercarriage falls
      straight through the runway when it is landed with its wheels up. Every
      aircraft now has an airframe to come down on: the five that had nothing
      - the 737-300, the 747-400, the B-2A, the F-22A and the F-35 - gained a
      belly, a nose, two wing tips and a tail measured from their own visual
      models, and an airframe now scrapes the runway instead of rolling along
      it on a tyre's friction, which had the A320 still doing 118 knots after
      three minutes and 11 km.
      *Verification: every aircraft landed on a runway with its wheels up
      comes to rest on its airframe, its centre of gravity above the ground,
      in a distance the stated friction gives.* Done, 2026-09-22: all fifteen
      landplanes rest on their airframes - the 747-400 at 4.9 ft above the
      runway, the B-2 at 6.7, the F-22 at 1.3, the F-35B at 1.4 - and none
      goes through it. The A320, which never stopped, now stops in 1,332 m.
      The five whose wheels do not retract are flown too and held to what
      applies to them, and the flying boat is left out with its reason.
- [x] **Propeller and mixture levers for the pilot.** *(Found making the
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
      lever.* Done, 2026-09-21: the controls gained a `propeller` a binding
      can name, the quadrant's three levers now sit in the order the hand
      finds them - throttle, propeller, mixture - and the keyboard works the
      mixture with the comma and full stop and the propeller with the square
      brackets, each lever staying where it is left. **The S.23 in coarse
      pitch at NORMAL turns 2,169 rpm**, inside the Pegasus's rated 2,600,
      where a pilot with no lever to move flew it at 3,185; the Mosquito's
      airscrews turn 2,530 with the lever forward and 2,283 at four tenths.
- [x] **Four visual models are drawn sunk into the ground.** *(Found giving
      the aircraft an airframe, 2026-09-22.)* `assets/models/alignment.txt`
      records where each visual model sits on its flight model. Its height
      was fitted along with its length, and the fit could buy a smaller
      average error by burying a model whose mesh and flight model disagree
      about where the undercarriage is: the 747-400 was drawn 2.15 m below
      the ground, the B-2 1.67 m, the F-22 0.86 m and the Mosquito 0.69 m.
      The height is now anchored instead of fitted - a model drawn with its
      undercarriage down has a tyre as its lowest point, and that point
      belongs at the aeroplane's lowest wheel contact.
      *Verification: every visual model's lowest point sits within a stated
      distance of its aeroplane's lowest wheel contact, the same distance for
      all of them.* Done, 2026-09-22: all fifteen sit within a centimetre,
      which is the alignment file's own rounding. The airframe contacts did
      not move, because they anchor to the wheels themselves - and they now
      lie within 2 mm of the mesh, where the 747-400's were 2.16 m from it.
- [x] **The F-15C's airframe slides on the wrong friction.** *(Found giving
      the aircraft an airframe, 2026-09-22.)* Its six airframe contacts came
      from the model it was made from and carried a rolling friction of 0.2,
      where this project states 0.4 for an airframe scraping a runway, on a
      spring of 10,000 lb/ft - four feet of give under an aeroplane of 45,713
      lb. Landed with its wheels up it slid 2,734 m and settled with its
      centre of gravity below the runway.
      *Verification: the F-15C's wheels-up landing stops in the distance the
      stated friction gives, as every other aircraft's does.* Done,
      2026-09-22: it now stops in 1,029 m where the stated friction gives
      1,093, and rests 2.9 ft above the runway instead of 0.35 ft below it.
- [x] **The F-35A becomes the F-35B.** *(Decided by the project owner,
      2026-09-22.)* The F-35A was the one aircraft with no visual model and
      the one that still fell through the runway with its wheels up, and both
      had the same cause: FGAddon has no F-35A. It does have an F-35B, whose
      visual model ships under a verbatim GPL-3.0 and draws its undercarriage,
      which is what the airframe measurement needs. So the simulator models
      the B instead - a change of variant, not of ambition: its own published
      size, weights, engine and figures throughout, written from what is
      published as the A was. **Its lift fan is not modelled**, so it takes
      off and lands on a runway like any other fighter; hovering, vertical
      landing and short take-off are named below as work not done.
      *Verification: the F-35B flies its published maximum Mach and range;
      its visual model sits on its flight model within the same distance
      every other is held to; and it comes to rest on its airframe with its
      wheels up, which empties the list of aircraft that do not.* Done,
      2026-09-22: it flies Mach 1.609 against its published 1.6 and ranges
      1,372 nm against more than 900; its model sits 0.31 m from the contacts
      it stands on, better than most; and it rests on its airframe 1.4 ft
      above the runway. **It is held to no ceiling**, because Lockheed Martin
      publishes none for any F-35 - the "above 50,000 feet" the F-35A was
      held to is the Air Force's, for the A.
- [ ] **The F-35B cannot hover, land vertically or take off short.**
      *(Found switching to the F-35B, 2026-09-22.)* The F-35B is a STOVL
      aeroplane, and its shaft-driven LiftFan, three-bearing swivel nozzle
      and roll posts are what make it one. None of that is modelled: the
      flight model flies its wing, weights and engine in conventional flight
      and uses a runway like any other fighter. Rolls-Royce publishes the
      LiftSystem's component thrusts and Lockheed Martin the installed total,
      so there are figures to build to; what is not published is how the
      aeroplane handles on them.
      *Verification: the F-35B hovers at its published vertical thrust, lands
      vertically and takes off in a published short-take-off distance, each
      figure named with its source.*
- [ ] **The Cesium cache still locks when the rendering tests run together.**
      *(Found running the suite, 2026-09-22.)* Each client test was given a
      cache file of its own on 2026-09-22, which fixed the case then seen.
      Adding nine tests changed how `ctest -j4` interleaves them and two
      rendering tests failed on "database is locked" again, each with its own
      named file - so a file per test is not enough. `CesiumAsync::SqliteCache`
      turns on WAL and sets no busy timeout, so any concurrent access is
      refused at once rather than waited for. The drawing itself was right in
      both runs: the failing one still put 100% of its drawn pixels inside the
      projected outline.
      *Verification: the whole suite run at `-j4` reports no locked cache,
      ten times over.*
- [x] **The reliable layer believes an acknowledgement it is told.**
      *(Found writing `THREATS.md`, 2026-09-22.)* `Reliable::received` takes
      the acknowledgement number out of any datagram handed to it and drops
      every message up to it, so one forged datagram carrying `0xFFFFFFFF`
      empties the send queue and those messages are never sent again. The
      layer's own header says it is fed by the transport above it and that
      being fed rubbish is not a reason to stop, and once the sealing exists
      nothing unauthenticated will reach it - but nothing enforces that
      today, and the layer is reachable in tests and in any future caller
      that forgets.
      *Verification: a forged acknowledgement from outside the session
      changes nothing that the session has not yet had acknowledged.* Done,
      2026-09-22: the layer now keeps the highest number it has actually put
      on the wire and ignores any acknowledgement above it, because an
      endpoint cannot have received a message that was never sent. A forged
      `0xFFFFFFFF` lets go of nothing and every message still arrives.
      **What this does not do**: a forged acknowledgement of a message that
      *was* sent is still believed, because nothing yet tells a forged
      datagram from a real one - that is the handshake and the sealing, which
      are named above as not built.
- [x] **No message rejects a number that is not one.** *(Found writing
      `THREATS.md`, 2026-09-22.)* Every `f64` on the wire is read as whatever
      bits arrive, so a NaN or an infinity in a latitude, a microburst's
      radius or the simulation's clock is accepted and handed up. A NaN
      position would spread through the floating origin and the terrain
      query; an infinite duration would never end.
      *Verification: every floating-point field of every message refuses a
      NaN and an infinity, and the space of fields walked is stated.* Done,
      2026-09-22: the refusal is in the one reader every message field goes
      through, so a field added later is checked without anyone remembering
      to. All nineteen floating-point fields the seven messages carry are
      walked - the other three kinds carry none, and are named - each with
      both infinities and four NaNs, and each with five extreme but real
      numbers that must still read. `docs/TRANSPORT.md` says so where it says
      what a reader must refuse.
