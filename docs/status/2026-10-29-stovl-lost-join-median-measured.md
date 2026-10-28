### The STOVL join with its first datagrams lost: its median bound measured, 2026-10-29 - tail done

**What is still not true first.**
- **Why the lost scene's error is larger than the join scene's is not
  known.** Its error lies along her path, not off it, which is updates
  placed a step off by the clocks' difference (Phase 12a's open item, "on a
  busy machine the server's word is placed a step off now and then"). That
  the lost scene's first words never came is a likely part of it; it is not
  shown.
- **Her pitch check went over half a degree on this machine under load.**
  Linux debug, with other agents compiling beside it (load average 13 to 15
  on 20 threads): three of ten runs failed at 0.502, 0.547 (lost) and 0.539
  degrees (join), against 0.114 to 0.394 in the seven that passed. CI has
  not failed it. Not changed here; the figures are for whoever bounds it.
- **The join scene's 60 mm holds on Windows with about 12 mm to spare**
  (its median 48 mm at most in five runs). It is kept: it has not failed.

**The failure.**
`a_client_joining_an_f35b_mid_bank_with_the_first_datagrams_lost_still_begins_on_the_servers_law_state`
(tests/cmake/server_stovl_join.cmake, `lost`) failed three times on CI's
Windows runners on its median, every other check inside its bound. Its
60 mm was server_stovl_vertical_landing.cmake's, copied, never measured for
this scene.

| Where | Run | Median | 95th pct | Worst | Off the path, worst |
|---|---|---|---|---|---|
| windows-clang | 40743161009 | 72 mm | 0.647 m | 1.153 m | 0.147 m |
| windows-debug | 40750172534 | 77 mm | 0.594 m | 1.260 m | 0.066 m |
| windows-debug | 40769073518 | 63 mm | 0.588 m | 0.736 m | 0.056 m |

Windows debug, the development machine (tools/windows_build.sh, then
`ctest -V`), five runs of each scene, the 120 mm bound in place:

| Scene | Medians | 95th pcts | Worsts | Pitch |
|---|---|---|---|---|
| join | 48, 46, 35, 35, 39 mm | 1.049, 0.858, 0.543, 0.612, 0.567 m | 0.700 to 1.326 m | 0.023 to 0.038 deg |
| lost | 56, 61, 59, 60, 54 mm | 1.265, 0.559, 0.614, 0.605, 0.625 m | 0.709 to 2.648 m | 0.030 to 0.097 deg |

One of the five lost runs (61 mm) would have failed at 60 mm.

Linux debug (gcc, FMA), five runs of each scene, on a busy machine:

| Scene | Medians | 95th pcts | Worsts | Pitch |
|---|---|---|---|---|
| join | 16, 32, 21, 50, 13 mm | 29, 67, 42, 108, 26 mm | 0.031 to 1.479 m | 0.114 to 0.539 deg |
| lost | 27, 38, 19, 45, 24 mm | 65, 75, 40, 89, 59 mm | 0.784 to 1.547 m | 0.163 to 0.547 deg |

Windows' medians sit two to three times Linux's in both scenes, and its
95th percentiles ten times, about a step's travel: the platform's timing,
not the scene alone.

**The decision: the platform's drift, bounded.** The error is along her
path (at most 0.147 m off it, against medians of 63 to 77 mm and 95th
percentiles near 0.6 m, about a step's travel at her 107 KCAS), so it is
where the server's word is placed in time, which Windows' 15.6 ms timers
spread - not her law or her flight. The lost scene's median is held to
120 mm: half again the worst on any platform, CI's 77 mm, and half the 246 mm of a client
begun without the law (2026-10-22), which her pitch check, 2.784 degrees
off then, catches first in any case. The join scene keeps 60 mm (CI has not
failed it; 48 mm on Windows and 50 mm on Linux under load are its most). The 95th percentile
(3.4 m) and the worst (5.6 m) are unchanged: every figure is well inside
them.

**Verification.** Both scenes pass on Linux debug and on Windows debug at
the bounds above (Linux: the pitch failures named first aside). **Seen
red**: with its bound set to 10 mm, "the median prediction error was 38 mm,
over 10 mm"; restored.
