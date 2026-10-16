### The model that plans an aircraft is chosen when it is handed to the AI, 2026-10-02 — item done

**What is not done** (tails in COMPLETION_PLAN.md):
- **A model planning an aircraft left by a player who goes was not told the
  plan it flies**: until its answer comes it flies the server's plan file,
  and the model is told "no route is flown: the autopilot holds what the
  aircraft is doing". (Done 2026-10-06: it is told the plan's waypoints.)
- **The window client's hand-over model is chosen at start**, by a flag,
  not by a key in flight; and its refusal for want of a key is tested on
  the headless client only - the window client's path is the same
  `PlayersCopilot` refusal, said in its own words, untested there.
- **The hand-over's words come from the installed data** beside the program
  (`platform::data_directory()`), as the catalogue is read on the clients -
  `glideslope_cli --data` does not move them. The server reads them from its
  own data directory.
- **Claude Haiku 4.5 misreads a climb as a descent.** In the player's
  recorded hand-over it reads the aircraft's own 3,791 ft/min climb, five
  seconds after joining, as a descent; its route, the third answer after
  two refused, flies to Sydney's runway 25 at 500 ft - checked and flown,
  not a good plan. The server's recordings are of aircraft in a real dive
  (the test client's `--fly` pattern): 9,934 and 4,786 ft/min down.

**What works: every hand-over has its model.**
- **The server's AI aircraft** - #62's per-aircraft choice, unchanged.
- **A player handing their own aircraft over chooses**:
  `--hand-over-model anthropic|openai|none`, either model with `:MODEL`, on
  the client with the window (A, or `--hand-over-after`) and on
  `glideslope_cli connect` (`--hand-over-at`); none by default. With a model
  the player's copilot (`frontend::PlayersCopilot`, #64's) is made for the
  aircraft the server says is theirs, told the data's hand-over words
  (`assets/tasks/hand-over.words` - keep it safe and near, fly to somewhere
  close by and orbit there) and asked, as it is handed over, "the pilot has
  handed you the aircraft", from where it is in the air, with the player's
  key on the player's machine. The route goes to the server as any copilot's
  (`COPILOT_ROUTE`), which checks it and flies it; the `CONTROLLER_SWAP`
  goes first, so the AI holds its course until the route comes and the
  aircraft is handed over once. With none the AI holds its course. A model
  with no key is refused, saying so, and the aircraft held as with none.
  On the window client the hand-over's model is also the copilot C asks;
  with a task of the player's (`--copilot TASK`) the hand-over's words are
  said with the hand-over's question ("..., saying: ..."), so it is still
  planned as a hand-over.
