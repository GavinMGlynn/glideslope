# A predicting client's files are written on a thread of their own, 2026-11-25

Closes the Phase 12a item "A predicting client went 401 ms of its own
without drawing a frame". A repeat on macOS gave a breakdown that places
the gap. Its cause, a write to the client's track file that the runner's
disk held up, is fixed. The 401 ms of nightly 44954156532 is put down to the
same cause because it looks the same, not because it was shown to be: that
run came before the breakdown existed. Its frames stopped and no socket was
read, so no work of the client's is known in that gap, and before #403
writes were already counted as the client's own time.

## Where the gaps were looked for

- **Nightlies since #403** (45076557747, 45317106368, 45523889599): all 120
  macOS network-check runs passed. A passing run printed nothing, so these
  gave no breakdowns. That is why the first change here was needed.
- **The workflow now prints every run's breakdown**
  (`.github/workflows/nightly.yml`, `macos-network`): the longest gaps, every
  aircraft built on a thread, and every prediction begun, for every run
  whether it passed or failed.
- **The client now says** each prediction it began: when, how many steps it
  caught up, and the wall time and CPU time that took. It also reports every
  build, including the spare begun at a take-back, which nothing flies and
  which never used to report.
- **One dispatched run** of 100 per check (run 45558329694, branch
  `12a-macos-401ms-gap`, macos-15) gave 200 breakdowns.

## What the gaps were made of (macOS debug, 200 runs)

- In 193 of 200 runs, the longest gap of the client's own was the frame in
  which a prediction began. It was CPU time, with almost no sleep. Beginning
  at the join took a median of 35 ms of wall time and at most 121 ms (CPU
  33 / 89 ms). Beginning at the take-back took a median of 49 ms and at most
  82 ms (CPU 48 / 75 ms). In this frame JSBSim is set up, its engines
  settled, and the steps since the server's word are flown. The longest such
  gap was 111 ms, of which 110 ms was CPU time.
- In the other 7 runs the longest gap was elsewhere. Six of them were 39 ms
  or less.
- The seventh failed the check (runner 3, run 23, 200 ms):
  **315 ms of its own, ending at 12.074 s: 3 ms of CPU time, 2 ms asleep,
  and the rest waiting on its files' writes.** In its track (artifact
  `network-tracks-macos-3`) the client drew nothing and read nothing from
  11.759 s to 12.074 s. It was mid-flight, with no switch near and no build.
  The breakdown said "594 ms" of writes. That figure counted one wait twice,
  because `xsputn` writes through `overflow` and both were timed. The wait
  itself was about 300 ms.
- The machine's share was never more than 170 ms in any run.
- On linux-debug in this worktree, the longest gap is also the frame in
  which a prediction begins: 31 of 34 ms, 26 ms of it CPU time, with seven
  steps caught up. Pinned to two cores the longest was 57 ms.

**The runner's steal time.** A macOS guest has nothing like Linux's steal
time. Virtualization.framework schedules the virtual CPUs inside the host
and tells the guest nothing about it. The closest counter Apple offers is
`hv_vcpu_get_wait_for_interrupt_time`, on the Hypervisor framework's host
side. Web searches turned up reports of uneven test times on hosted macOS
runners (actions/runner-images #11845, #9254; GitLab's M1 runners, issue
77). None of them reports a sub-second stall, and none tells a held disk
from a held CPU. So steal time could not place the gap, and the breakdown
did.

## The fix

The track (`--track`) and the report (`--heard`) are written on a thread of
their own (`TimedFileBuf` in `src/frontend/cli/main.cpp`). The client's
thread collects lines into 64 KiB chunks and hands each chunk over under a
lock. It never waits on the disk. The handing over is timed as the client's
own time, as before. Whatever is still unwritten when the file is let go of
is written then, and the thread is joined. The client also reports the
longest single write ("its files' writes: the longest N ms").

## The test, seen red

Both impaired checks (`server_impaired.cmake`) now pass the predicting client
`--slow-write 10 500`. This holds the track's first write that comes ten or
more seconds after opening for 500 ms, as a slow disk does. The checks
require the client to report a longest write of 500 ms or more, which proves
the hold happened, and keep the existing bound of 250 ms of the client's own
between two frames.

- **Red**: with the hold built into the old writes, on the client's own
  thread, the 100 ms check failed: "went 508 ms of its own without drawing a
  frame (ending at 10.504 s: 2 ms of its thread's CPU time, 6 ms asleep ...
  500 ms waiting on its files' writes)". That is the macOS failure's
  signature. With the nesting fixed, the 500 ms is counted once.
- **Green**: with the writes on their own thread, both checks pass. The
  longest writes were 500 ms, and the longest gaps of the client's own were
  53 and 41 ms, with 0 ms waiting on writes.

## Verification

- linux-debug: both impaired checks, red and then green, as above.
- The other multi-process tests whose clients write `--track` or `--heard`
  pass (listed in the pull request).
- The macOS figures above, from run 45558329694.
