### Through a server, a player's F-35B too heavy to hover is held wing-borne in STOVL mode; the stick never the surfaces when the law takes her; the selector through a real server, 2026-10-14 — item still open

**What is still missing, first.** **No player's vertical landing is flown
through a server**: a server gives a player's F-35B her model's own
loading, 45,400 lb - nothing in the catalogue, the plan or the
initiation chooses another - and that is over the 39,837 lb she is now
flown into a hover at, so through a server her STOVL mode holds her
wing-borne. The multi-process test below flies the selector through a
real server and holds the client to it; it does not convert her, hover or
land her. Closing the item needs a server that gives a player a loading
(and a prediction told it). **The hover limit is estimated**: her hover
thrust (Pratt & Whitney's 40,650 lbf) less 2 per cent for control; none
is published. **And what CI run 38517430777 found, still not true**
(below): a player's F-35B that nobody has touched dives - the server
gives her untrimmed at 300 KCAS and holds the stick centred, and she is
sinking 5,600 ft/min by five seconds and 10,400 by ten - so how far she
has dived when her selector arrives is how long the player took to join.
Now pulled out from any moment of her first ten seconds; **not from
later** - by 12.5 s she is at 780 ft sinking 12,800 ft/min, past what the
autopilot's 3 degrees a second of pitch saves. The untrimmed start is a
tail in COMPLETION_PLAN, not fixed here. **CI's debug abort is not shown
to be the wreck's replay**: a replay through the ground goes to a NaN in
one process only when every input since the wreck is flown again at every
word; the server's `last_applied` goes on rising through a wreck, so each
word flies 12 steps again, and flown so over flat ground nothing went
non-finite even without the hold. **A prediction is held at a wreck only
once the server's word says so**: for the trip before that word it flies
on into the ground, as before; and the windowed client is given the same
hold, compiled here and not run against a wreck. **Taken over in a climb
past its envelope, the autopilot walks the nose down at 3 degrees a
second whatever the seed**, and at 300 KCAS that pushes an F-15C to -1.2 g
(before and after this change alike) - not held by any test.

**CI run 38531751565 (f70c04da, with the fixes) is still red in this
test on four jobs**, and none of it is reproduced here:

- **Ubuntu linux-debug: wrecked twice, and the client aborted** on the same
  table assertion, the wreck hold notwithstanding. Its client sent 762
  inputs in the server's 68.8 s - about 11 a second, not 30 - so its
  150th input, the selector, came far later than ten seconds into her
  untouched dive, which nothing pulls her out of; the server says she was
  rolled to 180 degrees. Where the client's NaN came from is not found:
  not a wreck held (the hold is in this build), so a prediction flown into
  the ground before the word, or after one flying again, is what is left.
- **Windows release and debug: the worst error 3.56 and 2.99 m** (bound
  2), the 95th percentile 1.28 and 1.22 m, held wing-borne (lowest 768 and
  398 ft); **macOS release: the 95th percentile 0.890 m** (bound 10 cm),
  lowest 1,081 ft. A step of the clocks' difference at her 200 KCAS is
  0.86 m: on these runners one in twenty updates or more is placed a step
  off, where here none are. Not explained.
- **Ubuntu linux-debug, `a_pressed_during_a_take_over_hands_the_aircraft_taken_to_the_ai`**:
  "the client drew nothing", after an imagery fetch's "Connection reset by
  peer". The windowed client's word handling changed here
  (`OwnWord.wrecked`); not shown to be the cause, nor ruled out.

**What CI run 38517430777 found, and why** (2026-10-14, at 820a11a2). The
multi-process test failed on four platforms, and its main cause was not
the prediction: **the server's own F-35B went down.** "The server's word
... lowest" 1 ft and a wreck (Ubuntu linux-debug), 25 ft (Windows
release), 619 ft (macOS release), against about 770 here; Windows debug's
client aborted on JSBSim's table assertion (FGTable.cpp:618) after "aircraft
0 is a wreck"; macOS's 95th percentile was 0.901 m.

