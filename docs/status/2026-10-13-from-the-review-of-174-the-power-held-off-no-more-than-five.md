### From the review of #174: the power held off no more than five seconds, nor near the ground, nor with the wing past unloading; what a player can still do; the margin check said as it is, 2026-10-13 — item stays done

**The hold is bounded.** As first written, the recovery held the throttle
for as long as the wing stayed past its peak, so a wing the elevator could
not unload sank at idle. It is now let go on the first of
(src/sim/autopilot.cpp):
- **five seconds since it began** (`most_unload_s`): the longest unload
  measured across the roster, left thirty seconds in the stall, is the
  F-35B's 3.57 s, then the B-2A's 2.74 and the Cessna 172P's 2.72; at the
  warning none holds at all. Five is that with 40% over;
- **the ground nearer than 1,500 ft** (`least_unload_agl_ft`): the height
  the FAA's Private Pilot ACS (FAA-S-ACS-6, the stall tasks) has a
  single-engine aeroplane's stall recovered no lower than (3,000 ft for a
  multi-engine one, which the hold does not take: 1,500 is the floor where
  any aeroplane's height comes first); nearer the ground the height the power saves is
  worth more than the load it adds;
- **the nose at its floor with the angle of attack not falling**: a wing
  the elevator cannot unload, which only the power will.
`a_stall_recovery_whose_wing_stays_stalled_gets_full_power_within_five_seconds_and_near_the_ground_at_once`
flies every aeroplane taught a stall left thirty seconds in it with the
recovery's elevator overruled, held fully back, where it is practised and
at 1,000 ft: full power within 5 s where practised (nine still stalled when
it came, the F-35B let go at 4.53 s by her nose at its floor, the rest at
4.99 by the time), and at the hand-over at 1,000 ft (four still stalled);
the 737-300 at 1,000 ft is in the ground before her stall is called, named
in the test and asserted. Seen red: the release stepped over, "12
recoveries held the power off too long; the first: 737-300 ... 186.366667"
s; restored. The stall flights print how long each held the power.

**What a player can still do.** The hold is the autopilot's: a player who
gives the Mosquito full power at once at the hand-over, as the test's
flight did before, still pulls her to 1.88 g. Her stall lesson says "Full
power as the nose comes down" but judges only the height, not when the
power came, so it says nothing of it.

**The AI test's margin check, said as it is.** "How far she slows under her
warning, a tenth over, is no more than her warning's margin" is the same as
"never slower than her stall at her weight plus an eleventh of that
margin": under half a knot over the stall for the rule's 5 kt, 0.9 kt for
the Mosquito's 10. The test now computes and names that floor; the
aeroplanes and figures are as below.

**Only the Mosquito's figures give `stall_warning_kts`**, pinned by
`only_the_mosquitos_figures_give_her_own_stall_warning_ten_knots_over_her_stall`
(test_figures.cpp: every aircraft in the catalogue walked; each one's
landing speeds carry its file's margin). The parser refuses more than 20 kt:
twice the one published warning found and four times the rule's least - a
margin past it is more likely a stall speed written in its place.
**The optional attributes of `<published_figures>`** are now
`go_around_flaps_deg`, `stall_warning_kts`, `approach_speedbrake` and
`touchdown_fpm` (src/sim/figures.cpp); `flaps_full_deg` is required.

**Height lost rose for five aeroplanes** left thirty seconds in the stall,
each within its bound (the entry below): the 737-300, B-2A, F-15C, F-35B
and J-3 Cub.

**Verified** (linux-release, client and server built, displays unset,
`ctest -j4`, the whole suite on 06c24934's tree): 1,206 tests, 1,174 passed,
24 skipped (no key, no Vulkan, Windows-only), 8 failed - every one a
multi-process test on a fixed port, run while two other worktrees' agents
ran the same tests on the same ports (their servers seen listening; "refused,
reason 6", a bad handshake from another's server). Each of the 8 passed run
alone afterwards. The selftest hash does not move, `182dd6c996e0ee4c`.
