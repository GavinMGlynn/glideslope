### The copilot flies with you, in `glideslope_cli fly-copilot`, 2026-09-30 — finished above

**Was missing: only `glideslope_cli` flew with a copilot** - done above.
Nothing is built or run on Windows or macOS here but by CI and
`tools/windows_build.sh`; the code is not platform code.

**What flies**: `glideslope_cli fly-copilot AIRCRAFT LAT LON FEET HEADING
KNOTS TASK` flies an aircraft over the DEM with the AI, and a language model
as its copilot, with the player's own key - OpenAI's or Anthropic's, refused
without one. It is asked when engaged, when the engine stops
(`--engine-fails-at S`), when its route has been flown, and a minute after
each answer ("a routine look"); it is told where the aircraft is, what it is
doing, the route left and up to six runway ends within 40 km that say their
elevation. Its answer is flown 45 simulated seconds after it was asked
(`--thinking`), or when it comes if later; asked of a model now, the flight is
paced to the clock while a question is outstanding, so the model's seconds
are the flight's, and runs as fast as it can otherwise. A question the model
cannot answer changes nothing, and what happened is asked again, twice at
most; played back, a question the recording does not hold ends the flight
with an error. 45 s because GPT-5.5 asked for a whole route along a coast
once took 38 s; one slower than the allowance is said, and its flight plays
back otherwise than it flew.

- **It follows a coast as told**: off Bondi's south end at 1,500 ft, "follow
  the coast south to Cronulla at 1,500 ft, then orbit over Cronulla beach",
  GPT-5.5 (`gpt-5.5-2026-04-23`) gave eleven waypoints by the beaches and
  headlands - Bondi, Bronte, Coogee, Maroubra, Malabar, Little Bay, La
  Perouse, across the mouth of Botany Bay to Kurnell, Boat Harbour, Greenhills,
  Cronulla - and the AI flew them, each passed within 300 m. From its first
  route to Cronulla the coast - land and water both, the DEM's own mask, in
  rings 250 m apart in 16 directions - was within 1 km in 48 of 52 looks, ten
  seconds apart, and never more than 1,500 m away. **Two smaller models did
  not follow it**: Claude Haiku 4.5 and GPT-5.4 mini each gave Cronulla alone
  and flew straight there, over Maroubra and the middle of Botany Bay - within
  1 km of the coast in 73 to 78% of their looks, 2,000 to 2,250 m from it at
  most. Claude Sonnet 5 followed it, but with the recorder's 4,096-token limit
  it twice spent every token thinking and answered nothing; the limit is part
  of every recorded request, so raising it means recording the CBD plans
  again. `the_copilot_follows_the_coast_as_told_by_openai_as_recorded` flies
  GPT-5.5's answers again and asks for 85% of looks within 1 km and the coast
  never more than 1,750 m away; played back from Haiku's straight flight, it
  is red ("the coast was 2000 m from it at most").
- **It handles an engine failure**: off Bondi at 4,500 ft, told to follow the
  coast south to Cronulla, the engine stopped 90 s in. Claude Haiku 4.5
  (`claude-haiku-4-5-20251001`) answered with a glide at 60 kt to Sydney's
  runway 25 and an orbit over it - its first answer refused, an orbit 518 m
  across being too tight for 60 kt, and the second flown. The Cessna came over
  the threshold 1,924 ft above the ground 240 s after the engine stopped,
  circled down, and was 300 ft above the ground 247 m from it; from 45 s after
  the glide began it held 58 to 61 kt. `the_copilot_glides_to_a_runway_when_the_engine_stops_by_anthropic_as_recorded`
  asks for a glide, a threshold passed within 1.5 km more than 300 ft up,
  300 ft above the ground within 3 km of one, and the glide within a 10 kt
  band. With the glide not flown (the CLI setting none) it is red: the flight
  goes another way, and the recording holds no answer to what it asks next.
- **It never slows the step**: unit-tested below - the steps go on while the
  model is held.