- **The dive before the selector.** Until the selector arrives the server
  flies her untouched: untrimmed, 0.6 throttle, the stick centred, she
  dives. The test's pilot selects STOVL mode with its 150th input, so the
  server has flown her so for the join's time and five seconds more - and
  the join is longer on a slower machine. Flown in one process, selected
  after 5 s she was at 1,925 ft sinking 5,600 ft/min; after 8.3 s, 1,526 ft
  and 8,800; after 10 s, 1,260 ft and 10,400.
- **The law's autopilot seeded the pull-out away.** Taken over from that
  dive, the transition law's wing-borne autopilot seeded its vertical-speed
  integral from her pitch less the proportional part of the climb she was
  short of - 45 degrees nose-down from an 8,000 ft/min dive. As the dive
  eased, the pitch it wanted eased down with it; the command walked down at
  3 degrees a second, the integral not winding while it walked, and she
  pulled out in swings of 1.6 g and back to 0.8 every five seconds: from
  1,643 ft and 8,000 ft/min she bottomed at 134 ft, from 1,526 at 1 ft,
  from 1,260 into the ground - CI's 25 ft and its wreck. **Now** the seed is
  held within the pitch envelope (-10 to 15 degrees,
  `Autopilot::fly`): the command still starts from her pitch and walks at
  the pitch rate, so nothing steps, but it walks up and stays up. From 8.3
  s she bottoms at 734 ft, from 10 s at 291. It is every aircraft's
  autopilot: only an aircraft taken over in a climb or dive steeper than
  the envelope's own is seeded differently.
- **A wreck's replay can go to a NaN.** The server holds a wreck where it
  struck, for `wreck_s` (5 s), and flies her again from her start
  (`fly_again`). Through the wreck it still takes her player's inputs
  (`Fleet::fly` does not ask whether she is a wreck), so the word's
  `last_applied` goes on rising and at the respawn it is the newest input
  the server had; the motion is the wreck's, frozen, and then her start's.
  The client's prediction was put back to the wreck at every word and flown
  on through the ground for the inputs since. Flown again for every input
  since the wreck at every word, her transition law flying her, her whole
  state went to a NaN in one process - what a debug JSBSim's tables assert
  on; with `last_applied` rising, as the server's does, she was flown 12
  steps through the ground at each word and only drifted 7 m from the
  server's wreck. **Now** `Prediction::hear_wrecked`: told by the word
  that she is a wreck, the prediction keeps each input (the clocks'
  difference still told) but flies no step, forward or again, until a word
  says she flies again - in `glideslope_cli` and the windowed client
  (`Flight::reconcile`) alike.
- **The macOS 95th percentile is not shown fixed.** Diving, her speed
  was past 330 kt and each step of the clocks' difference 1.4 m. Held from
  the start, six runs here gave a 95th percentile of 1 to 8 mm, but a
  seventh, rebased onto main after #192, 44 mm (median 31) - the spread
  before was 42 to 51 mm. CI's 0.901 m on macOS is not reproduced here and
  not explained; the bound (10 cm) is unchanged.

**Verification of the fixes** (the three in-process tests in
linux-release, x86-64-v2 and linux-debug, the same figures in each):

- `a_heavy_f35b_selected_into_stovl_mode_at_any_moment_of_ten_seconds_untouched_is_pulled_out_above_her_least_conversion_height`:
  45,400 lb, untrimmed at 2,200 ft and 300 KCAS, fuel burning, flown
  untouched (0.6, the stick centred) to every 20th step of her first ten
  seconds - 61 moments, all flown, counted and asserted - then STOVL mode
  with the stick forward, her controller made on that step as the server
  makes it, a minute on: never lower than 90 ft (her least conversion
  height less 10), never touching, never slowed into jet-borne flight, and
  a minute on within 100 ft of the height asked or climbing to it. The
  lowest of all 178 ft (selected at step 1,180); from 10 s, 291. **Seen
  red** with the seed as it was: selected at step 920 she went down to 72
  ft. 5.1 s in linux-release, 47.5 s in linux-debug.
