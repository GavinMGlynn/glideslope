### A take-off with no plan levels off and holds its height, 2026-11-05

**What it does not do, first.**
- **No AI take-off raises her gear**, with a plan or without: Phase 10a's
  item on it. Every landplane here held 3,000 ft for ten minutes with her
  wheels down, the fighters at 337 to 384 kt. The F-22A's thrust vectoring
  is off while her gear is down (her model's `fcs/tvc-inhibit`), and her
  pitch hunts for a minute after the hand-over: she goes 386 ft past her
  height before she settles within 100 ft of it. Her bound in the test is
  named, 450 ft, her measured 386 and a margin. Every other landplane goes
  no more than 10 ft past.
- **The A380's F and S speeds are not Airbus's tables**, which are not
  public. They are Airbus's rule, 1.23 times the stall, applied to her
  model's own stalls (below).
- **The hold is the plain autopilot's**: her height, her heading and the
  speed she was handed over at. No server reaches it today, because every
  take-off a server gives has a plan.

**What it does.**
- **The A380 raises her flaps at Airbus's F and S speeds**
  (`assets/figures/a380.xml`, `<flap_manoeuvre_speeds>`). F is the least
  speed to retract from CONF 2 to 1+F and S the least to go clean. Airbus
  defines them as about 1.18 to 1.26 times the one-g stall at CONF 1+F,
  and 1.21 to 1.25 times the clean stall (Airbus Safety First, "The Airbus
  Speed Tape"; SKYbrary's bookshelf copy). Each here is 1.23 times her
  model's stall. The take-off flap's measured 136.76 kt at 1,267,657 lb is
  carried to flaps 8 and flaps up by the square root of her lift table's
  greatest coefficients: 2.11, 1.94 and 1.333. That gives 142.6 and
  172.1 kt, so F is 175 kt and S is 212 kt, under CONF 1+F's 222 kt limit.
  They are written over the approach speed worked from her landing stall,
  at her heaviest, rounded up (10.75 to 11 kt, 46.97 to 47), so she is
  never under them there, and a lighter A380 is flown a few knots over. Flap manoeuvre speeds may now be given over that worked approach
  speed as well as over a published `<approach>` (`sim/figures.cpp`).
- **A take-off with no plan levels off at its height**
  (`Departure::level_off_at_end`, set by `Controller::to_ai_take_off`,
  cleared by `to_ai_flying`). With flaps up and her take-off trim off, the
  take-off is over a tenth of her climb, in ft/min, short of its height.
  That is the lead an instrument pilot levels off by: "lead the level off by
  approximately 10 percent of the vertical speed", 300 ft short at
  3,000 ft/min (CNATRA P-1204, Constant Airspeed Climbs; AFMAN 11-217
  vol. 1). The autopilot then holds that height, not the one she was handed
  over at. It closes on it at up to the climb she was handed over with, not
  stepped down to 700 ft/min. A light aeroplane's climb is not eased then,
  because the capture is the easing. `climbed_out`, which the take-off
  trials read, is not moved, and a plan's take-off is unchanged.

**Why the A380 was lost.** With no flap schedule, her take-off raised
CONF 2 at her 174 kt initial climb speed, V2 and ten. Clean, at 575 t, that
put her wing at 10 degrees, its greatest lift. She climbed out at 177 kt
and the autopilot held that speed at 3,000 ft. As it levelled her and drew
the power back she stalled, and hit the ground at 11,400 ft/min, 41 s after
the hand-over. This was the 747-400's fault of 2026-11-01, and it is
answered the same way. Then the jets' capture: handed over at their height,
climbing at thousands of feet a minute, the 737-300 went 139 ft past it and
the F-22A 1,636 ft.

**Verification.**
`every_landplane_given_a_take_off_to_3000_ft_and_no_plan_holds_it_for_ten_minutes`
(tests/unit/test_lesson.cpp) flies each landplane in the catalogue, 15 of
15, asserted, with the Short S.23 named and left out because she takes off
from water. Each takes off at the weight of her take-off figures and is held
for 600 s. The test asserts:
- no wreck;
- she is handed over a tenth of her climb short of 3,000 ft;
- she never sinks 100 ft under where she was handed over;
- she goes no more than 100 ft past her height, the F-22A no more than
  450 ft, and every aeroplane named with a bound of her own is one flown;
- she is within 100 ft of it from two minutes on, the FAA's altitude
  tolerance (Private Pilot ACS);
- she keeps four fifths of the speed she was handed over at.

All 15 met it. The A380 was handed over at 2,805 ft, 213 kt and
+1,955 ft/min, and held 3,000 ft to the end. The worst figures were the
F-22A's: 386 ft past, then within 5 ft. On the code before the change the
test went red on the 737-300's capture, 139 ft past. The A380, flown on
that code, hit the ground at 214 s (the cause, above).

**Unchanged.** The take-off trials and every plan's take-off: the lead
applies only with no plan, and `climbed_out` is not moved. The A380's own
take-off figures are flown with her new flap schedule. Run on linux-release,
excluding gust_sweep: every test matching take, depart, circuit, lesson,
controller, figures, a380, selftest, climb, autopilot, go_around, straight,
level, plan, demonstrat, wet and lift_off - 480 of 480 passed, 11 skipped
for want of a key or of Windows. The gust_sweep label was not run.
Nothing here is platform-specific code, so there was no Windows build.
