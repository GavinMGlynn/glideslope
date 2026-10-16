### A light aeroplane's take-off climb eased down to the plan's, not a throttle that reads the speed's trend, and the climb loop's seed made exact; the F-22A at 130 kt, rough air and a descent's level-off measured, 2026-10-10 — item done

**What is still missing, first.** Nothing of the item. Found on the way and
put in Later, per the owner's rule (not a regression the item made, and not
shown to be a correctness bug): the first leg of a plan that turns strays
11 to 17 m further from its line since the take-off climbs at the
published speed, not traced. Not run on Windows or macOS; nothing here is
platform code.

**The throttle's lead is taken out.** The review asked for evidence beyond
the light aeroplanes for 9e64f083's throttle reading the speed two seconds
ahead. A new test holds five classes at their start speeds at 4,000 ft in
moderate turbulence (MIL-F-8785C severity 3) for three minutes; RMS off the
speed asked, kt, and throttle reversals (turning back after 0.02 of travel):

| | no lead | lead, both terms | lead, proportional only | lead only off a stop |
|---|---|---|---|---|
| 747-400 | 3.35, 210 | 3.61, 186 | 3.62, 190 | 3.41, 216 |
| A380 | 3.81, 200 | 4.27, 180 | 4.25, 184 | 3.87, 211 |
| Learjet 35A | 3.21, 210 | 3.63, 177 | 3.57, 181 | 3.28, 214 |
| Mosquito | 6.14, 156 | 8.18, 156 | 8.09, 157 | 7.51, 173 |
| 172P | 2.99, 221 | 3.23, 209 | 3.24, 213 | 2.92, 234 |

(The first column with the exact seed below, as committed: the Learjet
3.20, 212; the Mosquito 6.19, 154; the 172P 3.00, 221; the jets as shown.)
The lead worsens every class's speed in rough air by 8 to 33%: the
turbulence's own trend is noise to it. Into the integral or the
proportional term only made no difference worth having (the two middle
columns); limited to the throttle coming off a stop, it still cost the
Mosquito 22%, whose throttle reaches full in the gusts, and held the 182S
only to 1.5 over. So it is out, and the transition is fixed where it is:

