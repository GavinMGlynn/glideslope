### The owner's decisions of 2026-10-09 on items blocked on sources, and a flaky test listed - docs only, three items closed, three moved to Later

**What these closures are not, first.** None of the three ticked items meets
its verification as first written: each is closed on today's measured, cited
figure, by the owner's decision of 2026-10-09, with its shortfall written down
here and in the plan. No model, test or code changed. Wet-runway braking ("a
landing taken over on its roll is braked for a dry runway") stays open: it is
not a missing source but a missing model.

**The decision.** Items blocked only on a public source close on the figure
measured today, with the shortfall recorded; items with no public source at all
move to Later.

- **The F-15C's stall, held to her flight manual's speed - closed.** On NASA
  TN D-8052's 3/8-scale lift (#141) she settles at 110.1 KCAS held at full aft
  stick from 200 knots at 10,000 ft, clean, idle, against T.O. 1F-15A-1's 100.
  **Shortfall: 10 knots**; 100 would need 1.79 for her resultant coefficient
  against the 1.48 she makes, 21 per cent more lift and drag, and TN D-8052's
  data stop at 40 degrees. The owner accepted the measured 110.1.
- **The Learjet cannot be rotated early - closed.** Held fully back from 85
  per cent of her rotation speed she lifts her nose wheel at 113 knots and
  leaves at 124.1 against Vr 125.3 (#140): before it, but inside the five
  knots her take-off lesson allows, so the lesson does not call it early.
  **Shortfall: the lesson's call**; what would move it - her stabilizer's
  rigging neutral on the flight model's scale (3.6 degrees, estimated), or
  her elevator's power - has no source.
- **A landing on a runway too short is refused - closed for the ten that
  publish a landing distance** (#147): the C172P (1,280 ft over 50 ft, 558 m
  needed), C182S (1,350 ft, 589 m), Cherokee 180 (1,150 ft, 502 m), each times
  1.43 (EASA CAT.POL.A.330(a)); the 737-300 (4,600 ft), 747-400 (6,200 ft),
  787-8 (5,400 ft), A320 (4,400 ft) and A380 (6,350 ft) at their airport
  planning documents' field lengths; the Learjet 35A (about 3,050 ft, 1,553 m)
  and F-15C (about 4,200 ft, 2,138 m) times 1.67 (14 CFR 121.195(b)). Moved to
  Later: the J-3 Cub, Mosquito, F-22A, F-35B and B-2A, which have no landing
  distance from a primary source; the S.23 lands on water, so a runway's
  length does not apply to her. Also moved, as it is not a correctness bug
  (the check refuses a short runway whatever the model was shown): the runway
  lines a model is shown do not say their landing length or the aircraft's
  need.
- **Not every nose-wheel aeroplane can strike its tail - to Later.** The
  Learjet's part is done (2026-10-06, her ventral fin at 14.0 degrees, her
  drawings allowing 14.0 to 15.6); the F-15C, A380 and F-35B remain, and no
  strike attitude is published for any of them.
- **The F-35B lands on her power - to Later**: no public approach angle of
  attack; her 19.5 degrees is her model's.

**Flaky tests** (owner, 2026-10-09: they belong to "A month of clean nightly
runs", Phase 14b). Added to its list:
`prediction_interpolation_and_the_player_limit_hold_at_100_ms_with_jitter_and_loss`
read an interpolated control of 0.0051 against its 0.005 bound in CI run
37863482425, attempt 1. Not added, because #148 addressed them: the window
client's 5.683 m step at a switch (run 37866144089) - the clock it draws at now
slews rather than steps - and the copilot take-back's "no route came after the
take-back" (the same run, windows-debug) - the take-back is now taken on the
event, while the look is out.

**The plan's count.** Open items in the phases, by
`sed -n '/^## Phase /,/^## Later/p' docs/COMPLETION_PLAN.md | grep -c '^- \[ \]'`:
31 before, 26 after.
