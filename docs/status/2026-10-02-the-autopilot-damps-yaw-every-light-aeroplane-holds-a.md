### The autopilot damps yaw: every light aeroplane holds a heading in a crosswind, 2026-10-02 — tail done

**The cause was the autopilot's, not the models'.** Its rudder held the
ball - the sideslip, at a tenth of the rudder's travel a degree, with an
integral - and nothing else. The sideslip gives a rudder stiffness and no
damping, and since the autopilot was held to a hand's pace the rudder moves
a travel a second at most. A small swing is damped by the aeroplane; a large
one asks the rudder for more than a travel a second, the rudder falls a
quarter of a cycle behind the sideslip and feeds the swing - a limit cycle
the rate limit makes. A crosswind arriving all at once is such a swing: 10
to 28 degrees of sideslip to settle. Logged at 4 Hz, the Cessna 172P at 100
kt in a 20 kt crosswind: the rudder a triangle wave moving at exactly a
travel a second, its peaks a quarter of a cycle after the sideslip's, the
yaw rate 35 degrees a second, a 2.5 s period. The Cub and Cherokee met it
soonest because their rudders turn them hardest: the travel times the
model's rudder moment (Cn_dr), 0.52 rad x 0.0565 for the Cub, 0.47 x 0.043
for the Cherokee and 0.28 x 0.043 for the 172 - 2.4 and 1.7 times the
172's. Their yaw derivatives were left alone: the Cub's Cn_beta, 0.060,
and Cn_r, -0.081, are Du's (Cranfield, 2011), the Cherokee's 0.092 and
-0.093 its JSBSim model's, the 172's 0.059 and -0.094; none is out of
family, and a model change would not answer the Cessnas.

**Measured on the old law** (the new test, below): 

| | calm | 20 kt crosswind |
|---|---|---|
| C172P, 60 kt | within 0.01 | within 0.04 |
| C172P, 100 kt | within 0.01 | -14.2 to +14.3, heading 21 off |
| C182, 64 kt | within 0.02 | within 0.02 |
| C182, 120 kt | within 0.01 | -14.7 to +14.8, heading 21 off |
| J-3 Cub, 43 kt | within 0.01 | -33.7 to +36.1, heading 78 off |
| J-3 Cub, 60 kt | within 0.01 | -35.9 to +37.0, heading 62 off |
| PA-28, 64 kt | within 0.02 | -36.5 to +36.6, heading 44 off |
| PA-28, 110 kt | within 0.01 | -29.1 to +29.1, heading 34 off |

Sideslip in degrees after the first 30 s. So the Cessnas did it too, at
cruise; #70 measured them only at their approach speeds.

**The fix** (`sim/autopilot.cpp`, `rudder_per_degps_of_yaw`): a yaw damper,
the rudder against the yaw rate with its steady part washed out over a
second, so a steady turn's rate asks nothing of it - the classical yaw
damper with its washout filter (Stevens, Lewis and Johnson, Aircraft Control
and Simulation, chapter 4). 0.05 of the rudder's travel per degree a second.
Swept on the new test: 0.02 left the Cub at 43 kt and the Cherokee at 64 kt
swinging 30 degrees; 0.03, 0.05 and 0.1 settled all four, and a washout of
3 s as well as 1. The ball's gain and integral are unchanged. (Lowering the
ball's gain alone was tried: at 0.05 the Cub at 60 kt and the Cherokee still
swung, and at 0.03 the Cherokee swung 4.5 degrees in calm air.)

**After**: every case within 0.03 degree of sideslip after 30 s, the
headings within 1.02 degrees (the C182 at 120 kt, still drifting in from
the step); the most sideslip on the way, the step itself, 24 degrees (the
Cub at 43 kt).

**The Cub's and Cherokee's orbits in wind are flown again**:
`the_tightest_orbit_at_the_approach_speed_is_flown_on_its_circle_by_every_light_aeroplane`
leaves nothing out, 16 of 16 orbits. In a 10 kt wind the Cub holds its 268 m
circle at -24 to +44 m, the Cherokee its 593 m at -35 to +51, within 2 ft.

