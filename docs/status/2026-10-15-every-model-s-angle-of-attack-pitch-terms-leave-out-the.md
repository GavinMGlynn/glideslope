### Every model's angle-of-attack pitch terms leave out the crosswind, 2026-10-15 — item still open (done above)

**What is still not true first.**
- **The item stayed open on the Cub's lift** (three braked runs where her
  tail lifted or she went over and calm air's did not), done in #205 and
  checked on the combined tree above. Every model's pitch terms keyed to the
  angle of attack are now on `qbarUW`, the Cub's (#203) and the fifteen
  others'.
- **Every model but the Cub has her lift keyed to the angle of attack still
  counting the crosswind**, which the same guidance would move too (the
  FlightGear wiki: `QBarUW` "is more appropriate for CL"). The Cub's is
  moved (#205). A new Phase 9 item.
- **The swept wings are moved on an argument, not a measurement of a real
  aeroplane**: issue #89 did not look at swept wings, and nothing here
  compares a swept wing's moment standing in a beam wind with a panel method
  or a test. The argument (simple sweep theory, below) says `qbarUW` is the
  nearer of the two pressures, not that it is exact.
- **The learnt landing flies a C172P a little different from the one it
  learnt on**: the policy is not trained again. Its parity fixture is flown
  again on her (below), and every learnt-landing test passes, its limits
  held. Against the fixture's old flights, from the 27 verification starts
  (the bindings, new fixture less old): **calm air**, sink -2 to +20 ft/min,
  +0.56 to +0.60 m further across (now +2.32 to +2.70), 3 to 7 m further
  along; **10 kt from one side** (+10 kt), 6 to 37 ft/min less sink (now 72
  to 106), +0.61 to +0.94 m further across (now +1.76 to +2.44), 8 to 25 m
  further along; **10 kt from the other** (-10 kt), 15 ft/min less sink (now
  138 to 140), +0.35 to +0.37 m across (now +1.56 to +1.60), 8 m further
  along. All within her limits (5 m, 300 ft/min). Why it moves so much is not
  traced: the two pressures differ only where she slips, so she flies her
  approach slipped; most where the crosswind is.
- **Not moved, named with their reasons** (and pinned): the C182's `CmDe`
  and the F-22A's `CmDh` increment, elevator terms, where the angle of attack
  picks the coefficient of a deflection; the F-22A's `CmDtv`, her thrust
  vectoring; and `Cmq` in the F-22A and the S.23, pitch damping, the angle of
  attack picking the coefficient of the pitch rate.

**The change.** Each script calls
`alpha_rate.with_alpha_terms_on_qbar_uw` with the terms it names, and its
docstring says why and what standing in 15 kt across did before and after:
`Cmalpha` in the 737-300, 747-400, A320, A380, B-2A, C172P, C182, F-15C,
F-35B, Learjet 35A and PA-28; `Pitch_alpha` in the 787-8; `pitch-alpha` and
`pitch-stall` in the Mosquito (the moment past the stall is her Cm-alpha
curve continued); `Cm_basic` in the S.23; `Cma_M` in the F-22A. **The
F-22A has no Cm-alpha term of her own**: it is the column at no deflection of
`CmDh`, her tailerons' table of the angle of attack and deflection.
`make_f22.py`'s new `with_cm_alpha_split_out` makes that column its own
function, `CmDh_alpha` (moved), and leaves `CmDh` the deflection's increment
over it (a `<difference>` of the table and the column, on `qbar`), so the
two add to the old `CmDh` term for term. Every one of her figures prints the
same as before. The B-2A's script imports `alpha_rate` for the first time.
`tools/alpha_rate.py` gains the swept-wing argument in its docstring, and its
angle-of-attack match allows space before the tag's close (as the test's
does). **The F-22A's split refuses a CmDh with anything more in it**: its
properties must be qbar, Sw and cbarw, its value 1.5 and nothing else but
its two tables, so a factor added to the pinned model is not dropped; seen
refusing an added property and an added value.

