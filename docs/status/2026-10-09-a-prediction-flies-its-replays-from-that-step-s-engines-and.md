### A prediction flies its replays from that step's engines and actuators, and keeps a jet's spools, 2026-10-09 — narrows the Windows engine-stop tail, CI owed

**What is not shown yet, first.** That this is what failed on Windows CI
(medians of 0.4 to 1.5 m after the stop; 0.18 m/s in speed on windows-clang)
is not proven: the Windows copy passed this test before the fix as after,
and no Windows-only cause was found. What was found is a fault on every
platform whose size grows with the steps each word replays and the words a
frame takes - more on a slower runner. A month of CI runs is still owed.

**The cause found.** A word from the server puts back the motion alone, and
the steps since are flown again. The engines' spin - a propeller's rpm, a
turbine's N1 and N2 - is not in the motion, and was left where the newest
step had it, so each replay wound a stopping engine down again. Read from the
client's track on the Windows copy (release): the server's Cessna propeller
took six seconds to stop (2135 rpm at 20.1 s, 0 at 26.1); the client's was
at 926 rpm 0.2 s after it heard. For those seconds the client flew with
less windmilling drag than the server and was put right by it at every
word. **And a second fault beside it**: `Aircraft::set_motion` runs JSBSim
without advancing the clock, which JSBSim takes for a trim, and the step
after a trim puts a running turbine's spools where its throttle settles
them - so every correction of a jet moved its N1.

**The fix** (`src/sim/`): `Aircraft::replay_state`/`set_replay_state`
(`ReplayState`): each thruster's rpm; a turbine's N1 and N2; a turbine's or
turboprop's phase, so a replay spanning a start or a stop begins from the
first held step's; and every readable and writable flight-control value but
the commands, with the gear's position - which holds each kinematic
actuator's position (flaps, speedbrakes, gear), as JSBSim reads it back from
its output at every step. `Prediction` keeps each step's as it began and
puts it back before flying the held steps again (`reconcile` from motion,
and `adopt`); `set_motion` also puts each turbine and turboprop back in its
phase. **Not kept**, held by JSBSim in members with no setter: a lag
filter's, an integrator's or an actuator's own history (lag, rate limit,
hysteresis) - the Short S.23's flaps, wound by an integrator, are 0.13 of
their travel apart in the test below, which names them; a turbine's fuel
flow, EGT, oil temperature and EPR and a piston's temperatures; the fuel.
Nothing on the wire changed. **The selftest's hash did not move**:
be036519d2c19ea0 before and after (linux-release) - it flies no prediction.

**Verification.**
`an_engine_stopped_on_the_server_winds_down_in_a_prediction_put_right_as_it_does_flown_once_in_every_aircraft`
(unit): every engine of every aircraft stopped in turn - 35 engines in 16
aircraft, counted - two of each aircraft, the engine stopped at one step, one
flown once and one predicted and put right ten times a second by words
agreeing with it exactly, the throttle moving, the flaps running out, the
speedbrake lever out for a second and the gear going down. The engines'
spin must be apart by no more than a tenth of how far it wound, each surface
within a hundredth of its travel. Measured: jets 0 to 2.3 points of a spool
against 63 wound, propellers 1.5 to 39 rpm against 600 to 1050, surfaces
0.0000. **Seen to fail**: without the spin put back, 737-300 23.1 points
over a 6.3 bound (the propellers 600 to 1500 rpm; the Mosquito's
windmilling propeller holds either way, 1.6 rpm, named in the test);
without the turbine's phase kept, 7.6 points; without the flight controls,
the 737's gear 0.78 of its travel; each reverted. On the Windows copy
(release), the engine-stop test with and without the spin put back, two
runs each: speed error over the six seconds after the stop 0.076 and 0.086
m/s without, 0.015 and 0.007 with; the median distance after 0.046 and
0.020 m without, 0.013 and 0.009 with. Ten runs in a row there passed.

**The speedbrakes, which this showed.** The jets' worst correction at
200 ms, lever stowed, fell from 2 to 14 cm to 0.3 to 1.7 cm, and the F-22's
lever was found to cost her 11 cm (0.127 m worked, 0.017 stowed; before,
0.137 both, under the spools' error): her speedbrake's kinematic actuator
was moved on again by every replay. With the flight controls put back, the
lever costs nothing in any of the nine (F-22 0.0160 m worked and stowed),
and
`a_client_working_the_speedbrakes_is_predicted_as_the_server_flies_them_in_every_aircraft_with_them`
holds its quarter-over bound unchanged; without them, F-22 0.127 over 0.031,
red, reverted.
