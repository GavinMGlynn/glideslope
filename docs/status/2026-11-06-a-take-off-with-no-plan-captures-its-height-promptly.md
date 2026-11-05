### A take-off with no plan captures its height promptly, 2026-11-06

**What it does not do, first.**
- **The circuit lesson tests still ask the take-off for 750 ft**, not the
  650 they join the circuit at. The creep was not the only reason. Asked
  for 650 ft, every aeroplane now reaches it and is joined level there.
  The PA-28 then climbs to the circuit's 1,000 ft, overshoots to 1,078 ft,
  and starts her downwind leg at 954 ft. That puts her 124 ft outside her
  starting height, past the lesson's 120 ft band. With the old hold she
  went 1,004 to 1,035 ft. Joined climbing at 650, from a take-off to
  750 ft, she goes 978 to 1,076 ft and passes. This is a Phase 10a tail:
  a light aeroplane's climb to circuit height overshoots it.
- **A jet taken off with no plan to a low height climbs far past it.**
  Asked for 650 ft, the 737-300, 747-400, 787-8, A320, A380, F-22A and
  Learjet 35A go 139 to 532 ft past it. Most are handed over already
  above it, because a jet's flaps come up from 400 ft and the take-off
  waits for them. The capture brings each back, within 2 ft by 64 s at
  the latest. This is a Phase 10a tail.
- **7 ft/min a foot is chosen, measured on every landplane.** It is not
  sourced. The lead implies 10 (below), and 10 made the 787-8 ring.

**What it does.** When a take-off with no plan hands her over at its
lead, the autopilot captures that height (`Autopilot::capture`, called by
`Controller` at the hand-over). It flies to the height at 7 ft/min for
each foot off, not the plain hold's 3. It holds the height that way for
as long as it is still the height it flies to. Every other altitude hold
is unchanged, and so is every plan's take-off.

**Why she crept.** The plain hold asks 3 ft/min for each foot off. That
is a first-order approach with a 20 s time constant, and it is
exponential, like any linear capture law. A Sperry patent, US 5,117,362,
contrasts a constant-acceleration capture with "an exponential capture
characteristic of linear controllers". Handed over about 340 ft short,
the Mosquito was within 2 ft of 650 ft 74 s after the hand-over, but she
only reached it at 167 s: the last 4 ft at a few ft/min. The circuit test
joins at 650 ft, so she flew 2.7 minutes and 7 nm upwind. Every landplane
behaved the same way: within 2 ft 12 to 113 s after the hand-over, and at
the height up to 199 s after it.

**Research.** Searches:
- autopilot altitude capture laws: ALT* to ALT, a capture started as a
  fraction of vertical speed, asymptotic against fixed-rate capture;
- Boeing 737 ALT ACQ to ALT HOLD;
- Airbus ALT* to ALT;
- Garmin GFC 700 ALTS to ALT;
- a PI altitude hold's slow final creep.

What they found:
- Sperry's patents. US 4,377,848 is a circular, 0.05 g capture path with
  the selected altitude as its tangent, contrasted with an earlier
  "smooth asymptotic" capture. US 5,117,362 notes that linear controllers
  give an exponential capture.
- Airbus: ALT engages from ALT* "when the difference between the present
  altitude and the target altitude becomes less than 20 ft" (FlyByWire
  A32NX guidance; no Airbus FCOM copy was found).
- Garmin GFC 700: ALTS hands over to altitude hold at 50 ft (Garmin
  pilot's guide).
- Boeing 737: the FCOM excerpts found describe ALT ACQ and ALT HOLD but
  give no figure.
- The PI material puts a slow creep down to a low gain near zero error,
  or to an integrator unwinding. Here the vertical-speed loop's integral
  was not the cause. The trace showed the vertical speed following the
  3 ft/min a foot asked, about 12 ft/min at 4 ft off.

A real capture hands over to its hold within 20 to 50 ft. The time it
takes to close those last feet is the hold's gain. Here that gain was
too low to close them promptly.

**Why 7.** The instrument lead, a tenth of her climb, is 0.1 minute of
her climb. A capture that starts from that climb with no step closes at
10 ft/min a foot, a 6 s time constant. Measured, by the time after the
hand-over until she stays within 2 ft, worst of the 30 flights:

| Gain | Worst time | Notes |
|---|---|---|
| 3 (old) | 113 s | |
| 5 | 77 s | |
| 7 | 64 s | 787-8 within 2 ft at 45 s (650 ft) and 38 s (3,000 ft) |
| 10 | 69 s | 787-8 rang: 7 ft past 3,000, 54 s; 69 s at 650 |

A constant-pull capture, the Sperry circle, was tried with the gain as
its limit near the height. With the pull taken from the climb at the
hand-over, the circle never asked less than 7 ft/min a foot, so it was
taken out.

**Verification.**
`every_landplane_given_a_take_off_with_no_plan_is_within_2_ft_of_her_height_from_75_s_after_the_hand_over`
(tests/unit/test_lesson.cpp) takes off each of the 15 landplanes, at the
weight of her take-off figures, to 650 ft and to 3,000 ft: 30 of 30
flights, asserted. The Short S.23 is named and left out because she takes
off from water. Each is held for five minutes. The test asserts:
- no wreck;
- the take-off is over;
- she was held the five minutes;
- from 75 s after the hand-over on, she is within 2 ft of her height.

All 30 passed. The worst was the 747-400 to 650 ft, at 63.3 s: she was
handed over 412 ft above it (tail above). Of the take-offs handed over
short:
- the Mosquito: 24.0 s at 650 ft, reaching it at 31.7 s (was 166.7);
- the B-2A: 36.9 s at 3,000 ft (was 92.5).

Red on the old code: the 3 ft/min a foot put back, the 737-300 to 650 ft
was within 2 ft only from 100.1 s, and the test failed.

**What moved.** Measured on linux-release.
- **Overshoot of the level-off**: light aeroplanes go a few feet more
  past it.
  - C172P at 3,000 ft: 3.5 ft (was 1.8).
  - C182: 3.9 (0.9).
  - PA-28: 3.1 (1.4).
  - J-3 Cub: 9.7, unchanged.
  - 787-8: 1.9 (0).
  - In the ten-minute hold at 3,000 ft
    (`every_landplane_given_a_take_off_to_3000_ft_and_no_plan_holds_it_for_ten_minutes`)
    every one is still within 100 ft past, and within 0 ft after two
    minutes.
- **The step at the hand-over** is now to seven tenths of her climb, not
  three tenths.
- **The circuit lessons, the gear test and the take-off trials**: green,
  with their figures as before. Only a no-plan take-off's hand-over is
  touched, and the circuit tests' take-off to 750 ft is joined at 650,
  before it is over.
- Run on linux-release, excluding the gust_sweep label: every test
  matching take, depart, circuit, lesson, controller, autopilot,
  selftest, lift_off, level or hand: 240 of 240 passed, 9 skipped for want of
  a key or of Windows. Nothing here is
  platform-specific code, so there was no Windows build.
- **The selftest hash does not move**: the selftest flies a pilot's input
  log, not an AI take-off.
