### The Cherokee's 2.2 kt traced to her mixture: full rich on the FAA's curve; her plan's fastest measured again, 112 kt, 2026-10-10 — item done

**What changed.** 80aa204a (#142, 2026-10-09), which gave her engine the
FAA's mixture curve and a float carburettor (tools/make_pa28.py,
tools/piston_mixture.py) and held her full rich below 5,000 ft
(`mixture-lever 5000`, her handbook's rule). Nothing else: her JSBSim files
and catalogue entry are unchanged since. **Measured** with
`glideslope_cli --data DIR plan-speeds pa28` (linux-release, at 1f3430cf;
the commit that lands this, 31ecf991, changes only her plan speeds and the
docs, and the tests below ran on its tree),
the full-throttle level speed at 3,000 ft, on the build's data and on three
scratch copies of it (not committed):

| her engine | her mixture at 3,000 ft | level at full throttle | plan-speeds writes |
| --- | --- | --- | --- |
| before 80aa204a (JSBSim's table, no carburettor) | leaned (`mixture-lever 0`) | 129.1 kt | 117 |
| before 80aa204a | full rich (`mixture-lever 5000`) | 129.1 kt | 117 |
| the FAA's curve and carburettor | leaned | 128.4 kt | 117 |
| the FAA's curve and carburettor, as committed | full rich | **126.8 kt** | **112** |

The first row is the 129 her file was measured at on 2026-10-06. On JSBSim's
own table full rich and leaned make the same, because its metering richened
full rich at 3,000 ft onto that table's peak (tools/piston_mixture.py: a
peak 6.6% over sea-level full rich, which no engine has). On the FAA's
curve full rich is richer than best power, so she now gives 0.7 kt for the
engine and 1.6 kt for flying full rich, as her handbook says to below 5,000
ft. **So the figure is right and the file was stale**: her plan's fastest
is measured again, 112 kt (assets/figures/pa28.xml). 117 still held with
9.8 kt in hand, but plan-speeds' one rule writes 5 kt under the fastest
that leaves 5 kt in hand, and 122 now leaves 4.8.

**Her published figures compare like with like.** Her top speed, 152 mph
(132.1 kt) at sea level and full throttle, is under 5,000 ft, where her
handbook has her full rich; the `maximum_speed` flight flies her full rich
(no `lean`, the mixture at 1), 132.0 kt, in range. Her cruise, 75% power
at 7,000 ft, is above it, and the `cruise_speed` flight leans her to the
richest mixture giving the most rpm, 124.6 kt, in range. Her service
ceiling is flown on the autopilot, whose leaner holds her full rich below
5,000 ft and leans her above. All nine of her figures in range
(`glideslope_cli figures pa28`).

- **The selftest hash does not move**, `182dd6c996e0ee4c`: only her figures
  file changed.
- **Checked** (linux-release, `ctest -R
  "pa28|cherokee|leaner|every_aircraft_has_its_own|every_aircrafts_fastest_plan_speed|plan_speeds"`,
  33 of 34 passed, the server's landing of her skipped, its server not
  built here): her figures, both tightest orbits at 64 and 112 kt, one step
  past each, her crosswind
  headings at every plan speed, every aircraft's fastest leaving 5 kt in
  hand, the leaner's six, the plan-speeds CLI refusals, and her tuning
  script's files.
- **One step past her fastest passes by 0.2 kt**: 122 kt needs 127.0 level
  at full throttle for its 5 kt in hand, and she makes 126.8, so a few
  tenths of a knot more from her would flip that test red, and plan-speeds
  would then write 117 again.
