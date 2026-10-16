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
