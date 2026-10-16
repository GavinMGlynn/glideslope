### Every model's wing drag keyed to the angle of attack leaves out the crosswind, 2026-10-16 — item done

**What is still not true first.**
- **Drag not keyed to the angle of attack still counts the crosswind
  standing**: each model's drag at zero lift where it is a constant (the
  A320's, C172P's, C182's, PA-28's CDo, the Mosquito's drag-zero-lift), her
  gear's, flaps', Mach and sideslip terms stay on `aero/qbar-psf`. Standing in
  15 kt straight across they still draw her along the wind - the 737 1,110 lb,
  the 747 5,330, the F-15C up to 2,660 (her CDbeta, named below) - as a body
  standing in a beam wind is drawn. That is a sideways force on her airframe,
  not this item's undefined angle; whether its size is right is not measured.
- **The swept wings are moved on the same argument as their lift and pitch
  terms** (simple sweep theory, NACA Report 863; alpha_rate.py), not on a
  measurement of a swept wing's drag standing in a beam wind.
- **CDseparated** (F-15C, F-22A, F-35B) was under 0.1 lb standing on either
  pressure: it goes with her lift coefficient, so this moved nothing that
  shows standing. It is moved as the same wing's drag keyed to the same angle.

**The change.** Each make script names its drag terms keyed to the angle of
attack to `alpha_rate.with_alpha_terms_on_qbar_uw` (axis DRAG) beside its
pitch and lift terms, and its docstring says why, what standing in 15 kt
across did before and after, and which are not moved and why: `CD0` in the
737-300, 747-400, A380, B-2A and Learjet 35A; `Drag_basic` in the 787-8;
`CDwbh` in the C172P; `CDalpha` and `CDDf` (her flaps' drag, keyed to the
angle of attack) in the C182; `CDseparated` in the F-15C and F-35B;
`CDseparated` and `CDDlef` (her leading-edge flaps') in the F-22A;
`drag-stall` in the Mosquito; `CD`, `CdDf3L` and `CdDf3R` in the S.23. The
Cub's `Drag_basic` was moved in #205. The A320's and the PA-28's drag is not
keyed to the angle of attack: nothing to move. **Named, not moved**: the
F-15C's `CDbeta` (drag with sideslip: keyed to the sideways air by design,
the angle of attack only picks its coefficient), the F-22A's `CDq` and
`CDq_Dlef` (drag with pitch rate), and the S.23's `CdDe` (her elevator's
drag, not her wing's). The reasoning is her lift's: a wing's drag at its
angle of attack is made by the air in the plane of u and w; the sideways part
runs along the span. It is the term that rolled the Cub onto her wing
standing in a beam wind (2026-10-15). alpha_rate.py's docstring says so.

**Standing on full brakes in 15 kt straight across** (linux-debug, a probe
not committed: each landplane settled ten seconds in calm air, the wind on
from either side, the next fourteen seconds read; the S.23 afloat). The moved
terms, lb along the wind, before and after, and her angle of attack:

| model | angle of attack, deg | the moved terms, before | after |
|---|---|---|---|
| 737-300 | 0 to 96 | 19 to 1,339 | 0 |
| 747-400 | 0 to 92 | 73 to 6,451 | 0 |
| 787-8 | -82 to 96 | 40 to 4,266 | 0 |
| A380 | 0 to 91 | 114 to 11,062 | 0 to 0.2 |
| B-2A | 0 to 95 | 32 to 5,577 | 0 |
| C172P | -51 to 134 | 0 to 11 | 0 |
| C182 | -180 to 180 | 0.4 to 23 | 0 |
| F-15C, F-22A, F-35B | up to -153 to 157 | 0 | 0 |
| Learjet 35A | -52 to 99 | 4 to 309 | 0 |
| Mosquito | -178 to 178 | 0 to 104 | 0 |
| S.23, afloat | -3 to 13 | 21 to 169 | 1.5 to 33 |

The air's whole drag on them standing (`forces/fwx-aero-lbs`) fell with it:
the 747's 5,409-11,778 lb to 5,329-5,337, the A380's 2,916-13,852 to
2,790-2,803. Their pitch standing moved as before (each under 0.2 degrees but
the C182's 0.87 and the S.23's 0.44 afloat, as on main).

