### Who is flying, and the controls, on screen, 2026-09-25 — item done

**What is missing first.** On a server, the HUD shows this client's own
aircraft, which is always its pilot's there: the client with the window does
not hand over online yet (a tail). Watching another aircraft's controls is
the next item, "Ride along in any AI aircraft". The panel is lines of text in
the HUD's font, not a drawn stick and gauges.

**What works.** The HUD always says who has the aircraft: `FLYING PILOT`, or
`FLYING AI HOLD` or `FLYING AI NAV` and the waypoint. This replaces the `AP`
line, which appeared only while the AI flew. Below it are the controls as
the flight model has them:
- `STICK`: the aileron (right positive) and elevator (back positive);
- `RUDDER`;
- `THROTTLE` and `FLAPS`, each from nought to one;
- `GEAR DOWN` or `UP`, only where the gear retracts.

`--trace` prints them too.

**Verified.**
- **Three shots, each read back out of the frame by `glideslope_hud_check`
  and held to the trace to the hundredth**: the pilot flying
  (`the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_vulkan`), the
  AI holding (`the_hud_says_the_ai_holds_the_aircraft_and_shows_its_controls_on_vulkan`)
  and the AI flying its plan
  (`the_hud_says_the_ai_flies_its_plan_and_shows_its_controls_on_vulkan`). A
  jet's shot adds the gear line.
- **`the_hud_says_who_is_flying_and_where_every_control_is_in_every_case`**
  covers what a shot cannot reach: flaps part and all the way down, the gear
  up, down and fixed, and each way of saying who is flying, word for word.
  Four cases, and their count asserted.

**Seen to fail.** With the elevator's sign turned round, the AI-holding shot
went red (+0.18 shown against -0.18). With the gear always down, the
every-case test went red.

**Found on the way.** The three HUD tests share a script, and so shared a
Cesium cache file, which they locked against each other. The cache's name
now carries the case.

Ticked in `COMPLETION_PLAN.md` once PR #12, which added the item, had merged.

**Found while riding along**, fixed before the merge. The gear line asked
whether the flight model had a gear command, and JSBSim has one for fixed
gear too. So the Cessna's HUD said `GEAR DOWN`, and its shot passed, because
the trace asked the same wrong question. Now `Aircraft::gear_retracts` asks the
model's gear units. `an_aircraft_says_its_gear_retracts_only_where_its_model_has_it`
holds all sixteen aircraft against their own files: eleven retract, and five do
not - the four light aircraft and the flying boat.
With the old question put back, it goes red at the Cessna.
