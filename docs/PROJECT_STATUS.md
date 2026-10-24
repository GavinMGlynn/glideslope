# Project status

**What this file is.** The single source of truth for what actually works, and
the place detail belongs. `FEATURES.md` says what the simulator should be;
`COMPLETION_PLAN.md` says what order it gets built in and how each item is
verified; this file says what is true *today*, with the gaps named first.

**Never describe a partial module as working.** "Working" means 100% of what it
claims. Anything less is reported with the missing part named *first* — "terrain
streams and draws; nothing collides with it yet", never "terrain works".

**Words used here.** *Gearstick* is the author's earlier project, a racing game,
whose build, documents and discipline this one follows. *The brief* is the
design brief that became `REQUIREMENTS.md`. *Presentation* means anything that
draws, opens a window, reads an input device or plays sound. A *tail* is work
found along the way and added to the bottom of `COMPLETION_PLAN.md`. *Locally*
means the development machine: Rocky Linux 10 under WSL, with GCC 14.3.1;
anything proved elsewhere names the CI run.

---

## The honest summary, 2026-10-02

**Every numbered phase is ticked, 0 to 8; what is left is the tails** at the
bottom of `COMPLETION_PLAN.md` - flight-model shortfalls, flakes and CI
costs, and features named but not built (Phases 7 and 8, at the end of this
summary, and the gaps below say which).

**A Cessna 172P flies to its handbook over the real ground, in the real weather,
with a HUD and flight controllers, and the ground is drawn - satellite imagery
on the DEM, lit by its slope, but only around where the flight starts.** JSBSim is
built and linked and steps at a fixed 120 Hz, and glideslope's Cessna 172P lands
inside its tolerance on all nine published-figure checks; its state can be
captured and restored; `glideslope_cli selftest` flies a fixed five-minute log
and every package flies it; the five release builds fly every check to the same
numbers. `glideslope_cli height LAT LON` gives the Copernicus DEM's height
anywhere, fetching the tiles and geoid it needs, and the client's flight stands
the Cessna on that ground. The client flies it from the keyboard, joysticks,
HOTAS and yokes, with a HUD that tests read back out of the frame, and every
platform's package draws a frame of it on Vulkan, Direct3D 12 or Metal: sky,
HUD and the terrain, drawn by Cesium Native from the DEM over the cells around
where the flight starts, with EOX's Sentinel-2 cloudless imagery on it. There
are sixteen aircraft, fifteen of them drawn from FlightGear models, and a
server that flies them for up to four clients, each predicting its own and
drawing the rest (Phase 6, below). The weather is real: METARs and winds aloft, fetched live,
set JSBSim's wind, temperature and pressure, and the air moves as a pattern of
its own - gusts, turbulence, a boundary layer, reported shear, microbursts,
thermals and the terrain's lift - the same on every machine - and the weather
is seen: cloud decks where the METAR puts them, haze from its visibility, and
rain or snow.

**Phase 0 is complete — 7 of 7 items.** What exists is the ground everything
else is built on, one line per item, each verified:

1. a C++20 build with presets for Linux, macOS and Windows (MSVC and clang-cl);
2. a configure that refuses 32-bit toolchains and compilers below their floors;
3. warnings that are errors in every build type, on every compiler;
4. configure-time checks that the simulation includes and links no
   presentation, and a test that reads the CLI binary's real dependencies;
5. CI in which every preset configures, builds and passes its tests on its own
   platform;
6. a package per platform that runs from wherever it is unpacked;
7. these documents.

Every check above was also made to fail on purpose, and was seen to.

**Phase 1, the feel, is complete — 7 of 7 items.** JSBSim pinned and built; a
fixed 120 Hz step; the Cessna 172P flying to its handbook; state capture and
set/resume; the selftest's replay hash; the same flights on every platform; and
a packaged CLI that flies — each proved on every platform.

**Phase 2, the world, is complete — 12 of 12 items**, each proved in CI on
every platform: Earth-centred, Earth-fixed positions; a camera-relative
floating origin; reversed depth; a window and a GPU device on Vulkan (Linux,
and Windows through lavapipe), Direct3D 12 and Metal; the Copernicus DEM, read
directly, with a height query anywhere held to surveyed runway ends and
coastlines; and collision terrain the Cessna rests on (run 35241851702);
flight controllers; the HUD; the client's test flags; frames from CI and every
package (CI run 35244380011, package run 35244379942); and Cesium Native
drawing the open-data terrain, with open imagery on it (CI run 35331164089,
package run 35328306817). Windows took three tries: MSVC's limit on a path's
length, then a test tool's `sscanf`. What remains of the world is in tails:
terrain beyond the cells around the start, and buildings. Runways are flat
in the collision ground since 2026-10-01.

