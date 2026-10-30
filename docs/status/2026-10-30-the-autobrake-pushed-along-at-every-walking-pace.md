### The autobrake pushed along at every pace to walking pace for half a minute, and no idle allowance for a tail wheel, 2026-10-30 - item done

The Phase 10b item "The rollout's autobrake winds up when there is nothing
to slow" is now ticked. On 2026-10-29 (#271) two parts were still missing:

- no landplane rolled for longer than 2.2 s of the half minute;
- no jet reached the 0.8 idle-creep allowance.

Both are now closed.

**The situation, built explicitly: a push** (tests/unit/test_lesson.cpp,
`PushedData`).
- **What it is**: an external force (JSBSim's `<external_reactions>`, the
  way FlightGear's tugs push a JSBSim aircraft).
- **Where it lives**: the test makes a copy of the data, adds the force there,
  and deletes the copy afterwards. No committed flight model is changed.
- **How it acts**: along the runway and level (JSBSim's LOCAL frame), at her
  centre of gravity, which it follows every step. The push therefore pitches
  her not at all. What pitches her is the brake at her wheels, as a
  deceleration of that size would.
- **How strong it is**: a proportional-integral hand on her groundspeed,
  inside JSBSim's own flight control system (`<pid>`). Its gains are 0.5 of
  her weight for each ft/s she is short, and 1.0 of her weight per ft/s per
  second. It never pulls.
- **When it acts**: only during the 30 s she is held. Its integral is reset
  to zero outside that window.
- **Why a push**:
  - Set rolling once, she stops on her brakes within 2.2 s.
  - Held by her throttle, she is disarmed, as an advanced throttle should
    disarm her.
  - Her speed set every step re-initialises JSBSim, and the run blew up.
  - A push is neither her brakes nor her throttle.

**What it found**: the windup. Pushed at 1 to 2 kt with nothing to slow,
every landplane's autobrake went to the 0.8 idle allowance. The nose-wheel
aeroplanes held at that brake. The tail-wheel aeroplanes went over, dry, wet
and in 15 kt across:
- the J-3 Cub went to -7.8 and -12.5 degrees, and over (-44.5);
- the Mosquito struck her airframe, at -5.5 to -6.5 degrees.

**The fix** (`sim::Lander`, lander.cpp): **a tail-wheel aeroplane gets no
idle-creep allowance**, and stands at the 0.45 standing ceiling.
- **Source**: the FAA's Airplane Flying Handbook (FAA-H-8083-3C), chapter 14,
  tailwheel airplanes: brakes applied smoothly, with the tail kept down.
  - https://www.faa.gov/sites/faa.gov/files/regulations_policies/handbooks_manuals/aviation/airplane_handbook/15_afh_ch14.pdf
- **Second source**: Aviation Safety, "Braking Tactics". Letting a tailwheel
  aeroplane creep forward under power and then braking suddenly is how one
  is tipped over.
  - https://aviationsafetymagazine.com/?p=10861
- **Why this loses nothing**: in none of the 351 runs of 2026-10-29 did a
  tail-wheel aeroplane's idle thrust roll her against 0.45.
- **What does not change**: the nose-wheel aeroplanes, and the jets with
  them, keep the 0.8 allowance.

**The tests**: the three walking-pace tests (dry, wet, 15 kt across), now
pushed.
- **Coverage**: 13 landplanes by 9 paces, 0.5 to 4.5 kt (5 kt named). The
  flying boat is named.
- **New assertions**:
  - every run rolls for all 30 s, at 0.25 kt or more;
  - every run is held within 0.4 kt of her pace from 5 s on;
  - every nose-wheel landplane (11) at each creep pace (1 and 1.5 kt), 22
    runs, has her autobrake at 0.8. The one alternative allowed is the
    rollout's guard on her nose going past -4 degrees letting it off first.
    That is counted and printed. It happens to the C182, dry and across, at
    0.67 to 0.73, nose -4.0 to -4.1 degrees.
  - 2 tail-wheel landplanes.
- **Judged every step**, as before: the brake bound at her speed (0.8 only
  for a nose-wheel aeroplane creeping at idle), nose above -5 degrees, wings
  within 15, wheels only, turn within 30 degrees, and stopped afterwards.
- **Figures (linux-debug, this FMA build)**:

  | Runway | Rolling | Off her pace | Push, of her weight | Autobrake at most | At 0.8, creep runs | Least pitch | Most roll | Most turn |
  | --- | --- | --- | --- | --- | --- | --- | --- | --- |
  | Dry | 117 of 117, 30 s | 0.20 kt | 0.42 | 0.800 | 22 of 22 (2 let off) | -4.1° | 0.1° | 5.2° |
  | Wet | 117 of 117, 30 s | 0.17 kt | 0.37 | 0.800 | 22 of 22 | -4.0° | 0.1° | 5.2° |
  | 15 kt across | 117 of 117, 30 s | 0.21 kt | 0.42 | 0.800 | 22 of 22 (2 let off) | -4.0° | 1.8° | 6.8° |

  - Every jet reaches 0.8 at 1 and 1.5 kt, its nose at -1.6 degrees or
    higher (B-2A).
  - The Cub and the Mosquito stand at 0.45, their noses at +11.2 degrees or
    higher.
  - **Margin**: 0.9 degrees, the C182 to -5. The guard at -4 lets her brake
    off before the bound.

**Seen red** (linux-debug, dry):
- **With the push's time set to 0**: all 117 runs fail. "Rolled only 0.33 s
  of 30" is the first.
- **With the tail-wheel exclusion taken out**: 19 failures, the Cub and the
  Mosquito at 1 to 2 kt:
  - the Cub over at 2 kt, -44.5 degrees;
  - the Mosquito's airframe struck, -6.5 degrees;
  - the autobrake wound to 0.800 at the tail-wheel ceiling of 0.45.

Both were reverted, and all three tests pass again.

**Other tests run**: `ctest -R` over the walking-pace, autobrake, gust,
taken-back, steady-wind, crosswind, J-3 Cub, anti-skid, wet-short-runway,
touch-and-go and taxi-off tests, plus the selftest and hash tests. 103 of
103 passed (linux-debug). The full suite was not run here.

**Cost**: each walking-pace test takes about 115 s here, up from about 80 s.
tests/ci_costs scales their entries by 1.4.
