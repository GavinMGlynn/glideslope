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