**The take-off's climb is eased down to the plan's** (Autopilot::ease_climb,
called by the controller at the take-off's hand-over): handed over climbing
faster than the climb asked, the climb asked comes down to it at 25 ft/min a
second instead of in a step, and once there is flown as asked. The rate is
the throttle integral's: 600 ft/min is about a quarter of a 182S's power,
which the integral moves at 0.02 a second for each knot off, so 50 ft/min a
second at a knot off, and half that keeps inside a knot. 182S over her
climb speed: stepped 2.1 kt, 50 ft/min/s 1.6, 25 1.1, 15 1.0. **It needs the
seed below**: eased from her own climb with the old seed, the nose was
stepped 4 degrees up and the 172P sagged 3.2 kt under. It touches only a
light aeroplane's take-off hand-over; in rough air the autopilot is as it
was.

**Measured** (linux-release), handed over / slowest / fastest on the first
leg against her climb speed, KCAS: 172P 74.8 / 74.8 / 75.8 against 75.4;
182S 81.4 / 81.4 / 83.1 against 82.0; Cub 47.5 / 47.5 / 48.9 against 47.8;
Cherokee 73.5 / 73.1 / 75.0 against 73.9. The worst is the 182S, 1.1 over;
every one within 0.9 to 2 of the item's bound.

**The F-22A at 130 kt** (10 under her slowest, on the autopilot in calm
air) did not depart with the lead, and departs again without it (sideslip
10.4 by then), as before this item. **The lead, not the pitch seed, held
her**: without the lead she departs at 130, with either seed; with it (and
the seed of 9e64f083) she does not. Behind the power curve there, the lead opened the throttle
as her speed began to fall rather than once it had, so she never slowed
into the departure. The tables rule stays at 120 kt, where she departs
either way; the 130 kt outcome is pinned as a flight figure of its own,
`the_f22a_held_at_130_kt_on_the_autopilot_departs`, so a later change to
the autopilot that holds her there shows.

**The pitch seed, made exact** (review: in a descent it stepped the nose
up). 9e64f083 seeded the climb loop's integral with her pitch, so that its
proportional part acted on the climb asked from the first step. Engaged
holding her height in a 1,000 ft/min descent that raised the nose at once:
firmer by 0.04 to 0.09 g (the 737-300 0.30 -> 0.39), and the broad run
found a stalled 737-300 handed to a new autopilot at 36 degrees of alpha no
longer recovered. **The seed is now taken on the first step, from the climb
asked then**: the integral is her pitch less the proportional part of the
climb asked over the climb she has, so the first command is her pitch
whatever is asked. For a climb asked of none - an autopilot engaged holding
its height, in a stall or a descent - that is exactly the old seed, and
nothing there changes (the new test: 172P 0.12 g off 1 / 56 ft sunk,
737-300 0.29 / 38, 747-400 0.22 / 49, no control above 0.0016 a step,
within half a g and a hand). Where a climb is asked that differs from the
one she has - the take-off's hand-over, eased from her own climb - the old
seed stepped the nose by the proportional part of it (4 degrees up at 1,025
ft/min). Half a g is AC 25.1329-1C, chapter 8, page 78: "an incremental
normal acceleration in the order of 0.5 g is considered the maximum" for a
pilot's recovery to a normal flight path, the nearest figure the FAA
states; its engagement rule, free of perceptible transients in steady
flight and minor ones when manoeuvring, gives no number.

**The ease is a light aeroplane's only.** The broad run also found the
F-22A's recorded CBD plan climbing to 14,000 ft against 3,000: handed over
at 1,000 ft climbing at thousands of feet a minute, 25 ft/min a second took
minutes to bring her down. The controller eases only an aeroplane with a
best-climb floor - the light class, handed over at full throttle on it.

**The selftest hash does not move** (`182dd6c996e0ee4c`): it flies no
autopilot.

**Verified**, linux-release:
- `the_autopilot_holds_five_classes_speeds_in_moderate_turbulence_to_their_measured_spread`
  (new): each class's RMS within 5% of the table's first column, its
  reversals within 10%. **Seen to fail** with the lead put back: "747-400
  held its speed to an RMS of 3.611247 kt (at most 3.517500)"; taken out,
  green.
- `the_autopilot_engaged_in_a_descent_levels_off_within_half_a_g_at_a_hands_pace`
  (new): the three above, each engaged descending at least 900 ft/min.
  **Seen to fail** with the seed put 8 degrees nose up: "737-300 levelled
  off at 0.654076 g off 1 (at most 0.5)"; restored, green.
- `the_f22a_held_at_130_kt_on_the_autopilot_departs` (new): she leaves her
  tables, sideslip 5 to 15 degrees. **Seen to fail** with the lead put
  back: "she left her tables at 130 kt"; taken out, green.
- `every_light_aeroplane_is_handed_to_its_plan_at_its_climb_speed_and_climbs_its_first_leg_at_it`:
  green; **seen to fail** without the easing: "c182 climbed her first leg
  at 81.403965 to 84.087439 KCAS, against her climb speed of 82.000000
  (within 2)", and with the easing and the old seed, "c172p climbed her
  first leg at 72.161975 ...".
- `an_autopilot_engaged_on_an_aeroplane_already_stalled_recovers_it` and
  `take_off_climb_to_3000_ft_and_orbit_the_cbd_is_planned_for_the_f22_by_anthropic_as_recorded_and_flown`:
  red on the way (the seed from her pitch whatever was asked; the F-22A
  eased), green with the exact seed and the ease for light aeroplanes
  only.
- The first-leg test's 5 kt bound explained (the leg turns, and a turn may
  spend 3) and its figures brought up to date.
- Every test but the network, download, renderer and tooling ones, 980 by
  `ctest --preset linux-release -j4 -E "fuzz|sealing|handshake|shader|cesium|tile|http|post_|a_post|header|gzip|deflate|redirect|_ion_|shot|window_|key_|keys|32_bit|toolchain|layering|vcpkg|docs|readme|package|cpack"`: 959 passed, 21 skipped (the
  live-model `_now` tests and others wanting a key or a display; none
  given), none failed. The selftest hash does not move.
- **The stalled hand-over** (CI run 37940777669, macOS release, red on
  9e64f083: the 737-300 "lost 2866 ft, peak 1.87 g, NOT RECOVERED"): on
  Linux the seed from her pitch whatever was asked, without the throttle's
  lead, failed it the same way (737-300 lost 2863 ft, not recovered; the
  A320 3.15 g); with the lead it passed on Linux and not on macOS. With the
  exact seed an autopilot engaged in a stall, holding its height, is seeded
  as it always was and there is no lead: the 737-300 recovers losing 2697
  ft, the A320 2037 ft at 3.15 g, as on the base. **Not done**: the
  test's margin. It has no height or load bound of its own (they are the
  other stall test's), only recovered or not within its flight, and how
  near the 737-300 is to not recovering on another platform was not
  measured in the time given; CI on this push says whether macOS agrees.
