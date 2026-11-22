### The first-input and F-22A tests against a server held up, 2026-11-22

Phase 12a, "A server that falls behind real time puts its clients'
prediction off by metres": its last named gap, "the first-input test and the
F-22A's still run alone, not yet seen on CI held up". Held up, the F-35B's
and F-22A's 95th percentile was at first now and then a step's travel. The
cause is below. **Fixed by placement**: a word is now compared at the step
flown under the clocks' difference its own step maps to. Every 95th
percentile is then under 2 mm.

**What is still not true first.**
- The plain first-input and F-22A tests still run alone (RUN_SERIAL). What
  CI's shared runners did to them by chance is now built on purpose, below.
- The worst update is still a step's travel or two (up to 2.76 m), in one
  to three words a run. The 95th percentile is under 2 mm. Each such word is
  about a server step that no step here was flown under: when the
  difference rises a step, one server step maps to none of this client's.
  It is then placed by the difference as now known, as before.
- The re-anchoring itself is unchanged: when the difference moves, the
  next word put right from moves the prediction a step's travel. That is
  the estimate rule's cost (sim::ClockDifference, the owner's), not
  placement.
- Windows' two-step placements are their own item, not touched or measured.

**Why a step off, measured** (from the review: the item stays open on it).
glideslope_cli was run with `--track` in the F-22A's case, by hand as the
test runs it, Linux debug, unloaded. The server also said each step it let
slip (`--test-say-slipped`).
- **Not the server's stamping or owed steps**: no step slipped in any run,
  and every update a step off lay on the path flown. The off-path 95th
  percentile was 0.2 and 0.3 mm held up (0.0 plain), and the worst under
  0.5 mm. In the test runs above, the worst off the path was 9 mm.
- **The clocks' difference following a widened spread**: each word's sample
  (the server's step where it flew the input, less this client's step
  where it sent it) took two values, 341 and 342, in the plain run, with
  no change of the held figure. Held up, it took six or seven: 343 to 349,
  and 345 to 353. The least of each second wandered a step or two
  (347, 346, 345, 346, 347, 348 over 30 s). The held figure (sim::
  ClockDifference) followed it: down once, after ten of fifty samples were
  below, and up at once each time the window's least rose (346 to 347 to
  348). Each hold-up is not a slip the figure follows; nothing slipped.
  The least the window sees moves.
- **Each move puts a round trip of words a step off**: every update over
  half a metre came in the five or six words right after a move, each
  exactly one step's travel (1.378 m), at 12.2, 23.8 and 27.8 s. That run
  had three moves and 16 words (2.3%); another had one move and 8 words
  (1.2%). A run with six or more moves puts over 5% a step off, and its
  95th percentile is a step's travel.
