### Single-seed gust names close on their measured figures, 2026-11-04

**The ruling**: the owner ruled on 2026-11-04 to "close single-seed names on
measured figures". A gust landing named on one seed on one platform (in
`named_on_a_seed()`, tests/unit/test_lesson.cpp) no longer keeps its plan
item open. Its measured figure stays in the name, and the gust test still
fails with "take her name off" if she passes a seed she is named on, so a
name cannot outlive its failure.

It does not cover:
- **An aircraft named whole** (`not_yet_landed_in_gusts()`, today the
  F-22A). She is still to be fixed.
- **A loading named as failing**: the 737-300 light, and the Mosquito at her
  heaviest. These are still to be fixed.

**Why**: touch sink scatters about 80 ft/min a landing between platforms
(the gust item's own figure). Most single-seed names are inside the limit but
short of its margin on one platform, and tuning one seed moves another, so a
rule of none named does not converge. This follows the owner's 2026-10-09
ruling that blocked items close on measured figures.

**What it closes**: the flying boat's gust item. The S.23 lands every gust
seed on Linux and Windows, and every macOS seed but 18 and 32, where she
touches at 342 and 340 ft/min against her 360 limit (CI run 41732333742;
2026-11-04-the-flying-boat-held-off-in-gusts-with-her-own-pitch-gains).
Both names stay.

**Go-arounds on a single seed close the same way**: they are single-seed
names too. Two remain: the F-15C on Linux seed 34 and the 737-300 on macOS
seed 3.

**What it changes**: the gust item's verification now reads "no aircraft
and no loading named", with single-seed names closing on their figures. The
gust item stays open for the F-22A, for the A380 until #322 lands, and for
the two loadings.

**Verification**: no code changed. The names and their stale-name check are
unchanged: `every_aeroplane_lands_in_gusts` in tests/unit/test_lesson.cpp.