- **It replays in CI without a key**: both recordings are in
  `tests/data/copilot/`, and are played back matched but their numbers.
  Played back on this machine, each flight is the one recorded, line for line.
  The two tests asking now (`..._asking_{openai,anthropic}_now`, labelled
  `live`) skip without `GLIDESLOPE_LIVE_MODEL=1`, seen to skip.

The parts, each tested alone:

- **`copilot::Copilot`** (`src/copilot/copilot.hpp`): told once the aircraft
  and the pilot's task, and asked - at engagement, now and then, and when
  something happens - where the aircraft is, what it is doing, the route left
  and the runways nearby. It answers `keep`, or a whole new route of
  waypoints and orbits flown from where the aircraft is, first line
  `glide AIRSPEED_KT` when the engine has stopped. The answer is read as a
  flight plan (`sim::parse_flight_plan`) and checked against the flight as the
  model was told it: every waypoint within 200 km, every height 500 ft above
  the sea and the ground beneath, every airspeed from the approach speed to a
  fifth over the cruise, a glide only with the engine stopped and from the
  approach speed to the best climb, and with it stopped nothing but a glide.
  A glide flies neither its waypoints' heights nor their airspeeds - it ends
  low over its field - so neither is held to anything, but its orbits must
  be wide enough for the glide's airspeed. One refused is told back to the model, up to three answers, as the planner's
  are. It is built inside the copilot's walls (`cmake/Copilot.cmake`): it
  sees the plan and the autopilot's modes, and nothing that moves a control.
- **It never slows the step.** A question is asked on a thread of its own
  (`ask`), and its answer taken between two steps when it has come
  (`answered`); neither waits, and a question outstanding is not asked again.
- **`sim::Controller::replan`** gives the AI a new route while it flies,
  keeping the autopilot, so nothing it holds is dropped: the largest step in
  any control the second after a turn to a new route is 0.0017 of its travel.
- **`sim::Controller::set_glide`** flies the route at an airspeed with the
  engine stopped: the airspeed is held by the vertical speed asked of the
  autopilot - 80 ft/min for each knot off it, an integral of 4 ft/min a knot a
  second finding the glide's own sink, and 250 ft/min for each knot a second
  of the speed's trend to damp it. **Not the autopilot's airspeed on the
  elevator**, which is its stall recovery and keeps the wing below the
  greatest angle of attack it has seen: a Cessna that had only cruised,
  asked for 68 kt from 100, swung between 73 and 84 kt for a minute and a
  half. The glide's airspeed is still asked of the autopilot, so that the
  least speed it holds a descent to (`Aircraft::climb_floor_kts`, above the
  Cessna's best glide) is lowered to it. Slowed from 100 kt the Cessna dips
  to 62 and comes back to 68 without passing it; from 45 s on it holds 65.2
  to 68.3 kt.
- **A recording plays back but its numbers** (`copilot::Match::but_numbers`):
  a flight flown again is not where it was to the last digit, so the requests
  a copilot recorded are matched with every number in them disregarded - the
  words, what the model was told and what happened, must be the recording's.
  The vertical speed is told as a signed number for that reason, not as
  "climbing" or "descending", which would turn on a sign.

Tests, each seen red with a bug put in and taken out again:
`a_copilots_answer_is_read_as_keep_or_a_route_and_refused_wherever_it_cannot_be_flown`
(all 14 ways an answer is refused, counted; red with the 200 km check
widened tenfold), `a_copilots_answer_refused_is_told_back_to_the_model_until_one_can_be_flown`
(red with the reason left out of what the model is told),
`the_copilot_asks_on_a_thread_of_its_own_and_the_step_never_waits_for_the_model`
(1,200 steps taken while a stand-in model is held, which the steps let go;
red, after the minute the stand-in waits, with `ask` waiting for its answer),
`a_recording_played_back_but_its_numbers_answers_a_request_whose_figures_moved_and_no_other`
(red with every request taken as matching),
`a_plan_changed_while_the_ai_flies_moves_no_control_at_the_change_and_is_flown`
(red with the autopilot engaged afresh at the change) and
`a_glide_with_the_engine_stopped_holds_its_airspeed_on_the_elevator_along_its_route`
(red with the glide not applied).
