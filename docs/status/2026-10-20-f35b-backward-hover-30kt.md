### The F-35B flies backward at 30 kt in the hover, 2026-10-20

**What is still not true first.** Her pitching moment past 90 degrees of
angle of attack is this model's shape, not a published one: nothing public
gives the F-35B's (or the F-4C's, whose slope she borrows) in reversed air.
Her lift table is still clamped at its ends in reversed air (constant from
-0.6 rad down, nothing past 1.57), so her lift also steps at 180 degrees;
it is small at hover speeds (about 1,300 lb at 30 kt against her 39,000) and
the height law takes it, but it is not continuous either.

**The cause.** Her Cm-alpha was the F-4C's slope times aero/alpha-rad. Flown
backward, atan2(w, u) sits near 180 degrees and crosses from -180 to 180
with each small pitch swing, so the term flipped from 0.098 x pi nose-up to
as much nose-down: about 5,700 ft-lb either way at 30 kt (probed from
moments/m-aero-lbsft). The fan's share saturated at each crossing, and her
hover law swung her -9.5 to +9.6 degrees in pitch and 25 to 32 kt back. At
20 kt the jump was under half as large, which is why the limit was 20.

**The fix, in tools/make_f35b.py** (pitch_alpha_table): Cm-alpha is a table
on aero/alpha-rad - the slope as before to 90 degrees either way, then
straight to nothing at +/-180, so it is the same both sides of 180 (tail
first, the slope reversed, as a weathercock blown from behind). Within 90
degrees it is the old term to interpolation's rounding. STOVL_TRC_AFT_KT is
30 again (Farley on the VAAC Harrier; Kjelgaard on the F-35B).

**Verification** (linux-release):
`in_a_hover_the_f35bs_pilot_moves_her_forward_back_sideways_and_turns_her_with_the_lever_stick_and_pedals`
now asks 30 kt back: -29.96 kt averaged over 12 s; across the whole test
speeds swung 2.26 kt at most, pitch 7.7 degrees, bank 7.7 (pitch was 9.3
with the 20 kt limit). Seen red both ways: the old Cm-alpha with the 30 kt
limit failed on a 7.3 kt swing; the fix with the limit put back to 20 failed
on -20.0 kt. Every F-35B, STOVL and hover test, the committed-model-matches
-script tests and the selftest tests passed (77, `ctest -R
"f35b|stovl|selftest|script_makes|hover"`).

**Selftest hash** unmoved: the selftest flies the C172P, whose model is not
touched (5cac1fa158d7e506 here).
