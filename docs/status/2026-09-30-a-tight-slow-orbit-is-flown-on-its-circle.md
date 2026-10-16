### A tight, slow orbit is flown on its circle, 2026-09-30 — tail done

**Not every orbit a plan allows can be flown.** A plan may fly any aircraft
as slowly as its approach speed, and round a circle as tight as two and a
half times its 25-degree turn at that speed. Seven jets cannot fly that
clean, and nothing refuses such a plan yet: the new tail "A plan may fly a
jet clean at its approach speed". The refusal belongs in the copilot's
planner (`copilot/planner.cpp`, which already knows each aircraft's approach
speed): a clean floor for each aircraft, below which no plan may fly.

**What is not covered first.** Of the sixteen aircraft, each left out is
named in its test with the reason:
- **The 747-400 and the F-22 publish no stall speed**, so they have no
  approach speed and no plan can be made for them.
- **Seven jets cannot fly clean at their approach speed round a turn.** The
  approach speed is a flaps-down figure (1.3 times the landing stall), and
  the autopilot flies a plan clean.
  - The 737-300 stalls turning onto the circle and falls to the ground.
  - The 787-8, A380, Learjet 35A and F-35B come down to the ground round it
    (the Learjet slowed from 125 to 113 kt on the way).
  - The B-2 stalls turning towards the circle: its throttle closes, its
    angle of attack is 64 degrees within 15 s, and it falls inverted to the
    ground 25 s after starting. That is energy, not steering: it never
    reaches the circle.
  - The A320 stays up and holds its circle, -6 to +96 m, but loses 600 to
    641 ft: flown, with its circle held to 200 m and its height and speed
    named as not held.
- **The Cub and the Cherokee in wind.** Measured with no plan and no
  navigator - the autopilot holding heading north in a crosswind from the
  west - and then on a plain waypoint leg, the old drift path:
  - J-3 Cub, 10 kt: sideslip -36 to +34 degrees holding a heading, -35 to +36
    on the leg, its heading swinging 230 degrees while the heading asked
    stays at 0.
  - PA-28, 10 kt: sideslip -37 to +37 holding a heading and on the leg. The
    same at 20 kt.
  - C172P and C182, 10 and 20 kt: sideslip within 0.1 degree; each holds its
    heading, and on the leg crabs a steady 9 (C172P) or 8.5 degrees at 10 kt.
  So the yaw is the autopilot's with those two models, not this change, and
  their orbits in wind are left out: the tail "In wind the Cub and the
  Cherokee yaw from side to side". In calm air both are flown.
The wind in the new tests is 10 kt, not the 20 kt of the older orbit tests:
at 43 kt, the Cub's approach speed, 20 kt downwind needs about 25 degrees of
bank round its tightest circle, all the autopilot has. The plan reader's least radius
takes no account of the wind.

**The cause.** Round an orbit the navigator asked for a heading along the
circle's tangent where the aircraft would be five seconds on, turned in by 90
degrees a kilometre. In a steady turn the autopilot banks a degree for each
degree the heading is ahead, plus its integral near the heading, which winds
to 5 degrees: so the lead the circle needs is its bank less five. At the
tightest radius a plan may ask - two and a half times the 25-degree circle -
the bank is about 11.5 degrees whatever the speed, so the lead needed is
about 6.5 degrees. But five seconds along the tangent is v t / r, which grows
as the speed falls: 18 degrees for the Cessna's 521 m at 60 kt, 12 for
1,172 m at 90 kt. Turning out by 90 degrees a kilometre balanced the extra:
124 m inside, as flown. And flown to straight at its centre, the aircraft
crossed the circle at right angles and could not turn onto it: the PA-28 in
wind went to 57 m from the centre of its 593 m circle.

**The fix** (`sim/navigator.cpp`, `sim::heading_off_for_bank_deg` in
`sim/autopilot`). For every aircraft that can fly its approach speed clean,
the physics allows it: every tightest orbit wants about 11.5 degrees of bank,
well within the autopilot's 25. So the plan reader's least radius stays, and
no recorded plan is refused.
- **The lead is the autopilot's own law turned round**: the heading ahead
  that makes it bank for the circle now. Round a circle over the ground the
  track turns at the ground speed over the radius, and the air is turned
  through that at the airspeed: tan bank = v_air v_ground / g r. The
  inversion assumes the autopilot's integral at its 5-degree limit, which it
  reaches only while the heading is within 10 degrees of it - so below about
  15 degrees of bank - as the function says.
- **A trim** - an integral on how far off the circle, a sixtieth of the
  turn-in each second, 15 degrees at most, counted only within 100 m of the
  circle - for what the autopilot's bank and the aeroplane's turn do not
  quite agree on: a propeller turns one way more readily than the other.
