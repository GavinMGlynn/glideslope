### In gusts a propeller aeroplane's flare raises her nose faster the slower she flies: the C172P and C182 land on every seed, 2026-11-03 - item still open

**What is still missing, first**: the item. Six of sixteen are still named
in `not_yet_landed_in_gusts()` (tests/unit/test_lesson.cpp), the J-3 Cub
among them: on forty seeds a gust lifts her at 76 to 158 ft/min in her
flare on eight (seeds 9, 19, 20, 24, 37, 38, 39, 40 on Linux; six on
Windows), floating level a few feet up on the half of her approach power
the gusty flare holds. Her hard touches and her rise after touching are
gone (worst 235 ft/min, was 356). **Made worse**: on Windows release the
Mosquito, judged, strikes her airframe on seed 40 of forty, which she
passed before (on Linux she passes all forty, worst 203, was 267); and the
Cub passes 32 of forty on Linux (was 35), 34 on Windows (was 37). Not yet
seen on macOS: the C172P was named for macOS release's seed 3 (346 ft/min).

**What now works**: the C172P and C182 are judged, unnamed: ten of sixteen
land in moderate turbulence within limits with every margin on all eight
gust seeds. Jets are untouched (the change is a propeller aeroplane's in
gusts alone), and so is calm air.

**What the AI was doing** (linux-release, traced): the C182 on seed 23 lost
her headwind five feet up, 70 kt to 55; her flare asked for 9.1 degrees of
nose and she had 5.7 at the touch, her sink grown from 210 to 390 ft/min,
and she met the runway at 377. The flare's attitude asked rose at its 6
degrees a second, but the pitch loop damps any nose-up rate past a degree a
second (`want_q_degps`, tuned on 2026-11-02 for every aeroplane), and a
slow aeroplane needs more nose for the same sink: the change of path that
arrests a gust's sink is that sink over her speed.

**The published technique**: the FAA's Airplane Flying Handbook
(FAA-H-8083-3C), ch. 9, "Round Out", p. 9-8, as printed: "The round out
rate should also be proportional to the rate of closure with the ground",
and "the pitch attitude should be increased at a faster rate when full
flaps are used". (Read from the chapter as faasafety.gov hosts it.)

**The change** (src/sim/lander.cpp, `gusty_nose_rate_kt_degps`): in a
gusty flare a propeller aeroplane's undamped nose-up rate is 265 knot-degrees
a second over her approach speed, never less than a degree a second: 3.7
degrees a second for a C182 at 72 kt, 5.2 for a Cub at 51, 2.4 for the
Mosquito. The 265 is tuned, not sourced (about a quarter of a g of change in
her climb). A jet keeps a degree a second.

**Tried and measured** (forty seeds, passing every margin; linux-release
unless said):

| Variant | C172P | C182 | Cub | Worse |
|---|---|---|---|---|
| As it was | 40 (327) | 38 (377) | 35 | - |
| Throttle opened at 1/s, not 0.5 | 40 | 38 | 35 | - |
| Cushion from 75 ft/min, not 150 | 40 | 38 | 34 | - |
| Touch flown at 100 ft/min, not 150 | 39 | 40 | 33 | - |
| Speed held to the approach speed | 40 | 38 | 34 | - |
| Nose rate 3 deg/s, every aeroplane | 40 | 39 | 34 | Mosquito 37, A380, F-15C 35, 747 38 |
| Nose rate 6 deg/s, every aeroplane | 39 | 40 | 33 | Mosquito 3, F-15C 14, F-35B 24 |
| 250 kt-deg/s over speed, every aeroplane | 40 | 39 (338) | 32 | - |
| 265, every aeroplane | 40 (248) | 40 (331) | 32 | Windows: F-15C 39, F-35B 38, Mosquito 39 |
| 300, every aeroplane | 39 | 40 | 33 | F-35B 39, A380 35 |
| **265, propeller aeroplanes (taken)** | **40 (248)** | **40 (331)** | **32** | **Windows: Mosquito 39** |

At 265 for every aeroplane the 737-300 passed 39 of forty on Linux and 39
on Windows (was 36 there), the A380 32 on Windows (was 30), but the F-15C
and F-35B floated to within 25 m of the zone's end on Windows on seeds they
had passed (32; 13, 19): not taken for jets.

**Forty seeds, the propeller aeroplanes, after** (pass with every margin;
worst touch):

| Aircraft | linux-release | x86-64-v2 | Windows release | Windows before |
|---|---|---|---|---|
| C172P | 40 (248) | 40 (248) | 40 (268) | 38 (374) |
| C182 | 40 (331) | 40 (331) | 40 (303) | 40 (312) |
| J-3 Cub | 32 | 32 | 34 | 37 |
| Mosquito | 40 (203) | 40 (203) | 39 (seed 40 struck) | 40 |
| PA-28 | 40 (311) | 40 (311) | 40 (298) | 40 (307) |

The C182's worst, 331 on seed 21, is 4 ft/min inside the judged 335 on
Linux.

**Verification**: `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1`
to `_8` pass in linux-release; least margins of those judged over the eight:
sink 28 ft/min (seed 7), zone 169 m, rise 0.66 ft, flare climb 76 ft/min.
The forty-seed sweeps above were flown with a scratch test (not committed)
on linux-release, an `-march=x86-64-v2` build (no FMA, as CI) and Windows
release built by tools/windows_build.sh. The full suite was not run here
(the pre-push hook's quick tests were); CI's runs are the record.
