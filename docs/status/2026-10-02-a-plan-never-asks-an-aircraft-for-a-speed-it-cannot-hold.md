### A plan never asks an aircraft for a speed it cannot hold clean, 2026-10-02 — tail done

**What a plan may ask was the approach speed to a fifth over the cruise, for
every aircraft.** The approach speed is a flaps-down figure (1.3 times the
landing stall, or a manual's own), and a plan is flown clean: round the
tightest orbit a plan may ask at it, five jets came down to the ground, the
B-2 stalled turning towards it, and the A320 lost 600 ft (#70). And a fifth
over the Mosquito's 220 kt start speed, 264 kt, is more than she makes at
3,000 ft. The 747-400 and the F-22 publish no stall speed, so have no
approach speed.

**Each aircraft's figures file now gives the speeds a plan may fly it at**,
`<plan_speeds slowest_kcas fastest_kcas>`, required in every file
(`sim::read_published_figures`, `sim::plan_speeds`), each with what it was
measured from. They are measured, not worked out, by a new command,
`glideslope_cli plan-speeds AIRCRAFT [FROM_KT | --every]`: the tightest
orbit a plan may ask at each speed (`sim/orbit_trial`, the orbit tests'
flight moved out of the test so that the tool and the tests fly the same
thing), flown clean by the navigator and autopilot at 3,000 ft over Sydney
from 2 km outside the circle, both ways round, in calm air and in a 10 kt
wind, each to hold its height within 50 ft and its speed within 5 kt from
its first quarter-turn; and a heading held on the autopilot alone in calm
air and a 20 kt crosswind, the crosswind test's flight (#87), sideslip
within a degree and heading within two - both now in
`sim::holds_plan_speed`, which the command, the crosswind test and the
one-step-past test all fly. The slowest is sought up from the approach speed
in 5 kt steps; the fastest down from a fifth over the start speed. **The
rule for the number written, built into the command, which prints it**:
where the approach speed (or a fifth over the start speed) held, it is the
number - nothing a plan was let fly before is taken away; where it did not,
the first speed that held, with 5 kt to spare. Every file's number is what
the command printed (all sixteen run again after the review, 2026-10-02).

| aircraft | approach | slowest | how | fastest |
|---|---|---|---|---|
| C172P, C182, Cub, PA-28 | 60, 64, 43, 64 | the same | held at the approach | 120, 144, 72, 132 |
| Mosquito, S.23, F-15C | 123, 86, 160 | the same | held at the approach | **229**, 156, 360 |
| 737-300 | 137 | **172** | 162 came down 500 ft before the circle; 167 held | 300 |
| 747-400 | none | **220** | swept from 160; 210 came down; 215 held | 300 |
| 787-8 | 148 | **198** | 188 lost 89 ft in wind; 193 held | 300 |
| A320 | 147 | **162** | 152 came down; 157 held | 300 |
| A380 | 136 | **201** | 191 came down; 196 held | 300 |
| B-2A | 124 | **194** | 154 lost 28 ft and 7 kt; 159 held the orbit; 189 the crosswind too - see below | 300 |
| F-35B | 159 | **204** | 194 lost 108 ft; 199 held | 360 |
| Learjet 35A | 125 | **140** | 130 came down; 135 held | 300 |
| F-22A | none | **255** | swept from 150; 215 lost 46 ft; 220 held the orbit; 250 the crosswind too - see below | 360 |

KCAS. The Mosquito levels at 231 kt at full throttle from 239 kt up; 234
held within 5 kt, so 229. "Came down" is 500 ft lost on the way to the
circle, where a sweep stops.

**The B-2 and the F-22 yaw in a crosswind when slow**, which the orbit does
not show and the crosswind test (#87) does, flown now at each aircraft's
slowest: at 164 kt the B-2 swung its sideslip 6 degrees either way in a 20
kt crosswind, and the F-22 at 225 kt 7. First found by hand-editing the
build's data and running that test; since the review the command flies the
crosswind at every speed it tries: the B-2 at 179 kt swung 1.5 degrees, at
184 held its sideslip within 0.9 but its heading only within 2.15, and held
both from 189; the F-22 swung 2.4 at 240 and 1.6 at 245 and held from 250.
So their slowest is that with 5 kt to spare, 194 and 255, more than the
orbit asks; the files say so. The B-2 at 184 misses its heading bound by
0.15 degree, which the one-step-past test leans on (below). The autopilot's yaw at those speeds is not
fixed: the tail "The B-2 and the F-22 yaw from side to side in a crosswind
when slow".

**Held to them, everywhere a plan comes from**:
- **the planner** (`copilot/planner.cpp`): the model is told "every airspeed
  from SLOWEST to FASTEST", and where the slowest is above the approach speed,
  "never slower than SLOWEST: a plan is flown clean, and the approach speed
  is for flaps down", with the least radius at the slowest and the fastest;
  `refusal` refuses a waypoint outside them and the refusal is told back.
  `slowest_planned_kts` never goes below the approach speed; with none given
  (the old tests) it is the approach speed and a fifth over the cruise.
- **the copilot's routes** (`copilot/copilot.cpp`): told the same in its
  situation, and `change_refusal` refuses a route outside them - which is
  the check the server makes of a `COPILOT_ROUTE` (#64/#83). Every brief is
  filled from the figures: the server's `learn_speeds`, the CLI's
  `fly-copilot`, the players' copilot.
- **plan files** (`sim::refuse_speeds_it_cannot_hold`): read by the server
  (its AI aircraft's plan), the client (`--plan`) and `glideslope_cli
  fly-plan`, a waypoint outside its aircraft's speeds refused as it is read.
  Both plans in the data are within them.
- `docs/TRANSPORT.md` says the route check is now the figures file's speeds.

**What is not done**: **the 747-400 and the F-22 can be flown on a plan
file but not planned by a model nor routed by a copilot** (done 2026-10-06, their take-off speeds measured) - their speeds
are in their files, but the planner and the copilot are given an approach
speed and a best-climb speed first (`sim::approach_speeds`,
`sim::departure_speeds`), which neither publishes, so the server gives
them no brief and plans neither: the tail "A model cannot plan the 747-400
or the F-22, nor a copilot route them". A copilot's **glide**, with the engine stopped, is
still allowed from the approach speed to the best climb, and whether a jet
glides round its tightest orbit clean at its approach speed is not measured
- the F-35B's best climb, 200 kt, is below its slowest under power, 204: the
tail "A glide may still be asked of a jet at its approach speed". And **at
their fastest the jets wander off their circles**, up to 18% of 13 to 19
km - the F-15C 3.3 km off its 18.8 km at 360 kt - their height and speed
held: the tail "At its fastest, a fast
aircraft's tightest orbit is flown well off its circle".

**Every speed between, measured**: `glideslope_cli plan-speeds ID --every`
flew each aircraft's tightest orbit all four ways every 5 kt from its
slowest to its fastest (linux-release), 357 speeds and 1,428 orbits, and
every one held its height within 50 ft and its speed within 5 kt:

| aircraft | speeds | worst height | worst speed | worst off its circle |
|---|---|---|---|---|
| 737-300 | 27, 172 to 300 | 5 ft | 0 kt | 10.8%, 1,407 m at 300 kt |
| 747-400 | 17, 220 to 300 | 14 | 1 | 11.0%, 1,429 m at 300 |
| 787-8 | 22, 198 to 300 | 14 | 1 | 11.6%, 1,509 m at 300 |
| A320 | 29, 162 to 300 | 3 | 0 | 10.9%, 1,425 m at 300 |
| A380 | 21, 201 to 300 | 6 | 0 | 13.3%, 1,731 m at 300 |
| B-2A | 22, 195 to 300 | 3 | 0 | 10.6%, 1,380 m at 300 |
| Learjet 35A | 33, 140 to 300 | 2 | 0 | 10.9%, 1,416 m at 300 |
| F-15C | 41, 160 to 360 | 3 | 0 | 17.8%, 3,331 m at 360 |
| F-22A | 22, 255 to 360 | 23 | 4 | 13.8%, 2,596 m at 360 |
| F-35B | 33, 204 to 360 | 4 | 0 | 14.1%, 2,653 m at 360 |
| Mosquito | 23, 123 to 229 | 2 | 0 | 3.4%, 258 m at 229 |
| Short S.23 | 15, 86 to 156 | 2 | 4 | 36 m at 86 |
| C172P | 13, 60 to 120 | 1 | 0 | 32 m at 60 |
| C182 | 17, 64 to 144 | 1 | 1 | 34 m at 64 |
| J-3 Cub | 7, 43 to 72 | 2 | 1 | 44 m at 43 |
| PA-28 | 15, 64 to 132 | 1 | 3 | 51 m at 64 |

The S.23's and PA-28's worst speeds are at their fastest, 156 and 132 kt,
which they make 4 and 3 kt short of at full throttle. The command's sweep
stops an orbit once it has lost its height or speed, so these are orbits
flown out; it is not a CI test - 1,428 orbits - but the two ends are,
below.

**The approach speed is rounded once, and all twelve recordings were made
again.** The Cessna's approach speed is 59.8 kt: the request said "at
least 518 m at 60 kt", the radius at 59.8, while the plan reader holds an
orbit at 60 kt to 521 m - so a model that took the words at their face
would be refused. Now the approach speed is rounded to whole knots once,
where a request or a brief is filled (the server's two, the CLI's `plan`
and `fly-copilot`, the players' copilot), and the slowest is the figures'
own, never below that; the planner, the copilot's routes and plan files
are held to one rule, `sim::within_plan_speeds` (half a knot either side,
in `sim/plan`, which the copilot may call - `cmake/Copilot.cmake` names
it), and the copilot test pins that an orbit at the radius told is taken
and a metre tighter refused. The Cessna's requests now say 521 m, so all
twelve recordings in `tests/data/copilot` (each the Cessna's) were made
again, asked of the same models with the owner's keys
(GLIDESLOPE_LIVE_MODEL=1), and every one plays back. Two needed more than
one asking: ChatGPT's hand-over plan once circled 1.2 km off its waypoint
and was asked again; and Claude, asked after its pilot took the aircraft
back, answered `keep` three times out of three - which leaves the rule
"the copilot stands by" untested, as before - so that answer's text was
written in as the route it was in the old recording, with the same note.

**Tests**:
- `every_aircraft_gives_the_speeds_a_plan_may_fly_it_at_none_beyond_what_plans_were_let_fly_before`:
  sixteen of sixteen; slowest never below the approach speed, fastest never
  over a fifth above the start speed; the two with no approach speed counted.
- `a_plan_file_asking_a_speed_its_aircraft_cannot_hold_clean_is_refused_and_none_in_the_data_does`:
  the A320 at 147, 161 and 301 kt refused, at 162 and 300 not, and started
  at 147; the Mosquito at 240 refused; the Cessna's plan flown in an A320
  refused. Every `.plan` file under assets/ and tests/ is walked: each is
  flown within its speeds but the one kept to be refused, named, which must
  be found; tests/data/copilot holds recordings, not plan files, whose plans
  the planner checks as each plays back.
- `a_plan_or_route_outside_the_speeds_its_aircraft_holds_clean_is_refused_and_the_model_told_them`:
  planner and copilot - told, refused at 70, 79, 121, 126, taken at 80
  and 120; none given is the old rule; an orbit at the radius told taken
  and a metre tighter refused; a slowest below the approach is the
  approach.
- `no_aircraft_holds_what_a_plan_asks_one_step_past_its_slowest_or_fastest`:
  every aircraft whose slowest is above its approach (or that has none),
  flown 10 kt below it, and the Mosquito 10 kt above its fastest - the last
  speed the command saw not hold - must not hold (`sim::holds_plan_speed`).
  A slowest written too high, or by hand, is caught; one too low, by the
  slowest-orbit tests. 10 of the 32 bounds tried, the other 22 at the bound
  plans were held to before. About 11 s.
- `the_speeds_a_plan_may_fly_an_aircraft_at_are_read_and_refused_where_they_are_wrong`:
  the C172P's file with its `<plan_speeds>` taken out, missing either
  speed, a slowest of 0, the fastest not above the slowest (equal and
  below), and given twice - each refused, saying why; other speeds read
  back as written.
- `the_client_refuses_a_plan_for_another_aircraft_than_the_one_it_flies`:
  `glideslope --headless --aircraft a320 --plan sydney-harbour` exits 1,
  "the plan is for the c172p, and the aircraft flown is the a320", before
  anything is drawn.
- `the_cli_refuses_plan_speeds_...`: from a speed that is not a number, one
  off its scale, and for an aircraft it does not hold.
- `the_cli_refuses_a_plan_asking_a_speed_its_aircraft_cannot_hold_clean`:
  `fly-plan` of `tests/data/plans/a320-at-its-approach-speed.plan` exits 1
  saying "CBD is flown at 147 kt, outside 162 to 300 kt".
- The orbit tests (#70), renamed for what they fly:
  `the_tightest_orbit_at_the_slowest_speed_a_plan_may_ask_is_flown_by_every_...`
  and, new, `..._at_the_fastest_speed_...`, four groups each, **nothing left
  out** - the 747-400, F-22 and the five jets flown with the rest. Each
  orbit holds its height within 50 ft and speed within 5 kt; the circle
  within 60 m for the light aeroplanes at both ends and the warbird and
  flying boat at their slowest, 5% of the radius for the jets at their
  slowest (the F-22 4.5%, 427 m off its 9.4 km at 255 kt, the worst), and
  at the fastest 15% of the radius for the airliners and business jet
  (the A380 13.3%), 20% for the fighters and bomber (the F-15C 17.8%) and
  5% for the warbird and flying boat, from the table
  above.
- The crosswind test for every aircraft but the light ones (#87) flies each
  at its slowest instead of its approach speed, and leaves nothing out: 48
  of 48 cases.

**Seen to fail**, each then restored:
- the planner's helpers back to the approach speed and a fifth over the
  cruise: the copilot test, "the request says every airspeed from 80 to 120
  kt";
- the route check back to them: the copilot test, "a route at 70 kt is
  refused";
- the plan-file check made to pass everything: the plan-file test, "the
  A320 at 147 kt is refused";
- the A320's slowest put back to its approach speed, 147, in the build's
  data: the airliners' slowest-orbit test, losing 598 to 643 ft;
- the B-2's and F-22's slowest at 164 and 225 (the orbit's numbers): the
  crosswind test, as above, and the fighters' slowest-orbit test, the F-22
  slowing to 219 kt round its circle at 225;
- the A320's slowest written 172, too high, in the build's data: the
  one-step-past test, "a320 holds 162 kt, 10 past its slowest";
- the client checking the plan against its own aircraft, as it did: the
  client test (the A320 flown on the Cessna's plan).

**After the review** (linux-release, no display): the sixteen swept again
by the command, each printing the number in its file (the B-2's 195 became
194); the thirteen orbit, crosswind, speeds and one-step-past tests pass,
11 s for the new one; 56 played-back, copilot, planned, hand-over,
take-over, take-back, navigator, figures and refusal tests pass (one,
refused its port while the others ran, passed alone). The nightly
workflow gains a job, "Every speed a plan may ask of every aircraft",
running `plan-speeds --every` for all sixteen in linux-release, which the
tick rests on.

**Verified** before the review in linux-release (ctest -j4, no display, other working
copies' builds and sweeps running beside it): 124 tests chosen by name -
every orbit, navigator, autopilot, plan, figures, copilot, recorded,
planned, hand-over and crosswind test - passed but the five fixed above
(the fighters' slowest circle, held to 300 m before it was 5%, and the four
played-back ones), and those, with every played-back, copilot, take-over,
take-back and planned test, 52 in all, then passed; 12 live-model tests
skipped, needing a key and GLIDESLOPE_LIVE_MODEL. The twelve orbit,
crosswind, speeds and selftest tests again on the final build: all passed,
the slowest orbits 7 to 25 s each and the fastest 11 to 41 s. The selftest
hash does not move, `d36123c1eecc3e23`: it flies a logged input, and no
model or autopilot law changed. No platform code was touched, so no Windows
build.
