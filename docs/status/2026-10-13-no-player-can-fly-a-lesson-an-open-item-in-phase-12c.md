### No player can fly a lesson: an open item in Phase 12c, 2026-10-13 — docs only

Lessons and the instructor (Phase 5c, ticked 2026-09-23 and 2026-09-24) are
flown only through the simulation's `LessonRun`, by the tests' instructor and
stall flights: nothing in `src/frontend/` - the window client, the server or
`glideslope_cli` - starts a lesson or hands an aircraft to the instructor, found
while reviewing #169. `FEATURES.md` has lessons as `CORE` and the instructor as
`WANTED`, so a player would notice both missing. The simulation's part stays
ticked, as its verifications are met in it; reaching a lesson from the client
is a new open item in Phase 12c, the window client.

The dates written 2026-10-11 in the plan, this file, `REQUIREMENTS.md`,
`ASSETS.md`, the README and `tools/next_item.sh` were 2026-10-10, the day the
work was done; corrected here.