**Phase 3, weather, is complete — 4 of 4 items**, proved in CI on every
platform (run 35250647710) and fetched by every package (run 35248205205):
METARs from aviationweather.gov; winds aloft from Open-Meteo; both in JSBSim's
atmosphere, with MIL-F-8785C turbulence; and new reports blended in during a
flight.

**Phase 3b, wind that shears and gusts and hazardous air, is complete — 7
of 7 items**, proved in CI on every platform (runs 35331164089, 35336574855
and 35363959900): the same air on every machine, a METAR's gusts flown, the
wind near the ground as a boundary layer, reported wind shear, microbursts,
thermals and mountain waves, and weather you can see. **Phase 5, aircraft
choice, is complete - 15 of 15 items** - aircraft as data; the
Mosquito FB Mk VI, written here from its trials and Pilot's Notes and held to
fourteen of their figures, proved in CI on every platform (runs 35387301607
and 35409102752); the light aircraft from JSBSim's models - the Cessna 182S,
the Piper PA-28-180 and the Piper J-3 Cub, each made to fly to its handbook's
figures (CI run 35417893114); and the airliners - the Airbus A320 and the
Boeing 737-300, 747-400 and 787-8, held to their airport-planning documents
and type certificates (CI run 35423458464); and the fighters - the F-15C and
F-22A, held to the Air Force's and the Department of Defense's figures (CI
run 35430601204); an Airbus A380-841, written here from Airbus's and the
certifying authorities' documents (CI run 35435906751); a Gates Learjet 35A,
written here from its flight manual and NASA's measurements of the Learjet 23;
and the F-35B and B-2A, written here from what little is published of them
(CI run 35442214130); and any of them chosen at start, `--aircraft`, in the air or
on the ground (CI run 35442214130); the HUD's Mach number and flight level
for a fast aircraft; and water where the DEM's water body mask says it is,
on which a landplane ditches (CI run 35449051367); and the Short S.23
Empire flying boat, which takes off from the sea and from a lake, alights on
water and comes to rest afloat (CI run 35472220036). Fourteen of the sixteen ship a
visual model from FlightGear's aircraft, licensed, placed on the aeroplane
they draw and **drawn**: `--view` shows the aeroplane from the cockpit, from
ahead, behind, either side or above, or in an orbit around it, and V steps
round them. With that, Phase 5 is complete - 15 of 15.

**Phase 4, autopilot and navigation, is complete — 4 of 4 items**, proved in
CI on every platform (runs 35363959900, 35372183417 and 35378850716): the
holds for heading, altitude, airspeed and vertical speed; flight plans flown
past their waypoints; the user/AI controller swap; and `--autopilot`, with
which the client's AI flies a plan. It was begun before Phase 3b was
finished, which it should not have been; its work stopped until 3b was
proved. **Phase 5b, terrain providers, is complete — 4 of 4 items**: Cesium ion
and Google Photorealistic 3D Tiles with the user's own token or key, each
with its attribution on screen, and the gap between visual and collision
terrain measured at reference airfields.