- **An aircraft left to the AI by a player who goes (`--on-leave ai`) or by
  a take-over is planned by the server's model** (the project owner,
  2026-10-02; REQUIREMENTS.md section 5): `glideslope_server
  --hand-over-planner anthropic|openai|none[:MODEL]`, with
  `--hand-over-playback FILE` or `--hand-over-record FILE`; none by default.
  The server asks with its own key, from where the aircraft is: what it is
  told is read on the stepping thread (the aircraft's properties are that
  thread's), the runways near it looked up on the question's own thread
  from the world's runways, read once on a thread of their own as the
  server starts; the model is asked through `copilot::Copilot`, off the
  stepping thread, and its answer picked up between two steps. The answer
  is checked again against the aircraft as it is then, with the checks a
  player's copilot's route has (`fly_route_on`: `change_refusal`, the
  collision ground), and flown; the server says "aircraft N, left by its
  player, flies its model's route of ...", and where it has got to each
  half minute. **With none, or a model refused** ("aircraft N, left by a
  take-over: openai is refused: no OpenAI key ...; it holds its course
  instead"), it goes on as it always did: left by its player it flies the
  server's plan file - and with no plan file it is taken out of the sky,
  and nothing is planned; left by a take-over it holds its course. An
  aircraft wrecked or taken over again before its answer comes lets the
  question go. **`--until-empty` waits on it**: with a hand-over planner the
  server stops once its clients have gone and every aircraft so given has
  its answer and has been said to fly its route five times - two and a half minutes.
- **From the final check (2026-10-02), fixed:**
  - **Letting a hand-over's question go never waits on the stepping
    thread.** Dropping one (a new hand-over of the same aircraft, a wreck, a
    take-over, a player's aircraft taken out of the sky) ran `~Copilot`,
    which waits for its question - normally as long as abandoning the
    model's request takes, but for as long as the world's runways took to
    download if the question was still waiting on them. Now it is given up
    (`Copilot::give_up`) and kept in a list until nothing of it runs
    (`Copilot::settled`), swept between steps; and a question is not asked
    until the world's runways have been read, so it never waits on their
    download at all (the aircraft's situation is read when it is asked).
    Leaving, the server gives up every question first, then waits only as
    long as giving up takes - and, as before, for the runways' download if
    it is still going. **Not tested on its own**: building a runways'
    download slow enough to see it needs a slow server of its own; named
    here.
  - A model's route on an aircraft taken over goes with the take-over, and
    so does a question out for it.
  - **A left aircraft wrecked flies again holding its course**, from where
    the player started, as it always has (a left aircraft was never on the
    server's plan once wrecked: `take` does not mark it so); its model's
    route goes with the wreck. Not changed here.
  - The hand-over's words and a `--hand-over-playback` recording are read
    once, as the server starts - either not had refuses the start, saying
    which - not at each hand-over on the stepping thread. One recording is
    played through in order, hand-over after hand-over.
  - A planner with nothing to plan - `--hand-over-planner` on a server
    with neither `--on-leave ai` nor take-overs (`--no-take-over`) - is
    refused, and so is `none:MODEL`, saying none takes no model
    (`the_server_refuses_a_hand_over_planner_with_nothing_to_plan`,
    `..._of_none_with_a_model`).
  - **Not explained**: on the Windows development machine, freshly built,
    `glideslope_server` refusing its arguments ended `0xc0000409` (a fast
    fail) instead of exit 2 in four or five of 37 `the_server_refuses_...`
    runs, twice - which ones changed from run to run, old refusals
    (`..._a_key_that_is_not_hexadecimal`, `..._steps_with_a_window`) among
    them - while Webroot's log shows it monitoring each new
    `glideslope_server.exe` process; a refusal runs nothing of this item.
    Not checked against main's build on that machine; CI's Windows job is
    the judge. On Linux,
    `a_players_copilot_glides_its_aircraft_on_a_server_when_the_engine_stops_as_recorded`
    failed once after 6.9 s, its output not kept, and passed alone and
    beside the other ten server copilot tests after.
- Nothing on the wire changed: TRANSPORT.md is as it was.

**Verification**
- Player's own (`tests/cmake/server_hand_over_model.cmake`; the server with
  `--until-empty`, the headless client predicting, handing over five
  seconds after joining):
  - `a_players_aircraft_handed_to_the_ai_in_the_air_is_planned_by_claude_and_flown_as_recorded`:
    Claude Haiku 4.5 (`claude-haiku-4-5-20251001`),
    `tests/data/copilot/hand-over-anthropic.jsonl`, no key: to YSSY_RW25,
    10,114 m from it, then 8,923 m.
  - `..._planned_by_chatgpt_and_flown_as_recorded`: GPT-5.5
    (`gpt-5.5-2026-04-23`), `hand-over-openai.jsonl`: to Bondi at 3,500 ft
    and round it; 1,142 m from BONDI, then into the orbit.
  - `a_players_aircraft_handed_to_the_ai_with_no_model_holds_its_course`:
    nothing asked or sent; over 60 s heading 6.0 to 5.9 degrees, height
    4,218 to 4,274 ft (within 5 degrees and 300 ft).
  - `a_hand_over_model_with_no_key_is_refused_and_the_aircraft_held`: both
    models, 2 of 2, with no key anywhere: each refused naming its key, and
    held.
  - `a_players_aircraft_taken_back_at_once_is_not_handed_to_the_ai_again_by_its_copilot`
    (from the review): handed over and taken back in one breath, before any
    update shows the AI with it; the copilot stands by at once
    (`PlayersCopilot::taken_back`, called on every take-back by both
    clients) and no route is sent - the server hands it to the AI once.
- The window client (`client_copilot.cmake`, headless on Vulkan):
  `the_client_with_the_window_hands_its_aircraft_over_and_the_model_chosen_plans_it`
  (`--hand-over-model anthropic:...`, `--hand-over-after 3`), and the
  copilot's own `the_client_with_the_window_asks_its_copilot_and_the_server_flies_its_route`.
  **From the review, both now tell a route flown from one refused**: the
  server says nothing back and the AI has the aircraft either way, and its
  output is the client's input in that pipeline, so the client says how far
  its route's first waypoint was when the route was sent and at the shot,
  the shot waits until it is 200 m nearer, and the test wants that: 10,007
  then 9,806 m (hand-over), 6,634 then 6,433 m (asked); 0.091 and 0.093 m
  steps at the switch.
- The server's (`tests/cmake/server_hand_over_planner.cmake`; the server
  with `--until-empty`, waiting on its hand-over as above):
  - `an_aircraft_left_by_a_player_who_goes_is_planned_in_the_air_by_the_servers_claude_as_recorded`:
    the client flies five seconds and leaves; Claude Haiku 4.5,
    `tests/data/copilot/leave-server-anthropic.jsonl` (three answers, two
    refused: a glide with the engine running, and 254 ft), no key: to YSSY
    at 1,500 ft and round it: 9,955 m from YSSY when the route was taken,
    7,878 and 7,953 m at the last line in two runs.
  - `an_aircraft_left_by_a_take_over_is_planned_in_the_air_by_the_servers_chatgpt_as_recorded`:
    the client takes over the server's AI aircraft five seconds in; GPT-5.5,
    `take_over-server-openai.jsonl`: round Bondi Beach at 3,000 ft and
    1,600 m: 1,716 m from its centre when taken, 1,584 and 1,581 m - on its
    circle - at the last line.
  - **How these measure progress, changed in the final check**: the server
    says how far the route's first waypoint is when it takes the route, and
    the last half-minute line must be nearer it than that (or past it), and
    to one of the route's own waypoints. They had compared the first
    half-minute line with the last, which a one-orbit route already on its
    circle fails (Windows: 1,607 then 1,662 m round Bondi), and which an
    aircraft that took the route but flew on with the plan file passed. And
    the server waits for five half-minute lines, not three: the aircraft
    left by its player is handed over in a dive at 124 kt (the test client's
    `--fly`) and Claude's route is flown at 60 kt, so it had not turned back
    towards YSSY by the third line in one run in two (10,258 m against
    9,954). Red with the route taken and not flown (`replan` skipped for an
    aircraft left to the AI): "the aircraft is flying to THE_HEADS, not a
    waypoint of its model's route"; the take-over case ran to its 600 s and
    failed "the server did not stop on its events".
  - Each `..._asking_anthropic_now` / `..._asking_openai_now` (live, only
    with `GLIDESLOPE_LIVE_MODEL=1`) made its recording; none of the four
    recordings holds an authorisation header.
  - `a_servers_hand_over_planner_with_no_key_is_refused_and_the_aircraft_goes_on_as_before`:
    both cases and both models across them, 2 of 2, with no key anywhere:
    leave with anthropic ("it flies the plan file instead") and take-over
    with openai ("it holds its course instead"); no model's route flown.
    What flying the plan file and holding a course are is tested by
    `server_leave.cmake` and `server_take_over.cmake`.
  - Four `--dry-run` refusals, `the_server_refuses_a_hand_over_...`: a
    planner not anthropic, openai or none; a playback, and a recording, with
    no planner; a playback and a recording together. The flag walk knows the
    three new flags.
- **From the review, also fixed**: `glideslope_cli` refuses
  `--copilot-provider` or `--copilot-model` beside `--hand-over-model` even
  when they name the default; hand-over words that cannot be read are said
  as that ("the hand-over's words cannot be read: ..."), on both clients
  and the server ("cannot be planned"), not as a key refused.
- Built on Windows (windows-debug, `tools/windows_build.sh`), where the
  server's no-key test, its ChatGPT take-over and the take-back test passed
  too.
- Every test in the areas this touches, by name - copilot, hand, take-over,
  take-back, swap, planned, flag, refusal, ride-along, connect, message,
  leave and ports: 238, all passed but 13 skipped as they should be - the
  twelve live model calls, and
  `a_refusal_that_stays_is_given_up_on_after_its_stated_wait`, which skips
  here (not checked against main).

**Seen to fail.** Each run and reverted:
- the player's copilot not asked at the hand-over: both recorded player
  tests failed ("the client never said \"asked its copilot, the pilot has
  handed you the aircraft\"");
- the player's refusal made silent: "anthropic with no key was not refused,
  saying so";
- the server flying a plan of its own at a player's hand-over in place of
  holding: "heading 6.0 to 98.1, height 4218 to 4971 ft";
- the server's hand-over planning never started (leave and take-over): both
  recorded server tests ("the server never said a model planned the
  aircraft left by its player" / "by a take-over") and the no-key test
  ("leave: anthropic with no key was not refused") failed;
- every player's route refused by the server: the window hand-over test
  failed - "the shot was drawn before the copilot's route was flown", the
  aircraft never 200 m nearer;
- `taken_back()` doing nothing: the route was sent after the take-back,
  and the take-back test failed - "taken back, the aircraft was planned all
  the same".
