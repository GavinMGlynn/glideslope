### Why ten aeroplanes have only a turns lesson, 2026-09-22

**Looked into rather than assumed.** Four of the six exercises name a speed
the aeroplane publishes, and ten of the sixteen publish neither a stall speed
nor a rate of climb. What they do publish was checked one by one:

- the **airliners** publish `climb_at_altitude` and `climb_gradient_one_engine`
  - a ceiling check at Mach 0.78 and an engine-out gradient. Neither is a
  climbing speed and neither is a stall;
- the **B-2** publishes a maximum climb rate and a cruise range;
- the **Short S.23** publishes a water take-off and nothing else;
- the **F-15C** publishes a maximum climb rate, a level Mach, a service
  ceiling and a sustained turn rate.

**One of the ten turns out to have a way in.** The F-15's own flight manual,
already recorded in `ASSETS.md` for its checklist, publishes stall speeds - in
charts, and its worked sample gives 107 knots flaps and gear down at 40,000
lb, 15 degrees of bank, 10,000 feet and maximum thrust. That is not the
power-off wings-level figure this project's `stall_speed` entries hold, so it
cannot be dropped in: it needs either a figure that carries bank and thrust as
conditions, or the 1g number read off the chart rather than off its sample.
Recorded as a tail with the citation rather than half-built.

**The others are genuinely blocked on figures that are not published**, which
is a different thing from a lesson nobody has written. `sim::approach_speeds`
and `sim::departure_speeds` throw rather than guess, which is right: a
reference speed invented is a reference speed that means nothing.
