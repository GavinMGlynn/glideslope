### The Learjet's tail strikes and a take-off lesson says so; every aeroplane off 16R flies its plan to its orbit, 2026-10-06 — one tail done, two still open

**What is missing first.** The Learjet still cannot be rotated early (no
source for her centre of gravity's height; below). Of "every nose-wheel
aeroplane's tail strikes where its airframe would", the Learjet is done and
four things are not: the F-15C has no contact behind her main wheels; the
A380's only ones are her outboard engines, which she strikes at 34 degrees,
not her tail; the F-35B's tail is at 22.2 degrees and, held fully back with
flaps up, she flies off before it; and no aeroplane's strike attitude has
been checked against a published figure. Both stay `[ ]`.

**The Learjet's tail.** `tools/make_learjet35a.py` gives her a structural
contact, `VENTRAL_FIN`, at FS 596.2, WL 21.6 on the model's frame: the lower
aft edge of her ventral fin, measured off the maintenance manual's side view
(MM 6-00-00, Aircraft Dimensions, figure 1 sheet 2 - already pinned by
SHA-256 in `docs/ASSETS.md`), scaled at 200 dpi by its own 48 ft 7 in length
and 20 ft 2 in wheelbase, which agree to 0.3% (2.187 and 2.180 px/in). Of the
lower outline behind the main wheels, the point the least angle from the
main wheel's contact reaches is on the fin, 199.4 in aft and 47.3 in up -
13.4 degrees at rest as drawn, the fin's whole lower edge within half a
degree; the drawing's 12 ft 3 in height reads 3% short on the same scale, so
the height is good to about 1.5 in. On the model's struts, extended as she
rotates, `Aircraft::stance` puts her strike at 14.0 degrees. Her take-off
now holds her nose short of it, and her landing flares short of it, like
every other nose-wheel aeroplane's; the flare and take-off tests that cover
her pass unchanged.

**The lesson says so.** `lesson/airframe-down` is the lesson runner's third
property of its own: 1 while anything not a wheel touches the ground. The
take-off lessons of every landplane class (airliner, business jet, bomber,
fighter, light aircraft, Second World War) hold it at nought on the roll and
in the initial climb: "Keep the airframe off the runway: something other
than the wheels struck it" - the airframe, not the tail, for a wing tip, a
tip tank or an engine pod is struck the same way. Before, a tail dragged on the runway went unjudged:
**the 737-300's early rotation in the one-fault test had been dragging her
tail** at 12.9 degrees, leaving at 143 knots; her debrief now names the
strike. Held two degrees short of the strike instead she left at 161,
later than by the book - at that attitude she cannot leave early without
striking - so the strike is accepted as how her early rotation is caught
(`a_take_off_flown_with_one_fault_has_that_fault_in_its_debrief`, its
comment says why).

**Tests.**
- `an_over_rotated_take_off_strikes_the_tail_and_its_debrief_says_so`
  (new): every aeroplane with a tail to strike, flown by the take-off
  autopilot until it begins its rotation, then the stick held fully back
  with the flaps up until something not a wheel touches or she is fifteen
  feet up. Each must strike within a degree of the attitude its contacts
  give, and its debrief name the strike. Nine strike - 737-300 12.6 (12.8 by
  its contacts), 787-8 10.5 (10.5), A320 14.4 (14.3), A380 33.3 (34.0, her
  engines), B-2A 14.1 (14.3), C172P 10.1 (10.3), C182 10.5 (11.3), Learjet
  35A 14.7 (14.0), PA-28 13.8 (14.2); seven named: the three with no tail to
  strike, the 747-400 and F-22A with no take-off, the Mosquito on her tail
  wheel, and the F-35B. Flaps down, the Learjet and the A380 flew off before
  striking. **Seen to fail** twice: with the Learjet's fin taken out of the
  model the test is in, "learjet35a was held fully back and never struck
  its tail"; with `lesson/airframe-down` reading nought, all nine "debrief
  does not name the strike".
- The approach test's strike check is now put to thirteen aeroplanes, not
  twelve; the Learjet is no longer named as having no tail.
- The 16R test (was `every_landplane_rolls_down_sydneys_16r_on_the_dem_and_every_one_that_can_climb_away_takes_off`)
  now flies each of the thirteen that take off on through the
  sydney-cbd-orbit plan from the hand-over, made its own: its speeds
  brought within its `<plan_speeds>` and the orbit widened to
  `least_orbit_radius_m` at that speed (no function of the product makes a
  plan another aircraft's), then passed by `refuse_what_it_cannot_fly` -
  until it is on the orbit's leg within a tenth of its radius of the
  circle. All thirteen get there with nothing wrecked, 218 s (F-35B) to
  602 s (J-3 Cub) in; the Learjet at 140 knots, 303 s, the Mosquito at 123,
  332 s. **Split in five** (from the review), as it took 333 s in
  linux-debug against 43: `the_airliners_...`, `the_military_jets_...`,
  `the_learjet_and_the_mosquito_...`, `the_cessnas_...` and
  `the_cub_and_the_cherokee_...`, each checking that the five groups and
  the flying boat are the catalogue, each aircraft once. 95 to 149 s each
  in linux-debug, four at once. **`tests/ci_costs/` is estimated, not
  measured** (no green CI run yet): the debug presets take those local
  times, and the release presets a third of them, as the old test's 60 and
  20 stood; the over-rotation test 40 and 13.

**The Botany Bay tail's cause.** The plan was written for the C172P: CLIMB
at 80 knots and the orbit at 90. The 2026-10-01 run flew it as written in
every aircraft, before plans were held to their aircraft's speeds
(2026-10-02, "A plan never asks an aircraft for a speed it cannot hold
clean"); 80 knots is below both the Learjet's stall and the Mosquito's.
**Seen to fail**: with the speeds left as written and the refusal skipped,
both are wrecked, "came down on water", as before. `glideslope_cli
fly-plan` now refuses the plan as written for either.

**The Learjet's early rotation, looked at again.** The maintenance manual's
side view puts her nacelles' centreline about 63 in above the ground, near
WL 35 - higher than the model's estimated WL 22. Raised there, the thrust's
nose-down moment about the main wheels grows by about 7,400 ft-lb: the wrong
way, so it is not changed here. The one estimate that would help, the
centre of gravity's height (WL 0, the bottom of the fuselage), has no
source: nothing in the AFM, the maintenance manual's chapters 6, 8 and 27,
or a search for a minimum unstick speed gives it. Left as it was, and the
tail stays open.

**Unchanged**: the selftest's hash, 30ac70b84cab7d7c. The committed Learjet
is what its script writes. Every other test run passes: the take-off,
rotation, every-loading, approach, flare, circuit and instructor tests, the
Learjet's figures and the committed-asset checks.
