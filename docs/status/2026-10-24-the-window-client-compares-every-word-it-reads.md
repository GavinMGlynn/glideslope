### Both clients compare every word they read, each at its own step, 2026-10-24

**What changed.** Both predicting clients now handle the server's words on
their own aircraft the same way, through `frontend::OwnWords`
(`src/frontend/own_words.hpp`). glideslope_cli's `Predicting::read_word` and
`put_right`, and the window client's `Online::heard` and `Online::hear`, all
use it.

- **Every word read is placed** as it is read (`sim::Prediction::hear_clock`)
  and steers the pace (`sim::Pacing::follow`). The window client now steers
  its pace by every word, as #245 made glideslope_cli do. Before, it steered
  only by each frame's newest. A frame's words share one local time, the
  frame's.
- **Every word is held against where the client had flown by its own step.**
  A word whose step is flown is compared at once. A word placed past the
  steps flown is held until they are flown, then compared. That is about a
  moment the ticks had not reached when it was read: a step or two, 18 to 24
  of about 250 words with frames held 0.3 s. Before, such a word was not
  compared. When it was also the newest, it put the prediction right with
  nothing to fly again: the aircraft was put at the server's word, ahead of
  the steps the client had counted.
- **Put right once a pass, from the newest word compared** (or read before
  any word could be placed). The client uses
  `sim::Prediction::reconcile_placed`, a new entry point that takes the
  placement already made, so a word's clocks' difference is never taken
  twice. A pass whose words are all past the steps flown puts the prediction
  right from none of them; the next pass does, once it has flown them. This
  is the standard reconciliation: the client goes back to the newest
  authoritative state it can place and flies its inputs since (Gambetta,
  *Client-Side Prediction and Server Reconciliation*; Valve, *Source
  Multiplayer Networking*).
- **Named, never silently left out.** A word is not compared when:
  - its clocks' difference could not yet place it;
  - it is not finite. A test's NaN words (`--test-nan-words-at`, #248) now
    apply to every word as heard, not only to the one put right from;
  - it was dropped with the prediction it was held for: at a switch, at a
    loss (#248's let-go), at a start again, or after being held a second and
    never flown;
  - it is still held when the client reports.

  A word placed at a step flown but no longer kept is in none of these, and
  fails the test. Both clients print the same line about their words.
- **glideslope_cli keeps the steps it flies while joining** (`start`, from
  `before_`), marked as not counted. The first words after joining are about
  those steps: 5 of 429 in the starved test were placed there and had nothing
  to be held against.

**The situation is built, not waited for.** `--ticks-behind N` (window
client, for tests) makes each frame held by `--slow-frames` fly N of its due
ticks on the next frame instead, so the newest words it reads are about steps
not yet flown. Per-word pacing alone made such words come and go: 3 of 4
runs had none.

**Verification.**
- `a_client_with_the_window_reading_several_updates_a_frame_compares_every_one_at_its_own_step`
  (client_on_server.cmake with `EVERY_WORD`: `--slow-frames 300
  --ticks-behind 5`) and
  `a_predicting_client_too_slow_to_replay_every_word_puts_itself_right_from_the_newest_and_keeps_up`
  (glideslope_cli) both check the words through `own_words_check.cmake`.
  Words read must equal words compared plus words named. None may be at a
  step no longer kept. At most 25 may come before they could be placed, and
  at most 2 may be dropped (neither case has a switch). A pass must have read
  several words, and in the window case some words must have been held.
- Window client, Linux debug, three runs: 251 read, 250 compared, 51 of them
  held (at most 6 steps past); 250 and 249, 43 held; 251 and 250, 41 held.
  Each time 1 word came before it could be placed and none were dropped.
- glideslope_cli, starved: 429 read, 429 compared.
- Seen red with the hold taken out (a word past the steps flown compared at
  once): 208 of 251 compared, and 42 at a step no longer kept. The hold was
  restored and the test went green. The first round of this item saw it red
  on the old rule, comparing only each frame's newest: 61 of 226 compared.
- The related tests (`ctest -R` on the window client, prediction, take-over,
  STOVL, NaN and help tests, `-j4`, no display) pass. The ports check passes.
  Built on Windows (`tools/windows_build.sh`, windows-debug).

**The selftest hash** does not move (831a4e7954233b0a): the selftest flies
`sim::selftest`, not the prediction.
