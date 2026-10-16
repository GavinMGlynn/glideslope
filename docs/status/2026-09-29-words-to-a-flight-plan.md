### Words to a flight plan, 2026-09-25 — item done 2026-09-29

**What is still missing.**
- **The copilot plans once, on the ground.** Flying with you is the next item.
- **Nothing but `glideslope_cli plan` asks a model yet.** No server setting
  or hand-over chooses a provider for an aircraft; that is the "different
  model on each AI aircraft" item, still open.

**Both models have answered, 2026-09-29.** "Take off, climb to 3,000 ft and
orbit the CBD", asked of each for the Cessna at Sydney:
- **Claude, `claude-haiku-4-5-20251001`**, Anthropic's cheapest. Its first
  plan was refused - an orbit of 518 m at 60 kt, 3 m tighter than allowed -
  and told back; its second circles 521 m round a point 0.5 km from Town
  Hall, for ever, at 60 kt.
- **ChatGPT, `gpt-5.4-mini-2026-03-17`**, OpenAI's cheapest snapshot that
  plans it. It circles 1,447 m once, at 100 kt, first answer.
- Both fly to the CBD first and then round it, 16R, take-off to 1,000 ft.
- The recordings are `tests/data/copilot/cbd-orbit-{anthropic,openai}.jsonl`:
  URL, request body, status and answer body only, no header and so no key;
  read by eye before committing. A recording holds the model asked, so the
  tests name the model; the providers' defaults are unchanged.
- **Live calls made, and their cost.** Anthropic 3, OpenAI 3 (each counted
  as one Messages or Chat Completions request):
  - Anthropic: the recorded two (the refused plan and the one flown), and one
    more when a `ctest -R orbit` matched the live test by mistake.
    2,427 tokens in and 235 out on Haiku 4.5 at $1 and $5 a million: about
    $0.004.
  - OpenAI: the recorded one, and two more from the same mistaken run (a
    refused plan - no TURNS - and one flown). 2,276 tokens in and 252 out on
    GPT-5.4 mini; at its list price as understood here, $0.75 and $4.50 a
    million, about $0.003.
  - One free model listing from each, to choose the models.
  Together under a cent, of the $20 put on each account.

**A model's plan found a bug in the navigator.** Both models planned a
waypoint at the orbit's centre and then the orbit. The navigator began
counting turns wherever the aircraft first came within the radius - here at
the centre - so its "two turns" were the spiral out: Claude's plan was flown
164 to 492 m round a 521 m circle. Now the turns are counted only from
within 100 m inside the circle (`orbit_joined_m`), steered out to it until
then. Reached from outside, that is where the circle is first crossed, so
every orbit flown before is flown as it was.

**The replay's check is as the plan asks.** The test held every orbit to two
turns; ChatGPT planned one. It is now held to the turns the plan asks, or
two for an orbit flown for ever (`fly-plan --orbits 2`).

**What works.**
- **Plans that take off and orbit.** A plan may start on a runway (`runway`,
  `takeoff`): the take-off autopilot flies it off, and the navigator takes
  over where it hands over, from where it is then. An `orbit` is a waypoint
  flown round, either way, as often as asked or for ever. It is steered
  along the circle's tangent five seconds ahead, and turned in by 90 degrees
  a kilometre off it. No orbit may be tighter than two and a half times the
  circle the aircraft turns at the autopilot's 25 degrees of bank: 1,172 m at
  90 kt. At one and a half times, a Cessna wandered from 374 to 1,041 m round
  704 m. A plan with any line of `aircraft`, `start`, `runway` or `takeoff`
  twice is refused.
- **Every runway in the world**, from OurAirports' public-domain
  `runways.csv`, pinned by commit and SHA-256 (`docs/ASSETS.md`): 27,116
  open ends with a place and a heading. A plan's runway comes from here,
  never from a model's memory.
  It is read as a DEM tile is, through `FileSource`, so that on Windows a
  copy another process is renaming into place is still readable. That is the
  race `a_tile_held_open_for_deletion_as_a_rename_holds_it_is_still_read_whole`
  builds; found by the review of the Windows fix.
- **An HTTP POST** on all three backends, and JSON written (`write_json`).
  **A POST follows no redirect**, so a key in its headers goes nowhere but
  where it was sent. libcurl drops only the Authorization it made itself on
  a change of host, and Anthropic's key is `x-api-key`: found by review.
  A header with a control character is now refused by the platform itself,
  on every request. Before, only the terrain code checked, though
  `HttpRequest` said otherwise.
