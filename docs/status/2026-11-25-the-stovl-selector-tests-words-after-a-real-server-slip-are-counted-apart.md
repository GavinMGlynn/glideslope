### The STOVL selector test after real server slips: no putting right from a stale word, and the words after each slip the server says it made held to measured bounds, 2026-11-25

Phase 12a, "The STOVL selector test's worst error after a real server slip".

**What is still not true, first.**
- **The plain run under load is held as tightly as before**: every word
  within 5.6 m and no correction too large to hide. A loaded machine's own
  slips can still fail it, as they did in two runs of ten on 2026-11-22.
  Nothing excuses a word in the plain run.
- **The client's own guess at a slip is reported, not asserted.** That is a
  word placed two steps or more off, or its clocks' difference moving two
  steps or more at once. The review asked for no slip seen in the plain run.
  That is not asserted, because on Windows a word two steps off is the known
  spread of its timers (its own Phase 12a item), not a slip. Asserting it
  would fail there for a reason this item does not own. The 5.6 m on every
  word is the plain run's bound.
- A slip still costs what the held clocks' difference costs. Within 4 s of
  a slip of 20 steps a word was up to 21.6 m off and a correction 21.7 m.
  That is the owner's decision (2026-11-22). It is bounded here, not removed.

## A defect found: putting right from a word older than one already put right from

Under load, all ten runs of a first slipped variant broke #407's rule that
every putting right is from the newest word. That variant stopped the
server for 3.7 s, and the rule broke 1 to 10 times a run. The cause was in
`frontend::OwnWords` (src/frontend/own_words.hpp):
1. After a slip the clocks' difference moves down.
2. A newer word is placed at an earlier step than an older word still held
   for its step, so the newer one is flown, compared and put right from
   first.
3. When the older word's step came, it was offered too, and the prediction
   was put back to a superseded word placed by the difference from before
   the slip.

The fix: a word older than one already ready is compared and counted as
superseded, never put right from. glideslope_cli counts its joined
comparison as superseded too. The window client shares the fix.

- Unit test:
  `a_word_older_than_one_put_right_from_is_compared_and_superseded_never_put_right_from`,
  covering two passes and one.
  - Seen red: with the check removed it failed with "the older, flown later,
    is not put right from".
- In the forty slipped runs since the fix, nothing was ever put right from
  but the newest word.

## Slips of the item's size, built and said by the server

`--test-held-up` cannot make a slip. What a hold leaves owed is caught up:
always when it is under a quarter of a second (`net::most_steps_held`), and
otherwise while the server has not been flying flat out.

So the server has a test flag, `--test-slip EVERY_S STEPS`. Every EVERY_S
seconds it runs nothing for a quarter, a half, three quarters and then all
of STEPS steps' time, in turn, and lets those steps slip
(`net::CatchingUp::let_slip`) instead of catching them up. The slipped run
uses `--test-slip 8 20`: slips of 5 to 20 steps, 15 of them.

The server says:
- each hold: "held up to slip N steps";
- the steps each hold let go: "let N held-up steps slip at T s, M in all".
  The test counts these lines, not the server's total, which would include
  a slow machine's own slips;
- with `--test-say-slipped`, the moment of every slip it makes, held up or
  not: "let N steps slip at T s, M in all". This line now carries its moment.

## How the slipped run is judged

glideslope_cli's `--track` now gives each compared word its server moment
and whether it falls in a machine's stall. It also writes a `putright` line
for each putting right: the moment, how far it moved her, too large to hide
or not, joined or not, and across a stall or not.

The test reads the track against the moments the server said it slipped:
- **after a slip**: a word whose moment is within 4 s after one, and any
  putting right from such a word;
- **the rest**: every other word.

