### A session's copilot and planner told the runway is wet from its METAR; a landing handed over in a skip is landed, 2026-10-10 — two items done

**What is still not done, first.** Contaminated runways (codes 4 to 1) are
flown when given and reported by nothing, and the F-35B has no published
landing distance to size her wet runway by: both are Later items (the
latter is the item for the five without a landing distance). The wet
refusal test is a unit test of what the server and the clients share, from
a session's weather report through the brief to the refusal; no
multi-process session was run with a raining METAR and a model's landing.

**The runway's condition, wired** (closes the entry below's "every caller
still passes dry"):
- `frontend::runway_wet(report)`: the weather report's METAR wets the
  runway (world::runway_condition_of); none, still air, is dry.
- `copilot::Brief` carries both needs, `landing_need_m` and
  `wet_landing_need_m` (1.15 times, made by `frontend::brief_for`, since the
  copilot sees no simulation header); `copilot::Situation::runway_wet` says
  which applies, so a weather that changes during a flight changes it.
  `plan_request_for(data, id, wet)` has no default: every caller says.
- The server: its route check for a player's copilot (`fly_route_on`) and
  every situation it asks its own copilot about (`situation_of`) are told
  from the weather it flies; its planners, asked before its clock starts,
  from the `--metar` given or the `--weather` station's report, now fetched
  before the fleet is built (and flown from the start as before).
- The client tells its copilot from its flight's weather report; the CLI's
  headless client from the last METAR the server told it (one that cannot be
  read leaves it as it was). The CLI's `plan` and `fly-copilot` fly no
  weather: dry.

**A landing handed over in a skip** (`Controller::to_ai`): the roll's
lander is given her when her wheels bear weight, or bore it within the last
second (`skip_s`) and she is no more than three feet (`skip_ft`) above her
height over the ground then - recorded by `fly()` every step. Before, the
hand-over asked only `gear/wow` that step, and a 787 handed over in a
tenth-of-a-second skip on a wet runway ran 22.7 km on the plain autopilot.

**`sim::wheel_braking_coefficient`'s refusal** reads "no wheel braking
coefficient for runway condition code N" (an `std::string` built
explicitly; clangd's -Wstring-plus-int flag on it was a false reading of
`"..." + std::to_string(code)`, which is `operator+(const char*,
std::string)`); the coefficient test now checks the text for each code
refused. The file's unused `<cmath>` is gone.

**Verification.**
- `a_session_whose_metar_reports_rain_refuses_a_runway_long_enough_dry_but_short_wet`:
  a session's report, "-RA" at YSBK, read by `frontend::runway_wet` into the
  copilot's Situation and the planner's request, against the server's brief
  for the C172P (558 m dry, 642 wet), on a 600 m runway: both refuse it as
  "the runway YSBK 29C has 600 m to land on, and the aircraft needs 642 m on
  a wet runway: choose a longer one"; with no rain, both take it. **Seen to
  fail** with `runway_wet` answering dry: "'' and ''"; reverted.
- `every_landplane_landed_by_hand_and_handed_over_in_a_skip_is_landed_to_a_stop`:
  every landplane taught the approach (13; the flying boat named), landed by
  hand, the runway let down half a metre two steps before the take-back so
  that no wheel bears weight as she is handed over (asserted for each): all
  given the landing and stopped on the runway, 420 m (the Cub) to 1,990 m
  (the F-35B) past the threshold, within 7.0 m of the centreline, rising
  none after the take-back. **Seen to fail** without the fix: all 13 given
  the plain autopilot - the 737-300 ran 20.9 km, the 787-8 22.7 km, the
  A380 and F-15C wrecked.
- Run again, all passing (linux-release, 157 tests, the 7 that ask a live
  model skipped, none failed): every test whose name speaks of the roll, a
  take-back, a landing by hand, a short runway, the lander, a landing, a
  runway, a stop, a skip, the weather, a METAR, the copilot or the planner -
  among them the wet and dry short-runway take-overs, the multi-process
  copilot sessions played back and the server's weather tests.
- **The selftest hash does not move**, `182dd6c996e0ee4c`.
