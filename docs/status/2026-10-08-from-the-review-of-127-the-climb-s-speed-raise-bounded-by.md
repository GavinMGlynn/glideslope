### From the review of #127: the climb's speed raise bounded by the fastest, the turn off before the runway's end for the A380, one copy of the metres in a degree, 2026-10-08 — fixes

**What it is not, first.** **The server's own go-around from an occupied
runway is not exercised by any run**: its question
(`runway_clear_of_others`) is a dozen lines over the same `sim::on_runway`
the in-process test asks, and the server run that lands two aircraft on 16R
has the first long gone when the second arrives. Building the occupied case
on a server takes an aircraft held on the runway while another arrives - a
client's aircraft taken back on its roll, timed in real time - which was
not built (a tail). **No model gives a flap or gear speed limit**, so the
raise is bounded by the fastest a plan may ask and by 40 kt, nothing else.

- **The speed the autopilot raises for a climb is bounded** by the
  aircraft's fastest a plan may ask (`Autopilot::limit_speed`, through
  `Controller::limit_speed`, which the server gives every controller from
  `sim::plan_speeds`); with none given it is never raised, so every
  measurement made without one - the plan and glide speeds the figures
  files carry, `sim::orbit_trial`'s - is flown exactly as before. Only the
  plain autopilot raises it: holding, navigating, or round a go-around's
  circuit; the take-off, the approach, the flare, the roll-out, the learnt
  landing and the vacating fly their own laws.
  `the_speed_the_autopilot_raises_for_a_climb_never_passes_the_fastest_she_may_hold`:
  the F-35B round the rising-ground go-around with her fastest set 11 kt
  over her circuit's 179 kt: raised to 11.00 kt and no further, flew at
  most 189.9 kt. **Seen to fail** without the clamp: raised 25.46 kt, flew
  203.7 kt.
- **The plan speeds reproduce**: every aircraft's tightest-orbit tests at
  its slowest and fastest and its one-step-past test pass unchanged in the
  run below; they fly the committed figures.
- **The selftest hash did not move**: `glideslope_cli selftest` prints
  `hash 30ac70b84cab7d7c`, as before; it flies no autopilot.
- **Turned off before the runway's end, the A380 too**: an aeroplane
  rolling on for a free spot turns off 800 m from the runway's end (the
  A380 rolls 560 m in her turn), and with the right side taken there turns
  to the left if that is free. The free-spot window is 800 m either way:
  anything stopped further along than that is at least 240 m along from
  where she stops, so the window is all that need be looked at - and every
  aircraft the server flies is looked at. The in-process two-aircraft test
  flies the A380 as well: the second, forced to turn off at 2,200 m of the
  3,000 m runway with the first parked on the right, turned left, stopped
  2,753 m along and 154 m from the first. **Seen to fail** before the left
  turn: "the second stopped clear of the first: 74.6 m".
- **The metres in a degree in one place**: `sim::metres_per_degree_latitude`
  and `_longitude` (`sim/plan.hpp`), the five copies in the take-off, the
  lander, the learnt landing, the go-around's circuit and the vacating
  removed - the same expression, so nothing flown moves. One run of the
  408 tests matching the autopilot, orbit, plan, landing, hand-over,
  lesson, learnt, take-off, navigator and selftest: 408 of 408, linux-debug.
