### The F-22A holds every speed a plan may ask again, 2026-11-11

**What it does not do, first.**
- **Pull requests fly three of her 42 plan speeds** (197, 222 and 237 KCAS,
  below) besides her two ends; the rest stay with the nightly's
  `plan-speeds --every`.
- **Her throttle response is not sourced.** The filter removed here was the
  pinned model's, and no F119 spool time or F-22 throttle filter was found
  published.
- **Her sustained turn is out of its band: 7.87 deg/s against the report's
  7.34, whose band ends at 7.84** (`figures f22`, and the test
  `the_f22_sustains_its_published_turn_at_mach_0_9_and_30000_ft`, red). The
  turn is measured over 12 s from a throttle step, and without the filter
  her thrust arrives a second sooner. SPAN_EFFICIENCY, which that turn set,
  is left at 0.62 for the owner to decide, because every value that fixes
  the turn breaks something else (swept on linux-release):
  0.62 7.87; 0.61 7.82, 0.02 inside the band, her glide speed held, her
  climb-away speed 225 - which changes the brief, and the recorded CBD-orbit
  plan must be recorded again; 0.60 7.75, but she stalls gliding round her
  tightest orbit at her glide speed, 225 kt; 0.54 7.37, pinned to the
  report, and her glide stalls and her stall at idle moves 1.1 kt.
  Alternatively a half-second filter (lag 2) holds the three speeds with her
  turn at 7.63, but it moves her take-off speeds too, and its 42 speeds,
  glide and landing were not flown.

**What was red.** The nightly job "Every speed a plan may ask of every
aircraft" failed every night from 2026-11-03 (run 41728129167) to 2026-11-10
(run 42910882113). Every other aircraft held all its speeds; the F-22A held
39 of 42 from 157 to 360 KCAS. Reproduced on linux-release with
`glideslope_cli plan-speeds f22 --every`, figure for figure as CI's artifact:

- 197 kt, left, calm: 203 kt at a quarter turn, 6 over;
- 222 kt, right, calm: 230 kt, 8 over;
- 237 kt, right, calm: 242 kt, just over 5.

Each was flown off as soon as its speed left the 5 kt band, before half a
turn, so its circle reads "inf to 0 m": none was measured.

**Not a regression of the model.** Bisecting her November commits on exactly
these speeds found the oldest, 893f25f5 (2026-11-01), failing them too. What
changed on 2026-11-03 was where the speeds fall: her slowest plan speed rose
from 140 to 146 (027fe8d2, her measured stall) and then to 157 (13cd3361,
her stall with the gear down), and the nightly's 5 kt steps moved from 140,
145, ... - at 893f25f5 every one held from 140 to 245 - onto 146, 151, ...
(196 failed) and then 157, 162, ... (197, 222 and 237 failed). Those speeds
were always unsafe; the nightly only now flew them.

**The cause.** Traced every half second round the 197 kt orbit, her speed
hunted 197.5 to 209 kt and her throttle 0.08 to 0.99 every 7.5 s, from the
start and never settling. The pinned model sends each throttle through a lag
filter of a second (`fcs/throttle-filter`, made an actuator by make_f22.py
for the trim) before the engine, whose own spool (JSBSim's turbine) lags
again; the F-15C's and the F-35B's throttles reach their engines directly.
Two lags in the autothrottle's loop, against thrust near her weight, leave
it a limit cycle at some speeds. With the filter's lag 100 a second, all
nine speeds from 197 to 237 held within a knot.

**Searched, 2026-11-11**: JSBSim's f22 and its throttle filter, FGTurbine's
spool (its rate is set by the bypass ratio), the F119's FADEC and its idle
to military time, and autothrottle limit cycles with engine lag. No F119
spool time or F-22 throttle filter is published that we found; the F119's
descriptions say only that its FADEC tolerates rapid throttle movement, and
an autothrottle patent notes that filters in its path cause overshoot -
which is what the trace shows.

**The fix.** `tools/make_f22.py` removes the filter: each throttle command
reaches its engine's summer as the other fighters' do, and the engine spools
("The throttles reach the engines" in its docstring). Nothing in the
autopilot changes, so no other aircraft moves. Two of her figures, each
measured over seconds from a throttle step, moved with it:

- **Her sustained turn**, 7.35 to 7.87 deg/s (above: open). Traced: at the
  end of the turn both models make 21,000 lb an engine; it is the second's
  head start in the 12 s that moves it.
- **Her rotation speed**, `takeoff-speeds f22`: she lifts off from 140 kt,
  so 145 is written where 150 was. Her climb-away, 225.1 kt at 500 ft, is
  still written as 230.

**Verification.**
- `plan-speeds f22 --every`: 42 of 42 from 157 to 360 kt held. Of the
  other fifteen, flown the same way three at a time, the thirteen that ran
  through hold all of their speeds; the A380's and the C172P's runs were cut
  short by a reconfigure of the build under them (the A380 held every speed
  it reached, to 271 kt). No file of theirs is changed here.
- Three new tests, run on pull requests, each flying one of the speeds that
  failed, both ways round in calm air and a 10 kt wind:
  `the_f22a_holds_{197,222,237}_kt_round_her_tightest_orbit_where_her_throttle_once_hunted`,
  about 10 s each on linux-release. Seen red on main's model: all three
  fail, each in 2 to 6 s.
- `ctest -R f22` on linux-release: her gust, stall, roll-out, landing,
  take-off, glide and orbit tests pass but for her sustained turn (above);
  `figures f22`, 5 of 6 in range.
- `make_f22.py --check` passes. The selftest hash does not move: it flies
  the C172P, whose files are untouched.
