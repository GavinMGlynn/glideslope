### The autopilot has a stall recovery; four of fourteen are within their lesson, 2026-09-26 — tail still open

**Only part of the tail is done.**
- **Every aeroplane is recovered.** All fourteen aeroplanes taught a stall,
  left thirty seconds in one, are pulled out by the autopilot's new stall
  recovery, and stay recovered for five seconds.
- **Only four are within their lesson's height**, with the stated tolerance in
  hand: the C172P, C182, Cub and PA-28.
- **Two pull out harder than 2 g**: the A320 and the Mosquito.
- **Nothing in the game can reach the recovery yet.** It is a mode the
  autopilot flies when asked (`AutopilotModes::speed_on_elevator`), and only
  the stall tests ask. The AI pilot never notices a stall, and a flight plan
  cannot ask for it. The instructor's stall demonstration
  (`demonstrate_a_stall`) still asks for a descent, which serves from the
  approach to the stall; from deep in one it is the law that flew the B-2A
  into the ground.
- **Still to do:**
  - the instructor's demonstration to use the recovery;
  - a test that engaging the mode and letting it go step nothing;
  - the ten aeroplanes outside their lesson's height, and the two outside 2 g.

**Cause.** The recovery the stall tests fly asked the autopilot for a vertical
speed. An aeroplane mushing in a stall sinks faster than that, so the vertical
speed loop raised the nose, which held it in the stall. The B-2A was handed
over at 96 kt, 12.5° nose up, 30° of alpha, sinking 4,100 ft/min. It stayed
there, with the pitch loop at its 15° stop, from 19,700 ft to the ground.

**Also found, and fixed:** the stall tests called the autopilot twice per
physics step while recovering, so its loops ran at 240 Hz. This was already
so on main, for the by-the-book stall too. It is now called once a step,
with the modes set before the call. Every figure here was measured after
that fix.

**The stall recovery** (`src/sim/autopilot.cpp`). It is off by default, and
nothing else the autopilot flies changes.
- **The speed on the elevator, at full power.** The pitch wanted is an
  integral, less 0.5° for each knot short of the speed, plus 0.5° for each
  kt/s the speed is gaining. This follows the FAA's stall recovery
  (FAA-H-8083-3C, chapter 5): reduce the angle of attack first, whatever the
  height is doing.
- **The wing is kept below the angle it stalled at.** The pitch may be no more
  than θ − (α − α_stall).
  - α_stall is learned by watching: it is the angle of attack at the greatest
    lift coefficient seen since the flaps or gear last moved.
  - Before the aeroplane has flown past its lift peak, α_stall is lower than
    the real one and the recovery over-unloads the wing: safe, at the cost of
    height. `plan.hpp` says so.
  - An autopilot engaged on an aeroplane already stalled learns the peak as
    the nose comes down and the lift rises back through it.
  - Without the limit, an A320 diving out of a stall at 156 kt, already past
    its speed, was held at 27° of alpha with the elevator full up.
- **Below the −10° pitch floor only while recovering**, meaning short of the
  speed or stalled. It goes as far as θ − α (the flight path, wings level) or
  the wing's limit, and never below −30°. That bound still holds after the g
  limit. **It knows nothing of the ground**, and `plan.hpp` says so.
- **The nose goes down at 8°/s and comes up at 3°/s.**
- **The pull-out is held under 1.6 g.** Past that, the nose goes 10° below
  where it is for each g over.
- **The integral winds only while the law's pitch is the pitch given.**
- **It captures no height.** The caller clears it.

**Recovered means flying again, for five seconds together.** All three of
these must hold at once, counted in steps, with the lowest height taken
throughout:
- the angle of attack is below the one its lift peaked at, measured from the
  entry to the hand-over;
- it is level (sinking under 100 ft/min) or climbing;
- it is at or above its lesson's recovery speed.

The five seconds are there so that the top of a zoom climb is not called a
recovery. The height lost is counted from the hand-over to the lowest point.
The first version counted the speed alone, and called an A320 recovered after
1 ft at 27° of alpha, sinking 10,000 ft/min.

**One tolerance for every aeroplane, height and load alike: 10%.** An
aeroplane is held to its lesson's height, or to 2 g (the flaps-down limit load,
14 CFR 23.345 and 25.345), only with that much in hand: its figure plus 10%
must be within the limit. One without it is named in the test and held to its
own figure plus 10%. The test prints every aeroplane's figures, but CI's
ctest shows a test's output only when it fails (`outputOnFailure`), so a
green run on macOS, Windows or Rocky does not show them. The 10% is a
judgement, not yet checked against those platforms' own figures; a platform
that differs by more turns the test red and prints them.

**Verified** by `every_aeroplane_left_thirty_seconds_in_a_stall_is_recovered_within_its_lessons_height_or_a_named_bound`.
- **Coverage, asserted by the test:** 16 aeroplanes; 14 flown; 2 left out (the
  747-400 and F-22 have no published stall speed).
- **Height:** 4 held to their lesson's height, 10 to a named bound.
- **Load:** 12 held to 2 g, 2 to a named bound.
- **Stale entries turn it red:** a name that no longer meets its limit with
  the tolerance in hand, or that names an aeroplane not flown.

