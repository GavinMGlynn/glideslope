# A client whose every frame is held a second stays under the 20 m bound

**Done.** The window client with every frame held a second is put right by
6.9 m at worst, two at once beside 24 busy loops, where it was put right by
54 and 159 m. The cause is in the 2026-11-18 file; this one is the fix.

## Fix

Three changes, each to what a long frame did to the client's pace.

1. **A frame's words are one look** (`sim::Pacing::heard`). Words read
   together carry the frame's one local time. Each after the first now puts
   the loop back as it stood before the frame and is taken in its place, so
   the loop steps once a frame, over the time since the frame before. Before,
   with no time between two words, every move of the clocks' difference was
   forgiven as a jump.
2. **The loop is slower when looks are sparse** (`sim::Pacing`). Taking a
   frame's words as one look, with gains set for 25 looks a second, the
   loop rang: it flew at 1.2 to 1.4 against a server keeping real time.
   A look a second came less than twice in each radian of the loop's 0.5
   rad/s, with a frame's delay on top. Three parts:
   - The natural frequency is now at most one radian in five looks, using a
     running average of the time between looks. That is 0.2 rad/s at a look
     a second, and 0.5 as before at 25 looks a second.
   - The integral is kept as the pace it adds, so a change of gains moves no
     pace already learnt.
   - What the loop asks is flown for 1.5 looks' time when that is longer
     than the quarter second `steered_without_a_word_s` allows. Flown for a
     quarter of each 1 s frame, the pace barely left the integral's, and
     against a server at 0.5 the difference ran 1,300 steps away.
   The integral is still not wound up past what can be flown.
3. **The session clock's rate is fitted to one point a long frame**
   (`net::SessionClock::heard`). A frame longer than 0.1 s contributes only
   its newest update, the one that waited least. Before, a frame was a
   column of points a frame's session time tall at one local time. The line
   through such columns, the first after a slow start or one partly heard,
   said 0.5 to 0.64 of a server keeping real time, and the pace started from
   it. Two points now suffice for a fit, since a client looking once a
   second holds three in its window. Frames of 0.1 s or less keep every
   update: keeping only the newest of two put the fit of a server at 1.25
   with 60 ms of jitter 0.015 off, failing
   `the_client_follows_the_servers_clock_at_the_servers_own_rate`.

Searched first: "PI controller variable sample time anti-windup",
"NTP clock discipline FLL PLL long poll interval", and "game client tick
rate adjustment server time dilation". Two findings shaped the fix:
- A discrete PI's gains depend on its sample time.
- NTP makes its loop time constant longer as its poll interval grows (Mills;
  RFC 5905).

## Verification

**The on-server window test, frames held 1 s** (`tests/cmake/client_on_server.cmake`,
`-DSLOW_FRAMES=1000`), Linux debug (sanitized), two clients at once beside
24 busy loops on 20 cores, three runs:

| run | client a | client b |
|-----|----------|----------|
| 1   | 4.145 m  | 6.908 m  |
| 2   | 2.351 m  | 2.318 m  |
| 3   | 6.909 m  | 4.607 m  |

None snapped. **Before**, the same build with `src/sim/pacing.*` and
`src/net/interpolation.*` put back, the same load, two runs: 27.6 and 24.9 m,
then 20.7 and 69.1 m, each snapped 2 to 6 times. The 2026-11-18 runs gave 53.9
and 158.8 m. Alone, through ctest, after: 4.1 m over 32 corrections, and
1.4 m once the clocks' difference was known.

The ctest case that held frames 0.7 s now holds them a second, and is
renamed `a_client_whose_every_frame_is_held_a_second_is_never_put_right_too_far_to_hide`.
Its CI cost entries are renamed with it.

**Unit tests**:
- `a_client_whose_every_frame_is_long_holds_its_clocks_difference_and_flies_at_the_servers_pace`
  covers frames of 0.25, 0.5 and 1 s, each against servers at 0.5 to 1.0 of
  real time in tenths: 18 cases. Over the last ten of 60 s, each flies
  within 0.002 of the server's pace and holds the difference within two
  steps. **Seen red** before the fix: at 1 s, 0.68 against 0.5, and the
  difference 1,302 steps off. With the gains not scaled, 0.63 to 0.88.
- `a_client_whose_frames_are_long_fits_the_servers_rate` covers the same
  three frame lengths against servers at 0.5, 0.8, 1 and 1.25, each after a
  5 s stall. From two seconds after the stall, the rate is within 0.03; it
  was 0.027 off at worst. **Seen red** with the old fit: 0.56 to 1.2 off.
- Every test matching pace, paced, clock, predict, impair, drawn_at or
  session passes: 88 of them, including the paced and impaired prediction
  tests and the 60 fps pacing test, with its jumps and slowings.
  - Those 60 fps figures are unchanged: the gains and the steered time are
    the same at 25 looks a second.

**Not covered**: frames of 2 s converge more slowly. Against a server at
0.5, a 60 s run ends 0.014 off the pace with the difference 40 steps off.
No test holds frames that long.
