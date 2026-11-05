### Phase 10b's gust items close on their measured figures, 2026-11-05

**What closes, and on what**: "The AI cannot land every aeroplane in gusts"
and "The F-22A flies her stall-figure tests" are ticked. This applies two of
the owner's rulings:
- **2026-11-04:** "close single-seed names on measured figures";
- **2026-10-09:** blocked items close on measured figures.

**What is still not true**: the F-22A does not land every gust seed. She is
named on twelve of forty seeds on Linux and on macOS, and on fourteen on
Windows. Each name carries its figure in `named_on_a_seed()`
(tests/unit/test_lesson.cpp), and each fails "take her name off" if she
passes the seed. In all, 69 names remain, each one seed on one platform,
in `named_on_a_seed()` on 2026-11-05:

| Aircraft | Linux | Windows | macOS |
|---|---|---|---|
| F-22A | 9, 10, 11, 13, 15, 20, 21, 28, 32, 34, 39, 40 | 1, 3, 5, 9, 11, 15, 16, 17, 19, 22, 23, 28, 31, 37 | 4, 6, 7, 9, 10, 11, 23, 27, 31, 33, 35, 37 |
| J-3 Cub | 33 | 8, 32, 37 | 9, 10, 30, 40 |
| A380 | | 3, 7 | 5, 6, 31, 37 |
| 747-400 | | 29, 31, 33, 40 | 24 |
| 737-300 | | 8 | 3, 17 |
| 787-8 | | 4 | |
| F-15C | 34 | | 28 |
| C182 | | | 16, 34 |
| C172P | | | 16 |
| PA-28 | | | 34 |
| S.23 | | | 18, 32 |

**Why this is blocked, not unfinished**: her touch sink scatters about 1.7
times as widely as the F-15C's, with a standard deviation of 97 ft/min
against 57. No sourced change has moved her beyond the noise around her
70% of 120 seeds. The changes tried were:
- her flight controls fed body rates (#327, which raised her from 2 to 28 of
  40);
- a fighter's slower approach at angle of attack;
- about sixty sweeps of her figures and the gust flare's laws
  (2026-11-05-the-f22a-s-gust-failures-are-her-airframe-s-scatter-not-a-figure).

What is left is tuning her airframe's answer to a gust, which no source
supports.

**What was checked**: no aircraft is in `not_yet_landed_in_gusts()`, and no
loading is named. Every remaining name is a single seed on a single
platform, measured on CI:
- the full workflow runs 41867348299 (F-22A), 41814841853 (A380),
  41773183933 (Cub) and 41732333742 (S.23);
- the trains' full runs since.

The other F-22A stall-figure tests pass without her name on every platform:
the stall watch, both recoveries, her rise after the touch, the wet walking
pace and the wet crosswind roll-out.

**The owner can reopen both items.** If the F-22A's names should not count
as single seeds, untick both and reopen her gusts as the open part.

No code changed; the selftest hash is unaffected.
