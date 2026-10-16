### The Learjet 35A written from its flight manual, 2026-09-19 — item done (CI run 35442214130)

Phase 5's "a Learjet 35A flight model, written here from published data":
the Gates Learjet 35A, which JSBSim does not have, written by
`tools/make_learjet35a.py` from its FAA-approved flight manual (FM-108), its
type certificate, NASA's full-scale wind tunnel test and flight
identification of its forebear, the Learjet 23, and Gates Learjet's drag
analysis of the Learjet 25, lands inside its tolerances on seven figures -
the airliners' four and three of its flight manual's stall speeds.

| Learjet 35A | Measured | Published |
| --- | --- | --- |
| Take-off field length, 18,300 lb, flaps 8, sea level, ISA | 5,150 ft | 5,300 ± 10%, the AFM's chart |
| Engine-out climb, V2, flaps 8 | 7.6% | at least 2.4 (AFM 5-6, FAR 25.121(b)) |
| Level at 41,000 ft, full throttle, 15,300 lb | Mach 0.82 | 0.81 ± 0.03 (the C-21A fact sheet) |
| Climb at 45,000 ft, as light as it flies | 2,390 ft/min | at least 300 (its maximum operating altitude) |
| Stall, flaps 8, 15,800 lb | 107.4 KCAS | 109.0 ± 3, the AFM's printed example |
| Stall, flaps up, 15,300 lb | 118.6 KCAS | 119 ± 3, the AFM's chart |
| Stall, flaps 40, 15,300 lb | 96.2 KCAS | 96.5 ± 3, the AFM's chart |

**What is missing first:** the flight manual has no cruise or climb-rate
charts; the cruise is the Air Force's fact sheet's, and the ceiling only a
floor. Its lateral and longitudinal derivatives are the Learjet 23's, a
smaller wing's, taken as the 35A's; its damping in roll and yaw, side force,
inertias, empty centre of gravity and engines' thrust with height and speed
are estimates, each named in the script; its wing area, which no 35A document
gives, comes from its certified chord and span and the 23's planform. The
lift's maximum at each flap setting is set from the flight manual's stall
speeds, which the stall figures then fly - they check the pitch control and
the stall's dynamics, not the lift. It has no stick pusher, no Mach trim and
no trimmable stabiliser. No landing is flown. It has no visual model.

**The scripts that write a model whole share their XML** in
`tools/written.py`: the A380's, moved there, writes the same files as before.