**Why the swept wings too.** JSBSim issue #89 (Coconnier's XFLR5 and
vortex-lattice comparison, L = 0.5 rho (U^2 + W^2) S CL on an unswept wing)
and the FlightGear wiki's "JSBSim Aerodynamics" (dany93, reviewed by
Coconnier, 2019) give `qbarUW` for Cm and CL as guidance tested on the C172.
Coconnier: "Wing sweep will indeed be a major contributor but this needs
further investigation", and "swept wings might need a different treatment
that we have not investigated further". **Simple sweep theory** (R. T. Jones,
"Wing Plan Forms for High-Speed Flight", NACA Report 863): the pressures on a
yawed wing are set by the air's component normal to its leading edge alone.
A beam wind V on a wing swept by L meets each half at V sin L normal to its
edge (so a swept wing's sections do feel a beam wind, unlike a straight
wing's), but at an incidence of about w over V sin L, not atan2(w, u), and
standing w is near nothing. So the part of her moment that goes with the
angle of attack is small, of order rho V sin L w. On `qbar` the model's term
is Cm-alpha times atan2(w, u), anything at all, times the beam wind's whole
pressure; on `qbarUW` it is of order rho (u^2 + w^2), near nothing. One half
meets that normal flow leading edge first and the other trailing edge first;
the reversed half's force too goes with w, near nothing standing. **Standing,
neither is exact and `qbarUW` is the nearer, at any sweep.** **In flight it is
not so plain**: at sideslip b the theory's mean pressure over both halves is
cos^2 b + tan^2 L sin^2 b of the whole, between `qbarUW`'s cos^2 b and
`qbar`'s 1 - nearer `qbarUW` below 35.3 degrees of sweep, midway there, nearer
`qbar` above it, and `qbar`'s exactly at 45. At 5 degrees of sideslip: 0.994
at 25 degrees, 0.996 at 35, 0.999 at 42, against 0.992 and 1 - under 1%
either way, and no figure shows it. Taking a wing's sweep at its quarter
chord or its leading edge changes nothing standing, and in flight only which
of two choices under 1% apart is nearer; every wing here is swept 45 degrees
or less either way (the F-15C's leading edge is 45, her quarter chord 38.7).

**Standing on full brakes in 15 kt straight across** (linux-debug, a probe
not committed: each landplane settled ten seconds in calm air, the wind on
from either side, the next fourteen seconds read; the S.23 afloat). The
air's pitching moment, and the moved terms', before and after, ft-lb,
negative nose down:

| model | angle of attack, deg | the air's moment, before | after | the moved terms, before -> after |
|---|---|---|---|---|
| 737-300 | 89 to 92 | -12,976 | -2,677 | -10,490 -> 0 |
| 747-400 | 89 to 91 | -105,906 | +23,459 | -129,146 -> -2 |
| 787-8 | 77 to 89 | -218,185 | +8,435 | -226,541 -> 0 |
| A320 | -180 to 180 | -37,213 to +42,722 | -5,604 | +-39,970 -> 0 |
| A380 | 73 to 94 | -590,754 | -24,614 | -562,368 -> -11 |
| B-2A | 12 to 124 | -54,705 | -1,501 to +8,074 | -53,498 -> 0 |
| C172P | 11 to 113 | -2,424 | -257 | -2,188 -> 0 |
| C182 | 6 to 118 | -945 | -285 | -711 -> 0 |
| F-15C | 91 to 92 | -2,476 | +281 | -2,755 -> 0 |
| F-22A | 89 to 90 | -15,095 | -5,493 | `CmDh` -9,602 -> `CmDh_alpha` 0, `CmDh` 0 to 19 |
| F-35B | 74 to 103 | -944 | -5 to +5 | -939 -> 0 |
| Learjet 35A | 91 | -1,791 | -85 | -1,706 -> 0 |
| Mosquito | 92 to 119 | -3,106 | -172 | -2,936 -> 0 |
| PA-28 | -14 to 170 | -1,678 | +225 | -1,909 -> 0 |
| S.23, afloat | 2 to 13 | -6,802 | -2,015 to +932 | -5,602 -> -934 |

**What it did to them standing**: the C172P, in 15 kt from the right,
pitched at up to 22.3 deg/s standing on her brakes (her pitch moving up to
5.0 degrees); now 0.9 deg/s and 1.7 degrees. The C182 at up to 5.0 deg/s, now
1.0; the PA-28's pitch moved 1.9 degrees, now 0.2. The jets' and airliners'
pitch moved under 0.2 degrees either way: their weight holds them.

**Their figures** (linux-debug, every test in test_figures.cpp, 148, before
and after; all pass both ways). 134 print the same to the printed digit,
every C172P, C182, F-22A, F-15C, B-2A, A320 and A380 figure among them.
Moved: the S.23's long-range water take-off run 823.1 -> 798.3 yd (range
676 to 914) and time 34.75 -> 34.11 s (26 to 35), and her standard weight's
26.83 -> 26.83 s; the 787-8's take-off field length 10,884 -> 10,877 ft; the
Learjet's 5,348 -> 5,341 ft; the 737's 7,793.8 -> 7,793.8 and the 747's
11,269.0 -> 11,268.9; the Mosquito's take-off swing 2.61 -> 2.58 lb/sq in of
port lead (0.2 to 3.0), her distance and single-engine ceiling in the third
decimal; the PA-28's ground roll in the third; the F-35B's hover holds by a
few hundredths of a foot (the most, 0.02 -> 0.18 ft in 10 kt across, against
a 10 ft bound). **Why the S.23's run is 3% shorter, traced** (a per-half-
second trace of the 45,000 lb run, both models, not committed): she runs at
1 to 3 degrees of sideslip the whole way, from the throttles opening (2.3
afloat) to the unstick, so `qbarUW` is 0.998 of `qbar` and her nose-down
`Cm_basic` 0.2% less, some 70 ft-lb in 50,000. The two runs agree to the
yard until 34.5 s (692.6 yd); at her rotation her pitch then runs 0.05
degrees higher (9.96 against 9.91 at 37 s), and she comes unstuck half a
second sooner: 798 yd against 823. Not the start, where the angle of attack
is well defined afloat (4 degrees): a figure at the edge of her unstick,
moved by a small steady moment.

