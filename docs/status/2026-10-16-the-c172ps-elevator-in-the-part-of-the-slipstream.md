### The C172P's elevator in the part of the slipstream that reaches it: on held brakes at full power she stands, 2026-10-16 — item done

**What is still not true first.**
- **Stick full back is unsourced.** Nothing found says where a real C172's
  nose wheel is at full power on her brakes with the stick held back: her
  handbook has her elevator untouched there, Cessna publishes no static
  thrust, and the brake holding torques found online are bolt torques. What
  she does is measured and pinned, not judged: her nose wheel stays down at
  her model's own loading, farthest forward and at 2400 lb (0.04 to 0.09 ft
  of strut still compressed), and at her aft limit, 47.1 in, she sits on her
  tail strike at 11.2 degrees.
- **The tailplane's share is assumed, not measured**: the slipstream meets
  it centred, contracted no further than momentum theory's far wake, and the
  tailcone takes nothing out of it.
- **Her rudder (Cndr) still works on the whole slipstream**, as her
  elevator did. A new Phase 9 item.
- **Stick full forward her brakes slip**: at her model's own loading and
  farthest forward her mains are lightened and she creeps forward at up to
  9 and 13 ft/s over 30 s. Not judged; she does not go over.
- **linux-debug was not built here**; CI runs it.

**The cause.** Her elevator's moment (Cmde) was on
`aero/function/qbar-induced-psf`, the dynamic pressure of the fully grown
slipstream, as if her whole tailplane sat in it. Standing at full power that
is the static thrust over the disc area, 516 lb on 30.7 sq ft, 16.8 psf, on
all of it: stick full back +7,860 ft-lb, which reared her onto her tail
strike at 11.2 degrees at every loading, and stick full forward -6,200 ft-lb,
which lifted her main wheels and put her over her nose (pitch past -60) at
three of four loadings. Her weight about her main wheels is 2,500 to 2,700
ft-lb.

**The fix** (tools/make_c172p.py, its docstring the record). Momentum theory
has the far slipstream at twice the induced velocity on half the disc's
area, so its diameter is the propeller's over root two. Her handbook's
Figure 1-1 (Three View, page 1-2) gives the propeller, 75 in, and the
horizontal tail's span, 11 ft 4 in: the slipstream, 53.0 in across, covers
0.390 of it. A new function, `aero/function/qbar-tail-psf`, is the free
stream's dynamic pressure plus 0.390 of the slipstream's excess over it, and
Cmde is on it. Standing, her elevator has 0.39 of what it had (stick back
+3,060 ft-lb, forward -2,430); in flight the two nearly agree.

**The tests** (tests/unit/test_departure.cpp), each flying four loadings
inside her handbook's envelope (Section 2: 35.0 to 47.3 in, 2400 lb), the
envelope's ends reached (40.4 and 47.1 in), settled on full brakes, the
throttle opened over 3 s and held 30 s:
- `a_c172p_at_full_power_on_held_brakes_with_her_stick_central_stands_on_all_three_wheels_at_every_loading`:
  her handbook's short-field take-off (Section 4: "Brakes -- APPLY.
  Throttle -- FULL OPEN. ... Brakes -- RELEASE. Elevator Control --
  SLIGHTLY TAIL LOW"). All three wheels carry weight throughout, neither
  tail nor cowling touches, her brakes hold her under 0.5 ft/s, and she
  turns her static 2300 to 2420 RPM (2327).
- `a_c172p_at_full_power_on_held_brakes_with_her_stick_full_forward_never_noses_over_at_any_loading`:
  both mains carry weight throughout, her cowling never touches, pitch
  above -10 (least -4.0).
- `a_c172p_at_full_power_on_held_brakes_with_her_stick_full_back_does_what_is_measured_and_unsourced`:
  the outcomes above, pinned.

**Seen red.** On main's model (its file put back in the
`-march=x86-64-v2` build's data copy) the forward test failed - mains up at
every loading, pitch to -54.8 to -87.4 - and the back test failed - on her
tail at every loading; the central test passed there, as it should. With her
thrust line put 60 in below the runway in the data copy, the central test
failed: on her tail at three loadings. Each restored, green.

**What moved.** `glideslope_cli figures c172p` (`-march=x86-64-v2`, main's
model then this one), all eleven in range: take-off ground roll 908.3 to
919.4 ft, service ceiling 12,786 to 12,313 ft, cruise 119.63 to 119.16
KTAS, stall flaps up 51.26 to 51.15, flaps 10 47.67 to 47.65, flaps 30 45.89
to 45.76 KCAS; the rest within 0.1. **The selftest hash moves**,
deliberately: it flies a C172P, whose elevator term changed -
`b79bca7f3d8f6429` (main's, rebuilt at d2f489e7) to `5cac1fa158d7e506` in
linux-release, `07cc7d4a4d42a300` to `91171736935cd5c7` in `-march=x86-64-v2`
release; linux-debug's was not built here. **The learnt landing's
parity fixture is flown again** (`tools/rl/export.py --parity-only`) on the
new model; the policy is not trained again, and its landing tests pass.

**Two tests moved with her.**
- `rotating_early_is_caught_for_each_aeroplane_at_its_own_speed`: pulled
  fully back at 45 knots she is thrown onto her tail strike and her main
  wheels hop clear for a moment; the test's hand let go there, and she
  rolled on to 60 knots on the book's stick. The hand (`fly_the_take_off`)
  now lets go when her main wheels have been clear half a second; she is off
  at 50 knots, named, as on main (49).
- The dry walking-pace tests: at 3 kt the C172P's autobrake now reaches the
  rolling ceiling, 0.300 (0.287 on main), in linux-release and
  `-march=x86-64-v2`, so she is no longer named there.

**Verification.** In linux-release: the full suite run to 295 tests and
stopped for time, two red (the early rotation and the dry walking pace,
above); then the 253 not yet run whose names have c172p, every_landplane,
take-off, rotation, landing, learnt, walking, gust, crosswind, selftest,
committed, figure, cessna or tail, two red (the dry walking pace again and
the learnt landing's parity fixture, before it was flown again); after the
fixes the 40 learnt, walking-pace, early-rotation, one-fault and new tests,
all green. In `-march=x86-64-v2` release, the 43 tests named c172p,
every_landplane, walking pace or learnt policy, and the early-rotation,
one-fault and new tests: all green but the client-and-server learnt landing,
whose server that build did not make (green in linux-release).
`tools/make_c172p.py --check` passes.

