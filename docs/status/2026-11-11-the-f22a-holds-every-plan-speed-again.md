### The F-22A holds every speed a plan may ask again, 2026-11-11

**What it does not do, first.**
- **Pull requests fly three of her 42 plan speeds** (197, 222 and 237 KCAS,
  below) besides her two ends; the rest stay with the nightly's
  `plan-speeds --every`.
- **Her throttle lag is her model's, not a source's.** The one-second filter
  the autothrottle now leads by is the pinned JSBSim model's; no F119 spool
  time or F-22 throttle response was found published.

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

**The cause: the autothrottle, against a lagged engine.** Traced every half
second round the 197 kt orbit, her speed hunted 197.5 to 209 kt and her
throttle 0.08 to 0.99 every 7.5 s, from the start and never settling. Her
model sends each throttle through a lag of a second (`fcs/throttle-filter`)
ahead of the engine's own spool; with thrust near her weight, the
autothrottle's proportional and integral law on the speed alone - 0.08 of
the throttle a knot - is a loop that limit-cycles. A real F-22's engines lag
too, so the controller is what is fixed, not the model. (Removing the filter
held the speeds as well, but moved her sustained turn from 7.35 to 7.87
deg/s, out of the report's band, and her rotation speed; it was not kept.)

**Searched, 2026-11-11**: autothrottle design with engine spool lag, speed
loops with acceleration or derivative terms; JSBSim's own autothrottles
(its Global 5000 and C310 autopilots, and the FlightGear and JSBSim
autopilot documentation); JSBSim's f22 and its throttle filter; FGTurbine's
spool; the F119's FADEC and its spool time. Patents for autothrottles and
flight-path guidance compensate spool lag with an acceleration term or a
thrust-error term; a windshear patent (US 3,955,071) adds a proportional
acceleration term for the lead it gives the throttle command and models an
engine as a 1 to 2 s first-order lag. No F119 spool time was found.

**The fix.** The autothrottle's proportional term leads by the aircraft's
throttle lag: it answers the speed off less the speed's trend (its own,
smoothed over a second) times `throttle_lag_s`, a new attribute of the
figures file's `<published_figures>` (checked: over 0 and at most 5 s).
The F-22A's is 1, her filter's own; no other aircraft gives one, and where
it is 0 the term is nothing, so no other aircraft's flight changes. Swept on
the three speeds: 0 fails all three; 0.5, 1, 2 and 4 s hold all three.
`tools/make_f22.py` and her model are main's, unchanged.

**Verification.**
- `plan-speeds --every` on linux-release, three at a time, nothing
  reconfigured under the runs: the F-22A 42 of 42 from 157 to 360 kt; and
  every other aircraft all of its speeds - 737-300 27, 747-400 19, 787-8
  23, A320 29, A380 21, B-2A 28, C172P 11, C182 14, F-15C 41, F-35B 41,
  J-3 Cub 5, Learjet 35A 34, Mosquito 28, PA-28 11, S.23 11.
- Three new tests, run on pull requests, each flying one of the speeds that
  failed, both ways round in calm air and a 10 kt wind:
  `the_f22a_holds_{197,222,237}_kt_round_her_tightest_orbit_where_her_throttle_once_hunted`,
  about 7 s each on linux-release, 67 to 83 s on linux-debug under -j4. Seen
  red with the lead at 0 - main's autothrottle - all three, in 2 to 6 s.
- `ctest -R f22` on linux-release: 44 of 44 (one live-model test skipped,
  no key asked), her sustained turn 7.35 deg/s and rotation 150 kt as on
  main.
- The quick tests on linux-debug - every test under 5 s in
  tests/ci_costs/linux-debug.txt, live ones left out, and the three new
  ones: 666 of 666.
- The selftest hash does not move (`ctest -R selftest`): the C172P gives no
  throttle lag.
