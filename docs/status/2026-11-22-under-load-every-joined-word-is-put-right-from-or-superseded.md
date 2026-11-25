# 2026-11-22: under load every joined word is put right from or superseded

Phase 12a, "Under load fewer than nine in ten of the STOVL selector test's
updates are corrections once joined".

**What is still not true first.**
- **The item closes on its own check (owner, 2026-11-25).** All ten
  loaded runs passed it. Three failed the test on other checks:
  - two on the worst error, 6.732 m and 17.438 m against 5.6 m. Each came
    after the server really slipped, with words 5 and 21 steps off. That
    is the held clocks' difference's accepted cost (2026-11-22, "the
    cost"). It is now its own Phase 12a item, whose rule is the owner's and
    is not changed here.
  - one on the machine's stalls: an 8,865 ms stall 133.6 s in, then five
    more within 8 s. The stalls check refuses to judge that run (6 stalls,
    at most 2 allowed). That is out of this item's scope.
- Only glideslope_cli counts its joined words this way. The window client
  puts itself right in the same way (frontend::OwnWords), but it says
  nothing like this and no test holds it.
- The STOVL selector test's other bounds are as before: its 95th
  percentile and median are still set from every platform's CI figures,
  and Windows' two-step spread is its own item.

## What the check counted (measured)

The STOVL selector test's flight, beside 40 busy loops on 20 cores (Linux
debug, main at d8da8d14), with `--track`. For this, glideslope_cli's track
now also writes a `pass` line each time the socket is read dry: when, and
how many words were read.

- The client read its socket dry 3,162 times for 3,091 words. The time
  between two passes was 47 ms at the median, 88 ms at the 95th
  percentile, 122 ms at the 99th. The server says where she is 25 times a
  second, every 40 ms.
- Words in a pass: none in 1,157, one in 1,145, two in 724, three in 109,
  four to seven in 25, and 59 in one.
- The 59 came once, 1.8 s in, as she joined: a 395 ms stall of the
  machine, on top of the server catching up its start. Those words spanned
  2.4 s of the server's time. That stall is already counted apart
  (`the machine's stall 1`).
- Each pass with words put right once, from the newest (2,005 times).
  The other 1,087 words were compared and superseded. 1,981 corrections
  once joined, against the 2,700 the test asked for.
- The prediction itself was not worse for it: the 95th percentile was
  0.000 m, the worst 0.003 m, over 3,000 updates.

So the batches are not the read loop starved or the server sending in
bursts. They are a debug client's frames coming slower than the server's
words under load: each pass flies about six steps, puts right once and
flies about 25 steps again, with half a core. The largest batches, 35 to
78, come after a stall of the machine, which the stalls check counts
apart.

## Decision: putting right from the newest is correct; the check counted
## the frame rate

A client puts itself right once a pass, from the newest word it read
(frontend/own_words.hpp; Gambetta, *Client-Side Prediction and Server
Reconciliation*; Valve, *Source Multiplayer Networking*). Put right from an
older word and then the newest before a frame is drawn, what is drawn is
the newest's: the newer word's state and the inputs since, flown again.
So a superseded word costs the player nothing. It is still compared, at its
own step, so the error figures count every word.

Holding the corrections to nine in ten of the words compared held the
client's frame rate to more than 25 a second. That is not what the check
was for. Its comment said what it was for: that the "no correction too
large to hide" bound is held over the prediction, not over nothing.

**The rule now** (tests/cmake/joined_words_check.cmake, used by the STOVL
selector test and the vertical landing test, which had the same check).
glideslope_cli says `joined words: N compared, P put right from, S
superseded by a newer read with them, L let go of, D drawn without
either`. Words still in a pass when its prediction is let go of, whether
mid-pass, at a switch or while putting right, are counted in L.
P + S + L + D = N holds by construction, so the sum guards nothing. What is
guarded is:
- **D = 0: no frame was drawn with a joined word of its pass compared and
  not yet put right from or superseded.**
- **Nine in ten of the words compared were joined** (the clocks'
  difference known), so the corrections' bound is held over the
  prediction.
- **Every putting right was from the newest word** (frontend::OwnWords'
  "neither" is 0), which own_words_check.cmake also asserts. That file is
  not included whole, because it fails an idle run whose every pass read
  one word ("the rule went untested").

**First placed wrong, and the check found it.** The first count closed a
pass when the client flew on (`advance`), not when it drew. In a loaded run
it then said 1,898 of 3,000 were "flown on from" without being put right.
The client also flies on before each input it sends, mid-pass (2026-10-22),
and puts right afterwards, replaying those steps. That is not a loss, so
the pass is now closed when a frame is drawn.

**Seen red**, each in an idle run of the STOVL selector test, then
reverted:
- Put right only every other pass in the main loop: "175 were in a frame
  drawn before they were put right from or superseded".
- A word counted joined only 30 s after the prediction settled: "only 2296
  words compared once joined, under 2700" (2026-11-25, on main with #405).
- The oldest word of a pass offered to put right from, not the newest
  (frontend::OwnWords::offer): "1 of 3036 puttings right were not from the
  newest word" (2026-11-25).

## Verification

- **The item's verification**: the STOVL selector test, ten runs through
  ctest beside 40 busy loops on 20 cores, Linux debug, this branch:

  | run | result | joined words compared | put right from | superseded | drawn without either | most in a pass | 95th percentile | worst |
  | --- | --- | --- | --- | --- | --- | --- | --- | --- |
  | 1 | passed | 3,000 | 1,714 | 1,286 | 0 | 45 | 0.000 m | 0.002 m |
  | 2 | passed | 3,000 | 1,652 | 1,348 | 0 | 43 | 0.001 m | 0.005 m |
  | 3 | passed | 3,000 | 1,660 | 1,340 | 0 | 51 | 0.001 m | 0.003 m |
  | 4 | passed | 3,000 | 1,726 | 1,274 | 0 | 17 | 0.001 m | 0.005 m |
  | 5 | passed | 3,000 | 1,500 | 1,500 | 0 | 24 | 0.001 m | 1.410 m |
  | 6 | failed: machine's stalls | 3,000 | 1,478 | 1,522 | 0 | 59 | 0.001 m | 0.006 m |
  | 7 | passed | 3,000 | 1,470 | 1,530 | 0 | 32 | 0.001 m | 0.879 m |
  | 8 | passed | 3,000 | 1,555 | 1,445 | 0 | 49 | 0.001 m | 1.403 m |
  | 9 | failed: worst | 3,000 | 1,533 | 1,467 | 0 | 78 | 0.002 m | 6.732 m |
  | 10 | failed: worst | 3,000 | 1,389 | 1,611 | 0 | 64 | 0.002 m | 17.438 m |

  Every run had 0 let go of, and every putting right was from the newest
  word read (0 "neither"), so all ten pass the check as it now stands too.
  These runs were made before the rebase onto #405. Under the old check
  all ten would have failed: 1,389 to 1,726 put right from, against 2,700.
- **Before the pass was closed at the frame drawn**, a first set of runs
  was stopped after run 1, which failed with 1,898 "flown on from" (above).

- The vertical landing test, which has the same check, passed idle: 9,241
  joined words compared, 9,240 put right from, 1 superseded.
- Selftest hash: `27e94c795b47141c`, the same as main's. The selftest flies no
  prediction.
