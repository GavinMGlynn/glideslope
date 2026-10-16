### The autopilot banks only as far as the aeroplane can sustain, 2026-09-24 — tail done

**What was found first: the tail's diagnosis was wrong, and the fault was
real.** The tail said the Cessna 172P lost 2,076 feet in a ninety-degree
turn at 10,000 feet because the bank was more than it could sustain.
Measured again, it lost the height before the turn began: 10,000 feet is
above its ceiling as the AI flies it - full rich, because nothing the AI
flies leans the mixture - and wings level the altitude hold had already
spent its airspeed down to 47 knots. That is two faults of their own, now
tails: **the altitude hold flies an aeroplane into the stall when asked for a
height it cannot hold**, and **the AI never leans the mixture**, which puts
the light aeroplanes' ceilings on the autopilot at 8,200 to 8,500 feet
against the 172P handbook's 13,000.

The bank fault is there all the same, just below the ceiling. Where each
light aeroplane's best climb has fallen to 50 ft/min - halfway from its
service ceiling (100 ft/min, the FAA's Pilot's Handbook of Aeronautical
Knowledge, chapter 11) to its absolute ceiling - a ninety-degree turn held
the height and cost at most four knots, but **a full circle did not**: the
old autopilot banked to 25 degrees throughout, the pitch held the height by
spending the airspeed, and the 182 slowed from 82 knots to 60, the 172P from
75 to 66, the Cherokee from 74 to 66 and from 81 to 72.

**What changed** (`src/sim/autopilot.cpp`). While the throttle is at its
stop and the wings are banked, a turn may spend 3 knots' worth of the
aeroplane's total energy - its height plus its true airspeed's kinetic
height, `h + V²/2g` - and then no more: from there the bank limit is an
integral on how far the energy is below that allowance (0.05 °/s per foot)
and on its rate (0.02 °/s per ft/min, smoothed over a second), between 10 and
25 degrees. A descent asked for is not counted as spent. When the throttle has
more to give, or the wings come level, the limit returns to 25 degrees at
5 °/s, so in every flight where the throttle is not at its stop the autopilot
flies exactly as before. The energy is counted from when the bank passes 5
degrees, not from when the throttle reaches its stop, so what the airspeed
lost while the throttle was still opening counts too.

**Why this and not a fixed schedule.** Real autopilots limit bank by fixed
numbers - 22 degrees for Garmin's GFC 700 (its training overview:
"maximum commanded ... bank (22°)"), and the A320's flight guidance a
schedule on speed that drops to 15 degrees with an engine out. A fixed
number would have to be low enough for the worst case and would slow every
turn everywhere else; nothing tells this autopilot its aeroplane's power. The
rule it applies instead is the sustained turn's - how much load factor an
aeroplane holds without losing height or speed is set by its excess power
(Hurt, *Aerodynamics for Naval Aviators*, NAVWEPS 00-80T-80) - measured the
way Lambregts' total energy control measures it (AIAA 83-2239, 1983): by the
total energy's rate, which the throttle cannot make up once it is at its
stop.

**Verification.** `a_cessna_172p_…`, `a_cessna_182_…`, `a_piper_cub_…` and
`a_cherokee_near_its_ceiling_holds_its_height_through_a_turn_as_at_3000_ft`,
one test per aeroplane so that they run side by side, and
`every_light_aeroplane_the_data_holds_is_turned_near_its_ceiling`, which
fails if the catalogue's light aeroplanes are other than those four. Each
finds its aeroplane's height near the ceiling by flying it there, then flies
16 turns: at 3,000 ft and near the ceiling, asked for its published
best-climb speed and for its start speed (which near the ceiling it cannot
reach, so its throttle is at its stop - asserted, not assumed), turning 90
and 360 degrees each way, each from level flight settled until its airspeed
moves less than half a knot in half a minute. Every turn holds its height
within 20 ft (the calm-air altitude band), its speed within 5 kt of the
level speed, and ends within 2 degrees of its heading. Measured, near the
ceiling: height within 4.2 to 9.3 ft; the most any turn cost in speed is 4.0
knots (the 182 turning left at its stop, 86.3 to 82.3); at 3,000 ft within
11 ft and 0.9 knots, as before. 64 turns, four aeroplanes.

**Seen to fail.** With the limit disabled (the turn never counted as having
spent its allowance), the 172P, 182 and Cherokee tests failed, each on its
full circles near the ceiling (the 182 down to 59.7 knots from 82), and
passed again with it restored. **The Cub's test passes either way**: near its
ceiling it has the power for a 25-degree turn and loses at most three knots,
so it is covered but does not discriminate. No ninety-degree turn failed
without the limit, which is why the circles are in the test.

**Nothing else moves.** The selftest replays a pilot's inputs and does not use
the autopilot; its hash is unchanged. The rest of the suite - the capture
and handover tests, the navigator, the lessons, the circuits, the server's
AI - is green; none of them flies with the throttle at its stop in a turn.
