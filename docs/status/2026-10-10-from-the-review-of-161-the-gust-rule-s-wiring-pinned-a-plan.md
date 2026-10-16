### From the review of #161: the gust rule's wiring pinned, a plan's slowest flown for her weight, and a bug found doing it, 2026-10-10 — item stays done

Rebased onto main after #160 merged (`git rebase --onto origin/main
4ff2742c`; no conflicts).

**A bug the new test found**: the server's AI take-off read the session's
gust factor when its plan was given (`Fleet::fly_plan`) - before the
weather is flown in at start-up - so it always climbed out as in calm
air. Now the take-off reads it as it flies (`Departure::climb_kts`: its
initial climb speed, `in_gusts` of the aircraft's own
`Aircraft::gust_factor_kt`, capped at the plan's fastest the controller
gives it), so weather flown in later, or changing during the climb, is the
air it climbs out in. The server says the speed at the hand-over
("climbed out at 82.9 kt (75.4 in still air)").

**Changed**:
- `sim::in_gusts(DepartureSpeeds, ...)` is capped at the plan's fastest,
  as the held-speed rule is, and never lowers a speed.
- **A plan's slowest is flown for what she weighs** (`Controller::
  plans_within`, which the server gives each aircraft with her plan
  speeds): a plan's or a route's speed under her slowest is flown at the
  slowest scaled by `sim::for_weight(PlanSpeeds, ...)` - no longer called
  only from tests. At her model's weight, which a server flies, it is the
  file's slowest and nothing changes.
- `Autopilot::early_holds()` counts the floor's early take-overs.

**Tests** (each production line broken in turn, the test watched fail,
restored - in brackets, what failed):
- `the_controller_holds_a_plans_speed_half_the_gust_factor_faster_in_turbulence_up_to_her_fastest`
  - a Controller, its navigator and a ReportedWeather: 80 kt asked is
  80.0 in calm air and with no weather, 87.5 in moderate turbulence, and
  108 asked is capped at 110. (The controller's raise removed: this.
  `conditions_at`'s gust factor removed: this, the blend test and the
  server's.)
- `the_controller_holds_a_plans_slowest_for_what_she_weighs` - the Cub
  asked 43 kt: 43.0 at 752 lb, 54.77 at 1,220, 60 asked held at 60.
  (The weight scaling removed: this.)
- `an_aircraft_has_its_airs_gust_factor_after_a_step_blended_as_its_weather_is`
  - calm to moderate over a 60 s blend, checked every second for 70 s
  against 15 times the blend's share. (`ReportedWeather::at`'s mix
  removed: this.)
- `the_servers_ai_climbs_out_half_the_gust_factor_faster_in_gusts` and
  `..._in_steady` - the server, the C172P's recorded plan, `--metar`
  16010G25KT: 82.9; 16010KT: 75.4. (The take-off's raise removed: the
  gusts case. It failed first on the code as it was: the bug above.)
- `a_light_aeroplane_slowed_level_below_her_climb_speed_settles_on_it_and_the_floor_takes_over_early_at_most_once`
  - the C172P 100 to 60 kt level (floor 75.4), one early hold, 59.9 to
  60.3 kt from a minute on; the Cub 60 to 43, one, 43.0. **Not seen to
  fail**: with the early hold's deceleration gate removed it still
  passed - the gate is pinned instead by the near-ceiling turn tests,
  which failed without it (the entry below).
- `a_turbulence_severity_and_the_least_gust_spread_that_implies_it_round_trip`
  (test_air_motion.cpp): severities 0 to 7 and spreads 0 to 40 kt in
  tenths, 8 and 401, counted.
- The gust-rule test's climb out is capped at a fastest and never lowered.
- **A regression of this branch's first commit, found by the full run and
  CI**: `sim::plan_gust_allowance_kts` was a constant in sim/plan.hpp,
  which the copilot includes, and so a symbol in its object -
  `the_copilot_can_see_include_and_call_nothing_that_drives_the_aircraft`
  failed (CI run 37995973171, both Linux jobs, and here). Moved to
  sim/orbit_trial.hpp, which the copilot does not include; passing.
- **The full linux-release ctest** (every test, no filter): 1,179 tests,
  1,154 passed, 24 skipped, 1 failed - the copilot walls test above, run
  before its fix; after it, it and the plan, notice and wiring tests pass
  (31 run again). The 24 skipped: the 15 that ask a live model with no
  key in the environment, and 9 that need a Windows host, a display or
  a GPU. The selftest hash does not move, `182dd6c996e0ee4c`.

**Why the B-2A's and Mosquito's glide speeds under their new plan floors
are Later, not a regression**: their `<glide_speeds>` (159 and 128 kt)
were measured from the slowest a route could fly them, and a brief already
routed them no slower than their approach speeds for their models' weights
(169 and 129) before this branch - `copilot::glide_speeds` takes the higher
of the two, so no glide is flown, offered or tested any slower than it was.
Only the figures in their files are stale, and measuring them again with
`glideslope_cli glide-speeds` renames six glide-band tests; that is the
Later item "The B-2A's and Mosquito's slowest glides are under their plan's
slowest" (COMPLETION_PLAN.md). The other tail of this work, also Later:
"A jet, the Mosquito or the S.23 slowed from cruise into her climb sinks 2
to 6.4 kt past her climb speed".
