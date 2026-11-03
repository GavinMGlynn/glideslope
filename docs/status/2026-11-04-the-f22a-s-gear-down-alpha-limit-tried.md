### The F-22A's stall-figure tests: a gear-down alpha limit tried, nothing committed but this note - 2026-11-04

**What is still not true.** The item stays open, and every name it lists is
still on, at the figures of
2026-11-03-the-f22a-s-stall-figure-tests-investigated.md. This round stopped
at its hour without a change that keeps the tree green. No model, test or
figure is changed; this file and nothing else is committed.

**What was tried** (linux-release with FMA only; no x86-64-v2 build and no
Windows build were made). A gear-down alpha limit in her flight controls
(tools/make_f22.py, not committed): past an onset alpha, with her gear down,
a nose-down pitch rate per degree in her pitch loop's rate error, and a
nose-down stabilator per degree after the loop. **Tuned, not sourced**: no
published limit or gain was found. Searched again on 2026-11-04 (her power
approach alpha limit and her gear-down control law): only the YF-22 mishap
report as relayed on PPRuNe, and the Air Force's April 2005 mishap release,
both already cited on 2026-11-03.

Her stall at idle was then measured as 14 CFR 25.103(a) measures one, not by
the project's own departure rule: the same flight as `departure_speed`
(throttles closed, gear down, her autopilot holding her height), but at the
greatest lift coefficient, its calibrated airspeed divided by the root of the
load factor normal to the flight path. At her limit she sinks at 0.72 to
0.89 g with her stick at its aft stop. That is 25.201(d)(3)'s stall: the
pitch control at its stop and the nose rising no further. Each entry speed
from 160 to 280 KCAS gave the figure within 0.8 kt.

| onset, rate per degree, stabilator per degree | her stall, KCAS | at her warning: lost / g | thirty seconds: lost (bound) / g | most alpha before the recovery |
|---|---|---|---|---|
| 18, 0.05, 0.1 | 109.4 to 109.9 | 247 ft / 1.27 | 637 (2,084) / 1.61 | 22.6 |
| 17, 0.06, 0.1 | 113.8 to 114.5 | 187 ft / 1.25 | 639 (2,144) / 1.62 | 20.8 |
| 17.5, 0.06, 0.1 | 112.4 to 113.0 | 188 ft / 1.24 | 635 (2,102) / 1.62 | 21.4 |
| 17, 0.06, 0.15 | 114.7 to 115.0 | 185 ft / 1.26 | 4,511 (2,194) / 1.60 | 20.6 |
| 16, 0.05, 0.1 | 114.7 to 114.8 | 222 ft / 1.31 | 5,250 (2,147) / 1.65 | 20.4 |
| 18, 0.08, 0.1 | 114.6 to 115.4 | 2,602 ft / 1.51 | 4,738 (2,237) / 1.62 | 21.0 |
| 18, 0.10, 0.05 | departed from 160 KCAS | 231 ft / 1.27 | not recovered | departed |
| 19.5, 0.15, 0.1 | departed: a limit cycle | - | - | - |

So a limit of about 17 degrees holds her clear of her 22-degree pitch-up,
and with her stall measured at it, both stall recoveries come within their
bounds: 17/0.06/0.1 and its neighbour 17.5 pass both. A stiffer loop
oscillates and departs.

**Why some thirty-second recoveries lose 4,500 ft or more.** The
recovery reaches its speed within about 650 ft. Then, pulling out at
1.6 g near 156 kt, she is at 17 to 18 degrees of alpha. The autopilot's
stall alpha is learnt from where her lift peaked, and under the limit that is
the limit itself. So it takes her for stalled again
(sim/autopilot.cpp, `seen_to_stall_`) and unloads her to 1 degree of alpha
at full power. She dives from 160 to 600 kt before she is level. That is
the generic autopilot meeting a lift peak that the limit made, not the
wing's.

**Why it was not committed.** Her stall at the limit is about 114 KCAS,
not 105.8. Her approach speed would then be about 148 kt, 1.3 times it.
That moves everything that the change of 2026-11-03 moved, all to be
measured and recorded again:
- her measured landing distance and the copilot's table;
- her plan speeds;
- the recording `cbd-orbit-f22-anthropic.jsonl`;
- her gust namings, which another item holds.

None of the five landing and roll-out names was measured under the limit.
Nor were the x86-64-v2 build or Windows, where her stall figures have
scattered before. That did not fit the hour.

**For the next round**: the limit at 17, 0.06 and 0.1, and the 25.103(a)
measurement (a figure attribute beside `departure`, which throws if she
pitches up past her limit); then the cascade above. Then the stall watch,
the five landing names and an x86-64-v2 build. Watch the thirty-second
recovery's dive: it hangs on the autopilot's learnt stall alpha.

**Verification.** This round's figures are each from
`glideslope_tests the_f22a_s_stall_at_idle_landing_configured_does_not_hang_on_its_entry_speed`
(with its bounds widened, locally) and from a local trace of `fly_a_stall` for
her alone at the warning and at thirty seconds, linux-release, no display.
Nothing measured here is committed; `python3 tools/make_f22.py --check`
passes on what is.
