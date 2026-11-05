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
- `fly_again`, for a wreck whose start is within 1.5 nm of the plan's
  start, first asks whether anything flying holds its own height, measured
  from its own start (`held_at`). Every such wreck is checked except a
  planned aircraft that takes off from its runway. That one already waits
  for its runway to be clear (`may_fly_again`). If the height is held, the
  wreck is given `free_layer()` instead, counted from the plan's start, and
  keeps that start from then on. Its plan moves with it: a plan-file AI
  aircraft's stack height, or every waypoint of a plan of its own, so its
  autopilot does not fly it back to the layer it left (review of #344).
- It says where it flies again, moved or not: `aircraft N, ID, flies again
  H ft over the plan's start`.

Two test flags build the case on events, not on a clock (review of #344):
- the client's `--wreck-file FILE`, written the first time it hears any
  aircraft become a wreck, which another client's `--after-ready FILE`
  joins on;
- the server's `--test-wreck-waits-for-joins N`: no wreck flies again until
  N players have been given aircraft. The wreck then waits for that join,
  however slow it is, rather than flying again after five seconds.

**Verification.**
`a_wreck_flown_again_while_a_player_is_at_the_start_on_its_layer_is_kept_500_ft_from_her`
(`tests/cmake/server_wreck_flown_again_layer.cmake`) builds the case with
three clients on a two-player server with one AI aircraft (with none, the
server never steps). A joins on layer 2 and leaves once C is in the
session (C's `--told-file`). C joins on layer 3 and dives into the water. B joins when C's wreck is heard, so the
count is two and B is put on layer 3, C's. The
test asserts the case was built: one wreck, then B's start, then C flown
again, in that order, with A gone before the wreck and B on C's layer. It
then asserts C flies again at least 500 ft from B's height, both as the
server prints them (they are both at the start, within 1.5 nm, so
height is what separates them).

**Seen to fail** with the move taken out of `fly_again` (`taken` forced
false, the line still said): "B joined on layer 3 (3000 ft) while C was a
wreck; C flew again at 3000 ft, 0 ft from her", red on "C flew again 0 ft
from B, at the start: within 500 ft and 1.5 nm of her". Seen again after
the review's changes, on the event-built test, with the same figures.

Green with the fix restored: C flew again at 4,000 ft, 1,000 ft from B,
"its own layer was taken while it was a wreck". 43 s on linux-debug.

The other join, layer and wreck tests still pass: 101 tests matching
`join|layer|wreck|collid|flies_again|flown_again|usage|help` on linux-debug,
93 passed and 8 skipped (keys or a display not present), none failed.
After the review, `every_flag_the_server_prints_in_its_usage_is_one_it_takes`
passes with the new flag listed.
