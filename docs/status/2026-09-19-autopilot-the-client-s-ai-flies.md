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
