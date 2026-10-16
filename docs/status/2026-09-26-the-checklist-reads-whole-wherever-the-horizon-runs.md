### The checklist reads whole wherever the horizon runs, 2026-09-26 — tail done

**What is not covered first.**
- **The horizon is dimmed under the checklist.** Where it runs behind the
  checklist's panel it shows at half brightness, as it does under the
  credits' strip. It is not cut.
- **Frames under 318 pixels wide.** The panel reaches over the right-hand
  end of the HUD's text block there, but dims nothing of it: the HUD's text
  is drawn over the panel. The checklist's own letters start at
  (w + w % 12) / 2 on a frame w wide, inside the block's 156 pixels only
  where w + w % 12 < 312: every width below 306 (306 to
  311 are clear). There the two can overlap, as they did before this.
- **Banked 90 degrees** the horizon stands upright at the middle of the
  frame, left of or on the checklist's first column, crossing no row inside
  it, so the frames stop at 75 degrees of bank.
- **Not a GPU's frames.** The test paints the HUD's mesh on the CPU, with the
  canvas `glideslope_hud_horizon_check` uses (now `tests/tools/canvas.hpp`).
  The live checklist test still judges a real shot, with the same judgement.

**Cause.** The checklist is drawn down the top right, from half the width;
the horizon reaches two-thirds across. Pitched down and banked, it crosses
the checklist's rows, and `glideslope_checklist_check` compares each line
whole: a stroke through a cell reads as `?`.

**Now** the checklist is drawn over a panel that darkens what is behind it
by half (`gfx::checklist_panel`): a cell wider than its columns each side, a
line above and one below, the empty line `glideslope_checklist_check` reads
under the last. `hud_mesh` draws the horizon, then the panel, then the
aircraft's mark, the HUD's text and the checklist, then the credits - so the
panel dims the horizon and nothing else. (The first version drew the HUD's
text and the mark before the panel; the review found that the panel dimmed
the last glyph of a 24-character HUD line wherever w + w % 12 <= 320 - 312
to 316 wide, and every width below 312 but 311 - and the mark on short
frames.) The
horizon under it is no longer the HUD's colour, so it reads as nothing. The
HUD's approach - keeping the text left of the horizon - does not fit here:
the checklist is already at one screen pixel a font pixel, and starting it
right of two-thirds across would cut every item at a third of the width (33
columns at 640x480, where it has 51).

`glideslope_checklist_check`'s judgement moved, unchanged, into
`tests/tools/checklist_judge.hpp`, so frames built in a test are judged
exactly as shots are.

**Verified.** `the_horizon_across_every_checklist_row_leaves_the_checklist_read_whole`
(`glideslope_checklist_horizon_check`, about 18 s here) checks three things:
- **The panel at every size from 1x1 to 4096x4096, for 1 to 32 lines**:
  536,870,912 layouts. It covers every pixel `read_text` looks at, for the
  lines and the empty one below, in all of them. It starts right of the
  HUD's text block exactly where the frame is at least 318 wide, asserted
  layout by layout: the 41,549,824 layouts narrower (317 widths) reach over
  the block's end, and are held to that count.
- **Nothing over the panel is dimmed**: with the AI flying to
  LOOKOUT_POINT, so the FLYING line fills its 24 columns, and the horizon
  pitched off the frame, the HUD is painted with the checklist and without
  it, and every pixel lit without it must be lit with it. Four sizes, each
  for what it puts under the panel, each held to its own counts: 312x240,
  the whole last glyph (16 pixels) and the mark (12); 316x200, the glyph's
  right-hand column alone (5), the least any width puts under it, and the
  mark (12); 360x200 and 640x240, the mark alone (12 each). 1,020 pixels
  lit in each frame, none dimmed. (317x200, in the first version of this,
  put no glyph pixel under the panel: it starts at 155, the gap after the
  glyph.) Where the panel reaches is geometry, walked at every size above;
  that it dims neither is draw order, the same at every size.
- **1,672 frames**: a nine-item checklist (the most any phase of any
  aircraft has), four ticked, the first too long for its line, on 640x480,
  1280x720, 1920x1080, 800x800, 600x1000, 1080x1920, 474x800 and 360x640.
  For each of its ten lines and the empty one below, the horizon crosses
  the middle of the row at the checklist's first column, at the end of the
  horizon or the checklist, and half way, banked 30, 60 and 75 degrees
  either way, and level. In each the horizon's centre line is checked to
  cross the row inside the checklist's columns; the horizon is whole - every
  pixel under its centre line in the HUD's colour, or dimmed by half under
  the panel; and the checklist is judged whole in all 1,672.

The live `the_checklist_on_screen_is_the_one_the_flight_is_working_through_on_vulkan`
passes with the panel, and its shot shows it; the HUD tests and
`the_horizon_level_with_every_hud_row_is_drawn_whole_and_the_hud_read_and_judged_whole`
pass unchanged.

**Seen to fail:**
- With the panel not drawn: `1672 of 1672 frames misread the checklist; the
  first, 640x480, row 1, bank -75, 0.0 along: line 1 reads "?AKE-OFF 4/9"`.
- With the panel ending at the last line, not the empty one below: `1x1, 1
  lines: the panel [-23, -5) by [10, 30) does not cover what is read,
  [-17, -12) by [20, 37)`.
- With the HUD's text drawn before the panel: `312x240: the pixel at 151,
  80, lit by the HUD, is dimmed by the checklist's panel`.
- With 317x200 in place of 316x200: `317x200: 1022 pixels lit, 0 of the
  text's and 14 of the mark's under the panel, not 1020, 5 and 12`.
- With the aircraft's mark drawn before the panel: `312x240: the pixel at
  153, 119, lit by the HUD, is dimmed by the checklist's panel`.
- With the panel starting a cell further left: `318x1: the checklist's panel
  starts at 150 and the HUD's text ends at 156, and the narrowest the panel
  is clear of it is 318`. (The first version's check, "clear from 474
  wide", could not see that: at 474 the panel has 13 cells to spare. The
  review found it; the exact boundary replaces it.)
