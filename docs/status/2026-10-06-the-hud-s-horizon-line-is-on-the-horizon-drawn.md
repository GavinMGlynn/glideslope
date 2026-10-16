### The HUD's horizon line is on the horizon drawn, 2026-10-06 — tail done

**What is not shown first.** The walk is the cockpit view, banks -60 to 60
and pitches -15 to 15. Outside views draw the line from their own camera
the same way, but no frame of one is judged; nor is flight upside down,
nor a bank steeper than 60 degrees, where the line stands too near upright
for the check's columns. At a bank near 90 degrees with the horizon to one
side of the middle column, the line - centred on that column - is off the
frame although the horizon is on it. The line is the true horizontal, not
the visible horizon, which lies below it by the Earth's curve: 1.8 pixels
from 63 m on a 480-pixel frame, more higher up.

**The line.** `gfx::hud_horizon` moved its line a hundredth of the frame's
height a degree of pitch, where the camera's 60-degree field of view puts
the horizon 1.5% of the height a degree near the middle, and more further
out. Now it is the horizon as the camera projects it: the sky - up from the
ellipsoid under the eye - in the camera's axes (`HudReadings::sky_in_camera`)
and its field of view (`HudReadings::vertical_fov_rad`) give the line
n0 u + n1 v = n2 f, drawn across the middle third as before, centred where
it crosses the middle column - so the HUD's text, left of the middle third,
stays clear of it as `hud_text_clear_of_horizon` says. The client gives
every frame its camera's sky, in every view; without one (the HUD's own
tests) it is the cockpit's, from the readings' pitch and roll. Held at most
four frames' heights off the frame. `gfx::hud_pitch_for` gives the pitch
that puts the line's centre on a row, for the HUD and checklist horizon
tests, which used the old hundredth; the checklist test's horizon off the
frame is now at pitch 89, not 200 (which the true projection puts back on
the frame), and its text "PITCH +89.0" lights 1,000 pixels, not 1,020.

**For the test, `--attitude PITCH,BANK,HEADING`**: the client's flight
begun in the air at that attitude (`InitialConditions::pitch_deg` and
`roll_deg`, JSBSim's theta and phi at the start, 0 and 0 by default as
every start was). The selftest is untouched.

**Tests.**
- `the_huds_horizon_lies_on_the_horizon_drawn_banked_{minus_60,minus_30,0,30,60}_on_DRIVER`
  (`tests/cmake/frame_horizon.cmake`, `tests/tools/horizon_check.cpp`):
  the C172P over the Nullarbor (-31.0, 129.0) 63 m up, imagery off, clear
  air, cockpit view, each bank by pitches -15, 0 and 15 - fifteen frames.
  The check confirms the line the client names is the line drawn, then in
  every column outside it finds where the sky ends nearest the line carried
  on across the frame, in 135 to 427 columns a frame; measured, 1.7 to 2.7
  pixels below the line on average, 3.24 at worst: held to 4. About 150 s a
  bank in linux-debug; `tests/ci_costs/` has estimates (160 debug, 50
  release), not measurements. **Seen to fail** with the old scale put back
  (a degree of pitch 0.573 of the height a radian): "35.35 px at worst" at
  pitch -15.
- `the_horizon_level_with_every_hud_row_is_drawn_whole_and_the_hud_read_and_judged_whole`
  and `the_horizon_across_every_checklist_row_leaves_the_checklist_read_whole`
  pass on the new projection, through `hud_pitch_for`.
- The HUD's frame tests (`the_hud_shows_*`, `the_hud_says_*`, the weather
  skips) pass unchanged.
- **Built on Windows** (`tools/windows_build.sh windows-debug`): the first
  build refused the check's `sscanf` as unsafe (C4996, an error), as a test
  tool's had once before; it reads the line with `strtod` now.