- **Joined at a tangent.** Outside the circle and not yet on it, the
  aircraft flies the line that meets the circle at a tangent, the way round
  it is flown; it is on the circle within 100 m of it, either side, where
  before it had to cross into it. Begun from inside, it spirals out as
  before.
- **The wind by the wind triangle.** Round an orbit the drift changes all the
  way round, and five seconds' average of it lags. The wind does not change:
  where the aircraft goes over the ground, less where the air carries it -
  its body's air-relative velocity turned through its attitude, so its
  sideslip and its climb through the air count. It is averaged over five
  seconds, and taken as it is when the navigator starts or begins again
  (`begin_here`, after a take-off), not from calm. The heading is the track
  turned into the wind across it, at the horizontal airspeed. The legs
  between waypoints still use the drift.
The selftest's hash does not move: the selftest flies no plan, and the
autopilot's loops are unchanged (the new function only reads its constants),
which `the_selftest_prints_the_same_hash_every_run` confirms.

**Verified** (linux-debug; metres off the circle from the first
quarter-turn, the worst of calm air and a 10 kt wind from the west, left and
right):

| aircraft, radius at approach speed | before | after |
|---|---|---|
| Cessna 172P, 521 m at 60 kt | -152 to +92 m | -32 to +23 m |
| Cessna 182, 593 m at 64 kt | -160 to +124 | -35 to +25 |
| Piper Cub, 268 m at 43 kt, calm | -124 to +55 | -19 to +36 |
| PA-28, 593 m at 64 kt, calm | -105 to +210 | -31 to +50 |
| Mosquito FB.6, 2,189 m at 123 kt | not measured | -32 to +33 |
| Short S.23, 1,071 m at 86 kt | not measured | -36 to +4 |
| F-15C, 5,559 m at 196 kt | -253 to +936 (calm, left) | -88 to +196 |
| A320, 3,127 m at 147 kt (circle only) | not measured | -6 to +96 |

Four tests, splitting the catalogue by class so that none flies long on CI;
the first checks that the four groups take each of the seven classes once.
Each says how big its space is, and fails unless what it flew and what it
named are all of it, or on a named aircraft its space does not hold. Each
orbit flown must go twice round and on, hold its circle, and - unless named
- its height within 50 ft and its speed within 5 kt.
- `the_tightest_orbit_at_the_approach_speed_is_flown_on_its_circle_by_every_light_aeroplane`:
  12 of 16 orbits flown, the Cub's and the Cherokee's four in wind named;
  held to 60 m, at worst 50. 99 s.
- `..._by_every_airliner_and_business_jet_that_can_fly_it`: 4 of 24, the
  A320's with height and speed named; the 747 and five jets left out. Held
  to 200 m, at worst 96. 70 s.
- `..._by_every_fighter_and_bomber_that_can_fly_it`: 4 of 16, the F-15C's;
  the F-22, F-35B and B-2 left out. Held to 200 m - the F-15C's 5.6 km
  circle is 196 m off at worst, 3.5% of it. 89 s.
- `..._by_every_warbird_and_flying_boat`: 8 of 8, held to 60 m, at worst 36.
  148 s.
- Timings are linux-debug under a -j4 run here. On CI, before the split, the
  one test for every aircraft but the light ones took 57 s and 67 s on two
  runners and 425 s on a third (run 36702902558): the split is for that.
- **Claude's CBD plan**, 521 m at 60 kt: replayed by `glideslope_cli
  fly-plan`, 509 to 521 m (394 to 427 before); on the server, 509 to 521 m
  (394 to 428). ChatGPT's 1,447 m: 1,448 to 1,475 m (1,451 to 1,454). The
  hand-written `sydney-cbd-orbit.plan`, 1,500 m: 1,476 to 1,502 m.
- **The allowances**:
  - the replay tests and the server's planned-aircraft test come down from
    150 m to 60;
  - the older orbit test flown from outside, from 160 m to 80: at worst 47 m
    now, where it was 150 m outside;
  - the test begun from the centre, from 160 m to 110. That one is not a
    check on this change: the old navigator flew it within 69 m (62 in
    wind), and the new one within 88 m (+88 in a 20 kt wind, right) - its
    trim starts from nothing where it joins from inside. The old navigator
    passes it at 110 m, seen.

**Seen to fail**, the old navigator put back, each then restored:
- the light-aeroplane test: "on its circle within 60 m, c172p round 521 m at
  60 kt, left, in calm air: 391 to 516 m";
- the test for every aircraft but the light ones, before it was split:
  "within 200 m, f15c ... 5306 to 6495 m";
- Claude's replay: "round CBD from 394 to 427 m, off its 521 m circle by
  more than 60 m";
- the server's planned-aircraft test: "more than 60 m";
- the older orbit test at 80 m: "1172 m, left, in a 20 kt wind: 1081 to
  1322 m".
Not seen to fail: the begun-from-centre test at 110 m, as said above.
