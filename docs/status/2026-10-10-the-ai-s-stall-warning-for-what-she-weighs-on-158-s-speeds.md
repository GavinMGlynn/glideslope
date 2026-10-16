### The AI's stall warning for what she weighs, on #158's speeds for weight, 2026-10-10 — item still open

Rebased onto #158 (`approach-speed-for-weight`, 058e957e), whose
`for_weight` scales an aeroplane's approach speeds by the square root of
her weight over the loading they were measured at. **The AI's stall notice
now takes its stall and its recovery speed from them at what she weighs
now** (`Controller::notice_a_stall`): it read the figures' stall unscaled,
so a B-2A at her maximum loading, 336,500 lb against the light 177,160 her
95.4 kt stall was measured at, was warned at 100.4 kt - she stalls at
131.5. New test,
`the_ai_pilot_notices_a_heavy_b2a_stall_coming_at_the_warning_for_her_weight`:
noticed at 138.0 kt against her weight's warning of 138.1. **Seen to fail**
with the unscaled stall: noticed only at 19.9 kt, out of the sky. A lighter
aeroplane than her figures' (the PA-28's reverse case) is warned the
earlier for it, not later: the same scaling, downward.

**Verified on** this branch rebased onto #158 (058e957e), linux-release,
every target: the 291 tests matching stall, recover, upset, lesson,
instructor, take, glide, orbit, rolled, autopilot and trim. 289 passed in
the run; `a_players_copilot_glides_its_aircraft_on_a_server_when_the_engine_stops_as_recorded`
and `a_players_copilot_stands_by_when_its_pilot_takes_the_aircraft_back_as_recorded`
failed in 2.3 s each under ctest -j4 and passed three times of three run
alone - multi-process tests, on a machine other agents' tests were using;
watch them in CI. The selftest hash does not move, `182dd6c996e0ee4c`.