- **The copilot** (`src/copilot`), linked by nothing presentational.
  - OpenAI's Chat Completions or Anthropic's Messages, each asked with the
    user's own key, read at run time from `openai-key` or `anthropic-key` in
    the config directory. A provider with no key is refused, saying where
    the key goes. OpenAI is asked `gpt-5.5-2026-04-23`, a dated model.
  - A service's error is said in its own words, with the key taken out
    wherever it appears.
  - Only runway ends that give their elevation are offered to the model,
    since a plan's heights are checked against it.
  - What it answers is read as a plan and checked. A plan is refused if it:
    - does not parse, or is for another aircraft;
    - has an orbit too tight for its airspeed;
    - does not take off from one of the airport's runway lines, exactly;
    - flies below the approach speed, or more than a fifth over the cruise;
    - flies lower than 500 ft above the runway;
    - goes more than 200 km from the runway.
    A refused plan is told back to the model, up to three answers in all.
  - Exchanges can be recorded and played back. Played back, a request must
    be byte for byte the one recorded. No header is recorded, so no key is.
- **`glideslope_cli plan AIRCRAFT AIRPORT COMMAND`** asks, and
  **`glideslope_cli fly-plan FILE`** flies a plan over the DEM, saying how
  the take-off, each waypoint and each orbit went.

**Verified.**
- `the_navigator_flies_an_orbit_round_its_centre_as_often_as_asked_either_way_in_calm_air_and_in_wind_down_to_the_tightest_allowed`:
  8 orbits, flown twice round and then on:
  - 1,500 m and the tightest allowed at 90 kt, 1,172 m;
  - both ways round;
  - in calm air and in a 20 kt crosswind.
  The worst was 150 m outside the tightest circle, in the wind, held to
  160 m; in calm air, 80 m. Height held within 8 ft.
- `a_plan_that_takes_off_leaves_its_runway_and_flies_its_waypoints_in_every_light_aeroplane`:
  every light aeroplane in the catalogue, taken off by the take-off
  autopilot to 500 ft. Each then passes a waypoint 15 km off within 5 m, at
  its height. On the way, each strays at most 81 m from the leg starting
  where it was handed over, held to 150 m. Flown from the threshold instead,
  they strayed 219 to 383 m.
- `a_plan_that_takes_off_and_orbits_the_cbd_is_flown_over_the_dem`:
  `sydney-cbd-orbit.plan`, written by hand, flies off Sydney's 16R over the
  real ground and circles Town Hall twice. It stays 1,433 to 1,499 m from the
  centre and 3,000 to 3,002 ft high, held to 150 m and 50 ft.
- The copilot, offline, against a stand-in for each service:
  - each provider's request as its API has it;
  - a service's error said in its own words, never with the key;
  - nine ways a plan is wrong, each refused and told back;
  - a recording that replays only what it recorded, and holds no key.
- The runways reader, the POST (byte for byte, through a server on the
  loopback address) and `write_json` each have their own tests.
  `a_post_follows_no_redirect_so_its_key_goes_nowhere_else` answers a POST
  with each of 301, 302, 303, 307 and 308 pointing at a second server, which
  must hear nothing.
- `take_off_climb_to_3000_ft_and_orbit_the_cbd_is_planned_by_{openai,anthropic}_now_and_flown`
  ask the model now with the key on this machine, record the exchange, and
  report themselves skipped, saying why, with no key.
- `take_off_climb_to_3000_ft_and_orbit_the_cbd_is_planned_by_{openai,anthropic}_as_recorded_and_flown`
  play each recording back, with no key, and fly the plan over the DEM, in
  CI. Claude's: round 2 times at 394 to 427 m from the centre (521 m asked),
  2,991 to 3,000 ft. ChatGPT's: once round at 1,450 to 1,453 m (1,447 m
  asked), 3,000 ft.
- `an_orbit_begun_from_its_centre_counts_its_turns_only_from_its_circle`:
  a Cessna started at the centre of circles of 1,500 m and the tightest at
  90 kt, 1,172 m, both ways round, in calm air and in a 20 kt crosswind.
  Each joins 100 m inside its circle, goes round once, and is held within
  69 m of it (62 m in the wind).

**Seen to fail**, each put back:
- the orbit's way round reversed;
- a take-off ignored: the plain autopilot flew the aeroplanes off the ground
  by itself, which the test first missed, and now catches;
- a closed runway kept;
- a POST one byte short, and an unsafe header sent;
- control characters written unescaped;
- a plan too low taken;
- a POST following redirects;
- the key left in a service's error;
- every runway end offered, elevation or none;
- the first leg flown from the threshold, not the hand-over;
- a played-back request not compared;
- the geoid left out of a plan's heights: 72 ft at Sydney, so the orbit's
  height bound is 50 ft.
- a recorded answer altered to orbit at 2,500 ft: the replay failed, "not
  within 50 ft of 3000";
- the plan's turns taken as two when ChatGPT's asks one: "only 1.00 times of
  2 planned";
- the navigator's old join, counting from the centre: the new unit test
  failed ("joined 0 m out"), and so did Claude's replay (164 to 492 m).
