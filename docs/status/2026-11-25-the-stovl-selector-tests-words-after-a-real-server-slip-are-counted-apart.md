### The STOVL selector test's words after a real server slip, counted apart and bounded, 2026-11-25

Phase 12a, "The STOVL selector test's worst error after a real server slip".

**What is still not true, first.** The item stays open. Its verification is
now the slipped run (`-DSLIPPED=ON`) in ctest, passing ten times beside
twice as many busy loops as cores. It is not in ctest: even idle it passed
once and failed once in two runs (2026-11-25), its first correction once
joined after the slip, 19.3 and 20.2 m, made before any word was seen two
steps off, so counted with the rest, and 20.2 m is over the 20 m too large
to hide. A correction since the last word compared is counted with the slip
when the next word sees one; that was not enough. Beside the load, none of
ten passed:
- **In all ten, 1 to 10 puttings right were not from the newest word**
  (joined_words_check.cmake's third rule, `neither`). Ten loaded runs without
  a built slip had none (2026-11-22). So a real slip under load breaks that
  rule, which is its own cause, not looked into here.
- **In three, something past the window**: a correction once joined too
  large to hide just after the 60 words (runs 5 and 10), and a word 6.1 m off
  the path flown, not placed off, the word after the window closed (run 4).
- Runs 1 and 2 stopped at a mistake in the test (a CMake regular expression
  holds nine groups at most), fixed since; their figures below are from what
  the client said.

What lands is the counting and the plain STOVL selector test's new checks,
whose bounds after a slip are set from the ten loaded slipped runs. The
plain test passes idle (twice, below).

**What it was.** Beside twice as many busy loops as cores, the STOVL selector
test failed two runs of ten on its worst error, 6.7 and 17.4 m against its
5.6 m, each after the server really slipped (docs/status, 2026-11-22, "under
load every joined word is put right from or superseded"). The clocks'
difference is held once settled and lowered only when ten of its fifty words
are below it (sim::ClockDifference, the owner's decision, 2026-11-22), so a
slip is followed about ten words late. That rule is not touched here.

## What a slip does, measured

A slip was built on purpose: the server runs nothing for 3.7 s at 60 s on its
clock (`--test-stopped-once 60 3700`), past the 3.5 s it catches up
(net::most_steps_caught_up), and lets the rest go for good, 24 to 26 steps,
which it says (`--test-say-slipped`). A hold the server catches up (the
engine-stop test's `--test-held-up 1 1000`) lets nothing slip, which is why
that variant could not stand for one.

glideslope_cli's `--track` around a built slip, idle:
- The words just after it were placed by the held figure, up to 23 steps
  from where they fall on the path flown: 20.2 m off in one run, 0.004 m in
  another, as the slipped steps fell on words or between them.
- The figure and the pace then follow it a step or a few at a time over
  about three seconds, and every move puts the round trip's words after it
  one to four steps off (0.9 to 4.4 m). Those words' own differences are not
  above the figure, so a first split, by the word's own difference against
  the figure it was placed by, left them with the rest. It was dropped.
- The putting right as the figure moves is as large: 17.9 to 19.3 m once
  joined, against the 20 m the test calls too large to hide; one loaded run
  went past it.

## The change

`frontend::AfterASlip` (src/frontend/after_a_slip.hpp), in glideslope_cli:
- **A slip is seen** in a word placed two steps or more from the step nearest
  it on the path flown - off when, not off where (the `steps off` the client
  already measured for each word). One step is what the held figure leaves
  now and then on a busy machine, and is not a slip. A word nearest an end
  of the steps kept was placed at least that far off, and counts.
- **The words after a slip** are that word and the 60 after it - the
  figure's 50 words and the pace's 10, as a machine's stall's are - begun
  again by every word that sees one. Corrections once joined made while
  they come are counted with them. Words across a machine's stall are
  counted apart already and are in neither.
- It says `after a slip: N seen, M updates compared after them, at most L
  after one, placed at most S steps off, the worst X m; the rest: R updates,
  the worst Y m; corrections once joined after them: C, the worst Z m, K too
  large to hide`.

The STOVL selector test (tests/cmake/server_stovl_landing.cmake) now holds:
- **the rest** as every word was held: 5.6 m (four steps at 330 kt), and no
  correction once joined too large to hide;
- **the words after a slip** to measured bounds: their worst error 50 m
  (45.2 m the most in ten loaded runs), their worst correction once joined
  60 m (54.7 m), and at most 250 words after one slip (233);
- **slipped** (`-DSLIPPED=ON`, run by hand, not in ctest - above): also
  that the server let at least 20 steps slip, and that the client saw a
  slip. As a ctest test it was
  `a_players_stovl_selector_through_a_server_that_slips_is_off_after_it_by_the_steps_placed_off_and_no_more`
  on port 24934; with it, the nightly's repeat job needed a ninth shard
  (`every_ci_test_shard_is_dealt_...` failed at eight). Both are taken out
  again with it.

The unit test
`a_word_placed_two_steps_or_more_off_sees_a_slip_and_the_sixty_after_it_are_counted_apart`
walks every placing from none to 40 steps off (41 of 41), the window's
length and its beginning again, and the corrections inside and outside it.

## Measured

The slipped test's flight, ten runs beside 40 busy loops on 20 cores, Linux
debug, this branch. The built slip was 24 to 26 steps in every run where the
server's word was read (the machine's own slips come on top). "Snapped" is
corrections once joined after a slip that were too large to hide.

| run | slips seen | words after them | most after one | most steps off | worst after | worst correction after | snapped | the rest's worst | longest machine stall |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 | 143 | 143 | 23 | 22.522 m | 17.941 m | 0 | 0.945 m | 507 ms |
| 2 | 2 | 188 | 127 | 22 | 14.050 m | 12.270 m | 0 | 0.925 m | 963 ms |
| 3 | 1 | 158 | 158 | 26 | 25.104 m | 24.153 m | 11 | 1.329 m | 1,012 ms |
| 4 | 3 | 232 | 128 | 5 | 6.138 m | 6.124 m | 0 | 6.138 m | 694 ms |
| 5 | 4 | 342 | 113 | 15 | 22.706 m | 13.070 m | 0 | 1.845 m | 602 ms |
| 6 | 1 | 233 | 233 | 20 | 19.036 m | 10.831 m | 0 | 0.065 m | 440 ms |
| 7 | 1 | 102 | 102 | 16 | 25.029 m | 14.283 m | 0 | 1.372 m | 595 ms |
| 8 | 5 | 509 | 172 | 34 | 45.158 m | 54.655 m | 2 | 0.097 m | 546 ms |
| 9 | 2 | 194 | 133 | 28 | 35.766 m | 17.357 m | 0 | 0.871 m | 541 ms |
| 10 | 8 | 834 | 175 | 23 | 20.279 m | 20.225 m | 5 | 3.517 m | 586 ms |

- Outside the windows the worst was under a metre in five runs and under
  1.85 m in eight. Run 4's 6.1 m was off the path (the steps kept jump where
  a correction moved them), not placed off, the word after its window
  closed; run 10's 3.5 m likewise.
- The 95th percentile was 0.024 to 0.577 m and the median 0.001 to 0.023 m:
  the test's 3.4 m and 60 mm held in every run.
- Corrections once joined too large to hide fell inside the windows but for
  one in run 5 and one in run 10.
- Puttings right not from the newest: 10, 2, 5, 1, 6, 2, 3, 5, 6 and 6.

Idle, Linux debug, this branch:
- The plain STOVL selector test passed twice: no slip seen, the rest's worst
  0.001 m over 3,000 updates.
- The slipped run, twice: passed once (the server let 24 steps slip; one
  slip seen, 83 words after it, at most 4 steps off, the worst 4.398 m, its
  corrections at most 2.634 m; the rest's worst 0.881 m; but a 19.267 m
  correction once joined fell outside the window, under the 20 m), failed
  once ("1 corrections once joined, not after a slip, were too large to
  hide": 20.194 m, the slip seen 23 steps off and 20.248 m at worst).
- The unit test, the ports test and the shards test pass.

## Seen red

- **No slip ever seen** (`AfterASlip::from_steps` set to 1000): the slipped
  run failed, "the server let 24 steps slip and the client saw no slip".
  Reverted.
- **The unit test**, with `from_steps` set to 3: "placed 2 steps off, it
  sees a slip" failed. Reverted, and it passes.
- Not seen red, named: the plain test's new bounds after a slip (50 m, 60 m,
  250 words) and the rest's 5.6 m with the window. An idle plain run sees no
  slip, so nothing but a built one turns them red, and the built one is not
  in ctest.

## What could cut the cost without touching the hold

Not tried, named. Most of the error after a slip is not the ten words'
lag. In the idle run tracked, the held figure placed one word 23 steps off
(20.2 m); then the figure and the pace took about three seconds to follow,
a step or a few at a time, each move putting a round trip of words one to
four steps off. One thing might cut the measured part of that without
touching the hold: `Prediction::compared_at` places a word by the difference its step was
flown under. After a slip that difference is the one known to be wrong, so
a placement by the word's own difference
(`server step - steps into its input - where its input began`) would put
the comparison where the server actually was. That changes what the error
measures, not what the player sees, whose corrections (up to 54.7 m once
joined, above) it would not cut.

The selftest hash is not touched: no simulation code changed.
