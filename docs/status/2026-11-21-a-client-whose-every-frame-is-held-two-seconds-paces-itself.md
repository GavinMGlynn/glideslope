# A client whose every frame is held two seconds paces itself

**Done.** In the pacing unit test, a client whose every frame is held 2 s
now flies within 0.002 of the server's pace over the last ten of 60 s, and
holds its clocks' difference within two steps of the one held. Before, it
was 0.014 off against a server at half real time, with the difference 40
steps off (about 55 m for the F-15C). The 1 s fix is in the 2026-11-19 file.

## Cause

Two parts, both in `sim::Pacing::heard`.

1. **The loop steered by a difference a look old.** A client with long
   frames sends one input a frame and reads the frame's words together. The
   difference it hears at one frame describes the input sent at the frame
   before. What it flew in between, at a pace other than the server's, has
   already moved the difference of the input it sent since, but the loop
   cannot see that until the next frame. The loop's delay was a whole look.
   To stay stable with that delay, the gains had to drop to one radian in
   five looks: 0.1 rad/s at a look every 2 s. A critically damped loop that
   slow needs far more than 60 s to work off the first error. Faster gains
   rang instead: three and 3.5 looks a radian rang with a period of about
   12 s.
2. **The gains started for 25 looks a second.** The time between looks was
   an average that moved a fifth of the way to each new look. So for the
   first several 2 s frames the loop steered at nearly 0.5 rad/s. It asked
   for less than the slowest pace, was flown at 0.1, and went 316 steps off.

## Fix

1. **Steered by the difference still to come.** Each look notes this
   machine's time and the paced time at it. At the next look, the paced time
   flown since, less what the server would have flown at the loop's learnt
   pace, is taken from the error the loop steers by. That is a Smith
   predictor over one look. Once the client flies at the server's pace the
   term is zero, so the difference is still held where it was. `worst_off`
   and `off` still report the difference heard. At 25 looks a second the
   term covers 40 ms of flying and changes little.
2. **Looks per radian 5 to 2.5.** With the delay taken out, 2 to 3 all pass.
   2.5 is in the middle. At 25 looks a second the loop is capped at 0.5 rad/s
   as before.
3. **The time between looks is never less than the look just taken.** It
   slows the gains at once and speeds them up gradually. Without this, 2 s
   frames pass, but 3 s frames were 74 steps off in the last ten seconds
   against a server at 0.5.

Searched: "Smith predictor PI controller integrating process dead time".
The MathWorks example "Control of Processes with Long Dead Time: The Smith
Predictor" pairs a PI with dead-time compensation that predicts the delayed
measurement from the controller's own output. The searches of 2026-11-19
also covered NTP's FLL at long poll intervals (RFC 5905). The literature's
caveat is a wrong internal model. Here the model is this client's own pace,
known exactly, and the learnt server pace. A wrong learnt pace only slows
the integral; it cannot hold the error away from zero.
An FLL in the style of NTP was considered and not used.
It would need to know which input each word describes, and Pacing is told
only the difference.

## Verification

`a_client_whose_every_frame_is_long_holds_its_clocks_difference_and_flies_at_the_servers_pace`
now covers frames of 0.25, 0.5, 1 and 2 s, each against servers at 0.5 to
1.0 of real time in tenths: 24 cases, asserted. Over the last ten of 60 s,
each case is within 0.002 of the server's pace and the difference is within
two steps. At 2 s, the worst was 0.0003 off and 0 steps.

**Seen red**: the new test was built and run against the old
`src/sim/pacing.*`. It failed on the 2 s frames:
`frames 2.000000 s long, against a server at 0.500000 of real time it flew
at 0.486077`. The fix was then put back, and the test passes.

Outside the test, in a scratch harness running the same `fly_at_pace`,
frames of 1/60, 0.1, 0.25, 0.5, 1, 1.5, 2 and 3 s were each flown against
servers at 0.5 to 1.0 of real time. Every case ended within 0.002 and two
steps.

Against slower servers, long frames settle more slowly. That is a named
limit, not tested:

