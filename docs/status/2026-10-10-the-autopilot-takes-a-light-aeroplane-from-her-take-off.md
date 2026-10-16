### The autopilot takes a light aeroplane from her take-off within 2 kt of her climb speed: its climb loop starts from her pitch, and its throttle reads the speed's trend, 2026-10-09 — item done; the throttle's lead in it taken out 2026-10-10 (the entry above)

**What is still missing, first.** Nothing of the item. Both changes are to
the autopilot every aircraft flies on, not to the hand-over alone, and are
verified by the tests named below, not on Windows or macOS (no platform
code).
The first leg's stray from its line, 11 to 17 m more since the take-off
climbs at her published speed (the entry below), was not traced in the
time given; traced 2026-10-13 (the entry of that date): the turn onto the
leg, growing with the square of her speed.

**What moved her off her speed after the hand-over** (the 182S, 81.4 KCAS
handed over, 80.6 to 84.0 on her first leg against 82.0), traced step by
step: not the hand-over's speed, which is within 0.6 kt for all four, but
the autopilot's first seconds.
- **It pitched her up.** The vertical speed loop's integral was seeded with
  the pitch she had plus its proportional part of the climb she had. That
  is her pitch only when the climb asked is none, as it is for an autopilot
  engaged holding its height; asked for any other climb, the first command
  is her pitch plus 0.004 degrees per ft/min of the climb asked. Handed
  from the take-off at 1,025 ft/min to the plan's 700, the 172P was asked
  2.5 degrees more pitch, not less, and every light aeroplane sagged 0.8 to
  1.9 kt before the integral wound it back (the 172P 74.8 -> 73.6). Now the
  integral is the pitch she has - the pitch holding the climb she is in -
  and the proportional part acts on the climb asked from the first step. An
  autopilot engaged holding its height, climbing, now noses toward level at
  once at its pitch rate rather than holding her pitch while the integral
  winds.
- **The throttle waited for the speed to pass.** Coming down from the
  take-off's full-power climb, 1,260 to 1,420 ft/min, to 700, the throttle
  stayed at its stop until the speed was past the one asked, then came back
  at a quarter of its travel a second: the 182S 2.1 over. The throttle law
  now reads the speed two seconds ahead on its trend (`throttle_lead_s`,
  the speed's rate over the last second): a pilot holding a speed on the
  throttle moves it as the needle starts to move. Two seconds is the
  throttle's own pace, half its travel.
- **Tried and not kept**: easing the plan's climb down from the take-off's
  at 25 to 200 ft/min a second. With the pitch-up still in, it made the
  sag worse (the 172P 1.8 -> 3.2 kt under at 25 ft/min/s); without it, it
  took 25 ft/min/s - half a minute - to bring the 182S to 1.1 over, where
  the throttle's lead does it at once.

**Measured** (linux-release), handed over / slowest / fastest on the first
leg against her climb speed, KCAS, before -> after:
- 172P: 74.8 / 73.6 / 76.2 -> 74.8 / 74.8 / 75.7, against 75.4
- 182S: 81.4 / 80.6 / 84.0 -> 81.4 / 81.4 / 83.1, against 82.0
- Cub: 47.5 / 45.9 / 48.9 -> 47.5 / 47.5 / 48.5, against 47.8
- Cherokee: 73.5 / 72.2 / 75.3 -> 73.5 / 73.4 / 74.6, against 73.9

The worst is the 182S, 1.1 over, against the item's 2. Each of the two
changes alone is not enough: the pitch seed alone left the 182S 2.1 over;
the throttle's lead alone left the 172P 1.8 under.

The first-leg test (asked 80 kt): slowest on the leg, 172P 74.8, 182S 80.0
(in its turn), Cub 47.5, Cherokee 73.4; strays 82, 83, 41 and 62 m.

**The selftest hash does not move** (`182dd6c996e0ee4c`): it flies no
autopilot.

**Verified**, linux-release:
- `every_light_aeroplane_is_handed_to_its_plan_at_its_climb_speed_and_climbs_its_first_leg_at_it`,
  its bound now the item's: within 1 kt at the hand-over and within 2 kt,
  either side, on the first leg. **Seen to fail** with the two autopilot
  changes put back: "c182 climbed her first leg at 80.603498 to 84.035899
  KCAS, against her climb speed of 82.000000 (within 2)"; restored, green.
- `a_trial_stops_a_flight_whose_alpha_or_sideslip_leaves_its_tables_and_judges_it_not_held`
  **moved**: the F-22A at 130 kt, 10 under her slowest, no longer departs
  on the autopilot, so the test's situation was gone (red: "she left her
  tables"). It now flies her at 120, which departs (at 125 too, sideslip
  4.3; at 120, 1.4); the rule it pins is unchanged. Green.
- Every test but the network, download, renderer and tooling ones: 977 by
  `ctest --preset linux-release -j4 -E "fuzz|sealing|handshake|shader|cesium|tile|http|post_|a_post|header|gzip|deflate|redirect|_ion_|shot|window_|key_|keys|32_bit|toolchain|layering|vcpkg|docs|readme|package|cpack"`,
  the autopilot being in nearly all of them: 976 passed or skipped (the
  live-model `_now` tests, no key given) and the F-22A's above failed,
  then fixed and green. Among them every orbit, glide, take-off, landing,
  lesson, stall recovery, hand-over, separation and recorded-plan test,
  and the selftest's three.
