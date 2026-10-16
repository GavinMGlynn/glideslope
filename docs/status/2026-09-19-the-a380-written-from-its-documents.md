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
