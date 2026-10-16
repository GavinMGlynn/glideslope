### The HUD's text is kept left of the horizon, which is drawn whole, 2026-09-25 — tail done

**What is not covered first.**
- **Frames narrower than 474 pixels.** Even the smallest text reaches into
  the middle third there. The horizon is still drawn whole, over the text,
  and a row it crosses reads `?` where it does.
- **The checklist.** It is drawn down the top right, from half the width, so
  the horizon can still cross its rows. That is a tail.
- **Smaller text.** On a 640x480 frame, the size the HUD tests shoot, the
  text is now one screen pixel to a font pixel, half the size it was. At
  1280x720 it is two, where it was three.
- **Not a GPU's frames.** The test paints the HUD's mesh on the CPU. The live
  HUD tests still judge real shots, with the same judgement.

**Cause.** The horizon runs across the middle third of the frame. At
640x480 the HUD's 24 read columns, at two screen pixels a font pixel, reached
from 24 to 312 pixels, past the third at 213. Pitched and banked, the line
could cross the right-hand end of any row, and a stroke through a cell reads
as `?`. Rows 1-6 need an exact word count, the FLYING and GEAR lines are
compared whole, and `frame_hud.cmake` looks for `FLYING AI NAV THE HEADS`:
a `?` a few columns after the text broke each of them.

**Now** the text is kept clear of the horizon, and the horizon is not
touched. `gfx::hud_layout` draws the text at a scale no larger than keeps the
text block ending a pixel short of the furthest left the horizon reaches: a
third of the width, less half the line's thickness. The block is the HUD's
lines, each `gfx::hud_columns` (24) cells wide, two cells in. The cap applies
up to the old scale of a pixel for every 240 of the smaller side, and never
goes below one. `gfx::hud_text_clear_of_horizon` says whether a size manages
it, which it does from `gfx::hud_narrowest_clear_width`, 474 pixels wide.
`hud_lines` cuts the FLYING line at 24 characters, so a long waypoint name
cannot carry the text past the block. The horizon is drawn whole, always.

A first version of this cut the text block out of the horizon instead. That
removed about half of a level horizon at 640x480, and nearly all of it on
square and portrait frames. It is a flight instrument, and the review refused
it.

`glideslope_hud_check`'s judgement moved, unchanged but for reading
`gfx::hud_columns` columns, into `tests/tools/hud_judge.hpp`, so frames built
in a test are judged exactly as shots are.

**Verified.** `the_horizon_level_with_every_hud_row_is_drawn_whole_and_the_hud_read_and_judged_whole`
(`glideslope_hud_horizon_check`) checks two things:
- **The layout at every size from 1x1 to 4096x4096**, 16,777,216 sizes. The
  text is clear exactly at the 14,839,808 at least 474 wide. Where it is
  clear, the block ends a pixel short of the horizon's reach. The scale is
  the largest that keeps it so.
- **23,712 frames.** Cases: the pilot flying, the AI holding, the AI flying
  to THE_HEADS; slow and low, and with Mach and flight level; gear fixed, up
  and down. Sizes: 640x480, 1280x720, 800x800, 600x1000, 1080x1920, 474x800
  (the narrowest clear), and 473x600 and 360x640, which are too narrow. For
  every row of each, the horizon meets the middle of the row's height at its
  left end, its middle and its right end, banked 30 and 90 degrees either
  way, and level. In every frame the horizon is whole: every pixel under its
  centre line, on the frame and above the credits' strip, is in the HUD's
  colour, except the last pixel at each end. Where the text is clear, the
  horizon never comes within half its thickness of the text block, and the
  HUD is judged whole in all 17,784 frames. Where the frame is too narrow,
  the HUD is judged in the 4,512 frames where the horizon misses the text.
  The 1,416 where it crosses are counted, and there must be some.
- Each size is walked on a thread of its own: about 30 s in the sanitized
  debug build here.

`the_hud_says_who_is_flying_and_where_every_control_is_in_every_case` adds
two long waypoint names, cut at 24 characters, one of them at a space. The
four live HUD tests pass with the smaller text.

**Seen to fail:**
- With the scale not capped, the layout walk fails at 480x480. With that
  walk and the geometric guard taken out, the judgement fails:
  `800x800, ... row 1, bank -30, +1.00 along: line 12 reads "GEAR UP ??"`.
- With half the horizon drawn: `the horizon is not whole: 211 of 422 points
  on the frame lit`.
- With the FLYING line not cut: `line 7 says "FLYING AI NAV NORTH HEAD
  LOOKOUT POINT"`.
- The first version's test was seen to fail too, but its count of rows
  crossed held nothing that could fail. The review found that; the whole
  horizon and the layout walk replace it.