- `a_prediction_told_its_aircraft_is_a_wreck_holds_her_where_the_server_does_and_never_goes_to_a_nan`:
  an F-35B dived into flat ground on a server-side aircraft stepped and
  judged as the server does (a wreck at step 496), the prediction flown on
  the same inputs, each word three inputs late; the server holds her five
  seconds, its `last_applied` rising, STOVL mode selected meanwhile, then
  flies her again from 2,200 ft and 300 KCAS, untouched. Over 1,696 steps
  none with any property not finite; while a wreck, 0.000000 m from where
  the server holds her; at most 12 steps flown again at a word (bound 16,
  the delay and the input in flight); flying again, over 147 words, at most
  0.025 m from the server at the same step (bound 5 cm: her engine and
  actuators, carried through the wreck here where the server started her
  afresh) and 0.0014 m in the fifth second (bound 5 mm). **Seen red**
  without the wreck heard: 7.0 m from the server's wreck; and, in a first
  draft holding `last_applied` at the wreck's input, 1,005 steps not
  finite.
- `an_autopilot_handed_a_fighter_climbing_past_its_pitch_envelope_brings_her_back_to_her_height`
  (from the review of #194: the seed is clamped at the top too): an F-15C
  pulled into a 20-degree climb (4,656 ft/min) and an F-35B into a 25 (5,099)
  at 300 KCAS, handed over with the controls she is pulled by and asked to
  hold that height: the nose asked down at 3.00 degrees a second at most
  (bound 3), 1,039 and 998 ft above at most (bound 1,200), 197 and 159 ft
  off 90 s on (bound 250). **Seen red** seeded outside the envelope: the
  F-15C 2,041 ft above, and 1,349 above 90 s on - the old seed held her
  nose at the top while the climb went. The clamp is to 15 degrees in every
  mode: the stall-entry mode (`hold_height_to_the_stall`), whose top may be
  higher, is engaged level by the lessons, never in such a climb.
- The 297 in-process autopilot, hand-over, upset, stall, take-off, F-35B,
  STOVL, hover and transition tests (`-R
  "hand|take|upset|spiral|stall|autopilot|recover|f35b|stovl|hover|transition|engag|seed"`,
  less the multi-process) and the 24 in-process prediction tests green on
  linux-release, at 8dbbd86a. **The selftest hash does not move**:
  `de9bf01f96427593`.
- **The multi-process test through a server**, at 8dbbd86a, six runs:
  linux-release lowest 1,013 and 1,010 ft, the worst error 1.247 and
  0.007 m, the 95th percentile 8 and 7 mm, the median 2 and 3 mm;
  x86-64-v2 1,013 and 1,013 ft, 1.388 and 0.003 m, 7 and 3 mm, 2 and 1
  mm; linux-debug 778, 1,002 and 979 ft, 1.170, 0.874 and 1.403 m, 7, 1
  and 1 mm, 2, 0 and 1 mm. 200 KCAS at the end in each, 3,004 to 3,005
  corrections once joined, none too large to hide. Before, the lowest was
  770 to 774 ft in release and 131 and 259 in linux-debug, and the 95th
  percentile 42 to 51 mm. Rebased onto main after #192 (a3e6820b),
  linux-release once more: lowest 1,010 ft, the worst 1.416 m, the 95th
  percentile 44 mm, the median 31 mm. The worst stays a step of the clocks'
  difference at her speed (held to 2 m); the bounds are unchanged.

**What the server run found, and why.** Through a real server, the
player's F-35B selected into STOVL mode at her 300 KCAS start, 2,200 ft,
with the stick forward, was a wreck before any hover: "hit the ground
sinking at 4368 ft/min". Reproduced in one process
(`glideslope_tests`, flown as the server builds her controller): two
faults.

- **Her weight.** At 45,400 lb, slowed to a hover, the throttle reached
  its STOVL top (0.99) at 110 s and 91 KCAS, and she sank faster as the
  wing's lift went - 625 ft/min at 120 s, 1,831 at 140, 4,369 when she hit
  - the server's number to the foot a minute. Every in-process STOVL test
  had flown her at her vertical-landing loading, 37,530 lb. **Now**
  `Controller::too_heavy_to_hover` (her model's `fcs/stovl-hover-most-lbs`,
  tools/make_f35b.py): heavier, a pilot's STOVL mode asks the transition
  law for wing-borne flight at her conversion speed, the stick still asking
  her height (to her least conversion height, no lower), and is not let go
  while the selector is at it; a plan's vertical landing is not begun, its
  last waypoint flown on, as for an aeroplane without a lift system. As
  fuel burns below the limit, the next step's ask is a hover. **Held so,
  the stick's sideways and the pedals do nothing**: the law flies the
  heading she had and her wings level, so a heavy F-35B in STOVL mode
  cannot be turned until the selector goes back (or she burns under the
  limit). **On the ground, heavier, STOVL mode does nothing** (from the
  review of #194): the law made on her wheels stood her converted for a
  vertical take-off she could not hold; now no law is made there, her lift
  system is left as it is and the controls are the pilot's, so she can be
  rolled off on the wing, and once in the air the selector holds her
  wing-borne.
- **The stick seeded the law.** A controller made on the step the
  selector arrived - the server's (`controller_for`, given the pilot's
  input) - held the pilot's controls as what it had applied, and the
  transition law's wing-borne autopilot was engaged from them: the stick
  full forward was a full nose-down elevator, and from 300 KCAS she dived
  334 ft in five seconds, sinking 7,721 ft/min, before the autopilot had
  her back. **Now** the law is seeded from what the aircraft flies (her
  elevator, ailerons, rudder and throttle commands), never from the stick:
  in STOVL mode the stick asks a height at 10 ft/s up and 6 down, and never
  moves a surface. That rate is what the stick asked before too; only the
  seed was wrong.

**Verification** (linux-release):

- `stovl_selected_with_the_stick_forward_from_the_first_step_descends_at_a_bounded_rate_converts_hovers_and_lands_the_f35b`:
  her controller made as the server makes it, the stick forward from the
  first step, at her vertical-landing loading. From 300 KCAS at 2,200 ft:
  hovering at 159.3 s, down at 398.7 s at 3.00 ft/s, before the hover
  sinking at most 476 ft/min, lowest 1,335 ft, pitch 10.6. From 250 KCAS at
  1,000 ft: hovering at 133.8 s, down at 194.4 s at 3.00 ft/s, at most 486
  ft/min, lowest 276 ft, pitch 10.7 (bounds 600 ft/min, 90 ft, 15
  degrees). Two starts counted. **Seen red** with the law seeded from the
  stick: "she sank at 7721 ft/min".
- `heavier_than_she_can_hover_the_f35bs_stovl_mode_holds_her_wing_borne_at_her_conversion_speed`:
  at 45,400 lb from 300 KCAS at 2,200 ft, the stick forward ten minutes:
  never slowed into jet-borne flight, at 200 KCAS within 2.0 kt, lowest 100
  ft, sinking at most 378 ft/min, never touching, STOVL mode kept. **Seen
  red** with the limit off: slowed, sinking 4,555 ft/min, down to 3 ft.
- `on_the_ground_heavier_than_she_can_hover_the_f35bs_stovl_selector_does_nothing`:
  standing at 45,400 lb, brakes on, idle, STOVL selected and the stick
  full back a minute: never converted (0.00), no law made, never lifted, no
  wreck. **Seen red** without the guard: converted 1.00, a law made (she
  did not lift - she cannot - nor was she judged a wreck).
- `burned_below_her_hover_limit_in_stovl_mode_the_f35b_converts_and_hovers`:
  at 39,990 lb (combat and 240 lb), fuel burning, STOVL mode from 200
  KCAS at 1,000 ft: held wing-borne until under the limit at 82.1 s, then
  converted and hovering at 191.8 s; lowest 986 ft, sinking at most 516
  ft/min, pitch 13.8 (bounds 90 ft, 600 ft/min, 15 degrees).
- `a_players_stovl_selector_through_a_server_holds_a_heavy_f35b_wing_borne_and_the_client_agrees`
  (tests/cmake/server_stovl_landing.cmake, multi-process): a server, one AI
  Cessna, a 100 ms relay, and `glideslope_cli connect --aircraft f35b
  --predict --stovl-land` - a new pilot for the command-line client: the
  power a server holds for a player who has not touched anything (0.6) for
  five seconds, then STOVL mode and the stick forward, the lever still, its
  prediction built over the DEM as the server's ground is - until 3,000
  updates are compared: no wreck, never converted, 200 KCAS at the end,
  once joined no correction too large to hide, the worst error under 2 m,
  the 95th percentile under 10 cm and the median under 5 cm. Measured
  below (what CI found). **Seen red** before the two fixes: the server's
  F-35B a wreck, "hit the ground sinking at 4364 ft/min" and "4368" in two
  runs.
- Green with the rest: the targeted F-35B, STOVL, hover, transition,
  vertical, prediction, controller, plan, help and committed-asset tests.
- **The selftest hash does not move**: on main after #198, `de9bf01f96427593` (linux-release, rebased).

**What CI found in the multi-process test, and why** (run 38479432893, the
test then selecting STOVL mode with its first input): green on this
machine (0.908 m at worst) and red everywhere else - Windows "the worst
prediction error was 3535 mm", Rocky 9 and macOS "a correction was too
large to hide" (20.06 m), Ubuntu's debug build the client aborted on a
JSBSim table's assertion (FGTable.cpp:618, a NaN looked up). Traced step
by step - the server's controller and the client's prediction each
logging her state and every integral of the transition law and its
autopilot, joined on the server's step - in a build for x86-64-v2 (no
fused multiply-adds, as CI), the default build and linux-debug, it was
four things:

- **A replay burned the fuel again.** The client's prediction is put back
  to the server's motion at each word and flies the steps since again; the
  fuel in her tanks was not put back with it (sim::ReplayState kept the
  engines and the controls, not the tanks), so each step's fuel was burned
  once for every word that flew it again: the prediction burned 486 lb to
  the server's 234 in two minutes, and in a debug build, whose slower
  frames fly more steps again, faster still. **Now** ReplayState keeps
  each tank's contents. `a_prediction_put_right_at_every_word_burns_the_fuel_the_server_does`:
  the F-35B trimmed level at 20,000 ft, a minute, words every fifth step
  100 ms each way: both burn 89.9 lb (bound: a pound). **Seen red** without
  it: 305.6 lb to 89.9.
- **The controller put back was from a pass of its own past.** Each step
  held kept the lift system's controller as the pass that first flew it
  had it, never as a replay since had flown it; so each word put her
  transition law back to one of four lines of its own past, a frame's
  replay each - its autopilot's engaging offset cycling through four
  values the server's never had, and at one word its pitch integral put
  back to a value its line had left 25 steps before. **Now** each step
  held is kept as the newest replay flew it (Prediction::fly_held_again),
  and the trace shows no line but one. What it does to the error through
  a server is not told apart from a run's own spread (x86-64-v2: 1.270 m
  at worst before it, 0.048 m in the run after, 0.945 m in a later one).
  `a_prediction_put_right_twice_flies_the_second_word_from_the_controller_its_first_replay_left`:
  in STOVL mode at 60 s, a word from an F-35B flown from 2 ft higher puts
  her right by 0.512 m; a second word, half a second on, saying where her
  own replay had her, moves her 0.000005 m (bound 0.1 mm), in every build.
  **Seen red** without it: 0.008 m.