**The selftest hash moves**, deliberately: it flies a C172P, whose `Cmalpha`
is now on `qbarUW`. `fefe2a7f5d0f52a5` -> `ba8e97281095464f` in linux-debug
here; its printed flight (roll -0.77, pitch 0.92, heading 349.61, 106.41
KCAS, 20.2 ft/min, 2447 RPM) is the same to every printed digit.

**The learnt landing's parity fixture, flown again.** CI's first run on
this branch (38663200650) failed one test on every platform:
`the_learnt_policy_touches_down_in_the_simulation_where_it_did_in_training`,
27 of 27 landings apart from the fixture's flights by up to 37.46 ft/min,
0.937 m across and 24.70 m along (CI's own summary; per wind in the first
section above). Those flights were
flown by JSBSim's Python bindings on the C172P of the policy's last export,
before this change - and before #198's alpha-rate change and the tail-strike
change, which moved her too, inside the test's 0.5 m. The fixture is
`tests/data/rl/c172p-landing-parity.txt`, written by `tools/rl/export.py`;
it is written again by `tools/rl/export.py --parity-only` (new: it flies the
committed policy file with export.py's own `parity()`, no checkpoint; its
docstring says when), in `~/.venvs/glideslope-rl` on the pinned packages.
On her now, the bindings touch where the simulation does: the test passes
apart by at most 0.23 ft/min, 0.002 m across and 0.18 m along. **The fixture
now records the model it was flown on**: a `model` line, the sha256 of the
c172p.xml, and the test fails by name first when the simulation flies
another ("the parity fixture was flown on the c172p.xml 8929...85a2, and the
simulation flies 1ad0...: fly it again with tools/rl/export.py
--parity-only") - seen, with main's c172p.xml put in the build's copy, and
restored. **Its name is kept, with a note**: "where it did in training" now
means the bindings on the same model, not the model the policy was trained
on; the test's comment says so. (The observation test,
`the_learnt_landing_sees_and_acts_in_the_simulation_as_it_did_in_training`,
works on recorded readings and does not depend on the model; it is not
gated.) The same re-flight on main's C172P does not give the committed file
either (calm, from the left and low: 1.764 m against 1.761), which is the
earlier changes.

**The test.**
`every_flight_models_angle_of_attack_pitch_terms_leave_out_the_crosswind_or_are_named_with_their_reason`
(tests/tools/alpha_pitch_terms.py), a sibling of the alpha-rate check: in
every committed model's PITCH axis, every function that reads
`aero/alpha-rad` or `aero/alpha-deg` and names a dynamic pressure (those
that multiply the alpha rate left to the sibling) must be listed - moved,
every pressure in it `aero/qbarUW-psf`, or named, on `qbar`, with its
reason. Coverage asserted: sixteen aircraft, 19 terms moved and 5 named, and
it fails if the directory, a term or a count differs. **Seen red**: on main's
models, 16 terms on `qbar` and the F-22A's `CmDh_alpha` not found, 2 of 19
moved (the Cub's); a named term moved (the C182's `CmDe`); a term renamed
(unlisted, and the listed one missing); the F-22A's `CmDh_alpha` put back on
`qbar`; an aircraft not listed; and a renamed term on `qbar` written
`aero/alpha-rad </...>`, unseen by the first match and found by the one that
allows the space. Each restored, green.

**Verification.**
**The full suite in linux-debug** (`ctest -j4`, one run, with no display):
of 1,301 tests, 1,274 passed, 25 skipped (the live model calls, the Vulkan
window, and the Windows file-sharing tests), and one failed - the learnt
landing's parity, above, before its fixture was flown again; the run was
stopped by its time limit with one test, the window client's weather, still
going. Both were then run again, with every other learnt-landing test (29),
and passed. **In a `-march=x86-64-v2` debug build** (no fused multiply-adds,
as CI builds) **and in linux-release**: 237 tests - all 148 of
test_figures.cpp, every crosswind heading hold, landing, take-off, wet
runway, walking-pace, tail-wheel, braked-stop and standing test, and the 22
learnt-landing tests - pass in both. The v2 build prints the same 148 figures
as linux-debug to the last digit; release differs in two, the Mosquito's
take-off distance (2,520.30 ft) and swing (2.565). Selftest hashes:
`ba8e97281095464f` in linux-debug and v2, `9a85fba6d1ae67d0` in release.
**On CI**, run on 24f5a9f8 (the models and the fixture): every job on every
platform passed.

**From the review of #206** (all above, in place): the learnt landing's
whole movement, sink, across and along, per wind, where the first draft
said only "up to 0.6 m across"; the fixture made by a committed command
without the checkpoint, and pinned to the model it was flown on; the swept
wing's in-flight case said as it is (`qbarUW` the nearer standing, not
always in flight); the S.23's shorter run traced; the F-22A's split guarded;
the angle-of-attack match allowing a space. The models are unchanged by it
(every script run again; the committed-model tests pass). In linux-debug the
53 tests of the committed models, both term checks and every learnt landing
pass.
