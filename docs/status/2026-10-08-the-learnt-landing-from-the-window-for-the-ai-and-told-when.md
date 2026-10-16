### The learnt landing from the window, for the AI, and told when refused; flaps from the keyboard; protocol version 9, 2026-10-08

**What it is not, first.** **Nothing clears a runway**: an aircraft landed
stops on it and stays, so two AI aircraft on plans ending at one runway
would land into each other (a tail), and `--ai-on-final` puts only the first
plan-file AI aircraft on final for the same reason. **A copilot's route
cannot end in a landing**: `COPILOT_ROUTE` carries waypoints only, and is
left as it was; a model's plan text that says `land` would be read, but no
prompt asks for one. **A 172 a window client
joins on final leaves the gate in seconds if nobody flies it**: left alone
it was 8 degrees off the runway's heading six seconds in, and refused for
it. The client now keeps the server's throttle, flaps, gear and speedbrake
lever, but not its pitch trim, which no update carries; whether the trim is
why is not measured (a tail). The window test asks for the learnt landing
2.8 s after joining, inside that. The refusal on the HUD is not read back
from a shot by any test: the test reads the HUD's lines as the client
prints them.

**What works.**
- **Flaps from the keyboard**: F lowers them a notch and R raises one, a
  third of their travel a press, on the key going down - exactly full at
  three, as the gate asks (`platform::KeyboardControls`). A stick and a
  quadrant already bound them (an axis, buttons and the quadrant's hat); the
  README's controls list them all.
- **A window client joining keeps the server's levers**: `Online::join`,
  given its aircraft, rides along in it (`WATCH`) until an update says its
  controls, rides in nothing again, and the client starts its throttle,
  flaps, gear and speedbrake lever there - and its flight's flaps - rather
  than at its own (flaps up), which its first input ran in.
- **Protocol version 9: `LEARNT_LANDING_REFUSED`** (`0B`), the aircraft and
  why in the server's own words, to the client that asked and no other;
  printable ASCII only - it reaches a terminal - and 160 bytes at most, cut
  by the writer and refused past it by the reader. TRANSPORT.md byte for
  byte, THREATS.md's table, the document-only client at `09`. The window
  client prints it and shows it under who is flying on its HUD for ten
  seconds of flight (`gfx::hud_notice_lines`: capitals, what the font lacks
  dropped, wrapped at 24 columns, at most five lines); the command-line
  client's `--heard` says it.
- **The HUD's FLYING AI LEARNT LANDING is seen**: it was lost while riding
  along in one's own aircraft, which is what the client does while the AI
  flies it (the ridden aircraft's readings replaced the flight's); now it is
  kept there.
- **A flight plan can end in a landing** (`land NAME LATITUDE LONGITUDE
  ELEVATION_FT HEADING_DEG LENGTH_M`, read and refused as `runway` is, once):
  past the last waypoint the controller flies on to six and four miles out
  on the centreline and the glidepath at ten knots over the reference
  speed, then the approach - handed to the learnt landing at its gate where
  the server gave it one (`Controller::lands_learnt`, from every
  controller it makes), the approach autopilot alone otherwise. The server
  takes the threshold's elevation from the ground it collides on, as for a
  take-off. `assets/plans/sydney-arrival.plan` flies from over Chatswood
  down 16R's centreline and lands; the server says when an AI is handed
  over and announces it to every client.
- **An AI's approach handed to the learnt landing at its gate**:
  `sim::Controller::to_ai_approach(runway, speeds, policy)` - the approach
  autopilot flies, and the first step she is inside the gate
  (`sim::outside_learnt_gate` empty) the policy takes her, eased in as any
  hand-over; past it unmet (flared) she is left to the approach autopilot.
  The server's `--ai-on-final` starts the first plan-file AI aircraft three
  miles out, trimmed with the landing flap, flown down so (and again from
  there if it is wrecked), says when it is handed over, and announces it to
  every client as a player's is.
- For tests: the window client's `--press-after S KEY` (the key's event, and
  the key held for one pass) and `--shot-once-landed` (the shot held, up to
  five minutes, until the learnt landing has it at rest: under 0.1 m/s for
  two seconds of the session's clock, as `--until-landed` has it).

**Verification run** (linux-release, the targeted suites; each new test
seen red with a deliberate bug, then the bug reverted):
- `the_client_with_the_window_on_final_is_refused_the_learnt_landing_with_its_flaps_moving_told_why_and_handed_to_it_with_l`:
  joined with flaps 1.00 kept; R at 1.0 s, then L at 1.3 s refused "flaps
  at 91%; the gate is the landing flap, 100%", printed and on the HUD
  (REFUSED: NOT AT THE / LEARNT LANDINGS GATE: / YSSY 16R: FLAPS AT 91;),
  its FLAPS 0.67; F at 1.6 s, and L at 2.8 s handed over, the HUD reading
  FLYING AI LEARNT LANDING; seen at rest, the shot held for it; the server's
  landing 214 ft/min, +1.37 m across, stopped 463 m along, -2.51 m across.
  264 s. Red with the server's flaps not kept.
- `the_servers_ai_c172p_flying_a_plan_that_ends_in_a_landing_is_handed_to_the_learnt_landing_at_its_gate_and_landed`:
  the server with `--plan sydney-arrival.plan` and no other option, 90,000
  steps as fast as they go: handed over at the gate, 216 ft/min, +1.26 m
  across, stopped 477 m along, -2.51 m across; then `--ai-on-final`, 216
  ft/min, +1.23 m across. Red with the plan's landing skipped, and earlier
  with the policy not given to the approach.
- `a_landing_is_read_from_a_plan_and_refused_where_it_is_wrong`: read
  whole; eight ways wrong refused. Red with a second `land` let through.
- The `LEARNT_LANDING_REFUSED` writer puts `?` for any byte outside 20 to
  7E, and the refusal test writes all 256 and reads each back: red with the
  bytes written as they came.
- `an_ai_approach_is_handed_to_the_learnt_landing_inside_its_gate_and_landed_within_its_limits`:
  from three miles out, handed over 2.40 miles out, 214 ft/min, +1.34 m
  across, stopped 477 m along; the same approach with no policy never handed
  over, and stopped by the approach autopilot. Red with the gate test
  inverted (handed over on the first step).
- `a_players_c172p_handed_..._are_refused` now also holds that the C182S's
  client and the one three miles out are told why, and the one landed is
  not told it was refused. Red with the refusal not queued.
- `the_keyboard_sets_the_flaps_a_notch_a_press_to_exactly_full_and_back_up`
  (eight presses - every notch, both keys - and both kinds of device bound):
  red with a held key stepping every frame.
- `every_refusal_the_document_names_for_a_learnt_landing_refusal_is_refused`
  (161 unprintable bytes, a reason of 161 bytes; the longest, an empty one
  and every printable byte read): red with control bytes let through.
- `the_hud_says_who_is_flying_and_where_every_control_is_in_every_case`
  gains the learnt landing and two notices, wrapped and cut: red with the
  wrap a column short.
- The message suites walk eleven kinds; the version pin moves to 9.
