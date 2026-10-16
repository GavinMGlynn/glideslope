### AI aircraft are kept 500 ft or 1.5 nm apart along their routes, 2026-10-02 — tail still open

**What is missing first.** **No run measures an aircraft handed to the AI**
- left by its player, left by a take-over, or flying a copilot's route -
alongside the others. The monitor gives every AI-flown aircraft the same
limits, those included, but the measured run is the server's own AI
aircraft only (`--steps` runs with nobody joining). And **nothing keeps a
person clear**: AI aircraft give way to a person's aircraft, but a person
flying into one faster than it can climb or descend out of the way, or two
people, are not kept apart, and no person's aircraft is measured. So the
tail stays open, with those named.

**What works.** The minimum is **500 ft vertically or 1.5 nm (2,778 m)
horizontally**: two aircraft closer than both at once have lost separation
(`REQUIREMENTS.md` 6.5 and the decisions of 2026-10-02 - FAA JO 7110.65
7-9-4's Class B VFR minimum; a horizontal minimum alone would be larger
than the orbits two models plan round one place). Three things keep it
(`sim/separation.hpp`):
- **Layers 1,000 ft apart**, twice the minimum (`ai_stack_ft`, was 500).
  **The plan file's AI aircraft fly the plan at their own layer**, not only
  begin there: stacked at the start alone, every one came down to the
  plan's heights at its first waypoint, and AI 3 and AI 4 flew the whole
  harbour tour on top of each other, 37 m apart at closest - found by this
  measurement. A player given an aircraft starts above every one flying, on
  the same layers.
- **Planned aircraft stacked downwards in the order they take off**: the
  first away flies its plan 1,000 ft above the next, so on their first
  departures none climbs to its height through another's. One that flies
  again after a wreck, or is held on the ground past the next one's turn,
  does climb through the lower layers; then the monitor holds the others
  clear of it. An aircraft left by its player flies the plan file on a
  layer of its own, above the plan-file AI aircraft's.
- **Known limits**: a departure waits for clear sky with no way out - an
  aircraft that sits within 1.5 nm and 700 ft of a threshold (aborted on
  the runway, or landed there) holds every later departure from it for the
  rest of the run, said once. A glide (engine stopped) does not give way;
  it is given way to, since it cannot climb to a floor. The 500 ft / 1.5 nm
  minimum was taken working this tail and confirmed by the owner on 2026-10-02. **Changed figures**: in
  `each_ai_aircraft_is_planned_...` ChatGPT's (AI 1, first away) orbit is
  now flown at 4,000 ft and Claude's (AI 2) at 3,000 ft, where they were
  3,000 and 3,500; the test expects that. And a planned aircraft takes off
  only into clear sky - nothing within 1.5 nm of its threshold and 700 ft of
  its height - said once while it waits. Two due in one step are not kept
  from each other: told `--ai-spacing 0`, the server stands them on their
  runways together, which is how
  `two_planned_aircraft_wrecked_together_...` still builds its collision
  (it failed on "did not collide" until that was so).
- **An aircraft the operator puts on a course** (`--fly`) holds it exactly:
  it is given way to, as a person's is, and not measured. It is how
  `two_aircraft_on_a_collision_course_collide_...` builds its collision,
  and with the monitor moving one of its 737s it failed on "did not
  collide".
