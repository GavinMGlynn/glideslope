### An autopilot that takes off, 2026-09-21 — groundwork, no item of its own

**All four light aircraft take themselves off**, hold the centreline within
1.4 m on the ground, and climb away at their own best climb speed. This has
no item in the plan: Phase 5c's lessons need it and nothing else does, so it
is recorded here as what was built towards them.

| Aircraft | rotates at | airborne in | its handbook's roll | 500 ft in | worst off the centreline, on the ground |
| --- | --- | --- | --- | --- | --- |
| Cessna 172P | 55.6 kt | 280 m | 271 m | 56 s | 0.35 m |
| Cessna 182S | 57.5 kt | 260 m | 242 m | 48 s | 0.43 m |
| Piper PA-28 | 47.8 kt | 251 m | 219 m | 57 s | 1.35 m |
| Piper J-3 Cub | 37.9 kt | 129 m | none published | 47 s | 0.10 m |

**The speeds are the aeroplane's own.** The best climb speed is the speed its
published climb rate was measured at, which all four publish. The lift-off
speed is the speed its published take-off roll was measured at - three of the
four - and where there is none, a seventh above its published stall, which is
the usual relation; `DepartureSpeeds::rotate_is_published` says which it was,
and the Cub is the one that is worked out. An aircraft publishing no climb
speed at all is refused rather than given a guess.

**The test holds each to its handbook's ground roll**, within a third either
way, rather than to a number chosen to fit: 280 m against 271, 260 against
242, 251 against 219.

**A sign was wrong, in two places.** `sim/test_pilot.cpp` says it plainly -
"the model's rudder command yaws the nose left for positive values" - and
both the take-off roll and the landing rollout had it the other way, so the
rudder drove the swing instead of correcting it. On take-off it was obvious:
the Cessna left the centreline at once, wandered 182 m off, and turned right
round - full right rudder and right brake while the nose went from 070 to
359. **On the landing rollout it was not obvious at all**: the approach and
landing item passed its verification with the fault in it, because the
touchdown figures are set before the rollout begins and the aeroplane still
stopped on the runway. The touchdown figures are unchanged by the fix; the
stopping distances moved by about fifty metres.

**Verification run.** `every_light_aircraft_takes_itself_off_and_climbs_away`
and `the_take_off_speeds_come_from_each_aircrafts_published_figures`, both
registered, and both stating that the light aircraft are four and that three
of them publish a lift-off speed. 299 of 299 tests pass locally at `-j4`, in
1052 s.
