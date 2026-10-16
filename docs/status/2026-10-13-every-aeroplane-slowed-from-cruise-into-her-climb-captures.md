### Every aeroplane slowed from cruise into her climb captures her climb speed: the capture for all, the floor for the light ones, 2026-10-13 — item done

**Nothing of the item is missing.** The capture test's six named
aeroplanes, and the Learjet 35A, are within 0.9 kt of their climb speeds,
none named. No platform code, so it was not built on Windows.

**Why they sank** (traced on the capture test, every half second from the
slowing): the same cause as the light aeroplanes' before their capture of 2026-10-10. Asked
for a slower speed and a climb at once, the throttle law cut each to idle and
opened again only as the speed came - at its quarter of a travel a second,
from 0 to the 0.6 to 1.0 her climb wants - while the climb went on spending
speed at 2 to 4 kt a second. The S.23 at 100 kt asked: idle from 123 s,
opening from 131 s at 102.7 kt, 0.83 at 135 s at 95.2. A light aeroplane's
floor catches this by holding the climb back while the throttle comes up
(`early_hold_`); the others had no floor, so nothing did.

**Two things the trace showed beside it.** (1) **The test flew every
aeroplane whose gear retracts with it down**: the controls it handed the
controller left `Controls::gear` at its default, 1. Gear down, the F-22A
porpoised ±600 ft/min about her climb and still dipped 2.6 kt under it after
the capture; flown clean, as the test means, she holds 220.0. The test now
hands the gear up, and the old figures below are clean ones. (2) **The F-22A
is never "clean" by her flaps' position**: her model schedules them on her
speed and Mach (3.7 degrees at 252 kt, 15 at 220) and reads no flap lever,
so the autopilot's clean check (flaps up, gear up) said she had flaps out.
`Aircraft::flap_lever` is now found from the model - whether it, or a system
file it names, reads `fcs/flap-cmd-norm` - and `clean` counts the flaps only
where a lever moves them.

**What was tried first, and why not kept**: a lead on the throttle's speed
error, by her deceleration over half the time the throttle needs to come back
to its integral (the setting it was holding). Every one sank further - the
S.23 to 95.2, the F-22A to 210.9 (gear down) - because the climb wants more
power than the cruise the integral remembers, and the lead shrinks as the
throttle opens. The light aeroplanes' capture found the same of a fixed lead.

**The fix** (`Autopilot::fly`, `capture_only`): every aeroplane has the
light aeroplane's capture, though only a light aeroplane has the floor. With
no floor, the least is the speed asked; the hold engages only through the
capture's own test - slowing more than a knot a second and the throttle
unable to reach its stop before the speed - never because the throttle is
at its stop; while engaged it holds the climb she has as the throttle opens
fully, then finds the climb that keeps the speed, and lets go once she has
the climb asked. **It holds back a climb and never asks for a descent**
(its lower bound is level, not the pitch envelope's steepest): on the first
broad run, let down to a descent, the F-22A gliding round her tightest orbit
stalled at 225 kt, and the F-15C at 170 to 185; and the 747-400 and F-35B
entering an orbit level from a faster speed were 74 ft off their height. All
eight pass with the bound.

**Measured** (linux-release, calm air, gear up, 500 ft/min asked at her climb
speed from her catalogue start speed; slowest from then on, before -> after):

| Aeroplane | climb speed | before | after |
|---|---|---|---|
| B-2A | 230.0 | 227.4 | 229.5 |
| F-15C | 200.0 | 198.0 | 199.9 |
| F-22A | 220.0 | 215.5 | 219.9 |
| F-35B | 200.0 | 197.5 | 199.8 |
| Learjet 35A | 240.0 | 237.2 | 239.7 |
| Mosquito | 148.0 | 144.6 | 147.7 |
| Short S.23 | 100.0 | 97.7 | 99.1 |
| 747-400 | 170.0 | 172.8 | the same |
| C172P, C182, J-3 Cub, PA-28 | 75.4, 82.0, 47.8, 73.9 | 74.1, 81.3, 47.8, 72.7 | the same |

The 737-300, 787-8, A320 and A380 climb faster than their cruise and are
counted as not slowed. The "before" column is the old code flown clean, as
the test now flies it. The Learjet passed the old test only because it flew
her gear down: 238.1 there, inside the 2 kt, against 237.2 clean.

**Tests**:
- `slowed_from_cruise_into_a_climb_every_aeroplane_sinks_no_more_than_2_kt_under_its_climb_speed`:
  no names; the gear handed up. **Seen red** on the code before (this
  test, the old autopilot and aircraft): 7 sank past, the first "b2 sank to
  227.4 kt, under her 230.0 less 2". Green after.
- New, `every_aircraft_has_a_flap_lever_exactly_where_her_flaps_answer_one`:
  every aircraft flown 30 s with the flap control at 0 and at 1; her flaps
  move between the two exactly where her lever is found (eleven with, five
  without - the B-2A, F-15C, F-22A, F-35B and J-3 Cub), counted, the F-22A
  by name. **Seen red** with the finding inverted: "737-300's flaps answer
  the lever, and her model is found not to read one".
- **The broad run - a subset chosen by name, not the full suite**
  (linux-release, `ctest -j4`, DISPLAY unset): every
  in-process unit test whose name speaks of a climb, a take-off, a first leg,
  a departure, an orbit, a glide, a plan, a hand-over, a ceiling, the
  autopilot, a height, turbulence, a gust, a stall, a turn, a cruise, a
  capture, an approach, a speed, an airliner, a lesson, a crosswind, a
  heading, the throttle, level flight, a descent, a route or a waypoint -
  554 tests, none of the server, client or renderer ones: all passed, after
  the eight above were fixed. Among them the climb, cruise, capture,
  airliner low-height hold, stall recovery and AI-notice, approach,
  plan-speed and orbit tests.

**The selftest hash does not move**, `182dd6c996e0ee4c`: its C172P has a
floor, and no flaps her lever does not move.

**The flap lever's detection is a substring match.** `flap_lever_of`
(sim/aircraft.cpp) calls a model's flaps a lever's when any data line of
the model, or of a system file it names, contains `fcs/flap-cmd-norm`. A
comment, a property merely declared, or a lever read by something other
than the flaps would be taken for one, and a lever read under another name
missed. It is right for all sixteen today, and the new test flies each to
check it; an aircraft added where it is wrong turns that test red.

**Found on the way, a new open item in Phase 14b: many airborne tests fly
gear-down without meaning to.** `Controls::gear` defaults to 1, so a test
that hands a controller or autopilot default controls lowers the gear of
every aircraft whose gear retracts, whatever its initial conditions said.
The capture test was one (fixed here). The reviewer of #178 found the same
in tests/unit/test_lesson.cpp at about lines 3884, 3994, 4253, 4494, 4603,
4796, 4935, 5130, 8323 and 8688, the landing tests at about 2225, 7264 and
7443 (where gear down may be meant), and about 100 more `Controls`
declarations in the other test files. Each needs its gear stated - the
helper starting it as the aircraft starts - and an audit naming those that
mean it down; their figures may move when it is.
