### Every aeroplane's glide speeds are what the command measures now: the B-2A's 169 kt, the Mosquito's 129, 2026-10-13 — item done

**What is still not true first.** The glide band tests pin a figure by
gliding 5 kt under it, not every step down to a route's slowest as the
command flies; and a band turning one way checks only that way under the
figure, where the command needs both. Both are as they were; neither moved
a figure here.

- **Measured again**, `glideslope_cli glide-speeds` for all sixteen on
  linux-release, 2026-10-13. Fourteen write what their files say. The B-2A
  writes 169 kt (her file said 159): at 169, the slowest a route may fly her
  since her plan's slowest was raised on 2026-10-10, she went round both
  ways at 11.8 and 11.7 degrees of alpha, her lift peaking at 11.7. The
  Mosquito writes 129 (her file said 128): at 129 both ways at 9.1, where
  her lift peaked. Both files say so, with the old figures and why they
  stood.
- **Why it went unseen.** A brief takes the higher of the file's slowest
  glide and a route's slowest, so neither old figure was ever asked, and
  `glides_in_band` checked a file's figure only in the band that began at
  the first speed asked - which for these two began at neither figure, so
  the check never ran. New:
  `every_aircrafts_slowest_glide_each_way_round_is_the_first_asked_and_begins_the_glide_test_that_pins_it`
  holds every aircraft, each way round, to a figure that is the first speed
  asked, not under a route's slowest, and that begins a band, so the band's
  pin runs for every aeroplane. 32 aircraft-ways, 32 covered. **Seen red**
  on the Mosquito's stale 128 ("not the first asked, 129", 30 of 32), and
  with the B-2A's right band moved to 168 ("begins no band turning right",
  31 of 32), reverted.
- **Renamed**, same flights:
  `the_b2a_glides_round_its_tightest_orbit_turning_{left,right}_from_169_to_179_kt_without_stalling`
  (were from 159) and
  `the_mosquito_fb6_glides_round_its_tightest_orbit_from_129_to_148_kt_without_stalling`
  (was from 128); their costs in `tests/ci_costs/` carried over.
- **Run** (linux-release), green: every glide band of every aeroplane with
  every figures and published-figure test, 135; and the band-coverage and
  new tests, the glide-figure reading test, the plan-speed reading, giving
  and fastest-in-hand tests, every brief's approach speed and the
  engine-stopped glide. No plan speed moved, so the plan-speed flights were
  not re-flown.
- The selftest hash does not move: 182dd6c996e0ee4c.