- **Selected with the first input, the two laws began apart.** The server
  made its controller when the selector arrived and seeded the transition
  law from what the aircraft had: the power it holds for a player before
  any input (0.6). The prediction, flying her inputs from its first step,
  seeded its law from a model just loaded (0). The law's throttle
  integral is never put right by the server - it sends her motion, not her
  laws - and was still 0.17 apart two minutes on. **Now** the test's pilot
  flies the server's held power five seconds and then selects STOVL mode,
  as a pilot would once flying: both laws are made from the same controls
  on the same step (the client's step 630 and the server's 766, which the
  clocks' difference joins). A player who joins already holding the
  selector is still seeded apart - not fixed here, its own item in
  COMPLETION_PLAN: until the server has applied an input it flies her on
  the power it holds (0.6, the stick centred), and the prediction flies her
  inputs from its first step on a model just loaded (its commands 0), so a
  law made on the first input is seeded from different controls on each
  side, and the server never sends the law's state to put the difference
  right. Closing it wants both seeded from what both know - the pilot's
  input, or the server's held controls told to the client.
- **The DEM was read as the prediction began.** Its coverage, tiles' store
  and geoid held the client's first predicting frame 0.8 s in a debug
  build; the steps then flown at once were taken as begun on the input
  sent after the hold, and the clocks' difference was put 55 steps short
  (82 against 137) until a word that had waited less came 2.6 s later:
  the prediction was put right by 85 m, and compared meanwhile 30 m off.
  **Now** `--stovl-land` builds the ground before it joins (0.04 s left
  at the start). And at her 300 kt a prediction joining from an update a
  trip old is put right by up to 26 m before the clocks' difference is
  known on a debug build - no prediction could hide that - so the client
  now says its corrections once joined as well (`predicted, joined:`), and
  the test holds those, not the joining's, to the 20 m - **over at least
  nine in ten of the updates compared** (from the review of #194: an empty
  joined window passed on nothing; 3,005 against 3,000 compared, measured).
  **Seen red** with the joined count forced to nothing: "only 0
  corrections once joined, under 2700". The snapshot's reconcile puts back
  no held replay state before flying again - the snapshot carries every
  writable property and the engines, the server's own - and the first step
  held is kept as the server had it (said in the code).

**The abort is not reproduced** by the test since: in three linux-debug
runs here the client never aborted. The same assertion was met in one
process, in a first draft of the fuel test: an F-35B left at full power
with the stick alone dived into the ground at 850 kt, and the replay that
flew her into it looked up her ground effect at a NaN height. Not shown to
be what CI's client met. (Since CI run 38517430777: a wreck's replay
going to a NaN reproduced in one process, and held - above.)

