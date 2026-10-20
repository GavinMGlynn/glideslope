### The C172P's rudder in the part of the slipstream that reaches her fin, 2026-10-20 — item done; one learnt-landing test red

**What is still not true first.**
- **The learnt landing lands short in the most crosswind from the right**:
  `the_learnt_policy_lands_within_its_limits_from_every_corner_of_its_gate_in_the_most_crosswind_from_the_right`
  is red. The policy was trained on her rudder on the whole slipstream; from
  the gate's eight low corners (1.60 nm, 20 m low) it now touches 0.04 to
  20.2 m before the threshold (0 of 32 short on main). Every other learnt
  test passes. Training the policy again on this model, or not, is the
  owner's decision; nothing here retrains it.
- **The fin's share is read off a drawing**: Figure 1-1's side view, scaled
  by its own printed height, to about half an inch (about 0.01 of the share).
  Assumed, not measured: the slipstream runs back level with her thrust line,
  unswirled and contracted no further than momentum theory's far wake.
- **No `-march=x86-64-v2` build was made**: no figure moved by more than 0.03
  (below).

**The cause.** Her rudder's yaw moment (Cndr) was on
`aero/function/qbar-induced-psf`, the fully grown slipstream's dynamic
pressure, as if her whole fin sat in it - as her elevator was before
2026-10-16.

**The fix** (tools/make_c172p.py, its docstring the record). Her handbook's
Figure 1-1 (Three View, page 1-2; the 1985 printing named in docs/ASSETS.md,
SHA-256 `4d17d416...`), its side view scaled by its own 8 ft 9 1/2 in height
(401 px at 400 dpi), has her thrust line (the spinner's centre) 52.9 in above
the ground, her fin's root on the tailcone 50.7 in, and its tip below the
beacon 103.4 in. The slipstream is the elevator's, the 75 in propeller over
root two, 53.0 in across about the thrust line; its upper half reaches 79.4
in and covers 0.545 of the fin's 52.7 in span (below the thrust line is
tailcone). A new function, `aero/function/qbar-fin-psf`, is the free
stream's dynamic pressure plus 0.545 of the slipstream's excess over it, and
Cndr is on it.

**The test** (tests/unit/test_departure.cpp):
`a_c172p_standing_at_full_power_turns_her_rudder_on_the_part_of_her_slipstream_that_reaches_her_fin`.
On held brakes at full power, rudder full left and full right (both ends;
centred the term is nothing), her yaw moment is the free stream's plus 0.545
of the excess, to 0.1 per cent: 682.5 ft-lb at 0.279 rad, on 16.75 psf of
slipstream. The situation is asserted built: standing, excess over 10 psf,
rudder over 0.2 rad. **Seen red**: main's c172p.xml put in the build's data
copy, it fails at 1,252.3 ft-lb; restored, green.

**What moved.** `glideslope_cli figures c172p` (linux-release, main's model
then this one), all eleven in range: climb 763.15 to 763.12 ft/min, ceiling
12,313.47 to 12,313.49 ft; the other nine the same to the printed digit.
**The selftest hash does not move** (`b8ffab0bc34fc89b` both, linux-release):
its flight holds her rudder central, where the term is nothing. **The learnt
landing's parity fixture is flown again** (`tools/rl/export.py
--parity-only`); the policy is not trained again.

**Verification.** linux-release: the 118 tests named figure, committed,
c172p, selftest, learnt, parity or cessna - all green but the parity fixture
(then flown again, green) and the right-crosswind learnt landing above; the
27 learnt and parity tests again after: 26 green, that one red. Then the
166 others named every_landplane, crosswind, walking, rollout,
light_aircraft, gust, every_aircraft, rotat, take_off or landing: all green. linux-debug
was not built here; CI runs it. `tools/make_c172p.py --check` passes.
