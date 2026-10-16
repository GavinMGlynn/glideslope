### A lesson flown into the ground says what it needed, 2026-09-25

**Found by this pull request's macOS runs.** `a_stall_recovered_badly_is_named_in_the_debrief`
failed on both macOS jobs: the B-2's late recovery left a debrief that named
nothing.

**The cause was the test's, and a gap in the runner.**
- **The test.** The late recovery waited 35 s after the stall. A B-2 mushing
  that long, 12 degrees nose up at 98 kt, cannot be recovered by the
  autopilot's recovery, and falls from 20,000 ft to the ground. On main, on
  Linux, it bottomed out 9 ft up: a knife-edge the bank-limit change tipped.
- **The runner.** On macOS the B-2 reached the ground, where its recovery
  stage could never end. A need is judged only when its stage ends, so
  nothing was said.

**What changed.**
- **`LessonRun::ended`.** A flight that ends before its lesson does, in the
  ground most likely, has the current stage's needs judged as they stand. A
  stall recovery that ends in the ground did not recover with the least
  height. A lesson merely abandoned is still left unjudged.
- **The stall test ends the flight at the ground and calls it.**

**Verified** by `a_lesson_whose_flight_ends_part_way_names_what_its_stage_still_needed`.
A stage that never reaches its end has its unmet need named, and its met need
not. The lesson is over, and nothing after is judged. With `ended` judging
nothing, it goes red. The stall test names the B-2's fault on Linux at the
ground (0 ft).

The B-2 that cannot be recovered after half a minute of mushing is a tail.
