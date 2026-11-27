### The aircraft heard, 2026-11-28

Phase 12c, "The aircraft heard".

**What is still not true, first.**
- **Every sound is a synthesised placeholder.** Nothing is recorded; each
  aircraft's set (`assets/sound/<id>.sound`) is stand-in figures but her
  engines' cylinders (docs/ASSETS.md). Phase 15's "Every aircraft heard as
  herself" brings recordings.
- **Her stall warning's angle is her flight model's, at the certification
  rule's margin, not her own system's.** It is read from her lift curve
  (below); no aircraft's warning vane or horn setting is modelled. The
  J-3 Cub, which has no stall warning, is given one like the rest: the
  rule's 5 knots over her 38-knot stall puts it at 5.9 degrees.
- **Another aircraft's engine is heard at a fixed drive** - about her
  cruise RPM or 85% N1, her set's `heard-as-other` - because a state update
  carries nothing of her engine. No Doppler.
- **A flight begun again in the window client starts a new mix** (another
  Flight, or its clock gone back); no test drives that.
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
    touchdown, stall horn and the drive she is heard at as another
    aircraft. **Her stall warning's angle is not in her file**: loaded to
    be flown (`own_sound_set`), her set reads it from her flight model
    (`sim/lift_peak`): her lift coefficient, worked out by JSBSim with no
    time passing at every quarter degree of alpha her tables hold, clean and
    flaps fully down, and the first angle where it reaches its peak over
    her warning margin squared - her warning speed over her stall speed by
    sim::stall_warning_kts (the rule's 5 knots or 5%, or her figures' own:
    the Mosquito's 10, the F-22A's 12), 1.05 where she publishes no stall
    speed. Between the two by her flaps. A start may now be given an
    angle of attack (`InitialConditions::alpha_deg`), for this alone.
  - `mixer` synthesises the stereo mix, 48 kHz float: each engine's note at
    her set's fundamental for the RPM or N1 her flight model reports, its
    loudness by her throttle and nothing while she is stopped; the wind, a
    low-passed noise louder and brighter with airspeed; the gear and flap
    motors while they move; a thump from the step she meets the surface,
    louder the faster she sinks, once she has been off it a quarter second
    (a wheel chattering on the rollout is not a landing; a longer bounce
    is); the stall horn while her alpha is at or past her warning's for her
    flaps, off the ground; and every other aircraft by her set,
    panned by where she is from the listener (constant power) and fading
    as 50 m over the distance past 50 m, silent past 20 km.
  - `heard` reads a sim::Aircraft into what the mix is told.
  - `device` plays the mix on SDL3's default playback device through an
    audio stream, made by **simulated time flown**, not by the device's
    asking; at most a quarter second queues, what is past it trimmed off
    with a 5 ms fade out and the next given faded in. Its `tap` is the
    test hook: every frame mixed, handed back.
- **The window client is heard** (`client/sound.cpp`), unless
  `--no-sound`, and never headless: each pass mixes the ticks flown since
  the last. Where there is no audio device it says so and flies on silent.
- **`audio/` is a layer the simulation may not include**
  (cmake/Layering.cmake), walked by tests/cmake/layering_includes.cmake;
  an include of `audio/mixer.hpp` put in src/sim/lift_peak.cpp was seen
  refused at configure, and taken out.
- **Every ctest test runs with `SDL_AUDIO_DRIVER=dummy`**
  (tests/CMakeLists.txt), so no test is heard on the machine's speakers.
- 16 sound sets, one per aircraft.

## Verification

Twelve unit tests, each on SDL's dummy audio driver where it plays, asked for by name and
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
- `every_aircrafts_stall_warning_sounds_at_the_angle_her_own_lift_curve_gives`:
  32 lift curves, each peaking inside her tables (the C172P's CLmax 1.66 at
  16 degrees clean, warned from 13.0; the F-22A's 2.31 at 45, warned from
  36.6), her set carrying the angles, and the horn sounding at each and at
  half flap half way, and not 0.1 degree below.
- `a_c172p_at_2700_rpm_is_heard_at_90_hz_and_a_737_300_at_full_n1_at_86_hz`:
  figures worked by hand - 2,700 / 60 x 4 / 2 = 90 Hz, and 5,179 / 60 =
  86.32 Hz - heard at 89.99 and 86.31.
- `a_bounce_off_the_surface_is_heard_again_only_after_a_quarter_second_off_it`:
  6 bounces, 1 to 240 steps off.
- `every_aircrafts_landing_in_her_flight_model_is_heard_from_the_step_she_touches`:
  every aircraft set down in JSBSim from 30 ft (the S.23 on water), flown
  twice, with and without her touchdown's gain: the same to the sample
  until the step her first wheel or hull meets the surface, the thump 0.23
  (the S.23's) to 1.35 of the rest after it; the F-22A touches six times,
  five of them after a quarter second off, and five thumps are heard.
- `a_mix_made_faster_than_it_is_played_is_trimmed_to_what_fits_not_dropped`.

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
For the second round: the warning ignoring her flaps; a piston firing
every revolution (the pinned figures); the quarter second off taken away,
and the touchdown never heard (the bounce and landing tests, each); her
wheels' weight not read (the landing test); and a chunk over the cap
dropped whole (the trim test). The sound-set test went red for real first: the F-22A's set gave a flap
motor her flight model's lever does not work, and the Mosquito's none
where hers does.