**Test**:
`every_light_aeroplane_holds_a_heading_in_a_20_kt_crosswind_and_in_calm_air_without_yawing`:
every light aeroplane the catalogue holds (compared by id with the four), at its
approach speed and the speed it starts a flight at, in calm air and in a 20
kt wind from the west arriving all at once, heading north at 3,000 ft on the
autopilot alone; after 30 s its sideslip within 1 degree and its heading
within 2 for the next 90 s. 16 of 16 flown.

**Every other aircraft too**, since the damper's one gain acts on all of
them:
`every_aircraft_but_the_light_aeroplanes_holds_a_heading_in_a_20_kt_crosswind_and_in_calm_air_without_yawing`
flies the other twelve the same way, 34 of 48 cases. Left out, each named
in the test: the 747-400's and F-22's approach speeds (no published stall
speed, so none), and the approach speeds of five jets that come down to the
ground flown clean there, with the damper or without it and in calm air as
in wind - the 737-300, A380, B-2A, F-35B and Learjet 35A, the tail "A plan
may fly a jet clean at its approach speed". Every case flown holds its
sideslip within 0.07 degree after 30 s and its heading within 1.97 (the
787-8 at 250 kt, still drifting in); the 787-8 comes down 2,750 ft at its
approach speed but holds its heading. On the old law every case flown held
as well but one: **the F-22 at 300 kt swung 4.7 degrees either way in the
crosswind** and never settled, its heading 5.4 off; with the damper, within
0.01. So no class needs its own gain.

| | old law | yaw damper |
|---|---|---|
| F-22, 300 kt, crosswind | -4.73 to +4.77 | within 0.01 |
| 787-8, 148 / 250 kt, crosswind | within 0.09 / 0.03 | within 0.07 / 0.02 |
| A320, 147 / 250 kt, crosswind | within 0.01 | within 0.01 |
| F-15C, 160 / 300 kt, crosswind | within 0.01 | within 0.01 |
| Mosquito, 123 / 220 kt, crosswind | within 0.05 | within 0.01 |
| Short S.23, 86 / 130 kt, crosswind | within 0.09 | within 0.07 |
| 737-300, A380, B-2A, F-35B, Learjet, 747-400, at cruise, crosswind | within 0.02 | within 0.01 |

**The coverage is asserted**: the aircraft each test meets are compared,
by id, with those it expects (the four light aeroplanes; the rest of the
catalogue), each left-out is checked to be in the space, and an aircraft's
approach and starting speeds are checked to differ, so no case is flown
twice.

**Seen to fail.** On the old law, as the table: six of the eight windy cases.
With the damper's gain made 0 as a deliberate bug, both the new test (the
same six) and the light-aeroplane orbit test (the Cub round 268 m in wind at
233 to 502 m) failed, and so did the test of the rest (the F-22, as above);
restored, all three pass. With the Cherokee dropped from the light
aeroplanes expected, the light test failed, the catalogue giving it; and
the test of the rest failed, the two lists no longer the whole catalogue.

**Verified** in linux-release: 394 tests chosen by name - every autopilot,
orbit, navigator, plan, lesson, landing, lander, approach, circuit,
take-back, hand-over, no-step, stall, turn, crash, flare, departure, learnt,
copilot, figures and selftest test among them - none failed but two
client-with-window tests that could not listen on their ports while other
working copies' tests ran; alone, both pass. Eleven skipped: the live-model
ones, the window ones and Windows's own.
`every_aircraft_turns_ninety_degrees_without_overbanking_or_overshooting`
passes. The selftest hash does not move: `d36123c1eecc3e23` (it flies a
logged input, not the autopilot).

**The whole linux-release suite** was run once after the review's changes
(753 tests, ctest -j4, no display, other working copies' tests running
beside it): 731 passed, 17 skipped (the live-model, window and Windows
ones), and five failed. Three server-copilot tests were refused their
fixed ports by the other copies' tests, and pass alone. The open terrain's
two distance tests on Vulkan timed out at 30 minutes; they draw the terrain
at the surveyed runway ends and fly no autopilot, and were not run again.
Rebased on main at 0903c2c, the 414 tests chosen by name again: none failed
but two client-with-window tests refused their ports, which pass alone.