**Their figures** (linux-debug, every test in test_figures.cpp, 167, before
and after; all pass both ways). Moved: the Learjet's take-off field length
5,378.9 -> 5,376.9 ft (range 4,770 to 5,830); the 747's 11,269.4 -> 11,269.7,
the 737's and 787's in the second decimal; the Mosquito's take-off distance
2,522.02 -> 2,522.07 ft; **the Mosquito's take-off swing 2.614 -> 2.573 lb/sq
in of port lead** (-1.6%, range 0.2 to 3.0; standing on her brakes at +9
boost, the term her unmoved `lift-slipstream` sets, 2026-10-15's entry); the
S.23's long-range water take-off 798.9 -> 801.7 yd (+0.35%) and 34.13 ->
34.19 s, her standard 26.83 -> 26.82 s; the F-35B's hover turn by 0.01 ft.
Every other figure prints the same.

**The other flights** (linux-debug, 217 tests: every crosswind heading hold,
landing, take-off, wet runway, walking-pace, braked-stop and standing test,
and every learnt landing): all pass (two need a DEM the binary run had not
fetched, skipped both ways). 43 print something different, all small but
one: the walking-pace autobrake's rolling most for the C172P at 3 kt 0.286
-> 0.289; braked to a stop in 15 kt across the C172P's most pitch rate 5.5
-> 5.4 deg/s; the steady-crosswind stops within a metre and a tenth of a
second; the heading holds in 20 kt across by a hundredth of a degree; the
learnt landing from its gate's corners in calm air worst 0.97 -> 0.99 m
across and 158 -> 146 ft/min. The one not small is the take-off lesson's
fault run of the S.23 held nose low on the step - a run that is meant to go
badly, its fault in the debrief either way: off at 82.5 kt after 80.5 s and
71 swings, now 75.7 kt after 71.0 s and 55; flown by the book she is off at
79.2 kt after 38.6 s against 79.3 after 38.7.

**The selftest hash moves**, deliberately: it flies a C172P, whose `CDwbh` is
now on `qbarUW`. `07cc7d4a4d42a300` -> `ac2e6c3cf5e96481` in linux-debug; its
printed flight (roll -0.77, pitch 0.92, heading 349.61, 106.41 KCAS, 20.2
ft/min, 2447 RPM) is the same to every printed digit.

**The learnt landing's parity fixture, flown again** with
`tools/rl/export.py --parity-only` (its `model` line now 8a0a3cc6...9510).
Against the fixture's old flights, from the 27 verification starts (new less
old): **calm air**, 13 to 16 ft/min less sink (now 133 to 142), 0.02 to 0.06
m more across (now +0.68 to +0.96), 7 to 10 m longer (now 147 to 165 m
along); **10 kt from one side**, 9 to 15 ft/min less sink (now 144 to 169),
0.10 to 0.13 m more across (now +0.37 to +0.98), 14 to 16 m longer; **10 kt
from the other**, 8 to 9 ft/min less sink (now 202 to 204), 0.08 to 0.09 m
more across (now +0.64 to +0.72), 6 to 7 m longer. All within her limits (5
m, 300 ft/min), all touched down. She flies her approach slipped (2026-10-15's
entry), so her drag there is a few percent less and she floats a little
further. The policy is not trained again.

**The test.**
`every_flight_models_angle_of_attack_drag_terms_leave_out_the_crosswind_or_are_named_with_their_reason`,
by the lift check's script (tests/tools/alpha_lift_terms.py), which now takes
the axis, LIFT or DRAG, with a stated list for each. Coverage asserted:
sixteen aircraft, 18 drag terms moved and 4 named (the lift's 18 and 4 as
before), and it fails if the directory, a term or a count differs. **Seen
red**, each on a copy of the models: main's models (17 of 18 on `qbar`, only
the Cub's moved); the 737's `CD0` alone put back on `qbar` (17 of 18); a
named term moved (the F-22A's `CDq`); a named term renamed (the S.23's
`CdDe`: unlisted, and the listed one missing). Each restored, green.

**Verification.** In linux-debug: the 167 figures and 217 flights above; the
term checks, every committed-model test (each make script writes the
committed models unchanged; `--check` on all sixteen) and the selftest's
(`ctest -R "committed|every_flight_models|selftest"`, 32 tests); and every
test whose name has every_landplane, committed or figures (52, all pass).
**In a `-march=x86-64-v2` debug build** (no fused multiply-adds, as CI
builds): the same 384 tests, every one printing what linux-debug prints, to
the last digit; the selftest hash `ac2e6c3cf5e96481` there too. The full
suite is CI's.