| Aeroplane | Stalled at α | Handed over | Lost (ft) | Lesson (ft) | Held to (ft) | Peak g, 0.25 s mean | Held to (g) |
|---|---|---|---|---|---|---|---|
| 737-300 | 17.2° | 119 kt, α 36°, −7,670 ft/min | 1,314 | 1,300 | 1,450 | 1.78 | 2 |
| 787-8 | 13.8° | 116 kt, α 13°, −2,850 | 1,536 | 1,300 | 1,690 | 1.34 | 2 |
| A320 | 16.0° | 156 kt, α 27°, −10,080 | 1,535 | 1,300 | 1,690 | 2.18 | 2.40 |
| A380 | 14.3° | 123 kt, α 41°, −9,170 | 1,754 | 1,300 | 1,930 | 1.78 | 2 |
| B-2A | 14.9° | 96 kt, α 30°, −4,100 | 921 | 300 | 1,020 | 1.64 | 2 |
| C172P | 16.0° | 46 kt, α 16°, −640 | 113 | 300 | lesson | 1.08 | 2 |
| C182 | 16.0° | 63 kt, α 25°, −1,150 | 73 | 300 | lesson | 1.51 | 2 |
| F-15C | 18.5° | 154 kt, α 18°, −7,380 | 1,068 | 500 | 1,180 | 1.68 | 2 |
| F-35B | 31.5° | 127 kt, α 65°, −12,780 | 3,018 | 500 | 3,320 | 1.68 | 2 |
| J-3 Cub | 12.5° | 39 kt, α 17°, −1,040 | 54 | 300 | lesson | 1.43 | 2 |
| Learjet 35A | 14.0° | 103 kt, α 14°, −3,140 | 769 | 350 | 850 | 1.21 | 2 |
| Mosquito FB.VI | 12.0° | 128 kt, α 25°, −10,360 | 1,332 | 600 | 1,470 | 2.23 | 2.46 |
| PA-28 | 16.0° | 53 kt, α 24°, −890 | 205 | 300 | lesson | 1.13 | 2 |
| Short S.23 | 16.8° | 63 kt, α 12°, −750 | 264 | 200 | 300 | 1.63 | 2 |

**Why ten are outside their lesson.** Each lesson's height was set for
recovering from the approach to the stall. Handed over thirty seconds in, most
of these are sinking 3,000 to 12,800 ft/min, and stopping that within 2 g takes
the height. The B-2A shows it most plainly:
- it is handed over at 96 kt and must reach stall + 35, about 128 kt;
- at 19,700 ft that is roughly 128 to 170 kt true airspeed, about 550 ft of
  height by energy alone, before its sink is stopped;
- that is against its lesson's 300 ft.

**Also verified** by `an_autopilot_engaged_on_an_aeroplane_already_stalled_recovers_it`.
The same fourteen flights are each handed at the hand-over to a new autopilot,
one that has seen nothing of the aeroplane, and all fourteen are recovered.
Height and load are the other test's to judge; the A320 pulls 3.14 g this way,
and they are printed.

**Seen to fail.**
- **With the recovery flown the old way** (the mode off), the main test went
  red:
  - 11 were never recovered;
  - the A320 lost 2,071 ft against its bound of 1,690 and pulled 3.15 g;
  - the F-15C pulled 1.95 g, without the 10% in hand against 2 g.
- **With the g limit removed** (set to 16 g), it went red on load: the 737-300
  pulled 1.82 g, the F-15C 2.03 and the F-35B 1.95, none with 10% in hand.
- **With the C172P named** (113 ft against its lesson's 300), the stale check
  went red: "named for its height and now meets 300 with the tolerance in
  hand".
- **The new-autopilot test, with its engage seeded 35° nose up**, went red:
  the C172P, C182, Cub, Learjet and PA-28 were not recovered.
  - Neither turning off the learning of α_stall nor removing the wing limit
    turned that test red: the speed law recovers them without it. The
    learning's effect shows in the main test instead. With the learning off,
    the angle stays at the one seen when the autopilot was first engaged, the
    wing is over-unloaded, and the 737-300 lost 6,293 ft and the C172P 614.
- **Every one was put back, and all pass.**

**The stall lessons flown by the book still pass.** Now at 120 Hz; "before" is
main, at its 240 Hz:

| Aeroplane | Before (ft) | Now (ft) | Allowed (ft) |
|---|---|---|---|
| 737-300 | 344 | 319 | 1,300 |
| 787-8 | 420 | 418 | 1,300 |
| A320 | 321 | 320 | 1,300 |
| A380 | 394 | 354 | 1,300 |
| B-2A | 53 | 100 | 300 |
| C172P | 93 | 103 | 300 |
| C182 | 117 | 143 | 300 |
| F-15C | 25 | 85 | 500 |
| F-35B | 252 | 244 | 500 |
| J-3 Cub | 23 | 34 | 300 |
| Learjet 35A | 241 | 249 | 350 |
| Mosquito FB.VI | 256 | 247 | 600 |
| PA-28 | 151 | 155 | 300 |
| Short S.23 | 50 | 80 | 200 |

The stall lessons' comments quote the new figures.
`a_stall_recovered_badly_is_named_in_the_debrief` and
`an_instructor_demonstrates_a_stall_and_hands_it_over` still pass.

**The selftest hash is unchanged**: `d36123c1eecc3e23`, in linux-release. The
selftest replays a pilot's inputs and does not fly the autopilot.

**The whole suite, linux-debug (sanitized), `ctest -j4`: 515 of 515 passed**,
with the usual four skipped. The network tests on fixed ports were left out
with `-E "no_step_at|hold_at_(100|200)_ms|server_|client_on_server"`,
because another session was running them on the same ports; CI runs them.
