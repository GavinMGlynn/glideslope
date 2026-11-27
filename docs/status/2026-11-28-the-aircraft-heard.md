### The aircraft heard, 2026-11-28

Phase 12c, "The aircraft heard".

**What is still not true, first.**
- **Every sound is a synthesised placeholder.** Nothing is recorded; each
  aircraft's set (`assets/sound/<id>.sound`) is stand-in figures but her
  engines' cylinders (docs/ASSETS.md). Phase 15's "Every aircraft heard as
  herself" brings recordings.
- **Her stall warning sounds by angle of attack, at a stand-in angle**: 14
  degrees for the light aircraft and the propeller types, 12 or 13 for the
  airliners, business jet and bomber, 25 for the fighters. None is from her
  manual or measured from her flight model.
- **Another aircraft's engine is heard at a fixed drive** - about her
  cruise RPM or 85% N1, her set's `heard-as-other` - because a state update
  carries nothing of her engine. No Doppler.
- **A window client's sound is heard; nothing listens to it in a test.**
  The tests drive the mix and the device on SDL's dummy driver directly;
  the window client's own use of them (client/sound.cpp) is built and
  run by every window-client test, on the dummy driver, but its mix is not
  measured there.

## What changed

- **`src/audio/`, its own library** (`glideslope_audio`, linked by the
  window client and the unit tests; the simulation links none of it, and
  cmake/Layering.cmake still keeps SDL's audio out of `src/sim/`):
  - `sound_set` reads an aircraft's sound set: her engine - a piston's
    cylinders, whose firing rate is her note, or a turbine's fan speed at
    100% N1 and its blades - her wind, gear motor (only where her gear
    retracts), flap motor (only where her flap lever works flaps),
    touchdown, stall warning and the drive she is heard at as another
    aircraft.
  - `mixer` synthesises the stereo mix, 48 kHz float: each engine's note at
    her set's fundamental for the RPM or N1 her flight model reports, its
    loudness by her throttle and nothing while she is stopped; the wind, a
    low-passed noise louder and brighter with airspeed; the gear and flap
    motors while they move; a thump from the step she meets the surface,
    louder the faster she sinks; the stall horn while her alpha is at or
    past her set's, off the ground; and every other aircraft by her set,
    panned by where she is from the listener (constant power) and fading
    as 50 m over the distance past 50 m, silent past 20 km.
  - `heard` reads a sim::Aircraft into what the mix is told.
  - `device` plays the mix on SDL3's default playback device through an
    audio stream, made by **simulated time flown**, not by the device's
    asking; at most a quarter second queues. Its `tap` is the test hook:
    every frame mixed, handed back.
- **The window client is heard** (`client/sound.cpp`), unless
  `--no-sound`, and never headless: each pass mixes the ticks flown since
  the last. Where there is no audio device it says so and flies on silent.
- **Every ctest test runs with `SDL_AUDIO_DRIVER=dummy`**
  (tests/CMakeLists.txt), so no test is heard on the machine's speakers.
- 16 sound sets, one per aircraft.

## Verification

Seven unit tests, each on SDL's dummy audio driver, asked for by name and
checked to be the one opened, each walking every one of the sixteen
aircraft and asserting how many it walked:

- `every_aircraft_in_the_catalogue_has_a_sound_set_that_matches_her_flight_model`:
  16 sets for 16 aircraft, nothing else in data/sound; gear motor exactly
  where her gear retracts, flap motor exactly where her flap lever works,
  piston exactly where her engines report RPM.
- `a_sound_set_with_a_line_wrong_or_missing_is_refused`: 8 wrong sets.
- `every_aircrafts_engine_heard_on_the_dummy_driver_rises_in_pitch_with_her_rpm`:
  flown 15 s on levers shut and 15 s full in JSBSim, the mix's strongest
  frequency over 1.37 s (FFT) is her set's fundamental at her flight
  model's RPM or N1 within 2% and a bin, and higher at full power.
- `every_aircraft_heard_with_her_engines_stopped_on_the_ground_is_silent`:
  below -80 dBFS stopped on the ground (a seaplane afloat), above -40
  idling.
- `the_wind_heard_grows_louder_with_airspeed_in_every_aircraft`: 31
  speeds each, 0 to 600 kt; nothing at 0, louder at each.
- `every_event_is_heard_from_the_step_it_happens_in_every_aircraft`: gear,
  flaps, touchdown and stall warning, each two flights the same but for the
  event - the same sample for sample before it, and the event at least a
  tenth of the rest of the mix over its first quarter second. Gear and flap
  motors of aircraft without them are counted and named as not built.
- `another_aircraft_is_heard_louder_nearer_and_on_the_side_she_is_on`: 16
  aircraft at 7 places and 11 distances.

Locally, linux-release: the engine test heard every aircraft at her drive's
fundamental to 0.1 Hz - the C172P 23.5 Hz shut and 91.0 Hz full, the
737-300 25.9 and 86.3 (N1), the Mosquito 179.9 and 300.2 - in 4.9 s for
all sixteen; stopped on the ground the loudest was the Mosquito's 5e-6 of
full scale, idling the quietest 5.2e-2; each event was 0.10 (the A380's
flaps) to 0.57 (the Cub's touchdown) of the rest of the mix.

**Each seen to fail**, a deliberate bug introduced, the test watched go
red, and the bug reverted: the parser taking an unknown line (7 of 8
refused); a piston's pitch ignoring her RPM; a stopped engine still
sounding; the wind at one loudness whatever the speed; the touchdown never
thumping; the stall horn a hundredth as loud; another aircraft's pan
swapped left for right; and another aircraft not fading with distance.
The sound-set test went red for real first: the F-22A's set gave a flap
motor her flight model's lever does not work, and the Mosquito's none
where hers does.
