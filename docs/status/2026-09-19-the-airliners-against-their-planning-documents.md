### The airliners against their planning documents, 2026-09-19 — item done (CI run 35423458464)

Phase 5's "the airliners fly to their figures": the Airbus A320 and the
Boeing 737-300, 747-400 and 787-8, each made from JSBSim's model by a script
listing every change and why, land inside their tolerances on the take-off
runway length at maximum weight from their manufacturers' airport-planning
documents, the climb with an engine out their certification basis demands,
their cruise Mach, and their type certificates' ceilings.

**What is missing first:** these are four figures each, and much of each
aircraft is pinned by none of them. No approach or landing is flown - the
planning documents' landing lengths are not yet checked, and the autopilot
cannot land (a Phase 8 item). The climb is FAR 25.121(b)'s floor, not a
climb rate: Boeing publishes none, and Airbus's climb table (Getting to
Grips with Aircraft Performance) is for the V2500 A320, not the CFM. The
cruise is a band - above the published cruise Mach, and no more than 0.03 past
Mmo - not a speed at a thrust. The airliners fly only from the catalogue; the
HUD gives them no Mach or flight level yet (its own Phase 5 item); they have
no visual models. Some of what they are made of is estimated, and the scripts
say so: the engines' thrust with height and speed (Mattingly's lapse for a
high-bypass turbofan), the drag rise past the critical Mach (Lock's
fourth-power law), a windmilling fan's drag, a dry runway's braking, and the
lift and drag the take-off flaps and slats give, set where the take-off
figures need them.

| Aircraft | Take-off runway, maximum weight | Engine-out climb | Cruise Mach, full throttle, 35,000 ft | Climb at its ceiling |
| --- | --- | --- | --- | --- |
| A320 (-214), 73,500 kg | 6,200 ft against 5,850 (±10%) | 3.7%, at least 2.4 | 0.84, 0.78 to 0.85 | 2,550 ft/min at 39,100 ft |
| 737-300, 135,000 lb | 7,790 ft against 8,400 | 2.4%, at least 2.4 | 0.78, 0.74 to 0.85 | 1,540 ft/min at 37,000 ft |
| 747-400, 875,000 lb | 11,270 ft against 10,500 | 4.0%, at least 3.0 | 0.91, 0.85 to 0.95 | 1,840 ft/min at 45,100 ft |
| 787-8, 502,500 lb | 10,880 ft against 10,100 | 2.6%, at least 2.4 | 0.91, 0.85 to 0.93 | 2,730 ft/min at 43,100 ft |

The runway lengths are read from the planning documents' charts, to about
100 ft; the ceiling is climbed to at the lightest weight flown, the
operating empty weight and a tenth of the fuel, with the 300 ft/min that
makes an altitude a thrust-limited ceiling.

**The take-off runway length is FAR 25's, flown.** A new figure flight takes
the take-off speeds from the aircraft's own stall at the take-off flap and
weight - V2 1.2 times it, VR 5% below - and flies every engine to 35 ft, and
then the balanced field: an engine failing at a speed found by halving,
going on to 35 ft and, a second later, stopping, the brakes, throttles and
speedbrakes a second apart as FAR 25.109 allows. The runway length is the
longer of 115% of the first and the second. Where the aircraft cannot climb
at FAR 25.121(b)'s gradient with an engine out at 1.2 times its stall, V2
rises until it can, as a flight manual's "improved climb" does - which is
why the 737-300's chart bends sharply upward above 130,000 lb, and why the
737-300 here flies V2 at 1.24 times its stall.

**JSBSim's airliners did not fly.** None could be loaded to its maximum
weight - the 737 had nowhere to put a payload, the 747's tanks held a
seventh of its fuel and its empty weight carried the rest. Their engines
kept their thrust far too well with height and speed, and with no drag rise
worth the name all four flew past Mach 0.85 at nine-tenths throttle at
35,000 ft, the 747 to 0.97. A failed turbine relit itself; a stopped one had no drag. The
A320's static margin was 70% of its chord, so that it could not be flown
slower than 173 knots with its take-off flaps, and its drag rose with angle
of attack at twice a wing's; the 787's rudder pedals moved its rudder 3.6
degrees; the 747's nose gear let it sit 3 degrees nose down; the 737 opened
a telnet port and a UDP port whenever it was loaded, which a test now
forbids of every model. Each is set right in its script, with the reason.

**The simulation learned two things:** a jet's failed engine is cut off, not
only stopped, so that it cannot relight; and a speedbrake lever works an
aircraft's flight and ground spoilers. Besides the take-off runway length,
the figures have three new flights - the engine-out climb, the level Mach at
full throttle and the climb at a ceiling - and the stall flight runs long
enough for a jet's entry speed, a second a knot; the Cessnas' 70-knot entry
runs the eighty seconds it did.
