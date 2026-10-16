### The F-15C and F-22 against their published figures, 2026-09-19 — item done (CI run 35430601204)

Phase 5's "the F-15 and F-22 fly to their figures": the McDonnell Douglas
F-15C Eagle and the Lockheed Martin F-22A Raptor, each made from JSBSim's
model by a script listing every change and why, land inside their
tolerances on their maximum Mach at altitude, their climb, their ceilings
and their sustained turn at Mach 0.9 and 30,000 ft - the F-15C's six figures
from the Air Force's Standard Aircraft Characteristics (1992), the F-22's
five from the Department of Defense's Selected Acquisition Report (2010) and
the Air Force's fact sheet.

| F-15C | Measured | Published |
| --- | --- | --- |
| Maximum Mach at 45,000 ft, clean, 36,946 lb | 2.40 | 2.39 ± 0.08, page 6's chart |
| Rate of climb at sea level, military power, 45,713 lb | 15,300 ft/min | 15,250 ± 10% |
| Rate of climb at sea level, maximum power, 41,286 lb | 56,900 ft/min | 55,960 ± 10% |
| Service ceiling, 100 ft/min, military power, 45,713 lb | 46,900 ft | 46,750 ± 5% |
| Combat ceiling, 500 ft/min, maximum power, 41,286 lb | 57,600 ft | 56,100 ± 5% |
| Sustained turn, Mach 0.9, 30,000 ft, clean | 7.82 °/s | 7.87 ± 0.5 (3.95 g, page 5's chart) |

| F-22A, 54,960 lb | Measured | Published |
| --- | --- | --- |
| Supercruise, military power, best altitude | Mach 1.77 | 1.76 ± 0.05, demonstrated |
| Acceleration, Mach 0.8 to 1.5 at 30,000 ft | 52.7 s | 52.4 ± 10%, demonstrated |
| Sustained turn, Mach 0.9, 30,000 ft | 7.35 °/s | 7.34 ± 0.5 (3.7 g, demonstrated) |
| Level at 40,000 ft, full throttle | Mach 2.15 | "Mach two class", read as 1.95 to 2.35 |
| Rate of climb at 50,000 ft | 8,850 ft/min | at least 100 ("above 50,000 feet") |

**What is missing first:** the F-22's figures are few, and much of it is
pinned by none: its military thrust is JSBSim's, unpublished; its drag's
shape past Mach 1.2 and its engines' behaviour with speed and height are the
F-15C's, which the F-15C's charts pin; its pitch loop's gain schedule and its
roll stick's shaping are this project's, needed to fly it, and not the
F-22's, which are not public; its thrust vectoring is JSBSim's and flown by
no figure. Neither fighter's take-off, landing or stall is checked. The
F-15C's page 4 figures are for four AIM-7Fs, whose drag the model does not
carry. Past the flow leaving the wing, the drag is estimated - the lift times
the tangent of the angle of attack - and no figure flies there. The HUD
shows neither Mach nor g yet (its own Phase 5 item); neither has a visual
model.

**The F-15C is fitted to its charts.** Its thrust and drag with Mach are
fitted, twelve numbers, to thirty-three points read from the Standard
Aircraft Characteristics' chart of specific excess power at maximum power,
30,000 to 59,000 ft and Mach 0.9 to 2.4, and to its figures: the model meets
the chart to 27 ft/s, about what the chart can be read to. The fit needed an
engine that gives more thrust in cold air than Mattingly's typical
afterburning turbofan - the chart's excess power at Mach 0.9 and its
manoeuvrability chart's sustained load factor together pin the thrust there
a fifth above his - and loses some above the tropopause; `tools/fighter.py`
has the form, Mattingly's with his typical engine as one case.

**JSBSim's fighters did not fly to anything.** The F-15's lift fell to a
quarter between its only two Mach columns, 0.5 and 1.4, and its drag at zero
lift halved; its afterburner answered only a throttle past 1, which
glideslope's never reaches; the idle thrust JSBSim adds to the military
table was a quarter of the thrust at height. The F-22 could not be flown at
all: four of its actuators had a lag of 0, which JSBSim takes as a filter
that never moves, so every control surface stayed where it started. With
them moving, its lift and tail did not change with Mach; its lift-dependent
drag was a span efficiency of 1.9; its throttles passed 0.99 to its engines
as 0.991, lighting the afterburners at military power; its pitch loop went
into a limit cycle, the stabilators beating at their rate limit three times
a second, past Mach 2.06 at 36,000 ft; and its roll stick commanded 22
degrees a second in its first tenth of travel, so that any small correction
rolled it over. Each is set right in its script, with the reason.

**The simulation learned to measure a fighter.** Fuel can be frozen, so a
figure is flown at its weight. Four new figure flights: the greatest rate of
climb as a level acceleration's specific excess power, the flight test's way,
counted only once the aircraft is level at 1 g; the service and combat
ceilings, where that falls to 100 or 500 ft/min; the time to accelerate
between two Mach numbers; and the level Mach at the best altitude. The
sustained turn, which chased Mach with bank and never settled on the F-15, is
now the load factor at which the aircraft neither gains energy nor loses it,
found by halving - the load factor a manoeuvrability chart plots. A
calibrated airspeed past Mach 1 is found through Rayleigh's pitot formula,
behind the shock the pitot tube stands in.
