### A landing flown by hand is braked for the runway she is on; every control's key and button on screen with F1, 2026-10-08 — two tails done

**What is still missing first.** A landing handed over on its roll off any
runway of the world's file - a strip it does not have, or grass - is still
held to her track and braked at autobrake 3, as before. The help is drawn
only on the flight screen, and on a frame too small for it (below about
1,000 by 500 pixels) it is cut, saying so on screen; the client's own
1280x720 holds all of it. The HUD's numbers show faintly through its panel.

**The runway of a hand-flown landing.** `Controller::finds_runways_with`
gives a controller a way to ask which runway she is rolling on, as
`lands_with` tells it how she lands: the simulation does not know the
world's runways, so the server's `controller_for` and the client's `Flight`
give it `world::runway_rolled_on` over the collision ground's own runways
(`assets/runways/strips.csv`). That finds, of the runways whose rectangle
she is on, the end whose heading her track over the ground is nearest,
within 30 degrees - her heading when barely moving - with the ground's
elevation at that end; none off every rectangle or rolling across them.
Given one, the take-back on the roll is `Lander::on_its_roll(aircraft,
speeds, runway)`: the runway's centreline held, and the autobrake set as
her own landing's is at the rollout's first step, for the length left past
where she is less 300 m (clamped 5 to 14 ft/s squared). Without one, it is
her track and autobrake 3, as on 2026-10-06.

**Verification.**
- `a_landing_flown_by_hand_on_a_short_runway_is_stopped_on_it_by_the_ai`:
  the landed-by-hand take-back of every landplane taught the approach (13;
  the flying boat named), the controller told it rolls on a 1,500 m runway.
  All stop on it, 456 m (the Cub) to 1,365 m (the F-35B) past the
  threshold, within 3.0 m of the centreline. **Seen to fail** with the
  runway asked for and not used: six run off its end - the 737-300 at
  1,518 m, the F-15C 1,799, the F-35B 1,992.
- `an_aircraft_rolling_on_a_runway_of_the_worlds_is_given_that_runway_towards_where_she_rolls_and_off_one_none`:
  a C172P at Sydney rolling at 40 kt, nine cases walked of nine - down 16R,
  up 34L, down 16L, along 07 and 25, each way at the crossing of 07/25 and
  16R/34L, across 16R (none), between the parallels (none) and 40 m off
  16R's centreline on its shoulder (none) - ten of ten; elevation the
  ground's, length the runway's. The crossing is asserted to be on both
  runways' rectangles, and the parallels' places each on its own and off
  the other's. **Seen to fail** with the 30-degree limit taken off:
  "across 16R: given 'YSSY 34L', not ''"; with the first end within 120
  degrees taken rather than the best: "down 16R at the crossing: given
  'YSSY 07'"; and with the rectangle not asked: "across 16R: given 'YSSY
  25'". A wet or contaminated runway's longer stop is not known: it is
  braked for a dry one (a tail).
- `an_aeroplane_landed_by_hand_and_handed_over_on_its_roll_is_landed_to_a_stop`
  and the taxiing test pass unchanged (no runway given).

**Every control on screen.** F1 shows and hides the controls' help over the
flight (`--show-help` starts with it shown). Its lines are
`platform::controls_help`, made from the bindings file as the client reads
it - under STICK and THROTTLE a line a control, with every axis, button and
hat bound to it and its step - and from two tables in `platform/input.cpp`:
`keyboard_bindings`, which `KeyboardControls` now works from (it had its
keys written into its code), and `command_keys`, the client's own keys
(F1, A, L, M, C, T, W, V), which the event loop now asks for by name. So
the help says what the keys do because it is made from what they do.
`gfx::help_columns` lays it out small, in columns from two cells down to a
line above the credits' strip, over a panel darkening the frame to a fifth;
the credits stay on screen below it. What does not fit the width is cut,
the last line drawn saying "WIDEN THE WINDOW FOR THE REST".

**Verification.**
- `the_help_on_screen_names_every_binding_in_the_bindings_file_every_key_and_every_command`:
  56 of 56 bindings, each a whole item on its control's line under its
  device, 9 of 9 keyboard bindings with their control (F and R, the flaps a notch, since #122), 8 of 8 client keys with
  what they do, every character the font's. **Seen to fail** with the hats
  left out: "HAT 0 UP -0.05 (STICK, PITCH TRIM) is on the help's line".
- `the_controls_help_on_screen_reads_back_as_every_line_the_client_drew_on_vulkan`
  (`tests/cmake/frame_help.cmake`, `glideslope_help_check`): the flight
  shot at 1280x720 with the help showing; all 43 lines drawn, read back
  off the frame column by column as the client said it drew them. **Seen to
  fail** with the help drawn a line lower than laid out: "column 0 line 0
  reads '', not 'KEYS'".
- `the_controls_help_is_laid_out_inside_every_frame_size_and_says_so_where_it_is_cut`:
  782 sizes of 782 (160x120 to 1920x1080, with and without credits): 486
  whole, 262 cut and saying so, 34 too narrow even for that line, which
  draw nothing; every column inside the frame, a line above the bottom, in
  order. **Seen to fail** with the cut line added after a full column rather
  than in place of its last line: "400x120 with credits: a column is
  outside the frame, less than a line above the bottom".
- The keyboard and joystick tests pass unchanged on the tables
  (`the_keyboard_moves_every_lever_...`, `every_axis_button_and_hat_...`).