**Not fixed: the laws in the prediction drift from the server's.** The
server sends her motion, not her transition law's state, so a law that
has wandered in the client stays wandered: its autopilot's pitch loop is
rate-limited at the edge of its integrating, so a thousandth of a degree
decides whether a step's integral winds, and two minutes on the client's
elevator trim was at its stop where the server's was 0.09. Her predicted
pitch was then up to 4 degrees off the server's between words, her
position still within centimetres. A tail in COMPLETION_PLAN.

**Measured** (`a_players_stovl_selector_through_a_server_holds_a_heavy_f35b_wing_borne_and_the_client_agrees`,
2026-10-14, one run each): x86-64-v2 the worst error 0.945 m, the 95th
percentile 0.048 m, the median 0.036 m, the worst correction once joined
0.946 m; linux-release 0.048, 0.047 and 0.036 m, 0.009 m; linux-debug
1.465, 0.047 and 0.036 m, 1.462 m (26 m joining). None too large to hide
once joined; lowest 770, 774 and 131 ft above the ground, 200 KCAS at the
end. **Its 99th percentile is a step**: 0.867 m (x86-64-v2) and 1.144 m
(linux-debug) - the clocks' difference, held to a step and no closer,
moving by one is 1.4 m at the 330 kt she dives to - so the worst is held
to 2 m, and the 95th percentile, new, to 10 cm. The in-process tests
(`a_predicting_client_and_the_server_fed_the_same_stovl_inputs_...`, the
hover one, the two put-backs) unchanged: worst correction 0.0166 m in
every build; the 30 prediction tests (in-process and multi-process) green
on linux-release. **Rebased onto main after #198** (the alpha-rate terms
moved her model): the STOVL multi-process test again, the worst 1.145 /
1.270 / 1.460 m, the 95th percentile 0.047 / 0.047 / 0.051 m, the median
0.036 m in each, 3,004 to 3,005 corrections once joined, none too large
(linux-release / x86-64-v2 / linux-debug); the 75 F-35B, STOVL, hover,
transition and prediction tests green on linux-release, and in linux-debug
but for the frames-held test, cut off unfinished (green on release); on
x86-64-v2 three multi-process ones that need the drawing client and the
interpolation check, neither built there, failed for want of them;
`glideslope_cli figures f35b` 15 of 15 in range; **the selftest hash does not move**, `de9bf01f96427593` on main after #198 (rebased, linux-release).
