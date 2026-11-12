### Every aircraft trims in the session's air - 2026-11-12

**What it does not do, first.** Her trim still runs in still air: a steady
wind changes no force on her, so she is trimmed without it and then given
it (below). Turbulence and gusts start on her first step, as before.

**What was wrong.** `Aircraft::initialize` trimmed her in JSBSim's standard
atmosphere and still air; the session's temperature, pressure and wind were
first applied in `step()`. So she started at the calibrated airspeed she was
given in air of 15 degrees and 1013 hPa, and on her first step the air
became the METAR's: her true airspeed, and so her trim, were wrong, and a
wind arrived as a gust - her speed over the ground was her airspeed, and the
wind took its own share off it at once. The C182 left alone on final in a
calm METAR of 20 degrees and 1015 hPa was 6 degrees off her heading thirty
seconds on; in a 15 kt crosswind, a C172P started at 100 kt read 101.0 kt
and turned 4.7 degrees in twenty seconds, a Cub 12.2.

**What now works.**
- Her temperature and pressure are set before her airspeed and her trim.
- Once trimmed, her velocity over the ground is her airspeed's plus the
  wind's, and the wind is set with it (`Aircraft::moved_with_the_wind`), so
  she starts at the airspeed and heading she was given and drifts with the
  air. An aircraft started at rest is left at rest.
- **Not through JSBSim's initial conditions**: `FGInitialCondition` takes a
  wind as airspeed less ground velocity (`GetWindNEDFpsIC`), and
  `FGFDMExec::Initialize` hands that to `FGWinds`, which `FGAuxiliary`
  subtracts from the ground velocity - the opposite sense. A wind given
  there was felt twice: tried first, a C172P started at 60 kt in 10 kt
  read 40 kt with 85 degrees of sideslip, her trim failed, and JSBSim
  printed "Cannot modify angle 'alpha'" about 45,000 times in the suite.
  Main prints none; this version prints none. Searched first: JSBSim's
  issues and its current `FGInitialCondition.cpp` (unchanged on master)
  say nothing of the sense either way.

**The wrong order cannot pass unseen.** Weather given an aircraft after a
start in the air and before her first step is refused by that step
(`std::logic_error`), unless she was started again in it; after her first
step, after `set_motion`, or after a start at rest it is a change of air.
`an_aircraft_given_her_weather_after_she_is_started_in_the_air_is_refused_her_next_step`
builds all five orders; seen red with the refusal taken out.

**Every `Aircraft::initialize` caller, and its order** (`grep -rn
"initialize(" src`):

| Caller | Order |
|---|---|
| `frontend/client/flight.cpp` (her own start, `--weather`) | weather, then start - **reordered**; it was start, then weather |
| `frontend/client/flight.cpp:539` (heard air, online) | not a start: the server's air as it changes, after she flies |
| `frontend/cli/main.cpp` `Predicting` (connected: the server's motion) | weather, then start, then `set_motion` - **reordered** |
| `frontend/cli/main.cpp` `land` | weather, then start |
| `frontend/cli/main.cpp` (`fly-plan`) | no weather |
| `frontend/cli/fly_copilot.cpp` | no weather |
| `frontend/server/main.cpp`: a player's start, AI from the planner, the circuit's starts | weather (when the session has it), then start |
| `frontend/server/main.cpp`: on final, flown again after a wreck | started again, her weather already hers |
| `frontend/server/main.cpp` `Fleet::fly_in` (the session's weather first had) | weather, then each aircraft started in the air and not yet flown started again in it - **added**: a plan-file AI starting in the air was trimmed in standard air |
| `sim/orbit_trial.cpp`, its three windy trials | weather, then start - **reordered** |
| `sim/orbit_trial.cpp`, its glide and climb trials | no weather |
| `sim/takeoff_trial.cpp` | weather, then start |
| `sim/figures.cpp` (every figure's flight) | start on the runway, then weather: a start at rest |
| `sim/lander.cpp` (spool-up) | no weather |
| `sim/selftest.cpp` | no weather |
| the tests | weather, then start: 22 tests in eight files gave it after, and the refusal stopped each (16 call sites reordered; `airborne` in test_lesson.cpp and `at` in test_learnt.cpp now take the air to start her in; the client prediction test's clock is the prediction made once she is started). Starts at rest that are given weather after (the figures', the braking and standing tests) are left: no trim is flown |

**What else moved, and why.**
- **Two autopilot figures in moderate turbulence**, once those tests start
  her in their air (15 degrees, Q1013 - a hair off the standard 1013.25):
  the altitude capture from 4,000 to 4,500 ft settles in 70.8 s against a
  bound of 70, now 75; and the speed hold's measured table is re-measured -
  the 172P's throttle reversals 221 to 255, past its 10% (243), and each
  aircraft's RMS within 0.18 kt of 2026-10-10's. The orbit trials and the
  CLI's flights, reordered too, moved no figure a test holds.
- **The gate's wind reading.** Placed in her wind, she is now judged in it
  at once: the corner tests of every learnt gate put her at exactly its most
  headwind (15 kt), and its wind estimate - ground velocity less the whole
  true airspeed - read a hair over, since on a three-degree path part of
  her airspeed is downwards (about 0.1 kt at 57 kt; the gate's edge is
  judged to 0.05). The gate now takes the airspeed's level share. The
  policy's own reading is untouched, so no policy needed retraining:
  every learnt landing's tests pass on the policies as committed. Twelve
  corner tests were red on this, four aircraft by headwind and both
  quarters.
- **Two gust seeds.** The 737-300 on seed 40 touches at 362 ft/min and the
  A380 on seed 34 at 350 (inside 360, not by 25); her start moved, so each
  flight's gusts meet her elsewhere. The other 39 seeds of each still land;
  both are named on Linux in `named_on_a_seed`, as single-seed names are
  (the owner's ruling of 2026-11-04). Windows's and macOS's names are as
  CI finds them.
- **The first attempt's 81 reds**, wind given through the initial
  conditions, were that double-counted wind; none of them is red now.
- `the_stations_flown_are_kept_..._204` and `_404`: green on this branch,
  as on main since the kept-stations cases fly 40 s.

**Verification.**
- `every_aircraft_trimmed_in_a_calm_metar_away_from_the_standard_atmosphere_holds_her_heading_and_track_as_in_no_weather`
  walks all 15 landplanes (the S.23 named, started on the water nowhere
  here): in still air of 30 degrees and 1030 hPa her heading and track move
  as in no weather within half a degree, and in 15 kt across she starts at
  her given airspeed within half a knot with her heading as in no weather.
  Seen red on main (C172P, C182, Cub and PA-28 up to 1.29 degrees apart in
  the warm air) and with the wind step taken out (C172P 4.74 degrees and
  101.0 kt, the Cub 12.20 and 61.6 kt); the most apart now is 0.01.
- `glideslope_cli selftest`: hash 197366fd3fe6a7dc, unmoved - it flies no
  weather.
- The whole linux-release suite (1,637 tests, live ones left out): 14 red
  with the start alone - the twelve gate corners and the two gust seeds
  above - and those 14 plus the new test green after the gate's reading
  and the names; then all 116 gate, learnt-landing and on-final tests
  again, green.
- With the refusal and every caller reordered: the whole linux-release
  suite, 1,638 tests, green; the selftest hash unmoved.
- Reproduced first on main, with the new test: in 30 degrees and 1030 hPa
  the C172P, C182, Cub and PA-28 turned 1.14, 0.96, 0.99 and 1.28
  degrees in twenty seconds that in no weather they held.
