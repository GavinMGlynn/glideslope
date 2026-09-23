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

## The honest summary, 2026-09-18

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
is no aircraft model, cockpit or server, and one aircraft of the sixteen the
roster now names. The weather is real: METARs and winds aloft, fetched live,
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
terrain beyond the cells around the start, runways and buildings.

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
proved. **Phase 5c, learning to fly, is new and not started:
0 of 4 items.** Added
2026-09-18, as were the sixteen-aircraft roster of Phase 5 and a tail for
terrain over the whole Earth; see the log. On 2026-09-19 Phase 8 gained an
autopilot that flies an approach and lands, and a copilot that flies with
you - changing the autopilot's modes and the plan as the flight goes, never a
control surface - and the drawn weather's gaps became a tail.

## Gaps

Everything in `COMPLETION_PLAN.md`. The ones worth naming first, because they
are the risks the phase order is built around:

- **Restore settles what it cannot read.** It holds the aircraft at the captured
  state for two simulated seconds so JSBSim's hidden engine and actuator states
  converge; a restore is therefore not free, and whether that cost suits
  reconciliation many times a second is a question for Phase 6.
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
  aircraft of its kind whose derivatives are published.
- **Terrain is drawn around where the flight starts, and nowhere else.** The
  client draws the nine whole-degree cells around its start; fly out of them
  and there is sky below.
- **The HUD's horizon line is not the horizon.** It moves a hundredth of the
  frame's height a degree of pitch, which was a choice when there was nothing
  behind it; now the terrain is drawn, the two do not line up.
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
  it names, everywhere it goes; nothing picks the nearest station.
- **Summits are low in the DEM.** A 30 m grid does not hold a peak: at five
  surveyed summits the DEM is 8 to 35 m below the survey. Runway ends and
  coastlines are within the dataset's stated 4 m.
- **A landplane on water ditches, and nothing more.** It is stopped where it
  meets the water and held there; nothing of a ditching - the airframe
  striking the water, floating, sinking - is modelled. And seven of the
  models have no structure contact points: with their wheels up they pass
  through a runway, which a tail in `COMPLETION_PLAN.md` puts right.
- **The aeroplane is drawn, with no livery and nothing on it moving.** A
  model carries no texture, so each surface is the flat diffuse colour of its
  material, and no control surface, propeller or undercarriage moves: they
  are welded where the model has them, gear down. Its light is baked into the
  mesh, which is made again when it has banked five degrees. Two aircraft
  have no model at all and are drawn as nothing. All of that is tails in
  `COMPLETION_PLAN.md`. Where a model and its
  flight model disagree about the aeroplane, the disagreement is measured
  and each aircraft held to its own figure rather than made to vanish: the
  747-400's is the worst at 2.48 m, because JSBSim's has one main leg a side
  where the aeroplane has two. The Learjet 35A has no model, because
  FlightGear has no Learjet of any mark. See the log.
- **The DEM is not thread-safe.** One `world::Dem` caches tiles and blocks as it
  goes; whoever shares one between threads must lock it.

---

## Log, newest first

### CI split into builds and test shards, and a handshake that overflowed, 2026-09-23

**CI had been red since 2026-09-21**, through two sessions of pushing, and the
causes were stacked: each hid the next. libsodium's port needed autotools no
runner had; a bare `std::getenv` failed MSVC; the clang-warnings test fed
`cl`'s flags to `clang++`; two tests shared a Cesium cache; a timing threshold
tested the runner's speed; the F-15C's airframe contacts were wheels with
tyres; the belly landing idled its engines; and MSVC's debug runtime answered
every failed assert with a dialog box nobody on a runner will click, so four
tests hung until the job's limit cancelled it.

**Once asserts reached the log, the hang was a real bug**: the handshake copied
its 33-byte protocol name into a 32-byte hash. The last byte landed in the next
member and was overwritten at once, so nothing ever misbehaved and
AddressSanitizer - which guards objects, not the joins between their members -
could not see it. It copies only what fits now, which is byte for byte what the
hash held before. The larger fault underneath is a tail: Noise's BLAKE2b has a
64-byte hash, this uses one cut to 32, so the protocol is not quite the Noise
variant it is named after (`TRANSPORT.md` now says so).

**The workflow is rebuilt for time.** The Ubuntu debug "build, test" step took
53 minutes and Windows debug 90. Now every configuration builds in its own job;
ccache keeps objects between runs on Linux and macOS; a build packs what its
tests need (422 MB for Linux debug, compressed) and uploads it once; and its
tests run in shards, `ctest -I k,,n`, each a job on its own runner. A shard
cannot finish sooner than its longest test, and simulated on measured timings
sharding stopped improving at 9.9 minutes whatever the number of shards,
because one test took 592 s. **That test - every view shot in one - is seven
tests and a comparison now**, the views as the comparison's ctest fixture, so a
shard that runs the comparison runs the seven with it. Run alone the eight took
207 s. Every test also has a 900 s default timeout, so no hang can hold a job.

**Splitting the views reopened the shared cache**: seven view tests running at
once shared one Cesium cache until the view went into its name. On the first
warm run macOS builds took 30 s and Rocky 52 s, but Ubuntu took 6 to 9
minutes: GCC 14 has CMake scan every file for C++ modules, and ccache will not
cache a scanned compile, so 80% of calls missed. Ubuntu configures with the
scan off; glideslope has no modules. A macOS runner also lost DNS for
api.open-meteo.com for longer than a fetch's three tries spanned (6 s),
failing the weather and HUD tests; fetches now try five times, over 30 s.

**The next run found a real bug in a dependency.** When an imagery tile times
out, Cesium Native fills its place with the parent tile's pixels scaled up,
through stb's resizer - and the stb that vcpkg's pinned checkout carries
(stb_image_resize2 v2.10, 2024-07-29) writes a float past its decode buffer
when scaling three-channel pixels, and reads a field through a pointer it has
just freed. The sanitized macOS client stopped there in three tests. stb
fixed both in v2.11 and v2.12; `cmake/ports/stb` is vcpkg's own port at stb's
2026-08-02 commit (v2.18), pinned by SHA-512, as an overlay.
`a_parent_imagery_tile_scaled_into_a_missing_childs_place_is_sound` makes
that blit: under v2.10 AddressSanitizer stopped it with a heap overflow (seen);
under v2.18 it passes.

**The view tests shot imagery that streamed.** Each view is two launches,
with the aeroplane and without, and the outline is where the two frames
differ. A tile slow in one launch and not the other made them differ in the
terrain too: the cockpit's two shots differed with no aeroplane in either, and
the ahead, behind and orbit outlines took in terrain. They are shot with
`--imagery off` now, so the frames rest on the pinned DEM alone; the imagery
keeps its own tests. And the state-rate floor was 50 of 75 updates in three
seconds, where a debug server on a busy Windows runner sent 32: it is 10,
which still fails once a second, the thing it is there for.
The input-frame floor in the flying test was the same: 100 of 180, where the
same runner's debug client sent 97. It is 20; once a second would be six.
Its count of inputs still in flight at the end, which it allowed ten, was
seventeen on a loaded Ubuntu debug runner; it allows a second's worth, thirty.

**One shard took 19 minutes** because the test holding all seven views'
flights to one another had the seven as its ctest fixture, so whichever shard
ran it ran all seven, each two sanitized client launches. That test is gone:
the cockpit view is the fixture, and each outside view holds its own flight to
the cockpit's (seen to fail with the cockpit's account altered). A shard
running any view now pulls in one other test, not six.

**A Windows crash now prints its stack.** The virtual-joystick test
segfaulted once on Windows debug with nothing in the log but "SegFault"; it
passed the run before, and 200 runs under AddressSanitizer on Linux. Nothing
in SDL's virtual driver explains it on reading, so it is not fixed.
`no_crash_dialogs()` installs an unhandled-exception filter that prints the
exception and a symbolized stack, and the Windows packs keep each program's
PDB, so its next occurrence says where.

### Figures measured from the models, where nothing is published, 2026-09-23

**What is still missing: the airliners, the fighters, the bomber and the
flying boat are taught climbs and descents and turns, and nothing else yet.**
Their take-off, approach, stall and circuit lessons are not written.

**The project owner decided on 2026-09-23 that a lesson's reference speed may
be measured from the flight model** where no handbook publishes one. Ten of
the sixteen aeroplanes have no public manual, so they could be taught only
turns - the one exercise that names no speed. Sixteen figures now carry
`from="measured"`, which is a new attribute on a figure and a different claim
from a published one: **a published figure holds the model to the world, a
measured figure holds the model to itself.** `figures.hpp` says so and
`docs/ASSETS.md` records every one.

- Stall speeds for eight: 737-300 105.72, 787-8 113.71, A320 113.33, A380
  104.86, B-2A 95.42, F-15C 151.06, F-35B 122.24, Short S.23 66.32 KCAS.
- Rates of climb and the speeds they are flown at, for eight: 737-300 4,438
  ft/min at 260 kt, 787-8 5,457 at 300, A320 5,804 at 300, A380 4,773 at 270,
  B-2A 8,271 at 230, F-15C 24,569 at 200, F-35B 20,390 at 200, Learjet 35A
  8,097 at 240.

**A measurement that moves with how you approach it is not a measurement.**
Each stall speed was taken from three entry speeds a good margin above the
stall, and the eight vary by at most 1.2 knots between them. **The 747-400 and
the F-22A do not**: the same aeroplane came out anywhere between 146 and 160
knots, and between 128 and 149, depending only on where the deceleration
started. They get no stall speed, and no climbing speed either - the climbing
speed is floored at 1.3 times the stall speed, so without one there is nothing
to anchor it to, and the 747-400's unfloored answer lands at 360 knots, within
five of its maximum operating speed. **An aeroplane's reference speeds hang
together, and those two have none of them.** They are taught turns alone.

**Making the deceleration stop only on a sustained rise was tried and
reverted.** It changed nothing at all for the eight that already held still -
the same numbers to a hundredth of a knot - and widened the 747-400's spread
to 21.8 knots and the F-22A's to 84.3, one run mushing down to 57. What those
two do near the stall is the models, not a late break in the measurement.

**A reference speed belongs to a weight, and the lesson now flies the
aeroplane at the weight its figures were measured at.** Without that the two
are about different aeroplanes: the F-35B's speeds were measured at its 39,750
lb combat loading and the lesson flew it at the 45,259 lb the model loads by
default. `sim::Aircraft::load` was already there; nothing was calling it
outside the figure flights.

**The climbing speed is the lowest speed that still climbs within ninety-five
per cent of the best rate**, never below 1.3 times the stall, **and never
below a speed the aeroplane can actually be climbed at**. That last condition
is not the same as the first two and it caught the F-35B: the sweep measures
at full throttle, where a fighter climbs from almost any speed, but a lesson
asks the autopilot for a speed *and* a rate, and near the stall it runs out of
nose before it runs out of thrust. At its 160-knot floor the F-35B pinned the
nose at the fifteen degrees it is allowed, sat at nineteen degrees of alpha
and sank at 2,000 ft/min, arriving on the runway still doing 160 knots. 180
still pins the nose and manages 112 ft/min; 190 climbs; 200 is taken, where
the nose sits at 13.6 degrees and there is room. The peak itself
is no use for teaching: a jet's rate goes on rising to speeds it is never
operated at - the A320's peaks at 400 knots - and a fighter's is flat within a
few per cent over hundreds of knots, so for the F-15C and the F-35B the floor
is what chose the speed. The answers land where the real aeroplanes climb:
260 knots for a 737, 300 for an A320, 240 for a Learjet.

**A lesson belongs to a class; the figures belong to the aeroplane.** The two
do not always meet, and until now nothing noticed: `sim::cannot_be_taught`
reads the figures a lesson names and says which one an aeroplane has not got,
and the tests leave that aeroplane out **by name, with its reason**, rather
than flying it with a reference speed of zero. That had been happening - a
lesson naming a figure the aeroplane lacked resolved it to zero and the stage
ended on the first tick, which is a test that passes without testing anything.

Climbs and descents are now taught to the airliner, fighter, bomber, seaplane
and business-jet classes as well as the light aircraft and the Mosquito -
fourteen aeroplanes fly the lesson to an empty debrief, and the two that
cannot are named in the test's own output with the figure they lack.

**Two more things the weight turned up.** A demonstration used to ask every
aeroplane for 600 ft/min; a fully loaded Piper J-3 Cub climbs at 450, and it
had only ever managed 600 because it was being flown lighter than its own
figures. Demonstrations ask for seven tenths of the rate the aeroplane has
now, capped at the 600 the jets were already being given. And **the PA-28
loses 321 feet entering a stall where the 172 loses 61**, at the same 2,400
lb, flown by the same autopilot - so the light-aircraft stalls lesson carries
a band four hundred feet wide on one aeroplane's account. The lesson file says
so rather than passing it off as a tolerance, and it is a tail.

**Two tests named the B-2A as the aeroplane that publishes nothing**, which it
no longer is. They use the 747-400 now, whose stall speed is the one that
would not hold still.

**A stalls lesson for the airliners, fighters and bomber was written and
withdrawn the same day.** A lesson's `stall` is the landing-configuration
stall speed, and the exercise is flown clean. For a light aeroplane those are
close enough that slowing to six knots above the landing stall works; for a
737-300 the landing stall is 105.7 knots and clean it gives up at about 155,
so an entry ending twenty-five knots above the landing stall asks for a speed
it cannot reach without stalling first. Traced from 20,000 ft it holds its
height down to 159 knots at 12.6 degrees of alpha, and thirty seconds later it
is at 52 degrees of alpha, 26 degrees nose down and falling at 21,000 ft/min,
and nothing recovers it. **A swept wing past the stall departs; it does not
mush like a Cessna.** The exercise has to be flown in the configuration its
reference speed is measured in, which no lesson handles yet, so it is a tail
rather than a lesson with wider bands.

**Still at the model's default weight: the take-off lesson, the approach
lesson, their demonstrations and the circuit**, which each build their
aeroplane directly rather than through `airborne`. They pass, which means the
difference is small for those aeroplanes and not that it is absent. A tail.

### The circuit, and a bounce that switched the steering off, 2026-09-23

**What is still missing from the lessons item: take-offs, approaches, climbs,
stalls and circuits for the ten aeroplanes that publish no stall speed and no
rate of climb** - the airliners, the fighters, the bomber and the flying boat.
Those wait on figures, not on lesson-writing.

**The circuit is the sixth lesson, and the first that is a whole flight.**
Eight stages - the take-off, the climb out, the crosswind turn, the turn on to
downwind, the downwind leg, base and the turn on to final, final, and the
landing - flown off a runway and back on to the same one. The take-off
autopilot flies her off, the plain autopilot flies the pattern a heading and a
height at a time, and the approach autopilot brings her back. All four light
aircraft and the Mosquito fly it to an empty debrief and stop on the runway.

`lesson/flown-nm` is the runner's second own property: how far over the ground
she has flown since the stage began. The downwind leg is level and straight,
so height, speed and heading read the same at both ends of it and nothing
about the aeroplane's state says how far along it is.

**A faster aeroplane flies a bigger circuit, and the two numbers that make it
are one number.** The downwind leg is left where a three-degree glidepath
passes through circuit height, so the approach autopilot is handed an
aeroplane on its path. A Mosquito flown round the Cessna's thousand-foot
circuit and given the approach at the Cessna's distance arrived low, still
turning, and put itself into the ground.

**The take-off autopilot latched "unstuck" on a bounce and never unlatched.**
The Mosquito skips once at about 97 knots; `unstuck_` went true, the departure
handed her to its airborne law - bank to hold a heading - and she came back
down and rolled on at 115 knots with nothing steering the wheels. Fourteen
seconds later she was forty-five degrees off the centreline with the rudder
sitting at a tenth of its travel, because the airborne law had only a little
sideslip to answer. **On the ground is the roll, whatever the stage said** -
the mirror of the rollout rule the landing autopilot already had, and for the
same reason: the wheels decide. `unstuck_` itself is untouched, because where
she first came off is where the ground roll ends and the published take-off
distances are measured from it - and all of those tests still pass unchanged.

**No take-off lesson watched the heading**, which is why a forty-five degree
swing had never been caught: they watched the throttle, the rotation speed,
the attitude and the climbing speed. Both take-off lessons now hold
`lesson/turned-deg` to twenty degrees through the roll, measured against the
1.1 to 11.1 degrees the five aeroplanes taught a take-off actually swing.

**Two structural faults in the lesson data, both found by flying it.** A stage
that ends on a turn begins part-way through the one before: the turn on to
downwind ends its stage eighty degrees in and the aeroplane is still turning,
so the last twenty degrees fall inside the downwind leg. Ending that leg at
twenty degrees of turn gave the Cub a four-second downwind leg, and the
Cessna passed by a single degree - which is luck, not a test. It ends at fifty
now. And the downwind leg used to end after a mile and a half, which is not
where the leg ends, so three miles of straight and level flying fell inside a
stage called "base, and the turn on to final" - and a sink along the downwind
leg was judged by a stage that watches no height, and went unremarked.

**Landing is asked of the wheels, not of the altimeter.** `position/h-agl-ft`
is measured to the aeroplane's reference point, and a Mosquito parked on the
runway reads seven feet, so a lesson asking for five could never be satisfied
by one however well it was landed. The lesson asks `gear/wow` now.

- `the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief` - five
  aeroplanes, eight stages each, empty debriefs, stopped on the runway.
- `a_circuit_flown_low_downwind_is_named_in_the_debrief` - let sink 250 ft
  along the leg, the debrief holds one thing and it is the circuit height.

**Seen to fail.** With the bounce latch put back, the take-off lesson reports
`mosquito-fb6: [The roll] Keep her straight on the take-off roll` and three
tests go red.

The selftest hash does not move - it replays a pilot input log and never
engages the departure autopilot.

### The instructor demonstrates every exercise, 2026-09-23 — item done

All five exercises are demonstrated by the AI pilot through a
`sim::Controller`, and **every aeroplane taught an exercise flies it**, not a
sample of them: turns (16 aeroplanes), the take-off (5), the approach (6),
the climb and descent (5) and the stall (6). Each flies every stage of its
lesson to an empty debrief, hands the controls to a pilot whose hands are
nowhere near where the AI had them, and takes them back three seconds later.
All seventeen controls are measured at both swaps: **0.0083 handing over,
0.0000 taking back**, where a pilot's hand moves 0.0207 in a frame.

**What kept the item open was that two exercises were not flown by a
controller at all.** The climb and the stall drove an `Autopilot` directly and
reached into the controls it returned - the stall closed the throttle by hand
on the way in and opened it wide on the way out. There is nothing to hand over
in that: the throttle was flying the aeroplane, not the AI.

**The instructor now asks for a speed, not for a throttle.** Asking the
autopilot to hold a speed below the stall closes the throttle for it and holds
the height by raising the nose, which is the entry; asking for a speed above
the climbing speed opens it and puts the nose down, which is the recovery.
The same instruction goes to every aeroplane and each flies it with its own
controls.

**The speed asked for has to clear the one the lesson waits on.** The first
version asked for half as much again as the stall speed, which is what the old
hand-flown recovery aimed at. In a Cessna that is 72 knots and the recovery
stage ends at the climbing speed, 76: she settled just underneath it and the
stage never ended. The old code only got past it because the throttle was held
wide open by hand. The recovery now asks for the climbing speed plus ten
knots, whichever is the higher.

`demonstrate_in_the_air` is shared by the climb and the stall: they differ
only in what the instructor asks for, so the aeroplane, the lesson watching,
the two swaps and what is measured at them are written once.

- `an_instructor_demonstrates_a_climb_and_descent_and_hands_it_over`
- `an_instructor_demonstrates_a_stall_and_hands_it_over`
- `an_instructor_demonstrates_a_take_off_and_hands_it_over`,
  `an_instructor_demonstrates_an_approach_and_hands_it_over` and
  `an_instructor_hands_over_and_takes_back_with_no_step_in_any_control` each
  walked a hand-written list of four aeroplanes and now walk every aeroplane
  taught the exercise, and state how many that is.

**Seen to fail**: the stall demonstration reported `c172p flew every stage of
the stall: 1 of 2` until the recovery speed was raised.

One unrelated repair: `src/frontend/cli/main.cpp:580` promoted a `float` to a
`double` implicitly, which clang rejects and GCC does not, so
`every_first_party_source_compiles_clean_under_clang_too` was red.

### The approach demonstrated, and a handover that broke on its own limit, 2026-09-23

**What is still missing: the climb and the stall have no demonstration an
instructor can hand over.** Both are flown by driving an autopilot directly
rather than through a `Controller`, so there is nothing to hand over from.
The turns, the take-off and the approach are demonstrated.

**The approach.** Four aeroplanes - the Cessna, the PA-28, the Learjet and
the Mosquito - start two miles out on a three-degree glidepath, and
`Controller::to_ai_approach` flies all three stages of the approach lesson to
an empty debrief. The controls then go to a pilot whose hands are nowhere
near where the AI had them, and come back. Handing over 0.0083; taking back
**0.0000**, exactly.

**It took two defects to get to that 0.0000, and the second was the
interesting one.** Taking the controls back stepped the Learjet's elevator by
0.685 of its travel in one frame, and the Mosquito's rudder by its *entire*
travel - 1.000, in one frame, where a hand moves 0.0083.

Three of the autopilot's loops seeded an integral with a term that cancelled
the law's own damping, so that the first step after engaging reproduced the
controls it had been handed:

```
rudder_integral_ = controls.rudder + rudder_per_degree * beta;
...
c.rudder = -rudder_per_degree * beta + rudder_integral_;   // cancels, in theory
```

**The cancellation breaks whenever the damping term is larger than the
integral's own limit.** The integral is clamped to a control's travel. The
Mosquito, skidding in its landing roll, had a sideslip of about ninety
degrees: the seed wanted to be 9, kept 1 of it, and what came out was
`-9 + 1` clamped to the stop. The same arithmetic stepped the Learjet's
elevator off a large pitch rate.

**The loops no longer derive the cancellation; they measure it.** The first
call to `fly()` computes what the laws actually give, takes the difference
from the controls it was handed as an offset, and fades that offset out over
two seconds - the treatment the aileron channel already had. There is no term
to get wrong and no limit for it to fall foul of, and the offset is exact by
construction. On top of that **no loop may move a control faster than a
pilot's hand at any time**: 1/120 of its travel in a step. That bound was
being broken by 2.0 - one stop to the other - by the rudder law following
sideslip while recovering from a spiral.

**The test that should have caught this flew one Cessna, in one gentle
climbing turn, and passed.** It now walks **every aeroplane the data holds
through every state one can be handed over in**: trimmed and level, a
climbing turn, gliding, skidding on full rudder, rolling on full aileron,
bunted hard nose down, pulled into a stall, and a spiral on crossed controls.
16 aeroplanes x 8 states = 128 handovers, and it states those numbers and
fails if the walk does not cover them.

- `the_autopilot_never_moves_a_control_faster_than_a_pilots_hand` - 128
  handovers; worst first step **0.0000**, worst step in the five seconds
  after **0.0083**, which is the bar itself.
- `an_instructor_demonstrates_an_approach_and_hands_it_over` - four
  aeroplanes, three stages each, empty debriefs.

**Seen to fail.** The old autopilot was put back and the walk reports first
steps of 0.84, 1.70 and **1.80** of travel across the fleet - and 0.110 for
the 737-300 in the *gentle* climbing turn that was the only case the original
test ever flew.

**A flight model is only asked what it can answer.** JSBSim asserts on a table
lookup outside its range, and a 1930s flying boat put into a spiral at 20,000
ft left its own aerodynamic tables and aborted the run. The fly-in now stops
at the edge of what a model covers and the autopilot is handed the most
extreme state that model can actually produce. Stopping at the same bound the
measurement needs left 19 of the 128 handovers with nothing measured at all,
and the test said so rather than passing on an empty walk; the fly-in stops
inside it instead.

The selftest hash does not move: the selftest replays a pilot input log and
never engages the autopilot.

### The AI pilot can take off, 2026-09-23

**What is still missing: the approach, the climb and the stall have no
demonstration.** The machinery is there for the approach - the AI pilot can be
handed a runway to land on - and the test is not written. The climb and the
stall are demonstrated by driving an autopilot directly rather than through a
`Controller`, which is not a thing an instructor can hand over.

**`sim::Controller`'s AI could only be handed an aeroplane already flying.**
Its AI was the autopilot and the navigator; a take-off and an approach were
flown by `Departure` and `Lander` from a frontend, outside any controller. So
a take-off had no demonstration to hand over *from* - which is why the
instructor item could only cover turns.

`to_ai_take_off` and `to_ai_approach` hand the AI a runway. When the take-off
or the landing is over the plain autopilot engages **from the controls it
left**, so the aeroplane does not lurch at the moment the AI stops taking off
and starts flying.

**Four aeroplanes demonstrate a whole take-off** - the Cessna, the PA-28, the
Cub and the Mosquito - all three stages of the lesson with an empty debrief,
then the controls go over at 0.0083 and come back at 0.0018 to 0.0020, where
a pilot's hand moves 0.0207 in a frame.

### The instructor, and a jolt that was there all along, 2026-09-22

**What is still missing: the instructor can demonstrate one lesson.** Turns.
`sim::Controller`'s AI is the autopilot and the navigator; it does not wrap
`Departure` or `Lander`, so a take-off, an approach and a stall have no
demonstration to hand over from.

**What works, for four classes.** The Cessna, the Learjet, the Mosquito and
the A320 each fly all three stages of their turns lesson with an empty
debrief, then the controls go to a pilot whose hands are nowhere near where
the AI had them, and come back three seconds later. All seventeen controls are
measured at both swaps: **0.0083 handing over, 0.0017 to 0.0019 taking back**,
against the 0.0207 a pilot's hand moves in a frame.

**The verification found a defect that had been there since the autopilot was
written.** Taking the controls back stepped the elevator by up to **0.80 of
its travel in a single frame** - forty times a hand's pace - and the
`Controller`'s own header promises that handing over steps nothing.

The cause: the autopilot seeded its pitch command to the attitude the
aeroplane had, and then `fly()` clamped that command to the envelope it is
allowed to ask for. Handed an aeroplane at 33 degrees nose up when the limit
is 15, the command snapped to 15 in one frame and the elevator jumped by the
difference. **The envelope now bounds the target rather than the command**, so
the command walks to it at the loop's own pitch rate - which is the autopilot
taking over rather than grabbing.

**Three wrong measurements before the right one**, each worth naming because
each looked like a defect and was not. Commanding a ninety-degree turn in the
same frame as the swap measured the command, not the handover, and read 1.29.
Taking the worst step over the second after the swap measured the autopilot
flying and called it a jolt. And a "gentle" pilot input held for fifteen
seconds rolled the aeroplane into a spiral - pitch -26, bank -62 - so what was
being measured was an autopilot recovering from a dive. A swap is one frame,
and that is what "no step" means.

**And the fix moved a figure in a lesson.** The 172 left late in a stall now
loses less height, because the entry is flown more gently, so
`light-aircraft-stalls.lesson` records thirty-five seconds where it recorded
twenty-five, and says why.

### The client with the window can join a server, 2026-09-22

**What is still missing: it joins and then flies alone.** The session is kept
- the server's knocking is answered, its state updates are read - and nothing
is drawn from them. The aircraft on screen are still only this client's own.
That is the item below this one.

**The session lives in `src/net/` now.** It was written inside
`glideslope_cli`, which is why the client with the window could not connect to
anything at all: the code existed and was in the wrong place. `net::ClientSession`
owns its socket, does the handshake with retransmission, answers the pings
and reads the state updates, and both frontends use the one implementation.

**Both clients reach the server `server.txt` names**, and there is a test for
each. The command-line one needs no GPU and runs everywhere; the one with the
window draws, so it skips where there is no driver. Both matter - for most of
this project's life `--online` worked only in the client nobody flies.

**A day and a half of hung processes.** Five `glideslope --terrain ion` runs
were still alive from a previous session, each inside a `timeout -s TERM` of
four to eight minutes that had fired long ago. They hold the GPU and the
Cesium cache, and they are what the "database is locked" flake is: a run
meeting a process from another day. The TERM is sent and ignored. Recorded as
a tail with its own verification.

### A line you can send somebody, 2026-09-22

**What is missing first: the client with the window still cannot connect to
anything.** `--online` is in `glideslope_cli` only, so a person who flies
rather than types cannot use it. That is the whole gap in the item now.

**`server.txt` is one line: a host, a port and the server's public key.** That
is everything a client needs and nothing that is a secret - the key is the
half a server prints at startup precisely so it can be handed out, so unlike a
Cesium token the file may be e-mailed, posted, or committed to somebody
else's repository. `platform::default_server()` reads it from the config
directory, or from wherever `GLIDESLOPE_SERVER_TXT` names.

**Both forms `REQUIREMENTS.md` 6.6 names work**: `connect --online` and
`connect --server HOST PORT --server-key HEX`.

**Thirteen ways a line can fail to be one, walked and counted.** A port of
nought is refused because it means "any free port" to a server and is
meaningless to a client; a port past 65535; a key a digit short, a digit long,
or not hexadecimal; a fourth word. Comments and blank lines are skipped so the
file can say where it came from, an upper-case key is read because somebody
will paste one, and a line that is not one is skipped rather than fatal so a
file can keep old servers below the current one.

**The test names the file by the environment.** A test that dropped a
`server.txt` into the person's own `~/.config/glideslope` would be editing
their machine to make itself pass.

### A server you can deploy, 2026-09-22

**What is missing first: the Dockerfile has never been built.** It is written,
and Docker Desktop's WSL integration is off on this machine, so no daemon is
reachable and half the deployment item's verification is unproven. That is
said in the item rather than left to be discovered.

**The systemd half is proven.** The unit that ships is run by `systemd-run`
with only its paths pointed at the build, and a client connects to it: "a
server started by systemd admitted f21c7f56 to slot 0". A ctest does it, and
reports itself skipped where there is no systemd.

**The server runs with no privilege.** `ProtectSystem=strict`, `ProtectHome`,
`PrivateDevices`, `NoNewPrivileges`, `MemoryDenyWriteExecute`,
`RestrictAddressFamilies=AF_INET AF_INET6`, and `SystemCallFilter` cut to
`@system-service` less `@privileged` and `@resources`. It binds a high UDP
port, reads its own data and writes one SQLite file, and `StateDirectory=` is
what gives that file somewhere to live - a server that lost its key on every
restart would lock out every client it had.

**`-DGLIDESLOPE_SERVER_ONLY=ON`.** The server links no renderer, so a
deployment image should not build one. The option leaves out SDL, Cesium
Native, the shader compiler, the renderer, the model loader and the client,
and does not use vcpkg at all: libsodium, SQLite and libcurl come from the
system. It **configures in 6.2 seconds where the full build takes seven
minutes**, and builds the server in about thirty.

**And building Release found two defects the debug preset never sees.**

`src/platform/socket.cpp` writes sixteen bytes into a caller's address through
a raw pointer, and the bound that makes it safe was carried by a ternary
several lines above - a gap allows at most seven groups and no gap means
exactly eight. GCC at -O2 cannot follow that and says "writing 16 bytes into a
region of size 4". It was unreachable, and the invariant now costs one line
and is stated where the writes are.

`src/net/sealing.cpp` built the sequence number with eight `push_back` calls.
Inlined into `Sealer::seal` at -O2, GCC decides the vector's buffer might be
freed at a non-zero offset and refuses the build. Sizing the vector once and
writing into it says the same thing where the optimiser can see it, and does
one fewer capacity check per byte.

**Neither would have shown up in a debug build**, which is the argument for a
server-only Release configuration that takes half a minute: it is cheap enough
to run.

### Why ten aeroplanes have only a turns lesson, 2026-09-22

**Looked into rather than assumed.** Four of the six exercises name a speed
the aeroplane publishes, and ten of the sixteen publish neither a stall speed
nor a rate of climb. What they do publish was checked one by one:

- the **airliners** publish `climb_at_altitude` and `climb_gradient_one_engine`
  - a ceiling check at Mach 0.78 and an engine-out gradient. Neither is a
  climbing speed and neither is a stall;
- the **B-2** publishes a maximum climb rate and a cruise range;
- the **Short S.23** publishes a water take-off and nothing else;
- the **F-15C** publishes a maximum climb rate, a level Mach, a service
  ceiling and a sustained turn rate.

**One of the ten turns out to have a way in.** The F-15's own flight manual,
already recorded in `ASSETS.md` for its checklist, publishes stall speeds - in
charts, and its worked sample gives 107 knots flaps and gear down at 40,000
lb, 15 degrees of bank, 10,000 feet and maximum thrust. That is not the
power-off wings-level figure this project's `stall_speed` entries hold, so it
cannot be dropped in: it needs either a figure that carries bank and thrust as
conditions, or the 1g number read off the chart rather than off its sample.
Recorded as a tail with the citation rather than half-built.

**The others are genuinely blocked on figures that are not published**, which
is a different thing from a lesson nobody has written. `sim::approach_speeds`
and `sim::departure_speeds` throw rather than guess, which is right: a
reference speed invented is a reference speed that means nothing.

### Lessons for the aeroplanes that publish enough, 2026-09-22

**What is still missing: ten aeroplanes publish neither a stall speed nor a
rate of climb**, so four of the six exercises cannot be written for the
airliners, the fighters, the bomber or the flying boat. That is a figures
problem, not a lesson-writing one.

**What was added: six lessons.** The Mosquito publishes a rotation speed, a
climbing speed and a stall, so its class now has all five exercises; the
Learjet publishes a stall only, so its class has the approach, the stall and
the turn. Seventeen lessons in all.

**Every aeroplane taught an exercise is flown through it**, and the tests
count: five take off, six approach, six stall, five climb, all sixteen turn.
The Mosquito rotates at 121 knots and climbs at 148 against a Cessna 56 and
75; the Learjet crosses its threshold at 125 against a Cub 43. One line of
data holds each to its own.

**A test that was handing every aeroplane the Cessna lesson.** The take-off
helper still looked up `light-aircraft-take-off` by name, so the Mosquito was
flown against the light aircraft lesson, whose climbing band is a literal 45
to 120 knots - and a Mosquito climbing correctly at 150 was told to hold its
climbing speed. The lesson was right and the test was handing it the wrong
one. It looks the lesson up by the aeroplane class now, as the others do.

**Three things a light aeroplane does that the others do not.**

A clean jet at idle descends while it slows: the Learjet loses thousands of
feet entering a stall where a Cessna holds its height, so the exercise is
flown at twenty thousand feet and the entry band is wide enough to allow the
descent. A band written for a Cessna flew it into the ground.

A published stall speed is a low-level figure with everything down, and a
clean aeroplane practising high breaks well above it - the Learjet at twenty
thousand feet gives up at about 119 knots against a published 96. Waiting for
96 meant waiting for a speed that never came: she broke, dropped, accelerated
away in the descent, and the lesson watched her fall twenty thousand feet
without ever calling the stall. Both non-light classes call it at `stall+25`.

And a jet flares higher: the Learjet crosses its threshold at 125 and is down
to 95 by twenty feet, which is the flare doing its work. Its lesson watches
the speed to forty feet where the light aircraft lesson watches to twenty.

**A stall recovery is not a control position.** The recovery was a fixed
control column movement, which recovers a Cessna and flies a Learjet into the
ground - the two differ by a factor of three in speed and far more in
inertia. It is flown by the autopilot now: ask for a speed well above the
stall and a gentle descent, which is the same instruction to every aeroplane,
and each flies it with its own controls.

### A turns lesson for every class, 2026-09-22

**What is still missing: the other four exercises for the other six
classes.** The take-off, the approach, the climb and the stall all name
speeds an aeroplane publishes, and eleven of the sixteen publish none - so
those four exist for light aircraft and nowhere else.

**A turn names no speed, which is why it exists for everything.** `vref`,
`stall` and `climb` come from `assets/figures/<id>.xml`, and only five
aeroplanes publish a stall speed: the two Cessnas, the PA-28, the Learjet and
the Mosquito. Bank and height the simulation can see on anything that flies.

**All sixteen aeroplanes fly their own class's turns lesson**, not one of
each class - a lesson that suited the A320 and not the A380 would be teaching
one of them wrongly. Every one gets round all three stages with an empty
debrief, and the worst height lost is 55 feet of three thousand, by the
747-400. The test counts the aeroplanes and the classes and fails if either
falls short.

**Lessons are named `<class>-<exercise>`, and that is a rule now.** The five
light aircraft lessons were `light-take-off` and so on while the class is
`light-aircraft`, so the runner looked for `light-aircraft-turns` and found
nothing. They are renamed rather than special-cased.

**An aeroplane that publishes no figures still has lessons.**
`departure_speeds` and `approach_speeds` throw rather than guess, which is
right - a reference speed invented means nothing - so the test now flies an
aeroplane whose figures are missing and lets a lesson that names none of them
be flown all the same.

**And the plan had a warning the wrong way round.** It said the autopilot's
"gains are the Cessna's and will not suit a jet". Probed through a ninety
degree turn at 10,000 feet, every jet, the flying boat, the Mosquito and the
bomber hold their height to within sixteen feet - and the **Cessna** loses
2,076 feet and decays to 46 knots. At 3,000 feet the same Cessna is exact. It
is not a jet problem: it is an aeroplane near its ceiling being asked for a
thirty degree bank it has not the power to sustain, with nothing reducing the
bank. That is now a tail of its own.

### The circuit, written and withdrawn, 2026-09-22

**What is missing: the circuit lesson.** It was written, it would not fly,
and it has been taken out rather than shipped with a test that does not
demonstrate it.

**Two things the attempt produced, and they are kept.**

`lesson/turned-deg` is the runner own property: how far the aeroplane has
turned since the stage began, signed, and free of the wrap at north. A circuit
goes right round, so a raw heading cannot express one of its turns - a heading
of 340 is not less than 70 less eighty, and the stage would never end. Turning
is a difference, and differences are what the runner can work out and the data
cannot. The turns lesson used to end on `attitude/psi-deg <= start-60` and
passed only because its test happened to turn 90 degrees to 0 without crossing
north; it uses the difference now.

And a fault in the lesson itself: **a left circuit has four turns of ninety
degrees, not three.** The first draft had three and put the downwind leg
inside the turn on to it, so the leg began while she was still rolling out and
ended twenty degrees later - after which the aeroplane spent the rest of the
circuit being flown by a lesson that thought it was somewhere else. A leg
begins when the turn before it ends, which is why the turn on to downwind now
ends on the wings coming level rather than on ninety degrees of turn.

**The autopilot was suspected and cleared.** Commanded from 218 degrees to
250 - thirty two degrees to the right - while descending at 400 feet a minute
and holding 85 knots, it turned the *short* way, was on the heading inside
twenty seconds, held the speed to a tenth of a knot and descended at 396 feet
a minute. It was built by linking a standalone probe against the libraries
rather than by reading the code, because two readings of the heading loop had
already failed to settle it.

**So the fault is in the test that flies the pattern, and it is not found.**
The aeroplane climbed through downwind while the autopilot own modes read "no
altitude, -400 feet a minute" - the very configuration the probe descends in.
That is as far as it got. Guessing further would have cost more than stopping.

**Five of the six lessons stand**, all for light aircraft: the take-off, the
approach and landing, turns, climbs and descents, and stalls.

### Stalls, 2026-09-22

**What is still missing: the circuit, and six of the seven classes.** Five of
the six lessons exist, all for light aircraft.

**The stall is entered the way a stall is entered**, not simulated: the
throttle closes, the autopilot goes on holding the height by raising the
nose, and she slows to the stall of her own accord. Recovered as the wing
gives up - control column forward, full power - the four lose between fifty
nine feet and a hundred and ninety six: the Cub 59, the PA-28 170, the 172
189, the 182 196. The allowance is three hundred, which is that with a
hundred feet of margin.

**`stall` is the fifth figure a lesson may name**, after `rotate`, `climb`,
`vref` and `start`. The four light aircraft stall between about 33 and about
50 knots, so a number would have the Cub stalling at the Cessna speed.

**A parser trap, caught by its own test.** `stall` and `start` both begin with
"st" and the reference list is scanned in order, first match winning. The list
puts `stall` first and a test now holds both `start-150` and `stall+6` to
reading as themselves - because the next person to add a name beginning with
"st" will not know this.

**Two bad demonstrations, thrown away.** The first sloppy recovery used a
gentler control movement as well as a later one, and flew the aeroplane into
the ground: lowest four feet of five thousand. The test passed and the
debrief named the right fault, but what it had demonstrated was that the
lesson catches a crash, which is not what it is for.

The second timed the recovery off a speed six knots below the stall - a speed
that never comes, because an aeroplane mushing in a stall stops slowing. The
recovery never triggered at all, the autopilot went on holding the nose up
with no power, and she descended five thousand feet. That is not a late
recovery, it is no recovery.

**The demonstration that was kept changes one thing.** The same control
movement, the same power, started late - timed from the moment the lesson
calls the stall. Twelve seconds late costs about three hundred feet and
scrapes through; twenty five costs four hundred and thirty and is named. That
gap is in the lesson file, so the band can be judged rather than believed: a
band that caught a twelve second delay would be catching the difference
between two good recoveries.

### Turns, and climbs and descents, 2026-09-22

**What is still missing: the circuit, the stalls, and six of the seven
classes.** Four of the six lessons exist, all of them for light aircraft.

**`start` is the fourth figure a lesson may name**, after `rotate`, `climb`
and `vref`, and it is not a speed: it is whatever that watch own property
read when the stage began. `hold position/h-agl-ft start-150 start+150` is
"within a hundred and fifty feet of where you were", wherever that was. The
runner resolves it, because it alone knows when a stage began and what the
aeroplane read then. Without it a turn lesson would have to name an absolute
altitude, which would bake one test flight into the data.

**The skill a turn teaches is the height, and the numbers say so.** Flown by
the autopilot through ninety degrees of left turn at about twenty-six degrees
of bank, the four light aircraft hold their height to within ten feet of
where they started: the 172 to 2,990 of 3,000, the 182 to 2,991, the PA-28 to
2,996, the Cub to 3,003. Let sink at 1,200 feet a minute through the same
turn, the 172 reaches 2,532 and the debrief says "Hold your height through
the turn".

**A band has to be set against how long the stage lasts.** The first draft
allowed two hundred feet and caught nothing: at that bank and speed the turn
stage lasts about eleven seconds, so an aeroplane let sink at eight hundred
feet a minute through it loses only about a hundred and fifty. A hundred and
fifty feet is now the band - fifteen times what correct flying actually does,
and close to the hundred a licence test allows.

**And a lesson begins when the exercise begins, not when the simulator
does.** The climb lesson faulted every correct climb at first, because it
started judging on the first tick - while the aeroplane was still at its
cruise speed and the autopilot had not yet slowed it to the climbing speed.
It was judging the settling, not the climb. The test lets her establish
first, which is what an instructor would do.

### The approach and the landing, 2026-09-22

**What is still missing: four of the six lessons, and six of the seven
classes.** There is a take-off and an approach, for light aircraft. No
circuit, no stalls, no turns, no climbs and descents.

**The approach lesson is flown by all four light aeroplanes with an empty
debrief**, down a three-degree glidepath from two miles, each at its own
reference speed: the 172 at 60 knots, the 182 and the PA-28 at 64, the Cub at
43. `vref` is the third figure an aeroplane can publish to a lesson, after
`rotate` and `climb`, and it comes from `sim::approach_speeds` - 1.3 times the
published stall with everything down.

**Flown twenty knots fast, the debrief names two things and both are true**:
hold the approach speed, and cross the threshold on speed. The aeroplane flies
a perfectly good approach the whole way down - it is simply doing it at a
speed it has no business using, which is what an approach flown fast is. The
approach autopilot is handed a reference speed the aeroplane has not got; the
lesson resolves `vref` from the published figures and sees the difference the
autopilot cannot.

**Two drafts of the bands were wrong, in opposite directions, and the
measurement settled both.**

The first held a speed band down to five feet and faulted every correct
landing: the PA-28 crosses its threshold at 64 knots and touches at 37, which
is a good landing. **The flare is where the speed is meant to go** - throttle
closed, nose up, flown onto the wheels as the speed decays - so the speed is
watched to twenty feet and let alone after that.

The second was a knot too tight at the bottom and far too loose at the top.
Against their own `vref` the four hold between `vref-2` and `vref+5` on the
approach; the band was `vref-5` and the PA-28 failed it by one knot, while a
top of `vref+25` would not have caught an approach flown twenty knots fast at
all. It is `vref-8` to `vref+12` now - about six knots of margin either side
of what correct flying actually does - and the measurement is written into
the lesson beside it.

**Per-stage instrumentation is what found it.** The first measurement took the
speed over the whole flight and mixed the approach, the threshold and the
flare into one range, which said only that something somewhere was out of
band. Recording the speed against the stage it happened in said which stage,
which aeroplane, and by how much.

### A lesson figure that is the aeroplane own, 2026-09-22

**What is still missing: one lesson, for one class.** The take-off, for light
aircraft. No circuit, no stalls, no turns, nothing for the other six classes.

**The limitation written down an hour ago is gone.** A lesson figure may now
name a speed the aeroplane publishes rather than a number: `rotate`, `climb`,
with an optional offset - `rotate-3` - worked out per aeroplane by
`sim::departure_speeds` from its own `assets/figures/<id>.xml`. One line of
data holds the Cub to 31 knots and the Cessna to 52.

**This is what teaching a class means.** A lesson is written for a class of
aeroplane, and the four light aircraft rotate between about 34 knots and
about 55. A plain number can only be the slowest of them, so the take-off
lesson watched a 172 come off at 47 knots - against the 55 or so it should
use - and said nothing. With `rotate-3` the debrief names it: "Let her reach
the rotation speed before easing the nose up".

**The early rotation now says two things, and both are true.** She came off
early, and the attitude wandered while she did it - which is what hauling an
aeroplane off before its speed actually does. A debrief naming only one would
be hiding the other. The throttle fault, flown by the book in every other
way, still says exactly one thing.

**The parser refuses eighteen ways of being wrong now**, the two new ones
being a reference there is none of - `stall` - and one with an offset that is
not an offset, `rotate*3`.

**A false green, for the third time today, and the same shape each time.** A
`python3 - <<PY` script with an apostrophe escaped as `\\'` inside a
single-quoted string is a syntax error; the script writes nothing, exits
non-zero, and the `&&`-chained compile or test that follows runs against the
*unmodified* file and passes. "lesson.cpp compiles clean" was printed about a
file that had not been changed. Every script here now re-reads the file it
wrote and asserts the change is in it, which is what caught this one.

### A lesson, and a debrief, 2026-09-22

**What is missing first: there is one lesson.** The take-off, for light
aircraft. No circuit, no stalls, no turns, and nothing for the other six
classes of aeroplane.

**But a lesson is data now, and a debrief exists.** A `.lesson` file is a
sequence of stages, each with what to do, what ends it, bands that must
`hold` through it and things that must be true by its end. The two are not
the same and the file says why: a `hold` is broken the moment the aeroplane
leaves the band, because an approach flown too fast is wrong while it is
happening; a `need` is judged when the stage ends, because flap down by the
time you turn final is not a fault until you have turned final. The debrief
is what to do differently, in the order it happened, and nothing else - there
is no score (`FEATURES.md`).

**Flown by the book, all four light aeroplanes leave it empty** - the Cub,
the PA-28, the 172 and the 182, each taken off by the take-off autopilot on
its own published figures. Flown on part throttle, the debrief holds exactly
one thing and it is "Open the throttle fully for the take-off".

**The attitude band was guessed, and then measured.** The first band, -2 to
20 degrees, faulted every correctly flown take-off there is. Flown by the
book the four hold between -4.5 and 19.4 degrees once off the ground - Cub
-2.5 to 18.3, PA-28 -3.0 to 18.7, 172 -3.9 to 19.1, 182 -4.5 to 19.4, the
nose-down figures being the moment after lift-off as she settles into the
climb. The band is that range with about four degrees of margin, and the
measurement is written into the lesson beside it.

**Rotating early cannot be injected by telling the autopilot to rotate
early.** Tried: with the rotation speed set to 25 knots the Cessna came off
at 80 all the same, because an aeroplane below its stall will not fly
whatever its nose is doing - she simply rolls on with the nose up. Flown the
way it actually happens, a steady touch of back stick, she is at 47 knots
where the book has her at 80.

**And the lesson does not catch it, which is worth more than hiding it.** A
lesson teaches a class; the four light aeroplanes rotate between about 34
knots and about 55; so the only speed a class-wide lesson can name is the
lowest of them, and 47 knots is nowhere near it. Catching a 172 rotating
early needs the lesson to say "her own rotation speed" rather than a number.
The figure is already in `assets/figures/<id>.xml` and `sim::departure_speeds`
already reads it - the format simply has no way to refer to it. That is named
in the lesson file, in the test, and in `COMPLETION_PLAN.md`. What catches a
badly flown rotation today is the attitude band, which it does: -9.0 degrees
against a floor of -8.

**The parser refuses sixteen ways of being wrong**, each walked and counted:
no name, no stages, a stage that never ends, a stage with nothing to do, a
command before any stage, a class there is none of, a command there is none
of, an `until` without an operator or with a bad one or without a number, two
`until`s in one stage, a `hold` without a band or with a backwards one or
without words, and a `need` without words or with a bad operator.

### What kind of flying an aeroplane is for, 2026-09-22

**What is still missing: the lessons themselves.** There is no lesson format,
no debrief, and nothing that flies a circuit, a stall or a turn to a
standard. What there is now is the thing a lesson has to be written against.

**The classes are data.** `REQUIREMENTS.md` section 4.2 names seven in a table
and no aircraft file said which it was, so a lesson - written per class,
because a circuit in a Cub and a circuit in a 747 are not the same lesson -
had nothing to attach to. Every one of the sixteen now carries a `class` line,
one of light-aircraft, seaplane, second-world-war, business-jet, airliner,
fighter or bomber; a file without one is refused where it is wrong.

**The test holds the data to the document, aircraft by aircraft.** It reads
`REQUIREMENTS.md`, finds the line each aeroplane is named on, and fails if
that row is not the class the file claims. Watched failing: with the F-22
marked a bomber it said "f22 says it is a bomber, and REQUIREMENTS.md names it
on the line: | Fighters | F-15 Eagle, F-22 Raptor | JSBSim's |". All seven
class names round-trip, a name that is not one of them is refused, and all
sixteen are counted.

**A verification that was not one, found and corrected.** The three checklists
written from handbooks earlier today were each reported as "8 of 8 checklist
tests pass". `CMakeLists.txt` copies `assets/` into `build/<preset>/data` **at
configure time**, and the tests read that copy - so running `ctest` after
editing an asset, without reconfiguring, exercises the *previous* file and
passes on it. All three of those runs were against stale data and proved
nothing about the new files.

It was found only because an unrelated `.aircraft` edit happened to trigger a
reconfigure and the cost was worth asking about. The files turn out to be
sound: after a reconfigure, with `diff` showing the copy matches the source,
all nine checklist tests pass - including the one that flies every aeroplane
to see that each band is reachable. **The fix is not to trust the green
tick**: after an asset edit, show the copy is current before believing a test.

### "Survives only as scans" was not a reason, 2026-09-22

**What is still missing: ten of the sixteen aircraft have no handbook behind
their checklists**, and for all ten that is now a stated reason rather than an
omission.

**The J-3 Cub was recorded as unsourceable because its manual is a scan with
no text layer.** That stops a text search. It stops nothing else: the pages
were read as pages, the same way the Mosquito's Pilot's Notes were read an
hour earlier. Piper's Owner's Manual for the J3C-65 carries "Flying Hints" on
pages 37 to 42 - sections A to H, from before starting the engine through to
parking and mooring - and it is the manual `ASSETS.md` already recorded for
the Cub's figures.

**What it gave that the unsourced file did not have**: the throttle a tenth
open to start rather than "a quarter of an inch"; 700 r.p.m. to idle on and
550 to 600 once warm; the magnetos checked at 2,100 with no more than 75
r.p.m. dropped; 2,300 r.p.m. for take-off and no full throttle beyond three
minutes; airborne at 39 m.p.h.; best climb at 55; 2,150 r.p.m. for a 73 m.p.h.
cruise; the glide at 50 to 60; carburettor heat on *before* the throttle comes
back; the engine cleared every 200 to 250 feet of a long glide; and 122 m.p.h.
as the diving limit. The file is in knots with the manual's miles an hour
beside them, as the PA-28's is.

**Two near misses in one afternoon, both the same shape.** A script wrote one
living document and silently did nothing to the other, and the commit went
ahead because nothing checked. The first time `ASSETS.md` said five and
`COMPLETION_PLAN.md` said three. The second time this entry was not written at
all - the replacement was looking for an escaped newline that the file does
not contain - and the commit that was meant to carry it went out without it.
**A string replacement that matches nothing is not an error unless something
asserts it is.** Every replacement in these scripts now asserts.

**And a third, of a different shape**: the first attempt at the Cub's file had
a quoting error and wrote nothing, and the checklist tests were run anyway and
reported eight of eight passing - on the unchanged file. A green tick on a
file that was never written is not evidence of anything. The second attempt
printed `git diff --stat` beside the test result: 51 insertions, 33 deletions,
and then the eight passes.

### Two checklists that were never actually blocked, 2026-09-22

**What is still missing: eleven of the sixteen aircraft have no handbook
behind their checklists**, and for eleven of them that is now an answer rather
than an omission.

**The item said thirteen and explained ten.** The J-3 Cub's manual survives
only as scans; no flight manual is public for the B-2A, the F-22A or the
F-35B; the airliners' and the business jet's are their operators'. That is
ten aeroplanes. The Mosquito, the F-15C and the Short S.23 were in the list
with no reason given at all, and nobody had noticed because the number was
carried in prose rather than counted.

**The Mosquito's Pilot's Notes were already in the repository's own
documentation.** A.P. 2019E-P.N., January 1950, recorded in `ASSETS.md` a few
sections above the checklist entry - cited there for the aircraft's figures,
as `[PN50]`. It carries its own **"PILOT'S CHECK LIST", 128 numbered items on
pages 6 to 10**, with Part II paragraphs 37 to 44 giving the handling to go
with them. There was nothing to find; it only had to be read.

**Reading it corrected three things** in the file that had been written from
ordinary practice for the type: the radiator shutters were open for starting
where para. 37 shuts them and opens them at the magneto check; the fuel cocks
were on the main tanks where para. 37 starts on the outer ones; and the climb
was at 148 knots, the 1944 edition's 170 mph, where the 1950 edition's para.
41 gives 150.

**The F-15C's flight manual is on the Internet Archive.** T.O. 1F-15A-1,
section II, gives the drills and the numbers: engines to 80 per cent before
brake release, half aft stick at about 120 knots and ten degrees of pitch,
350 knots to 0.90 Mach on the climb, 300 knots at the break, gear and flaps
on downwind below 250, on-speed angle of attack on final, the nose to thirteen
degrees for aerobraking and not past fifteen, and the wheel brakes only after
aerobraking has done its work above 90 knots.

**What the F-15C's file cannot say, and says so.** The model has no flap
channel, so "Flaps - CHECK DOWN" before take-off and "Flaps - UP" after
landing are the pilot's to confirm rather than checks; and there is no
angle-of-attack channel, so the manual's on-speed approach is a confirmation
beside a speed band from this project's own figures.

**A slip worth recording.** The script that updated both living documents
failed its assertion on `COMPLETION_PLAN.md` and the commit ran anyway,
because the two were separate commands rather than one. `ASSETS.md` said five
and the plan said three, pushed. The next commit fixed it; chaining the write
to the commit would have stopped it.

### Google's tiles, through the door that had never been opened, 2026-09-22

**What was missing was a key, and nothing else.** The direct Google Maps
Platform route had been written on 2026-09-21 and never run, because no such
key existed on either machine. The project owner supplied one; it went to
`~/.config/glideslope/google-maps-key`, mode 600, which is where
`platform::google_maps_key()` already looked. Nothing needed writing.

**Mount Taranaki, 241 tiles, through the Maps Platform key alone** - drawn
with a configuration directory holding that key and no ion token at all, so
there was no route for it to fall back to. Google's attribution is on screen
with it: GOOGLE, AIRBUS, DATA SIO/NOAA/U.S. NAVY/NGA/GEBCO, VEXCEL IMAGING,
MAXAR TECHNOLOGIES, LANDSAT/COPERNICUS.

**The test was not proving what the item asked.** "Through both ways in" was
being answered by a run that used whichever secret the machine had - and the
code prefers the Google key when both are present, so on a machine with both,
the ion route would never be taken and would go untested for ever. Each way in
now gets a configuration directory of its own holding one secret, and the two
are counted: two of two drew, nought not testable here.

**A bug in that test, found by reading its own output.** The first way in
points `XDG_CONFIG_HOME` at its isolated directory; the second then asked the
environment where this machine keeps its secrets and was told *that*
directory, found nothing, and reported itself untestable - passing while
testing one route twice. It reads the real location once, before the loop,
now. The give-away was the line "no google-maps-key on this machine" printed
on a machine where the key had just been written by hand.

**Watched failing.** With the direct-key branch disabled, the ion route still
drew its 241 tiles and the other was told it "drew nothing" - the half that
had never run before today.

**A note the owner should act on.** The key was pasted into the conversation
rather than placed in a file, so it exists in that transcript. It is not in
the repository and never will be, but restricting it to the Map Tiles API or
rotating it would be prudent.

### Joining late, and leaving, 2026-09-22

**What is missing first: a player cannot hand their own aircraft to an AI
pilot and take it back.** That is the controller swap, it is the thing the
whole project is named around, and it needs a `CONTROLLER_SWAP` message to
travel - which needs the reliable layer inside a sealed body, which is not
built. What works is only what the *server* does when somebody goes.

**AI traffic with nobody connected.** A client that waits five seconds before
connecting finds a session at **4.200 s** with both AI Cessnas away from where
the plan starts - `-33.90301` and `-33.90298` against a start of `-33.905` -
and its own aircraft, made the moment it joined, still on the start point.
That last one is the control: if everything in the packet had moved, the test
would be measuring the plan's start point rather than the passage of time.

**A player leaving.** There is no goodbye message and a crashed client could
not send one, so leaving is going quiet and the `--timeout` sweep is what
notices. `--on-leave remove` takes the aircraft out of the sky; `--on-leave
ai` hands it to an AI pilot flying the server's plan, renumbers it - a
player's aircraft is numbered by their slot, and that slot is about to be
given to somebody else - and says so. Both settings walked and counted.

**Under `ai` the test makes the aircraft prove it is being flown.** Handing it
to a pilot that then flew nothing would leave it hanging where its owner left
it, and a check that counted aeroplanes would pass. So the handed-over
aircraft must have moved from the start: it ends at `-33.902101, 151.289744`,
which it could only reach by being flown there.

**`--after N` on the client is a test flag and says so.** A client that
connects the instant the server does can learn nothing about whether the
server was flying before it arrived.

### A client flies, 2026-09-22

**What is missing first: a client is never told anything.** None of the seven
reliable messages travels, so a connected client never learns the lobby, the
session, the weather, what aeroplane anybody is in or what the terrain dataset
is. It also does not choose its aeroplane: a player flies whatever the
server's flight plan flies and starts where it starts.

**But it flies.** A client sends inputs and nothing else; the server owns the
aircraft and flies it by them; the client learns what happened only from the
state updates coming back. Measured over the loopback: **177 input frames sent
in six seconds, 176 applied**, and the aeroplane rolled to 180 degrees of bank
- inverted - on full left aileron.

**A player joining is given an aircraft numbered by their slot.** That is why
a state update can say `your_aircraft` and a client can pick its own line out
of twenty: slots 0 to 3 are the players, and everything else is numbered from
4 upwards and never moves. A player leaving takes their aircraft with them;
handing it to an AI pilot instead is a session setting and its own item.

**A slot is not an aeroplane.** A server with nothing to fly has loaded no
terrain and has nowhere to put one, so a client connecting to one gets a slot,
no aircraft, and state updates that say `no_aircraft`. That is what every
server test that passes `--ai 0` is doing, and it is why they need no network.

**The bug worth writing down.** The first run flew: the server applied the
inputs, the client saw its aeroplane roll - and only to 13 degrees in four
seconds. Full aileron on a Cessna does far better than three degrees a second,
so it went back for a look. `Fleet::step()` set the controls itself on every
step for an aircraft with no AI pilot - throttle 0.6 and everything else at
rest - so a client's inputs were applied when the datagram arrived and wiped
120 times a second afterwards. **A control is a position, not an event**: a
stick held over stays over. The aircraft holds its controls now, and the same
run gives 180 degrees where it gave 34.

**It would have passed a careless test.** "Rolls more than ten degrees" is
what a test written to the first run's output would have said, and it would
have been green on a server that threw away 119 of every 120 inputs. The bar
is 90 degrees - past it the aeroplane is inverted - which neither a server
that drops the inputs nor one that forgets them can reach.

### The state stream, over a socket, 2026-09-22

**What is missing first: a client still cannot fly.** The server sends state
25 times a second and reads nothing back from a client but a `PONG`. Inputs
are defined and encoded and reach no aircraft. A client is a spectator with a
slot.

**But it can see.** Run as a test and by hand: a server flying two AI Cessnas
over Sydney Harbour, a client connecting over the loopback, and **73 state
updates in three seconds** - 25 Hz is seventy-five - each carrying both
aircraft, which the client turned back into latitude, longitude and height:
-33.905, 151.290 at 914 m and 1,067 m.

**Why the test checks the places and not the count.** A packet of the right
size full of the wrong place is what a wrong frame, a wrong sign, or a
latitude and longitude the wrong way round all produce, and a count would pass
every one of them. So the client turns the ECEF metres back into a position
and the test holds them to the harbour the flight plan starts over.
**And to the gap between them**: the server stacks AI aircraft 500 ft apart,
which is 152 m, and a height dropped on the way out would leave them both in
the right place horizontally. The measured gap is 153 m.

**25 Hz, and why that number.** `REQUIREMENTS.md` 6.6 asks for 20 to 30. 25 is
the middle of it and divides 120 exactly, so a state update always lands on a
simulation step rather than between two.

**`ned_to_ecef` had to be written first.** A flight model gives a velocity in
north, east and down; a position on this wire is Earth-centred. The rotation
between them is four lines and is now in `world/geodesy`, held by
`a_local_velocity_becomes_the_same_velocity_in_the_earths_frame`: **361 points
over the whole Earth** - every ten degrees of latitude and twenty of longitude
- each checked for three unit vectors at right angles to each other and for a
speed that survives the rotation, plus the three places at 0, 0 where the
answer can be written down by hand. **And that down is towards the ellipsoid
and not towards the centre**, which is the only reason the function takes a
geodetic latitude at all: at every latitude but 0 and the poles the two must
differ, and the test fails if they do not.

### What the server will send back, 2026-09-22

**What is missing first: nothing sends one.** The `STATE` packet is defined,
encoded and tested, and no server writes one and no client reads one. The
server flies its aircraft and tells nobody where they are.

**But it is the piece the whole of Phase 7 waits on**, because a controller
swap is only visible if there is a state stream to see it in. A packet carries
the simulation's clock, the newest input sequence from this client the server
has applied - which is what a client reconciles against - and up to twenty
aircraft, each with its number, who is flying it, a position, a velocity and
an attitude.

**Positions are Earth-centred, Earth-fixed and double precision, and that is
not a style choice.** The whole world is in play, so there is no session
origin for an aircraft to be near: two in one session may be on opposite sides
of the planet, which the server already does. A float has 24 bits of mantissa,
which at Earth's radius is half-metre steps. Velocities and angles are floats,
because a float holds them far better than anything can measure them, and
twenty aircraft twenty times a second is worth the bytes.

**Twenty aircraft is the most, and it fits.** Four players and sixteen AI is
what `--players` and `--ai` allow. A full packet is **1,014 bytes, and 1,044
with the envelope and the sealing in front of it, inside the 1,232 a datagram
holds** - measured by the test, not calculated in the document. The weather
message taught this lesson the expensive way: it was 5,419 bytes in an item
that had already been ticked.

**`f32` had to be added to the wire first**, with its own row in the format
table and its own eleven values in
`every_value_written_to_the_wire_reads_back_as_itself`, which now walks 51
values rather than 40 and counts them.

**Walked, not sampled.** Every count of aircraft from nought to twenty; 1,456
truncations; all 1,312 one-bit changes to a three-aircraft packet - and the
rule there is not "does not crash" but **what the reader takes must write back
to the very bytes it was given**, so nothing is lost and nothing invented: 37
refused, 1,275 read exactly. All 256 bytes in the controller's place, three of
which are controllers. A NaN and both infinities in each of the ten numbers a
packet holds, refused on the way out and on the way in, thirty cases counted.

**Watched failing.** With the trailing-byte check and the reader's NaN check
taken out, three of the seven went red - including the one-bit test, which
found the NaN hole on its own, because flipping bits into an exponent makes
one.

**And two parsers were missing from the fuzzing corpus.** `read_state` is new,
but `knock_token` was not: it went in when the ping did and nothing put it in
the list. Thirteen parsers and nineteen seeds now, each counted in the test so
that a parser added without a seed fails.

### A hole found by re-reading the threat document, 2026-09-22

**What is missing first: nothing a client sends drives an aircraft.** A client
connects, holds a slot, answers the server's knocking and is drawn on the
dashboard. It cannot fly.

**A second handshake initiation used to take a live session away.**
`connections[who] = std::move(c)` was unconditional, so any datagram that the
server could answer gave that address fresh keys - and **anybody can make a
valid `IK` initiation, because it only needs the server's public key**. Replay
a captured one with a player's source address and that player is off the
server, in silence, for the price of one packet. It bit honest clients too:
the client resends its initiation every 250 ms until answered, so two answers
in flight left the client on the first set of keys and the server on the
second.

**It was found by making `THREATS.md` true again, not by a test.** The document
had been written when the server accepted nothing; bringing it up to the code
meant reading `take()` line by line against what the document claimed, and the
claim "there is no per-peer connection table" turned into "there is one, and
here is what it does wrong". Three more came out of the same reading: a
displaced key was never released from `Slots`, so four `connect` runs from one
address filled a four-player server for good; the table is keyed by address, so
a replayed initiation from many spoofed addresses costs an X25519 apiece; and
`SERVER_FULL` and `BAD_HANDSHAKE` go to senders that have not authenticated.
The first two are fixed below. The third and fourth are written down and not
defended - there is no rate limit of any kind.

**The fix is one session per address.** The initiation that made a session is
kept beside it: the same bytes get the same answer back, which is what an
honest client whose answer was lost needs; different bytes are dropped without
a word, so a stranger cannot rekey somebody's session. A slot goes back only
when no connection is left on that key, because two addresses may share one.

**Held by a test that was watched failing.**
`a_repeated_handshake_initiation_does_not_take_a_live_session_away` runs a real
client and a real server at once - the client completes a session, sends its
initiation again, and must still be answering pings three seconds later.
Against the code as it was it fails with "0 slots have a client on them after
one client connected twice".

**What it cost to find is worth saying.** The subagent that re-read the
document found it; the hole was in code written earlier the same day, by me,
and no test I wrote for that code would ever have caught it, because every one
of them had a well-behaved client in it.

### The store, and the timeout shown to work, 2026-09-22

**What is missing first: what the store keeps is one key and nothing else.**
There is no session in it - no slots, no aircraft, no history - because there
is no session state yet worth surviving a restart. It is a table of names and
values with one row in it.

**But that row is the one that matters.** A client is given the server's
public key out of band: written in a `server.txt`, pasted into a command line.
A server that minted a fresh key at every start would lock out every client it
had at each restart, which is exactly what `REQUIREMENTS.md` 6.6 means by
"minted once and stored if not given". `--store FILE` is an SQLite file, made
if it is not there, and `a_server_given_a_store_keeps_its_key_across_restarts`
walks **all four ways a server comes by a key** - given on the command line,
read from a store, minted into a store, minted with no store - and counts
them, so a way that stopped being walked fails the test. Across three restarts
against one store the key held; two starts with no store gave two different
keys, which is the thing the store exists to prevent, shown to be real; and
`--key` wins over the store without overwriting what the store holds, because
a key given on a command line is the operator's to manage.

**A secret is one member access away from a log line, unless it is not.**
`SecretKey` still has no `text()`; writing one down goes through
`net::secret_for_keeping()`, which is named to be greppable and has one
caller. `PublicKey::text()` is the only thing a stray `printf` can reach.

**The store was watched failing.** Opened on `:memory:` instead of its file,
three of its five tests went red - the three that claim persistence - and the
two that only claim a round trip inside one store stayed green, which is
right. Against the server, making the key lookup use a name nothing writes
turned `a_server_given_a_store...` red with "the second start should read the
key, not minted it".

**`--timeout` was taken and checked and drove nothing that any test watched.**
Its parsing had tests; its effect had none, because until this week no client
could connect to go quiet. Now one can:
`a_server_lets_go_a_client_it_has_not_heard_from_for_its_timeout` runs the
client and the server **at the same time**, the server admitting the client to
slot 0 and letting it go 0.5 s after it exited, once - not once round every
loop, which the test also holds.

**Two processes at once, out of CMake, which has no way to start one in the
background.** The commands of a single `execute_process` are a pipeline, so
they run together: the client goes first and its output into the server's
standard input, which the server never reads, and `OUTPUT_VARIABLE` catches
the last command's output, which is the server's. Nothing else in this project
needed two live processes before.

**A comment that described code that was not there.** `connect_to` said
"resending while nothing comes" above a loop that sent once and waited five
seconds. It resends every 250 ms now, which the timeout test needs - both
processes start at the same instant, so the first initiation goes out before
the server has bound - and which a client over UDP owes anyway.

**A line on the dashboard that had become a lie.** It said "nobody can connect
yet: the handshake is not built". It now says what is still true: ping and
traffic are not counted per player.

**The dashboard shows ping and traffic now.** The server knocks on each
connection once a second - a `PING` inside the seal carrying eight bytes, sent
straight back as a `PONG` - and draws the round trip and the bytes each way.
That knock is also the keepalive, which is why it is the server's job: the
server is the one deciding who has gone. `--plain` draws the same rows without
the escape codes that clear the screen, so a test can read them, and
`the_dashboard_shows_the_round_trip_and_the_traffic_of_a_client_that_stays`
holds a real round trip off a real socket.

**What is inside a sealed body is now decided**, because more than one thing
has to go in there. The plaintext begins with a byte saying which of five kinds
it is; `PING` and `PONG` are built, `RELIABLE`, `INPUTS` and `STATE` are
numbered and nothing sends them. The numbering is fixed before anything uses it
so that it cannot move later, `TRANSPORT.md` writes it down, and a test holds
the document and the code to each other.

**And the server item still does not tick.** `REQUIREMENTS.md` 6.6 asks for "a
live dashboard with connected clients, ping, traffic and a drop control". Ping
and traffic are there; **the dashboard is drawn in a terminal rather than a
window, and there is no way to drop a client from it.** Both are named in the
item.

**A flag that was documented and silently ignored.** `--plain` went into the
usage text and the settings block and not into the parser, and passed both of
its own tests, because neither of them looked at what the flag did. The lesson
generalises, so the test does:
`every_flag_the_server_prints_in_its_usage_is_one_it_takes` reads the usage
text itself, runs the server with each flag it finds, and fails if the parser
has never heard of one. Sixteen flags, counted against a written list, so a
flag that stopped being listed fails too.

### A session, over a socket, 2026-09-22

**What is missing first: a session does nothing yet.** A client connects, is
given a slot and can seal to the server, and nothing is done with what it
seals - no input reaches an aircraft and no state goes back. So a client can
connect and be counted, and cannot fly.

**But it connects.** Run by hand: `glideslope_server` printed
`server key bedf297d...`, `glideslope_cli connect 127.0.0.1:47900 <key>`
answered `session with bedf297d...` and sealed 35 bytes, and the server
logged `admitted 1f141fb4 to slot 0`. Held automatically by
`a_client_and_a_server_complete_a_session_over_a_socket`, which puts a
102-byte initiation and a 54-byte answer through the loopback in the
envelopes they really travel in and seals both ways under different keys -
every other test of the transport hands bytes from one object to another, and
proves nothing about a wire that was never used.

**Sealing.** A `SEALED` body is its sequence number and ChaCha20-Poly1305
ciphertext. The number is the nonce and the additional data, so changing it
only stops the body opening. A replay window of 64 refuses anything already
opened and anything further behind than that, **and the limit is stated**: a
datagram delayed by more than the window cannot be told from a replay. Only a
body that opens moves the window, so something that is not ours cannot push it
forward. Held over **all 720 orders of arrival** of six datagrams, each
delivered once and twice, every one opening exactly once; 7,650 one-byte
changes and 30 truncations refused. Both replay tests were watched failing
with the window taken out.

**Every one of the server's flags drives something now.** `--key` takes a
static key or mints one, and prints the public half because a client cannot
begin an `IK` handshake without it; `--timeout` lets go a client that has gone
quiet and gives its slot back; `--players` bounds who gets in, and a fifth is
refused with `SERVER_FULL`; and `--store` is below.

**A datagram that does not open is dropped without a word.** A refusal would
tell a forger that the address was right.

**A line that had gone stale under me.** The settings block still printed "this
build cannot mint one" of `--key` - true before libsodium and false after. Left
alone it would have been a small lie at every startup.

**And the same name collision twice.** The handshake's result was called
`Session`, which is already the `SESSION` message's structure. It is
`SessionKeys` now. The compiler caught it, as it caught `Slots` earlier.


### The handshake, and a decision taken without the owner, 2026-09-22

**What is missing first: nothing is sealed yet.** The handshake agrees keys
and neither end uses them. `SEALED` bodies are still plaintext and there is no
replay window, so the transport is not yet private for a single byte.

**The suite is not the one `REQUIREMENTS.md` 6.7 names, and the reason is
libsodium.** It asks for `Noise_IK_25519_ChaChaPoly_BLAKE2s`. libsodium - the
library that section chooses - ships `crypto_generichash_blake2b` and **no
BLAKE2s at all**, which was checked against its installed headers rather than
assumed. BLAKE2b is a hash the Noise specification itself defines, so
`Noise_IK_25519_ChaChaPoly_BLAKE2b` is a real suite and not an invention. The
alternatives were SHA-256, also native, or carrying a BLAKE2s of our own -
hand-written cryptography in a project that has none. **The owner was asked
and has not ruled**; the goal was reaffirmed instead, so the choice was taken
rather than blocking on it. It is one line: `handshake_name` and the functions
it names.

**What the tests prove, and what they do not.** They prove two honest ends
agree crosswise - each sends under the key the other receives under - that
each learns who the other is, that eight runs with the same keys give eight
different sessions, that a client told the wrong server key gets nowhere, and
that **25,755 one-byte changes and 101 truncations of an initiation, and 190
changes to an answer, are refused**. They do **not** prove it matches the Noise
specification: there are no published vectors for this suite in the project,
and conformance is not among the things shown. `THREATS.md` says so.

**A replayed initiation is answered.** The responder has no memory and cannot
tell, so it does the work; the session it makes is one the replayer cannot
read, lacking the initiator's ephemeral secret. That is a denial of service,
not a compromise, and it is named rather than defended.

**A hole in my own test, not in the handshake.** The answer-tampering test
made a fresh handshake for each change but decided what to skip from the
*first* answer's bytes. The ephemeral key differs every run, so sometimes it
"changed" a byte to the value it already had and then reported that an
unmodified answer had been accepted. One acceptance in 191. The skip is now
made against the answer actually being changed.

**The server's static key is real too**: minted, printed and read back, held
against **RFC 7748's own X25519 vector** - a round trip of our own could never
say the key agreement had stopped being X25519.


### Two things the wire believed that it should not, 2026-09-22 — two tails done

**What is missing first: nothing has ever arrived over a network.** There is
no handshake and no sealing, so every byte these two defences have turned
away was put there by a test. They are the parsers' own defences, not a
session's, and neither of them tells a forged datagram from a real one; that
is the sealing's job and the sealing is not built. Both were found writing
`THREATS.md` and both are in `COMPLETION_PLAN.md`'s tails.

**An acknowledgement of a message that was never sent.** `Reliable::received`
took the acknowledgement number out of any datagram handed to it and let go
of every message up to it, so one datagram carrying `0xFFFFFFFF` emptied the
send queue and nothing sent those messages again. The layer now keeps the
highest number it has actually put on the wire and ignores anything above it:
an endpoint cannot have received what was never sent. The rest of the
datagram is read as before - this is one field not believed, not a reason to
throw a message away - and every legitimate acknowledgement behaves exactly
as it did.

**The space, stated, because a `u32` has too many values to feed one at a
time.** 240 acknowledgements are fed: **175 at the boundary**, which is every
number from 0 to two past the end for every sender that has put 0 to 4
messages on the wire with 0 to 4 more queued behind them; **64 far ones** -
every power of two, every power of two less one, and `0xFFFFFFFF`; and one in
the middle of a real exchange, after which all four messages still arrive
once each and in order. Everything not walked is strictly above what was
sent, which is the case `0xFFFFFFFF` walks, and the layer does no arithmetic
on the number for a larger one to do differently.

**Watched failing**: before the fix, *"a sender that had put 0 on the wire
with 1 behind them, told it was acknowledged up to 1, should hold 1 and holds
0"*.

**A number that is not one.** Every `f64` was read as whatever bits arrived,
so a NaN or an infinity in a latitude, a microburst's radius or the
simulation's clock was accepted and handed up - a NaN position spreads
through the floating origin and the terrain query, and an infinite duration
never ends. **The refusal went where it cannot be forgotten**: one reader,
wrapping the wire's `Reader`, through which every floating-point field of
every message passes. A field added later is checked without anyone
remembering to, and it wraps rather than derives so that reaching past it for
something it does not forward is a compile error rather than a way round.
`src/net/protocol.hpp`'s `Reader` is untouched, because the envelope and the
input stream have their own rules about what they carry.

**The space, stated**: the seven kinds carry **nineteen** floating-point
fields between them - one in `SESSION`, three in `WEATHER` and six per
microburst, five per pressure level and three per near-ground wind in
`WEATHER_ALOFT`, one in `CONTROLLER_SWAP`. `LOBBY`, `AIRCRAFT` and
`TERRAIN_DATASET` carry none at all, and are named in the test as left out
for that reason. Each of the nineteen is overwritten with **six** patterns
that are not a number - both infinities and NaNs quiet and signalling, signed
and with a payload - and with **five** that are numbers however extreme, the
largest finite double, the most negative, the smallest subnormal and both
zeros, which must still read. **114 refused, 95 still read.** The test finds
each field by writing a distinct dense-mantissa value into it and insisting
its eight bytes appear in the encoded message once and once only, so the
layout is not written out a second time to be got wrong.

**Watched failing**: before the fix, *"session with +infinity at byte 34 must
be refused"*.

**`docs/TRANSPORT.md` says both**, where it says how values are written, what
a reader must refuse, and what the reliable layer does with an
acknowledgement; the test that holds that document to the code passes.


### Other aircraft, shown a little in the past, 2026-09-22 — item done

**What is missing first: no snapshot has ever arrived over a network.** There
is no handshake and no state update on the wire, so every snapshot these
tests use was put there by the test. What is built is the rule for showing an
aircraft from snapshots that arrive late, out of order, or not at all.

**Three behaviours, each with a stated limit.** An aircraft is drawn between
the two snapshots straddling the moment 100 ms ago. When none has arrived it
is carried on from its last velocity - marked as a guess - for at most half a
second, because half a second at 200 m/s is 100 m and that is as far as this
project will invent. When a snapshot finally arrives the difference is taken
up over a quarter of a second, because the jump is the thing a player sees.

**The bound was reasoned before it was measured, and the measurement agreed.**
Two metres: carrying on straight through half a second of a rate-one turn at
200 m/s leaves the arc by about 1.3 m. Measured over **every pattern of loss
across twelve snapshots - all 4,096** - the worst is **1.309 m**, at the
pattern that loses every snapshot but the first. Under jitter of up to 80 ms,
which puts snapshots out of order, it is 9 mm; with nothing lost, 2.9 mm.

**Two bugs in the blend, and the test found both.** The first measured the
correction from `held_.back()` - which by then is the snapshot that *ended*
the guess, not the one the guess was carried on from - and reported 20 m. The
second, after that was fixed, compared one frame's answer with the previous
frame's: those are answers for different moments, so it measured 16.7 ms of
the aircraft's own motion, 3.3 m, and called it a jump. The correction is now
worked out at one instant - what the guess would have said for this moment,
against what is now known for the same moment - and the error fell to 24 mm
in that case.

**And one wrong measurement of mine.** The first run reported 10 m and I was
about to raise the bound; the 10 m was the aircraft being held at the oldest
snapshot for moments *before* any snapshot covered them, which is a startup
artefact and not interpolation error. The test now measures from the first
moment its snapshots cover, and the startup behaviour is held by its own
test.


### Every network parser fuzzed, 2026-09-22 — item done

**What is missing first: nothing has ever reached these parsers over a
network.** There is no handshake, so every byte they have seen was put there
by a test. What is proved is that none of them can be made to read past the
end of what it was given.

**Eleven parsers, eighteen seeds, and every seed through every parser.** The
parsers are the envelope, the seven reliable messages, the kind byte, the
reliable layer and the input receiver. The seeds are every datagram and
message this project writes plus five it never would - an empty datagram, one
byte, sixty-four bytes of `0xFF`, an HTTP request and 1,232 bytes of `0xA5`.
**Cross-feeding is the point**: a datagram arrives before anybody knows what
it is, and a parser only ever shown its own output has not been tested at all.

**188,617 reads, none out of bounds.** Each seed whole, cut to every length,
changed a byte at a time at six values, and then mutated from a fixed seed -
bytes changed, dropped, inserted, and two seeds joined - so that a failure can
be had again.

**It is under sanitizers by construction.** The build compiles with
`-fsanitize=address,undefined -fno-sanitize-recover=all`, so a read past the
end ends the test rather than printing and carrying on, and ctest runs it in
CI with everything else. The corpus is written to `build/fuzz-corpus` so it
can be handed to libFuzzer or AFL rather than only living inside the test.

**Watched failing**: a `kind_of` that takes `body[0]` without checking there
is a byte is caught on the empty seed, with UBSan reporting a reference bound
to a null pointer and the test exiting non-zero.

**A false alarm I raised against my own rig.** I first read that exit code
through a pipe into `tail`, got `0`, and concluded the sanitizers were not
failing the test - which would have been a hole in the whole project's test
build. They were: `$?` after a pipe is the last command's, not the test's.
The flags were right all along.


### A client's inputs, streamed with redundancy, 2026-09-22 — item done

**What is missing first: nothing sends them.** There is no handshake, so no
client is connected and no input packet has ever crossed a socket. What is
built is the stream itself, where it can be walked against every pattern of
loss rather than against a network that happens to be working.

**Repeated, not retransmitted.** A lost input frame is worth nothing a moment
later - the aircraft has moved on - so waiting for an acknowledgement would
deliver it too late to use and cost a round trip to find out. Every packet
carries the last four frames instead, at 141 bytes.

**The bound is stated, because "no loss ever" is not true.** A frame rides in
the packet of its own number and the three after it, so **it arrives if and
only if at least one of those four arrives**. That is the invariant, and it is
held frame by frame over all **4,096** patterns of loss across twelve packets:
1,490 lose nothing at all, 2,606 lose at least one.

**The first shape of that test was wrong, and it caught itself.** It claimed
nothing is lost unless four packets go in a row - true in the middle of a
stream, false at its end, because losing only the last packet loses the last
frame, which no later packet carries. Pattern 2048 found it. The rule above
has no such edge.

**Quantised, and the client flies what it sent.** Each control goes as a
16-bit fraction of -1 to 1: both ends exact, a step of 3e-5, a quarter of a
double's size. The client predicts on the rounded value rather than on what
its stick said, because a prediction fed different inputs from the server
would drift for a reason no measurement could explain - which is exactly the
drift this project promises to bound.

**`src/net/` still includes nothing but the standard library.** The wire does
not know what a control is: `sim::Controls::as_list` hands seventeen numbers
over and `from_list` takes them back, so the list lives with the flight model.
`every_control_the_flight_model_has_is_one_the_wire_carries` holds
`sizeof(sim::Controls)`, so a control added without being put in `as_list`
fails rather than being quietly left out of every flight.

**Watched failing**: a receiver that keeps only the newest frame, throwing the
redundancy away, is told "frame 1 did not arrive but a packet carrying it did
get through".


### The server's AI aircraft, 2026-09-22 — item done

**What is missing first: they all fly one plan.** There is one flight plan in
the data, a tour of Sydney Harbour, so every AI aeroplane flies it. They are
stacked 500 ft apart rather than put in one piece of sky, which says what is
happening instead of hiding it. More plans, and aircraft that fly different
ones, are not built.

**An AI aeroplane is an aeroplane with its controller set to the AI.** It goes
through `sim::Controller::to_ai(plan)`, the same path a player's aircraft
takes when it is handed over, and its controls each step are what
`Controller::fly()` returns - the autopilot steered by the navigator. Nothing
on the server bypasses that, which is what keeps "the LLM plans, the
controllers fly" true at this end.

**Verified**,
`the_server_runs_the_number_of_ai_aircraft_it_is_given_and_four_when_given_none`:
asked for two it runs two, asked for one it runs one, and asked for nothing it
runs four. The count is read back out of the server's own report and cross-
checked against the number of aeroplanes it says it flew, so a server that
printed a number and made a different number would fail. **Watched failing**:
one that ignores `--ai` and always makes four is told "asked for 2 AI aircraft
and it ran 4".

**Seen flying**: three AI aircraft at 2,933, 3,429 and 3,929 ft over ground at
72 ft off Bondi, each tracking north up the plan's first leg.

**A CMake list cannot hold a list.** The test's cases were written
`"2;2" "1;1" ";4"` and flattened into six elements, so every case read one
number and the first comparison failed. They are `"asked:want"` now.


### The server flies aircraft anywhere on Earth, 2026-09-22 — item done

**What is missing first: nobody is flying them.** The server owns the
aircraft and steps them, but no client can connect and no input reaches a
control surface, so they fly at a fixed throttle and nothing else. What is
proved is that the server simulates at a fixed rate against the real
collision terrain wherever an aircraft is, not that anybody can fly one.

**Two aircraft, Sydney and Denver, in one session.** `--fly ID@LAT,LON` gives
the server an aeroplane and where it starts, up to the four a session holds;
`--seconds N` stops it after a set time, which is what lets a test watch a
whole run rather than kill one. `--port 0` asks the system for a free port and
the server prints the one it got, so two tests never fight over a number.

**One DEM, one thread.** A `Fleet` owns a single `world::Dem` and an aircraft
per `--fly`, each with terrain over that DEM. The DEM caches tiles as it goes,
so aircraft on opposite sides of the world each pull their own and neither
waits for the other's; every aircraft is stepped on the one thread, because
`PROJECT_STATUS.md` has said since the DEM was written that it is not
thread-safe. Stepping is `sim::FixedStep`: every step that has become due is
taken, so the rate is the simulation's and not the loop's.

**Verified**,
`the_server_flies_aircraft_on_opposite_sides_of_the_world_each_over_its_own_terrain`:
the two flew **2.003 s in 240 steps - 120 Hz exactly** - and found ground at
**77 ft** and **5,183 ft** above the ellipsoid. Those are the right numbers:
Sydney Airport is a few feet above the sea with the geoid some 70 ft above the
ellipsoid there, and Denver is 5,280 ft above the sea with the geoid about
56 ft below it. Each is held to a band that place really has, so a server
flying both over one terrain fails both bands at once.

**The test was watched failing.** Given a flat terrain at the ellipsoid, it
reports "the aeroplane over Denver found ground at 0 ft, which is outside the
4500 to 6500 ft that place has: it is not over its own terrain". Reverting the
bug makes it pass.

**A contradiction of my own making.** The server refused `--port 0` - "name a
port" - while the test that flies it asked for 0, so the verification skipped
itself rather than running. A skipped verification is not one, so `--port 0`
is now allowed and documented as what a test wants.


### Slots, and who gets which, 2026-09-22 — item done

**What is missing first: nobody can be admitted yet.** There is no handshake,
so no key ever reaches the server and every session is empty. What is built
is the rule about slots, and it is built where it can be walked exhaustively
rather than watched over a network.

**A slot is a key's rank, not an arrival.** `REQUIREMENTS.md` 6.5 says the
server decides who is which player, "not whoever connected first", so slots
cannot be handed out in order of arrival. A player is known by a key - the
static public key the handshake will use - and their slot is that key's rank
among those in the session. The assignment is therefore a function of who is
present, not of the sequence, which is exactly what the item asks for.

**What that costs, said plainly.** A slot is a property of the set, so
somebody joining can move somebody already in: admit a key that sorts first
and everybody after it shifts down one. That is the price of an assignment
that does not depend on arrival order. The lobby is sent whole for this
reason - a client is told its slot rather than remembering it.

**Verified exhaustively**, `slots_are_the_same_whatever_order_the_players_connect_in`:
every non-empty subset of four players and every order each could arrive in -
**fifteen subsets, 64 orders** - all ending with the same person in the same
slot. The four test keys are **deliberately shuffled against their names**,
so an implementation that assigned by arrival, or alphabetically, would fail.
Five more tests hold the rank rule itself, fullness at each player count 1 to
4 with the next refused, a key admitted twice being one player, the 1 to 4
clamp, and the lobby message round-tripping through the wire format.

**The server draws its dashboard from it**, so the rows on screen are the
rows a `LOBBY` would carry rather than a count of empty seats.

**A name collision caught by the compiler, not by a test.** The class was
first called `net::Session`, which is already the `SESSION` message's struct.
It is `net::Slots` now, which is the requirement's own word.

### `THREATS.md`, 2026-09-22 — item done

**It leads with the fact that the server accepts nothing yet.** There is no
handshake and no sealing, so `glideslope_server` binds a port, counts what
arrives and drops every datagram unread. Every defence in the document is
marked built, unwired or not built, so that none of it reads as protection
that is presently doing anything: the envelope reader is built but **unwired**
- `read_envelope` is called by its test and by nothing else in `src/`.

**It names all four datagram types and all seven reliable messages** with the
defence each has, citing the limits the code enforces rather than describing
them in general terms, and says which of the seven a server would accept from
a client at all - only `CONTROLLER_SWAP`; the rest are server-to-client, which
materially narrows the surface. Amplification, replay, an unauthenticated
datagram, exhaustion of the reliable layer's queues, a client claiming a slot
or a player count, and a wrong terrain dataset each have a section.

**Writing it found six disagreements between the code and the documents**,
which is what writing it was for. Four were fixed the same day: the oversized
`WEATHER` above; a turbulence severity byte free to carry anything when the
flag before it said there was none; a slot index with no range check in three
messages; and a server receive buffer of 1500 bytes where a datagram is 1232.
Two were not, and are items in `COMPLETION_PLAN.md`: the reliable layer
believing a forged acknowledgement, and no message rejecting a NaN.

**Verified**, `every_message_the_server_accepts_is_named_in_the_threats_document`:
every message kind the code knows is named in the document, and the count is
held against the code's own list so a new kind cannot be added without being
named in both.


### The server binary and its flags, 2026-09-22 — item begun, not done

**What is missing: three of its six flags drive nothing yet.** `--store`
names a file nothing is written to, because there is no session to keep.
`--key` is checked to be 64 hexadecimal characters but nothing seals
anything, and this build cannot mint one: a server secret is an X25519 static
key and there is no libsodium here to derive its public half with, so a
secret whose public half cannot be printed would be of no use to a client.
`--timeout` cannot let a silent client go, because no client can connect
until the handshake exists. The dashboard says so on screen rather than
showing an empty table that looks like a fault.

**What works.** `glideslope_server` takes every flag the item names, checks
each, binds its port and draws a dashboard that refreshes each second: the
version, the port, how long it has been up, the datagrams and bytes it has
seen, and a row per slot with who is in it, their ping and their traffic.
`--headless` prints the settings and runs without one. `--dry-run` prints the
settings and exits without binding, which is what lets a test exercise a flag
without a server that has to be killed.

**It links nothing presentational.** The dashboard is written to a terminal,
not drawn, so the server links the simulation, the world and the platform and
no SDL. It does not link the transport yet either, because it does not speak
to anyone yet.

**Verified**, fourteen tests: `--version` and `--help`; the defaults with no
flags; **every one of the six flags together in one run**; both ends of the 1
to 4 player range and both sides of them, which is the item's own
verification; a key of the wrong length and a key that is not hexadecimal; a
port that is not one; a timeout that is not a length of time; a flag with
nothing after it; and an option it does not know.

**Two things CMake does with a semicolon.** `-DARGS=--players;0` does not
pass three arguments - `separate_arguments(... NATIVE_COMMAND)` splits on
whitespace, so the whole thing arrived as one argument named
`--players;0;--dry-run`. And a semicolon inside an expected pattern is a list
separator too, so `--players is 0; a session is 1 to 4` became two patterns.
Nine of the fourteen tests failed on those two until the arguments were
separated by spaces and the server's own messages written without semicolons.


### The reliable messages, 2026-09-22 — item done

**What is missing first: nothing carries them yet.** There is no server and
no handshake, so these messages are written, read and delivered between two
endpoints in a test and nowhere else. What is proved is that the six exist,
that they survive loss, and that a third party could write them from
`TRANSPORT.md`.

**The six the item names now exist**: the lobby, the session, the weather, an
aircraft's definition, the terrain dataset and a controller swap. Each body
begins with one byte saying which kind it is. `src/net/messages.hpp` defines
them and `docs/TRANSPORT.md` writes every field of every one out byte for
byte.

**There are seven kinds, because the weather is two of them.** The first cut
made the weather one message carrying the METAR and the forecast above it,
with limits of a 1024-byte report, 64 pressure levels, 8 near-ground winds
and 32 microbursts. At those limits its body was **5,419 bytes against the
1,218 a datagram leaves** once the envelope and the reliable header are in
front of it - 4,201 over - and even at the figures this project really
fetches, nineteen levels and four winds, a report with a single microburst
came to 1,235. Nothing fragments, so such a message could not be sent at all:
the socket refuses it and the reliable layer would repeat it for ever. The
report now goes as `WEATHER` and the forecast as `WEATHER_ALOFT`, the limits
came down to 256 bytes, 24 levels, 4 winds and 16 microbursts, and
`every_message_filled_to_its_limits_fits_in_one_datagram` fills every message
to its limits and holds it to the datagram. The largest is now the forecast
at **1,093 of 1,218**, and the forecast actually fetched is 877.

**`src/net/` still includes nothing but the standard library.** The wire
structures are the wire's, not the simulation's - turning a
`world::WeatherReport` into a `net::Weather` is the server's job. That is what
lets the whole transport be tested without a socket, a terrain tile or a
flight model, which is how it can be put through 4,096 loss patterns.

**The weather is sent as what it was made from.** A METAR is a line of text
with twenty optional fields; re-encoding them is twenty chances for two ends
to disagree. The raw report goes on the wire and the receiver parses it with
the same code the sender did. The forecast above it has no such text, so its
levels go as numbers.

**Verified:**

- `every_message_writes_and_reads_back_what_went_into_it` - seven kinds, every
  field set to something that is not its default, so a field the writer forgot
  cannot pass.
- `no_message_reads_as_a_kind_it_is_not` - all **49 pairs**; the 42 that are
  not a kind reading as itself are refused.
- `every_truncation_of_every_message_is_refused` - **465 prefixes**.
- `every_message_with_anything_trailing_is_refused` - nothing can be hidden
  behind a message.
- `every_single_byte_change_to_every_message_is_read_or_refused` - the whole
  space, **118,575 changes**: 108,141 still read (most bytes are a double's,
  where another value is another valid double) and 10,434 are refused. None
  crashes and none reads as a kind it is not.
- `the_message_kinds_and_controllers_are_the_ones_the_document_names` - seven
  kinds and three controllers, and 249 of the 256 possible first bytes are no
  kind at all.
- `every_reliable_message_arrives_exactly_once_and_in_order_under_loss`
  - **the item's own verification**: all seven real messages through the
  reliable layer under all **4,096** patterns of loss over twelve datagrams.
  In each one every message arrived exactly once, in order, and read back as
  the message it was written from. The worst pattern took 21 datagrams.
- `the_transport_document_and_the_code_agree_about_the_messages` - 7 kinds,
  3 controllers and 8 limits read back out of `TRANSPORT.md` and held against
  the code. It was watched failing when the limits changed and the document
  had not, which is what it is for.

**A build trap worth knowing.** `glideslope_net` lists its sources rather than
globbing them, so `cmake --build --target glideslope_net` succeeded without
compiling `messages.cpp` at all, and only the link of the tests found it.


### Every visual model stands on the ground, 2026-09-22 — tail done

**The alignment's height was fitted, and the fit could buy accuracy by
burying the aeroplane.** Where a mesh and its flight model disagree about
where the undercarriage is - the 747-400's model has four bogies where its
flight model has three legs, so no fit of three to four can be right - sinking
the model brings its contacts nearer the mesh on average. Measured against the
ground: the 747-400 was drawn 2.15 m under it, the B-2 1.67, the F-22 0.86 and
the Mosquito 0.69.

**The height is now anchored, not fitted.** A model drawn with its
undercarriage down has a tyre as its lowest point, and an aeroplane standing
on a runway has every wheel on the ground together, so that point belongs at
its lowest wheel contact. There is nothing to fit. Only x is fitted now; y is
nought by symmetry.

**What it cost and what it bought.** The `on` column - the worst distance from
a wheel contact to the mesh - grew, because it now reports the real
disagreement instead of hiding it: the 747-400's from 2.479 m to 3.402. The
`at` column, the worst distance from an airframe contact to the mesh, fell to
**0.002 m** for all four aeroplanes whose airframes were measured from their
meshes, where the 747-400's had been 2.155. Two independently computed things
now agree to 2 mm, because they finally share an anchor. No x moved, and no
airframe contact moved.

**Verified**, `every_visual_model_stands_on_the_ground_rather_than_in_it`: all
fifteen models sit within a centimetre of their lowest wheel contact, which is
the alignment file's own rounding. **The failing case was measured rather than
run**: with the previous alignment the same quantity was 2.15 m for the
747-400, far outside the centimetre this holds, but the test binary was not
run against it.


### The F-35A becomes the F-35B, 2026-09-22 — item done

**What is missing: it cannot hover, land vertically or take off short.** The
F-35B is a STOVL aeroplane and its LiftFan, swivel nozzle and roll posts are
what make it one. None of that is modelled. It flies its wing, weights and
engine in conventional flight and uses a runway like any other fighter, and
the STOVL work is a tail in `COMPLETION_PLAN.md`.

**Why the variant changed.** The F-35A was the only aircraft with no visual
model and the only one still falling through the runway with its wheels up,
and both had one cause: FGAddon has no F-35A. It has an F-35B, under a
verbatim GPL-3.0, whose model draws its undercarriage — which is what
`tools/ground.py` needs to measure an airframe. The project owner chose the
variant.

**The figures are the B's own, read from the documents rather than a
summary.** Lockheed Martin's F-35B product card (©2023, 23-08442_002, PIRA
AER2023060207) and the April 2020 Fast Facts (FG19-24749_004) were fetched and
their text extracted here; `ASSETS.md` records both with their SHA-256.

| | F-35A | F-35B |
|---|---|---|
| Length | 51.4 ft | 51.2 ft |
| Empty weight | 29,300 lb | **32,300 lb** |
| Internal fuel | 18,250 lb | **13,100 lb** |
| Engine | F135-PW-100 | **F135-PW-600** |
| Thrust | 43,000 / 28,000 lb | **38,000 / 26,000 lb** |
| Range | >1,200 nm | **>900 nm** |
| Max g | 9.0 | **7.0** |

**Two conflicts between primary sources, resolved deliberately.** Internal
fuel is 13,100 lb on the product card and 13,500 lb in Fast Facts; the product
card is the later Lockheed publication and the B's own, so 13,100 is used.
Thrust is 38,000/26,000 lb on the product card and 40,000/25,000 lb in Fast
Facts — but Fast Facts prints that same pair for the A, the B and the C,
labelled "uninstalled thrust ratings", so it is a family figure and not the
B's; the product card's is used.

**No ceiling is claimed, and the ceiling test is gone.** Lockheed publishes no
service ceiling for any F-35. The "above 50,000 feet" the F-35A was held to is
the Air Force's fact sheet, for the A alone, so the F-35B is held to its
maximum Mach and its range and to nothing else. The figures file and the test
file each say so where the figure used to be.

**What had to be retuned, and how it was measured.** On 38,000 lb of thrust
and 3,000 lb more empty weight, the A's drag numbers gave Mach 1.35 against a
published 1.6. The wave drag is the number this project sets to fly the
figure, as the script's docstring says. Measured: 0.066 → Mach 1.35, 0.060 →
1.48, 0.056 → 1.56, 0.052 → 1.64, and **0.054 → 1.609 against the published
1.6**. The range figure did not move with it, because that flight is level at
Mach 0.8 where there is no wave drag — so the two were set independently.

**The visual model, and one thing left out of it.** FGAddon's F-35B at the
same revision 21588 every other model is pinned at: `F-35B.ac`, `Engine.ac`
and `Gear.ac`, 20,649 vertices. Converted whole it came out **10% longer than
the published aeroplane** — an `antennas` object holding a nose air-data boom
reaching 1.5 m past the radome. With it left out the mesh is 15.64 m against a
published 51.2 ft, **+0.2%**. Its span is 3.6% over, which is its wingtip
navigation lights; the wing alone is 1.6% over, so the lights stay on the
model and the check states the reason.

**Two counts the switch moved, and both were assertions that caught it.** The
figure count fell from 94 to 93 when the ceiling went, and the visual-model
size test had no published dimensions for an aeroplane it had never had a
model for. Neither failed silently: each named exactly what had changed, which
is what those counts are for.

**The emptied list was watched failing.** `awaiting_a_model` is the list of
aeroplanes allowed to go through the runway, and it is now empty. Putting
`f35b` back into it turns the test red - "f35b no longer does, so the list
above is stale" - so the guard catches a stale entry as well as a real
regression, and both directions have been seen.

**Verified locally**: it flies **Mach 1.609** against its published 1.6 and
ranges **1,372 nm** against more than 900; its model sits **0.31 m** from the
contacts it stands on, better than most of the fleet; and it comes to rest on
its airframe **1.4 ft above the runway** with its wheels up, which emptied the
list of aircraft that do not. A full `ctest` run stands behind it; the
platform-wide figures wait on the next CI run.


### An airframe to land on with the wheels up, 2026-09-22 — item done

**All five aeroplanes that had nothing to land on now have an airframe.** Four
were done here; the fifth, the F-35A, could not be, because no F-35A flight
model carrying airframe contacts exists anywhere and FGAddon has no F-35A to
measure a mesh from. It became the F-35B the same day — its own entry below —
and that closed this item.

**The problem.** JSBSim gives a retracted wheel no force. An aeroplane whose
only contacts are its undercarriage therefore has nothing at all between it
and the ground once the gear is up: landed wheels-up it passes through the
runway and keeps going. Five of the sixteen were in that state — the 737-300,
the 747-400, the B-2A, the F-22A and the F-35A.

**The source: the visual mesh, not a flight model.** The hunt for JSBSim
models carrying these points ran over FGAddon, FGMEMBERS, JSBSim's own
aircraft, the Naval Postgraduate School's mirror and arktools's, and turned up
exactly one usable belly — FlightGear's 737-300. This project already ships a
visual mesh for fourteen aircraft, and a mesh *is* the airframe. `tools/ground.py`
measures the points off it.

**Why it needs no alignment, which matters because the alignment is poor.**
`assets/models/alignment.txt` records where a mesh sits on its flight model,
but the height in that fit carries a residual of up to 2.48 m — the size of
the very number wanted. So height is not taken from it. Every mesh draws its
undercarriage extended, so the mesh's lowest point is a tyre on the ground,
and a point's height *above the tyres* is a difference within one mesh, in
which the alignment offset cancels exactly. That difference is added to the
flight model's own wheel contact. Only x and y come from the alignment, where
a foot of error slides a belly point along the belly and changes nothing.

**Telling the undercarriage from the belly.** The lowest thing at a gear
station is the wheels. The legs cannot be found by looking near the flight
model's own contacts — the B-2's mesh draws its nose leg 100 in forward of
where its flight model puts the nose wheel — so they are found from the shape:
the lowest point is taken at 48 stations along the keel, the typical belly
height is the median over the flat run of it, and any station lower than half
of that is over the undercarriage and is dropped.

**The B-2 read 100.9 in where its belly is 53, and it took two guards to see
why.** Measuring across the whole fuselage width rather than a narrow keel
picks up the point where a flying wing's body blends up into the wing; and the
B-2's mesh carries 6,875 vertices for a 52 m aeroplane, at which density a
station can hold two vertices whose lowest is wherever they happen to be, so
the stations that would have contradicted it were themselves nonsense. Either
guard alone brings the reading back to 56.4 in. Both are kept, deliberately
redundant, because they guard different things — and this was measured rather
than assumed: a deliberate widening of the band no longer reddens the check,
which is how the redundancy is known to be real.

**What it measures.** Belly height above the wheel contact:

| aircraft | belly | mesh length vs published | mesh span vs published |
|---|---|---|---|
| 737-300 | 50.3 in | 33.30 m vs 33.40 (+0.3%) | 28.89 m vs 28.88 (+0.0%) |
| 747-400 | 106.5 in | 70.94 m vs 70.66 (+0.4%) | 65.42 m vs 64.44 (+1.5%) |
| B-2A | 53.0 in | 21.21 m vs 21.03 (+0.8%) | 52.20 m vs 52.43 (−0.4%) |
| F-22A | 38.7 in | 18.92 m vs 18.92 (+0.0%) | 13.56 m vs 13.56 (+0.0%) |

**Checked against the one independent answer there is.** FlightGear's own
JSBSim 737-300 puts its belly 43.7 in above the wheel contact. Measured from
the mesh, with no reference to that model, it comes out 50.3 in: **the two
agree to 6.6 in on a 33 m aeroplane**, which is what makes the same method
trustworthy on the three that have no second opinion. `python3 tools/ground.py
--check` holds that agreement to 10 in, holds each mesh to within 1.5% of its
published length and span, and fails if any airframe contact sits at or below
the wheels, where it would drag on every ordinary landing.

**A second fault, found by the same test: the A320 rolled along on its wing
tips.** Its nine contacts that never retract — wing tips, nacelles, nose, tail
— are airframe, but carried JSBSim's rolling friction of 0.02, a tyre's,
because the model it was made from left it there. JSBSim tells a wheel from a
wing tip by nothing but what the model says. Landed wheels-up the A320 was
still doing 118 knots after three minutes and had run 10,962 m without
stopping. This project now states one number for an airframe scraping a
runway — **0.4, aluminium on dry concrete** — and the A320 stops in 1,332 m.

**How hard an airframe is.** A belly that sinks into concrete puts the centre
of gravity under the runway, which is what the 737 did at first: its contacts
were a tenth of the gear's spring and it settled 1.17 ft under. The spring is
now set so the whole aeroplane's weight on one point gives a quarter of a
foot, with damping critical for it so it settles rather than rings.

**Verified**, `every_aircraft_put_down_with_its_wheels_up_rests_on_its_airframe`:
fifteen of the sixteen flown onto a runway at 0.6 of their approach speed
with the gear up and the engines closed. The four that fell through now rest — the
747-400 at +4.90 ft, the B-2 at +6.73, the F-22 at +1.29, and the A320, which
never stopped, now stops. The five whose wheels do not retract are flown too
and held to a different thing, both named in the test: they have no wheels to
raise, so they land on them and free-roll, and are asked not to go through the
runway rather than to stop. The flying boat is the one aircraft left out, with
its reason. **The test was watched failing** on nine aircraft before any of
this, and again on the count when the Mosquito's gear lever turned out to move
an indicator while none of its contacts retract.

**Tails found on the way**, both in `COMPLETION_PLAN.md`: the visual
alignment's height is badly fitted for three aircraft, which is still open;
and the F-15C's airframe contacts carried the old 0.2 friction on a spring of
10,000 lb/ft and slid 2,734 m, which was fixed the same day - it now stops in
1,029 m where the stated friction gives 1,093, and rests 2.9 ft above the
runway instead of 0.35 ft below it.


### Reliable delivery over an unreliable channel, 2026-09-21 — item begun, not done

Phase 6's second item. **The layer is built and its verification is met; the
messages it is for do not exist yet.**

**How it works**, in four sentences. Every reliable message is numbered, from
one, and carries its number. The sender keeps a message until it has been
acknowledged and sends it again if it has not been, no faster than once every
quarter of a second. The receiver hands messages up in number order, holding
one that arrives early until its predecessors have come and throwing away one
that arrives twice. The receiver answers with the highest number below which
nothing is missing, which acknowledges that one and every one before it at
once.

**An endpoint with nothing to say still answers.** Without that the far end
retransmits for ever at a receiver that already has everything - so an
endpoint that owes an acknowledgement and has nothing of its own to send
sends a header with message number 0, which is an acknowledgement and nothing
else.

**It is bounded at both ends.** A sender that gets 256 messages behind
refuses rather than queueing for ever, and a receiver stuck behind one
missing message holds 256 and no more. Neither is a window in the congestion
sense: these messages are few, small and occasional, and the channel below is
a game's, not a file transfer's.

**Nothing here touches a socket**, which is what lets it be tested against
every pattern of loss rather than against a network that happens to be
working.

**Verification run.** Seven tests. The item's own -
`under_every_pattern_of_loss_every_message_arrives_exactly_once_and_in_order`
- walks **all 4,096 patterns of loss over twelve datagrams in both
directions**, and in every one of them all six messages arrived exactly once
and in order. It states how many patterns there are, how many actually lost
something (4,064: an exchange of six messages is over in seven datagrams, so
the 32 patterns setting only bits 7 to 11 never touch anything), and what the
worst pattern cost in datagrams, so that a change making delivery far more
expensive shows up here rather than nowhere. Watched to fail with
retransmission taken out: "with loss pattern 1 of 4096, 0 of 6 messages
arrived and it never finished". 319 of 319 tests pass locally at
`-j4`, in 1006 s.

The other six hold the ordinary cases: nothing lost; a message delivered
twice handed up once; three messages arriving before the one they follow,
held and then handed up together; an endpoint answering with nothing to say
and then going quiet; four kinds of rubbish off the wire handed up as
nothing; and a sender refusing at 256.

### UDP, on both kinds of system, 2026-09-21 — groundwork, no item of its own

**`src/platform/` had no socket code on any platform; now it has UDP on
both.** BSD sockets on Linux and macOS, Winsock on Windows, one header and
two implementations, and nothing above knows which it has. Like the take-off
autopilot this has no item in the plan - Phase 6 needs it and nothing else
does.

**Nothing blocks.** A socket is non-blocking from the moment it is made:
`receive` answers at once with what was waiting, or with nothing. The
simulation steps at a fixed rate and cannot wait on a datagram that may never
come. **Nothing throws** either: a socket that cannot be made is an empty
optional, and a send or receive that fails says so.

**An address is bytes, not a name.** Nothing here resolves a host name -
that is a blocking call into the system's resolver, and it belongs where
waiting is allowed - so `localhost:26000` is refused as firmly as
`256.0.0.1:1`. The parser and the printer are shared by both systems and use
no system headers at all, so the two cannot drift.

**A datagram is at most 1232 bytes.** IPv6 obliges every path to carry 1280;
40 are its header and 8 are UDP's. Nothing this sends is fragmented, and more
than 1232 bytes is refused rather than broken up. One that arrives too long
for the buffer is dropped rather than cut, because half a datagram is not a
datagram.

**Winsock has to be started before it can be used**, once per process, and
stopped as many times as it was started; a counter does that at the first
socket and undoes it with the last, so nothing above has to know.

**Verification run.** Five tests:
`an_address_written_down_and_read_back_is_the_same_address` walks nine
addresses of both families, each written, read back, and written again;
`anything_that_is_not_an_address_is_refused` walks twenty-three things that
are not one, host names among them, each with its reason in the test;
`a_datagram_sent_to_the_loopback_arrives_whole_and_says_where_from`;
`a_datagram_too_large_for_the_smallest_path_is_refused`; and
`a_socket_gets_the_port_it_asks_for_and_no_two_share_one`. Watched to fail
with the size guard taken out of `send`: "a datagram of 1233 bytes is
refused". **The Windows implementation is written and has never been run
here** - this machine is WSL - so CI's two Windows jobs are the first thing
that will have compiled it. 312 of 312 tests pass locally at `-j4`,
in 972 s.

### What GCC cannot see, now seen, 2026-09-22 — tail done

**Both Linux builds are GCC, and GCC does not warn about an unused constant
at namespace scope in C++** - not under `-Wall -Wextra`, and not under
`-Wunused-const-variable` at any level, which it honours for C alone. Clang
does, so that class was invisible here and reddened macOS and Windows
clang-cl together on a commit both Linux jobs had passed.

`tests/cmake/clang_warnings.cmake` closes it by re-running the compile
commands CMake already exports - under clang, syntax only, with the same
flags the build used and the unused-constant warning added. Same flags, same
files, so a warning here is a warning the other compiler would give. 93
first-party sources in 100 seconds; `ext/` is somebody else's and is left
alone, and where clang is not installed it reports itself skipped.

**Watched both ways**, which is the point of it: a constant nobody uses put
into `checklist.cpp`, and GCC built the whole project **without a word**,
while the test failed with "unused variable 'nobody_uses_this'
[-Werror,-Wunused-const-variable]".

**What it does not compile**, with its reason: flags GCC has and clang has
not - its module mapper, its dependency format, and a suppression named for a
GCC warning - are dropped; `-o` and its argument go, and the dependency
files; Objective-C++ is macOS's own. Getting `-c` wrong is how the first run
failed: `-c` takes no argument, the source merely follows it, and dropping
what followed dropped the input, so clang said "no input files" 93 times.

### Why the shared Cesium cache locks, 2026-09-22

Read out of Cesium Native rather than guessed at. **It is not readers
blocking writers**, which was the obvious guess: `SqliteCache` already turns
on WAL - `PRAGMA journal_mode=WAL` at `SqliteCache.cpp:58`, run at line 214 -
and that is the part that would otherwise hurt.

**It is that no busy timeout is ever set.** The connection is opened with
`sqlite3_open` and there is no `sqlite3_busy_timeout` or
`sqlite3_busy_handler` anywhere in the file - the word "busy" does not appear
in it. SQLite's default timeout is zero, so a second *writer* - a second
process sharing the cache, which WAL does not serialise - gets `SQLITE_BUSY`
on its first attempt instead of waiting a moment, and the store fails. That
is what "database is locked" in our test logs has been all along.

Their fix is one line after the open, and it is drafted as a third issue in
`docs/cesium-issues.md`.

**Ours was to stop the tests sharing one file**, and that is done.
`GLIDESLOPE_CESIUM_CACHE` names the database, `platform::cesium_cache_file()`
reads it, and `tests/cmake/client.cmake` - which all thirteen client tests
include - gives each a name of its own made from the script running it and
the things that tell its runs apart. They stay in the downloads directory, so
each test still finds its own cache on the next run.

**The verification is enforced from that one place too.** A locked cache
loses nothing but the caching, which is why it went unnoticed for so long, so
`glideslope_judge_leaks` now fails any run that reports one. Every client
test calls it, directly or through `glideslope_client`.

**It costs nothing, measured rather than assumed.** The worry was that
isolating the caches would mean fetching the same tiles for each test: 323
tests in 973 s, against 977 to 1,006 s for the same suite before. Writes now
succeed instead of failing and the tile being fetched again, which appears to
pay for the loss of sharing.

### Why WinHTTP's decompression failed, 2026-09-22

**The first attempt turned the option on, watched every Open-Meteo fetch
fail on all three Windows jobs, turned it off, and wrote "why is not known"
beside it.** The reason was in Microsoft's documentation, and reading it
rather than guessing again found it.

**The body was read by asking how much was there.**
`WinHttpQueryDataAvailable` was called, and the loop stopped when it answered
none. Its own documentation says not to do that: *"Do not use the return
value of WinHttpQueryDataAvailable to determine whether the end of a response
has been reached, because not all servers terminate responses properly."* It
is also not the decompressed length when WinHTTP is undoing an encoding, so
turning decompression on was the thing that exposed a loop that was already
wrong. The documented pattern is fixed chunks - it advises 8 KiB or more -
read until a read returns no bytes, which is the end of a body as
end-of-file is the end of a file. That is what it does now, and it is right
whether anything is being decompressed or not.

**And `WinHttpSetOption`'s result is checked**, which it was not before. An
option that did not take is an option that quietly does nothing, and the body
would arrive compressed with nothing to say so; where it cannot be set -
it wants Windows 8.1 - `content-encoding` is left on the response, which is
what tells a caller the bytes are not what they look like.

**The number in the old note was a red herring.** "WinHTTP saying
2147500036" is 0x80004004, E_ABORT, which is not in WinHTTP's error range at
all.

**And CI said the diagnosis was wrong.** Turned on a second time with the
loop fixed, every Open-Meteo fetch failed again on all three Windows jobs -
but this time with a real WinHTTP code instead of a stale one: **12002,
`ERROR_WINHTTP_TIMEOUT`, at `WinHttpSendRequest`**. That is raised before a
single byte of the body is read, which is earlier than the read loop runs, so
the loop was never the cause. Asking for a compressed body from that host,
from those runners, times out the send; why is still not known.

The option is off again - a red tree is the stop-everything condition - and
the read-loop fix stays, because it is right on its own account whatever
WinHTTP is asked to decompress. What has been gained is a symptom worth
having: a known code at a known call, instead of "2147500036, why is not
known". The next attempt starts from the send, not the read.

**What CI can and cannot answer.** The weather half of the tail's
verification is exactly what failed, and `tests/unit/test_weather.cpp`
fetches Open-Meteo on the Windows jobs, so CI is real evidence for it - it is
what produced this diagnosis. The ion half needs a token no CI job has.

### The propeller and mixture levers a pilot can reach, 2026-09-21 — tail done

The tail found making the Short S.23: the controls had a propeller lever and
a mixture that the figure flights set, and no key or binding worked either,
so a pilot flew every aeroplane full rich with its airscrews at their
highest. The S.23 turned 3,185 rpm and gave 1,185 hp where its Pegasus is
rated at 2,600 and 920.

**`propeller` is now a control a binding can name**, beside the mixture that
already was one. Every input of both devices was already bound - the file
says so and a test walks all 56 - so this is a rebinding, not an addition:
**the quadrant's three levers now sit in the order the hand finds them**,
throttle, propeller, mixture, on its first three axes, and the rudder comes
off the quadrant, where a rudder does not belong. Two buttons on each device
step the propeller.

**The keyboard works them too**: the comma and full stop for the mixture, the
square brackets for the propeller, each moving at the throttle's rate. **A
lever stays where it is left**, as the throttle does - one that sprang back
to rich when the key came up would be no use for leaning.

**The keyboard moved house to be testable.** It was a class inside the
client's `main.cpp`, asking SDL for the key state itself, and the tests link
no client code. It is `platform::KeyboardControls` now, given the key state
rather than fetching it, which is where the joystick mapping already lives
and is what let the keys be held to the same standard as the bindings.

**What it is worth, measured:**

| Aeroplane | Levers | Engine |
| --- | --- | --- |
| Short S.23 | Coarse pitch, mixture at NORMAL | **2,169 rpm**, inside its rated 2,600 |
| Short S.23 | Fine pitch, mixture through the gate | 2,871 rpm - the take-off setting, and now a choice |
| Mosquito FB VI | Lever fully forward | 2,530 rpm |
| Mosquito FB VI | Lever at four tenths | 2,283 rpm |

The S.23's take-off setting still turns above its rating, which is what a
take-off setting is for and why it has a gate; what the tail was about is
that a pilot had no way to come off it, and now has one.

**Verification run.** Four tests:
`the_keyboard_moves_every_lever_it_names_and_leaves_it_where_it_is_put`
(which walks all three levers, each down, up, to both stops, and held after
the key comes up), `a_binding_moves_every_lever_the_controls_have`,
`the_short_s23_in_coarse_pitch_at_normal_boost_turns_within_its_rated_rpm`
and `the_mosquitos_rpm_follows_its_propeller_lever`. Watched to fail with the
propeller taken out of the mapper's `fields`: "and back puts it at nought,
not 1.000000".

**And the walk of every input found the new control before a person did.**
`every_axis_button_and_hat_of_a_virtual_stick_and_throttle_moves_the_controls`
failed with "a yoke button 11 moved no control": the binding was right, and
the test could not see it. Its own list of controls to compare had nine and
not the propeller, so a binding that moved only that one looked like it moved
nothing, and it started the propeller at its default of 1, where a step
upward has nowhere to go. Both put right, and it still walks all 56 inputs of
both devices. It was a filtered run - `ctest -R 'input|binding'` - that
missed this; the whole suite found it.

323 of 323 tests pass locally at `-j4`, in 978 s.

### Ubuntu had quietly stopped being checked, 2026-09-21

**A job that runs out of time is reported as cancelled, not failed.** It
therefore does not look like a red build, and the Ubuntu job had been ending
that way for hours without anyone treating it as a problem - including in
this session, where it was noted three times as "cancelled, not failed" and
left alone.

Measured rather than guessed: the job ran **60 min 17 s** and **60 min 15 s**
against a `timeout-minutes: 60`, twice. A third cancellation really was a
push cancelling it at 33 minutes, which is what made the other two look like
the same thing.

**Why that job and not the others.** Ubuntu builds both presets and runs the
whole suite twice, the sanitized one included; Rocky 9 builds
`linux-release` alone, which is why it finishes in 14 to 17 minutes doing
what looks like the same work. The suite has grown from 282 tests to 319 in
this session, and Ubuntu crossed the hour on the way.

Ubuntu is now given 150 minutes. The other three were at 44 to 47 minutes of
their 60, which is the same cliff a little further off, and are given 90.

### Three checklists written from the handbook, 2026-09-21

**The provenance blocker was mine, not the world's.** The checklists were
recorded as written from ordinary practice because no handbook had been read
- and `docs/ASSETS.md` had been holding URLs for several of those handbooks
all along, which nobody had opened. The project owner said to go and look.

**Three of the sixteen are now written from the aeroplane's own handbook**,
read rather than remembered:

| Aircraft | Handbook | What was taken |
| --- | --- | --- |
| Cessna 172P | POH and FAA Approved Airplane Flight Manual, 1981 Model 172P, section 4, pp. 4-6 to 4-10 | Its eleven checklists, in its order, with its figures |
| Cessna 182S | POH section 4, pp. 4-11 to 4-17 | The same |
| Piper PA-28-180 | Owner's Handbook, Cherokee "E", section III, pp. 17 to 23 | Its numbered before-landing check list, and its prose procedures |

**Two things the reading changed.** The Cessna 182's climb band was 82 knots
and the handbook says 85 to 95; the 172P's take-off gained the run-up at
1,700 RPM, with the magneto drop the handbook allows. Both were written from
memory before and were not wrong so much as not sourced.

**And one thing found on the way.** The 172P's handbook has no URL in
`ASSETS.md`, so it had to be searched for. The first copy found was a scan;
the second had a text layer but had scanned two climb speeds into nonsense -
"Climb Speed -- ~ ISJAS" - so a third copy was read to recover them, 70 to 80
knots on the take-off and 70 to 85 en route. Both copies are now recorded.

**The J-3 Cub's manual was looked for and is not readable.** Both copies
`ASSETS.md` records carry no text layer, and the text versions that exist are
a flying club's checklist and a flight-simulator vendor's manual, neither of
them Piper's. That is now a checked fact rather than an assumption, and its
checklist stays this project's own words.

**The frame test earned its keep.** Rewriting the 172P's take-off list from
the handbook took it from six items to seven, and
`the_checklist_on_screen_is_the_one_the_flight_is_working_through_on_vulkan`
failed because it pins that number - which is what it is for. A silent change
to what is taught is exactly what it refuses.

**The two Cesium defects were checked against `main`** rather than against
the pinned copy, and both are still there. The second is worse than the draft
said: `parseQuantizedMesh` and `decodeIndices` read unaligned the same way as
`readValue`, so a fix wants the file rather than the function. No matching
issue is open on their tracker. `docs/cesium-issues.md` says so now.

**What a lesson's reference speed can be** was looked into rather than
assumed. Six of the sixteen publish a stall speed. The airliners' and
business jet's manuals are their operators' and are not published; the A320
family's stall speed is not a tabulated figure at all, being VS1g behind a
low-speed protection the crew cannot override; and no flight manual is public
for the B-2A, F-22A or F-35A. `figures.cpp` can already measure a stall speed
from any model, which is a way through that invents nothing - but it changes
what a lesson's figures mean, so it is written in the plan for the project
owner rather than taken.

319 of 319 tests pass locally at `-j4`, in 991 s.

### The wire format, 2026-09-21 — item begun, not done

Phase 6's first item, started. **The envelope and the encoding are built,
tested and written up byte for byte; nothing yet connects to anything.**

**Six bytes at the front of every datagram**: four of magic, one of version,
one of type. The magic is `GLDS`, this project's own, so a gearstick client
and a glideslope server refuse each other at the first four bytes rather than
somewhere deeper. The magic is checked before the version, so another
protocol's datagram is told it is another protocol rather than told its
version is wrong - true, but useless.

**Little-endian, fixed-width, no padding**, and a double goes on the wire as
its IEEE-754 bits in a `u64`: one representation rather than a compiler's
choice of one. The test pins the bytes themselves, not only the round trip -
`1.0` is `00 00 00 00 00 00 F0 3F` - because a third party writing a client
needs the bytes.

**Everything off the wire is hostile until it has been read.** The reader
never reads past the end of what it was given, whatever the lengths inside
say: it marks itself broken on the first read it cannot satisfy, answers zero
from then on, and is asked once at the end whether any of it was real. That
is what lets it be fuzzed without a crash being the expected outcome.

**Walked, not sampled.** Every truncation of a whole datagram - every length
from nothing to one byte short - and every single-byte change at every
position, all 256 values at each: the whole space, with its size stated and
checked. The round-trip test states that it walks 40 values, and caught its
own arithmetic when it was written as 39.

**The document and the code are held to each other.** A document written so a
third party could build a client from it alone is worth nothing if it drifts
from what this end sends, so a test reads every number back out of
`docs/TRANSPORT.md` - the magic in hex and in ASCII, the version, the
envelope's size, every type and every refusal by value and by name - and
holds them against the code. It also holds the document to having a section
saying what the transport does not claim, and one saying what is not built.
It found the document at once: it said "six bytes" in words where a
byte-for-byte specification should give the number.

**What is not built**: the handshake, the sealing, the messages, and sockets.
`src/platform/` has no socket code on any platform and libsodium is not a
dependency. Until those exist a client written from the document can encode
and decode an envelope and its values, and no more - which the document says
plainly rather than leaving to be discovered.

**Verification run.** Seven tests, all registered:
`every_value_written_to_the_wire_reads_back_as_itself`,
`the_wire_puts_the_least_significant_byte_first`,
`the_envelope_is_the_magic_then_the_version_then_the_type`,
`an_envelope_that_is_wrong_is_refused_with_the_reason_it_is_wrong`,
`every_truncation_of_a_datagram_is_refused_without_running_off_the_end`,
`no_single_byte_changed_anywhere_in_a_datagram_can_break_the_reader` and
`the_transport_document_and_the_code_agree_byte_for_byte`. They take under a
fifth of a second between them. 307 of 307 tests pass locally at `-j4`, in
945 s.

### A checklist band the aeroplane can reach, 2026-09-21 — tail done

The tail found writing the checklists: the tests proved a property was real
and that the aircraft's own model drove it, but not that the figure was one
the aeroplane could get to. A flap band of 33 to 35 degrees on a type whose
flaps stop at 32 passed everything and would never have ticked once.

**A lever and what it moves are held differently**, because they are
different things. A command is the lever, and its travel is known without
flying anything - 0 to 1, or -1 to 1 for the ones that go both ways. A
position is where the aeroplane has got to, which only its own model knows,
so its levers are worked through their travel and what the position really
covers is measured.

**The aeroplane is not flown to find out**, and three attempts taught why.
Driving every lever to its stop at once - full throttle against full brakes,
the trim hard over - put a tail-wheel aeroplane on its nose and off the end
of its own aerodynamic tables. Retracting the undercarriage while the
aeroplane stood on it did the same. Both end the flight inside JSBSim, which
asserts rather than extrapolating. Only the configuration levers are worked,
with the brakes holding the aeroplane still, and the undercarriage is read
from the model instead: an aeroplane that never names `gear/gear-cmd-norm`
or `gear/gear-pos-norm` has no undercarriage channel and its wheels stay
down. `<retractable>` on a leg is not the marker - the Mosquito's legs do not
carry it and its undercarriage still comes up.

**A flying boat needs water under it.** Given dry land its hull tells the
truth about dry land, and every hydrodynamic item looked unreachable: the
S.23's "the hull in the water and carrying the boat" wanted
`hydro/active-norm` at least 0.5 and measured 0 throughout. That was the
test's fault, not the checklist's.

**Verification run.**
`every_checklist_band_is_one_its_aeroplanes_controls_can_reach`, registered,
watched to fail with the Cessna's landing flaps put at 33 to 35 degrees:
"c172p's landing ... wants fcs/flap-pos-deg between 33 and 35, but its levers
only move it between 0 and 30". It takes 68 seconds. 300 of 300 tests
pass locally at `-j4`, in 1039 s.

**What GCC cannot see.** The two constants this left unused when its trace
came out were invisible to both Linux builds: GCC does not warn about an
unused constant at namespace scope in C++, not under `-Wall -Wextra` and not
under `-Wunused-const-variable` at any level, which it honours for C alone.
Clang does, so macOS and Windows clang-cl went red together on a commit that
both Linux jobs had passed. Every first-party file is now checked by hand
under clang before a push, which is review rather than a check, and a tail
records that.

### An autopilot that takes off, 2026-09-21 — groundwork, no item of its own

**All four light aircraft take themselves off**, hold the centreline within
1.4 m on the ground, and climb away at their own best climb speed. This has
no item in the plan: Phase 5c's lessons need it and nothing else does, so it
is recorded here as what was built towards them.

| Aircraft | rotates at | airborne in | its handbook's roll | 500 ft in | worst off the centreline, on the ground |
| --- | --- | --- | --- | --- | --- |
| Cessna 172P | 55.6 kt | 280 m | 271 m | 56 s | 0.35 m |
| Cessna 182S | 57.5 kt | 260 m | 242 m | 48 s | 0.43 m |
| Piper PA-28 | 47.8 kt | 251 m | 219 m | 57 s | 1.35 m |
| Piper J-3 Cub | 37.9 kt | 129 m | none published | 47 s | 0.10 m |

**The speeds are the aeroplane's own.** The best climb speed is the speed its
published climb rate was measured at, which all four publish. The lift-off
speed is the speed its published take-off roll was measured at - three of the
four - and where there is none, a seventh above its published stall, which is
the usual relation; `DepartureSpeeds::rotate_is_published` says which it was,
and the Cub is the one that is worked out. An aircraft publishing no climb
speed at all is refused rather than given a guess.

**The test holds each to its handbook's ground roll**, within a third either
way, rather than to a number chosen to fit: 280 m against 271, 260 against
242, 251 against 219.

**A sign was wrong, in two places.** `sim/test_pilot.cpp` says it plainly -
"the model's rudder command yaws the nose left for positive values" - and
both the take-off roll and the landing rollout had it the other way, so the
rudder drove the swing instead of correcting it. On take-off it was obvious:
the Cessna left the centreline at once, wandered 182 m off, and turned right
round - full right rudder and right brake while the nose went from 070 to
359. **On the landing rollout it was not obvious at all**: the approach and
landing item passed its verification with the fault in it, because the
touchdown figures are set before the rollout begins and the aeroplane still
stopped on the runway. The touchdown figures are unchanged by the fix; the
stopping distances moved by about fifty metres.

**Verification run.** `every_light_aircraft_takes_itself_off_and_climbs_away`
and `the_take_off_speeds_come_from_each_aircrafts_published_figures`, both
registered, and both stating that the light aircraft are four and that three
of them publish a lift-off speed. 299 of 299 tests pass locally at `-j4`, in
1052 s.

### An autopilot that flies an approach and lands, 2026-09-21 — item done

Phase 8's approach-and-landing item, **brought forward on purpose**: Phase
5c's lessons teach take-off, the circuit, stalls, approach and landing, and
until something can land there is nothing to demonstrate them with. Reading
what the AI pilot could do settled it - it holds a heading, an altitude, a
vertical speed and a speed, follows waypoints in the cruise, and does nothing
else. So this came first although it is written three phases later.

**All four light aircraft fly an approach from five miles out and land**, in
calm air and in a ten-knot crosswind:

| Aircraft | Vref | calm: sink, across, stopped | crosswind: sink, across, stopped |
| --- | --- | --- | --- |
| Cessna 172P | 59.8 kt | 128 ft/min, 0.14 m, 570 m | 217 ft/min, 1.39 m, 569 m |
| Cessna 182S | 64.4 kt | 72 ft/min, 1.89 m, 406 m | 255 ft/min, 2.72 m, 442 m |
| Piper PA-28 | 64.4 kt | 77 ft/min, 1.61 m, 722 m | 243 ft/min, 3.99 m, 708 m |
| Piper J-3 Cub | 42.9 kt | 177 ft/min, 0.19 m, 539 m | 149 ft/min, 1.66 m, 491 m |

held to under 300 ft/min, within 5 m of the centreline, and stopped on a
3,000 m runway.

**The reference speed is not a number written here.** `approach_speeds` reads
the aeroplane's own published figures and takes a third above its stall in
the landing configuration - the most flap it publishes a stall speed at,
which is all of it for an aeroplane with flaps and none for a Cub. An
aircraft that publishes no stall speed is refused, because a reference speed
guessed is a reference speed that means nothing; the B-2 publishes none, and
a test holds that it is refused.

**It is not the cruise autopilot, and could not have been.** That one holds
an altitude on the elevator and a speed on the throttle and never touches the
flaps, the gear or the brakes. `sim::Lander` flies the glidepath on the
elevator and the speed on the throttle - an autopilot and an autothrottle -
and works the flaps, the gear and the brakes as the stages need them:
approach, flare, rollout, stopped. **The simulation links no world library**,
so the geometry is a local one: the metres in a degree of latitude and
longitude at the threshold, which over five miles is right to better than a
metre.

**Four things had to be learnt from watching it fly.**

1. *The glidepath aims past the threshold, not at it.* Aiming at the
   threshold puts the flare before it and the wheels on the grass - the
   Skylane touched down 36 m short. Aiming 300 m down the runway puts the
   aeroplane about fifty feet up as it crosses, which is where it should be.
2. *The flare is a sink that decays with height, not an attitude.* A fixed
   nose-up ramp landed the Cessna gently and the heavier Skylane at 472
   ft/min. Commanding the sink instead - forty feet a minute at the ground,
   more the higher it is - is one law that fits a Cub and a Skylane.
3. *Stopped is over the ground, not through the air.* An aeroplane standing
   still in a ten-knot wind still reads ten knots of airspeed, so the first
   version never noticed it had stopped.
4. *The stick comes back and stays back on the rollout.* Brakes with the
   stick forward put the Cub on its nose: its pitch went to -71 degrees and
   JSBSim asserted inside its own aerodynamic tables. The stick full aft
   holds a tailwheel down and keeps a nosewheel light, so one rule fits both,
   and the brakes come on as the aeroplane slows rather than the moment it
   touches. The flare also stops raising the nose past twelve degrees of
   incidence, for the same reason: JSBSim asserts rather than extrapolating,
   so an aeroplane flown off the end of its tables ends the flight.

**Verification run.** Three tests:
`every_light_aircraft_is_flown_down_a_glidepath_and_lands_in_calm_air`,
`every_light_aircraft_lands_on_the_centreline_in_a_ten_knot_crosswind` and
`the_approach_speed_is_a_third_above_the_published_landing_stall`. Each
states that the light aircraft are four and fails if that moves. Watched to
fail with the cross-track integral taken out: the Cessna touched down 11.74 m
from the centreline instead of 1.39 m, and was 16.33 m off at its worst on
final - which is the standing offset a proportional loop alone leaves in a
crosswind, and the reason the integral is there. The two flying tests take 80
seconds each. 297 of 297 tests pass locally at `-j4`, in 1471 s.

**What it is not.** It lands on a runway it is given; nothing finds a runway
for it, there is no airfield data, and it does not go around. It flies no
part of a circuit and cannot take off, which is still what Phase 5c's lessons
need next.

### Checklists on screen, ticking themselves, 2026-09-21 — item done

Phase 5c's second item. **`--checklist PHASE` puts that phase's list down the
right of the screen, and it ticks itself as the aeroplane flies.**

**An item ticks at the first tick its state shows it done, and stays ticked.**
A checklist records that a thing was done, not that it is still true: flaps
set for take-off and raised on the climb does not untick the take-off list.
`sim::ChecklistRun` holds one `ItemProgress` per item of every phase - the
tick it was first seen done at - and the run is kept for every phase at once,
so turning to a list shows what the aeroplane has already done rather than an
empty page. An item the simulation cannot see is the pilot's, and ticks only
when the pilot says so; an item the aircraft has not got the property for is
left alone rather than ending the flight, because that is a fault for a test
to catch and not a reason to stop flying.

**On screen** it is drawn small down the top right, as the credits are drawn
small along the bottom, so a long item reads without crowding the numbers
down the left. The first line is the phase and how much of it is done; each
line after begins "X" for a ticked item or "-" for one still to do. The font
has capitals, digits and a little punctuation and no more, so the words go in
capitals and what it lacks - a comma - is dropped; an item too long for the
line is cut rather than wrapped, so that every line but the first begins with
a mark and the two can never be told apart.

**Verification run.** Four tests fly it and one reads it off a frame:

- `every_checklist_item_the_cessna_can_see_ticks_when_its_state_first_shows_it_done`
  flies a take-off from a runway and, for every item of the taxi, take-off,
  climb and landing lists, holds the tick recorded against the first tick the
  item was really done - and holds that, flown by the book, every item of the
  three lists it flies that the aeroplane can see is ticked. Watched to fail
  with the recorded tick moved by one: "taxi item 0 (Engine running) ticked at
  1, but was first done at 0".
- `an_item_once_ticked_stays_ticked_though_the_aeroplane_moves_on` finds an
  item the take-off undoes - the taxi list's "throttle back to a walking
  pace", true at rest and false at full power - and holds it ticked. Watched
  to fail with unticking allowed: "was done at tick 0, stopped being so at
  144, and must still be ticked".
- `flown_with_the_flaps_left_up_the_landing_flaps_item_stays_unticked_and_is_flagged`
  and `the_pilot_ticks_their_own_items_and_only_their_own`.
- `the_checklist_on_screen_is_the_one_the_flight_is_working_through_on_<driver>`
  shoots a frame, reads the block back glyph by glyph and holds every line to
  what the same run said it drew, and refuses a phase of flight there is none
  of by name. Watched to fail twice: with the drawing moved three pixels, line
  one read "???????? ???" instead of "TAKE-OFF 4/6"; and with the heading's
  count made one too many, "the checklist says 5 done but 4 items are ticked"
  - that second one is a real independent check, because the tool counts the
  marks itself rather than trusting the heading.

294 of 294 tests pass locally at `-j4`, in 1036 s.

**The verification names a phase there is none of.** It asks for "the
Cessna's before-take-off, take-off and climb"; the nine phases are
`FEATURES.md`'s - before start, taxi, take-off, climb, cruise, descent,
approach, landing, after landing - and there is no before-take-off among
them. Taxi is what was flown in its place, and it is the list that holds the
run-up.

### Checklists for every aircraft, 2026-09-21 — item not done

Phase 5c's first item. **All sixteen aircraft carry a checklist for each of
the nine phases of flight - 144 checklists and 757 items - and every item
either names a state the aeroplane really has or is marked the pilot's to
confirm.** The item is not ticked, for a reason given at the end.

They are the aeroplane's own, not one aeroplane's copied round: the Cub is
swung by hand and has its nose weaved on the ground because nothing is
visible straight ahead, the S.23 taxies on water and finishes at a buoy, the
Mosquito has its superchargers and radiator flaps, and the F-22's cruise item
wants Mach without reheat. A turbofan names no mixture and no pitch lever
anywhere, and each of those files says in its header that the engine has
neither.

**The format** is `assets/aircraft/<id>.checklist`, beside the `.aircraft`
file, line-oriented with `#` for comments as every other data file here is;
`src/sim/checklist.hpp` owns it. Four commands: `phase`, and three kinds of
item - `check PROPERTY OP VALUE TEXT` where OP is `<=` or `>=`, `range
PROPERTY LOW HIGH TEXT` where a band is what shows the item done, and
`confirm TEXT` for what the simulation cannot see. The machine-readable part
comes first so the pilot's words can be free text to the end of the line,
which is how `start AIRSPEED THROTTLE` already reads. All nine phases must
appear, once, in the order they are flown, and none may be empty: a file that
has quietly lost a phase is refused rather than teaching less than it claims.

**A property being there does not mean the aeroplane has the thing.** The
first test asked each model for every property its checklists name, through
`Aircraft::property`, which throws for one the model has not got. That is
weaker than it looks: JSBSim's `FGFCS::bind` ties `fcs/flap-pos-deg`,
`fcs/flap-cmd-norm` and `fcs/speedbrake-pos-norm` for every aircraft whether
or not its model has that channel (`ext/jsbsim/src/models/FGFCS.cpp:725, 753,
758`), so they answer on a Piper Cub, which has no flaps at all, and on a
B-2, whose drag rudders answer the pedals. An item resting on one would never
tick and the test would have said nothing.

What separates a real channel from a phantom is whether the aircraft's own
flight model names the property, to drive it or to read it back. So each
aircraft's model files are read and a control property its own model never
mentions is refused. Three are named in the test as exceptions with their
reason - JSBSim ties `fcs/throttle-cmd-norm`, `fcs/mixture-cmd-norm` and
`fcs/advance-cmd-norm` once per engine, so a model that uses them need never
spell them - and mixture and propeller pitch are refused on an aircraft whose
engine is neither piston nor turboprop, which is the one way those two could
still be nonsense on a jet.

**Verification run.** Six tests, all registered:
`every_aircraft_has_a_checklist_for_every_phase_of_flight` (which states the
roster is sixteen and the phases nine, and fails if either moves),
`no_checklist_item_rests_on_a_control_its_aircrafts_model_has_not_got`,
`every_checklist_item_names_a_state_its_own_aircraft_has_or_is_the_pilots`,
`an_item_is_done_when_the_aircrafts_state_is_inside_its_band`,
`a_checklist_file_that_is_wrong_is_refused_and_says_where` (ten separate
refusals) and `the_nine_phases_of_flight_each_have_one_name_and_answer_to_it`.
The phantom-control test was watched to fail: a flap item put into the Cub's
file gave "j3cub's take-off: j3cub's flight model never mentions
fcs/flap-pos-deg, so the item could not tick", while the older property test
passed the same file - which is the gap, seen. 289 of 289 tests pass locally
at `-j4`, in 1014 s.

**Why the item is not ticked: the words' provenance.** The item asks for
checklists "from its handbook or pilot's notes". No such manual was read to
write these. They follow the ordinary practice for each type, in the order it
is flown, with their speeds and settings taken from this project's own
`assets/figures/<id>.xml` and from the flight models themselves where the
model is what decides. Every file says so in its own header, and
`docs/ASSETS.md` has a section, "Written here, from no outside source", that
records it rather than naming a source that was not used - the file's rule is
that terms are quoted from the source, not paraphrased from memory, and a
handbook cited but unread would be exactly that. **For three of the sixteen
it could not be done as written in any case: no flight manual is public for
the B-2A, the F-22A or the F-35A.**

### The terrain sample that settled on having no answer, 2026-09-21

**A place that has not answered is not a place that has settled.** The
visual-to-collision measurement asks the drawn surface for a height until two
answers running agree, because a sample reports success whether or not the
tiles beneath it had arrived. That comparison treated "no answer" and "no
answer" as agreeing, so two failures in a row ended the asking on the second
round - the least loading time it can give - and a cell whose terrain was
merely slow was reported as having no surface at all.

It showed up as `the_open_terrain_is_within_its_stated_distance_of_the_ground_flown_on_vulkan`
failing on Ubuntu, and only Ubuntu, while Rocky 9, the same compiler on the
same architecture, passed: "open did not answer with a height at KLAS-8L,
KLAS-26R". Las Vegas's two runway ends sit in one whole-degree cell, so one
slow cell took both with it. Every place must now have a height before two
rounds are compared at all, and a round the tileset never answered carries on
to the next instead of giving up on every place at once.

**A message that said the opposite of what it meant** went with it: a place
with no drawn surface printed "none flown" and then the flown height beside
it, which reads as the DEM having failed when the DEM was the half that
worked. It says "none-drawn" now.

### Google's tiles draw, and nothing read them before, 2026-09-21

**Google's Photorealistic 3D Tiles draw, photographs and all, with Google's
attribution**, and their surface is 10.1 m from the ground flown at worst.
Phase 5b's Google item is still not ticked, because the other way in to them
- a Google Maps Platform key used directly - has no key here to prove it
with. Phase 5b's attribution item is ticked: every provider, in both its
states, is now walked by a test.

**Nothing read a tile's content until its readers were registered.** Cesium
Native keeps a table of converters - glTF, B3DM, PNTS, composite - looked up
by the first bytes of what arrives, and `registerAllTileContentTypes()` fills
it. Nothing called it. A tileset whose tiles are glTF therefore loaded every
one of them and drew none: not knowing what a body was, Cesium fell back to
reading it as an external tileset, and 441 perfectly good glTF binaries came
back as "Error when parsing JSON content, error code Invalid value. at byte
offset 0".

It had never mattered. The open provider builds its glTF here, and Cesium
ion's quantized mesh has a reader of its own; Google's is the first content
that arrives as glTF and has to be recognised. Two guesses were wrong before
this one - that the body was still compressed, and that it was an
authorisation failure - and what settled it was printing the status and first
four bytes of every response: 548 of them, all 200, 441 beginning `glTF`, and
441 parse errors.

**Two real faults were fixed on the way**, neither of them the cause:

- A bearer token of ours went onto requests for anything Cesium ion points at
  without a token of its own, which is how Google's asset was asked for.
- Google's root carries a key and no session, and the child tiles it names
  carry a session and no key, so neither "it already has a query" nor "it
  already has a session" says whether the key is there. Taking either for an
  answer is a 403. Each parameter is now merged on its own.

**They draw their own pictures now.** Photorealistic 3D Tiles carry their
textures inside their glTF, and the only textures this renderer uploaded were
imagery draped as raster overlays - which is what the open provider and
Cesium ion use, so nothing had needed it. A primitive's base-colour texture
is now followed to its image, decoded once per tile, uploaded with the tile
and freed with it, and its TEXCOORD_0 read where no imagery is draped over
it. A textured surface starts white, because the shader draws the vertex
colour times the texture.

**And they refine.** The settling that tells a streamed provider when it has
arrived watched what was drawn and how deep it went. Google's tiles are not a
quadtree, so their depth reads 0 throughout, and what is drawn can sit still
for a moment part-way down - so it stopped at 19 tiles of smooth green. It
now also watches how many tiles the tileset holds, which keeps rising while
it is still refining: 452 tiles drawn of 795 held, and Mount Taranaki is a
snow-capped cone with the coast behind it.

**What is left: the other way in.** A Google Maps Platform key used directly,
rather than through Cesium ion, is written and has never been run, because
there is no such key on this machine. `--terrain google` takes it in
preference to the ion token when one is there, so the test covers whichever
the machine has; until one machine has a Google key, half of "through both
ways in" is unproven and the item stays open.

**Verification run.** 282 of 282 tests pass locally at `-j4`, in 1067 s.
`the_google_terrain_draws_with_its_attribution_or_says_why_not_on_vulkan`
takes 38 s and `the_google_terrain_is_within_its_stated_distance_of_the_ground_flown_on_vulkan`
40 s; the second was watched to fail with its bound tightened to 5 m. The
settling change is what let Phase 5b's first item be ticked as well:
`the_ion_terrain_draws_with_its_attribution_or_says_why_not_on_vulkan` now
takes 23 s, where waiting for a whole Earth had run past 25 minutes.


### The visual terrain against the terrain flown, 2026-09-21 — item done

Phase 5b's "a measured visual-to-collision terrain mismatch". **The open
provider's surface is 0.18 m from the ground an aircraft meets at worst,
Cesium ion's is 10.2 m and Google's is 10.1 m**, at the twelve surveyed
runway ends of six airfields.

**What the ground is.** The ground an aircraft meets is always the open DEM -
the rule that lets a server and every client agree on where it is. A visual
provider may put its surface somewhere else, and how far is what this
measures. `--mismatch FILE` names places, samples the drawn surface at each
through Cesium Native's `sampleHeightMostDetailed`, asks the DEM for the same
places, and prints both. The places are the twelve surveyed runway ends in
`tests/data/dem/surveyed.txt`, already in the repository because the DEM
itself is held to them.

One tileset is opened for each whole-degree cell rather than one for them
all: the open provider builds its terrain over the region it is given, and a
region from Barrow to Boston is most of a continent.

| Airfield | open | Cesium ion | Google |
| --- | --- | --- | --- |
| KDEN, Denver | 0.08 m | 3.38 m | 2.94 m |
| KLAS, Las Vegas | 0.02 m | 1.67 m | 0.77 m |
| KBOS, Boston | 0.01 m | 2.44 m | 1.84 m |
| PAJN, Juneau | 0.02 m | 2.79 m | 2.66 m |
| PANC, Anchorage | **0.18 m** | 1.66 m | 0.23 m |
| PABR, Barrow | 0.05 m | **10.22 m** | **10.12 m** |

The open provider's figure is what it should be: the mesh drawn is built from
the same DEM the aircraft meets, so what is left is its interpolation between
the points it is built on. Cesium ion's is a different survey of the same
ground, a metre or three out over most of them and ten at Barrow, where the
Arctic coast is thinly surveyed by anyone. Google's follows the same shape
for the same reason, and agrees with ion's worst figure to a tenth of a
metre - two independent surveys finding the same ten metres at Barrow is
better evidence that the Arctic coast is the hard part than either alone.

**Asking once is not enough, and the answer does not say so.** A sample
reports success whether or not the tiles beneath it had arrived. Boston and
Anchorage answered, repeatably, with a surface 36 km and 12 km *below* the
ellipsoid - no land is there - while Denver and Las Vegas answered properly.
What told them apart was which airfields straddle a whole-degree boundary:
Boston's and Anchorage's two runway ends fall in different cells, so each was
asked about alone, and the same call had half as long to load. The sample is
now made again until two answers running agree to a centimetre, which is the
only thing that says the tiles it needed were there. With that, all twelve
answer with a height and the whole measurement repeats byte for byte.

A height outside -500 m to 9,000 m is still set aside rather than folded into
a bound, and a provider that gives one fails: a bound with 36 km in it would
mean nothing.

**Verification run.** `the_open_terrain_is_within_its_stated_distance_of_the_ground_flown_on_<driver>`
holds the open provider to 0.25 m and Cesium ion and Google to 12 m each,
the figures above with a little room; each takes about 45 seconds, and the
two streamed ones report themselves skipped where there is no key. Watched to
fail with the bound tightened to 50 mm: "open's terrain is 179 mm from the
ground flown at PANC-7R, beyond the 50 mm stated in
docs/PROJECT_STATUS.md", and again for Google with its bound at 5 m:
"google's terrain is 10121 mm from the ground flown at PABR-8, beyond the
5000 mm stated in docs/PROJECT_STATUS.md". Google's takes 40 seconds.

### Cesium ion as a visual terrain provider, 2026-09-21 — item done

Phase 5b's first item. **Cesium ion draws Cesium World Terrain under Bing
Maps Aerial with its attribution, and a provider without its key says what is
missing rather than failing** - both walked by one test that takes 17 seconds
and reports itself skipped where there is no token.

**A key belongs to the user and is never in the repository.**
`platform::config_directory()` is where their settings live -
`%APPDATA%\glideslope`, `~/Library/Application Support/glideslope`, or
`$XDG_CONFIG_HOME/glideslope` - and `cesium_ion_token()` and
`google_maps_key()` read one at run time, from an environment variable first
and then a file of their own. A missing, unreadable or empty one is "none",
because a provider that needs a key says what is missing and where to put it:

    Cesium ion needs your own token: put it in cesium-ion-token in
    glideslope's config directory, or set GLIDESLOPE_CESIUM_ION_TOKEN. One is
    free from https://cesium.com/ion/

**`--terrain open|ion|google`** chooses what is drawn. The ground the aircraft
meets is the open DEM whichever is drawn, which is the rule that lets a server
and every client agree on where the ground is.

**Three things had to be built before any of it could fetch a tile.**

1. *Requests carry headers.* The accessor refused anything but a plain GET
   with no headers, and ion authorises every tile with one. `HttpRequest` now
   carries headers and all three backends send them - libcurl's `slist`,
   WinHTTP's CRLF block, NSURLSession's `setValue:forHTTPHeaderField:`. A
   name or value holding a control character is dropped rather than passed
   on, so nothing can be smuggled in by splitting a header across lines.
2. *Content encoding is the HTTP layer's own business.* ion serves its
   `layer.json` with `content-encoding: gzip` whether the client asks for it
   or not. libcurl only undoes an encoding it negotiated, so a forwarded
   `Accept-Encoding` left a body nothing could parse. libcurl now accepts
   every encoding it can undo, and a provider's own `Accept-Encoding` is not
   passed on.
3. *A token goes only to the host that issued it.* `AuthorisingAccessor` puts
   the bearer token on requests to one place and no other, so a tileset
   naming a URL elsewhere cannot make it leak.

**Cesium Native logged the token.** It says at info level which URLs it
fetched, and an ion URL carries the token in it - which would put the user's
token in whatever kept the output, a CI log included.
`gfx::log_to_standard_error()` now also quiets the log to warnings and worse.

**Two defects in Cesium Native v0.64.0, both found by the sanitized build.**
Drafts of both are ready to post upstream; neither is glideslope's to fix.

1. *A dangling reference on every ion tile load.*
   `TileLoadInput::pAssetAccessor` is a reference member
   (`const shared_ptr<IAssetAccessor>&`), and `CesiumIonTilesetLoader` passes
   a `shared_ptr` to a *derived* accessor. The conversion makes a temporary
   that is bound to the reference and destroyed at the end of the statement,
   so every later use reads it dangling - `stack-use-after-scope`. The open
   provider never meets it, because its types match and no temporary is made.
   **Worked around**: ion's asset endpoint is resolved here, with one plain
   GET, and the tiles are then an ordinary tileset with an Authorization
   header, which goes nowhere near that code. `main` upstream still has the
   same reference member, so bumping the pin would not have helped.
2. *Misaligned loads on every terrain tile.*
   `QuantizedMeshLoader::readValue` is
   `return *reinterpret_cast<const T*>(data.data() + offset)` at an arbitrary
   byte offset, and quantized mesh does not align its fields. With
   `-fno-sanitize-recover=all` that ends the process, so a sanitized build
   could not stream Cesium World Terrain at all. **Decided 2026-09-21 by the
   project owner**: the alignment check alone is off for Cesium Native's own
   targets - `glideslope_allow_misaligned` in `cmake/Sanitizers.cmake` - and
   every other check, and all first-party code, is untouched. It goes away
   when they fix it.

**Waiting for a whole Earth is not a thing that ends.** The open provider's
terrain is built here, over a region, and stops at the DEM's own spacing, so
waiting for every tile a view needs makes the same command draw the same
terrain on every machine. A streamed provider has no such end, and asking
Cesium Native for it - `updateViewGroupOffline`, which refines regardless of
screen-space error - ran past 25 minutes. A streamed provider is given a
settling instead: rounds of loading, taking up what the workers finish, until
what is drawn, how deep it goes and how many tiles the tileset holds have all
stood still for three seconds, and never more than three quarters of a
minute. **Its frame is therefore not
claimed to be the same on every machine**, because what arrives depends on
the network and on the provider; what is claimed is that it drew terrain and
that its attribution is on it. With that, ion reaches level 13 at the same
screen-space error the open provider uses, in 13 seconds.

**What it draws.** Mount Taranaki from the north-east, Cesium World Terrain
under Bing Maps Aerial, 126 tiles, the deepest at level 13 - with ion's own attribution along the bottom: the USGS, CGIAR-CSI,
Copernicus, Land Information New Zealand, data.gov.uk, Geoscience Australia,
Microsoft, Mapbox, Earthstar Geographics SIO, Maxar and Airbus DS, and the
free tier's "upgrade for commercial use".

**Both things this left were picked up the same day**, and are written up
under "Google's Photorealistic 3D Tiles" and "How far the terrain drawn is
from the terrain flown" above. Google's tiles did not draw because
`Cesium3DTilesContent::registerAllTileContentTypes()` had never been called,
so every `glTF` body was offered to the JSON tileset reader and refused at
byte offset 0 - the guess recorded here, that content was being read as the
wrong thing, was right. The two things found on the way to it and kept are
`QueryAccessor`, which carries Google's `session` and `key` onto child
requests that name neither, and reading ion's third answer shape
(`externalType: 3DTILES`, a url and no token).


### Views of the aeroplane, 2026-09-21 — item done

Phase 5's last item. **The aeroplane is drawn, and `--view` says where it is
seen from**: `cockpit`, `ahead`, `behind`, `left`, `right`, `above` or
`orbit`, and V steps round them while flying. A view there is none of is
refused by name, with the ones there are - as `--aircraft` does.

**What it took.** `gfx/aircraft.hpp` holds the views and turns a model into a
mesh; `Flight` works out where the model goes and what the camera sees. The
cockpit looks out along the nose from the flight model's own eyepoint, and
rolls with the aeroplane. The six outside views stand off three times the
model's radius in the body frame and look back at it, held upright, so they
swing with the aeroplane rather than staying level; the orbit goes round once
a minute of the flight's own time, which makes the same command draw the same
frame every run.

**The cockpit draws no aeroplane.** The renderer culls nothing by its facing
and the models have no interior, so from inside, the skin would be drawn over
the windscreen. Nothing of it is drawn there, and the test holds the shot
with the aeroplane and the shot without it to being the same file.

**The light is baked in.** The mesh shader takes a position, a colour and a
texture coordinate, and no normal, so an aeroplane is shaded on the way in:
each vertex's colour times how much light its normal catches, under the sun
the terrain is lit by - north-west, 45 degrees up - so an aeroplane is lit as
the ground beneath it is. The sun is worked out once in the body frame rather
than rotating every normal, and the mesh is made again when the aeroplane has
banked five degrees. `gfx/terrain_colour.hpp` now hands out that sun and the
light on a surface, which is what the terrain was already doing inside
itself; the terrain's own figures are unchanged.

**The drawing that draws nothing.** `glideslope_model` grew from the model
reader alone to the views, the scene's geometry and the terrain's colours -
all of it arithmetic with no SDL and no Cesium Native behind it - so the unit
tests hold the cockpit's eye and where each view stands without linking a
renderer. `glideslope_gfx` is now the part that needs a GPU.

**How it is held.** The frame test shoots each of the seven views at tick 120
and, for the six outside ones, shoots the same frame again with
`--draw-aircraft off`. Every pixel that differs between the two is one the
aeroplane covered and nothing else did, so the pair gives its outline exactly,
with no guessing at which pixels are aeroplane and which are sky or ground.
`glideslope_view_check` then projects every vertex of the same model file from
the camera the client printed - the projection written out by hand, sharing
nothing with the renderer but the numbers - and holds the two outlines' edges
within two pixels.

| View | left | right | top | bottom |
| --- | --- | --- | --- | --- |
| ahead | 1 | 1 | 1 | 2 |
| behind | 1 | 2 | 1 | 1 |
| left | 1 | 1 | 1 | 1 |
| right | 0 | 2 | 1 | 1 |
| above | 1 | 1 | 1 | 2 |
| orbit | 1 | 2 | 0 | 2 |

The centre of what is drawn and the centre of the vertices projected are not
the same number and are not meant to be - a model's vertices crowd where it
has detail, and only the side facing the camera is drawn, which on a side
view of the Cessna is 13 px apart on a 320 px frame - so that is held to a
tenth of the frame, and the edges are what pin it.

`--draw-aircraft on|off` is a test flag, as `--shot` and `--trace` are, and
says so in the usage.

**Changing the view steps nothing in the flight**, held by every view having
traced the same number of steps and left the flight in the same state when
the frame was shot. The first attempt at that check compared the line
numbered with the flight's own tick, which is the same in every view whatever
the view did to the flight - it was watched not to fail, and replaced.

**What is left, and named as tails.** No livery and nothing moving: a model
carries no texture and no animation, so a surface is the flat colour of its
material and the control surfaces, propellers and undercarriage are welded
where the model has them, gear down. And the light is baked rather than
worked out on the GPU, which is why the mesh is made again as the aeroplane
banks.

**Verification run.** Locally, 276 of 276 tests pass in 489 s at `-j4`; the
views frame test is 339 s of that, seven views shot twice at 20 s a shot. The
four new checks are `the_cockpit_view_puts_the_eye_where_the_flight_model_says_the_pilots_is`,
`every_view_stands_where_its_name_says_and_looks_at_the_aeroplane`,
`the_client_draws_the_aeroplane_where_each_view_puts_it_on_<driver>` and
`the_client_refuses_a_view_there_is_none_of`. Each was watched to fail: the
outline check given the `behind` shot and the `left` view's camera reports it
43 px out and exits 1; the cockpit exclusion removed, its two shots stop
matching; and a deliberate extra step on the `above` view leaves the flight
at tick 240 where every other view is at 120.


### A visual model put where its aeroplane is, 2026-09-21 — item done

Phase 5's "a visual model put where its aeroplane is" is done. **A model is
drawn at its flight model's visual reference point, moved by a measured
offset; nothing draws one yet**, which is the views item.

**JSBSim already had the anchor.** Its `<metrics>` carries a VRP - a visual
reference point, which is what a VRP is for - and every one of the sixteen
defines one. Anchoring there rather than at the structural origin is most of
the answer on its own: it puts the Cessna 172P's wheels within 0.05 m of its
flight model's, where the structural origin had them 1.03 m out, and the
Cub's, the PA-28's and the Short Empire's within 0.02 m.

**What is left is measured, not guessed.** `tools/align_models.py` reads the
committed meshes and the flight models - no network, no build - and writes
`assets/models/alignment.txt`: three metres of offset per aircraft, and what
the fit leaves over. A test fails if the committed file differs from what the
script makes. It measures the meshes, so it is stale if they change.

**The undercarriage is the contacts the aeroplane rests on**, and which those
are is geometry, not a label. A flight model's `<contact>` points are not all
wheels: the A320's include its wingtips, its nose tip and the top of its fin,
and the Short Empire's are the keels of a hull. No field tells them apart -
the A320's wingtip carries the same rolling friction as its wheels - so the
script takes the hull of the contact set seen from below, along the span of
it the centre of gravity lies over. That picks a tricycle's nose and mains, a
taildragger's tail and mains, the A380's nose and four bogies, and the flying
boat's forward keel and step, and nothing else. **JSBSim confirms it**: with
each aeroplane stood on the ground, the contacts it puts weight on are
exactly as many as the script worked out without running anything.

| Aircraft | Offset, m (x, y, z) | Left at its wheels | At its shape |
| --- | --- | --- | --- |
| 737-300 | +0.21, 0, -0.52 | 0.47 | - |
| 747-400 | +1.86, 0, +1.41 | 2.48 | - |
| 787-8 | +0.54, 0, -0.21 | 0.91 | 2.16 |
| a320 | -1.78, 0, -0.91 | 0.34 | 1.65 |
| a380 | -22.66, 0, -3.26 | 0.63 | 2.04 |
| b2 | +0.11, 0, +2.37 | 0.53 | - |
| c172p | +0.21, 0, +0.07 | 0.03 | 0.29 |
| c182 | +1.02, 0, +0.37 | 0.22 | 0.51 |
| f15c | +0.71, 0, +0.30 | 0.13 | 0.46 |
| f22 | -3.87, 0, +0.54 | 0.98 | - |
| j3cub | +0.03, 0, -0.02 | 0.07 | 0.07 |
| mosquito-fb6 | -0.50, 0, +0.42 | 0.45 | 0.86 |
| pa28 | -1.31, 0, -0.13 | 0.14 | 0.57 |
| short_s23 | +1.36, 0, +0.20 | 0.35 | 1.35 |

The offset across the centreline is held to zero: both the model and the
flight model are symmetric, so a lateral offset would be a mistake, not a
measurement.

**A flight model and a visual model of the same aeroplane do not always
agree**, and no placement can make them, so what is left over is recorded and
each aircraft held to its own figure. JSBSim's 747-400 has one main leg a
side, 5.5 m out; the aeroplane has two, at 3.7 m and 11.4 m, and FlightGear's
model draws both, which is the 2.48 m. The B-2's flight model, written here,
puts its wheels 2.37 m below where FlightGear's model draws them; the model
is moved to the flight model, because the flight model is what flies.

**The walk needs a good start.** Fitting from nothing settles in the wrong
place for an aircraft whose model is far from its flight model - the A380's
is 22.7 m away along the fuselage - so it is walked from three starts and the
one that settles closest is taken.

**What the flight models say about their shape.** Ten of the fourteen carry
contacts beyond the undercarriage - wingtips, tailcones, a radome, propeller
tips, a belly - and all 97 contacts across the fourteen are within their
aircraft's stated distance of the model. Four - the 737-300, 747-400, B-2 and
F-22 - describe nothing but their wheels, so their span is the only shape
they can be held to, and the test names those four so a fifth cannot join
them unnoticed. Every model's span is within 6% of its flight model's, the
A320 the tightest at 5.5%, its model having sharklets its flight model's span
does not; the PA-28 is excepted and named, because FlightGear's is the
PA-28-161 Warrior II with a 35 ft wing where the flight model is the
PA-28-180 Cherokee with a 30 ft one, and it is held to the wing it actually
draws.

**Standing.** Thirteen aircraft are stood on the ground in JSBSim, brakes on,
for twenty seconds, and the model under every wheel taking weight is on the
ground - above it by no more than what the fit left over, below it by no more
than that plus the gear's compression, which is real and not a fault: a rigid
model sinks by however far the legs squash, 0.07 m on the Cessna 172P and
0.62 m on the 747-400. The flying boat is not stood: it floats, and its hull
sits below the surface by its draught, so "the wheels on the ground" is not a
fact about it; its keels are held to the flight model's by the geometry check
instead.

**Verification run.** Locally, 272 of 272 tests pass in 351 s at `-j4`. The
four new checks are `the_committed_model_alignment_is_what_its_script_measures`,
`every_visual_model_is_aligned_to_the_aeroplane_it_draws`,
`each_visual_models_wheels_sit_on_the_ground_the_aeroplane_stands_on` and
`each_visual_model_is_where_its_flight_model_says_the_aeroplane_is`. Every one
was watched to fail: the A380's offset zeroed (its wheels then have no model
under them, and its nose contact is 3.29 m from it, against the 2.06 m it is
held to), a line removed (14 models, 13 alignments), an aircraft's wheel count
wrong (JSBSim puts weight on three where the alignment says two), an aircraft
given a shape contact it does not have, and a committed figure altered by
3 mm.


### Cesium Native's log off the client's standard output, 2026-09-21 — a fix

CI went red on Windows (run 35506857255) with

    no altitudes at tick 600 in the trace: trace tick 600 time 5.0000 lat
    -33.94[2026-09-20 11:23:17.706] [error] [SqliteCache.cpp:592] database is
    locked

**A log line had been written into the middle of one of the client's own.**
Cesium Native logs through spdlog, whose default logger writes to standard
output; the client's `--trace` goes to standard output too, and the test reads
it from there. The trace line was cut after `lat -33.94` and every number
after it was gone. `gfx::log_to_standard_error()` now puts the log on standard
error, where what goes wrong belongs, and the client calls it first thing.

Seven families of test parse the client's standard output and were open to
this - the HUD's two, the aircraft chosen, the AI's flight plan, the sky's
two and the floating origin's - and the same commit adds the check to the one
that failed: no line of the client's standard output carries a stamp, which a
log line does and none of ours does. **It was watched to fail**, with the
logger deliberately put back on standard output and a line written through
it: "something logged to the client's standard output, where its own output
goes".

The lock itself is not a fault: up to four tests run at once against one
Cesium cache file, SQLite refuses the write, and Cesium Native logs it and
carries on. That the tests share one cache with nothing serialising them is a
tail in `COMPLETION_PLAN.md`.

Verified locally: the five client tests on Vulkan pass, and the HUD test fails
with the deliberate bug in place.


### Visual models from FlightGear's aircraft, 2026-09-20 — item done

Phase 5's "visual models from FlightGear aircraft, each licence checked" is
done. **Fourteen of the sixteen aircraft ship a visual model, and nothing
draws any of them yet** - the renderer has never been handed one, and that is
the views item.

`tools/make_models.py` fetches FlightGear's aircraft, follows their model XML
to the geometry, flattens each aircraft's exterior into one mesh in the body
frame and writes `assets/models/<model>.mesh`. Every file it reads is pinned
by URL and SHA-256 in `assets/models/sources.txt` - 69 of them now - in the
four-field form `tests/cmake/fetch.cmake` reads, so the test fetches exactly
what the script converted. `src/gfx/model.cpp` reads the meshes back; it is
its own library, `glideslope_model`, because it is presentation that links
nothing presentational, so the unit tests read a model without SDL or Cesium
Native behind them.

| Model | Triangles | Length | Published | Span | Published |
| --- | --- | --- | --- | --- | --- |
| c172p | 45,451 | 8.17 m | 8.28 | 11.32 m | 11.00 |
| c182 | 30,169 | 8.50 m | 8.84 | 11.15 m | 11.00 |
| pa28 | 91,464 | 7.33 m | 7.25 | 10.69 m | 10.67 |
| j3cub | 51,387 | 6.86 m | 6.83 | 10.65 m | 10.74 |
| 737-300 | 48,318 | 33.30 m | 33.40 | 28.89 m | 28.88 |
| 747-400 | 24,382 | 70.94 m | 70.66 | 65.42 m | 64.44 |
| 787-8 | 28,759 | 56.68 m | 56.72 | 59.60 m | 60.12 |
| a320 | 107,449 | 37.61 m | 37.57 | 35.77 m | 35.80 |
| a380 | 17,278 | 73.51 m | 72.72 | 79.78 m | 79.75 |
| b2 | 5,612 | 21.21 m | 21.03 | 52.20 m | 52.43 |
| f15c | 162,347 | 19.51 m | 19.43 | 12.60 m | 13.05 |
| f22 | 29,949 | 18.92 m | 18.92 | 13.56 m | 13.56 |
| mosquito-fb6 | 12,506 | 12.49 m | 12.55 | 16.26 m | 16.51 |
| short_s23 | 85,604 | 26.82 m | 26.82 | 34.72 m | 34.75 |

16 MiB in all. Positions are quantised to 16 bits across each model's own
bounding box - under a millimetre on the largest - normals to signed bytes and
colours to unsigned, which is what keeps that figure down.

**The six that state no licence.** Eight of the fourteen come from a
FlightGear directory that states its terms, quoted per model in
`docs/ASSETS.md` with its source and revision - FGAddon at Subversion r21588,
and the c172p from the c172p team's own repository at commit `84477612`. The
other six - the A380, B-2, F-15, F-22, Mosquito and Short Empire - state none
at any level of their directory, and **the project owner decided on 2026-09-20
that they ship on FGAddon's project-wide requirement that its content is
GPL**, recorded in `ASSETS.md` as the basis: a policy, not a grant by the
author. Each entry also names what was looked at and found - which file names
the author, and what it says. One of the six turned out to say more than the
first pass found: the Short Empire's `Short_Empire-set.xml` and its model XML
each carry "Copyright (C) 2007 - 2025 Anders Gidenstam ... This file is
licensed under the GPL license version 2 or later", which is a grant, but it
is on those two files and not on the geometry, and its `AUTHORS` credits the
propeller models to the Boeing 314 and the engine model to the Lockheed-Vega
without terms. Two aircraft have no FlightGear model at all: there is no
Learjet of any mark in FGAddon, and the only F-35 is the F-35B, a different
airframe with a lift fan.

**The A380 needed a second geometry format.** Its fuselage and wing are AC3D
like every other aircraft here, but its horizontal tailplane, its four pylons
and its four engines are 3D Studio (`.3ds`), and without them it was a
fuselage and a wing. The script now reads both: a .3ds is a tree of chunks,
each a 16-bit identifier and a 32-bit length that counts its own header, and
only the chunks geometry needs are read. AC3D is authored with up along +Y
and 3D Studio with up along +Z, so a .3ds in the same aircraft is the AC3D
frame turned a quarter circle about X - the turn that keeps up pointing up,
since the other one would put it underground. Read as if it were AC3D, the
A380's tailplane is 2 m across and 30 m thick; read properly it is 30.38 m
across, against a published 30.37, and its pylons at 14.8 m either side come
out as exact mirrors of each other.

**A file's offsets place everything in that file**, its own geometry and its
`<model>` children alike, and the script applied them only to its own
geometry. The Short Empire found it: its model XML turns the aeroplane
through 180 degrees, so its four engines stood six metres ahead of the bow.
They now sit on the cowlings and nacelles its hull already carries. The same
bug had the A320's fuselage not getting the 0.831 m rise its engines were
given by `A320-common.xml` - a file with that offset and no geometry of its
own, which is the proof that the offset is meant for the children - so its
engines rode 0.831 m high. `a320.mesh` changes with this commit for that
reason and no other; its triangle count, length and span are unchanged.

**The model XML is parsed, not searched.** The script used to find tags by
searching the text, which meant stripping comments and Nasal by hand because
Nasal is code and can hold anything that looks like a tag. It now parses with
the standard library's ElementTree: comments go on their own, and Nasal is
skipped as the structure it is not. libxml2 through lxml would do as well and
is not installed; expat is in the standard library and this script is to have
no dependency of its own. The change was held to producing the eight existing
meshes byte for byte before anything else was done to it. It matters: the
A380's Nasal is not inside CDATA, so its jetway doors are real elements in
the tree.

**What each aircraft leaves out.** The walk is an allow-list - the entry
XML's own geometry and only the children named per aircraft - because
FlightGear hides the rest with animations this does not interpret. The six
new ones add: the A380's pushback tug, cabin, flight deck and light cones
left out and its wing, tailplane, belly fairing, pylons and engines taken;
the F-15C's missiles, bombs, rails and drop tanks, which are a payload
FlightGear picks and not the airframe, and its boarding ladder; the
Mosquito's propeller blades taken from `pdisk.ac`, because its airframe
carries hubs and no blades, without the two discs FlightGear blurs them into;
and the Short Empire's four propellers, four Pegasus Xc engines and cowling
gills taken, without their blur discs.

**The facing tests had to grow two exceptions.** Every model was held to
being wider at the tail than at the nose and to its highest point being its
fin. Neither holds for all fourteen: the Mosquito's two propellers stand at
its nose and are 3.8 m across, wider than its tailplane, and the B-2 is a
flying wing with no fin at all, whose highest point is its cockpit, 28% of
the way from the nose. The test now states both facts, names which model is
excused which and why, and counts that exactly one is excused each.

| Fact | Held to | Narrowest | Widest |
| --- | --- | --- | --- |
| The aft seventh is 1.3x wider than the forward seventh | all but the Mosquito, which measures 0.77 | the Cub and the c172p, 1.56 | the F-15C, 7.03 |
| The aft seventh reaches higher than the forward seventh by a fiftieth of the model's height | all but the B-2, which measures -21.2% | the F-22, 5.4% | the A380, 58.9% |
| It stands on its undercarriage: the lowest tenth spreads further along it than the highest tenth | the B-2 alone, 55% against 39% | - | - |

The B-2's rule is its alone because a taildragger drawn level has its
tailwheel well above its main wheels: the Cub's lowest tenth is its two main
wheels and spreads over 7% of it.

**What is left, and named elsewhere.**

1. **Nothing draws a model.** They are data on disk; the renderer has not
   been given one. That is the views item.
2. **No model's origin is aligned to its flight model's.** Each mesh is in
   its FlightGear aircraft's own frame, and that aircraft carries its own
   FDM: the Cub's and the 747's wheels land within 0.14 m of where JSBSim's
   contact points put them, but the c172p's are 1.03 m out and the A320's
   0.78 m. Placing a model on the aeroplane glideslope flies needs that
   offset per aircraft, measured and recorded; the views item is where it
   shows.
3. **No texture, so no livery**, and a surface takes the flat diffuse colour
   of its AC3D or 3D Studio material. Liveries are large and separately
   licensed, and the renderer has no texture path for a model yet.
4. **No animation.** Control surfaces, gear and propellers are welded where
   the model has them, gear down, propeller blades in place.
5. **The PA-28 is the wrong mark.** FlightGear has a PA-28-161 Warrior II;
   glideslope's flight model is the PA-28-180 Cherokee, which has the
   constant-chord wing rather than the Warrior's tapered one.
   `docs/ASSETS.md` says so.

**Verification run.** Locally, 268 of 268 tests pass in 416 s at `-j4`. The
five checks are `every_visual_model_that_ships_is_named_in_assets_md_with_its_source_revision_and_licence`,
`every_aircraft_the_data_holds_has_a_visual_model_or_a_named_reason`,
`each_visual_model_is_its_aircrafts_size_and_faces_the_way_it_flies`,
`a_model_file_that_is_damaged_or_of_another_version_is_refused` and
`the_committed_visual_models_are_what_their_script_writes`. Every one of them
was watched to fail: a licence row removed from an entry, a whole entry
removed, an aircraft's reason for having no model removed, an entry naming a
model that does not ship, a model mirrored, a model inverted, a committed
mesh with a byte flipped, and - with the reader's own checks broken in turn -
a model cut short and one holding an index past its vertices. The three
facing facts were each watched to fail on a deliberately broken model in this
round: the Mosquito mirrored nose to tail reaches 18.2% lower at its tail,
the c172p mirrored is 0.95 m wide at the tail against 1.77 at the nose, the
B-2 mirrored is 3.96 m wide at the tail against 26.12 at the nose, the F-22
turned upside down reaches 4.9% lower at its tail, and the B-2 turned upside
down spreads 39.0% at its lowest tenth against 55.4% at its highest. Removing
`a380.mesh` from the built data was watched to fail the two entry checks.

### The Short S.23 on water, 2026-09-20 — item done (CI run 35472220036)

Phase 5's "the Short S.23 on water, from JSBSim's model and its
hydrodynamics": the Short S.23 Empire flying boat of 1936, from Anders
Gidenstam's JSBSim model and its hull's and floats' hydrodynamics, written
into the data by `tools/make_short_s23.py`, takes off from and alights on
the sea and lakes where the DEM's water body mask puts them, and flies to
the figures Short Brothers and Flight published.

| Short S.23 | Measured | Published |
| --- | --- | --- |
| Take-off from calm water at 45,000 lb, from full throttle | 32.7 s, 825 yd | 30.5 s and 795 yd ± 10% (Gouge's tests, Flight 17 Dec 1936) |
| The same from the Tasman Sea off Sydney and from Lake Macquarie, on the DEM | 32.6 s, 819 and 820 yd | the same |
| Take-off at 40,500 lb, the standard boat | 25.0 s | 24 s (Gouge's Table II), 21 s (the specification) |
| Draught at the main step, afloat at 40,500 lb | 3.69 ft | 4.0 ± 0.5 ft, scaled from Flight's general arrangement |
| Level at 5,500 ft, +2 1/2 lb boost, coarse pitch | 202 mph | 200 ± 5% |
| Climb at sea level, +2 1/2 lb, fine pitch | 912 ft/min | 950 ± 10% (933 in Gouge's Table II) |

It alights on water, its nose on the step, and with its engines stopped
comes to rest afloat, its centre of gravity 7 ft above the water; the client
starts it `--on-ground` afloat, in Rose Bay, and refuses to stand a
landplane on water, where it would ditch.

**What it took.** The hull's hydrodynamics keep a water level of their own,
at JSBSim's sea level: glideslope now puts it where the terrain's water is
before every step - over the sea off Sydney 72 ft above JSBSim's sea level,
the ellipsoid - and out of reach over land, and a model with hydrodynamics
floats rather than ditches. On land the hull sank into the ground, meeting
it only at its bow, tail and wings; keel and float skids, as JSBSim's wheels
that water does not bear, now set it on its keel, heeled onto a float. Its
airscrews' pitch levers were the other way round from glideslope's
propeller control, and every take-off was made in coarse pitch, never
leaving the water; and its drag, a DATCOM estimate, flew it at 225 mph where
its maximum is 200, climbing half as fast again as it did, its engines
overspeeding to 945 hp. Times 1.9, the one change flies both figures and
lengthens the take-offs by no more than a tenth.

**What is missing first:** no draught, alighting run or take-off with a
wind or a sea running is published, and none is flown; the draught is
measured off a 1936 magazine's drawing, give or take half a foot. Its
stalling speed, 73 mph, is not flown: at its weight it needs more lift than
Gouge's own wind tunnel gave, so it was at some lighter weight or with
power, neither stated. The water is calm and all one density: no waves, no
current, and a lake as salt as the sea - the mask says ocean, lake or
river, not salt or fresh. And the client has no propeller or mixture lever
for the pilot: the S.23's airscrews stay in fine pitch and its mixture full
rich, through the gate to take-off boost, so that at full throttle in the air
its engines turn 3,185 rpm and give 1,185 hp where the Pegasus is rated at
2,600 and 920 - a tail in `COMPLETION_PLAN.md`.

### Water where the DEM says it is, and landplanes ditch on it, 2026-09-20 — item done (CI run 35449051367)

Phase 5's "Water where the DEM says it is": the ground under an aircraft is
water - the sea, a lake or a river - where the Copernicus DEM's water body
mask says it is, and there JSBSim is told its surface is not solid, so no
wheel takes weight on it. A landplane that comes down on water ditches: the
step any of its contact points, a wheel or a point of its structure, reaches
the water, it is brought to rest and held there with JSBSim's hold-down,
until it is started again.

**The mask.** Each Copernicus tile is published with a water body mask
beside it, `AUXFILES/..._WBM.tif`, on the same grid: bytes, 0 no water, 1
ocean, 2 lake, 3 river (the Product Handbook's table 8). The GeoTIFF reader
now reads 8-bit unsigned samples and the horizontal differencing predictor
they are published with, in every layout it reads floats in (16 layouts
more, tested), and refuses a float predictor on bytes or a float format on
eight bits by name. `world::Dem::water` gives a place its nearest sample's
value - the next tile's first row or column where that is nearer, the sea
where there is no tile - and refuses a mask with a value the handbook does
not give, or heights in its place. The client fetches each tile's mask as it
fetches the tile, checked against the bucket's MD5.

**Held to.** The pinned mask south of Sydney decodes to what an independent
decoder (Python's zlib, the differencing undone by hand) reads at eighteen
samples. At eleven places at least a kilometre from a shore, a Cessna flown
1,000 ft over each is over water or land as the mask says and the handbook's
class is the place's: the Tasman Sea off Bondi and off Maroubra, ocean;
Maroubra, Sydney airport, Hyde Park and Parramatta, land; Lake Macquarie and
Tuggerah Lake, lakes; Sydney Harbour, Botany Bay and Broken Bay, river - the
mask calls its harbours and bays, where rivers meet the sea, river. And
every one of the fifteen aircraft, set down at idle from 30 ft on water as
on land, ditches where it meets the water, moving not a hundredth of a foot
after, no wheel ever taking its weight, where on land it lands on its wheels
and rolls on.

**Why ditching, and not a landplane floating or sliding.** A first version
let the airframe's structure contact points meet the water, as they meet
the ground. Seven of the fifteen models have none - the 737-300, 747-400,
A320, B-2A, F-15C, F-22A and F-35A - and fell through the water; and of those
that have them, the J-3 Cub, resting on the two points at its wing tips,
rocked between them each step harder than the last until, a second after
touching, the water threw it 125 ft/s upward: point contacts as stiff as a
wing tip's, meeting water with a light aircraft's small inertia in roll, are
beyond a 120 Hz step. How an airframe meets water is hydrodynamics no model
here has, so a landplane ditches, and is still. The flying boat to come has
its hull's hydrodynamics and will not ditch.

### The HUD for fast aircraft, 2026-09-19 — item done (CI run 35449051367)

Phase 5's "the HUD for fast aircraft": below the bank, the HUD gives the
Mach number, to a hundredth, from Mach 0.40 - where airliners' displays begin
to show it - and the flight level, the pressure altitude to the nearest
hundred feet, from 18,000 ft, the United States' transition altitude, above
which altitudes are flight levels. Below either, the line is not there, and a
Cessna's HUD is as it was.

**Held to**, as the Cessna's HUD is: the A320, flown for five seconds from
11,000 m over Sydney, is shot, and `glideslope_hud_check` reads every line
back out of the frame and holds it to the aircraft's traced state at that
tick - the Mach number within half a hundredth, the flight level within half
a hundred feet - and requires each line where it applies and nowhere else: at
Mach 0.76 and 36,100 ft, both. The test checks the trace first that both
apply, so it cannot pass by showing neither. The Cessna's shot still has
neither line.

**What is missing first:** the flight level is JSBSim's pressure altitude,
and JSBSim reckons its atmosphere from its sea level, which with the DEM under
the aircraft is the WGS84 ellipsoid, not the geoid: the flight level is off by
the geoid's height there, 22 m (about 70 ft) at Sydney and at most about 110
m anywhere. No altimeter setting is modelled: the altitude line is the height
above the geoid, the flight level pressure's, and nothing switches between
them at the transition.

### Any aircraft chosen at start, 2026-09-19 — item done (CI run 35442214130)

Phase 5's "an aircraft chosen at start": `glideslope --aircraft ID` flies any
aircraft the data's catalogue holds - `glideslope_cli aircraft` lists them -
from its catalogue start in the air, and `--on-ground` stands it instead on
the ground at `--at`'s latitude and longitude, its engines idling and its
brakes on until B is pressed, for the pilot to take off. An aircraft the data
does not hold is refused, the ones it does named.

Every aircraft the data holds takes off: a test stands each of the fifteen on
a runway, opens its throttles over three seconds, steers it by rudder and,
below 60 knots, differential brake, and raises the nose at six tenths of its
catalogue airspeed - each climbs through 200 ft. The Mosquito needed the
technique its Pilot's Notes give, the power brought on steadily and the swing
met with brake: opened at once, its throttles ground-looped it, as they would
the real one. Each passes its published-figure checks, as the figure tests
show; and in the client the F-22 chosen flies at its start's 300 knots, and a
Cessna on the ground at Sydney stands on the DEM at rest.

**What is missing first:** the AI does not take off - `--on-ground` is flown
by the pilot, and `--autopilot` with it is refused; nothing chooses the
aircraft once the client is running; and the aircraft is still unseen, the
view the cockpit's, until the visual models and views arrive.

### The F-35A and B-2A written from what is published, 2026-09-19 — item done (CI run 35442214130)

Phase 5's "F-35A and B-2 flight models, written here from what is
published": the Lockheed Martin F-35A Lightning II and the Northrop Grumman
B-2A Spirit, which JSBSim does not have, written by `tools/make_f35a.py` and
`tools/make_b2.py`, land inside their tolerances on every figure published of
their performance - maximum speed, ceiling and range - and on nothing more,
as nothing more is published.

| Aircraft | Figure | Measured | Published |
| --- | --- | --- | --- |
| F-35A, 39,995 lb | Maximum Mach, full throttle, best altitude | 1.62 | 1.6 ± 0.05 ("Mach 1.6", no altitude given) |
| F-35A, 39,995 lb | Rate of climb at 50,000 ft | 5,950 ft/min | at least 100 ("above 50,000 feet") |
| F-35A, 49,120 lb | Range on internal fuel, 40,000 ft, Mach 0.8, no reserve | 1,830 nm | more than 1,200 |
| B-2A, 253,000 lb | Level at 40,000 ft, full throttle | Mach 0.89 | "high subsonic", read as 0.80 to 0.95 |
| B-2A, 177,160 lb | Rate of climb at 50,000 ft | 1,450 ft/min | at least 100 (ceiling 50,000 ft) |
| B-2A, 336,500 lb | Range, 40,000 ft, Mach 0.8, no reserve | 6,100 nm | about 6,000 ± 15% |

**Which of their behaviour no figure pins - nearly all of it.** Neither
aircraft's aerodynamics is public. The F-35A's lift and drag are a fighter's
of its wing (tools/fighter.py), their numbers set to fly the figures; its
moments are the F-4C's, from NASA CR-2144, the nearest published fighter's;
its inertias the F-4C's scaled; its engine's thrust with speed and height the
F100's, fitted to the F-15C's charts, and its fuel consumption an estimate.
Its fly-by-wire flight controls, and their limits and protections, are not
modelled: a pitch and a yaw damper stand for them, on an airframe given
positive stability. Its combat radius, 669 nm demonstrated (the Selected
Acquisition Report), is recorded but not flown, as no profile is given; nor
are its sustained turn and transonic acceleration, which DOT&E reports
were reduced without saying from what. The B-2A has less: every derivative is
a tailless swept wing's, estimated; its wing area and chord are estimates from
its span and length; its flight controls' directional stability - the drag
rudders against sideslip - is modelled as a gain; its engines' fuel
consumption is set to fly the range. Its speed is "high subsonic" and its
range on no stated profile. No take-off, landing or handling figure is
checked for either; they have no visual models.

**A new figure flight, the range.** The specific range, level at an altitude
and Mach with the throttle holding the Mach, is measured over two minutes at
full, half and nearly empty tanks, and integrated over the fuel by Simpson's
rule - as fast as a few minutes of flight, where flying the whole range would
take the B-2 twelve hours.

### The Learjet 35A written from its flight manual, 2026-09-19 — item done (CI run 35442214130)

Phase 5's "a Learjet 35A flight model, written here from published data":
the Gates Learjet 35A, which JSBSim does not have, written by
`tools/make_learjet35a.py` from its FAA-approved flight manual (FM-108), its
type certificate, NASA's full-scale wind tunnel test and flight
identification of its forebear, the Learjet 23, and Gates Learjet's drag
analysis of the Learjet 25, lands inside its tolerances on seven figures -
the airliners' four and three of its flight manual's stall speeds.

| Learjet 35A | Measured | Published |
| --- | --- | --- |
| Take-off field length, 18,300 lb, flaps 8, sea level, ISA | 5,150 ft | 5,300 ± 10%, the AFM's chart |
| Engine-out climb, V2, flaps 8 | 7.6% | at least 2.4 (AFM 5-6, FAR 25.121(b)) |
| Level at 41,000 ft, full throttle, 15,300 lb | Mach 0.82 | 0.81 ± 0.03 (the C-21A fact sheet) |
| Climb at 45,000 ft, as light as it flies | 2,390 ft/min | at least 300 (its maximum operating altitude) |
| Stall, flaps 8, 15,800 lb | 107.4 KCAS | 109.0 ± 3, the AFM's printed example |
| Stall, flaps up, 15,300 lb | 118.6 KCAS | 119 ± 3, the AFM's chart |
| Stall, flaps 40, 15,300 lb | 96.2 KCAS | 96.5 ± 3, the AFM's chart |

**What is missing first:** the flight manual has no cruise or climb-rate
charts; the cruise is the Air Force's fact sheet's, and the ceiling only a
floor. Its lateral and longitudinal derivatives are the Learjet 23's, a
smaller wing's, taken as the 35A's; its damping in roll and yaw, side force,
inertias, empty centre of gravity and engines' thrust with height and speed
are estimates, each named in the script; its wing area, which no 35A document
gives, comes from its certified chord and span and the 23's planform. The
lift's maximum at each flap setting is set from the flight manual's stall
speeds, which the stall figures then fly - they check the pitch control and
the stall's dynamics, not the lift. It has no stick pusher, no Mach trim and
no trimmable stabiliser. No landing is flown. It has no visual model.

**The scripts that write a model whole share their XML** in
`tools/written.py`: the A380's, moved there, writes the same files as before.

### The A380 written from its documents, 2026-09-19 — item done (CI run 35435906751)

Phase 5's "an A380 flight model, written here from published data": the
Airbus A380-841, which JSBSim does not have, written by `tools/make_a380.py`
from Airbus's A380 Aircraft Characteristics (revision 20, December 2025), the
EASA and FAA type certificates, the Trent 900's engine certificate, JSBSim's Trent 900
engine and the Boeing 747's stability derivatives published in NASA CR-2144, lands inside
its tolerances on the same four figures as the airliners.

| A380-841 | Measured | Published |
| --- | --- | --- |
| Take-off field length, 575,000 kg, ISA, sea level, CONF 2 | 9,300 ft | 9,734 (2,967 m) ± 10%, the AC's chart |
| Engine-out climb, V2, CONF 2 | 4.2% | at least 3.0 (JAR 25.121(b)) |
| Level at 35,000 ft, full throttle, 394,000 kg | Mach 0.91 | 0.85 to 0.92 (cruise Mach 0.85, Mmo 0.89) |
| Climb at 43,000 ft, as light as it flies | 1,800 ft/min | at least 300 (A58NM's maximum operating altitude) |

**What is missing first:** much of the A380 is pinned by no figure. Its
stability and control derivatives are the 747's, the nearest aircraft of its
kind whose derivatives are published; its flight controls are a conventional
aircraft's with a yaw damper, not the A380's fly-by-wire laws, whose
protections - the pitch, bank and angle-of-attack limits - it does not have.
Its drag at zero lift, span efficiency, flaps' lift and drag, stall and drag
rise are set to fly the figures; its mean aerodynamic chord, operating empty
weight, inertias, tank positions and gear springs are estimates, each named
in the script. No source gives a flap setting for the take-off chart; it is
flown in CONF 2. The landing is not flown - the autopilot cannot land, a
Phase 8 item - though the full flaps' lift is set from Airbus's final approach
speed. Its body gears do not steer. It has no visual model, and the HUD gives
it no Mach or flight level yet.

**Its operating empty weight comes from its payload-range chart.** The AC
gives none; its chart for the Trent 900 carries a structural payload of 84 t
to the maximum take-off weight and, at full tanks, 34.6 t: only the 575 t
variant's zero-fuel weight, 369 t, makes the two agree, at 285 t.

### The F-15C and F-22 against their published figures, 2026-09-19 — item done (CI run 35430601204)

Phase 5's "the F-15 and F-22 fly to their figures": the McDonnell Douglas
F-15C Eagle and the Lockheed Martin F-22A Raptor, each made from JSBSim's
model by a script listing every change and why, land inside their
tolerances on their maximum Mach at altitude, their climb, their ceilings
and their sustained turn at Mach 0.9 and 30,000 ft - the F-15C's six figures
from the Air Force's Standard Aircraft Characteristics (1992), the F-22's
five from the Department of Defense's Selected Acquisition Report (2010) and
the Air Force's fact sheet.

| F-15C | Measured | Published |
| --- | --- | --- |
| Maximum Mach at 45,000 ft, clean, 36,946 lb | 2.40 | 2.39 ± 0.08, page 6's chart |
| Rate of climb at sea level, military power, 45,713 lb | 15,300 ft/min | 15,250 ± 10% |
| Rate of climb at sea level, maximum power, 41,286 lb | 56,900 ft/min | 55,960 ± 10% |
| Service ceiling, 100 ft/min, military power, 45,713 lb | 46,900 ft | 46,750 ± 5% |
| Combat ceiling, 500 ft/min, maximum power, 41,286 lb | 57,600 ft | 56,100 ± 5% |
| Sustained turn, Mach 0.9, 30,000 ft, clean | 7.82 °/s | 7.87 ± 0.5 (3.95 g, page 5's chart) |

| F-22A, 54,960 lb | Measured | Published |
| --- | --- | --- |
| Supercruise, military power, best altitude | Mach 1.77 | 1.76 ± 0.05, demonstrated |
| Acceleration, Mach 0.8 to 1.5 at 30,000 ft | 52.7 s | 52.4 ± 10%, demonstrated |
| Sustained turn, Mach 0.9, 30,000 ft | 7.35 °/s | 7.34 ± 0.5 (3.7 g, demonstrated) |
| Level at 40,000 ft, full throttle | Mach 2.15 | "Mach two class", read as 1.95 to 2.35 |
| Rate of climb at 50,000 ft | 8,850 ft/min | at least 100 ("above 50,000 feet") |

**What is missing first:** the F-22's figures are few, and much of it is
pinned by none: its military thrust is JSBSim's, unpublished; its drag's
shape past Mach 1.2 and its engines' behaviour with speed and height are the
F-15C's, which the F-15C's charts pin; its pitch loop's gain schedule and its
roll stick's shaping are this project's, needed to fly it, and not the
F-22's, which are not public; its thrust vectoring is JSBSim's and flown by
no figure. Neither fighter's take-off, landing or stall is checked. The
F-15C's page 4 figures are for four AIM-7Fs, whose drag the model does not
carry. Past the flow leaving the wing, the drag is estimated - the lift times
the tangent of the angle of attack - and no figure flies there. The HUD
shows neither Mach nor g yet (its own Phase 5 item); neither has a visual
model.

**The F-15C is fitted to its charts.** Its thrust and drag with Mach are
fitted, twelve numbers, to thirty-three points read from the Standard
Aircraft Characteristics' chart of specific excess power at maximum power,
30,000 to 59,000 ft and Mach 0.9 to 2.4, and to its figures: the model meets
the chart to 27 ft/s, about what the chart can be read to. The fit needed an
engine that gives more thrust in cold air than Mattingly's typical
afterburning turbofan - the chart's excess power at Mach 0.9 and its
manoeuvrability chart's sustained load factor together pin the thrust there
a fifth above his - and loses some above the tropopause; `tools/fighter.py`
has the form, Mattingly's with his typical engine as one case.

**JSBSim's fighters did not fly to anything.** The F-15's lift fell to a
quarter between its only two Mach columns, 0.5 and 1.4, and its drag at zero
lift halved; its afterburner answered only a throttle past 1, which
glideslope's never reaches; the idle thrust JSBSim adds to the military
table was a quarter of the thrust at height. The F-22 could not be flown at
all: four of its actuators had a lag of 0, which JSBSim takes as a filter
that never moves, so every control surface stayed where it started. With
them moving, its lift and tail did not change with Mach; its lift-dependent
drag was a span efficiency of 1.9; its throttles passed 0.99 to its engines
as 0.991, lighting the afterburners at military power; its pitch loop went
into a limit cycle, the stabilators beating at their rate limit three times
a second, past Mach 2.06 at 36,000 ft; and its roll stick commanded 22
degrees a second in its first tenth of travel, so that any small correction
rolled it over. Each is set right in its script, with the reason.

**The simulation learned to measure a fighter.** Fuel can be frozen, so a
figure is flown at its weight. Four new figure flights: the greatest rate of
climb as a level acceleration's specific excess power, the flight test's way,
counted only once the aircraft is level at 1 g; the service and combat
ceilings, where that falls to 100 or 500 ft/min; the time to accelerate
between two Mach numbers; and the level Mach at the best altitude. The
sustained turn, which chased Mach with bank and never settled on the F-15, is
now the load factor at which the aircraft neither gains energy nor loses it,
found by halving - the load factor a manoeuvrability chart plots. A
calibrated airspeed past Mach 1 is found through Rayleigh's pitot formula,
behind the shock the pitot tube stands in.

### The airliners against their planning documents, 2026-09-19 — item done (CI run 35423458464)

Phase 5's "the airliners fly to their figures": the Airbus A320 and the
Boeing 737-300, 747-400 and 787-8, each made from JSBSim's model by a script
listing every change and why, land inside their tolerances on the take-off
runway length at maximum weight from their manufacturers' airport-planning
documents, the climb with an engine out their certification basis demands,
their cruise Mach, and their type certificates' ceilings.

**What is missing first:** these are four figures each, and much of each
aircraft is pinned by none of them. No approach or landing is flown - the
planning documents' landing lengths are not yet checked, and the autopilot
cannot land (a Phase 8 item). The climb is FAR 25.121(b)'s floor, not a
climb rate: Boeing publishes none, and Airbus's climb table (Getting to
Grips with Aircraft Performance) is for the V2500 A320, not the CFM. The
cruise is a band - above the published cruise Mach, and no more than 0.03 past
Mmo - not a speed at a thrust. The airliners fly only from the catalogue; the
HUD gives them no Mach or flight level yet (its own Phase 5 item); they have
no visual models. Some of what they are made of is estimated, and the scripts
say so: the engines' thrust with height and speed (Mattingly's lapse for a
high-bypass turbofan), the drag rise past the critical Mach (Lock's
fourth-power law), a windmilling fan's drag, a dry runway's braking, and the
lift and drag the take-off flaps and slats give, set where the take-off
figures need them.

| Aircraft | Take-off runway, maximum weight | Engine-out climb | Cruise Mach, full throttle, 35,000 ft | Climb at its ceiling |
| --- | --- | --- | --- | --- |
| A320 (-214), 73,500 kg | 6,200 ft against 5,850 (±10%) | 3.7%, at least 2.4 | 0.84, 0.78 to 0.85 | 2,550 ft/min at 39,100 ft |
| 737-300, 135,000 lb | 7,790 ft against 8,400 | 2.4%, at least 2.4 | 0.78, 0.74 to 0.85 | 1,540 ft/min at 37,000 ft |
| 747-400, 875,000 lb | 11,270 ft against 10,500 | 4.0%, at least 3.0 | 0.91, 0.85 to 0.95 | 1,840 ft/min at 45,100 ft |
| 787-8, 502,500 lb | 10,880 ft against 10,100 | 2.6%, at least 2.4 | 0.91, 0.85 to 0.93 | 2,730 ft/min at 43,100 ft |

The runway lengths are read from the planning documents' charts, to about
100 ft; the ceiling is climbed to at the lightest weight flown, the
operating empty weight and a tenth of the fuel, with the 300 ft/min that
makes an altitude a thrust-limited ceiling.

**The take-off runway length is FAR 25's, flown.** A new figure flight takes
the take-off speeds from the aircraft's own stall at the take-off flap and
weight - V2 1.2 times it, VR 5% below - and flies every engine to 35 ft, and
then the balanced field: an engine failing at a speed found by halving,
going on to 35 ft and, a second later, stopping, the brakes, throttles and
speedbrakes a second apart as FAR 25.109 allows. The runway length is the
longer of 115% of the first and the second. Where the aircraft cannot climb
at FAR 25.121(b)'s gradient with an engine out at 1.2 times its stall, V2
rises until it can, as a flight manual's "improved climb" does - which is
why the 737-300's chart bends sharply upward above 130,000 lb, and why the
737-300 here flies V2 at 1.24 times its stall.

**JSBSim's airliners did not fly.** None could be loaded to its maximum
weight - the 737 had nowhere to put a payload, the 747's tanks held a
seventh of its fuel and its empty weight carried the rest. Their engines
kept their thrust far too well with height and speed, and with no drag rise
worth the name all four flew past Mach 0.85 at nine-tenths throttle at
35,000 ft, the 747 to 0.97. A failed turbine relit itself; a stopped one had no drag. The
A320's static margin was 70% of its chord, so that it could not be flown
slower than 173 knots with its take-off flaps, and its drag rose with angle
of attack at twice a wing's; the 787's rudder pedals moved its rudder 3.6
degrees; the 747's nose gear let it sit 3 degrees nose down; the 737 opened
a telnet port and a UDP port whenever it was loaded, which a test now
forbids of every model. Each is set right in its script, with the reason.

**The simulation learned two things:** a jet's failed engine is cut off, not
only stopped, so that it cannot relight; and a speedbrake lever works an
aircraft's flight and ground spoilers. Besides the take-off runway length,
the figures have three new flights - the engine-out climb, the level Mach at
full throttle and the climb at a ceiling - and the stall flight runs long
enough for a jet's entry speed, a second a knot; the Cessnas' 70-knot entry
runs the eighty seconds it did.

### An empty answer is tried again, 2026-09-19

CI's live weather check failed on Rocky Linux (run 35421371495) when one of
the weather services, after two minutes of trying, answered 200 with nothing
in it, and the empty answer reached the JSON reader as "not a value". Nothing
glideslope fetches is ever empty - aviationweather.gov says it has no report
with a 204 - so a 200 with nothing in it is now tried again, as a server's
error is, and a test holds it so.

### A start on the ground puts the wheels on it, 2026-09-19

Found starting the airliners. An aircraft started on the ground had its
centre of gravity set at the ground's height, which left its wheels as far
under the surface as they hang below it: four feet for the Cessna 172P,
which its struts threw back into the air, and nine for the A320, which they
threw hard enough to end its flight in NaNs on its first seconds. The ground
tests had worked around it for the Cessna alone, with a rest height of 4.4
ft set by hand. `Aircraft::initialize` now raises an aircraft started on the
ground by its deepest wheel's compression, so that wheel starts touching and
the aircraft settles onto its struts, whatever the aircraft and whatever the
ground; the ground tests set the Cessna down at the surface itself, on level
ground and on slopes. Every published figure flown from the ground lands
where it did - the brakes held each aircraft for eight seconds or more, long
enough for the bounce to die - but for the Mosquito's take-off over 50 ft,
2,516 ft to 2,520, and its swing, 2.59 lb/sq in to 2.58. The selftest, which
starts on the ground, has a new hash for it: on this machine,
`e999a51640cdff03` to `d36123c1eecc3e23`.

### The Piper J-3 Cub against its manual, 2026-09-19 — item done (CI run 35417893114)

The last of the three light aircraft. With it, Phase 5's "the light aircraft
fly to their figures" is done: the Cessna 182S, the Piper PA-28-180 and the
Piper J-3 Cub each land inside their tolerance on every figure recorded from
their handbooks, as the Cessna 172P does, forty-four figures in all across
the five aircraft with them.

**What is missing first:** the Cub's figures are few and loose. Its manual
and Piper's 1945 booklet give five that can be flown, in mph, none with an
altitude or an airspeed calibration, and only the climb with a weight; no
take-off distance, top speed or ceiling a flight here measures. Like the
Cessna 182S and the PA-28, it flies only from the catalogue - the client
still flies the Cessna 172P - and it has no visual model.

**JSBSim's J3Cub did not fly to the Cub's figures.** Its engine made its 65
hp at 2,800 rpm, 500 past the A-65's limit; its propeller was JSBSim's
generic fixed-pitch one, the Cessna 172P's numbers, so coarse that on 65 hp
it turned 1,590 rpm static, against the type certificate's 1,950 to 2,250;
its drag at zero lift was its wing section's and its gear's alone, and it
glided at 13.5 to 1, against 10; and its elevator moved 8 degrees each way,
not the type certificate's 34 up and 29 down, too little to raise the nose to
the wing's stall. `tools/make_j3cub.py` makes glideslope's from the pinned
files with each change listed and justified, and a test fails if the
committed files differ from what it makes.

| Figure | Manual | Range | glideslope |
| --- | --- | --- | --- |
| Static RPM, full throttle | 1950 to 2250 (A-691) | the same | 2060 |
| Climb, full load, 55 mph | 450 ft/min | ±10% | 437 |
| Cruise, 2,150 rpm | 73 mph, 63.4 KTAS | ±3 kt | 64.6 |
| Glide, 55 mph | 10:1 | ±10% | 9.63 |
| Stall | 38 mph, 33.0 KCAS | ±2 kt | 34.3 |

The stall is flown at 1,092 lb - the Trainer's instructor and pupil and full
fuel - not the 1,220 lb gross weight the others are: the manual names no
weight for its 38 mph, the booklet gives it as the Trainer's landing speed,
and at 1,220 lb it would take a lift coefficient of 1.85, more than the
Cub's flapless wing gives (the model stalls at 36 knots there).

**What the changes are, in short:** the booklet's empty weight and the type
certificate's seats, baggage and tank; its elevator travel; drag for the
fuselage, tail, struts and wires, and a light aircraft's induced drag; the
engine's 2,300 rpm limit; and the propeller's 74 in, its curves drawn in on
the advance ratio for a 45 in pitch and scaled for the static rpm, climb and
cruise.

**The figure flights learned two things.** A figures file can say an
aircraft has no flaps, and a figure asking for them is refused. And the
cruise flight stops leaning once the rpm has fallen 3% below its best: the
Cub's engine is at its best full rich, and leaned on down to half it
stopped. The Cessna 172P's cruise moved from 119.7 to 119.4 knots with it,
the PA-28's from 125.1 to 124.5.

### The Piper PA-28-180 Cherokee against its handbook, 2026-09-19 — part of an item

The second of the three light aircraft in Phase 5's "the light aircraft fly
to their figures", after the Cessna 182S; the Piper Cub, above, completes the
item.

**JSBSim's pa28 is the right airframe with the wrong propeller.** Its 30 ft
constant-chord wing, fixed gear, 40-degree flaps and 180 hp engine are the
1962-72 Cherokee 180's, but it had a constant-speed propeller, which only the
retractable Arrow has; one seat and one tank, so the handbook's 2,400 lb could
not be loaded; main wheels that castored, as the 182's did; flaps whose
first detent was 15 degrees, not 10, and gave less lift at 25 than at 10; and
a tail half as powerful as a stabilator's, too weak to lift the nose for
take-off or to reach the stall with full flap. It could not be flown to the
handbook as it stood, so there is no column for it below. `tools/make_pa28.py`
makes glideslope's from the pinned files with each change listed and
justified, and a test fails if the committed files differ from what it makes.

| Figure | Handbook | Range | glideslope |
| --- | --- | --- | --- |
| Static RPM, full throttle | 2275 to 2450 (TCDS) | the same | 2348 |
| Take-off ground roll, flaps 25 | 720 ft | ±10% | 747 |
| Climb, sea level, 85 mph | 725 ft/min | ±10% | 712 |
| Cruise, 7,000 ft, 75%, 2,640 rpm | 143 mph, 124.3 KTAS | ±3 kt | 124.5 |
| Top speed, sea level | 152 mph, 132.1 KTAS | ±3 kt | 132.0 |
| Stall, flaps up | 67 mph CAS, 58.2 KCAS | ±2 kt | 57.4 |
| Stall, flaps 40 | 57 mph CAS, 49.5 KCAS | ±2 kt | 48.8 |

The handbook is the Cherokee 180 "E" Owner's Handbook (1969, revised 1974),
in mph with no airspeed calibration; the stalls, as calibrated, are the 1962
Airplane Flight Manual's, and the static rpm the type certificate data
sheet's, 2A13. Neither gives a glide ratio, so there is no glide figure.

**What the changes are, in short:** the handbook's empty weight, seats and
tanks; flaps at 10, 25 and 40 degrees; fixed main wheels; a fixed-pitch 76 in
propeller, with less power and more thrust at low advance ratio for the
static rpm and the take-off; the 172's gear dampers; a stabilator's moment
and lift; a lift curve and flap lift reaching the flight manual's stalls;
induced drag with ground effect, which the model had on its lift and not its
drag; and its zero-lift and gear drag, for the top speed.

**glideslope raised fixed gear.** Every flight begun in the air puts the gear
lever up, and glideslope set the gear's position from it whatever the gear
was. The Cessnas never noticed; the pa28 charges its gear's drag by that
position, and with its fixed gear "up" it shed more than half its zero-lift
drag - 33 knots at full throttle, 140 against 107. Fixed gear now stays down
whatever the lever says, and a test holds it there.

### The Cessna 182S against its handbook, 2026-09-19 — part of an item

The first of the three light aircraft in Phase 5's "the light aircraft fly
to their figures"; the Piper Cub and the PA-28 Cherokee 180 follow, and the
item is not done until all three are.

**JSBSim's c182 did not fly to the 182S handbook**, and on the ground it did
not fly at all. Its main wheels castored (`max_steer` 360), so nothing held
it straight: at full throttle it turned circles, and lightly loaded its
state went to NaN. It is JSBSim's c172 with a bigger engine - the same
inertias, geometry, centre of gravity and, largely, aerodynamics - and its
flaps stopped at 30 degrees where the 182S's go to 38. `tools/make_c182.py`
makes glideslope's from the pinned files with each change listed and
justified, and a test fails if the committed files differ from what it
makes.

| Figure | Handbook | Range | JSBSim's, with the handbook's weights and flaps | glideslope |
| --- | --- | --- | --- | --- |
| Static RPM, full throttle | 2300 to 2400 (TCDS) | 2300 to 2405 | 2400 | 2400 |
| Take-off ground roll, flaps 20 | 795 ft | ±10% | turned circles | 793 |
| Climb, sea level, 80 KIAS | 924 ft/min | ±10% | 818 | 895 |
| Cruise, 6,000 ft, 80%, best power | 140 KTAS | ±3 kt | - | 138.8 |
| Maximum speed, sea level | 145 KTAS | ±3 kt | 136 | 144.0 |
| Glide, 75 KIAS | 8.9:1 (read from a chart) | ±10% | 9.76 | 8.93 |
| Stall, flaps up | 54 to 56 KCAS | 52 to 58 | 59.6 | 55.3 |
| Stall, flaps 20 | 50 to 52 KCAS | 48 to 54 | 54.7 | 51.4 |
| Stall, flaps full | 49 to 50 KCAS | 47 to 52 | 54.0 | 49.8 |

The handbook is the Cessna Model 182S Skylane Information Manual (1997), with
its airspeed calibration table converting indicated speeds; the static rpm
and the flaps' travel are from the type certificate data sheet, 3A13.

**What the changes are, in short:** the handbook's empty weight, seats and
tanks; flaps to 38 degrees; fixed main wheels; a lift curve reaching the
handbook's stalls; the elevator's drag cut to what a tailplane that size
gives in trim (JSBSim charged it a fifth of all the drag at full speed); the
drag at incidence raised 15%, standing in for the windmilling propeller the
handbook glides with - in JSBSim the propeller of a stopped engine stops, its
tables ending where it would windmill; a stopped engine's friction; and 12%
more propeller thrust at low advance ratio, for the take-off.

**The figure flights learned three things.** A cruise figure can name a
manifold pressure and have the mixture leaned for best power, as Cessna's
tables do, and a level speed can be in knots. And a stall is measured to the
point where the stalled aircraft gathers speed again: the 182, with the stick
held back past its stall, dived and zoomed, and the lowest speed of the zoom
had been taken for its stall. The Cessna 172P's stalls and the Mosquito's
measure what they did.

### The Mosquito FB Mk VI, 2026-09-19 — item done (CI run 35409102752)

**What is missing first:** the Mosquito flies only from the catalogue - nothing
yet lets a pilot choose it at the start (a Phase 5 item of its own) or work
its extra controls from the keyboard: the propeller levers, the supercharger's
gear change switch, the radiator shutters and the undercarriage are in
`sim::Controls` and the model, but only the figure flights and the autopilot's
throttle move them. It has no visual model yet. Its handling at the stall and
on one engine is set from the Pilot's Notes' words and numbers, not from any
flight test of its derivatives, which were not found.

**The model** - `assets/jsbsim/aircraft/mosquito-fb6/`, the Merlin 25 in
`assets/jsbsim/engine/merlin25.xml` and the propeller in
`assets/jsbsim/engine/prop_dh_hydromatic.xml` - is written here from the
aircraft's trials and Pilot's Notes (`docs/ASSETS.md` records each source);
no other simulator's Mosquito is used. Every number in it says where it comes
from, what was estimated, and what was set to meet a figure. The propeller's
tables are computed by blade-element momentum theory in
`tools/make_mosquito_propeller.py`, and a test fails if the committed file
differs from what it makes.

| Figure | Published | Range | glideslope |
| --- | --- | --- | --- |
| Level, sea level, +18, MS gear | 332 mph TAS (HX809) | ±2% | 331.0 |
| Level, 5,100 ft, MS full-throttle height | 353 mph | ±2% | 353.4 |
| Level, 12,500 ft, FS full-throttle height | 363 mph | ±2% | 363.8 |
| Level, 18,000 ft, FS full throttle | 357 mph (read from fig. 1) | ±2% | 359.9 |
| Climb, 10,400 ft, MS gear, radiators open | 1,740 ft/min (HJ679) | ±10% | 1,747 |
| Climb, 17,000 ft, FS gear | 1,440 ft/min | ±10% | 1,346 |
| Time to 20,000 ft | 12.85 min | ±10% | 13.81 |
| Stall, clean, power off, 18,000 lb | 105 KIAS (Pilot's Notes 1950) | 100 to 110 | 105.6 |
| Stall, wheels and flaps down | 95-100 KIAS (1950), 96 at full load (1944) | 89 to 100 | 92.0 |
| Take-off over 50 ft, B Mk IV weight and boost | 795 yd, 2,385 ft (B IV data sheet) | ±10% | 2,516 |
| Swing on take-off: the port throttle's lead | "slightly ahead" (1944) | 0.2 to 3 lb/sq in | 2.59 |
| Safety speed, +18, 17,000 lb | 170 KIAS (1950) | ±10 kt | 174.9 |
| Safety speed, +9 | 155 KIAS | ±10 kt | 147.5 |
| Single-engine ceiling, 20,500 lb | 12,000 ft (1950) | ±2,000 ft | 12,480 |

The engine against Rolls-Royce's own curve (AVIA 6/5817 fig. 1, +18 and 3,000
rpm at 400 mph): MS gear 1,542 bhp at sea level (1,540) and 1,613 at 6,000 ft
(about 1,600); FS gear 1,446 at 10,000 ft (1,450), 1,469 at 12,000 (1,468),
1,378 at 16,000 (1,376) and 1,194 at 20,000 (1,164). Full-throttle heights at
HX809's speeds: about 5,200 ft and 12,700 ft, where it measured 5,100 and
12,500.

**What JSBSim does not model, and the Mosquito needed.** Each of these was
found by flying a figure and asking why it missed:

- *The propeller.* JSBSim's P-51D tables, the first basis, gave an efficiency
  above 1 at the Mosquito's speeds. The tables are now computed for a
  three-bladed 12 ft blade of estimated planform, with tip Mach tables and a
  5% installation loss; a wide "paddle" blade (activity factor about 140),
  because narrower ones took the take-off power only by stalling.
- *The Merlin's power with boost and height.* JSBSim's mixture table makes the
  most power at a fuel-air ratio of 0.10, so the Merlin's enrichment above +9
  would have made more power, not less; a table with its best power at 0.08
  gives +9 the climbing power the engine family was rated at. JSBSim takes the
  charge at the outside air's temperature; the ram and the impeller heat it,
  which makes the power rise and fall with height as Rolls-Royce's curve does.
  The supercharger changes gear by aneroid at 7,000 ft, as the Notes say, and
  in low gear the figure flights climb until the boost has fallen 2 lb/sq in,
  as the Notes tell the pilot to.
- *A dead engine.* JSBSim charges a running engine its friction but not a
  stopped one, so a failed engine's windmilling propeller dragged about 100
  lbf; charged the same friction, about 550 lbf at 195 knots - "the drag of a
  windmilling propeller is very high".
- *The slipstreams.* The propellers wash half the wing: at take-off power
  that lifts the wing behind them, which is what lets the model leave the
  ground as the B IV's figure needs while its power-off stalls stay at the
  Notes' speeds, and the Notes stall it "power on under typical approach
  conditions" 5 knots slower. Over the tailplane's outer parts it raises the
  tail on the take-off run, and on the fin it pushes the nose to port.
- *The swing.* On the ground with the tail down, the propellers' torque has a
  component about the vertical that turns the nose to starboard, and the tail
  wheel castors and does not resist it. The slipstream on the fin outweighs
  it slightly, and the swing is to port, as the Notes say.
- *The stall.* Without a nose-down moment past the stall the model locked
  into a deep stall at 49 degrees; with one, "the nose drops gently".

**Set to meet a figure, and so not checked by it:** the drag at zero lift
(the level speeds), the lift curve's peaks (the stalls), the radiator
shutters' drag (between the two climbs), the slipstream's push on the fin (the
swing), and the rudder's power (between the two safety speeds - the fin's
area was not found). The model puts the two safety speeds further apart than
the Notes do: with the rudder set between them, +18 is 5 knots fast and +9 8
knots slow. The take-off is 5.5% long against a different mark's figure.

**How the stalling speeds were taken.** The Notes give indicated airspeeds;
the model reports calibrated. HJ679's measured position error (de Havilland,
20 September 1943, fig. 3) is under a mile an hour at the lowest speed it
reached, 180 mph, so the stalls are taken as calibrated.

**The tests:** one per figure, fourteen more (`tests/unit/test_figures.cpp`),
flown by the same flights as the Cessna's where they fit and by new ones -
level speed, time to height, take-off distance to 50 ft, the swing, the safety
speed, the single-engine ceiling - where they do not. A figure names its
flight and its loading; the Cessna's nine measure exactly what they did. The
catalogue test holds the Mosquito at 3,000 ft and 220 KCAS on the autopilot.
**Watched to fail**, each by breaking one thing in a copy of the model:
without the slipstreams' lift the take-off is 2,966 ft, out of range; with
both propellers turning the other way the swing is to starboard (a lead of
-2.88 lb/sq in); without a stopped engine's friction the safety speed at +9
is 136 knots; and without the nose-down moment past the stall the aircraft
settles at 49 degrees of incidence, nose up, which a test of its own catches
(`tests/unit/test_handling.cpp`).

**Found on the way:** JSBSim starts a running engine by stepping it half a
second at a time, which a constant-speed propeller's governor cannot follow -
it drove the blades to full coarse and stalled the engine. `Aircraft::initialize`
now starts those engines itself. A failed engine is stopped with its ignition
off, since JSBSim restarts one that windmills with spark and fuel.

### Aircraft as data, 2026-09-19 — item done (CI run 35387301607)

**What is missing first:** the catalogue holds one aircraft, the Cessna 172P;
the rest of Phase 5 adds the others, each held to its published figures
first. Nothing chooses an aircraft yet - the client flies the catalogue's
Cessna - and the catalogue says nothing of how an aircraft looks, sounds or
is started on the ground.

**The catalogue** (`assets/aircraft`, read by `sim::read_catalogue`) is a file
to an aircraft: its name, its JSBSim model, and the airspeed and throttle a
flight begun in the air starts with. Its published figures and its selftest,
if it has them, are found by its model's name. The client looks its aircraft
up there rather than naming the Cessna in code, and starts it at the
catalogue's airspeed and throttle; `glideslope_cli aircraft` lists what the
data holds.

**The tests:** a copy of the data with one `.aircraft` file more - another
aircraft on the Cessna's model, started at 80 KCAS - holds one aircraft more,
which loads and, on the autopilot, holds its start; every aircraft the data
holds does the same at 3,000 ft for a minute; and a file with a command it
does not know, a throttle past full, no start, or a model the data does not
hold is refused, by its line. **Watched to fail:** the catalogue finding any
aircraft for any name, and reading no files.

**Found on the way: a server hanging up could end the client.** The package
job's client, flying under gdb, stopped on SIGPIPE in a terrain download
(package run 35387301586). libcurl's requests are made with CURLOPT_NOSIGNAL,
as threads must, and then ignoring SIGPIPE - raised when TLS writes to a
connection the server has closed - is the program's job; nothing did, and
by default it ends the process. The platform's HTTP now ignores it when it
first loads libcurl, a test checks it is ignored once a download is made
(watched to fail), and gdb in the package job passes it through.

### `--autopilot`: the client's AI flies, 2026-09-19 — item done (CI run 35378850716)

**What is missing first:** the AI flies only the plans in the data or a file
given it; nothing makes a plan in the client, and the AI neither takes off
nor lands.

**The client** flies its aircraft through a controller (`sim::Controller`):
`--autopilot` hands it to the AI from the first step, holding what it is
doing; `--plan NAME` - a file, or a plan in `data/plans` by its name - has the
AI fly the plan from its start, its altitudes put above the ellipsoid as the
aircraft's are; and A hands the aircraft between the pilot and the AI at any
moment, the AI resuming what is left of the plan. The HUD adds a line while
the AI flies - `AP  HOLD`, or `AP  NAV` and the waypoint - and the client
prints each waypoint as it is passed, how close and at what altitude. In
shot mode a frame is two ticks, or as many as keep the flight to 300 frames,
so a fifteen-minute plan can be flown headless in seconds.

**The test:** on every driver, the client flies `--plan sydney-harbour` and is
shot at tick 115,000: it passes the Heads 0.4 m off, the bridge 23, Olympic
Park 0.3 and the airport 4.8, each at its altitude, as the unit test's flight
does - the same simulation - and the plan is flown. **Watched to fail:** a
plan loaded without handing the aircraft to the AI.

**Found on the way: the HUD's altitude was above the ellipsoid, not sea
level** - 72 ft high at Sydney, where the geoid is 22 m above the ellipsoid.
The simulation's altitude is above the ellipsoid, on which the DEM's ground is
set; the HUD and the trace now show it less the geoid there, and the trace
gives the height above the ellipsoid beside it. The HUD test holds the one to
the other less the geoid `glideslope_cli height` gives at the traced place, to
the thousandth of a foot; showing the ellipsoid's again is caught, 72 ft off.

### The user/AI controller swap, 2026-09-19 — item done (CI run 35372183417)

**What is missing first:** the swap across the network is Phase 7's; here
it is one client's.

**The controller** (`sim::Controller`) is who flies an aircraft: its pilot,
through the controls their hands and devices set, or the AI pilot - the
autopilot, with a navigator when it has a plan. Handed to the AI, the
autopilot engages from the controls the aircraft has. Handed back, the pilot's
hands are seldom where the AI had the controls, so the controls move from the
AI's towards the pilot's at a hand's pace - full travel in a second - until
they meet, and follow the pilot's directly from then.

**The test:** the test pilot flies the Cessna through six phases - the takeoff
roll, the climb, the cruise, a 30-degree turn, the descent and a flapped
approach - hands it to the AI for ten seconds and takes it back. To the AI, no
control moves more than 0.0022 of its travel in a step. Back, the controls
meet the pilot's in 0.09 to 0.69 s, moving 1/120 of their travel a step at
most, and are the pilot's exactly after; the load factor changes by 0.033 g in
a step at most over the three seconds after either hand-over. **Watched to
fail:** handing back by jumping to the pilot's controls - the controls moved
up to 1.1 of their travel in a step, and the load factor 0.36 g in the climb,
0.24 in the descent, 0.13 on the approach and 0.08 in the turn; on the ground
it cannot move, which is why the controls are checked too.

### Flight plans, and a crash at exit found by gdb, 2026-09-18 — item done (CI run 35363959900)

**What is missing first:** a plan cannot yet be given to the client - that is
`--autopilot`, the last item of Phase 4 - so plans fly only in the tests. The
navigator flies straight legs and passes a waypoint when it is abeam, turning
only then, so it overshoots each turn and comes back to the leg; it knows no
holds, procedure turns, speed or altitude constraints, or fly-by turns.

**A plan is data** (`assets/plans/*.plan`, read by `sim::parse_flight_plan`):
the aircraft it is for, where it starts in the air, and its waypoints with
their altitudes and airspeeds, one to a line; anything it cannot read is
refused by its line. **The navigator** (`sim::Navigator`) flies each leg along
its great circle, steering for the leg's track where the aircraft is abeam
of it, back towards the leg by 30 degrees a kilometre off it, and into the
wind by the drift it measures; it gives the autopilot the waypoint's altitude
and airspeed.

**The test:** the Cessna flies the Sydney Harbour plan - off Bondi, the Heads,
the Harbour Bridge, Olympic Park, the airport: 45 km, turns of 90 and 135
degrees, climbs and descents of 500 ft. In calm air it passes the waypoints
0.4, 23, 0.3 and 4.8 m off, each at its altitude to the foot; in a 20 kt
wind from the south, 0.4, 30, 0.4 and 4.7 m. **Watched to fail:** the drift
not corrected, 372 m off the bridge in the wind; the intercept turned away
from the leg; and no waypoint ever passed.

**Found on the way: the packaged client's crash was a race at exit.** It
segfaulted once in the ubuntu:24.04 container, and not when run again. Flown
under gdb since, it crashed again (package run 35347082677) and left every
thread's stack: one of the terrain's worker threads was in OpenSSL, inside a
download, while the main thread was in `exit()` tearing the libraries down
under it. Cesium Native's pending work holds its task processor - the
terrain's pool of worker threads - for as long as that work lives, so the pool
outlived the terrain, and its threads, never joined, ran on into the process's
exit. The terrain now stops and joins its workers when it is destroyed -
once they are idle and the main thread has run everything their work left
for it, which may give them more: stopped sooner, what they had finished lay
unrun in Cesium Native's queue, and the sanitized build caught 15 MB of
decoded imagery leaked that the running threads had been keeping in reach.
Even then, CI's Ubuntu run (35351306516) leaked the same 54 imagery tiles at
exit: work waiting on the cache's own SQLite thread, which the worker pool does
not see, was abandoned when the workers stopped, and held Cesium Native's
imagery cache in a cycle. Every request is now counted from its asking to its
answer, the count going down on a worker so that what follows is queued before
the pool can look idle, and the terrain waits for none to be in flight too.

### The autopilot's holds, 2026-09-18 — item done (CI run 35363959900)

**What is missing first:** it flies only what it is told. Nothing yet tells
it - no flight plan, no key in the client, no `--autopilot`; those are the
rest of Phase 4. Its gains are the Cessna's, found for it, and will not suit
a jet; its airspeed is held on the throttle alone, so in a climb the Cessna's
power, not the autopilot, sets how fast it can go up at a speed.

**The autopilot** (`sim::Autopilot`) is loops within loops on the aircraft's
state: heading to bank, 25 degrees at most, with an integral near the heading
that finds the bank the propeller's torque and slipstream need, to aileron,
damped by the roll rate; the ball to rudder; altitude to vertical speed, at
the climb rate asked for at most, to pitch - with an integral that winds only
while the pitch it asks for is the pitch it gets - to elevator, damped by the
pitch rate, with an integral that finds the trim; and airspeed to throttle.
It engages holding what the aircraft is doing, each integral set so that the
first controls it gives are the ones the aircraft had, and its bank, pitch
and throttle move at a pilot's pace from there.

**The tests:** at 4,000 ft and 100 KCAS, each hold's step, in calm air and in
moderate Dryden turbulence. Calm: heading 0 to 90 overshoots 2.2 degrees and
is within 2 for good at 40 s; altitude up 500 ft does not overshoot and is
within 20 ft at 68 s; airspeed up 10 kt overshoots 0.4 and is within 2 at 9 s;
climb 0 to 500 ft/min overshoots 0.2 and is within 50 at 8 s. In turbulence,
on ten-second averages: heading within 5 degrees at 29 s; altitude within 50
ft at 56 s; climb within 150 ft/min at 21 s; airspeed within 5 kt at 12.5 s,
then as much as 8.3 off, as the altitude hold trades speed for height through
the vertical gusts. Engaging it in a climbing turn moves no control more than
0.0025 of its travel in a step. **Watched to fail:** the trim not carried over
at engaging, the bank the wrong way, the throttle backwards, and no climb to a
new altitude - each caught.

**Found on the way:** without an integral the heading hold settled a degree
short of every heading; and a heading averaged in degrees from 0 to 360
wraps, so the test averages it from north.

### Weather you can see, 2026-09-18 — item done (CI run 35363959900)

**What is missing first:** the cloud is flat. Each deck is two sheets - its
base, grey, and its top, white - over a disc 60 km across the station,
curved with the Earth, cloudy where its pattern says; there are no cloud
volumes, towers or shading, and a cumulonimbus is a deck 6 km deep. The cloud
does not drift with the wind, and a new report makes the sky again rather
than blending into it. Rain and snow are streaks and flakes in a box 40 m
across about the eye.

**What is read** (`world::parse_metar`): the visibility, in metres or statute
miles - 9999, CAVOK, 10SM, 1 1/2SM, M1/4SM, P6SM; the weather present, with
intensity, vicinity, descriptor and phenomena; and the cloud - FEW, SCT, BKN
and OVC with their heights and CB or TCU, VV, and SKC, CLR, NSC and NCD. Held
to aviationweather.gov's own decoding of all eighteen recorded reports.

**What is drawn** (`world/sky`, `gfx::Sky`): a deck for each layer at the
report's height above the station, 300 m deep (2 km for towering cumulus, 6
for cumulonimbus), cloudy over the middle of its eighths - FEW 1.5, SCT 3.5,
BKN 6, OVC 8 - in a pattern of the weather's seed, the same on every machine;
inside cloud, a whiteout. The haze is Koschmieder's law in the shader: what is
seen through air of visibility V keeps exp(-ln 20 d / V) of its contrast with
the haze, V the report's in a layer from the ground to 1,000 m above the
station or the lowest cloud, 40 km above it, integrated along each fragment's
path through the layer's top. Rain falls at 7 m/s, drizzle at 3, snow at 1,
below the lowest cloud. The flight draws its report's sky; the terrain screen
takes `--metar REPORT` and `--station LAT,LON`; `glideslope_cli sky` says what
a report shows over a station, and where its cloud is thick and where it has
gaps.

**The tests:** the METARs' new groups, against aviationweather.gov; a report's
decks, visibility and precipitation; each deck's pattern covers its eighths
within 0.03 of the sky; and frames on every driver - under broken cloud at
1,500 ft over Hawera, cloud overhead 30 m below the base (100% of the frame)
and the ground below (54%), a whiteout 30 m above it (100%), clear sky in a
gap (100%): the base within 30 m of where the test, from the DEM's ground and
1,500 ft, puts it. Through 3,000 m of mist from Taranaki's slope the frame
matches the DEM ray-cast faded by Koschmieder's law to 0.20 of 255 on average,
ground beyond 3 km within 6 of the haze's colour and ground within 1 km 55
from it. Heavy rain changes 0.7% of a frame, every changed pixel lighter.
**Watched to fail:** a deck's base read in metres rather than feet, the
whiteout gone, the visibility doubled, the rain gone, and a deck's cover
inverted - each caught by the test meant for it.

**Found on the way:** from 30 m below a broken deck, looking up sees only 150
m of it, so whether the frame shows cloud depends on where the eye is under
the pattern. The frame tests take their places from `glideslope_cli sky`,
which finds them from the same pattern; the base's height they compute for
themselves. And the whiteout was first shot looking straight up, where the
deck's top sheet alone fills the frame as evenly as a whiteout: with the
whiteout taken out, that still passed. It is shot looking level now, where
between the sheets the base shows below and the top above; the rain was
first broken so it would not compile, which tested nothing, and was broken
again so it did. Sydney's calm weather this afternoon left the HUD test's
Cessna zooming at 32 degrees of pitch at its shot, its horizon line drawn
across the credits, which then could not be read back: the credits are now
drawn last, over a strip that darkens what is behind them by half, so they
can be read over anything - the Copernicus notice must be.

### Thermals, ridge lift and mountain waves, 2026-09-18 — item done (CI run 35336574855)

**What is missing first:** the thermals do not know the ground. They rise as
strongly over the sea, a lake or a north slope as over a sunlit field, from
the station's report alone, and nothing marks them - no cumulus, since nothing
draws cloud yet. The terrain's lift sees the ground only along the wind
through the aircraft, 32 km each way: a ridge parallel to the wind lifts
nothing, and the air across the wind is not in it. It is linear theory, so it
has no rotor, no hydraulic jump and no flow blocked by a mountain too high for
the wind to carry over it (N h / U well above 1), and with one wind and one
stability for the whole column it makes waves that rise away rather than lee
waves trapped under a stable layer.

**Thermals** (`world::thermal_updraught`) are Allen's updraft model
(NASA/TM-2006-213477, 2006), transcribed from the MATLAB in his appendix B:
his mean updraft and radius with height, his bell fitted to Konovalov's
measured updrafts, the ring of sinking air round each in the layer's upper
half, and the sink between them that his mass balance sets. Where he places
updrafts at random, one stands in each cell of a grid - his count per area,
taken at 0.4 of the layer's depth, sets its size, 470 m for a layer 1.4 km
deep - drawn from the weather's seed, 0.7 to 1.3 times his strength, living
twenty minutes and growing and fading over three at each end; the pattern
drifts with the wind halfway up the layer. The layer comes from the report: a
parcel 1 C warmer than the METAR's air, rising dry-adiabatically through the
forecast's temperatures until it is no warmer, up to 4 km; its convective
velocity 2 m/s for 1,500 m, with the cube root of the depth; none under 300 m
deep or in a surface wind over 25 kt, as in Allen's.

**Ridge lift and mountain waves** (`world::terrain_updraught`) are linear
theory: the terrain along the wind, 256 samples 250 m apart from the DEM,
transformed; each wavenumber a wave rising with height where it is longer than
2 pi U / N, and dying away where it is shorter; the vertical wind U times the
displaced air's slope. At the ground that is the wind up the slope - lift on
the windward side, sink in the lee - and above, waves leaning into the wind.
The wind is the report's 1,000 m above the station, the stability its lapse
between 1 and 4 km. The client gives the weather its DEM; the CLI's `air`
command compares a synthetic ridge's lift and a hot afternoon's thermals
across the platforms with the gusts and turbulence.

**The tests:** Allen's updrafts are the size his worked example gives and the
speeds his figure 10 shows, within 0.05 m/s; in its prime, each thermal of the
pattern is his model to 1e-12 m/s, and beyond its reach the air sinks at his
rate; over the pattern the air sinks as much as it rises within 15% (it is 3
to 8%); each thermal grows and fades smoothly, one after another; the
convective layer is the depth a parcel rises to, to 1e-6 m. The terrain's
lift is within 2.4% and 1.0% of the strongest wind over two ridges in stable
air - linear theory integrated in the test by quadrature, with nothing in
common with the code's transform - and within 0.1% of potential flow with no
stability. A Cessna gliding round a thermal at 65 KCAS in a 45-degree bank
climbs as it does in still air at each height, sinking faster higher up where
the air is thinner, and faster by the model's updraught along its path: 0.006
m/s off 2.5 m/s of lift, where the test allows 2%. Without a forecast's
temperatures aloft there are no thermals: the standard atmosphere's lapse
alone made a 303 m layer, and thermals at night. **Watched to fail:** the waves
leaning downwind, the transform unpadded, the thermals sinking, and no sink
between thermals - each caught by the test meant for it.

**Found on the way:** Queney's hydrostatic formula, the first reference, was
18% off the code over a ridge 5 km wide; the code is right. Its waves are
non-hydrostatic, and a ridge's slopes hold wavelengths short enough for that
to matter several kilometres up; linear theory integrated exactly agrees
within 2.4%. And the transform pads the terrain with as much level ground
again: a transform takes what it is given to repeat, and with the copies 64 km
away rather than 128, the narrow ridge's waves were 4.2% off rather than 1.0%.

### Microbursts, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** nothing places a microburst but whoever sets the
weather - a test, or `glideslope --weather STATION --microburst LAT,LON` - so
a thunderstorm in a METAR makes none. The model's column is steady: a real
burst's ring vortex, its tilt and its moving with the storm are not in it. It
fades above 1 km, where the model's column would fall forever, and there it is
no longer mass-conserving.

**The model** (`world::microburst_wind`) is Oseguera and Bowles' downburst
(NASA TM-100632, 1988): a column falling within about a radius of its centre,
spreading along the ground, fastest some 70 m up about 1.1 radii out; with this
project's constants - an outflow depth of 200 m and a ground layer of 30 m - a
10 m/s downdraught of 1 km radius spreads at up to 11 m/s each way, a 44 kt
change from headwind to tailwind across it. It is a function of place and time,
growing over two minutes and fading over two, so it is the same air on every
machine; a `WeatherReport` carries any number, and the client's refreshed
reports keep them.

**The test:** at a thousand places the field is the model's formulas, written
out again in the test; its divergence is nothing to 1e-7 per second at 500
places - what falls spreads; it is nothing before it starts and after it ends,
and half grown at a minute; and down a 3-degree approach to Sydney through a
burst 2.5 km before the runway, the wind at every 10 m is the model's to 1e-9,
the headwind strongest before the centre, the downdraught at it, the tailwind
after. **Watched to fail:** the downdraught turned to an updraught.

### Reported wind shear, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** a report of shear is flown as a model, not as what
was measured: a METAR says only that there is shear on a runway, not how much
or where. The runway's direction is its number, magnetic taken as true - off by
the local variation, 8 degrees at Narita. Peak winds and wind shifts are read
and not flown: a peak wind 50 minutes old is not the air now.

**Read** (`world/metar.hpp`): wind shear on a runway or all of them in every
form a METAR writes it - `WS R02`, `WS RWY27`, `WS TKOF RWY20`, `WS LDG RWY09L`,
`WS ALL RWY` - and from the remarks the peak wind (`PK WND 22033/0832`, its hour
the report's when only minutes are given) and a wind shift (`WSHFT 0743`, with
`FROPA` when a front brought it).

**Flown** (`world::with_air_motion`): within 8 km of the station, a 15 kt
headwind on the named runway's approach - along the surface wind for all
runways - between 60 and 600 m above the ground, all of it from 300 to 450 m
and none by 60 m: the airspeed an aircraft loses on short final flying down
out of it.

**The test:** five recorded reports - Lisbon's `WS R02`, Narita's `WS R34R`
before its `TEMPO`, Jackson Hole's `WSHFT`, the peak winds at Clines Corners and
Albuquerque - decode to what they say, and each other form as written; and on
runway 02's approach at Lisbon the headwind is the surface wind's plus the
model's to 1e-9 at seven points, from 8.5 km out to 1.1 km, 60 to 700 m up.
**Watched to fail:** the shear blowing as a tailwind; `WS` not read.

### The wind near the ground as a boundary layer, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** where the METAR and the forecast disagree, the
shear between 10 and 80 m is their disagreement: at Sydney on 18 September the
observation was 6 kt from the south and the forecast's 80 m wind 8 kt from the
north-north-east, so the wind swings round in 70 m. The ground under the
profile is the station's, flat: the terrain's own height and roughness do not
shape it, and 3 cm - short grass - is the roughness everywhere.

**The profile** (`world::conditions_at`): the METAR's wind 10 m above the
station, where it is measured; below, falling logarithmically to nothing at a
roughness length of 3 cm - 72% of it 2 m up, about where a light aircraft sits
on its wheels; above, through Open-Meteo's winds 80, 120 and 180 m above the
ground, logarithmically in height between each; then linearly to its lowest
pressure level, and its levels above, as before. The forecast is now asked for
those winds near the ground (`world::open_meteo_url`), and a response without
them gives the old profile. `glideslope_cli weather` prints them.

**The test:** a recorded Open-Meteo response with the winds near the ground,
read as the response gives them; with Sydney's METAR, the wind at 80, 120 and
180 m is the forecast's, at 10 m the METAR's, at 30 m logarithmically between,
at 2 m the METAR's times ln(2/0.03)/ln(10/0.03), and still below 3 cm; and a
3-degree approach from 300 m to 15 m, flown by the test pilot, is given the
profile's wind at its height before every step, to 0.01 ft/s. **Watched to
fail:** linear rather than logarithmic between the heights (at 30 m); the
forecast's winds near the ground ignored (at 80 m).

### Gusts and turbulence: the same air on every machine, 2026-09-18 — items done (CI run 35331164089)

**What is missing first:** gusts and turbulence are the wind's: nothing yet
makes wind shear near the ground (the boundary layer is the next item), a
microburst, a thermal or a wave. The turbulence is translation only: Dryden's
model also turns an aircraft - rates in roll, pitch and yaw from the air's
gradients - and that is not given to JSBSim yet. Its spectrum's shape along a
line is not exactly Dryden's (below). And the weather's patterns are drawn on
a flat Earth about the reporting station, so they are the weather of a few
hundred kilometres around it.

**The air is a pattern the wind carries** (`world/air_motion.hpp`): gusts and
turbulence are functions of place, time and a seed the report carries, with no
state - so every machine with the report flies the same air, and an aircraft
restored to a state meets the air it left. The seed is made from the station
and the observation's time (`world::air_seed_of`).

**Gusts:** the wind along its direction rises from a METAR's mean speed
towards its gust and falls back, as smooth noise over 4 to 16 seconds, in full
up to 10 m above the station and fading to none 600 m above that. **Turbulence**
is MIL-F-8785C's Dryden model - its lengths and intensities by height, with a
table of intensities by severity above 2,000 ft - made as a sum of cosine
waves; a report's severity is judged from its gusts' spread over its mean wind
(5 kt light, 15 moderate, 30 severe), unless it gives one. For reported weather
JSBSim's own turbulence is now off; `sim::Weather` can still ask for it, and
the Phase 3 test of it stands.

**The tests.**
- The same air: two weathers from each of three recorded gusty reports -
  Edinburgh, Albuquerque in a thunderstorm, Mount Washington - give exactly the
  same wind at a thousand places and times, asked in either order and asked
  again; another seed is other air.
- An aircraft restored in a gust: its first step, from the same place at the
  same time, meets the original's wind within a millionth of a foot a second,
  and every difference after that is the weather's between where the two are,
  to 1e-9 - restoring is not exact, and Dryden's air differs by a foot a second
  over a metre.
- Gusts flown: ten minutes at each station's anemometer height stay between the
  mean and the gust and reach both within half a knot; 700 m up, none; a report
  without gusts gives a steady wind; a minute's flight through Albuquerque's
  gusts, twice, ends in the same place.
- Dryden: MIL-F-8785C's lengths and intensities at 100 and 5,000 ft; each
  component's RMS within 12% of its intensity along 200 km at four heights;
  and, over four seeds, three directions and two heights, the distance at which
  its correlation falls to 1/e within 15% of Dryden's length.
- Across platforms: `glideslope_cli air` prints the air at a thousand places and
  times in whole 1e-11 m/s, and CI's cross-platform job holds every platform
  within 1e-9 m/s of the first (`tests/cmake/cross_platform_flights.cmake`,
  whose test now also refuses one sample 1e-8 m/s off).

**Watched to fail:** hidden state - the air nudged by how many times it had
been asked (three tests); gusts overshooting the gust by 30%; the severity
table read a row off; the waves left full length (correlated to 2.53 of
Dryden's length). The rescaling of the waves to Dryden's intensity was also
removed, and nothing failed: the waves already make nearly Dryden's variance,
and the rescaling corrects only what the 2 m to 50 km band leaves out.

**Why the waves are shortened.** Waves in every direction cross a line at an
angle, so along it they are longer than they are: the first version's
turbulence stayed correlated 2.5 times Dryden's length along the wind and 1.4
times across and vertically. Each wave is shortened by those measured factors,
2.47 and 1.40, which brings the average back to Dryden's length; any one line
still varies from 0.8 to 3 times, as 48 waves sample a spectrum.

**Also:** the plan's first verification for this asked for bit-identical air on
every platform, which floating point across compilers cannot promise, and was
amended to 1e-9 m/s before this was built.

### Open imagery on the terrain, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** the imagery is 2016's - EOX's later mosaics are not
open - and 10 m a pixel at its finest, so a runway is a grey stripe with no
markings. It is fetched from EOX's service as the view needs it, so the first
look at a place waits for it, and without the network there is none. How
sharp the imagery is has no check: the reference frame the test holds it to
chooses its own imagery level, and a level set too coarse matched it better.

**The source is settled** (`REQUIREMENTS.md`, section 9): EOX's Sentinel-2
cloudless mosaic of 2016 - the whole Earth, cloud-free, 10 m a pixel, under CC
BY 4.0, served in latitude and longitude as a Web Map Tile Service (terms and
credit in `ASSETS.md`).

**Drawn by Cesium Native as a raster overlay.** The terrain tileset carries a
Web Map Tile Service overlay (`gfx::open_imagery()`); Cesium Native works out
each terrain tile's texture coordinates, chooses and fetches the imagery tiles,
and the glue decodes each into a texture on the GPU - with mipmaps - and puts
it on the terrain tiles it covers, with the scale and offset that take a
tile's coordinates into its part of the image. The renderer's meshes now carry
texture coordinates and its shader samples a texture, white for a mesh with
none. With imagery the terrain's vertices carry only the sun's light on their
slope, and the imagery is drawn times it; `--imagery off` tints by height as
before. Everything fetched goes through Cesium Native's SQLite cache in the
cache directory, kept as long as the service's caching allows - a week.

**Credited on screen.** The imagery's credit, EOX's text, is drawn along the
bottom of every frame with imagery, after the DEM's notice; the font gained
`:` and `/` for its address. `README.md` carries it too.

**The tests.**
- `a_shot_of_mount_taranaki_drapes_the_open_imagery_where_it_belongs_on_<driver>`
  shoots the same view as the terrain test at 640x480 with imagery, and reads
  both credits back out of the frame. Its reference frame is the DEM ray-cast
  from the same eye, each hit coloured by the slope's light times the imagery
  there, fetched from the service at the level whose pixel is the ground the
  frame's pixel covers - independent of Cesium Native, the textures and the GPU.
  As drawn: the colour 1.86 of 255 out on average and 7 at the 95th percentile;
  and, blurring both by 4 pixels and moving the frame up to 3 pixels each way,
  the best match is where it was drawn. The tolerances are 3.0 and 12, and the
  best match within a pixel.
- The DEM test now shoots with `--imagery off`; the HUD test flies over imagery
  and reads three credits.

**Watched to fail:** the imagery upside down (the colour 4.90 out, 20 at the
95th percentile); drawn without the slope's light (5.42 out, and the best match
two pixels down). **Not caught**, and stated: the imagery moved east by 2% of an
imagery tile - some 40 m, about a pixel here - and a coarser imagery level.

**Also:** attaching an imagery tile, the resources Cesium Native hands over
are the imagery tile's, not the terrain tile's; taking them for the terrain
tile's overran them, which the sanitized build caught on the first run.
Windows CI's first build of the dependencies failed in Draco: vcpkg builds
deep under its root, and under the user's AppData the paths passed the 260
characters MSVC can open. vcpkg now lives in `C:\gs-vcpkg\<12 characters of
its commit>` on Windows.

### The plan grows: weather hazards, sixteen aircraft, learning to fly, the whole Earth, 2026-09-18

At the project owner's asking. **Learning to fly** is Phase 5c: checklists for
every aircraft as part of its data, ticking themselves from the aircraft's
state, and lessons the AI pilot demonstrates and debriefs (`REQUIREMENTS.md`
section 4.3) - a debrief, never a score, which `FEATURES.md` rules out. **`FEATURES.md`** gains wind that shears and
gusts, hazardous air, weather you can see, and the same air for everyone, and
its choice of aircraft now names them. **`COMPLETION_PLAN.md`** gains Phase 3b
(seven items: the same air on every machine, gusts flown, the boundary layer,
reported shear, microbursts, thermals and mountain waves, visible weather),
and Phase 5 the roster of `REQUIREMENTS.md` section 4.2 - the J-3 Cub, PA-28,
Cessna 182, the Short S.23 flying boat, the Mosquito (first after the Cessna),
the Learjet 35A, the A320, A380, 737, 747 and 787, and the F-15, F-22, F-35A
and B-2 - with water where the DEM says it is and a HUD for fast aircraft. A
tail, found doing it: terrain over the whole Earth, which "anywhere on Earth"
needs and nothing in the plan provided, and which a jet makes pressing.

**Why Phase 3b's weather is computed here:** JSBSim's own gust and turbulence
models keep hidden state that a restored aircraft does not carry, and the
server's weather is authoritative, so every client's prediction must fly the
same air as the server. Each perturbation is to be a function of position, time
and shared parameters alone.

**What JSBSim does not ship:** flight models for the A380, Learjet, Mosquito,
F-35A and B-2, which are written here. Much of the F-35A's and B-2's performance
is not public; they will be held to what is, and no more claimed.

### Terrain, drawn by Cesium Native from the DEM, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** the terrain is a region, not the world - the nine
whole-degree cells around where the flight starts, or the one cell the terrain
screen's eye is in - so a flight that leaves it flies over nothing. There is no
imagery: the ground is tinted by height and lit by a fixed sun. Tiles are made
from the DEM one at a time, behind one lock, so a region loads slowly the first
time, fetching each 30 m tile it needs (25 MB each). The HUD's horizon line does
not line up with the drawn horizon (see Gaps). Nothing from Cesium ion or
Google yet, and no measured visual-to-collision mismatch: that is Phase 5b.

**Cesium Native is built and linked** (`ext/cesium-native`, v0.64.0), with its
thirty dependencies from vcpkg, pinned to the commit Cesium Native's release is
built against and fetched outside the tree (`cmake/Vcpkg.cmake`,
`cmake/triplets/`, `vcpkg.json`; `ext/README.md` says how and why). They are
installed after the platform gate accepts the compiler. The first configure of
a machine builds them - 20 minutes here - and vcpkg's binary cache keeps them;
CI keeps that cache. Every package carries their licences.

**The open-data terrain is a Cesium Native tileset** whose loader builds its
tiles from the DEM (`gfx/terrain_tiles.hpp`): a quadtree over the region, each
tile a 32-by-32 grid of cells with skirts (`world/terrain_mesh.hpp`), down to
the DEM's own spacing - level 7 over one degree - and each tile's geometric
error half its cell size. Cesium Native chooses the tiles a view needs, loads
them on worker threads and caches them; the glue turns each tile's glTF into
the renderer's meshes, uploads them on the main thread and frees them when
Cesium Native drops the tile. The renderer can now remove a mesh. A `--shot`
waits for every tile its view needs, so the same command draws the same frame.
Requests Cesium Native makes go through the platform's HTTPS - plain GETs only,
for now; the DEM loader makes none.

**Shown where it is drawn.** The client draws the Copernicus DEM's notice along
the bottom of every frame with terrain in it, in the small type credits now use,
and `glideslope_cli height` prints it; `README.md`, which ships, carries the
liability sentence the licence asks for (`ASSETS.md`).

**The terrain screen**, `--screen terrain --at LAT,LON,HEIGHT --toward
LAT,LON,HEIGHT`, draws the terrain alone from a point toward another. The
flight draws the terrain under the HUD.

**The tests.**
- A terrain mesh over ground with a known slope puts every vertex on it within
  a centimetre, each normal perpendicular to it within half a degree, every
  triangle facing up, the skirt 50 m straight down; it meets the tile beside it,
  and a tile of the next level, within 2 mm at every shared vertex; and a
  rectangle off the Earth, or with no cells, is refused.
- **Mount Taranaki, against the DEM itself**
  (`a_shot_of_mount_taranaki_matches_the_dem_ray_cast_from_the_same_eye_on_<driver>`):
  the terrain screen shoots the mountain from 9 km east of its summit, and
  `glideslope_terrain_check` makes the reference frame without Cesium Native,
  tiles, meshes or GPU - a ray through every pixel, marched through the DEM
  until it meets the ground, coloured as the terrain is coloured - and holds
  the two together above the notice, which it reads back first. As drawn: the
  skyline 0.12 pixels out on average and 1 at most; 99.94% of pixels ground or
  sky alike; the colour 4.76 of 255 out on average, 23 at the 95th percentile.
  The tolerances are 0.25 and 2 pixels, 99.5%, 5.5 and 28.
- The HUD test now flies over the drawn terrain and reads both credits, the
  DEM's and Open-Meteo's, back out of its frame.

**Watched to fail**, each against the tolerances: only coarse tiles drawn
(screen-space error 200: the skyline 0.48 pixels out, the colour 7.80); tiles a
level or so too coarse (error 16: 0.30, 6.95); the terrain 20 m too high (0.34,
6.13); 100 m too high (2.10 pixels, 98.90% agreeing, 8.08); a vector leaked in
the client (the leak judged glideslope's). The first tolerances, set before any
of this, passed all four breakages, and were tightened on these numbers.

**Also:** LeakSanitizer gives Mesa's lavapipe driver, unloaded before it
reports, as the client itself, at offsets past the end of its image and with
no function named; a frame given as the client's with no function named is now
judged not to be the client's (`tests/cmake/client.cmake`). Under WSL, CMake's
package search walked Windows' PATH through `/mnt/c` for minutes; Cesium
Native's packages are now found through vcpkg's prefix alone. A sanitized build
at ninja's default of every core ran WSL out of memory; builds here are capped
(`CLAUDE.md`).

### Weather: METARs, winds aloft, JSBSim's atmosphere and turbulence, 2026-09-18 — items done

Proved in CI on every platform (run 35250647710, after the retries below), and
every package fetched the weather and flew its frame in it (run 35248205205).

**What is missing first:** the weather is one airfield's. A flight is flown in
the weather reported at the station it names - its METAR at the surface and
Open-Meteo's winds aloft above it - wherever it flies; nothing chooses the
nearest station or blends between stations. There is no cloud, visibility,
precipitation or icing, and nothing is drawn: JSBSim is given wind,
temperature, pressure and turbulence, and that is all. Turbulence is off unless
something asks for it - no report gives it, and nothing yet sets it outside the
tests. Gusts in a METAR are read, and not flown. The server, which will own the
weather, does not exist.

**METARs** (`world/metar.hpp`, `world/weather.hpp`). aviationweather.gov's JSON
response is read for each report's raw text and its station's position, and
the raw text is decoded here: station, time, wind in knots, metres a second or
kilometres an hour, with gusts and variability; temperature and dew point,
with the tenths of a US `T` remark; QNH from `Q` or `A` groups. Trend groups -
`BECMG`, `TEMPO` - are skipped, so a forecast wind is never read as the
observation. A station is checked to be four letters or digits before it goes
into a URL, and a station with no report is said to have none.

**Winds aloft** (`world/winds_aloft.hpp`). Open-Meteo's forecast for the current
UTC hour on 19 pressure levels, 1000 to 30 hPa: each level's wind as north and
east components, its temperature, and its geopotential height converted to
geometric height. Between levels the wind is interpolated as a vector.

**The weather at a height** (`world::conditions_at`): the METAR's wind up to
10 m above the station, where it is measured; the winds aloft from the lowest
level above that; linear in height between. The temperature follows the same
profile as an offset from the International Standard Atmosphere, so the air at
the station is the METAR's temperature. The pressure is the METAR's QNH.

**JSBSim's atmosphere** (`sim/weather.hpp`). The simulation still knows nothing
of reports: `Aircraft::set_weather` takes any `sim::Weather`, and before every
step the aircraft asks it for the conditions where it is and sets JSBSim's wind,
temperature offset, sea-level pressure and MIL-F-8785C turbulence severity. The
sea-level values rebuild JSBSim's atmosphere, so they are set only when they
change. Turbulence is seeded the same every time, so a flight in it repeats.

**Changing weather** (`world::ReportedWeather`): a new report blends in over an
interval, every value moving linearly from the old report's to the new one's,
and turbulence changing halfway. The client fetches the weather again every
fifteen minutes of flight, on another thread, and blends it in over five; a
fetch that fails is reported and the weather kept.

**Where it shows.** `glideslope_cli weather STATION` prints the METAR and the
winds aloft over it. `glideslope --weather STATION` flies the flight in it, with
"WEATHER DATA BY OPEN-METEO.COM" along the bottom of the HUD, as Open-Meteo's
CC BY 4.0 terms ask, and `--trace` now includes JSBSim's wind. JSON is read by
a strict RFC 8259 parser written here (`world/json.hpp`) rather than a library.
Sources, terms and the credit are in `ASSETS.md`.

**The tests.**
- JSON: every kind of value, escapes and surrogate pairs, numbers at their
  limits, and duplicate keys read; trailing commas, comments, leading zeros,
  control characters, lone surrogates, bad escapes, truncation and nesting past
  128 refused.
- Ten recorded METARs, from Toronto, Heathrow, Boston, Denver, Christchurch,
  Utqiagvik, Punta Arenas, Moscow, Sydney and Beijing, decode as
  aviationweather.gov's own decoding beside each says - station, temperature,
  dew point, wind, direction and altimeter setting. Every group's forms, and a
  trend's wind and temperature not taken for the observation's.
- Each recorded METAR, flown at its station, gives JSBSim the wind it reports
  to 0.01 ft/s, the temperature at the station to 0.05 C, and its QNH to
  0.01 hPa.
- A recorded Open-Meteo response for Sydney: all 19 levels read as the JSON
  says; JSBSim's wind at each level's height is that level's, and halfway up to
  it from the level below - or from the surface - halfway between.
- A minute holding a northerly heading at 3,000 ft in 20 knots from the west
  drifts the aircraft east of the still-air flight by what 20 knots covers in a
  minute, 617.3 m, within 2%, with no turbulence at all. With moderate turbulence (severity 3), each component's RMS
  is within 0.4 to 1.6 of JSBSim's MIL-F-8785C intensity at that height, 7.21
  ft/s; no gust exceeds five times it; the aircraft stays within 30 degrees of
  bank; and the same turbulence flown twice ends in the same place.
- A report of 30 knots from the east, replacing 10 knots from the west and
  blended over 60 seconds: the old wind before it, halfway at halfway with the
  pressure halfway, the new wind after, and no step between ticks larger than
  the whole change spread evenly over the interval.
- Live: Sydney's weather now, from both services - where the station is, a
  report from today or yesterday, 19 levels in order with 500 hPa between 5,000
  and 6,200 m; stations that are not stations refused before any fetch; a
  station with no METAR said to have none. Skipped without the network, except
  in CI.
- The HUD test now flies in Sydney's weather now, and reads the credit back
  out of the frame as well as the numbers.

**Watched to fail:** turbulence type left off (every component's RMS 0.000
ft/s against 7.21); the credit removed from the HUD ("the credit reads "", not
"WEATHER DATA BY OPEN-METEO.COM""); the station check reduced to refusing
nothing but an empty name (a three-letter station reached a URL).

**Tried again.** CI's first run of this failed on Ubuntu because
aviationweather.gov answered the HUD test with 504 Gateway Timeout; the
packages, minutes later, fetched it everywhere. Weather and DEM downloads are
now tried three times, two and then four seconds apart, when a server fails
(5xx) or nothing answers, and never after any other status
(`world::fetch_with_retries`; watched to fail with every status taken as a
failure).

### The flight screen: HUD, test flags, flight controllers and frames, 2026-09-18 — items done

Proved in CI on every platform (run 35244380011) and by every package (run
35244379942), each uploading its frame.

**What is missing first:** there is still nothing to see but sky: no terrain,
no aircraft, no cockpit. The HUD is text and a horizon line. There is no
gamepad binding, no way to rebind a device but editing the file, and no
settings screen - `--screen` chooses between the flight and three test scenes.

**The flight screen** (`src/frontend/client/flight.hpp`), the client's default:
the Cessna standing on the DEM - tiles and geoid downloaded into the cache -
started over Sydney at 1,000 m, 100 kt, stepped at 120 Hz, seen from the
cockpit through a camera built from its position and attitude.

**The HUD** (`gfx/hud.hpp`): airspeed, altitude, heading, vertical speed,
pitch and bank as text, and a horizon line that pitches and banks. Text is a
5-by-7 pixel font drawn here, each font pixel a square of whole screen pixels,
over everything through a second pipeline with no depth test - so a frame can be
read back exactly (`gfx::read_text`).

**The test flags.** `--screen NAME`; `--shot FILE` and `--shot-at TICK`, which
now counts simulation ticks, with every frame advancing exactly two while
shooting whatever the clock says; `--trace`, a line of the flight's state after
every tick. Each is used by a ctest: the frame tests choose their scenes with
`--screen` and shoot at ticks, and `the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_<driver>`
flies 600 ticks with `--trace`, shoots the last, and has
`glideslope_hud_check` read every HUD line back out of the BMP and hold each
number to the traced state, within half its last digit, having checked the
trace holds ticks 1 to 600 in order.

**Flight controllers** (`platform/input.hpp`, library `glideslope_input`, apart
from `glideslope_platform` so that the CLI still links no SDL). `Joysticks`
reads every device SDL sees; `ControlMapper` turns them into controls by the
bindings in `assets/input/bindings.txt` - per kind of device SDL reports,
flight sticks and yokes or HOTAS throttles, each axis centred or a lever, each
button held or stepping, each hat direction stepping. An axis sets its control
when it moves, so a lever left alone does not undo a button. Pitch trim is a
new control. The keyboard still works beside them.

**Tested through SDL's virtual devices.** A virtual yoke and a virtual HOTAS
throttle, eight axes, sixteen buttons and a hat each: every one of the 56 inputs
alone moves a control, and none is unbound; a particular set of inputs sets the
controls they name to the values they should - stick forward is nose down, a
lever three quarters forward is 0.75, two presses are two notches of flap, the
brake held and let go - and those controls reach JSBSim's commands with the
signs JSBSim uses; a bindings file that cannot be read is refused by line.

**Frames from everywhere.** Packages now carry the client and SDL's licence.
Every package job runs the unpacked client headless on its platform's driver -
Vulkan through Mesa's lavapipe, installed in the stock Linux containers; Metal;
Direct3D 12 - flying 600 ticks of the flight, and uploads the frame. CI uploads
every frame its tests wrote.

**Watched to fail:** the HUD's airspeed shown one knot high ("SPD shows 79 at
tick 600 when the state is 77.896"); a binding removed from the file (the yoke
had an unbound input). The first run of the controller tests found that an axis
rewrote its control every read and so undid button steps to the same control -
flaps stayed at 0 - which is why axes now act only when they move.

### Standing on the DEM, 2026-09-18 — item done

**What is missing first:** only the tests connect the simulation to the DEM;
no program flies over it yet, and the selftest and the published-figure checks
still stand on JSBSim's own level ground. Nothing has flown into terrain: this
is ground contact for wheels, and whatever JSBSim does with the rest of the
airframe.

**`sim::Terrain`** (`sim/terrain.hpp`) is the ground the simulation stands on:
heights above the WGS84 ellipsoid at a latitude and longitude, from anything
- the frontends will give it the DEM. `Aircraft::set_terrain` puts a JSBSim
ground callback in place of JSBSim's level ground: the contact point straight
below, at the terrain's height, and the surface normal from the slope by
central differences 15 m to each side. `AircraftState` now reports height
above the ground and the terrain's elevation.

**A real bug, found by it.** `InitialConditions` latitude was given to JSBSim's
geocentric setter, while every latitude here - and the state JSBSim reports -
is geodetic. At 45 degrees that started an aircraft 0.19 degrees, 21 km, north
of where it was asked; at Sydney, where the figures and the selftest start,
about 20 km. It is now geodetic. The nine figures stay in range; the selftest
ends somewhere slightly different, and its hash is different.

**The tests.**
- Level terrain through the callback, at sea level and at 1,656 m, holds the
  Cessna exactly as JSBSim's own ground does: the same height, attitude and
  strut compression to within a millionth.
- On a 10% slope, facing up it and down it, the Cessna rests on all three
  wheels, its centre of gravity 4.39 ft above the slope as 4.36 ft above level
  ground, pitched by the slope's angle plus what its struts add - worked out
  from their measured compressions over the wheelbase - to within 0.1 degree.
- Engine stopped and brakes off, it rolls down that slope at over 5 kt in ten
  seconds, and stays still on level ground.
- On the real DEM, through the downloaded tiles and the geoid: at Boston's
  runway 33L (4.8 m), Denver's 34L (1,624 m), and halfway up Courchevel's
  runway, the Cessna comes to rest on all three wheels; the ground JSBSim
  stands it on is the DEM's height where it stopped, to 0.01 ft; its centre of
  gravity is 4 to 5 ft above it; and at Courchevel it is tilted by the DEM's
  own slope under its wheels, within 0.5 degree.

**Setting down.** JSBSim's initial altitude places the centre of gravity:
set down at the ground's height, the wheels start four feet under it and the
struts throw the aircraft up. On level ground it settles; facing down a slope
it went end over end for half a minute. The tests set it down 4.4 ft up.

**Watched to fail:** the normal set straight up (the rolling test: 0.000 kt
down the slope - the slope and rest tests passed, which is why the rolling test
exists).

**Also:** CI said MSVC refuses `getenv` in the tests (now one helper in the
harness), and CMake on macOS needs Objective-C++ enabled at the top of the
project before any target; CI's Windows runner now finds lavapipe
(`vulkaninfo`: llvmpipe, Vulkan 1.4.354) through the registry.

### The DEM, fetched as needed and held to the survey, 2026-09-18 — item done

**What is missing first:** summits. At five NGS summit stations the DEM is 8 to
35 m below the surveyed height, and even its highest sample within 90 m of each
is 5 to 21 m below: a 30 m grid of a radar surface does not hold a peak. The
plan's verification asked for summits within the stated accuracy; that is not
something a 30 m DEM can meet, and the verification is amended to say so
(`COMPLETION_PLAN.md`), with the shortfall pinned by a test. Also missing:
nothing in the simulation uses a height yet (the next item), and the DEM is not
thread-safe.

**Fetching** (`world/download.hpp`). `DownloadedTiles` keeps tiles in the cache
directory, fetching a missing one from the public bucket, checking it against
the MD5 the bucket gives as its ETag, and writing it into place only whole; a
tile that arrives damaged, missing, untagged or not at all is refused with the
reason and not kept. `fetch_pinned` does the same for a file pinned by SHA-256,
and `egm2008_geoid` uses it for the geoid grid. `world/digest.hpp` is SHA-256
and MD5, tested against FIPS 180-2's and RFC 1321's vectors and around every
padding boundary, whole and in pieces. `platform::cache_directory()` is
`GLIDESLOPE_CACHE`, or the user's cache directory on each system.

**HTTPS** (`platform/http.hpp`), through what each system trusts: WinHTTP,
NSURLSession, and on Linux the system's libcurl, loaded at run time, so nothing
is needed to build or to start and a missing libcurl is named when a download
is wanted. Recorded in `REQUIREMENTS.md`. Tested against the network: a pinned
1.1 MB file byte for byte, with its ETag equal to its MD5; GitHub's redirect to
the licence file at the commit that added it; a 404 as a response rather than an
error; a body over its limit, and a host that cannot exist, as errors.

**`glideslope_cli height LAT LON`** gives the height above sea level, the geoid,
and the height above the ellipsoid, fetching what it needs. Every package now
runs it at Sydney airport from an empty cache and must get 6.234 m: on stock
Ubuntu, which has no libcurl, it must first fail saying so, and then succeed
once libcurl is installed.

**Held to the survey** (`tests/data/dem/surveyed.txt`, sources in `ASSETS.md`).
Twelve runway ends from the FAA's data - Denver, Las Vegas, Boston, Juneau,
Anchorage, Utqiagvik, from 36 to 71 N, and so four bands of tile widths - are
within the Copernicus DEM's stated 4 m (the product handbook's "< 4m (90% linear
error)"), every one of them, worst 3.53 m, most within 2 m, and all below the
survey; five coastal waters are at 0.00 m. Twelve tiles are fetched for it, and
CI keeps them.

**Watched to fail:** the ETag check removed (the damaged tile was accepted);
heights above the ellipsoid compared with the survey instead of heights above
the geoid (Denver 20.7 m out); a wrong SHA-256 round constant, and a padding
boundary moved (both digest tests).

**Also:** AppleClang's `-Wdouble-promotion` refused a float returned as a
double in `dem.cpp` that GCC let pass; the Vulkan loader ignores
`VK_DRIVER_FILES` in an elevated process, which is what CI's Windows runner
runs, so CI now registers lavapipe in the registry instead.

### A height anywhere on Earth, from tiles on disk, 2026-09-18

**What is missing first:** the tiles must already be on disk - nothing fetches
them while the program runs - and no height has yet been compared with a
surveyed one. Nothing in the simulation or the renderer asks for a height yet.

**`world::Dem`** gives the DEM's height above the geoid, or above the WGS84
ellipsoid with the geoid added, at any latitude and longitude. Heights between
samples are bilinear from the four around them. Near a tile's south or east
edge those come from the next tile, and when that tile's samples are spaced
differently - north of 50 degrees, or a 90 m tile beside 30 m ones - its row or
column is interpolated to the point needed. The sea, which has no tiles, is at
zero. Positions on the grid are whole half-arc-seconds, in which every spacing
either dataset uses is a whole number, so tiles meet exactly. A tile that does
not fill its cell sample for sample is refused. Up to 16 tiles and 64 decoded
blocks are kept.

**Which tiles exist** is `assets/dem/coverage.txt`, 66 KB: for each 1-degree
cell, a 30 m tile, a 90 m tile only (25 cells around Armenia and Azerbaijan,
which the public 30 m set leaves out), or the sea. `tools/make_dem_coverage.py`
makes it from both buckets' tile lists, pinned by SHA-256 and fetched by the
tests, and a test checks the committed file is still what they make.

**The tests.** The coverage has 26,450 cells at 30 m, 25 at 90 m only and the
rest sea, and names Sydney's, the Tasman Sea's and Armenia's rightly. Tile
names and URLs follow the buckets'. On synthetic tiles built in the test, a
plane - which bilinear interpolation reproduces exactly - comes back to within
a millimetre at over a thousand places across 25 tiles with two longitude
spacings and one coarser tile among them, and exactly on their corners; a
height that depends on latitude comes back across the antimeridian from both
sides and from 540 degrees; both poles and past them; the coast falls to zero
halfway to the sea east and south, opening only the land tile; a tile for the
wrong cell is refused. On the real Sydney tile, the height at seven of the
independent decoder's samples, and halfway between two samples in different
internal tiles, comes back within a millimetre, and a point in the Tasman Sea
is at zero.

**Watched to fail:** edge samples placed with the latitude spacing rather than
the longitude spacing (the plane was 0.11 m out north of 50 degrees). The first
run also found a real bug: a latitude exactly on a whole degree was placed in
the tile to its north, whose southern edge is not its own, so an edge sample
asked for itself again until the depth guard stopped it, and a query at 33 S
opened the tile for 33-32 S that it did not need. A whole-degree latitude is
now the northern row of the tile below.

### The geoid, and zip archives, 2026-09-18

**What is missing first:** the geoid converts heights but nothing uses it yet:
there is still no height query. (The grid's terms were at first not found
stated; PROJ's data package records NGA's EGM2008 as public domain, quoted in
`ASSETS.md`.)

**`world/geoid.hpp`** reads GeographicLib's geoid grids in their PGM form and
interpolates bilinearly, wrapping longitude and clamping latitude at the poles.
**`world/zip.hpp`** reads stored and DEFLATE entries out of a zip archive, as
GeographicLib distributes its grids, checking each against its CRC-32; Zip64,
encryption and multi-disk archives are refused by name.
**`world/byte_source.hpp`** is where the GeoTIFF reader's byte sources went, so
the zip reader could share them.

**The tests.** CRC-32 against its published check value. A zip archive built in
the test, with stored, deflated and empty entries, reads back, and six bad ones
are each refused for their reason. A small synthetic grid interpolates exactly
as bilinear interpolation must: at samples, between columns, between rows, in
a cell, across 360 degrees, at negative longitudes, at 180 E and W, at both
poles and past them; five malformed grids are refused. **The real grid**,
`egm2008-5.zip` pinned by SHA-256, gives GeographicLib's own online GeoidEval
undulations at 14 places within the grid's stated 0.478 m - the worst is 0.118
m - including the Indian Ocean low (-106.9 m), New Guinea (+70.2 m), both
poles, and the date line from both sides.

**Watched to fail:** negative longitudes negated rather than wrapped (the
synthetic test at "a negative longitude is the same place", and Denver 27 m
out); the interpolation weights between columns swapped (New Guinea 0.55 m
out). The sanitized build also caught a real bug on the first run: a read of
zero bytes passed a null pointer to `memcpy`.

### Reading the Copernicus DEM's GeoTIFF tiles, 2026-09-18

**What is missing first:** there is no height query yet. Nothing converts the
DEM's heights, which are above the EGM2008 geoid, to heights above the WGS84
ellipsoid; nothing fetches tiles at run time; nothing knows which tile covers a
point, or interpolates between samples. What exists reads a tile's samples.

**DEFLATE, written here** (`world/inflate.hpp`): zlib streams and raw DEFLATE,
with the Adler-32 checked and a limit on how much a stream may decompress to.
It is not zlib because the renderer's dependencies will bring their own, and
two static zlibs in one program are one set of symbols too many. It is tested
against ten streams Python's zlib made (`tools/make_inflate_fixtures.py`) with
every strategy zlib has - fixed and dynamic codes, stored blocks, runs,
literals only, sync and full flushes, and matches 32,000 bytes back - and
against hand-built streams: stored blocks of the largest size, a failed
checksum, every length of three streams cut short, a decompression limit one
byte too small, eight kinds of malformed header and block, and 2,000
deterministic corruptions under the sanitizers.

**GeoTIFF** (`world/geotiff.hpp`): single-channel float32 rasters on WGS84
latitude and longitude, in tiles or strips, either byte order, uncompressed or
DEFLATE, with or without the floating-point predictor, sample grids of points
or areas, the GDAL no-data value, and overviews. Anything else - BigTIFF,
integer samples, projected coordinates, a transformation matrix - is refused
by name. Tests write GeoTIFFs in all 32 combinations of those layouts and read
every sample back, and write 14 files it must refuse, each for its stated
reason.

**The real tile.** `Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif`, pinned by
SHA-256, decodes to exactly the heights a separate decoder gave - Python, its
zlib, and the predictor written again from TIFF Technical Note 3 - at 15
samples across its internal tiles, including tile edges and the sea. Its
sample (0, 0) is exactly at 151 E, 33 S: the Copernicus grid's samples are
points on whole arc-seconds, and each tile's south and east edges belong to
its neighbours.

**Downloads for tests** (`tests/cmake/fetch.cmake`): the files the tests need,
listed with their SHA-256 in `tests/data/downloads/files.txt`, fetched once
before the tests that read them. Without the network those tests are skipped;
with `GLIDESLOPE_REQUIRE_NETWORK` set, as CI sets it, that fails. A file that
arrives with the wrong hash always fails and is deleted. CI keeps the
downloads between runs. The unit-test harness can now skip a test (exit 77).
The DEM's source, pinned file and licence, with its notices quoted, are in
`ASSETS.md`.

**Watched to fail:** a distance code's extra-bits entry one short (the far
fixture referred back before its start); the stored-block length check
removed (the malformed test caught it); the predictor's running sum removed
(the layouts test, and the real tile at sample (0, 0): 198.016 for 274.590); the
half-sample shift for area grids reversed (the layouts test); a wrong pinned
hash (the fetch failed and left nothing behind). The first run of the
truncation test took 277 s: zeros past the end of a stream decoded as literals
up to the limit. The decoder now refuses input read more than four bytes past
its end.

### Shaders, reversed depth and the floating origin, 2026-09-17

**CI run 35229869171 said:** Metal on macOS and Direct3D 12 (WARP) on Windows
pass every frame test - sky, window, reversed depth, floating origin - in every
preset, and Rocky 9 passes. Ubuntu's sanitized build failed on the leak rule
(below), and Vulkan on Windows could not start: the runner has no Vulkan loader
or driver. CI now installs Mesa's lavapipe from mesa-dist-win 26.2.0 and the
Khronos loader from LunarG's 1.4.357.0 runtime components, each pinned by
SHA-256.

**What was missing first, before that run:** everything here was proved on
Vulkan on Linux alone. The meshes are coloured boxes and quads standing
in for mountains and an aircraft, drawn by one unlit pipeline. There is no
terrain, no texture and no culling.

**The shader toolchain.** Shaders are GLSL, compiled during the build by
`glideslope_shaderc` (`tools/shaderc/main.cpp`): glslang makes SPIR-V,
SPIRV-Cross makes MSL and HLSL, and on Windows D3DCompile makes DXBC. The
output is a C++ source per shader with each form and the resource counts
SDL_GPU asks for, read from the SPIR-V. The compiler checks every resource
against SDL_GPU's documented layout and fails the build naming the one out of
place. `ext/glslang` (16.6.0) and `ext/spirv-cross` (vulkan-sdk-1.4.357.0) are
new submodules; the program links neither. The decision is recorded in
`REQUIREMENTS.md`. The MSL and HLSL for a two-texture fragment shader were read
and put textures, samplers and uniform buffers where SDL_GPU's documentation
says: `[[texture(0)]]`, `[[sampler(0)]]`, `[[buffer(0)]]`; `t0`/`s0` in
`space2`, `b0` in `space3`.

**The renderer draws meshes.** `gfx::Renderer` now has a D32 float depth buffer
and a mesh pipeline, uploads meshes, and draws them from a camera. Positions are
double-precision ECEF until each mesh's transform relative to the camera is
worked out, in double, and narrowed to float (`gfx/scene.hpp`). The projection
is perspective with an infinite far plane and reversed depth: cleared to 0,
kept when greater. The client has `--scene sky|origin|depth` and
`--at LAT,LON,HEIGHT` / `--at-ecef X,Y,Z` to put a scene anywhere on the Earth.
`--size` is parsed without `sscanf`, which MSVC refuses as deprecated.

**The tests.**
- `a_distant_mountain_and_a_nearby_aircraft_draw_with_no_z_fighting_on_<driver>`:
  a face 40 km away with another 1 m in front, and a skin 1 m away with a decal
  1 mm in front, the nearer of each drawn first. Every pixel of both halves,
  shot at the Earth's centre and at Sydney airport, must be the nearer surface.
- `a_scene_draws_the_same_at_the_earths_centre_and_far_from_it_on_<driver>`:
  boxes from 7 cm to 2 m, shot at the Earth's centre, on the equator, at 45 N
  45 E 10 km up and at Sydney on frames 1 and 60 - every file identical, and at
  least a quarter of the pixels not sky.
- Six shader compiler tests: a shader keeping the layout compiles; a uniform
  buffer in the wrong set, a texture not bound from 0, a separate sampler, push
  constants and a GLSL error each fail with the reason.

**Leaks, judged rather than switched off.** Mesa's lavapipe leaks two small
allocations on a worker thread when it builds a pipeline, and Xlib, behind a
window, keeps its resource database and input method; both are unloaded before
exit. `tests/cmake/client.cmake` reads the leak report and judges each leak by
who allocated it - the first frame outside the sanitizer and the C and C++
runtimes: a leak allocated in glideslope or SDL fails the test, one allocated
inside a library loaded at run time does not, even when SDL called it. The
first CI run judged by any frame in SDL, and Ubuntu's Xlib, which keeps frame
pointers, failed it; the Ubuntu report was replayed through the new rule and
all 105 of its leaks were judged Xlib's, and a `new int[1000]` and a leaked
`std::string` in `main` still failed. The windowed test no longer runs with
leak detection off.

**Watched to fail:**
- positions narrowed to float before the camera offset was taken: the equator,
  45 N 45 E and Sydney frames all differed from the centre's;
- conventional depth (near 0.1 m, far 100 km, cleared to 1, kept when less):
  all 1,536 pixels of the far pair showed the face behind, at both places;
- the depth test off: both pairs showed the surface behind in every pixel;
- `new int[1000]` leaked in the client's `main`: the sky test failed, quoting
  the frame in `main.cpp`;
- each shader compiler case, as listed.

CI also said clang-cl compiled SDL's MSVC-only inline functions under this
project's warnings: SDL's include directory came from `SDL3_Headers`, which was
not marked as a system target. It is now.

**Verified locally:** `linux-debug` (sanitized) and `linux-release` pass 55 of
55.

### A window, and Metal's video driver, 2026-09-17

CI run 35226940913: macOS passed both presets, Metal headless and in a window;
Ubuntu and Rocky 9 passed with the windowed test on Xvfb. Windows did not build
(below, fixed in the next entry).

**What is missing first:** Direct3D 12 and Metal have still not drawn a frame.
The first CI run of the frame tests failed on both before a device was asked
for, for reasons that had nothing to do with the GPU (below); this commit is
their second attempt. Windows Vulkan needs a Vulkan driver the runner may not
have.

**What CI said about the last commit** (run 35224985264): Ubuntu and Rocky 9
passed, Vulkan on lavapipe. Windows did not compile the client — MSVC's
`<string>` does not bring in `std::runtime_error`, which GCC's does; the client
now includes `<stdexcept>`. macOS failed
`the_client_renders_the_sky_headless_on_metal` in both presets with SDL's
"SDL_HINT_GPU_DRIVER metal unsupported!". SDL's `METAL_PrepareDriver` accepts
only a video driver that can create a Metal view, and headless set SDL's
offscreen video driver, which cannot. On macOS headless now keeps Cocoa's video
driver and simply opens no window; everywhere else it is still `offscreen`.

**A window, tested.** `gfx::Renderer::presented()` counts the frames that
reached the window's swapchain, and the client prints it after a windowed
`--shot`. `the_client_renders_the_sky_in_a_window_on_<driver>` opens a window,
renders ten frames, requires at least one to have been presented, and reads the
frame back as the headless test does. On Linux with no `DISPLAY` or
`WAYLAND_DISPLAY` it reports itself skipped; CI sets
`GLIDESLOPE_REQUIRE_WINDOW`, which makes that a failure, and runs the Linux
tests on Xvfb. The sanitized build runs the windowed test with leak detection
off: the X11 and driver libraries loaded for a window leak allocations whose
stacks hold no frame of glideslope or SDL. The headless test keeps it on.

**Watched to fail:** with the present count never incremented the test failed
with "no frame reached the window's swapchain"; with no display and
`GLIDESLOPE_REQUIRE_WINDOW` set it failed, and without it it skipped.

**Verified locally,** under WSLg: both frame tests pass in `linux-debug`,
presenting 10 of 10 frames to the window.

### SDL3, a GPU device, and a frame, 2026-09-17 — Vulkan on Linux only

**What is missing first:** only Vulkan has drawn a frame, on Linux, through
Mesa's lavapipe on the CPU. Direct3D 12 and Metal have never created a device;
CI tries them with this commit, on runners that have no GPU, and may not be able
to. No test opens a window: every frame so far is headless. The frame is the
sky's clear colour and nothing else.

**What exists.** `ext/sdl` is SDL 3.4.16, built statically by
`cmake/Sdl.cmake`. `glideslope_gfx` is a new library: `gfx::Renderer` owns an
SDL_GPU device on the driver asked for, or SDL's choice, renders into a 1280x720
(or `--size`) colour target cleared to the sky, blits it to a window when there
is one, and reads a frame back off the GPU; `gfx::save_bmp` writes it.
`glideslope`, the client, is a new executable: `--headless` renders without a
window, `--gpu-driver` picks Vulkan, Direct3D 12 or Metal, `--shot FILE` and
`--shot-at FRAME` write a frame and exit. The simulation still links no SDL;
the layering checks still pass.

**The test** — `the_client_renders_the_sky_headless_on_vulkan` on Linux, and
the same for `direct3d12` and `vulkan` on Windows and `metal` on macOS — runs
the client headless on that driver, requires it to report that driver, and
reads the BMP it wrote: header, bit masks, and all 3,072 pixels of a 64x48
frame, each channel within 1 of the sky times 255.

**Watched to fail:** a sky a shade greener failed with pixel 0's green at 168;
a render pass that loaded rather than cleared its target failed with red at 0.

CI now installs SDL's Linux development packages on Ubuntu and Rocky 9 (EPEL and
CRB on Rocky), and Mesa's lavapipe with the Vulkan loader.

**Verified locally:** `linux-release` passes 46 of 46; the frame test passes
under the sanitized `linux-debug` too, with no leak reports from SDL or Mesa.

### Positions on the Earth, 2026-09-17

**Proved on every platform** by CI run 35223462224 (commit `29ed2f3`), which
passed all 45 tests in every preset. Nothing uses the conversions yet — the
simulation keeps JSBSim's positions, and there is no renderer or terrain.

`glideslope_world` is a new library, and `src/world/geodesy.hpp` its first
file: `Ecef` (double-precision metres, Earth-centred and Earth-fixed),
`Geodetic` (latitude, longitude, height above the ellipsoid), and the
conversions between them on WGS84. The forward conversion is the exact formula;
the inverse is Vermeille's closed form (2004), with no iteration.

**Tests:**

- `wgs84_reference_points_convert_exactly` — the equator at 0°, 90° E and 180°,
  both poles (100 m above the South Pole at an arbitrary longitude), and the
  derived semi-minor axis against WGS84's published 6,356,752.314245 m.
- `geodetic_positions_round_trip_through_ecef_within_a_millimetre` — 819
  positions, counted: 13 latitudes (both poles exactly and a ten-millionth of a
  degree away, the equator exactly and a billionth either side), 9 longitudes
  (both sides of the date line, the prime meridian), and 7 heights from 430 m
  below sea level to 40 km. Latitude and longitude must come back within a
  billionth of a degree and the position within a millimetre; the worst was
  3.4 nanometres.
- `the_geodetic_conversion_agrees_with_jsbsims_within_a_millimetre` — the same
  819 positions converted back by JSBSim's `FGLocation`, a different algorithm
  set up with JSBSim's own WGS84 axes in feet, not this project's constants.

**Watched to fail, three ways:** a spherical Earth in the forward conversion
failed the reference and round-trip tests; one term dropped from the inverse's
height failed the round-trip and JSBSim tests; and a flattening of 1/298.26
instead of 1/298.257223563 failed the reference test and — once the JSBSim
check used JSBSim's own axes rather than this project's — the JSBSim test too.

**Verified locally:** `linux-release` passes 45 of 45.

### The packaged CLI flies, 2026-09-17

**Proved on every platform.** The `package` workflow's run 35223462263 (commit
`29ed2f3`) passed all five jobs, each unpacked copy flying the selftest to the
same hash as the CI build of its platform: `3bf3106980a5ad7b` from the Linux
tarball in both containers, `6d19773416ce7b3f` on macOS, `d9a3d51b8c2cdd52` on
Windows.

Every package's check now runs `glideslope_cli selftest` out of the unpacked
copy — the Linux tarball in the stock Rocky 9 and Ubuntu 24.04 containers, the
macOS tarball and the Windows zip from a separate directory — after
`--version` and `aircraft c172p`. The selftest needs the model, the tuned
propeller and the selftest log to have travelled with the program, and flies
five minutes with them. The workflow now also runs when anything under
`assets/` changes, since that is what a package carries.

### Cross-platform flight checks, 2026-09-17

**The platforms fly identically, to the precision they print.** CI run
35223462224 (commit `29ed2f3`) compared the five release builds for the first
time: every one of the nine figures agreed to its two printed decimals on GCC
on Ubuntu and Rocky 9, AppleClang, MSVC and clang-cl — a spread of zero against
the 1% allowed — and the five-minute selftest ended at -34.1073165, 151.2320529,
1,537.3 ft, heading 335.73°, 110.33 KCAS on all five.

**The hashes still differ**, as they were designed to be allowed to: Linux GCC
prints `3bf3106980a5ad7b` (the same on Ubuntu and Rocky 9), MSVC and clang-cl
both `d9a3d51b8c2cdd52`, AppleClang `6d19773416ce7b3f`. So does the development
machine's own release build (`13253df30045304a`, GCC 14.3.1) from CI's GCC 14.2
release build, while CI's matches the development machine's sanitized build.
The bits differ somewhere in five minutes of flight; where the aircraft is, to
seven decimal places, does not.

**The tolerances, set before any cross-platform number was seen:** every one of
the nine published figures within 1% of its published value across all five
release builds, ten times tighter than the handbook ranges; and the five-minute
selftest ending within 300 ft horizontally, 30 ft vertically, 1 kt and 2° of
heading of the first platform's. The selftest's hash is not compared: the
machines are expected to differ in the last bits.

**How.** Each CI job now runs `glideslope_cli figures c172p` and
`glideslope_cli selftest` on its release build — GCC on Ubuntu and on Rocky 9,
AppleClang, MSVC and clang-cl — and uploads what they print. A new job, "The
same flights everywhere", needs all four jobs, downloads the five results, and
runs `tests/cmake/cross_platform_flights.cmake`, which names the five platforms
and fails if any is missing.

**Its test** makes five platforms from this build's own output. Identical, with
one platform's climb rate 0.3% higher, they must be accepted; a glide ratio 1.9%
higher on one, the selftest ending 400 ft north on one, and one platform's
figures missing must each be refused. **Watched to fail for the right reasons:**
the first version of the test moved the glide ratio from 9.38 to 956 instead of
9.56, and the check refused it — correctly, but for a gross error rather than
the 2% it claimed to test. With the arithmetic fixed, each refusal's message
names the glide ratio's platforms, the 400 ft, and the missing platform. The
test is
`the_cross_platform_check_accepts_platforms_that_agree_and_refuses_one_that_does_not`.

### The selftest and its hash, 2026-09-17

**Proved on every platform.** CI run 35222154134 (commit `3a6ef0c`) passed all
41 tests in every preset, so on each the hash held run to run and moved when
the drag did; the `package` workflow (35222154187) passed too. The hash holds
on one build: the release and sanitized builds on the same machine print
different hashes, and different platforms are not expected to agree.

**The hash, and the rule that goes with it.** On the development machine
(Rocky Linux 10, GCC 14.3.1), `linux-release` prints
`hash 13253df30045304a` and `linux-debug` prints `hash 3bf3106980a5ad7b`. A
change that moves either is deliberate, and is written here with its reason.

`glideslope_cli selftest [NAME]` reads `assets/selftest/NAME.log` (default
`c172p`) and flies it: brake release with 10° of flap, rotation at 55.6 KCAS, a
climb at 75.4 KCAS, flaps up, a climbing turn at 20° of bank, levelling at
1,500 ft, a level turn at 25°, and cruise, for 300 s — 36,000 steps, 0.19 s in
release. After every step it folds sixteen fields of the aircraft's state into
a 64-bit FNV-1a hash, bit for bit, and prints the hash with where the flight
ended: at 1,537 ft, 110 KCAS, heading 336° for the development machine's
release build.

**The log is of pilot commands, not stick positions.** The first log held the
controls themselves — throttle, elevator, aileron — and the aircraft rotated,
rolled left, stalled and cartwheeled down the runway, over and over, for three
minutes: a stable aircraft cannot be flown open loop. The log now holds commands
(`throttle`, `flaps`, `brakes`, `rotate_at`, `hold_speed`, `hold_altitude`,
`bank`, `end`) that the test pilot flies, which is as fixed as the flight needs
to be and stays sensible when the model is retuned. On the ground the pilot now
steers along the starting heading: without it the take-off roll swerved 90°,
and a first version of the steering had its sign the wrong way round and spun
the aircraft on the runway, because this model's rudder yaws left for positive
commands.

`glideslope_cli` gained `--data DIR`, reading data from DIR instead of `data/`
beside the program.

**Tests, 41 now:**

- `the_selftest_prints_the_same_hash_every_run` — two runs, identical output.
- `a_one_line_change_to_the_physics_moves_the_selftest_hash` — the selftest
  flown on an unchanged copy of the data prints the same hash as the data
  itself, so `--data` really is read; on a copy whose zero-lift drag is 0.0311
  instead of 0.031 it prints a different one (`f7de68708bc9c1ce`).
- `a_selftest_log_with_a_command_it_does_not_know_is_refused_at_its_line`
- `the_cli_refuses_a_selftest_for_an_aircraft_with_no_log`

**Watched to fail:** adding the wall clock to the hash failed the run-to-run
test; hashing the simulation time alone failed the physics-change test.

**Verified locally:** `linux-release` passes 41 of 41, and the sanitized
`linux-debug` passes 41 of 41 in 182 seconds, up from 97; the selftest flights
under the sanitizers are most of the difference.

### State capture and set/resume, 2026-09-17

**Proved on every platform.** CI run 35221060150 (commit `7050a3f`) passed all
37 tests in every preset, the tracking tests among them. A restore settles
JSBSim's hidden states over two simulated seconds rather than setting them, so
it is close, not exact, and costs about 240 steps of the flight model each
time.

**The key technical risk of the design is answered: an aircraft can be put into
a captured state and flies on as the original does.** Restored into a fresh
instance in every phase the figure checks fly, and then flown alongside the
original on the original's controls:

| Phase | After 1 s | Worst over 10 s |
| --- | --- | --- |
| Static run-up | 0.002 ft, 0.013°, 0.003 kt | 0.006 ft, 0.074°, 0.066 kt |
| Take-off roll | 0.0003 ft, 0.0001°, 0.0004 kt | 0.012 ft, 0.000°, 0.001 kt |
| Full-throttle climb | 0.002 ft, 0.002°, 0.002 kt | 0.099 ft, 0.008°, 0.004 kt |
| Cruise | 0.011 ft, 0.011°, 0.013 kt | 0.557 ft, 0.020°, 0.031 kt |
| Glide, engine stopped | 0 | 0 |
| Stall approach, flaps up | 0.123 ft, 0.424°, 0.062 kt | 10.8 ft, 1.27°, 0.57 kt |
| Stall approach, full flaps | 0.087 ft, 0.341°, 0.046 kt | 8.9 ft, 1.17°, 0.37 kt |
| Level turn, 30° | 0.029 ft, 0.027°, 0.037 kt | 1.36 ft, 0.057°, 0.070 kt |

**The tolerance** is 0.5 ft, 1° and 0.25 kt after one second, and 25 ft, 3° and
1 kt over ten — about twice the worst measured, which is approaching a stall,
where small differences grow fastest. One second is what reconciliation needs:
it resets to the server's state and replays the inputs the server has not yet
seen, a quarter of a second of flying at 200 ms of latency. Ten seconds is a
margin beyond it. An aircraft that has flown on for five seconds with other
controls and is put back to the snapshot stays within 0.24 ft, 0.022° and
0.006 kt of a fresh restore over the next ten.

**How.** JSBSim has no snapshot call. `Aircraft::capture()` reads the rigid-body
state relative to the Earth (position, attitude quaternion, body velocities and
rates), each engine's running flag and propeller RPM, and every property that is
both readable and writable — 286 for the Cessna — except the run's settings,
the atmosphere and the rigid body. `Aircraft::restore()` starts a fresh instance
at the snapshot's position, then for two simulated seconds re-applies the
snapshot before every step so the hidden states converge, applies it once more,
recomputes the forces with no time passing, and seeds the integrators from them.

**What each part is for, found the hard way and each watched to fail:**

- **The Earth's rotation.** JSBSim integrates in an inertial frame whose angle
  to the Earth grows with time, so an inertial state from one instance means
  somewhere else in another. The first prototype put the copy 25,000 ft away
  after twenty seconds. Restore rebuilds the inertial position, orientation and
  velocity in the instance's own frame. Leaving out the position or the
  orientation fails both tracking tests, in at least five phases each; leaving
  out the velocity fails all eight phases.
- **Instance-relative property names.** Two instances in one process sit at
  different indices in the property tree; the prototype's writes went to the
  wrong one and silently changed nothing.
- **Readable and writable only.** `propulsion/set-running` is write-only and
  reads back as 0; restoring it stopped the engine.
- **The settling.** Restoring only what can be read left the engine 60 hp adrift
  and the copy 80 ft and 3.6° away after thirty seconds of manoeuvring. Without
  it the stall approaches and the turn fail.
- **The properties.** Without them — controls, flaps, fuel, mixture — six of the
  eight phases fail.
- **The propeller RPM.** Without it the stall approaches fail.
- **A sensible start.** A first version started a fresh instance at an altitude
  estimated from the Earth's mean radius — 37,000 ft underground at 34°S — and
  JSBSim's start left NaN in the engine; the snapshot now carries the geodetic
  position for that start.
- **One path for the rigid body.** Position, attitude and velocity properties
  are not restored, because their setters move the aircraft as a side effect.
  With both paths, removing the explicit inertial position changed no test; with
  one, removing it fails both tracking tests.

`src/sim/test_pilot.hpp` now holds the test pilot, shared by the figure checks
and these tests.

**Tests, 37 now:**

- `an_aircraft_restored_in_every_phase_of_flight_tracks_the_original` — eight
  phases, counted.
- `an_aircraft_put_back_to_an_earlier_snapshot_flies_on_as_a_fresh_restore_does`
- `a_snapshot_of_one_model_cannot_be_restored_into_another`

**Verified locally:** `linux-release` passes 37 of 37, and the sanitized
`linux-debug` passes 37 of 37 in 97 seconds.

### The Cessna 172P against its handbook, 2026-09-17

**Proved on every platform.** CI run 35219070877 (commit `e6b97eb`) passed all
34 tests in every preset — GCC 14.2 on Ubuntu and Rocky 9, AppleClang 17, MSVC
19.51 and clang-cl 20.1 — so all nine figures land in range on each, and the
`package` workflow (35219070739) passed with the tuned model in every package.
The handbook publishes no turn rate, so the turn is checked against physics
rather than a published number.

**JSBSim's own C172P did not fly to its handbook.** The checks below, flown on
JSBSim's untouched model at the same 2,400 lb, landed out of range on five of
nine, so the model was tuned — the question Phase 1 exists to ask. The tuning
is not hand-edited into the model: `tools/make_c172p.py` makes
`assets/jsbsim/` from the pinned files with each change listed and justified in
its docstring, and a test fails if the committed files differ from what it
makes.

| Figure | Handbook | Range | Stock JSBSim | glideslope |
| --- | --- | --- | --- | --- |
| Static RPM, full throttle | 2300 to 2420 | 2300 to 2420 | **2538** | 2316 |
| Take-off ground roll | 890 ft | ±10% | 940 | 922 |
| Climb, sea level, 76 KIAS | 700 ft/min | ±10% | **978** | 742 |
| Cruise, 8000 ft, 2650 RPM | 121 KTAS | ±3 kt | 123.1 | 119.7 |
| Glide, 65 KIAS, engine off | 9.1:1 | ±10% | 8.38 | 9.38 |
| Stall, flaps up | 51 to 52 KCAS | 49.5 to 54 | **54.4** | 51.1 |
| Stall, flaps 10 | 48 to 49 KCAS | 46 to 51 | **51.0** | 47.6 |
| Stall, flaps 30 | 46 KCAS | 44 to 48 | **48.8** | 45.6 |
| Turn, 30° bank | g·tan(bank)/V | ±3% | 98.8% | 99.0% |

The figures, their sources and their conditions are in
`assets/figures/c172p.xml`: every number is from the Cessna Model 172P Pilot's
Operating Handbook of 12 May 1981, by section and figure, with indicated
airspeeds converted to calibrated by its figure 5-1. The ranges are this
project's.

**What each change does**, measured by leaving it out and flying every check:

| Left out | Static RPM | Ground roll | Climb | Cruise | Glide |
| --- | --- | --- | --- | --- | --- |
| nothing | 2316 | 922 | 742 | 119.7 | 9.38 |
| propeller power and thrust factors | 2538 | 697 | 1147 | 121.5 | 9.38 |
| thrust boost below advance ratio 0.5 | 2316 | 1216 | 725 | 119.7 | 9.38 |
| drag changes | 2316 | 935 | 661 | 121.3 | 7.89 |

The stall speeds moved from 54.4, 51.0 and 48.8 KCAS to 51.1, 47.6 and 45.6 with
the lift-curve and flap-lift changes, which barely move anything else.

**How it was found.** A first prototype flew each figure and showed the
propeller over-revving (2,800 RPM in a full-throttle climb, past the 2,700 RPM
redline) and the stall limited by elevator travel rather than by the wing: the
elevator reached its stop at 16.5° angle of attack, at a lift coefficient of
1.45. The handbook's static RPM range, its airspeed calibration table and its
stall table then gave targets that separate the propeller from the airframe.
JSBSim's two other C172 models were flown too and were no closer: `c172x` glided
at 13:1 and `c172r` climbed at 905 ft/min against its handbook's 720.

**The flights** are in `src/sim/figures.cpp`, flown by a small test pilot — a
pitch-attitude hold with slower speed and altitude loops above it, a wing
leveller and a rudder that holds the sideslip at zero. It is not the Phase 4
autopilot and is not meant to fly like a person. A first version of its rudder
let the aircraft slip half a degree in the turn, which turned 2% slower than its
bank demanded; an integral took the slip out.

- **Static RPM:** brakes on, full throttle, mixture leaned in steps; the most
  RPM any mixture reaches.
- **Take-off:** flaps 10, full throttle against the brakes, released; ground
  distance until no wheel is on the ground, rotating at 55.6 KCAS (51 KIAS).
- **Climb:** full throttle at 75.4 KCAS (76 KIAS); the climb over forty seconds
  through sea level, after thirty to settle.
- **Cruise:** level at 8,000 ft, mixture leaned to the most RPM, then throttle
  holding 2,650 RPM; average true airspeed over the last fifty seconds of three
  minutes. It fails if the RPM cannot be held.
- **Glide:** mixture cut off, propeller windmilling, 66 KCAS (65 KIAS); ground
  distance over height lost across three minutes.
- **Stalls:** power off, from 70 KCAS the target speed falls one knot a second
  and the pilot follows it with the nose; the slowest calibrated airspeed.
- **Turn:** 30° bank level at 3,000 ft; the turn rate as a percentage of
  g·tan(bank)/true airspeed, both averaged over thirty seconds after a minute.

Every flight loads the aircraft as the figures file says and fails unless JSBSim
then weighs it at the file's 2,400 lb.

`glideslope::sim::Aircraft` gained `load()` and `property()`. The CLI gained
`glideslope_cli figures NAME [FIGURE]`, which prints each measurement against
its range and exits 1 if any is out. `assets/` is new: `assets/jsbsim/` (the
made model) and `assets/figures/`, copied to `data/` beside the programs; a
change to any file there now reconfigures, which `CONFIGURE_DEPENDS` alone had
not done — the first tuning run changed the assets and measured nothing new.

**Tests, 34 now:** one per figure —
`a_cessna_172p_at_full_power_climbs_near_its_published_rate` and eight like
it — plus
`every_published_figure_has_a_flight_and_every_flight_a_figure`,
`the_committed_cessna_172p_is_what_its_tuning_script_makes` (skipped, not
passed, where there is no Python 3), and CLI tests for one figure, a figure the
aircraft does not have, and an aircraft with no figures file.

**Watched to fail, six ways:** a hand edit to the committed model failed the
script check; writing the climb figure as 500 failed the climb test, naming the
measurement, the range and the source; a figure in the file with no flight, and
a flight in the code with no figure, each failed the coverage test; a loading
that came to 2,330 lb failed with the weight; and the stock propeller factors
failed the static RPM, take-off and climb tests.

**Verified locally:** `linux-release` passes 34 of 34, and the sanitized
`linux-debug` passes them too, the nine flights taking 44 of its 67 seconds.

### A fixed 120 Hz step, 2026-09-17

**Proved on every platform.** CI run 35216463701 (commit `1d2649c`) passed all
20 tests in every preset — GCC 14.2 on Ubuntu (sanitized and release) and Rocky
9, AppleClang 17 (sanitized and release), MSVC 19.51 (debug and release) and
clang-cl 20.1 — so the five differently-chunked flights ended in identical
states on each. Before that run it had been proved on Linux only.

`glideslope::sim::FixedStep` turns elapsed time into whole 120 Hz steps.
**The steps taken depend only on the total time elapsed**, never on how it was
divided: time is held in whole nanoseconds, and the steps due are computed from
the total each time, split into whole and part seconds so nothing overflows for
the 292 years an int64 of nanoseconds lasts. `alpha()` is how far the time since
the last step has got towards the next, for interpolation.

`glideslope::sim::Aircraft` now also takes initial conditions (position,
altitude, terrain elevation, heading, calibrated airspeed, engine running),
controls (elevator, aileron, rudder, throttle, mixture, flaps, brakes), steps
once per call with JSBSim's time step set to exactly 1/120 s, and reports its
state (time, position, attitude, body velocities and rates, calibrated
airspeed, climb rate, engine RPM). Positive elevator is stick back; JSBSim's own
command is the other way round, and the wrapper turns it over.

**A unit-test harness**, `tests/unit/harness.hpp`, is new: a registry of named
test functions in one executable, `glideslope_tests`, run one test per ctest.
`every_compiled_unit_test_is_registered_with_ctest` compares the executable's
`--list` with the names given to ctest, both ways, because a test compiled and
never registered would never run and nothing would say so.

**Tests, 20 now:**

- `the_fixed_step_counts_steps_from_the_total_time_alone` — 11 ways of dividing
  time (1 µs, 1 ms, 16 ms, 33 ms, a 60 Hz and a 144 Hz frame rounded to the
  nanosecond, one step less a nanosecond, 100 ms, 1 s, uneven chunks of 0 to 50
  ms, and all at once), each over 1 s and over 10 s. After *every* advance the
  steps taken must equal the steps in the time fed so far, as computed by the
  standard library's own ratio arithmetic, and the advance must have returned
  the difference.
- `the_fixed_step_reports_how_far_it_is_into_the_next_step` — 10.004 s is
  1200 steps and 0.48 of the next; 4,333,333 ns more is a third of a nanosecond
  short of completing it, and one nanosecond more completes it.
- `the_fixed_step_refuses_time_running_backwards`.
- `the_fixed_step_counts_a_century_without_overflowing` — 36,525 days,
  3.156e18 ns, whose steps would overflow if multiplied naively.
- `a_flight_fed_its_time_in_any_chunks_ends_in_the_same_state` — a Cessna over
  Sydney at 3,000 ft and 100 kt, flown for ten seconds by a script that is a
  function of the step number alone (throttle, a slow elevator sine, an aileron
  and a rudder input), with its time arriving in 1 ms, 16 ms, 100 ms and uneven
  chunks and all at once. All five must take 1200 steps and end in exactly the
  same state, compared field by field with `==`. The test also requires the
  flight to have rolled and the engine to be running, so the flights cannot
  agree by all doing nothing.
- `the_elevator_pulled_back_pitches_the_nose_up` — and pushed forward, down.
- `every_compiled_unit_test_is_registered_with_ctest`.

**Watched to fail, five ways.** Counting steps by adding up floating-point
fractions of a step failed the fixed-step test ("16 ms: after 400000000 ns, 47
steps taken, 48 due") and the flight test ("16 ms: took 1199 steps"). The first
version of the fixed-step test checked only the final count and **passed** that
same bug; it was changed to check after every advance and to include 16 ms and
33 ms chunks, and only then failed it. Setting the controls once per chunk of
time instead of once per step failed the flight test ("16 ms: ended in a
different state from 1 ms"). Reversing the elevator's sign failed the elevator
test. Leaving a compiled test out of the ctest list failed the registration
test.

**Verified locally:** `linux-release` and the sanitized `linux-debug` each pass
20 of 20. The flight test's five ten-second flights take about 0.04 s in
release and 1.9 s sanitized.

### JSBSim, pinned and built, 2026-09-17

**Proved on every platform.** JSBSim builds and the aircraft tests pass in every
preset (CI run 35216463701). The first run (35215481035) failed only in
`windows-clang`: enabling C left that preset with clang-cl for C++ and MSVC for
C, which CMake refuses, and commit `b234322` names clang-cl for both. The
`package` workflow (runs 35215392807 and 35216463589) passed on every platform,
running `glideslope_cli aircraft c172p` out of each unpacked package.

**What exists.** `ext/jsbsim` is JSBSim v1.3.1 (`3b25f25`), built by
`cmake/Jsbsim.cmake` from its `src/` directory only, statically, with its
headers as system headers and the sanitizers applied in the sanitized presets.
The project now enables C as well as C++, for JSBSim's bundled expat.
`ext/README.md` says why each of those choices was made.

- `glideslope_sim` links `libJSBSim`. `glideslope::sim::Aircraft` loads a model
  through `FGFDMExec`, keeping JSBSim's headers out of its own, and reports the
  model's figures. JSBSim's own output is turned off unless `JSBSIM_DEBUG` asks
  for it; it still prints to standard error when a model fails to load.
- `glideslope_platform` is new: `data_directory()` finds `data/` beside the
  running program, through `/proc/self/exe`, `_NSGetExecutablePath` or
  `GetModuleFileNameW`.
- The Cessna 172P's files — `aircraft/c172p/`, `engine/eng_io320.xml` and
  `engine/prop_75in2f.xml` — are copied at configure time into `data/jsbsim/` in
  the build tree, and installed into packages. Which aircraft are copied is a
  CMake list for now, which Phase 5 replaces with data.
- `glideslope_cli aircraft c172p` prints the model's name, description, wing
  area (174.0 sq ft), wingspan (35.8 ft), chord (4.9 ft), empty weight
  (1500.0 lb) and engine count (1). An aircraft with no files exits 1 naming
  it.
- Packages now carry the aircraft data and `licenses/JSBSim.txt`,
  `licenses/expat.txt` and `licenses/GeographicLib.txt`, and nothing else of
  JSBSim's: our install rules are the `glideslope` component, and CPack packages
  that component alone. The `package` workflow now also runs
  `glideslope_cli aircraft c172p` out of each unpacked copy, and checks the
  Linux tarball's licences.

**Tests, 13 now:**

- `the_cli_prints_the_figures_jsbsim_read_from_the_cessna_172_files` — reads
  the wing area, span, chord, empty weight and engine count straight out of
  `c172p.xml`, independently of JSBSim, requires each element's unit to be the
  one the CLI prints in, and compares all five with the CLI's output.
- `the_cli_refuses_an_aircraft_it_has_no_files_for` — exit 1, naming it.

**Watched to fail:** reading wing area from the tail's property failed the
figures test ("the CLI says 21.9, the file says 174"); making the refusal exit
0 failed the refusal test.

**Verified locally:** `linux-release` and the sanitized `linux-debug`, with
JSBSim sanitized too, each pass 13 of 13, with no sanitizer reports. JSBSim
builds in about 19 seconds of wall time. The layering check still passes with
`src/sim/` including JSBSim's headers, and a local package made with
`cpack --preset linux-release` unpacks to a folder whose
`glideslope_cli aircraft c172p` works.

### The living documents, honest, 2026-09-17 — Phase 0 complete

Every living document was re-read in full against the repository at the end of
Phase 0. What that pass changed:

- this file: a summary of the finished phase; the compiler gate's CI evidence
  moved from the warnings entry, where it had landed, to the gates entry; a
  hand check in the packaging entry relabelled as what came before the workflow;
- `RELEASES.md`: what a package is and what each operating system will say. It
  had said there was nothing to download; the workflow now keeps a package per
  platform as an artifact. What macOS and Windows say about a *downloaded*
  copy is stated as expected, not as seen, because no downloaded copy has been
  opened yet;
- `CLAUDE.md` and `README.md`: how to make a package.

`TRANSPORT.md`, `THREATS.md` and `GUIDE.md` still say their subject does not
exist, which is still true. `ASSETS.md` still says nothing third-party is used,
and `ext/README.md` that nothing is pinned; both true. `FEATURES.md` marks
nothing `DONE`, because nothing on it is a thing a player could use yet.

**The verification**, taken literally: a fresh agent with no other context was
given this file alone, told not to open anything else, and asked what works
today, what does not, how the working parts were proved, and what comes next.

**Its answer was right on every point** when checked against the repository and
the CI runs: the CLI's three behaviours and the version-only library; no flight
model, state restore, terrain, renderer or server; each item's proof by platform
and compiler, including that most deliberate breakages were made locally and
that the sanitizer claim rests on one small GCC program; no downloaded package
opened yet; and Phase 1 next.

It also named what it could not tell from the file alone, and this pass fixed
each: the summary listed six things beside "7 of 7"; the CI table had five rows
under "four jobs"; the package workflow's five jobs were not named; "gearstick",
"the brief", "presentation" and "tails" were not explained; and it was not
always clear which proofs were local and which were CI. The corrected file has
not been re-read by a fresh agent.

### Packaging, 2026-09-17

**Proved on every platform.** The `package` workflow's first run on `main`
(35199775868, commit `4b94984`) passed all five jobs — build the Linux tarball,
run it on Rocky 9, run it on Ubuntu 24.04, build and run the macOS tarball,
build and run the Windows zip. Each unpacked
`glideslope_cli --version` printed `glideslope_cli 0.1.0`, matching its file
name, and each checksum verified:

- the Linux tarball, built on Rocky 9, in stock `rockylinux/rockylinux:9` and
  `ubuntu:24.04` containers;
- the macOS tarball, whose program depends on `/usr/lib/libc++.1.dylib` and
  `/usr/lib/libSystem.B.dylib` only;
- the Windows zip, whose program depends on `KERNEL32.dll` only.

CI on the same commit (35199775840) was green in all four jobs.

**Watched to fail.** A throwaway branch removed the static runtime line and the
workflow was run on it by hand (35199809443): the Windows job alone went red,
with `dumpbin` listing `VCRUNTIME140.dll` and the step saying the packaged
program depends on the Visual C++ redistributable. The branch was deleted.

`cpack --preset linux-release` (and `macos-release`, `windows-release`) makes
`glideslope-0.1.0-<platform>.tar.gz` — `.zip` on Windows — with a SHA-256 file
beside it. It holds one folder of the same name with `glideslope_cli`,
`LICENSE` and `README.md` side by side. On Windows the C++ runtime is linked
into the program (`CMAKE_MSVC_RUNTIME_LIBRARY`), so a zip unpacked on a machine
without the Visual C++ redistributable still runs.

`.github/workflows/package.yml` runs on demand and when the build files change:

- **Linux:** built on Rocky 9 with gcc-toolset-14, then run in a stock
  `rockylinux/rockylinux:9` container and a stock `ubuntu:24.04` container,
  neither with a toolchain.
- **macOS:** built, unpacked into a different directory, and run.
- **Windows:** built, unpacked into a different directory, run, and read with
  `dumpbin /dependents`, which must not list `vcruntime` or `msvcp`.

Every run checks the checksum file, and requires `glideslope_cli --version` to
print exactly the version in the package's file name.

Before the workflow first ran, the same was checked by hand on Rocky Linux 10:
the tarball is 28 KB, unpacks to the one folder with its three files, and the
unpacked `glideslope_cli --version` prints `glideslope_cli 0.1.0` and exits 0.

### The simulation links no presentation, 2026-09-17

**Proved on every platform.** CI run 35199507565 (commit `c88b7f0`) passed all
11 tests in every preset, including the binary check reading `otool -L` on
macOS and `dumpbin /dependents` under both MSVC and clang-cl. The check fails
when it finds no dependencies at all, so a pass means each reader understood its
tool's output. Before that run it had been proved on Linux only.

`cmake/Layering.cmake` holds three checks.

**Includes, at configure time.** `glideslope_check_layering` reads every source
under `src/sim/` — `.h .hh .hpp .hxx .inl .ipp .c .cc .cpp .cxx`, recursively —
line by line, and refuses an `#include` of SDL's umbrella or entry-point headers
(3), its video headers (23), its input headers (12) or its audio header, taken
from `include/SDL3` at release-3.4.16, or of anything under `gfx/`, `ui/`,
`platform/` or `frontend/`, with or without leading `../`. The refusal names the
file, the line number and the line, for example
`src/sim/version.cpp:2: #include <SDL3/SDL_gamepad.h>    <- includes SDL
presentation (SDL_gamepad.h)`. SDL's non-presentation headers — threads,
atomics, timers — are not refused by this check. The text is read with `;`,
`[` and `]` swapped out first, because CMake lists split on the first and stop
splitting inside the other two, and C++ is full of all three.

**Links, at configure time.** `glideslope_check_sim_links` walks everything
`glideslope_sim` links — `LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES`,
through aliases and `$<LINK_ONLY:...>`, recursively — and refuses any SDL3
target, library name or library file. It is scheduled with
`cmake_language(DEFER)` so a `target_link_libraries` anywhere later in
`CMakeLists.txt` is still seen. This is stricter than the include check: SDL is
one library, and linking any of it links its video, input and audio code.

**The binary, at test time.** `tests/cmake/binary_dependencies.cmake` reads
the dynamic dependencies recorded in `glideslope_cli` itself and refuses any of
26 Linux, 16 macOS or 18 Windows windowing, graphics, input and audio libraries,
SDL included. It fails, rather than passing, if it finds no dependencies at all,
because that means the reader stopped understanding the tool's output. On Linux
today the CLI depends on `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1` and
`libc.so.6`.

**Tests:**

- `every_forbidden_include_in_the_simulation_fails_the_configure` — 64 cases,
  derived from the lists and counted: every forbidden SDL header (39); every
  layer as `"layer/x.hpp"`, `"../layer/x.hpp"` and `<layer/x.hpp>` (12); every
  scanned extension (10); a subdirectory, `#  include`, and an include after a
  line holding an unclosed `[` and a `;` (3). Each must be refused naming the
  file, line 3, and the include.
- `includes_that_only_look_like_presentation_are_allowed_in_the_simulation` —
  allowed SDL headers, `sim/` and `world/` headers, `gfxtools/`, `uikit.hpp`,
  `platforms/`, a commented-out forbidden include, one in a block comment, and
  one inside a string, all in one file that must be accepted.
- `sdl_linked_into_the_simulation_by_any_route_fails_the_configure` — 7 cases
  in `tests/layering_link/`: SDL linked directly, through a public dependency,
  through a private one, through an interface library, by plain library name,
  and by file path must each be refused; a simulation linking only non-SDL
  targets must be accepted.
- `the_cli_links_the_simulation_and_nothing_presentational` — the binary check
  on `glideslope_cli`.

**Watched to fail, seven ways**, all locally. In the real tree: a
`#include <SDL3/SDL_gamepad.h>` added to `src/sim/version.cpp` stopped the
configure naming `src/sim/version.cpp:2`; a `target_link_libraries` of
`SDL3::SDL3` into `glideslope_sim` appended to the end of `CMakeLists.txt`
stopped it naming the route; and linking the CLI against ALSA failed the binary
test naming `libasound.so.2`. In the checkers: matching only `::SDL3` failed the
library-name and file-path cases; removing the recursion failed the three
indirect cases; removing the `../` normalisation failed all four `../layer`
cases; and removing the `[` escaping failed the unclosed-bracket case.

Verified on Rocky Linux 10 with GCC 14.3.1: `linux-debug` and `linux-release`
each pass 11 of 11 tests, and every configure prints
`src/sim/ includes no presentation (2 files)` and
`glideslope_sim links no SDL`.

### Warnings as errors, 2026-09-17

**Proved on every compiler.** CI run 35198828479 (commit `d437341`) passed the
test below in every preset: GCC 14.2.0 on Ubuntu, GCC 14.2.1 on Rocky 9,
AppleClang 17.0.0, MSVC 19.51 (narrowing failing as C4244, the sign probe
excluded) and clang-cl 20.1.8 (both probes failing, through the conversion
warnings added for it). Before that run it had been proved with GCC and Clang
only.

`cmake/CompilerWarnings.cmake` gives every first-party target, through
`glideslope_configure`:

- GCC and Clang: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion -Wdouble-promotion -Wold-style-cast -Wnon-virtual-dtor
  -Woverloaded-virtual -Wimplicit-fallthrough`, and `-Werror`.
- MSVC: `/W4 /utf-8`, and `/WX`. clang-cl gets the same, plus
  `-Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`, because its
  `/W4` stops at `-Wall -Wextra`.

`GLIDESLOPE_WERROR` defaults to on and is defined in that file, not in
`CMakeLists.txt`, so anything including the file gets the project's default.
Nothing in the flags depends on the build type.

**The test** —
`a_narrowing_or_sign_conversion_fails_the_build_in_every_build_type` —
configures the small project in `tests/warnings/`, which includes the real
`CompilerWarnings.cmake`, with this build's own generator and compiler, once
for each of Debug, Release, RelWithDebInfo and MinSizeRel. In each it builds
three probes: one with explicit casts that must build, one narrowing a double
to a float that must fail naming `float-conversion` (C4244 on MSVC), and one
converting int to unsigned that must fail naming `sign-conversion`. That is 12
probe builds, counted. The sign probe is excluded under MSVC itself, and named
as excluded, because MSVC has no default warning for it.

**Watched to fail, four ways**, locally with GCC 14.3: dropping `-Wconversion`
failed the narrowing probe in all four build types; making `-Werror` apply to
Debug only failed both probes in the other three; dropping `-Wsign-conversion`
failed the sign probe in all four; and a syntax error in the clean probe failed
it in all four.

Verified on Rocky Linux 10: `linux-debug` and `linux-release` with GCC 14.3.1,
and a plain build with Clang 21, each pass 7 of 7 tests, and the project's own
code builds with no warnings under the full set.

### The 64-bit and compiler gates, 2026-09-17

`cmake/Platform.cmake` runs straight after `project()`. It refuses a toolchain
with pointers smaller than 8 bytes, and a compiler older than its floor —
GNU 14, Clang 19 (clang-cl included, which CMake reports as Clang),
AppleClang 16, MSVC 19.39 — with a message naming what it found and what it
needs; for GCC it also says how to enable gcc-toolset-14. A platform outside
Linux x86_64, Windows x64 and macOS arm64, or a compiler with no floor, is
warned about rather than refused. Every configure prints the resolved line, for
example `glideslope: linux-x86_64, GNU 14.3.1 (C++20)`.

The gate is written against `CMAKE_SYSTEM_NAME` and does no `try_compile`, so
it runs in CMake script mode against a toolchain described by `-D` flags. That
is how it is tested, because no machine here has a 32-bit toolchain or an old
compiler to hand:

- `a_32_bit_toolchain_is_refused_on_every_platform` — 3 cases: Linux, Windows
  and macOS, each with 4-byte pointers, each refused with "64-bit only" and
  "4-byte pointers" in the output.
- `every_compiler_is_refused_below_its_floor_and_accepted_at_it` — 8 cases,
  derived from the floor table rather than listed in the test: each compiler one
  version below its floor is refused with the found and required versions named,
  and at its floor is accepted with the resolved line printed. The test counts
  the cases it walked against twice the size of the table.

**Watched to fail, four ways**, locally. Relaxing the pointer check to `LESS 4`
failed all three pointer-size cases. Comparing floors with `VERSION_LESS_EQUAL`
failed every at-floor case. Downgrading the refusal to a warning failed every
below-floor case. Adding a compiler to the table with no platform for the test
to run it on failed with "walked 8 cases, expected 10".

**And against a real old compiler:** CI's Rocky 9 job configures once with the
system's own `g++` and requires the configure to fail with "required: GNU 14 or
newer" before building with gcc-toolset-14. In CI run 35198316459 (commit
`7921839`, green in all four jobs) that compiler was `g++ (GCC) 11.5.0`, refused
with `found: GNU 11.5.0`.

The same commit makes the later preset steps in each CI job run even when an
earlier one failed, closing the gap the deliberately red run showed.

Verified on Rocky Linux 10 with GCC 14.3.1: `linux-debug` and `linux-release`
each pass 6 of 6 tests.

### CI, and every preset on its own platform, 2026-09-17

`.github/workflows/ci.yml` runs every preset on the platform it belongs to, in
four jobs — Ubuntu, Rocky 9, macOS and Windows; the Windows job builds with two
compilers, which is why the table has five rows. Its first run on `main` (run
35197837005, commit `f5e3890`) was green in all four, each preset passing 4 of 4
tests:

| Job | Presets | Compiler |
| --- | --- | --- |
| Ubuntu | `linux-debug`, `linux-release` | GCC 14.2.0 |
| Rocky 9 container | `linux-release` | GCC 14.2.1 (gcc-toolset-14) |
| macOS 15 | `macos-debug`, `macos-release` | AppleClang 17.0.0 |
| Windows | `windows-debug`, `windows-release` | MSVC 19.51 |
| Windows | `windows-clang` | clang-cl 20.1.8 |

`linux-debug` runs on Ubuntu only, not in the Rocky job: the preset is the
same, and Rocky 9's job exists for its compiler and its distribution.

**CI was watched to fail.** A throwaway branch changed the expected version
string in one test and CI was run on it by hand (run 35198071126): all four
jobs went red, each naming the failed test and the expected string. The branch
was deleted afterwards. That run also showed a gap: a failing preset step
skipped the presets after it in the same job, so a debug failure hid whether
release passed.

`windows-*` steps run under `cmd`, where a failing command does not stop the
script; each command ends in `|| exit /b 1`, which is what made the red run red.

### The build and its presets, 2026-09-17

When this landed, only the Linux presets had run; the macOS and Windows presets
were first run by CI, in the entry above.

**What exists.** A C++20 CMake project, `LANGUAGES CXX` only, with Ninja presets
`linux-debug`, `linux-release`, `macos-debug`, `macos-release`,
`windows-debug`, `windows-release` and `windows-clang`, and a test preset for
each that fails when it finds no tests. C++20 because Cesium Native's own build
requires it.

- `glideslope_sim` — a static library holding `glideslope::sim::version()`,
  which returns the project version compiled in. It is the simulation in name
  only until JSBSim arrives.
- `glideslope_cli` — `--version` prints `glideslope_cli 0.1.0` and exits 0;
  `--help` prints usage to standard output and exits 0; no arguments, or any
  other argument, prints usage to standard error and exits 2.
- The debug presets on Linux and macOS build with
  `-fsanitize=address,undefined -fno-sanitize-recover=all`, from
  `cmake/Sanitizers.cmake`. The no-recover flag is the important one: a small
  signed-overflow program built with GCC 14 without it prints the
  undefined-behaviour report and still exits 0; with it, exits 1.
  `windows-debug` does not sanitize.

**Tests, four, covering every way the CLI can be called today:**
`the_cli_reports_the_project_version`, `the_cli_prints_its_usage_when_asked`,
`the_cli_with_no_arguments_prints_its_usage_and_fails` and
`the_cli_refuses_an_argument_it_does_not_know`. Each runs the CLI through
`tests/cmake/expect_run.cmake`, which checks the exit code *and* the output —
ctest's own `PASS_REGULAR_EXPRESSION` ignores the exit code. Every kind of check
the script makes was watched to fail: a wrong version string, a wrong exit code,
standard error not matching, and standard output not matching each end the
script with an error.

**Verification run.** On Rocky Linux 10 in WSL with GCC 14.3.1, CMake 3.31 and
Ninja 1.11: `linux-debug` and `linux-release` each configure, build and pass
4 of 4 tests. The sanitizer flags appear in every compile command of
`linux-debug` and in none of `linux-release`. The same tree also builds and
passes with Clang 21. `ldd glideslope_cli` lists the C++ runtime, libm, libgcc
and libc, and nothing else.

### The living documents, 2026-09-17

`CLAUDE.md`, `docs/FEATURES.md`, `docs/COMPLETION_PLAN.md`, this file,
`docs/TRANSPORT.md`, `docs/THREATS.md`, `docs/ASSETS.md`, `docs/GUIDE.md`,
`docs/RELEASES.md` and `ext/README.md` now exist, following gearstick's shapes.
The plan's phases are the ones `docs/REQUIREMENTS.md` section 7 starts from,
with the decisions of 2026-09-17 folded into their items. Five tails were
written down on the way: runways on the DEM, free buildings, macOS signing, a
single Linux download, and a hosted public server.

The executables are named `glideslope`, `glideslope_cli` and
`glideslope_server`, after the project as gearstick's are; the brief's
`flightsim_*` names were examples, and `REQUIREMENTS.md` now uses the real ones.

The documents whose subject does not exist yet — the transport, the threat
model, the player's guide, the releases — say so in one paragraph each rather
than describing plans as if they were facts.

The Phase 0 item for these documents stays unticked until the rest of Phase 0
has landed and the documents have been re-read against it.

### Requirements and licence, 2026-09-17

`docs/REQUIREMENTS.md` is the design brief in Markdown, with section 9 recording
the decisions closed and still open. `LICENSE` is the unmodified GPL-3.0 text;
the project is GPL-3.0-or-later.

---

## Known risks

- **JSBSim state set/resume.** Reconciliation needs to put an instance into a
  full captured state and have it fly on cleanly. Proved or disproved in
  Phase 1, before any networking is built on it.
- **The Cesium-to-SDL_GPU glue.** The biggest rendering risk; Phase 2.
- **Reading the Copernicus DEM directly.** Cesium Native streams only
  quantized-mesh terrain and 3D Tiles, so the open-data provider depends on a
  GeoTIFF reader this project writes; Phase 2.
- **Runway surfaces.** The DEM is a surface model sampled every 30 m, so
  runways carry bumps that are not there. Open in `REQUIREMENTS.md`.

---

### The runway lessons fly at their figures' weight, 2026-09-23

**Every lesson now flies its aeroplane at the weight the figures it is judged
against were measured at.** The lessons flown in the air already did; the
take-off lesson, the approach lesson, their two demonstrations and the circuit
built their aeroplane and loaded nothing. A take-off now loads the loading of
the figure its rotation speed comes from - the published ground roll where it
gives a lift-off speed, otherwise the flaps-up stall - and an approach the
loading of the stall with the most flap, which is the one its reference speed
is a third above. The circuit flies at the take-off's loading; for the five
aeroplanes taught it every figure is at one loading anyway.

**Two things broke at the new weights, and both were real.**

- **The circuit turned crosswind inside the stage that asks her to fly
  straight.** The lesson's climb out ends four hundred feet above where it
  began, about 605 ft; the circuit flight turned at 600. Lighter, the Cessna
  climbed the last five feet within a few degrees of turn; at 2,400 lb she was
  38 degrees round, so "keep her straight" was broken and every later stage
  began one turn early - the downwind leg was counted inside the turn on to it
  and came out short of its mile. The circuit flight turns at 650 ft now.
- **The take-off autopilot's climb law had nothing to damp it.** It held the
  climbing speed with a pitch trim alone, 2.4 degrees a second for each knot
  off, and an integral with no proportional term feeds the phugoid: the J-3
  Cub, at its Trainer loading, swung between 3 degrees nose down and 18 up
  every eight seconds. It met the crosswind turn at the top of a zoom with the
  speed falling, bled to 32 knots with 44 degrees of bank, stalled, and mushed
  seven hundred feet into the ground at 38 degrees of alpha with the elevator
  held fully up. The law is half a degree of nose a knot and a trim of 0.2
  degrees a second a knot; the Cessna's climb out, which swung 69 to 83 knots
  against its 75, now holds 75 to 78.

**Verification:** all five aeroplanes taught the circuit - the 172P, the 182S,
the J-3 Cub, the PA-28 and the Mosquito - fly it to an empty debrief at their
figures' weights; the take-off and approach lessons, their faults and both
demonstrations pass there; and the whole suite, 480 tests, passed on Linux
debug.

### Approaches for the airliners, the fighters and the bomber, 2026-09-23

**Every aeroplane that publishes or has measured a landing stall now flies
its class's approach lesson to an empty debrief**: the 737-300, 787-8, A320
and A380, the B-2, the F-15C and F-35B, the Learjet, the Mosquito and the
four light aircraft - thirteen. The 747-400 and F-22A are left out by name:
neither has a reference speed. What it took:

- **An approach starts established on it.** `InitialConditions` gained
  `flaps` (the landing flap already down, set in one step by running the
  initial conditions in trim mode), `flight_path_deg` (already on the
  three-degree path, not level at its height) and `trim` (JSBSim's
  longitudinal trim for that path). Started clean at its landing speed the
  A380 stalled before its flaps were a third out and fell 688 ft in ten
  seconds; started on the path but untrimmed - nose on the path, no angle of
  attack, no lift - the F-15C dropped at 41 ft/s and ran from 196 knots to 208
  before the elevator caught it. JSBSim cannot trim the B-2 or the Mosquito;
  they start as they did, the Mosquito's engines started again the way every
  constant-speed propeller's are, and the test says which started untrimmed.
- **The approach autopilot learns the attitude the path needs.** Its pitch
  was proportional to the vertical-speed error alone, so an aeroplane that
  flies the path nose-up could hold that attitude only by sinking faster than
  the path: the F-35B needs about twelve degrees at its reference speed, which
  took 2,000 ft/min of error, and the limit was ten. A slow trim now learns the
  attitude, from the one it has on the first step, and the limit is fifteen.
- **Power for the path on the back of the drag curve.** A sink more than 300
  ft/min faster than the glidepath asks opens the throttle in proportion. On
  its path an approach never comes near that margin.
- **Every approach lesson watches the descent**, `velocities/h-dot-fps >=
  -30` by the threshold: every stage ends on a height, and a crash reaches a
  height as surely as a landing does. The A380 that fell 688 ft was passed
  through all three stages.

**Verification:** `the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief`
flies all thirteen: the airliners within 3 to 6 knots of their reference
speed, the F-15C 195 to 198 against 196, the F-35B 159 to 166 against 159,
none sinking faster than 35 ft/s. **Not yet done:** the fault test still flies
only the Cessna.

### Every class's lesson is flown with its fault, 2026-09-23

**The fault half of the Lessons item's verification now covers every class's
lesson, not only the light aircraft's.** The six fault tests each flew the
Cessna 172P, which tested one lesson file of each exercise out of up to seven.
They now fly the first aeroplane of each class that can fly its class's lesson,
and assert that every class's lesson was covered: take-off 2 of 2, approach 6
of 6, turns 7 of 7, climbs and descents 7 of 7, stalls 3 of 3, circuit 2 of 2.

**"Names that fault and no other" is checked, not only "names that fault".**
Every line of the debrief must be one its own lesson gives for the property
the fault is seen on - the speed for an approach or a climb flown fast, the
height for a turn or a downwind leg that sinks and a stall recovered late, the
throttle for a take-off on part power, the rotation speed and the attitude for
rotating early. Four of the six used to check only that the fault was there.

**Two faults had to be flown the way they are meant, and the checks found
it.** A climb thirty knots fast is outside a light aeroplane's fifteen-knot
band and inside a jet's forty, so the 737 flew it to an empty debrief; the
fault is now flown fifteen knots past the top of each lesson's own band. And
rotating early held back stick from a standstill, which kept the Mosquito's
tail down the whole roll: hauled off at 107 knots she swung, and the debrief
named keeping her straight as well. The stick now comes back from 85 percent
of her rotation speed - off at 110 against 160, and the Cessna at 51 against
81 - and each debrief names the rotation speed and nothing it should not.

### Stalls for every class, entered in the landing configuration, 2026-09-23

**Fourteen aeroplanes now stall and recover to an empty debrief**, in seven
classes: the airliners, the fighters, the bomber and the flying boat join the
light aircraft, the Mosquito and the Learjet. The 747-400 and F-22A are left
out by name, having no stall speed.

- **The entry is flown flaps and gear down**, as they are for the landing,
  because a lesson's `stall` is the stall with everything down. Flown clean
  the 737 had to stall to reach twenty-five knots above its landing stall and
  departed: 52 degrees of alpha, 26 nose down, 21,000 ft/min. In the landing
  configuration it recovers losing 896 ft. The instructor's demonstration
  flies the same configuration, on the AI's controls and the pilot's alike, so
  handing over moves no flap.
- **The recovery unloads in proportion to the wing**: the autopilot is asked
  for twelve feet a minute of descent for each knot of stall speed, never less
  than six hundred. At six hundred the A380, flaps down at 120 knots, pitched
  up to hold so little sink and porpoised between four degrees nose down and
  twenty-one up, stalling again each time, for four minutes; at 1,260 it
  recovers losing 887 ft.
- **The bands are measured**, with half as much again for room. Entry and
  recovery, worst of each class: light aircraft 74 and 151 ft, Mosquito 40
  and 380, Learjet 58 and 225, airliners 128 and 896, fighters 468 (the
  F-35B) and 260, B-2 8 and 91, Short S.23 36 and 71. The light aircraft's
  entry band was 400 ft wide on the PA-28's account alone: clean it lost 321
  ft entering the stall, five times the others; in the landing configuration
  it loses 62, and the band is 150.

**Verification:** `the_stalls_lesson_flown_by_the_book_leaves_an_empty_debrief`
and `an_instructor_demonstrates_a_stall_and_hands_it_over` fly all fourteen;
`a_stall_recovered_badly_is_named_in_the_debrief` flies one aeroplane of each
of the seven classes' lessons late and names the height lost and nothing else.

## Detail moved from the completion plan, 2026-09-23

`COMPLETION_PLAN.md` was cut down to a short task list on 2026-09-23. What it
held beyond that is kept here verbatim, phase by phase and item by item, as it
stood that day. The plan's original introduction:

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

### Phase 0 — Foundations

#### C++20 + CMake/Ninja build

- [x] **C++20 + CMake/Ninja build**, presets per platform with matching test
      presets. *Verification: every preset configures, builds and tests on its
      own platform.*

#### 64-bit-only and compiler gates.

- [x] **64-bit-only and compiler gates.** *Verification: a 32-bit toolchain, and
      each supported compiler one version below its floor, are refused with a
      message naming what was found and what is needed; the configure line names
      the resolved platform and compiler.*

#### Warnings as errors in every build type

- [x] **Warnings as errors in every build type**, first-party targets only.
      *Verification: a double-to-float narrowing fails the build in every build
      type on every compiler, and a sign conversion does on GCC and Clang.*

#### The simulation links no presentation

- [x] **The simulation links no presentation**, checked at configure time.
      *Verification: every forbidden include placed in `src/sim/` fails the
      configure naming the file and the line, linking SDL into the simulation
      fails it too, and `glideslope_cli` links the simulation and nothing
      presentational.*

#### CI on Ubuntu, Rocky 9, Windows and macOS

- [x] **CI on Ubuntu, Rocky 9, Windows and macOS**, building and testing every
      preset. *Verification: every job green on `main`.*

#### Packaging

- [x] **Packaging** — a tarball for Linux and macOS and a zip for Windows, each
      running in place. *Verification: the `package` workflow unpacks each
      artifact into a different directory on its own platform and runs
      `glideslope_cli` out of the unpacked copy.*

#### The living documents exist and are honest

- [x] **The living documents exist and are honest** — `CLAUDE.md`, this plan,
      `FEATURES.md`, `PROJECT_STATUS.md`, `TRANSPORT.md`, `THREATS.md`,
      `ASSETS.md`, `GUIDE.md`, `RELEASES.md` and `ext/README.md`.
      *Verification: someone who has not seen the code can say what works from
      `PROJECT_STATUS.md` alone.*

### Phase 1 — The feel

#### JSBSim, pinned and built

- [x] **JSBSim, pinned and built**, linked by the simulation, with the Cessna
      172 loaded from its model files. *Verification:
      `glideslope_cli` loads the Cessna 172 and prints the figures JSBSim read
      from its files, and the layering check still passes.*

#### A fixed 120 Hz step with an accumulator.

- [x] **A fixed 120 Hz step with an accumulator.** *Verification: the same
      scripted flight fed its time in small, large and uneven chunks ends in the
      same state.*

#### Scripted flight checked against published figures

- [x] **Scripted flight checked against published figures** — static RPM,
      take-off roll, climb rate, cruise speed, glide ratio, stall speeds, and a
      turn rate against the physics of a coordinated turn, since the handbook
      publishes none. **In progress:** all in range on Linux; CI proves the
      other platforms. *Verification: each figure lands within its stated
      tolerance of the Cessna 172's published number.*

#### State capture and set/resume

- [x] **State capture and set/resume** — a full aircraft state written into a
      fresh instance, which flies on from it. *Verification: an instance
      restored mid-flight tracks the original within a stated tolerance, from a
      capture taken in every phase of flight the scripted flights cover.*

#### The same-machine replay hash

- [x] **The same-machine replay hash** — `glideslope_cli selftest` flies a fixed
      input log and prints a state hash. *Verification: the hash is identical
      run to run on one build, and a deliberate one-line change to the physics
      moves it.*

#### Cross-platform flight checks by tolerance.

- [x] **Cross-platform flight checks by tolerance.** *Verification: every CI
      platform flies the same scripted inputs and agrees with the published
      figures, and with the other platforms, within stated tolerances.*

#### The packaged CLI flies.

- [x] **The packaged CLI flies.** *Verification: the
      `package` workflow runs `glideslope_cli selftest` out of every unpacked
      artifact.*

### Phase 2 — The world

#### Earth-centred, Earth-fixed positions in double precision

- [x] **Earth-centred, Earth-fixed positions in double precision**, with
      latitude, longitude and height conversions. *Verification: conversions
      round-trip within a millimetre at the poles, the equator, the date line,
      and from below sea level to cruising altitude.*

#### A camera-relative floating origin.

- [x] **A camera-relative floating origin.** *Verification: a still scene
      rendered far from the origin is identical frame to frame, with no jitter.*

#### Reversed-Z depth.

- [x] **Reversed-Z depth.** *Verification: a frame holding both distant
      mountains and a nearby aircraft draws with no z-fighting at either.*

#### The Copernicus DEM, read directly

- [x] **The Copernicus DEM, read directly** from its GeoTIFF files at a pinned
      version, with a height query anywhere on Earth. *Verification (amended
      2026-09-18): heights at surveyed airfields and coastlines match within
      the dataset's stated accuracy, and the version and hash are in
      `ASSETS.md`. Summits were in the original verification, and cannot be:
      a 30 m grid does not hold a peak, and the DEM lies 8 to 35 m below five
      surveyed summits; that shortfall is pinned by a test and named in
      `PROJECT_STATUS.md`.*

#### Collision terrain in the simulation.

- [x] **Collision terrain in the simulation.** *Verification: an aircraft set
      down at sea level, at a high airfield and on a slope rests on the DEM
      surface in each.*

#### A window and a GPU device through SDL3

- [x] **A window and a GPU device through SDL3** on Vulkan, D3D12 and Metal.
      *Verification: the client writes a frame with `--shot` on every backend
      of every platform that has it.*

#### Cesium Native drawing the open-data terrain through SDL_GPU

- [x] **Cesium Native drawing the open-data terrain through SDL_GPU** around one
      region. *Verification: a `--shot` of a known region matches a reference
      frame of it within a stated tolerance.*

#### Open imagery on the terrain

- [x] **Open imagery on the terrain**, from the source `REQUIREMENTS.md`
      settles on. *Verification: a `--shot`
      shows the imagery and its attribution.*

#### Joysticks, HOTAS and yokes.

- [x] **Joysticks, HOTAS and yokes.** *Verification: every axis and button of a
      virtual device reaches the aircraft's controls, walked by test.*

#### A basic HUD

- [x] **A basic HUD** — airspeed, altitude, heading, vertical speed, attitude.
      *Verification: the numbers in a `--shot` match the simulation's state at
      that tick.*

#### The client's test flags

- [x] **The client's test flags** — `--shot FILE`, `--shot-at TICK`,
      `--trace`, `--screen NAME`. *Verification: each is used by a ctest.*

#### A frame rendered headless in CI and from every package.

- [x] **A frame rendered headless in CI and from every package.**
      *Verification: CI and the `package` workflow each upload a frame from
      every platform.*

### Phase 3 — Weather

#### METARs from aviationweather.gov.

- [x] **METARs from aviationweather.gov.** *Verification: a recorded METAR sets the surface wind, temperature and
      pressure it reports.*

#### Winds aloft from Open-Meteo.

- [x] **Winds aloft from Open-Meteo.** *Verification: a recorded response sets the wind at every level it
      reports, and between levels.*

#### Weather in JSBSim's atmosphere, with turbulence.

- [x] **Weather in JSBSim's atmosphere, with turbulence.** *Verification: an aircraft in a steady crosswind drifts at
      the rate the wind predicts, and turbulence disturbs it within a stated
      bound when on and not at all when off.*

#### Weather that changes during a flight without a jump.

- [x] **Weather that changes during a flight without a jump.** *Verification: a new report blends in over a stated
      interval with no step in the wind.*

### Phase 3b — Wind that shears and gusts, and hazardous air

Beyond the steady wind of Phase 3 (`FEATURES.md`: wind that shears and gusts,
hazardous air, weather you can see, the same air for everyone). The server's
weather is authoritative, and every client's prediction must fly the same air,
so everything here is a function of position, time and the weather's shared
parameters alone - computed here, not in JSBSim's own gust and turbulence
models, whose hidden state a restored aircraft would not carry.

#### The same air on every machine.

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

#### A METAR's gusts flown

- [x] **A METAR's gusts flown**, and its turbulence judged from the gusts'
      spread. *Verification: a recorded gusty METAR gives winds between its
      mean and gust speeds, reaching the gust within a stated tolerance over a
      stated time; a report without gusts gives none; the same flight twice is
      the same.*

#### The wind near the ground as a boundary layer

- [x] **The wind near the ground as a boundary layer**, from Open-Meteo's 10,
      80, 120 and 180 m winds and a logarithmic profile below them.
      *Verification: a recorded response sets the wind at each of those
      heights to what it reports, and between them to the profile within a
      stated bound; a 3-degree approach flies down through the shear the
      profile gives.*

#### Reported wind shear read

- [x] **Reported wind shear read** - `WS RWY`, `WS ALL RWY`, and the `WSHFT`
      and `PK WND` remarks. *Verification: recorded METARs carrying each decode
      to what they report, and a report of shear on a runway gives its approach
      the shear the model states.*

#### Microbursts

- [x] **Microbursts**, placed by the weather's parameters. *Verification: an
      aircraft on a 3-degree approach through a microburst of stated strength
      meets the headwind, downdraught and tailwind the published outflow model
      gives, at every point within a stated tolerance.*

#### Thermals and mountain waves

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

#### Weather you can see

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

### Phase 4 — Autopilot and navigation

#### Holds for heading, altitude, airspeed and vertical speed.

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

#### Waypoint following and flight plans as data.

- [x] **Waypoint following and flight plans as data.** *Verification: a plan loaded from a file passes every
      waypoint within a stated distance.* Stated, 2026-09-18: the Cessna flies
      `assets/plans/sydney-harbour.plan` - four waypoints over 45 km, with
      turns of 90 and 135 degrees and climbs and descents of 500 ft - past
      every waypoint in order, within 100 m of each and within 50 ft of its
      altitude, in calm air and in a 20 kt wind across and against it.

#### The user/AI controller swap.

- [x] **The user/AI controller swap.** *Verification: swapping mid-flight in either direction causes no step in
      any control or in the aircraft's state, in every phase of flight.*
      Stated, 2026-09-19: in the takeoff roll, the climb, the cruise, a
      30-degree turn, the descent and a flapped approach, handing the Cessna to
      the AI moves no control more than 0.01 of its travel in a step; handing it
      back moves none faster than a pilot's hand - full travel in a second -
      until the controls meet the pilot's, within two seconds, and they are the
      pilot's from then; and over the three seconds after either, the load
      factor changes by no more than 0.05 g in a step.

#### `--autopilot`

- [x] **`--autopilot`** — the AI flies this client's aircraft. *Verification: a ctest flies a plan with it.* Stated,
      2026-09-19: the client, headless on every driver, flies
      `--plan sydney-harbour` from the plan's start and prints each waypoint
      as its AI passes it - every one passed in order, within 100 m and 50 ft
      of its altitude - and the plan flown to its end.

### Phase 5 — Aircraft choice

The roster (`REQUIREMENTS.md`, section 4.2): light aircraft - the Piper J-3
Cub, Cessna 172P, Piper PA-28 and Cessna 182; a seaplane, the Short S.23 Empire
flying boat; from the Second World War, the de Havilland Mosquito; a business
jet, the Learjet 35A; the airliners Airbus A320 and A380 and Boeing 737, 747
and 787; the fighters F-15 Eagle, F-22 Raptor and F-35B Lightning II; and a
bomber, the B-2 Spirit. JSBSim ships flight models for all but the A380, the
Learjet, the Mosquito, the F-35B and the B-2, and those it ships are of uneven
quality: each is held to published figures before it is offered, as the Cessna
172P was. The five it does not ship are written here, from published data.

#### Aircraft as data.

- [x] **Aircraft as data.** *Verification: an
      aircraft is added without a code change, and a test walks every aircraft
      the data holds.* Stated, 2026-09-19: a copy of the data with one
      `.aircraft` file more holds one aircraft more, as its file describes,
      which loads and flies; and every aircraft the data holds loads and, on
      the autopilot, holds 3,000 ft within 100 ft and its catalogue airspeed
      within 5 kt for a minute.

#### A Mosquito flight model

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

#### The light aircraft fly to their figures

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

#### The airliners fly to their figures

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

#### The F-15 and F-22 fly to their figures

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

#### An A380 flight model

- [x] **An A380 flight model**, written here from published data: JSBSim has
      none. *Verification: as the airliners.* Stated, 2026-09-19, CI run
      35435906751: the A380-841, written from Airbus's A380 Aircraft
      Characteristics, its type certificates and the Boeing 747's published
      derivatives, flies the airliners' four figures inside their ranges on
      every platform - 9,300 ft of take-off field at 575,000 kg against the
      AC's 9,734, 4.2% with an engine out against JAR 25's 3.0, Mach 0.91 level
      at 35,000 ft, and still climbing at its 43,000 ft maximum operating
      altitude.

#### A Learjet 35A flight model

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

#### F-35B and B-2 flight models

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

#### The Short S.23 on water

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

#### Water where the DEM says it is

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

#### The HUD for fast aircraft

- [x] **The HUD for fast aircraft** - Mach and flight level where they apply.
      *Verification: the numbers in a `--shot` of a jet at altitude match its
      state at that tick, as for the Cessna.* Stated, 2026-09-20, CI run
      35449051367: on every platform and GPU driver a shot of the A320 at 11,000 m
      over Sydney shows MACH and FL lines that match its traced state at that
      tick, read back out of the frame, and a Cessna's shows neither.

#### Visual models from FlightGear aircraft

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

#### A visual model put where its aeroplane is.

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

#### An aircraft chosen at start.

- [x] **An aircraft chosen at start.** *Verification: every aircraft can be
      chosen, takes off, and passes its published-figure checks.* Stated,
      2026-09-19, CI run 35442214130: `--aircraft` flies any of the fifteen the
      data holds, in the air or `--on-ground`, and refuses one it does not,
      naming those it does; a test stands each on a runway and every one
      climbs through 200 ft; each passes its published-figure checks; and in
      the client on every platform the F-22 chosen flies at its start's 300
      knots and a Cessna on the ground at Sydney stands at rest on the DEM.

#### Views: the cockpit, and outside from ahead, behind, left, right and above, and a free orbit

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

### Phase 5b — Terrain providers

#### Cesium ion as a visual provider

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

#### Google Photorealistic 3D Tiles

- [x] **Google Photorealistic 3D Tiles** with the user's own Google Maps
      Platform key or Cesium ion token. *Verification: the same checks as
      Cesium ion, through both ways in.* Done, 2026-09-22, when the project
      owner supplied a Maps Platform key - which was the only thing missing.
      **Both ways in are now drawn separately and counted**: the test gives
      each its own configuration directory holding that one secret, so a
      machine that has both cannot quietly test one twice. Each drew 241
      tiles over Mount Taranaki with Google's attribution on screen, and a
      way in this machine has no secret for is reported and skipped rather
      than passed. Without either, the provider says which two things it will
      take and where to put them. **Watched failing**: with the direct-key
      branch disabled, the ion route still drew and the other was told it
      "drew nothing", which is the half that had never run before today.

#### Attribution on screen for whichever provider is active.

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

#### A measured visual-to-collision terrain mismatch.

- [x] **A measured visual-to-collision terrain mismatch.** *Verification: the
      bound for each provider at a set of reference airfields is stated in
      `PROJECT_STATUS.md`, with how it was measured.* Stated, 2026-09-21:
      `--mismatch` samples the drawn surface at the twelve surveyed runway
      ends of six airfields and prints it against the DEM the aircraft meets.
      **The open provider is 0.18 m from it at worst, Cesium ion 10.2 m and
      Google 10.1 m**, and a test holds each to that. All three are worst at
      Barrow, where the Arctic coast is thinly surveyed by anyone.

### Phase 5c — Learning to fly

Checklists and lessons (`FEATURES.md`, learning to fly). They build on the AI
pilot (Phase 4), which demonstrates, and on aircraft as data (Phase 5), which
checklists are part of. A lesson ends in a debrief, never a score
(`FEATURES.md`, deliberately not).

#### Checklists as part of each aircraft's data

- [x] **Checklists as part of each aircraft's data**, for every phase of
      flight, in this project's own words - **from the aeroplane's own
      handbook where one is public, and otherwise from the ordinary practice
      for the type** - with `ASSETS.md` saying which of the two each aeroplane
      is, and why, aeroplane by aeroplane. *Verification: every aircraft in the
      roster has a checklist for each phase of flight, and every item either
      names a state of the aircraft that shows it done or is marked the pilot's
      to confirm; a test walks them all.* Done 2026-09-23. **The wording was
      narrowed that day, by the project owner, from "from its handbook or
      pilot's notes"**: ten of the sixteen have no lawfully public manual - the
      B-2A's, F-22A's and F-35B's do not exist publicly, the airliners' and the
      Learjet's are their operators' and unpublished, and the Short S.23's were
      searched for and not found - and the operator manuals that circulate on
      document-sharing sites are reposted without permission and are not a
      source this project will record. `ASSETS.md` names none where none was
      read, rather than naming one that was not used.
      All sixteen aircraft have all nine phases, 144 checklists in all, and
      seven tests walk them: every item either names a state its own aircraft
      really has or is the pilot's to confirm, every band is one its
      aeroplane's controls can reach, and a file that is wrong is refused
      where it is wrong. **Six of the sixteen are now written from the
      aeroplane's own handbook** - the Cessna 172P's and 182S's Pilot's
      Operating Handbooks, the PA-28-180's Owner's Handbook, the Mosquito
      FB 6's Pilot's Notes (A.P. 2019E, January 1950), the F-15C's flight
      manual (T.O. 1F-15A-1) and the J-3 Cub's Owner's Manual - each read
      from the copies `ASSETS.md`
      records, their items and figures kept in the handbook's own order.
      The last two were added on 2026-09-22, when the reason this item gave
      for skipping them turned out not to cover them: it named thirteen and
      explained ten, and the Mosquito's Notes were already in `ASSETS.md`
      for its figures. Reading them corrected three things in the Mosquito's
      file - radiator shutters open for starting where para. 37 shuts them,
      fuel on the main tanks where it starts on the outer ones, and a climb
      at the 1944 edition's 148 knots where the 1950 edition gives 150.
      **The J-3 Cub was added the same day**, when "survives only as scans"
      turned out not to be a reason: a missing text layer stops a text search
      and stops nothing else, and its "Flying Hints" were read as pages.
      **The other ten have no public manual**, and say so. No flight manual is
      public for the B-2A, the F-22A or the
      F-35B; the Short S.23's were searched for and none found; and the
      airliners' and business jet's operating manuals are
      their operators' and are not published. Those ten follow the
      ordinary practice for the type with figures from this project's own
      `assets/figures/<id>.xml`, and `ASSETS.md` says which is which rather
      than naming a source that was not used, aeroplane by aeroplane.

#### Checklists on screen, ticking themselves.

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

#### Lessons

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
      although it is written after.** **The classes of aircraft are data now**, as of
      2026-09-22: every one of the sixteen carries a `class` line - one of
      light-aircraft, seaplane, second-world-war, business-jet, airliner,
      fighter or bomber - a file without one is refused, and a test holds the
      roster to `REQUIREMENTS.md`'s own table in section 4.2, aircraft by
      aircraft, so the prose and the data cannot drift apart. Two pieces of it are now built and tested, for the light
      aircraft: an autopilot that flies an approach and lands (Phase 8's own
      item, ticked below), and one that takes off - each light aircraft holds
      the centreline within 1.4 m and is airborne within a third of its
      handbook's ground roll. **The lesson and debrief layer is built,
      2026-09-22.** A lesson is data - `assets/lessons/*.lesson`, a sequence
      of stages each with what to do, what ends it, bands that must hold
      through it and things that must be true by its end - and a runner flies
      one and leaves a debrief: what to do differently, in the order it
      happened, with no score. The first lesson is the light aircraft's
      take-off, and all four fly it by the book with an empty debrief; flown
      on part throttle the debrief holds one thing, and it is the throttle.
      **A lesson figure may be the aeroplane own published speed**, which is
      what makes teaching a class work: `rotate`, `climb`, with an offset -
      `rotate-3` - resolved per aeroplane from its `figures/<id>.xml` through
      `sim::departure_speeds`. The same line of data holds the Cub to 31
      knots and the Cessna to 52. It was added the same day it was found
      wanting: a plain number could only be the slowest aeroplane in the
      class, so a 172 hauled off at 47 knots against its own 55 went
      unremarked. It is remarked now.
      **The approach and landing is the second lesson, 2026-09-22**, and
      `vref` joins `rotate` and `climb` as a figure an aeroplane publishes.
      All four light aircraft fly it down a three-degree glidepath from two
      miles with an empty debrief; flown twenty knots fast the debrief names
      holding the approach speed and crossing the threshold on speed, both of
      which are true. **No band watches the flare**, because the flare is
      where the speed is meant to go - a first draft held one to five feet
      and faulted every correct landing there is.
      **Turns, and climbs and descents, are the third and fourth lessons,
      2026-09-22**, and `start` joins the figures a lesson may name: what
      that watch own property read when the stage began, which is how a
      lesson says "hold the height you are at" without knowing where the
      aeroplane is. All four light aircraft fly both by the book with an
      empty debrief - through ninety degrees of turn holding their height to
      within ten feet, and up nine hundred feet and back down at their own
      climbing speed. Let sink through the turn, the debrief says to hold
      your height; flown thirty knots fast in the climb, it says to hold the
      climbing speed.
      **Stalls are the fifth lesson, 2026-09-22**, and `stall` the fifth
      figure an aeroplane may publish to one. The stall is entered the way
      one is entered - throttle closed, the height held, the nose rising as
      she slows - and all four recover losing between 59 and 196 feet with an
      empty debrief. Left twenty-five seconds past the stall the 172 loses
      430 and the debrief says to recover with the least height you can.
      **`lesson/turned-deg` is the runner own property**, added 2026-09-22:
      how far the aeroplane has turned since the stage began, signed and free
      of the wrap at north. A raw heading cannot say "turned ninety degrees
      left" - 340 is not less than 70 less eighty - so the turns lesson used
      to work only because its test never crossed north.
      **The circuit is the sixth lesson, 2026-09-23** - the first that is a
      whole flight, off a runway and back on to the same one. Eight stages:
      the take-off, the climb out, the crosswind turn, the turn on to
      downwind, the downwind leg, base and the turn on to final, final, and
      the landing. All four light aircraft and the Mosquito fly it to an
      empty debrief; let sink two hundred and fifty feet along the downwind
      leg, the debrief holds one thing and it is the circuit height.
      `lesson/flown-nm` joins `lesson/turned-deg` as a property the runner
      works out rather than the aeroplane holds: it says the downwind leg was
      long enough to be one.
      **It found two real defects.** The take-off autopilot latched "unstuck"
      on a bounce and never unlatched, so a Mosquito that skipped once at 97
      knots was steered by the airborne law - bank to hold a heading - while
      still on the runway at 115. She rolled fourteen seconds with nothing
      steering the wheels and swung forty-five degrees off the centreline.
      The wheels decide now, whatever the stage says, which is the rule the
      landing autopilot already followed. **And no take-off lesson watched the
      heading at all**, which is why that had never been caught: they watched
      the throttle, the rotation speed, the attitude and the climbing speed.
      They watch keeping straight now, at twenty degrees against the 1.1 to
      11.1 the five aeroplanes actually swing.
      **Every class has a turns lesson, 2026-09-22**, and every aeroplane in
      the roster flies its own class's: all sixteen, all seven classes, each
      through ninety degrees of left turn holding its height - the worst of
      them, the 747-400, loses 55 feet of three thousand. That is possible
      because **a turn names no speed**: `vref`, `stall` and `climb` come
      from published figures and only five of the sixteen publish a stall
      speed, while bank and height the simulation can see on anything.
      Lessons are named `<class>-<exercise>`, which is a rule and not a
      convention: the runner finds a lesson by the class its aeroplane
      declares.
      **The Mosquito class has all five lessons and the Learjet class three,
      2026-09-22** - every lesson its aeroplane has the published figures to
      support. The Mosquito publishes a rotation speed, a climbing speed and
      a stall, so it takes off, climbs, approaches, stalls and turns; the
      Learjet publishes a stall only, so it approaches, stalls and turns.
      Every aeroplane taught an exercise is flown through it: five take off,
      six approach, six stall, five climb, and all sixteen turn.
      **Climbs and descents are the seventh lesson, 2026-09-23**, and the
      first taught to the classes that publish nothing: the airliners, the
      fighters, the bomber, the flying boat and the business jet. Fourteen
      aeroplanes fly it to an empty debrief. That is possible because the
      project owner decided the same day that **a lesson's reference speed may
      be measured from the flight model** where no handbook gives one, and
      sixteen figures are now marked `from="measured"` (`ASSETS.md`).
      Still missing for those classes: take-offs, approaches, stalls and
      circuits. **The stalls lesson for them was written and withdrawn the
      same day** - a swept-wing jet asked to slow to twenty-five knots above
      its *landing* stall while flown *clean* has to stall to get there, and
      departs rather than mushing; it is a tail, with what the trace showed.
      **Two aeroplanes are taught turns alone and always will be until their
      models change**: the 747-400 and the F-22A, whose stall speeds will not
      hold still to be measured. The tests name them, with the figure they
      lack, rather than flying them against a reference speed of zero -
      which is what used to happen, silently.
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

#### The instructor demonstrates, then hands over

- [x] **The instructor demonstrates, then hands over** — done 2026-09-23.
      *Verification: for each lesson the AI pilot flies the demonstration
      within the lesson's own limits, hands the controls to the player with no
      step in any control, and takes them back on request the same way.*
      **All five exercises, and every aeroplane taught each one**, not a
      sample: turns (16 aeroplanes), the take-off (5), the approach (6), the
      climb and descent (5) and the stall (6). Each flies every stage of its
      lesson to an empty debrief, then the controls go to a pilot whose hands
      are nowhere near where the AI had them and come back three seconds
      later. **Every one of the seventeen controls is measured at both
      swaps**: 0.0083 handing over and **0.0000** taking back, where a pilot's
      hand moves 0.0207 in a frame - and the Learjet hands back from 38
      degrees nose up without moving a control.
      **The AI pilot can take off and land**, which it could not before:
      `Controller::to_ai_take_off` and `to_ai_approach` hand it a runway, and
      when the take-off or the landing is over the plain autopilot engages
      from the controls it left, so the aeroplane does not lurch as the AI
      stops taking off and starts flying.
      **The instructor asks for a speed, not for a throttle.** The stall was
      flown by reaching into the controls the autopilot returned and setting
      the throttle to 0 and then to 1 by hand - an exercise flown that way has
      no controller to hand over, which is what kept this item open. Asking
      the autopilot for a speed below the stall closes the throttle and holds
      the height by raising the nose, which is the entry; asking for one above
      the climbing speed opens it and puts the nose down, which is the
      recovery.
      **It found two real defects, and neither was visible before.** Taking
      the controls back stepped the elevator by up to 0.80 of its travel in
      one frame - forty times a hand's pace - because the autopilot clamped
      its pitch *command* to the envelope instead of the target it walks
      toward. Then the Learjet stepped the elevator 0.685 and the Mosquito
      slammed the rudder its full travel: three of the loops seeded an
      integral to cancel their own damping term so that engaging matched the
      controls handed over, and **that cancellation broke whenever the damping
      term was larger than the integral's limit** - a skidding aeroplane
      seeded a rudder integral of 9, kept 1 of it, and the rudder went to its
      stop. The loops now measure what they differ from the handed controls
      on the first step and fade that offset out over two seconds, which
      cannot break, and no loop may move a control faster than a hand at any
      time. Fixed in `src/sim/autopilot.cpp`.
      The test that was supposed to catch this flew one Cessna in one gentle
      climbing turn. It now walks **every aeroplane the data holds through
      every state one can be handed over in** - trimmed, turning, gliding,
      skidding, rolling, bunted, stalled and spiralling, 128 handovers - and
      on the old code it reports first steps of 1.80 of travel.

### Phase 6 — Client and server

#### The transport

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

#### Reliable delivery

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

#### The server

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

#### Lobby, identity and slot assignment.

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

#### The server flies every aircraft

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

#### Server-run AI aircraft

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

#### Inputs streamed with redundancy.

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

#### Client prediction and reconciliation.

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

#### Other aircraft interpolated 100 ms in the past

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

#### Collisions resolved on the server

- [ ] **Collisions resolved on the server**, mid-air and with the ground.
      *Verification: two aircraft put on a collision course collide on the
      server, and every client shows it.*

#### Every network parser fuzzed

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

#### The server's test flags

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

#### Network checks in CI

- [ ] **Network checks in CI** with injected latency, loss and jitter.
      *Verification: prediction error, correction size, interpolation error and
      the `--players` limit all checked against their stated bounds.*

#### `THREATS.md` written.

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

#### Deployment

- [ ] **Deployment** — a systemd unit and a Dockerfile under `deploy/` —
      **still to do: the Dockerfile has never been built**, because Docker
      Desktop's WSL integration is off on this machine and no daemon is
      reachable. *Verification: a server started from each accepts a client.*
      **The systemd half is done, 2026-09-22**: the unit that ships is run by
      `systemd-run`, with only its paths pointed at the build, and a client
      connects to it - "a server started by systemd admitted f21c7f56 to slot
      0". `systemd-analyze verify` passes on the unit as written. It runs as
      its own user with no privilege at all: `ProtectSystem=strict`,
      `RestrictAddressFamilies=AF_INET AF_INET6`, `SystemCallFilter` down to
      `@system-service` less `@privileged` and `@resources`, and
      `StateDirectory=` for the one file it writes, its key store. Without
      systemd the test reports itself skipped, never passed.
      **`-DGLIDESLOPE_SERVER_ONLY=ON` is what makes a server image sane.** It
      builds the simulation, the network and the server and leaves out SDL,
      Cesium Native, the shaders, the renderer and the client, and uses no
      vcpkg: libsodium, SQLite and libcurl come from the distribution. It
      configures in **6 seconds against the full build's seven minutes** and
      builds the server in about thirty.

#### `--online`

- [x] **`--online`** through a one-line `server.txt`. *Verification: a client
      started with `--online` reaches the server `server.txt` names.* Done,
      2026-09-22, for **both** clients: `glideslope_cli` and the one with the
      window. `server.txt` is one line naming a host, a port and the server's
      public key - everything a client needs and nothing that is a secret,
      since the key is the half a server prints at startup for exactly this
      purpose. Both forms `REQUIREMENTS.md` 6.6 asks for work: `--online` and
      `--server HOST PORT --server-key HEX`. Thirteen ways a line can fail to
      be one are refused and counted, a client with no `server.txt` says what
      it needs rather than connecting to nothing, and the tests name the file
      through `GLIDESLOPE_SERVER_TXT` rather than writing into the person's
      own config directory. **The session lives in `src/net/` now**, not in
      one program's `main`: it was written inside the command-line client,
      which is why the client with the window could not connect to anything
      at all until today.

#### Four machines in one sky.

- [ ] **Four machines in one sky.** *Verification: four clients on different
      operating systems and one server-run AI Cessna fly together; a fifth
      client is refused; `--players 2` refuses a third; all are visible on the
      server dashboard; and each client's prediction error at 100 ms and 200 ms
      of simulated latency is within its stated bound.*

### Phase 7 — User/AI controller swap across the network

#### Player to AI.

- [ ] **Player to AI.** *Verification: the aircraft flies on with no step, and
      the client stops predicting it and interpolates it like any other.*

#### AI to player.

- [ ] **AI to player.** *Verification: the client resumes prediction from the
      next full state, with no step.*

#### A player disconnecting

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

#### AI traffic with nobody connected.

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

#### Controller-swap continuity in the network checks.

- [ ] **Controller-swap continuity in the network checks.** *Verification:
      swaps under injected latency, loss and jitter stay within the stated
      bound.*

### Phase 8 — LLM copilot

#### Natural-language commands become flight plans

- [ ] **Natural-language commands become flight plans**, planned off the
      simulation thread and flown by the autopilot. *Verification: "take off,
      climb to 3,000 ft and orbit the CBD" produces a plan that the autopilot
      flies.*

#### An autopilot that flies an approach and lands

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

#### The copilot flies with you.

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

#### The model never drives a control surface.

- [ ] **The model never drives a control surface.** *Verification: the copilot
      can produce a flight plan and the autopilot's modes and nothing else,
      checked at configure time.*

#### Reinforcement-learning agents

- [ ] **Reinforcement-learning agents** (stretch goal). *Verification: an agent
      trained through JSBSim's gym-style wrappers lands an aircraft within
      stated limits.*

### Tails

Found while implementing something else. Added when found, not when remembered.

#### The handshake is not quite the Noise protocol it is named after.

- [ ] **The handshake is not quite the Noise protocol it is named after.**
      Found 2026-09-23. It is `Noise_IK_25519_ChaChaPoly_BLAKE2b`, and Noise's
      BLAKE2b has a 64-byte hash; this uses BLAKE2b cut to 32 bytes, and starts
      the hash with the 33-byte protocol name cut to 32 where Noise pads it to
      64. Both ends agree, so sessions work, but a client built on any standard
      Noise library would fail to complete the handshake - which is exactly
      what the transport item's "a client written from TRANSPORT.md" would
      find. It surfaced as a one-byte buffer overflow, copying the 33-byte
      name into the 32-byte hash, which MSVC's debug checks caught and
      AddressSanitizer could not; the overflow is fixed without changing a
      byte on the wire. *Verification: the handshake completes against an
      independent Noise implementation, and TRANSPORT.md's description of it
      is byte for byte what is sent.*

#### A stalls lesson for the airliners, the fighters and the bomber needs the landing configuration, which no lesson handles yet.

- [ ] **A stalls lesson for the airliners, the fighters and the bomber needs
      the landing configuration, which no lesson handles yet.** Written and
      withdrawn 2026-09-23. A lesson's `stall` is the stall speed in the
      landing configuration - flaps down, gear down - because that is what a
      reference speed is for. The exercise is flown clean, and for a light
      aeroplane the two are close enough that asking it to slow to six knots
      above the landing stall works. **For a 737-300 they are nowhere near.**
      Its landing stall is 105.7 knots and clean it gives up at about 155, so
      an entry ending twenty-five knots above the landing stall asks for a
      speed it cannot reach without stalling first - and a swept wing pushed
      past the stall does not mush like a Cessna, it departs. Traced from
      20,000 ft: it holds its height down to 159 knots at 12.6 degrees of
      alpha, and thirty seconds later it is at **52 degrees of alpha, 26
      degrees nose down and falling at 21,000 ft/min**, and nothing recovers
      it. The climbs-and-descents lessons for these classes are written and
      flying; only the stall needs this. *Verification: the approach to the
      stall is flown in the configuration its reference speed is measured in,
      and every aeroplane taught it recovers within the height the lesson
      allows.*

#### The runway lessons still fly at the model's default weight, not at their figures'.

- [ ] **The runway lessons still fly at the model's default weight, not at
      their figures'.** Found 2026-09-23, immediately after fixing it for the
      lessons flown in the air. `airborne` now loads the aeroplane as the
      loading its figures were measured at - which is what stopped the F-35B
      flying a lesson at a weight its speeds were never measured for - but the
      take-off lesson, the approach lesson, their two demonstrations and the
      circuit each build their aeroplane directly and load nothing. So a
      take-off is judged against a ground roll measured at one weight while
      flying at another, and an approach against a reference speed for a
      landing weight it is not at.
      **Tried the same day and reverted**, because it is not a one-line
      change. Loading them costs what it cost the lessons flown in the air,
      and more: the take-off lesson's climbing-speed band was a flat 45 to 120
      knots - the span of four aeroplanes, and so not a band at all - which is
      fixed and kept, but **the circuit comes apart**. A heavier Cessna climbs
      more slowly, so it is further from the runway at every height, the
      pattern no longer sits where the legs expect, and two of its bands break
      at once: the nose wanders past the twenty-five degrees the climb-out
      allows, and the downwind leg comes out shorter than the mile the lesson
      asks for. The circuit's geometry has to be re-tuned at the new weight in
      the same change. *Verification: every lesson flies its aeroplane at the
      weight the figures it is judged against were measured at, the circuit
      still flies its pattern, and the bands are re-measured there.*

#### The PA-28 loses five times the height the other light aircraft lose entering a stall.

- [ ] **The PA-28 loses five times the height the other light aircraft lose
      entering a stall.** Found 2026-09-23 while flying the lessons at the
      weight each aeroplane's own figures are measured at. Throttle closed and
      the height held as the speed comes back, all four at maximum gross: the
      Cessna 172P sinks 61 feet, the 182S 61, the J-3 Cub 28 - and the PA-28
      **321**, at the same 2,400 lb as the 172. It is not the flying: the same
      autopilot flies all four. Either the model's wing gives up earlier than
      it should or it needs more pitch than the loop is allowed, and the
      light-aircraft stalls lesson carries a band four hundred feet wide on
      one aeroplane's account. *Verification: the four light aircraft enter a
      stall by the book within the same band, and the lesson's band is set
      from what they all do rather than from the worst of them.*

#### The Learjet ends its landing roll nose down through the runway.

- [ ] **The Learjet ends its landing roll nose down through the runway.**
      Found 2026-09-23 while demonstrating an approach: the Learjet flies the
      whole approach lesson and stops, and at the end of the rollout it is at
      **37 degrees nose down and a foot below the ground**. The Cessna, the
      Piper and the Mosquito all finish level on their wheels. Nothing in the
      approach itself is wrong - every stage is flown within the lesson's
      limits - so this is the aeroplane settling onto its gear at the end,
      not the flying. *Verification: every aeroplane the data holds, landed
      by the AI pilot, is on its wheels at the end of its rollout - upright,
      at an attitude its gear allows, and not below the surface.*

#### A `--terrain ion` run can hang for ever, past its own timeout.

- [ ] **A `--terrain ion` run can hang for ever, past its own timeout.** Found
      2026-09-22: five `glideslope --terrain ion` processes were still alive
      after **a day and a half**, each wrapped in a `timeout -s TERM` of four
      to eight minutes that had long since fired. They hold the GPU and the
      Cesium cache, which is what the "database is locked" flake recorded
      above is: a later run meets a process from a previous day. The TERM is
      sent and ignored - something in the shutdown path waits on Cesium's
      network or its cache without a deadline. *Verification: a terrain run
      given a timeout is gone when the timeout has passed, and a run killed
      part-way leaves no lock behind.*

#### A published stall speed for the F-15C, which would give the fighter class its approach and stall lessons.

- [ ] **A published stall speed for the F-15C, which would give the fighter
      class its approach and stall lessons.** Found 2026-09-22 while looking
      for why ten aeroplanes can have only a turns lesson. The F-15's own
      flight manual does publish one: T.O. 1F-15A-1, Appendix A, the Stall
      Speeds charts, whose worked sample gives **107 knots with flaps and
      gear down at 40,000 lb, 15 degrees of bank, 10,000 feet and maximum
      thrust**. That is not directly what this project's `stall_speed`
      figures hold - those are power off and wings level - so taking it needs
      either a figure that carries its bank and thrust as conditions, or the
      1g power-off number read off the same chart rather than from its
      sample. The manual is in the Internet Archive copy `ASSETS.md` already
      records for the F-15C's checklist. *Verification: the F-15C stalls near
      its published speed in the configuration the figure names, and its class
      gains the approach and stall lessons.*

#### The autopilot banks to its limit even when the aeroplane cannot sustain the turn.

- [ ] **The autopilot banks to its limit even when the aeroplane cannot
      sustain the turn.** Found 2026-09-22 by probing every class through a
      ninety-degree turn. At 3,000 feet the Cessna 172P holds its height to
      within fifteen feet and its speed exactly, at 85 knots or 100. At
      10,000 feet - near its ceiling - the same turn costs it 2,076 feet and
      its speed decays to 46 knots, at either speed, because a thirty-degree
      bank needs more lift than it has power for and nothing reduces the
      bank. Every other class holds its height within sixteen feet at 10,000
      feet, so this is an aeroplane near its ceiling rather than a class of
      aeroplane. *Verification: a light aeroplane turned through ninety
      degrees near its ceiling holds its height within the same band it holds
      at three thousand feet.*

#### Cesium ion on Windows, where a body arrives compressed unasked.

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

#### A livery on the aeroplane, and its control surfaces moving.

- [ ] **A livery on the aeroplane, and its control surfaces moving.** A model
      ships no texture, so a surface takes the flat diffuse colour of its
      AC3D or 3D Studio material, and no control surface, propeller or
      undercarriage moves: they are welded where the model has them, gear
      down. *(Found 2026-09-21 drawing the aeroplane.)* *Verification: a
      livery is on the aeroplane in a shot, and a shot with the stick over
      shows the ailerons moved.*

#### The aeroplane is lit by a light baked into its mesh.

- [ ] **The aeroplane is lit by a light baked into its mesh.** The mesh shader
      has no normals, so the light is worked into each vertex's colour and
      the mesh made again when the aeroplane has banked far enough to see -
      five degrees. A normal in the vertex would light it on the GPU and
      make that unnecessary, at the cost of a normal on every terrain vertex
      too. *(Found 2026-09-21 drawing the aeroplane.)* *Verification: the
      aeroplane's lighting follows it through a roll with no mesh remade.*

#### A checklist item's band is not held against what the aeroplane can reach.

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

#### Nothing here compiles first-party code under clang.

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

#### One download that fails once reds the tree.

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

#### Tests that run at once share one Cesium cache.

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

#### Runways on the DEM.

- [ ] **Runways on the DEM.** *(Found writing `REQUIREMENTS.md`; open there.)*
      The terrain data shows a runway with bumps it does not have.
      *Verification: decided in `REQUIREMENTS.md`, and if runways are smoothed,
      an aircraft rolls the length of reference runways with no bump beyond a
      stated bound.*

#### Free buildings for the default scenery.

- [ ] **Free buildings for the default scenery.** *(Found writing
      `REQUIREMENTS.md`; open there.)* *Verification: a source is decided and
      recorded in `ASSETS.md`, and a `--shot` of a city shows its buildings.*

#### Signed and notarised macOS builds.

- [ ] **Signed and notarised macOS builds.** *(Found planning Phase 0.)*
      *Verification: a downloaded macOS package opens on a Mac with no
      Gatekeeper warning.*

#### One Linux download for every distribution

- [ ] **One Linux download for every distribution** — an AppImage or Flatpak.
      *(Found planning Phase 0.)* *Verification: one file runs on a fresh
      Ubuntu and a fresh Rocky install.*

#### A hosted public server

- [ ] **A hosted public server**, on the project owner's AWS, if one is
      needed. *(Found planning Phase 6.)* *Verification: `server.txt` names a
      running server that a client reaches with `--online`.*

#### Terrain over the whole Earth, streamed as an aircraft flies.

- [ ] **Terrain over the whole Earth, streamed as an aircraft flies.** *(Found
      adding jets to the roster: a region three degrees across is crossed in
      minutes at Mach 0.8, and "anywhere on Earth" is a `CORE` feature with no
      item.)* *Verification: a flight from Sydney to Melbourne at cruise draws
      terrain under it the whole way, and the tiles in memory stay under a
      stated bound.*

#### Thermals from the ground beneath them, and lee waves trapped under a stable layer.

- [ ] **Thermals from the ground beneath them, and lee waves trapped under a
      stable layer.** *(Found building thermals and mountain waves.)* The
      thermals rise alike over the sea, a lake and a sunlit slope, from one
      station's report; the waves are linear theory in one wind and one
      stability, so they rise away rather than lie trapped in a ridge's lee,
      and there is no rotor. *Verification: on a convective day no thermal
      rises over open water; and where the Scorer parameter falls with height,
      the lee of a ridge holds waves at the wavelength two-layer theory
      gives.*

#### Weather seen as it is.

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

#### Every aircraft's airframe meets the ground with its wheels up.

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

#### Propeller and mixture levers for the pilot.

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

#### Four visual models are drawn sunk into the ground.

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

#### The F-15C's airframe slides on the wrong friction.

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

#### The F-35A becomes the F-35B.

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

#### The F-35B cannot hover, land vertically or take off short.

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

#### The Cesium cache still locks when the rendering tests run together.

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

#### The reliable layer believes an acknowledgement it is told.

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

#### No message rejects a number that is not one.

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
