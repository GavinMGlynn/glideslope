### A model's flight plan from the ground may end in a landing, 2026-10-09 — item done

**What it is not, first.** **No runway is refused for being too short**,
for the planner or the copilot: neither ever held a landing's length to
anything (a new tail). **The copilot itself has still not been seen to
answer with a landing**: the live landing below is the planner's, asked
from the ground, not the copilot's in the air, so that item stays open.
The flown test's own failure was not watched with the server's runway
check broken; the unit test's was (below).

**What changed.**
- **The planner is told `land`** (`copilot/planner.cpp`), as the copilot
  is: the command's line, and a rule - last, only when the pilot asks for a
  landing, copying a runway line with `land` for `runway`, no orbit for ever
  before it. The request lists "Runways it may land on, nearest first" - the
  airport and its runway line, those with an elevation - or, for an aircraft
  with no approach speed, says it does not land.
- **Where it may land** (`PlanRequest::fields`) is filled by
  `frontend::landing_fields`: whole airports within 40 km of the airport's
  first end with an elevation, nearest first, each with all its ends that
  have one - at most 4 airports and 24 ends, an airport that would pass the
  ends' cap passed over whole and the next tried. From YSSY that is Sydney's
  six ends, Bankstown's six and Hoxton Park's two. (First chosen as the 12
  nearest ends, which Sydney and Bankstown filled alone, crowding every
  other airport out; changed on review.) `glideslope_cli plan` and the
  server's `--ai-planner` fill it the same, so a recording made by one plays
  back in the other.
- **The same checks as a copilot's landing**: `copilot::landing_refusal`,
  taken out of `change_refusal` and called by both - an approach speed, no
  orbit for ever before it, and a runway it was told of (`landing_field`,
  now also over a list of ends: threshold within 100 m, heading within 5
  degrees). **`land` is the last line of any plan**: the plan reader itself
  (`sim::parse_flight_plan`) refuses a line after it, so a plan file, a
  planner's plan and a copilot's route are all held to it.
- **The server lands a planned landing on its own ground's runway**, as a
  copilot's route's (`add_planned`): the collision ground's end there
  (`ends_near`, `landing_field`) - its threshold, heading, length and the
  ground's elevation - or the plan is refused, said, and the aircraft flies
  the plan file. The AI flies it as any plan ending in `land`: the final
  approach, the learnt landing at its gate. The model only writes the plan.
- **`tasks/sydney-to-bankstown.task`**: "take off, fly to Bankstown airport
  and land there", for a C172P at YSSY.

**Verification.**
`the_runways_a_plan_may_land_on_are_the_nearest_airports_whole_and_no_more_than_the_caps`
(unit, synthetic airports on a meridian, no downloads): with two six-end
airports near, the third and fourth are offered whole, a fifth is past the
airports' cap, one 60 km off past the radius and one with no elevation not
offered; a 20-end airport that would pass the 24 ends is passed over whole;
the caps asserted. **Seen to fail** on the nearest-ends selection: "HOME (6)
BIG (6) THIRD (2) FOURTH (2) - not ... FIFTH (2)". The plan reader's test
(`a_landing_is_read_from_a_plan_and_refused_where_it_is_wrong`) has a ninth
case, a waypoint after `land`; **seen to fail**, with the planner's
`land`-last refusal, with the reader's check taken out; reverted.
`a_plan_from_the_ground_may_end_in_a_landing_on_a_runway_it_was_told_of_and_on_no_other`
(unit, a stand-in model): the instructions carry `land` and the request the
runways to land on; a plan ending on Bankstown 29C is taken first time;
seven refusals, each built and each told back to the model - `land` not
last, a threshold 2.6 km off, the other end's heading, an orbit for ever
before it, no approach speed, no runway told, a runway with no elevation;
the count asserted. **Seen to fail** with the planner's landing check
returning nothing: the first refusal's plan "is refused ... it was taken";
reverted.
`a_flight_planned_from_the_ground_to_a_landing_is_landed_by_the_ai_as_recorded`:
the server plans its one AI C172P from the committed recording
(`tests/data/copilot/bankstown-anthropic.jsonl`) and flies 150,000 steps; the
plan ends in `land 29C`, it is handed to the learnt landing at its gate on
final to YSBK 29C and touches down under 300 ft/min within 5 m of the
centreline, stopped on it, unwrecked (133 s, linux-debug).
`..._asking_anthropic_now` (live, `GLIDESLOPE_LIVE_MODEL=1`, skipped
without it or a key) asks Claude Haiku 4.5 by `glideslope_cli plan --record`
and flies what it recorded the same way. **Run live 2026-10-09, three times** (the third
after the airports' selection changed the runways listed, which no
recording then played back to, so all five planner recordings were made
again): each time Claude answered in one answer, taking off from 16R (`takeoff
1000`), one waypoint BANKSTOWN, and `land 29C -33.926800 150.996002 26 304
1415`; the first flight touched down at 155 ft/min, +2.04 m across, stopped
440 m along and -3.38 m across.
**Every planner recording re-recorded** - its instructions changed, and a
recording plays back only the words it recorded: the four `cbd-orbit`
recordings, each by its `..._now_and_flown` test with the owner's keys,
scanned for a key (`sk-`, `x-api-key`, `Bearer`, each key's first
characters): none. **The first re-recording found a fault in the words**:
told only that it may land, GPT-5.4 mini, asked to orbit the CBD, ended its
plan in a landing on 16L after one turn, and in the server's two-planner
test came down through Claude's orbit, which the separation monitor held
off its height until 1.84 turns (`each_ai_aircraft_is_planned_...` red).
The rule now says `land` only when the pilot asks for a landing; recorded
again, none of the four lands. **Restacked on the take-off flap's branch**
(whose 747-400 climbs at 170 kt measured, not 190, which changes what she
is told): of the five planner recordings only the 747-400's no longer
played back, and it alone was recorded again, live (no key in it); all
five then play back, and the 747's take-off, orbit and figures tests and
the walk of every aircraft through its take-off flap are green on the
combined tree. Green on the new recordings: the four
`..._as_recorded_and_flown`, `each_ai_aircraft_is_planned_...`,
`every_ai_aircraft_a_server_runs_planned_or_not_is_kept_500_ft_...`,
`two_planned_aircraft_wrecked_together_...`,
`an_ai_aircraft_whose_runways_cannot_be_read_...`, and the copilot's
landing, plan and speeds unit tests.