**Phase 5c, learning to fly, is complete — 4 of 4 items** (2026-09-24):
checklists in every aircraft's data, ticking themselves on screen; lessons -
take-off, the circuit, climbs and descents, turns, stalls, and approach and
landing - for all seven classes, 42 lesson files, each flown by the book to
an empty debrief by every aeroplane whose figures give the speeds it names,
and each flown with a stated fault naming that fault and no other; and the
instructor, the AI pilot, demonstrating each and handing the controls over
and back with no step. The AI pilot takes off, flies the pattern, lands and
stops - on water, too, in the flying boat - every one of the fourteen
upright on its wheels from the touch to the stop, the jets as the FAA lands
a jet. The 747-400 and F-22A are taught turns alone, having no stall or
climbing speed; the Learjet cannot be rotated early since her stabilizer
was tied to her maintenance manual (a tail). See the log for 2026-09-22 to
2026-09-24, and later entries for what was fixed since (the F-35B's
circuit touching short, 2026-09-27; the fighters' rotation, 2026-09-26).

**Phase 6, client and server, is complete — 17 of 17 items.** A server flies
every aircraft, wherever on Earth, with AI aircraft of its own, and resolves
collisions. Clients join, are told what every aircraft is, stream their
inputs, predict their own aircraft and reconcile - once a pass, from the
newest update, so that a slow client keeps up (2026-10-23); a prediction
given a NaN as an input, or left with one by a step, is let go of and
started again from the server's word - but a NaN made inside one JSBSim
step from finite inputs still asserts in a debug build, and only
glideslope_cli's trap names it (the window client has the guard alone), and what made the one CI saw is unknown (2026-10-23) - and see the
others 100 ms in the past, over a reliable layer whose every parser is fuzzed. All of that
holds, within stated bounds, through 200 ms of latency with jitter and loss.
The client with the window flies on a server and draws everyone. Four clients
on Windows and Linux have flown together with an AI Cessna, and a fifth was
refused. A player's aircraft is held trimmed until her first input, and
that input keeps the trim, every control as the server held it, her
prediction agreeing (2026-10-21); every one of the sixteen so held keeps
her height at 3,000 ft (2026-10-22); every aircraft is trimmed at every
loading at 1,000, 5,000 and 10,000 ft but where her book says she cannot fly
(2026-10-23; the Learjet 35A's own loading inside her limits since
2026-10-24), and a trim that fails is said in the server's log. Every
aircraft's own loading - what a player given none flies - is checked against
the weight and centre-of-gravity envelope her books publish: five inside,
the J-3 Cub forward of hers (a tail), ten with none public, each named. The server deploys as a systemd unit or a container image. Still
missing from multiplayer:
- choosing an aeroplane: a player flies what the server's plan flies;
- the lobby, the session, the weather and the terrain dataset: they are
  defined and do not yet travel;
- any limit on how often a client sends.
Each is a tail in `COMPLETION_PLAN.md`.

**Phase 7, the user/AI controller swap across the network, is complete - 8
of 8 items** (2026-09-25). A player hands their aircraft to the AI and takes
it back, rides along in any AI aircraft with its controls shown, and takes
one over, never another player's; both clients do it, and the network checks
hold the swaps within their bounds at 100 and 200 ms with jitter and loss.
What the client with the window shows of its own aircraft does not step at
a switch, at 20 fps and above. Left in tails: the window client's bounds
not yet seen passing on CI's windows-release. (A pressed during a take-over
waits for the server's answer since 2026-10-07.)

**Phase 8, the LLM copilot, is complete - 6 of 6 items** (2026-10-02).
Claude and ChatGPT turn words into flight plans the autopilot flies; the
copilot flies with a player, asked on the player's machine with the
player's key, and the server flies only the route it is sent, checked; each
AI aircraft, and each aircraft handed to the AI, is planned by the model
chosen for it, or none; a build check keeps the model from any control
surface; and a landing learnt by reinforcement learning lands the Cessna
172P on the centreline in a crosswind. The learnt landing is offered for one
aircraft, by the CLI, to a player at its gate on a server (L in the window
client, told why when refused), and to an AI aircraft the server is told to
flying a plan that ends in a landing, which then taxis off the runway; a
landing goes around from a runway not clear (since 2026-10-08) - but a
copilot's route cannot end in one (a tail). Everything a model answered is recorded,
and CI flies it again with no key.

## Gaps

Everything in `COMPLETION_PLAN.md`. The ones worth naming first, because they
are the risks the phase order is built around:

- **Restore settles what it cannot read.** It holds the aircraft at the captured
  state for two simulated seconds so JSBSim's hidden engine and actuator states
  converge; a restore is therefore not free. Reconciliation does not use it:
  it sets the aircraft's motion alone (`Aircraft::set_motion`), and the
  client's own engines and actuators, flown on the same inputs, stay as they
  are.
- **Sixteen aircraft are checked, and the handling of those written here is
  estimated.** The Cessna 172P and 182S, the Piper PA-28-180 and the Piper
  J-3 Cub fly to their handbooks, the Airbus A320 and A380 and Boeing
  737-300, 747-400 and 787-8 to their airport-planning documents and type
  certificates - four figures each, none of them a landing - the Learjet 35A
  to its flight manual, the F-15C and F-22A to the Air Force's and the
  Department of Defense's figures, none of them below 30,000 ft but the
  F-15C's climbs, the F-35B and B-2A to the little published of them - the
  B-2A's speed, ceiling and range, and the F-35B's speed and range, for
  Lockheed Martin publishes no ceiling for any F-35 - the Mosquito FB Mk VI
  to its trials and Pilot's
  Notes, and the Short S.23 to Flight's figures: its take-offs from water,
  its speed and climb, and a draught measured off its drawing. The Cub's
  handbook is the thinnest: five figures, none with an altitude, one with a
  weight. The Mosquito's stability derivatives and inertias are estimates
  from its geometry - none were found measured - and its only take-off
  figure is the B Mk IV's. The A380's are the Boeing 747's, the nearest
  aircraft of its kind whose derivatives are published. A player's F-35B
  joins at a loading she asks for and converts, hovers and lands vertically
  through a server (2026-10-16), and the windowed client flies her at it
  (2026-10-22); her converting, jet-borne and hover laws are each side's own
  and agree, both changing stage on the same step, her predicted pitch within
  half a degree of the server's selected at any moment (2026-10-22); joining
  already selecting STOVL, her law is seeded in her prediction as the
  server's is (2026-10-21). A client joining while her law already flies
  her predicts her from a law made afresh, her fan's doors shut. She flies
  30 kt backward in the hover (2026-10-20), her lift and pitching moment
  the same either side of 180 degrees; her lift past 90 degrees either way
  is lopsided and unsourced, chosen so that flight inside 90 degrees is
  untouched. Jet-borne her stick rolls her and, centred, holds her bank, as
  the VAAC Harrier's unified law does, and her pedals turn her heading
  (2026-10-21); the laws' figures are this model's - nothing published
  gives hers.
- **Terrain is drawn around where the flight starts, and nowhere else.** The
  client draws the nine whole-degree cells around its start; fly out of them
  and there is sky below.
- **The weather seen is a sketch of it.** Cloud decks are flat sheets, not
  volumes, over a disc 60 km across the station, and do not drift with the
  wind; cumulonimbus is a deck 6 km deep, not a tower. A new report makes the
  sky again, so its cloud jumps rather than blends. The haze is one colour,
  lit by nothing. Rain and snow fall only within 20 m of the eye. All of it is
  a tail in `COMPLETION_PLAN.md`, "weather seen as it is".
- **Thermals do not know the ground.** They rise as strongly over the sea as
  over a sunlit field, and the terrain's lift is linear theory seen along the
  wind only: no rotor, and no lee waves trapped under a stable layer. A tail
  in `COMPLETION_PLAN.md`.
- **Weather is one station's.** A flight flies in the weather of the airfield
  it names, everywhere it goes; nothing picks the nearest station - a tail in
  `COMPLETION_PLAN.md`. On a server the server's weather travels and every
  client flies it - one joining mid-blend blended from the same weather - but
  over no ground: no ridge or wave lift there, a tail.
- **Summits are low in the DEM.** A 30 m grid does not hold a peak: at five
  surveyed summits the DEM is 8 to 35 m below the survey. Runway ends and
  coastlines are within the dataset's stated 4 m.
- **A landplane on water ditches, and nothing more.** It is stopped where it
  meets the water and held there; nothing of a ditching - the airframe
  striking the water, floating, sinking - is modelled.
- **The aeroplane is drawn, with no livery and nothing on it moving.** A
  model carries no texture, so each surface is the flat diffuse colour of its
  material, and no control surface, propeller or undercarriage moves: they
  are welded where the model has them, gear down. Its light is baked into the
  mesh, which is made again when it has banked five degrees. The Learjet
  35A has no model at all and is drawn as nothing. All of that is tails in
  `COMPLETION_PLAN.md`. Where a model and its
  flight model disagree about the aeroplane, the disagreement is measured
  and each aircraft held to its own figure rather than made to vanish: the
  747-400's is the worst at 2.48 m, because JSBSim's has one main leg a side
  where the aeroplane has two. The Learjet 35A has no model, because
  FlightGear has no Learjet of any mark (a tail). See the log.
- **Only runways OurAirports places are flat.** The collision ground under a
  runway is its own straight line between its ends' elevations, for the
  14,814 runways whose ends the pinned file places; the rest, mostly small
  strips with no position, are the DEM's, bumps and all. Where two
  runways' pavements overlap the ground is the mean of their lines, and
  each is pulled off its own by up to 0.61 m. See the log, 2026-10-01 and
  2026-10-06.
- **The DEM is not thread-safe.** One `world::Dem` caches tiles and blocks as it
  goes; whoever shares one between threads must lock it.

---

## The log

The log is `docs/status/`: one file per item, named by its date and the first
words of its title. `ls -r docs/status` lists it newest first. An item adds
its own file, so items do not conflict on one shared file; there is no index
to keep. The plan's detail as it stood on 2026-09-23 is
`docs/status/2026-09-23-detail-from-completion-plan.md`.

---

## Known risks

The four risks the phases were ordered around are all answered (re-read
2026-10-02, at the end of Phase 8):

- **JSBSim state set/resume** - answered in Phase 1, 2026-09-17: a restored
  instance tracks the original in every phase of flight ("State capture and
  set/resume"), and reconciliation sets the motion alone (Gaps, above).
- **The Cesium-to-SDL_GPU glue** - answered in Phase 2, 2026-09-18: Cesium
  Native draws the open-data terrain and imagery through SDL_GPU on every
  backend.
- **Reading the Copernicus DEM directly** - answered in Phase 2, 2026-09-18:
  the GeoTIFF reader is written here and held to surveyed runway ends and
  coastlines.
- **Runway surfaces** - decided 2026-10-01 (`REQUIREMENTS.md` section 9) and
  built: the collision ground under every runway OurAirports places is the
  runway's own line. What is left is in the plan's tails: runways that meet
  pulled off their lines, and Sydney's 16R not flat enough for every
  aeroplane.
