### Each AI aircraft is planned by the model its server chooses, 2026-09-30 — item done 2026-10-02, above

**What is missing first.** **Nothing chooses a model when an aircraft is
handed to the AI.** A player's aircraft handed over (`--on-leave ai`, the
hand-over button, a take-over) still flies the server's plan file or holds
its course. Planning one needs a request for an aircraft already in the
air - the planner's instructions and checks are for one standing on a runway
and taking off - and a new request needs new recordings, which are live model
calls this change did not make. So the item stays open, with that named.
(Every hand-over is planned by a model since 2026-10-02: the player's own
by the one they choose, one left by a player who goes or by a take-over by
the server's - above.)

**What works.** A server's owner chooses, per AI aircraft, who plans its
flight: `--ai-planner N=anthropic[:MODEL]`, `N=openai[:MODEL]`, or `N=none`
(the plan file, as before). The task is data: `--ai-task FILE`, by default
`assets/tasks/sydney-cbd-orbit.task` - the aircraft, the airport, and the
words, "take off, climb to 3,000 ft and orbit the CBD". When the server
starts, before its clock does, each chosen model is asked with the server
owner's own key (`GLIDESLOPE_OPENAI_KEY` / `GLIDESLOPE_ANTHROPIC_KEY` or the
config directory's key files), through the same `copilot::plan_from_words`
as `glideslope_cli plan`: the request is byte for byte the CLI's, so the
recordings in `tests/data/copilot/` play back to it. The answer is only ever
a flight plan, checked, which the autopilot flies; the server prints the plan
and who planned it. `--ai-playback N=FILE` plays a recording back instead of
asking, for tests.

- **A model with no key is refused, not faked**: the server says
  "AI N: anthropic is refused: no Anthropic key: put yours in ...; it flies
  the plan file instead", and the aircraft flies the plan file. The refusal
  comes before anything is fetched or sent. A model whose every plan is
  refused falls back the same way, saying why.
- **A planned aircraft takes off.** It stands on the plan's runway, at the
  DEM's height at the threshold (what it collides with), and the take-off
  autopilot flies it off; the navigator takes over where it hands over. The
  plan's heights are above sea level and are flown above the ellipsoid, the
  geoid added, as `fly-plan` does.
- **Two planned aircraft are kept apart on the runway and in the orbit -
  not along the way between.** Both recordings take off from 16R, and two
  aircraft on one threshold collide before either moves. So the n-th planned
  departs 90 simulated seconds after the one before (`--ai-spacing` says
  otherwise), and is not in the sky until then; and, as the plan file's AI
  aircraft are stacked, each flies its plan 500 ft above the one planned
  before - ChatGPT's inner-and-outer circles round the CBD come within 14 m
  of Claude's at one height. Both are said: the second's orbit is flown at
  3,500 ft. **What is not kept apart**: the second climbs through the
  first's orbit height on its way to its own, and nothing keeps a planned
  aircraft clear of the plan file's AI aircraft. That is a tail in
  COMPLETION_PLAN.md.
- **A planned aircraft wrecked flies again from its runway only when the
  runway is clear**: nothing flying within 2 km of its threshold. Before,
  two wrecked together - by colliding - were put back on one threshold in
  one step and collided again every 5 s for ever. Now the aircraft are gone
  through in turn, so of two due in one step the second sees the first on
  the runway, waits as a wreck, and says so once: "aircraft 17, ..., waits
  for 16R to be clear of aircraft 16 before it flies again". An aircraft
  that loiters within 2 km of the threshold keeps it waiting for as long as
  it does.
- **An aircraft whose airport's runways cannot be had is not planned, and
  the server goes on**: OurAirports unreachable with nothing cached, or its
  file damaged, was an exception that stopped the server at start. Now the
  server says "AI N cannot be planned: the runways cannot be read: ...; it
  flies the plan file instead", as it does for a refusal.
- **A task file with a key given twice, or a tab after its key, is
  refused**, saying which; before, the second value silently won, and a tab
  was reported as a line that was "not aircraft, airport or task".
- **`--steps N`** takes N steps as fast as they go with nobody joining, then
  stops: simulated time for a test, not the machine's.
- **The end of a run says how each plan went**: "planned by openai; took off
  from 16R, handed over 1000 ft above it; round SYDNEY_CBD 1.00 turns, 1451
  to 1454 m from its centre, at 3000 to 3001 ft".

**Verification.**
- `each_ai_aircraft_is_planned_by_the_model_its_server_gives_it_and_one_with_no_key_is_refused`
  (`tests/cmake/server_planned.cmake`), in CI with no key. Three cases, and
  it fails unless all three are walked:
  - a server with two AI aircraft, the first planned by ChatGPT and the
    second by Claude, each from its recording, flown for 84,000 steps (eleven minutes and forty seconds of simulated time):
    each takes off (handed over at least 400 ft up), flies round its orbit as
    often as its plan asks (twice for Claude's, for ever), within 150 m of
    its circle and 50 ft of its height, centred within 2 km of Town Hall,
    and nothing is wrecked. Flown here (linux-debug): ChatGPT's round
    SYDNEY_CBD once, 1,451 to 1,454 m from its centre (1,447 m asked), at
    3,000 to 3,001 ft; Claude's round CBD 3.52 times, 394 to 428 m (521 m
    asked - 93 to 127 m inside it, the navigator's open tail on tight
    orbits, and the reason the allowance was 150 m; 60 m since that tail was
    done, below), at 3,490 to 3,500 ft (3,500 asked, stacked). Both handed over
    1,000 ft above 16R. The test took 493 s under a loaded -j4 run; its
    timeout is 1,800 s.
  - a server told to plan with Anthropic, and one with OpenAI, with no key
    anywhere it could look - both key variables unset, and `HOME`,
    `XDG_CONFIG_HOME` and `APPDATA` an empty directory - refuses, saying so,
    and the aircraft flies the plan file. No model is called.
- `a_task_file_names_its_aircraft_airport_and_words_and_anything_else_is_refused`:
  the committed task reads as the recordings' words, and five wrong tasks
  are each refused.
- `two_planned_aircraft_wrecked_together_on_one_runway_fly_again_one_after_the_other`
  (`tests/cmake/server_planned_together.cmake`) builds the collision: told
  `--ai-spacing 0`, the server stands both recordings' aircraft on 16R at
  once, and the test checks they collided (two wrecks, each "collided").
  Then exactly those two wrecks in 36,000 steps (300 s), the second saying
  it waits for 16R, and both handed over 1,000 ft above 16R. 56 to 84 s here.
- `an_ai_aircraft_whose_runways_cannot_be_read_flies_the_plan_file_and_the_server_goes_on`
  (`tests/cmake/server_planned_runways.cmake`) gives the server a cache of
  its own - the downloads linked, OurAirports' runways a file with no
  runways' columns, which the cache is not fetched past - so the runways
  cannot be had without touching the network. The server says the aircraft
  cannot be planned and it flies the plan file. **Changed the same day** by
  "A cached tile cut short or damaged is fetched again" (above): a cached
  copy that is not the pinned file is now fetched again, so on CI, with
  the network, the real runways arrived and the aircraft was planned. The
  test now also sends the runways' fetch where nothing listens
  (`GLIDESLOPE_RUNWAYS_SOURCE=http://127.0.0.1:1`), as OurAirports
  unreachable, and expects the refused download in the message.
- Ten `--dry-run` refusals, `the_server_refuses_...`: a planner for AI
  aircraft 3 of 2, and for AI aircraft 0; a planner not anthropic, openai
  or none; a playback for an aircraft with no model; a task with no model;
  negative `--steps`; `--steps` with `--seconds`, with `--until-empty` and
  with `--window`; a negative `--ai-spacing`.
- `a_task_file_names_its_aircraft_airport_and_words_and_anything_else_is_refused`
  now walks nine wrong tasks - each of the three keys given twice, and a tab
  after a key, among them - and checks each is refused saying why.
- `every_flag_the_server_prints_in_its_usage_is_one_it_takes` walks the five
  new flags.

**Seen to fail.** Two deliberate bugs, each run and reverted:
- the planned aircraft flown without its take-off (the plan given to the
  navigator from the runway, no take-off autopilot): both struck the ground,
  and the test failed on "an aircraft was wrecked";
- the refusal made silent (said to stderr, without "is refused"): the
  planned half still passed and the keyless half failed on "anthropic with no
  key was not refused, saying so".
From the review, each run and reverted:
- the stack taken out (every planned aircraft's plan flown at its own
  heights): the planned test failed on "AI 2 flew round CBD at 2991 to 3000
  ft, not within 50 ft of 3500";
- every orbit flown half as wide again as planned: it failed on "AI 1 flew
  round SYDNEY_CBD 2134 to 2137 m from its centre, off its 1447 m circle by
  more than 150 m";
- the runway-clear rule taken out: two planned aircraft stood on 16R at once
  wrecked each other 60 times in 300 s, and the together test failed on "120
  wrecks, not the two of the one collision built";
- the runways' failure not caught: the server stopped - "the runways cannot
  be read: the runways file has no column airport_ident" - and the runways
  test failed;
- the new checks on the options taken out of `wrong_with`, and the WHO check
  from the parser: each of the ten refusal tests failed;
- `parse_task`'s duplicate check taken out: the task test failed on the
  first duplicated key; its tab check alone taken out: it failed on "is
  refused saying 'has a tab after its key', not: ... is not aircraft,
  airport or task".
A first attempt at the first bug - the planned aircraft given the server's
plan - crashed the server under UBSan instead, because the server's plan is
not yet read when a planned aircraft is made; that ordering is only reached
by the bug, and the crash is why the bug used was changed.
