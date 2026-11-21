# 2026-11-22: a client joining with no tiles cached is judged off the path

Phase 12a, a tail of "One end fetching a tile the other has differs by the
lift there" (ticked 2026-11-21; it stays ticked: the lift does arrive with
the tile, and the test now shows it on a busy machine too).

## The CI failure

`a_client_joining_over_hills_with_no_tiles_cached_flies_the_servers_ground_lift_within_the_seas_bound_once_they_arrive`
failed on CI run 44921665961, Ubuntu linux-debug, shard 8 (job 136899956049):
once its tile was had, the fetching client's median error was 0.440 m over
300 updates, more than the sea's 0.25 m. Its passing figures are 0.035 to
0.041 m. The client's own lines were not in the log.

## Cause

**Confirmed: the words were placed a step off, and the air was flown
right.** The test judged the error at the step each word's input placed it
at. On a loaded machine the server is held up unevenly, so a word is placed
whole steps off ("On a busy machine the server's word is placed a step off
now and then", Phase 12a). Its error there is a step's travel, about 0.44 m
at the Cessna's speed, on a path flown alike. Reproduced here beside twice
as many busy loops as cores: the median at the placed step was 0.400 to
0.755 m in 6 of 7 loaded runs (0.033 m in the other), while the median off the path flown was 0.010
to 0.016 m. Idle, both were small (0.033 to 0.038 m and 0.009 to 0.010 m).

## Fix

- `glideslope_cli connect --predict` now says the median of each update's
  distance from the path it flew ("prediction error off the path median"),
  and the same once the air's ground was had. With that it counts the updates
  it did not hold to the path: those across a machine's stall, and those
  nearest an end of the steps it kept.
- The test judges the off-path median once the tile was had. It must be
  within 0.25 m and under LIFT_SHARE of the liftless client's off-path median.
  The median at the placed step is still said, but not judged. Every update
  once the tile was had must be held to the path or named as not, and at least
  half must be held. Under load, 23 to 85 of 300 were named: 0 to 85 across a
  stall, and 0 to 23 at an end.
- LIFT_SHARE goes from 0.5 to 0.4, from the figures below.
- A failing judgement prints both clients' full lines, so the next CI red can
  be read from the log.

## Verification

Debug build, WSL, 2026-11-22:

- **Passing, fixed:** idle, 3 runs: 0.009 to 0.010 m off the path, against
  0.085 to 0.107 m over no ground (0.08 to 0.12 of it). Beside twice as many
  busy loops as cores, 7 runs: 0.010 to 0.016 m, against 0.080 to 0.096 m
  (0.13 to 0.19 of it), each within both bounds. The old judgement would have failed 6
  of the 7 loaded runs (0.400 to 0.755 m at the placed step). The first 3 loaded runs
  were made before the count of updates not held to the path was added. They
  failed only on the first form of that count, which wanted nine in ten held
  to the path, and so led to its present form.
- **Seen red:** the client's lift thrown away once its tile was had (its air
  asked the ground and then threw `GroundNotHad`; the tile was still fetched).
  Idle: 0.069 and 0.085 m against 0.105 and 0.084 m, 0.66 and 1.01 of it.
  Loaded: 0.083 and 0.067 m against 0.085 and 0.115 m, 0.98 and 0.58 of it.
  All 4 runs failed the share. They were run against 0.5, and 0.4 is tighter.
  Reverted.
