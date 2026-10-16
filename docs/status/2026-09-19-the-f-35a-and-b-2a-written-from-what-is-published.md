### The F-35A and B-2A written from what is published, 2026-09-19 — item done (CI run 35442214130)

Phase 5's "F-35A and B-2 flight models, written here from what is
published": the Lockheed Martin F-35A Lightning II and the Northrop Grumman
B-2A Spirit, which JSBSim does not have, written by `tools/make_f35a.py` and
`tools/make_b2.py`, land inside their tolerances on every figure published of
their performance - maximum speed, ceiling and range - and on nothing more,
as nothing more is published.

| Aircraft | Figure | Measured | Published |
| --- | --- | --- | --- |
| F-35A, 39,995 lb | Maximum Mach, full throttle, best altitude | 1.62 | 1.6 ± 0.05 ("Mach 1.6", no altitude given) |
| F-35A, 39,995 lb | Rate of climb at 50,000 ft | 5,950 ft/min | at least 100 ("above 50,000 feet") |
| F-35A, 49,120 lb | Range on internal fuel, 40,000 ft, Mach 0.8, no reserve | 1,830 nm | more than 1,200 |
| B-2A, 253,000 lb | Level at 40,000 ft, full throttle | Mach 0.89 | "high subsonic", read as 0.80 to 0.95 |
| B-2A, 177,160 lb | Rate of climb at 50,000 ft | 1,450 ft/min | at least 100 (ceiling 50,000 ft) |
| B-2A, 336,500 lb | Range, 40,000 ft, Mach 0.8, no reserve | 6,100 nm | about 6,000 ± 15% |

**Which of their behaviour no figure pins - nearly all of it.** Neither
aircraft's aerodynamics is public. The F-35A's lift and drag are a fighter's
of its wing (tools/fighter.py), their numbers set to fly the figures; its
moments are the F-4C's, from NASA CR-2144, the nearest published fighter's;
its inertias the F-4C's scaled; its engine's thrust with speed and height the
F100's, fitted to the F-15C's charts, and its fuel consumption an estimate.
Its fly-by-wire flight controls, and their limits and protections, are not
modelled: a pitch and a yaw damper stand for them, on an airframe given
positive stability. Its combat radius, 669 nm demonstrated (the Selected
Acquisition Report), is recorded but not flown, as no profile is given; nor
are its sustained turn and transonic acceleration, which DOT&E reports
were reduced without saying from what. The B-2A has less: every derivative is
a tailless swept wing's, estimated; its wing area and chord are estimates from
its span and length; its flight controls' directional stability - the drag
rudders against sideslip - is modelled as a gain; its engines' fuel
consumption is set to fly the range. Its speed is "high subsonic" and its
range on no stated profile. No take-off, landing or handling figure is
checked for either; they have no visual models.

**A new figure flight, the range.** The specific range, level at an altitude
and Mach with the throttle holding the Mach, is measured over two minutes at
full, half and nearly empty tanks, and integrated over the fuel by Simpson's
rule - as fast as a few minutes of flight, where flying the whole range would
take the B-2 twelve hours.
