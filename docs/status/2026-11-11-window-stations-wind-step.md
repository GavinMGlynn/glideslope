### The thermals step nothing at the top of their layer, and the stations' flight flies one hour's air - 2026-11-11

**What it does not do, first.** The thermals above 0.9 of the layer's depth
are no longer Allen's: his ring of sinking air and the sink's share are held
at their 0.9 values, and the whole fades to nothing over the top tenth. Below
0.9 of the way up nothing changed.

**The red tree.** `a_flight_on_its_own_flies_the_stations_around_her_..._with_no_step_in_the_wind`
(#361) failed on #363's full run (Linux debug, Windows debug twice):
"the wind changed by 0.14130 m/s in a step, more than 0.05", at step 53804,
with YSWS's share 1.000. No refresh, no blend and no variable wind: a probe
of the vertical wind showed YSWS's thermals going from -0.14129 m/s to
0.00000 as her height above the ground (679.750 m) crossed the top of the
convective layer (679.749 m). Allen's mean updraught at the top is not
nought but -0.1 w*, w* (z/zi)^(1/3) (1 - 1.1 z/zi), and `thermal_updraught`
returned nothing at and above it: a step in the air. Allen's ring and the
sink's share, ending at 0.9 of the way up at all of themselves, were two
more, about 0.017 m/s.

**Why it passed on #361 and failed now.** The METARs and the forecast were
recorded, but the flight asks for the forecast at the hour it flies, UTC,
and the test answered every hour of the recorded day: the temperatures
aloft at that hour set the convective layer's depth (`lift_of`). #363's run
flew at 13Z, whose layer over YSWS is 679.7 m deep and was crossed;
14Z's, run here, passed. The test sometimes tested its rule.

**Fixed, both.**
- `world::thermal_updraught` fades to nothing over the layer's top tenth;
  `environment_sink`'s and `allen_updraught`'s 0.9 terms are held above 0.9.
- `window_stations.cmake` answers every hour of three days with the
  recording's 13Z (`HOUR`), so she flies the same air at any hour, through
  the layer's top.

**Verification.**
- `up_through_the_top_of_the_convective_layer_the_thermals_air_steps_nothing`
  walks every point 20 m apart over four cells, at three moments of a
  thermal's life (6,627 walks of 280 one-metre steps, from 0.85 to 1.05 of
  the depth, stated and asserted): seen failing at 0.723 m/s in a metre
  before the fix; 0.0059 after, and 0.00059 at a tenth of a metre - no step
  left. Bound 0.01.
- The stations' flight with `HOUR=13` reproduced CI's failure exactly
  (0.14130 m/s at step 53804) before the fix, and passes after: at most
  0.00763 m/s in a step, at step 149, in YSSY's air alone.
- The 51 tests matching thermal, lift, convect, updraft, soar, selftest,
  stations, ridge, glider, allen or hash pass on linux-debug.