| frames | server at 0.2: pace off | steps off, last 10 s |
|--------|-------------------------|----------------------|
| 1 s    | 0.003                   | 1                    |
| 1.5 s  | 0.017                   | 45                   |
| 2 s    | 0.018                   | 35                   |
| 3 s    | 0.044                   | 119                  |

**Jumps and slowings with long frames** (from the review of #393). These
cases are now in the same test, with bounds set at the measured figures
plus a margin. **Limit**: a look 1 or 2 s apart cannot tell a held-up
server's jump from a change of pace. Over one look of 1 s, two paces may
part by up to 61 steps (`most_parting`), so a jump smaller than that is
steered on. The faster loop swings more on a jump than the old one did, and
settles better after a slowing:

| case                     | after: pace swing / late steps / end pace off | before        |
|--------------------------|-----------------------------------------------|---------------|
| 1 s, jump 36 / -36 / 120 | 0.29 / 0.30 / 0.49; 0, 0, 1; 0.0005 at worst  | 0.15 / 0.15 / 0.24; 2, 2, 3 |
| 2 s, jump 36 / -36 / 120 | 0.14 / 0.15 / 0.48; 5, 3, 19; 0.009 at worst  | 0.08 / 0.08 / 0.25; 8, 5, 26 |
| 1 s, slowed to 0.3 / 0.6 | worst 153 / 96 steps; late 10 / 0; 0.005 / 0.0001 | 206 / 125; 15 / 5 |
| 2 s, slowed to 0.3 / 0.6 | worst 300 / 194 steps; late 53 / 6; 0.026 / 0.003 | 398 / 241; 143 / 63 |

A held-up server's catch-up after a stall arrives as a jump. With frames
a second or two long it moves the pace by up to half for a few looks. This
is named here; it is not fixed. Two cheap cuts were tried and dropped:
- **Capping the loop's proportional part at 0.15 or 0.25.** The 1 and 2 s
  jumps swung less, but slowings at sixty frames a second went 118 to 141
  steps off, past the bound of 80. Frames of 3 s against 0.6 and 0.7 ran
  away by 195 and 265 steps.
- **Leaving the jump out of the difference still to come.** That term is
  computed from what this client flew, so it never contained the jump.

**Seen red (the new cases)**: the test was built and run against the old
`src/sim/pacing.*`, with the new cases moved ahead of the rate cases so
that they ran first. It failed on the 1 s jump of 120 steps: 3 steps off in
the last ten seconds against a bound of 2. The fixed code and the test's
order were then put back.

The other pacing tests still pass, but their figures moved a little:
- Sixty frames a second: the paced test at rates of 0.2 to 1.0 stays within
  0.0006 and one step (bound 0.002). At 0.2 it is 0.0004 off, where it was
  0.0001. The reviewer's own harness, which sends differently, put 0.2 at
  0.0012, so the margin there is narrower than before.
- Jumps of 36, -36 and 120 steps: the pace went at most 0.042 from the
  server's. It was 0.044 before; the bound is 0.05.
- Slowing to 0.3: followed in 14.3 s (was 17.1), worst 72 steps (was 66;
  the bound is 80).
- Slowing to 0.6: 10.9 s and 39 steps (was 37).

Through ctest, Linux debug (sanitized), every test matching pace, paced,
clock, predict, impair, drawn_at, session or long passed: 105 of them. That
includes the window clients' F-35B stall test, the 200 ms jitter-and-loss
test and the prediction tests. The window client with every frame held a
second, `a_client_whose_every_frame_is_held_a_second_is_never_put_right_too_far_to_hide`,
also passes, run alone through ctest.

| run   | corrections | worst   | worst while the difference was learnt | worst once it was known |
|-------|-------------|---------|---------------------------------------|-------------------------|
| after | 32          | 2.764 m | 1.386 m                               | 2.764 m                 |

On 2026-11-19, alone, it was 4.1 m over 32 corrections, and 1.4 m once the
difference was known. That day's 7 m was a different run: two clients at
once beside 24 busy loops. That loaded run was not repeated, so that it
would not load the machine other agents share.

**Selftest hash**: `glideslope_cli selftest` gives 27e94c795b47141c, the
same as on 2026-11-19. The selftest does not fly through Pacing.