- **A monitor, every step** (`sim::separate`, called by `Fleet::keep_apart`
  before anything is flown): an AI aircraft whose autopilot is flying gives
  way to every aircraft that does not (a person's, one taking off or
  landing, one on the operator's course) and to every AI aircraft before it
  in the server's order. Near
  one it gives way to - within 3.8 km now, or by the straight lines both fly
  within 90 s - it may not come within 700 ft (the minimum and 200 ft in
  hand) of the heights the other is at or flying to: held below if below,
  above if above, and within 50 ft of level, the side the height it is
  flying to is on. **It acts only through the autopilot**:
  `Controller::limit_height` puts a floor or a ceiling on the height the
  autopilot flies to (`Autopilot::limit_height`), and nothing else is
  touched - no control moved, no turn. A ceiling that would hold it within
  500 ft of the ground is a floor instead; squeezed between two, it takes
  the side that asks less of it. The server says when an aircraft is held
  and when it flies its own height again: "aircraft 6, c172p (AI 1,
  openai's plan), is held below 3727 ft, clear of aircraft 4".
- **Measured**: the server records every two AI aircraft at every step - the
  closest in a straight line, the least height between them while within
  1.5 nm, and how long within both minima - and ends a run with a line for
  each pair and one for them all ("kept apart: 6 pairs of AI aircraft over
  108000 steps, separation lost for 0 steps").

**Measured** (four AI aircraft - two planned from the CBD recordings, two
flying the harbour plan - 108,000 steps, the 15 minutes of the harbour
tour, linux-debug):

| pair | before: least height within 1.5 nm, lost | after |
|---|---|---|
| AI 1 and AI 2 (the two orbits) | 445 ft, lost 229 s from 386 s | 904 ft, never; 647 m at closest |
| AI 1 and AI 3 (orbit and tour) | 572 ft | 706 ft, never |
| AI 3 and AI 4 (the tour) | 0 ft, 37 m apart, lost 896 s from the start | 975 ft, never |
| the other three | never within 1.5 nm | never within 1.5 nm |

Before is this branch's measurement on the code as it was: 500 ft stack,
planned aircraft stacked upwards, no monitor, no departure check.

**Verification.**
- `every_ai_aircraft_a_server_runs_planned_or_not_is_kept_500_ft_or_1_5_nm_from_every_other_along_its_whole_route`
  (`tests/cmake/server_kept_apart.cmake`): the run above. Separation lost
  for no step of any pair, nothing wrecked, both planned aircraft round
  their orbits. Coverage asserted: six pairs for four aircraft, each
  aircraft in three, the server's measurement over all 108,000 steps, and
  each pair over every step both flew - at least 108,000 less three minutes
  (the second planned departs at 90 s: 97,201 steps). 627 s here; its
  timeout is 3,000 s.
- `the_monitor_holds_an_aircraft_that_gives_way_off_the_heights_of_those_it_gives_way_to_and_no_other`:
  ten rules built and checked - below held below, above held above, far and
  parting nothing, far but meeting head on held already, a person's
  aircraft given way to and given nothing, level by the side of its height,
  not pushed into the ground, a climbing aircraft's lookahead, squeezed
  between two, and a later aircraft clear of the height an earlier one is
  held to - and it says it walked ten of ten.
- `a_cessna_climbing_through_another_or_meeting_one_head_on_is_kept_apart_by_the_monitor_and_without_it_is_not`:
  two C172Ps in JSBSim, each case flown without and with the monitor for
  240 s. Without, each loses separation (85.8 s climbing through the height
  of one 1 km ahead, 51.5 s head on), which shows the situation is built;
  with it, at least 700 and 680 ft apart within 1.5 nm, never lost.
- `an_autopilot_holding_a_climb_stops_at_its_ceiling_and_one_holding_a_descent_at_its_floor`:
  a C172P told to climb, or descend, at 700 ft a minute with no height to
  fly to stops at a ceiling 500 ft up (3,499 to 3,500 ft over the last
  minute of three) or a floor 500 ft down (2,500 to 2,501 ft); with no
  limit it goes on (4,345 to 5,045 ft, 935 to 1,635 ft). Seen to fail with
  the ceiling ignored: "climbing, a ceiling 500 ft up: held at its limit".
- `each_ai_aircraft_is_planned_...` and `a_client_hears_where_every_aircraft_is`
  changed for the new layers (above).
- From the review: a glide no longer gives way (it was given a floor it
  could only climb to by losing speed); an aircraft left by its player flies
  the plan file on a layer of its own (it flew AI 1's heights); the rules
  test's count of rules walked, which could not fail, is gone.
- Built on Windows (`tools/windows_build.sh windows-debug`), where both unit
  tests pass. The first push broke CI's Windows builds: the monitor's
  nearness test was called `near`, which Windows' headers define as nothing.

**Seen to fail.** Each run and reverted:
- the monitor taken out (`keep_apart` doing nothing), layers kept: the
  run test failed - AI 1's orbit at 4,000 ft and AI 3's tour at 4,500 ft
  came within 456 ft inside 1.5 nm, separation lost for 13.8 s from 333 s;
- and with the planned aircraft stacked upwards again too, which is the
  climb-through of the tail, the second's climb through the first's orbit
  height came within 556 ft - on 1,000 ft layers it is not lost, so the
  climb-through alone does not fail this run; the monitor taken out does;
- the old code itself, measured (the before column), lost separation for
  1,125 s in all;
- the limit not passed from the controller to the autopilot: the flight
  test failed, "climbing through ...: with the monitor, never within the
  minimum" - 0 ft, lost for 85.8 s;
- the sides swapped (above for below): the rules test failed on its first
  rule.
A first version chose the side from which way the aircraft was going, and
held off it went one way and then the other: head on, the monitor dithered
between above and below for the whole pass and kept nothing apart. The side
is now the one it is on, or that of the height it is flying to.
