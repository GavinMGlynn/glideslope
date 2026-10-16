### A fly-by cuts its waypoint by no more than 278 m where she can turn that tight, and the OpenAI coast recording made again, 2026-10-13 — item stays done

**What is still missing, first.** Nothing of the item. A fly-by's miss is not
always under the cap. Where even an arc at 22 degrees of bank cannot pass
that near, that arc is flown, the nearest she can with bank to spare. That
covers every 120- and 150-degree turn here, and the jets' 90-degree turns:
the worst is 16.7 km, a jet turning 150 degrees. No platform code changed,
so this was not built on Windows.

**Why.** With fly-by turns, the copilot's coast flight passed BRONTE
1,275 m off, and the live re-record failed the coast check: within 1 km in
83% of its looks, against 85%. A plan that traces a coast or a landmark asks
for its waypoints, not corners cut past them.

**The cap** (`most_fly_by_miss_m`, sim/turn_laws.hpp): 278 m. That is half of
RNP 0.3's 556 m, and about a quarter of the kilometre the coast is held to.
- Where a fly-by's arc would pass its waypoint further off than that, the arc
  is tightened to r = 278 / (1/cos(A/2) - 1), and the lead shortened with it.
- The arc is never tighter than its radius at 22 degrees
  (`fly_by_cap_bank_deg`), at her speed through the air plus the wind's.
- **A waypoint is never flown over because of the cap.** Flown over, as first
  proposed, 92 of the 112 turns below swung 0.37 to 2.1 turn radii past the
  next leg.
- **Why 22 and not 25.** Tightened to 25, the F-15C at 200 kt and the Mosquito
  at 148 turning 120 degrees strayed 0.108 and 0.105 r, against the tenth.
- **Why the wind is counted.** Worked at the turn's start across a 20 kt wind,
  the 172P turning onto it from behind strayed 0.142 r.
- `sim::fly_by` takes the wind as a fifth argument, from the navigator's own
  estimate of it.

**Test**
`a_turn_of_45_90_120_or_150_degrees_at_a_waypoint_passes_it_within_the_fly_by_cap_in_an_aircraft_of_every_class`.
- One aircraft of each of the 7 classes, at climb and at cruise speed,
  turning 45, 90, 120 and 150 degrees each way: 112 turns, coverage asserted,
  none flown over.
- Each passes its waypoint within the larger of 278 m and the miss of her
  22-degree arc. On top of that it allows 25 m for her flight off the arc and
  1% of the arc for ground speed against true airspeed.
- Worst pass: 0.99 of what it is allowed. The 172P at 75 kt turning 90
  degrees passed 282 m off; the F-15C at 300 kt turning 120 degrees passed
  6,936 m off against an arc of 6,909.
- Within 278 m: 12 of the 28 turns of 45 degrees and 1 of the 28 of 90.
- Worst stray off the next leg: 0.81 of its bound, the F-15C at 150 degrees,
  0.121 r against 0.15. To 120 degrees the worst is 0.062 r.
- **Seen to fail** with the cap off: 60 of the 112 turns, for example the
  B-2A at 230 kt passing 2,548 m off at 90 degrees.

**The other stray tests still hold.** All four 90-degree groups, the 45/120/150
angles test, the 20 kt crosswind test, short legs, fly-over, the bank hold,
trim decay, the 90-degree heading turn and the climb capture: 15 of 15 pass.

**OLYMPIC is now flown over.** The harbour tour's last landmark, a turn of
more than 120 degrees, was passed 1,277 m off as a fly-by, and 1,213 m even
with the cap. The client's own tour test
(`the_clients_ai_flies_a_flight_plan_past_every_waypoint_on_vulkan`, run
here) holds every waypoint within 100 m, and it failed at 1,944 m off. Now the
plan marks all three landmarks `flyover`, and OLYMPIC is passed 0.3 m off.

**The OpenAI coast recording was made again**, once, live, with the owner's
key read at run time. The model's route had 8 waypoints. The flight came to
Cronulla with the coast within 1 km in 32 of 34 looks (94%), never more than
1,500 m from it. Waypoints were passed 4 to 462 m off, except COOGEE, the
first, 854 m off as she turned onto the route from her start. The recording
was scanned for `sk-`, `bearer`, `authorization` and `api-key`, and for each
key file's contents: none found.

**The selftest hash does not move**: `hash 182dd6c996e0ee4c`.

**Verified**, linux-release, display unset: the 15 tests above;
`the_navigator_flies_a_plan_past_every_waypoint_in_calm_air_and_in_wind`
and the client's vulkan tour test pass; and every `as_recorded` test, run
one at a time: 16 of 16 passed.