- **The fix, placement only** (sim::Prediction::compared_at,
  `flown_under`): each step is flown on from the word last put right from,
  placed by the difference as it was then. That is the difference the step
  was flown under, kept by step for the steps a client keeps. A word is
  compared at the step whose difference maps it to its own server step,
  trying the figure as now known first, then up to three either side. The
  estimate, and where the prediction is put right from, are as before.
  Both clients compare so (glideslope_cli's `compare_word`, the window
  client's `Flight::compare`). glideslope_cli now also says the off-path
  95th percentile.

**What now works.** Two tests run `server_first_input.cmake` against a server
held up every second of its clock for 150, 300, 450 and 600 ms in turn
(`-DSERVER_HELD=unevenly`, the server's `--test-held-up 1 600`, as the held-up
paced test), while the relay and the client run on:
- `a_players_f35b_and_c172p_flown_from_their_held_trim_against_a_server_held_up_every_second_hold_height_and_attitude_and_predict_alike`
  (port 24912): the four cases, each aircraft untouched and nudged.
- `a_players_f22a_untouched_against_a_server_held_up_every_second_is_predicted_on_the_held_trim_it_tells_her_client_and_holds_height_and_attitude`
  (port 24922).

The bounds are the plain tests', not loosened: height within 100 ft,
attitude, the median under 60 mm, the 95th percentile within four steps'
travel, every update within 60 mm of the path flown, and the F-22A's held
trim told and put. **The hold-ups are counted, not believed**: the server
now says each one on its error stream (`held up MS ms at S s, N times in
all`), and each case fails if it was held up fewer than 30 times in her
thirty seconds. Every case so far: 31.

Both are RUN_SERIAL, so the hold-ups are the ones built. COST 13 and 4: a
release runner's seconds as Linux debug's 143 and 35 s over twelve, as the
window client's paced tests are costed. Each case is flown on the session's
clock, so a release runner takes about as long; CI's tables will measure
them.

**Verification, with the placement fix** (Linux debug, this machine).
- The two held-up tests, three runs each beside 10 busy loops on 20 cores:
  all passed. Every case's 95th percentile was 0.000 m, except one C172P
  nudged at 0.001 m; median 0. Off-path 95th percentile 0.0000 to 0.0004
  m. Worst 0.000 to 2.757 m (the unmapped steps above).
- `a_word_is_compared_at_the_step_flown_under_the_clocks_difference_its_step_maps_to_while_the_difference_moves`
  (unit, five cases counted): the difference settled at 100 and put right
  from, then risen to 101 by fifty words. A step flown under 100, and one
  flown after the move but before a word placed by 101, are compared at
  their own steps; one flown after a word placed by 101 is compared there;
  a step not yet flown is placed by the difference as now known. **Seen
  red** with `compared_at` placing by the difference as now known (the old
  placement): "flown under 100: compared at 277, not its own 278". Reverted.
- **The STOVL selector test, once beside 40 busy loops** (as 2026-11-22's
  ten runs): prediction error 95th percentile 0.000 m, worst 0.002 m over
  3,000 updates, off-path 95th 0.0003 m (that file's worst run was 39.4 m,
  its best 0.003 m). It failed on the check that file names as not
  touched: 1,768 corrections once joined, under 2,700. That run's
  1,684 to 2,122 is the same tail, not a regression.
- Beside them, `-j4`, all passed: both window client paced tests, the plain F-22A test,
  glideslope_cli's paced test, and the put-right-from-within-its-input
  test.

**Before the fix** (the figures that kept the item open):
- Unloaded, one run:

| case | median / 95th / worst | nudge steps here / server |
|------|------|------|
| F-35B untouched | 0 / 1.379 / 4.136 m (711 updates) | |
| F-35B nudged | 0 / 1.374 / 2.755 m (711) | 61 / 61 |
| C172P untouched | 0 / 0.462 / 0.923 m (707) | |
| C172P nudged | 0 / 0.460 / 1.381 m (707) | 60 / 63 |
| F-22A untouched | 0 / 0 / 1.378 m (692) | |

  Off the path flown: 9 mm at worst, placed up to 3 steps along it, none at
  an end.
- Beside 10 busy loops on 20 cores, three runs each, all passed. 95th
  percentile / worst, run by run: F-35B untouched 0 / 1.38 m each time;
  nudged 0 / 1.38 m each time (62, 65 and 62 steps on the server for 62,
  63 and 60 here); C172P untouched 0.002 / 0.46, 0.461 / 0.92, 0.462 /
  1.85 m; nudged 0.002 / 0.46, 0.459 / 0.92, 0.459 / 1.84 m (64, 63, 67
  steps on the server); F-22A 0 / 2.76, 1.378 / 1.38, 0 / 2.76 m. Medians
  all 0. 138 and 37 s a run.
- **Seen red** with main's stamping before 2026-10-28 (the newest input
  due at `stepped + min(owed, 30)`, put in, built, run, taken out): both
  failed on their first case. F-35B untouched, 95th percentile 62.0 m,
  worst 95.1 m; F-22A, 68.9 m and 103.4 m. Rebuilt without it, both pass.
- **Not red** with each older input of a pass due with the newest
  (2026-11-17's spacing taken out): both passed, every 95th percentile
  0.46 m or less. Every input of the untouched cases is the same, and a
  nudge has only two changes, so this test does not pin that rule; the
  held-up paced test, whose pilot moves every step, does (2026-11-17). The
  F-35B's nudge was then flown 23 steps on the server against 61 here, and
  passed: the attitude bound follows the server's count, and nothing holds
  the two counts together. Named here, not a plan item: it is that rule's
  test's job.
- The flag and dealing checks and the plain first-input and F-22A tests
  passed (`-j4`); the port check, once the ports were moved into the
  tests' block (24912, 24922), passed.
- **Selftest hash** 27e94c795b47141c, unchanged after the placement fix too; the
  server prints one more line only with `--test-held-up`.