| | the bound | 10 idle | 10 beside 40 busy loops on 20 cores |
| --- | --- | --- | --- |
| the latest word over 5.6 m or correction too large, after a slip | (4 s window) | 0.57 s | 3.0 s |
| the rest's worst word | 5.6 m | 1.14 to 1.35 m | 0.88 to 1.75 m |
| the rest's corrections too large to hide | 0 | 0 | 0 |
| worst word after a slip | 28 m | 3.5 to 15.9 m | 4.1 to 21.6 m |
| worst correction after a slip | 28 m | 18.7 to 19.8 m | 17.8 to 21.7 m |
| share of words after a slip | 60% | 49% | 49 to 51% |
| holds / held-up steps let slip | at least 10 / all | 15 / 180 | 15 / 180 |
| clocks' difference's largest move at once | at least 15 steps | 17 to 18 | 17 to 18 |
| puttings right not from the newest | 0, as before | 0 | 0 |

- 20 steps at 330 kt is 28 m.
- The share is high by design, with a slip every 8 s and 4 s after each. A
  bound on it stops windows chained one into the next from swallowing the
  run.
- Two loaded runs were judged with a 1.5 s window before the 4 s was chosen.
  They failed on a word 5.9 m off 3.0 s after a slip, which is what set the
  window. Their server's lines were not kept, so their figures at 4 s come
  from the other eight loaded runs.
- The plain test passed idle throughout.

## Seen red

Each check was seen red in an idle run through ctest, then reverted:

| check | bug | how it failed |
| --- | --- | --- |
| plain: every word at 5.6 m | each word compared 8 steps from its own step | "the worst prediction error was 11061 mm, over four steps' travel at 330 kt (5.6 m)" |
| slipped: the rest at 5.6 m | the server says each slip 4 s late | "the worst prediction error apart from the server's slips was 16683 mm, over four steps' travel at 330 kt (5.6 m)" |
| slipped: none too large in the rest | slips twice the size, said 4 s late, the 5.6 m check out | "8 corrections once joined, apart from the server's slips, were too large to hide" |
| slipped: words after a slip at 28 m | slips twice the size asked | "the worst prediction error after a slip was 32138 mm, over 28000 mm" |
| slipped: corrections after a slip at 28 m | the same, the words' check out | "a correction once joined after a slip was 39521 mm, over 28000 mm" |
| slipped: the share at 60% | a slip every 2 s, the move check out | "95% of the words were within 4000 ms after a slip, over 60%" |
| slipped: the difference moved 15 steps | the client never records a move | "the client's clocks' difference moved at most 0 steps at once, under 15" |
| slipped: at least ten holds | `--test-slip` left out | "the server was held up to slip 0 times, under the 10 asked" |
| slipped: every held step let slip | `let_slip` lets none | "the server let 0 of its held-up steps slip, under the 180 it was held up for" |

Two bugs did not turn red, and what that showed:
- Placing every word 8 steps off through the clocks' difference itself
  passed the plain run. A constant offset is self-consistent, so the run
  stays green, rightly.
- Slips said 4 s late, with the rest's check out, put no correction too
  large to hide in the rest idle, because idle has few of them. That is why
  the snapped check's bug also doubles the slips.
- A slip every 2 s first failed the move check ("moved at most 8 steps").
  The share was then seen red with that check out.

The unit tests:
- `a_word_placed_two_steps_or_more_off_sees_a_slip_...`: with `from_steps`
  set to 3 it failed.
- The OwnWords test: see above.

## What lands, and costs

- The slipped test is
  `a_players_stovl_selector_through_a_server_that_slips_is_held_to_measured_bounds_after_each_slip`,
  port 24934, about 130 s. It is costed at its plain twin's figure.
- The nightly's repeat job has nine shards: the shards test failed at eight.
- `--test-slip` is in server_flags.cmake's list and usage.

The selftest hash is not touched: no simulation code changed. The fix is in
`src/frontend/`, and the test flag is in the server's loop.

## History

A first measurement used a single 3.7 s stop, 24 to 26 steps past the
3.5 s the server catches up. It gave 6.1 to 45.2 m and corrections to
54.7 m, and is not this item's event. The client's own guess at slips was
then the bucket, and the review of #411 found that it loosened the plain
run, so it was replaced by the server's word.
