### A fast aircraft's tightest orbit is flown on its circle, and its fastest leaves power in hand, 2026-10-06 — tail done; the B-2's and F-22's yaw still open

**What is not done first.** The B-2 and F-22 still yaw in a crosswind when
slow (the cause is found, below; the fix is not in). The glide, F-15C stall
and stall-recovery tails were not worked on.

**The cause.** The navigator turned in towards the circle by 90 degrees a
kilometre whatever the speed: half a kilometre off, a jet at 360 kt was asked
for 45 degrees, which at 1.4 degrees a second it took half a minute to turn
back from. The F-15C swung 3.5 km either side of its 18.8 km circle.

**The fix** (`sim/navigator.cpp`): it turns in by the angle that would bring
it back in twelve seconds at its airspeed (the old law is twenty seconds at 60
kt), and its trim counts while that is 9 degrees or less. Twenty seconds let
the C172P of the recorded OpenAI plan run 67 m wide at 100 kt; fifteen, the
S.23 at 86 kt in wind 67 m inside.

**The join is measured apart.** From 2 km outside heading for the centre, a
jet at 360 kt (turning on 7.9 km) cannot help going 30% of the radius inside
before it is round. So the circle is held from the half-turn - in the orbit
trial, `fly-plan` and the server's orbit line (whose height #104 counts from
when the aircraft is level, from the quarter-turn) - and the join, from first
reaching the circle, to 35% inside and 20% outside (150 m either way at
least: the Cub's join is 100 m outside its 268 m). Measured (release): jets
27-30% inside, 12-15% outside; the Mosquito 18/5%; the C182 1/3%.

**Verified** (linux-debug, all four ways at each end - both ways round, calm
and a 10 kt wind - for every aircraft at its slowest and fastest): every jet's
circle at its fastest within 2% (worst 1.3%, the A380's 172 m at 300 kt; was
up to 18%), the others within 60 m; at the slowest every jet within 0.5%.
`plan-speeds` re-run (release) for the nine with a measured slowest: only the
787-8 moved, 198 to 193 kt (188 now holds; the one-step-past test found it).
Recorded plans by CLI and server, separation and selftest tests pass.
**Seen to fail**, each put back: the old 90 degrees a kilometre turned the
fighters' and airliners' fastest tests red (F-15C 7.7%, 787-8 10.2%); a
correction five times slower (sixty seconds) turned the light aeroplanes'
(C182 163 m) and the warbirds' (Mosquito 293 m) red; the join's bound set at
25% inside turned the fighters' red (27-30%). On Windows CI main's light-aeroplane
fastest test failed (the C182 at 144 kt, 215 m outside from the quarter-turn),
and on this branch still did: right, calm, 151 m inside and 145 outside from
the half-turn, its speed sagging to 139 kt, where Linux holds 5 m.

**The fastest a plan may ask now leaves power in hand.** The C182's 144 kt
was measured before #100 re-rated her engine; she now makes 143.7 kt level
at full throttle, and a speed held 5 kt short counted as held. So
`sim::holds_plan_speed` (and `plan-speeds`) now also asks that the aircraft's
full-throttle level speed - 3,000 ft, calm, until it settles - be at least
5 kt above the speed (`sim::full_throttle_level_kts`). `plan-speeds` re-run
for all sixteen; six fastests came down (level speed at full throttle in
brackets): C172P 120 to 110 (123.9), C182 144 to 129 (143.7), Cub 72 to 62
(74.8), PA-28 132 to 117 (129.1), S.23 156 to 141 (151.9), Mosquito 229 to
219 (231.0) - each 5 kt under the first that held, the command's rule. The
other ten printed what their files say. New test
`every_aircrafts_fastest_plan_speed_leaves_5_kt_in_hand_at_full_throttle`,
every aircraft; seen to fail with the C182 put back at 144.

**Twelve copilot recordings made again**, live (`GLIDESLOPE_LIVE_MODEL=1`),
since a plan's speed range is in every prompt: all twelve light-aeroplane
recordings in `tests/data/copilot`. Eleven passed as answered; the take-back
one answered `keep` three times running after the take-back, which tests
nothing, so its answer there is written in by hand as before, with its note
(the same route, BONDI at 100 kt). OpenAI's CBD plan was asked again until it took
off from 16R, as the runway-wreck test needs both planned aircraft to; and
Claude's now orbits 1,447 m at 100 kt, so the server-planned test runs
120,000 steps, not 84,000. Scanned for keys before committing: none.
#103 also re-records `leave-server-anthropic.jsonl`; whichever lands second
re-records it.

**The B-2's and F-22's yaw: the cause.** The rudder's integral lags the
sideslip a quarter cycle and undoes the yaw damper; a flying wing has no fin
to hide it (B-2 at 164 kt: 6 degrees either way every 7 s; with no integral,
within 0.1 at 159). A third the rate, counted within 2 degrees of the ball,
held the B-2 at 159 and the F-22 at 185 (it still swings in calm air below
170) - but spun the S.23 left thirty seconds in her stall lesson, so it is
not in.

The selftest's hash does not move, 30ac70b84cab7d7c.

**The B-2's and F-22's yaw: the cause.** Traced at 164 kt in the 20 kt
crosswind, the B-2 swings its sideslip 6 degrees either way every 7 seconds
with its rudder well inside its travel. The yaw damper's gain does not move
it (0.05 to 0.2 a degree a second: 6.1, 5.2, 6.6 degrees), nor the
sideslip's gain. The rudder's integral does: it lags the sideslip by a
quarter of a cycle, which is the damper's phase turned round, and at 0.05 a
second a degree on a swing of 0.9 radians a second it undoes more than the
damper gives; a flying wing, its yaw held almost wholly by its drag rudders,
has no stiffness of its own to hide it. With none, the B-2 at 159 kt holds
within 0.1 degree. At a third the rate, counted only within 2 degrees of the
ball, the B-2 held within 0.4 at 159, the F-22 at 185 (its orbit holds from
160, but it swings in calm air below 170), and every other aircraft held
its crosswind case. **It is not landed**: the same change sent the S.23,
left thirty seconds in its stall lesson, over into a spin (alpha -78 degrees
at the hand-over, 16 g, not recovered), and every stall lesson's figure
would want measuring again with it. The tail stays open with that named.

The selftest's hash does not move, 30ac70b84cab7d7c: it flies no plan, and the autopilot is as it was.
