### The F-15C's speedbrake draws drag, and stays in past 15 degrees of alpha, 2026-10-07 — tail done

**What was missing.** Her model's speedbrake channel moved
`fcs/speedbrake-pos-norm`, but no coefficient read it: the lever slowed
nothing, so her catalogue did not say `speedbrakes` and the HUD hid it.

**What changed** (`tools/make_f15c.py`, `with_speedbrake`, and the model it
writes):
- **Her drag is the F-15's own**: `CDsb`, a drag coefficient of 0.0436 on
  the wing's area times the surface's position - CXDSPD, "delta CD due to
  speedbrake", of the 1988 F-15 aerodynamic database, as McDonnell's AFIT
  thesis lists it (Investigation of the High Angle of Attack Dynamics of
  the F-15B Using Bifurcation Analysis, AFIT/GAE/ENY/90D-16, December 1990,
  page 81). The same thesis gives the surface as 31.5 sq ft, 45 degrees up.
- **It stays in past 15 degrees of alpha**, as the same page says it will
  not deploy beyond: the channel's command passes a switch
  (`fcs/speedbrake-allowed-norm`) that is the lever only at 15 degrees or
  less, so pulled past it the surface runs in at its own rate.
- Her catalogue entry says `speedbrakes`; her checklist's note that it drew
  no drag is gone. Nine aircraft now have them.

**Verification** (linux-release, locally):
- `every_aircraft_with_speedbrakes_slows_on_its_lever_held_level_and_every_other_ignores_it`:
  the F-15C 303.0 kt stowed, 271.6 out, 31.4 kt slower in ten seconds (bound
  2); the other fifteen as before. **Seen to fail** with the coefficient
  zeroed in the build's data: "f15c has speedbrakes, and with them out is
  0.000000 kt slower".
- `the_f15cs_speedbrake_stays_in_past_fifteen_degrees_of_alpha_with_the_lever_out`
  (new): level at 300 kt, 2.4 degrees, the surface fully out; pulled until
  three seconds past 15 degrees (31.9), in, the lever still out. **Seen to
  fail** with the limit raised to 90 in the build's data: out at 31.3.
- `a_client_working_the_speedbrakes_is_predicted_as_the_server_flies_them_in_every_aircraft_with_them`:
  nine now (asserted), the F-15C's worst correction 0.098 m worked against
  0.096 stowed (bound 0.120).
- The committed-model check, and every test matching f15c, land, lesson,
  figure, selftest, stop, flare, touch, speedbrake, checklist, reject or
  accelerate (114): green. The AI opens her speedbrake on the landing roll
  and a rejected take-off, as every jet's.
- **The selftest hash does not move** (`d36123c1eecc3e23`, linux-release):
  it flies the Cessna.
