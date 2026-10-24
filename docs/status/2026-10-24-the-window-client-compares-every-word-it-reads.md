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
  --ticks-behind 12`) and
  `a_predicting_client_too_slow_to_replay_every_word_puts_itself_right_from_the_newest_and_keeps_up`
  (glideslope_cli) both check the words through `own_words_check.cmake`.
  Words read must equal words compared plus words named, and must equal the
  client's own count of words heard. None may be at a step no longer kept.
  At most 28 may come before they could be placed, and none after a word was
  placed. At most 2 may be dropped (neither case has a switch). Every replay
  must be from the newest word. The window case must also meet the
  ticks-behind counts above. glideslope_cli's case must have a pass that read
  several words.
- Window client, Linux debug, at `--ticks-behind 5` before the review: 251
  read, 250 compared, 51 of them held; 250 and 249, 43 held; 251 and 250, 41
  held. At 12, after it, on a loaded machine: 251 read (251 heard), 249
  compared, 77 held, 2 not yet placeable; and 251, 249, 79 held, 2.
- glideslope_cli, starved: 429 read, 429 compared.
- Seen red with the hold taken out (a word past the steps flown compared at
  once): 208 of 251 compared, and 42 at a step no longer kept. The hold was
  restored and the test went green. The first round of this item saw it red
  on the old rule, comparing only each frame's newest: 61 of 226 compared.
- The related tests (`ctest -R` on the window client, prediction, take-over,
  STOVL, NaN and help tests, `-j4`, no display) pass. The ports check passes.
  Built on Windows (`tools/windows_build.sh`, windows-debug).

**From the review: the checks tightened** (own_words_check.cmake).
- **Counted twice, apart.** The words read must equal the client's own count
  of the words it heard on its own aircraft, kept where it hears them, not by
  `OwnWords`. The window client's corrections must equal the words put right
  from.
- **Only from the newest.** `OwnWords` counts which word each replay is from:
  the newest read in its pass, or, with newer words held for their steps, the
  newest whose step was flown. It also counts any other replay, which fails
  the test. Window client: 98 put right from, 72 from the newest read and 26
  from the newest flown with newer held.
- **Why a word cannot be placed.** A word is placed by the input it says the
  server last applied, as this client began that input. Until the server has
  applied one this prediction flew, no word has a place. So unplaceable words
  come only in the first frames after a prediction begins, never after a word
  has been placed, and the test asserts that. The worst count seen was 14, in
  a frame of 17 words on a loaded machine (load 20 to 40). The test allows
  28, twice that.
- **The situation built, by count.** `--ticks-behind` is now 12, more than
  two words' steps. At 5, a loaded machine's long frames held only 1 word in
  21 passes. The client says how many passes it flew behind. The test
  requires at least ten such passes, and at least as many words held as
  passes. Two runs: 25 passes, with 77 and 79 words held.
- **A held word is released wherever it is in the queue**, not only from the
  front. Before, a word placed earlier than one read before it waited behind
  that word, and a newer word's comparison forgot its step: 2 in 252 on the
  loaded machine.

**Related tests after the review**: 82, `-j4` with no display, twice. The
second run was green (one skipped: the live ChatGPT case). The first, on a
machine at load 20 to 40, failed the every-word test once, and its output
was not kept. Run alone and beside the other heavy tests since, it passed
four times. This is named here, not explained.

**The selftest hash** does not move (831a4e7954233b0a): the selftest flies
`sim::selftest`, not the prediction.
