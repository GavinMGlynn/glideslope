### A wreck flown again is kept off a joining player's layer, 2026-11-05

**What was missing.** A player joining is put on the lowest free layer at
or above one over everything flying, skipping any layer an aircraft holds
near the start. A wreck holds no layer: it is not in the sky. But five
seconds later it flew again from its own start, on its own layer, and that
could be the layer a player had just been given. The two were then on one
layer near the start, 0 ft apart.

**What changed** (`src/frontend/server/main.cpp`).
- The join's search is now two functions, `held_at_start` (whether
  anything flying is within 1.5 nm of the start and 700 ft of a height)
  and `free_layer` (the lowest free layer by the join's rule). The join
  calls `free_layer`; what it does is unchanged.
- `fly_again`, for a wreck whose start is in the air within 1.5 nm of the
  plan's start, asks `held_at_start` about its own height first. If that
  height is held, it is given `free_layer()` instead, and keeps that start
  from then on. A plan-file AI aircraft's stack height moves with it. A
  planned aircraft that takes off from its runway is left alone: it already
  waits for its runway to be clear (`may_fly_again`).
- It says where it flies again, moved or not: `aircraft N, ID, flies again
  H ft over the plan's start`.

The client gains a test flag, `--wreck-file FILE`, written the first time
it hears any aircraft become a wreck. Another client's `--after-ready FILE`
then joins during the wreck's five seconds, on the event rather than a
clock.

**Verification.**
`a_wreck_flown_again_while_a_player_is_at_the_start_on_its_layer_is_kept_500_ft_from_her`
(`tests/cmake/server_wreck_flown_again_layer.cmake`) builds the case with
three clients on a two-player server with one AI aircraft (with none, the
server never steps). A joins on layer 2 and leaves after 3 s. C joins on
layer 3 and dives into the water. B joins when C's wreck is heard, so the
count is two and B is put on layer 3, C's. The
test asserts the case was built: one wreck, then B's start, then C flown
again, in that order, with B on C's layer. It then asserts C flies again at
least 500 ft from B's height (they are both at the start, within 1.5 nm, so
height is what separates them).

**Seen to fail** with the move taken out of `fly_again` (`taken` forced
false, the line still said): "B joined on layer 3 (3000 ft) while C was a
wreck; C flew again at 3000 ft, 0 ft from her", red on "C flew again 0 ft
from B, at the start: within 500 ft and 1.5 nm of her".

Green with the fix restored: C flew again at 4,000 ft, 1,000 ft from B,
"its own layer was taken while it was a wreck". 43 s on linux-debug.

The other join, layer and wreck tests still pass: 101 tests matching
`join|layer|wreck|collid|flies_again|flown_again|usage|help` on linux-debug,
93 passed and 8 skipped (keys or a display not present), none failed.
