### The STOVL selector test after real server slips: a word older than one put right from is no longer put right from, and the words after a slip are held to measured bounds, 2026-11-25

Phase 12a, "The STOVL selector test's worst error after a real server slip".

**What is still not true, first.**
- The slipped run uses slips built by the server (`--test-slip`), not a
  loaded machine's own. Ten loaded runs had the machine's own slips as well,
  and they passed.
- A slip still costs what the held clocks' difference costs: up to 23.4 m
  of error and 26.9 m of correction after a slip of 20 steps, 1 to 3 of those
  corrections too large to hide. That is the owner's decision (2026-11-22),
  and it is bounded here, not removed.
- Windows' two-step spread, its own item, is not touched.

## A defect found: putting right from a word older than one already put right from

Under load, all ten runs of a first slipped variant (the server stopped for
3.7 s) broke #407's rule that every putting right is from the newest word.
They did it 1 to 10 times a run. Ten loaded runs without a slip had never
done it.

The cause is in `frontend::OwnWords` (src/frontend/own_words.hpp):
1. After a slip the clocks' difference moves down.
2. A newer word is then placed at an earlier step than an older word, which
   is still held because its step has not been flown yet.
3. The newer word is flown, compared and put right from first.
4. When the older word's step came, `offer` gave it to `settle` too. The
   prediction was put back to a word that had already been superseded, and
   which was placed by the difference from before the slip.

The fix: a word older than one already ready to be put right from is now
compared and counted as superseded, never put right from
(`superseded_late`). glideslope_cli counts such a word's joined comparison
as superseded too. The window client shares `OwnWords` and gets the same
fix.

- Unit test:
  `a_word_older_than_one_put_right_from_is_compared_and_superseded_never_put_right_from`
  covers two passes and one.
  - Seen red: with the check removed it failed with "the older, flown later,
    is not put right from". Reverted.
- After the fix, the twenty slipped runs below put right 0 times from
  anything but the newest word.

## Slips of the item's size, built

The item's words were 5 and 21 steps off. `--test-held-up` cannot build a
slip: what a hold leaves owed is caught up whenever it is under a quarter
of a second (`net::most_steps_held`), or while the server has not been
flying flat out. That covers every hold of 50 to 200 ms. In the engine-stop
variant it placed words 0 steps off.

So the server gets a test flag, `--test-slip EVERY_S STEPS`. Every EVERY_S
seconds on its clock it runs nothing for a quarter, a half, three quarters
and then all of STEPS steps' time, in turn. It lets those steps slip
(`net::CatchingUp::let_slip`) instead of catching them up, as a loaded
server too slow to catch up does. Each hold is said on stderr.

The slipped run uses `--test-slip 5 20`, which gives slips of 5, 10, 15 and
20 steps, 24 or 25 of them over the flight.

## The detector

A slip is seen in either of two ways:
- a word placed two steps or more from where it falls on the path flown;
- the clocks' difference moving two steps or more at once, as a word is
  read (`AfterASlip::figure`).

Seen only the first way, the correction made as the figure moved (about
22 m for 20 steps) came before any word was seen off, in 3 of 9 idle runs.
Words placed by the held figure before it moves are where the server was,
because its aircraft never flew the steps it let slip. The cost comes at
the move.

The words after a slip are the word that saw it and the 60 after it. The
corrections once joined made in that time are counted with them.

## Bounds, from 20 runs (10 idle, 10 beside 40 busy loops on 20 cores), Linux debug

| | idle | loaded | bound |
| --- | --- | --- | --- |
| worst word after a slip | 3.8 to 23.4 m | 7.1 to 22.7 m | 30 m |
| worst correction after a slip | 22.1 to 23.3 m | 19.3 to 26.9 m | 35 m |
| too large to hide, after a slip | 1 a run | 0 to 3 | (counted apart) |
| too large to hide, the rest | 0 | 0 | 0 |
| the rest's worst | 1.1 to 1.4 m | 0.007 to 2.4 m | 5.6 m, as before |
| puttings right not from the newest | 0 | 0 | 0, as before |
| holds / steps let slip | 24 / 300 | 24-25 / 300-305 | at least 10, all let slip |

- 20 steps at 330 kt is 28 m, so 30 m allows for the largest slip built.
- All twenty runs passed every check of the test with these bounds.
- The plain test passes idle: no slip seen, the rest's worst 1 mm.
- The words-after-a-slip window bound was dropped. Slips 5 s apart chain
  their windows (up to 817 words), so a bound on the window's length held
  nothing.
- A first set of figures came from a 3.7 s stop (24 to 26 steps, past the
  3.5 s caught up). It gave 6.1 to 45.2 m and corrections to 54.7 m, and is
  not this item's event.

## Seen red

Each was an idle run of the slipped test through ctest, then reverted:

| bug | how it failed |
| --- | --- |
| no slip ever seen (`from_steps` 1000) | "the worst prediction error apart from a slip was 11400 mm, over four steps' travel at 330 kt (5.6 m)" |
| a slip twice the size asked (`--test-slip` holds and slips 2x) | "the worst prediction error after a slip was 34718 mm, over 30000 mm" |
| the same, with the 30 m check taken out | "a correction once joined after a slip was 45621 mm, over 35000 mm" |
| corrections never counted after a slip (`corrected` always false) | "1 corrections once joined, not after a slip, were too large to hide" |
| the window one word long (`left_ = 1`) | "1 corrections once joined, not after a slip, were too large to hide" (the rest's 5.6 m held) |
| `--test-slip` left out of the slipped run | "the server was held up to slip 0 times, under the 10 asked" |
| `let_slip` letting nothing slip | "the server let 0 steps slip, under the 300 it was held up for" |

The unit tests:
- `a_word_placed_two_steps_or_more_off_sees_a_slip_...`: with `from_steps`
  set to 3 it failed with "placed 2 steps off, it sees a slip".
- The OwnWords test: see above.

## What lands, and costs

- The new test is
  `a_players_stovl_selector_through_a_server_that_slips_is_held_to_measured_bounds_after_each_slip`,
  port 24934, about 129 s.
- It is costed at its plain twin's figure in every `tests/ci_costs` table.
- The nightly's repeat job needed a ninth shard:
  `every_ci_test_shard_is_dealt_...` failed at eight, and passes with nine.
- `--test-slip` is in server_flags.cmake's list.

The selftest hash is not touched: no simulation code changed. The fix is
in `src/frontend/`, and the test flag is in the server's loop.
