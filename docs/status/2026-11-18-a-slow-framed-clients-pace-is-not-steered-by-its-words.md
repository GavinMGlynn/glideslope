# A slow-framed client's pace is not steered by its words: the cause found, not fixed

**Not done.** The window client with every frame held a second is put right
by far more than 20 m on a loaded machine. The cause is found. A fix for
part of it was tried and is not landed. Its patch is on PR #383 as a comment.
The plan item stays open.

## Figures

The on-server window test (`tests/cmake/client_on_server.cmake`) was run
with `-DSLOW_FRAMES=1000`, Linux debug (sanitized), on 2026-11-18:

- **Alone, three runs**: the worst correction was 2.76, 2.76 and 5.53 m. Each
  came while the clocks' difference was being learnt. Once it was known, the
  worst was 0.39 m. It passes.
- **Two clients at once, beside 24 busy loops on 20 cores**: 53.9 m and
  158.8 m, both snapped, once the difference was known.

## Cause

The window client reads a frame's words together, so all of them carry the
frame's one local time (`Online::listen`, `arrived_s_`). Each word steers
the pace (`Flight::pace_by_clocks`, `sim::Pacing::heard`):

1. **Words read together are taken as separate looks.** Between two of them
   no time has passed, so the most the difference may move without being a
   jump (`most_parting`) is one step. Inputs go once a frame, so the clocks'
   difference moves once a frame, and always between two words read
   together. Every such move was forgiven as a jump and never steered by.
2. **The session clock's rate fit is skewed by the same batches.** It was
   fitted while a frame's words were only partly heard, and said 0.5 to 0.64
   against a server keeping real time (1,488 steps in 12.9 s). The pace
   started from it at 0.64.

So the client flew at about 0.65 of the server, and the difference rose
about 30 steps a frame. Placement uses the least difference over 50 words,
which lagged that rise by 35 to 72 steps. The F-15C covers 1.38 m a step, so
the corrections came to 48 to 159 m. In the diagnostic lines, the moved
distance equals the lag times 1.38 m.

## Fix tried (not landed)

`Pacing::heard` took words read together as one look. Each word after the
first of a batch put back the loop's state from before the batch, so the
loop stepped once a frame over the frame's time. Beside 24 busy loops this
gave 11.1 m in one run (not snapped). The other run was still snapped:
22.1 m and 34.5 m. The loop now overshoots, at a pace of 1.2 to 1.4. It is
sampled about once a second with its measurement a frame late, and the gains
were chosen for 25 looks a second. Within a frame flown 0.3 off the server's
pace, the difference moves about 36 steps.

The patch also adds a unit test with every frame a second long (paced by
`fly_at_pace` with a frame length, each word carrying the input it was flown
on). Against a server at half real time, the old loop flew at 0.17 and the
patched one at 0.68. Both are red, so the steering is still wrong. The
patch's per-word sample history also turned the existing jump case red: the
pace went 0.24 from the server's at a -36 step jump, so that change needs
reworking.

**Still open:** steering when looks are sparse - gains or integral windup
for a look a second - and a rate fit that is not skewed by batches.
