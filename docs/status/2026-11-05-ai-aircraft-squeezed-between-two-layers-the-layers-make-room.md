### An AI aircraft squeezed between two layers: the layers make room; and one flying its copilot's route is measured in a run, 2026-11-05 — item done

**What was missing.** On 2026-10-09 one handed to the AI between two
layers 1,000 ft apart, inside 1.5 nm of the lower, was turned away and held
in the middle of the gap. No height in a 1,000 ft gap is more than 500 ft
from both layers, so it stayed under the minimum until it had turned
1.5 nm away: 38.9 s. Nothing moved the layers. And no run measured a
handed aircraft flying a copilot's route.

**Researched first.** Searches:
- FAA JO 7110.65: radar minima 5-5-4 (3 nm terminal within 40 nm of the
  antenna, 5 nm beyond), vertical 4-5-1 (1,000 ft to FL410), vectoring
  5-6-1, and holding 4-6;
- ICAO Doc 4444: 1,000 ft vertical, 5 nm surveillance, 3 nm where the
  radar allows (8.7.3);
- holding stacks: Wikipedia's *Holding (aeronautics)*, IVAO's stack
  management, the Gatwick and Albanian AIP stack pages. Stacks are
  1,000 ft apart, a newcomer is put at the top, the bottom leaves first
  and the stack moves down a level;
- FlightGear's AI traffic and TCAS wiki pages: ground-network separation
  only, and a TCAS II v7 instrument that sees AI traffic but does not
  coordinate with it;
- X-Plane's TCAS API for AI and plugin traffic (12.4.1 on);
- MSFS's AI and the add-ons that fix it: AIFlow spaces arrivals by speed,
  then by holds, and BeyondATC sends traffic around.

faa.gov and SKYbrary refused the fetch (403), so the paragraph numbers come
from search extracts and earlier editions. The minimum is unchanged: 500 ft
or 1.5 nm, JO 7110.65 7-9-4 (sim/separation.hpp, decided 2026-10-02).

**What was chosen, and why.** A level change before entry is what the
layers already are: stacked 1,000 ft apart, and planned departures stacked
downwards. Delaying entry does not fit, because this aircraft is not
entering anything: it is handed over already inside. Vectoring is what
the 2026-10-09 rule does, and it alone is the 38.9 s. What remains is to
move the other two. The nearer precedent is a controller's safety alert
(JO 7110.65 2-1-6), which may go to either aircraft, and which the
2026-10-09 rule already follows for the turn. The move is TCAS-like:
TCAS II gives two aircraft complementary advisories, one climbing and one
descending. A stack moving down a level, once the bottom has left, is the
same kind of move.

**What changed** (`sim::separate`, `sim/separation.cpp`):
- **The layers make room.** An aircraft that is squeezed and not yet clear
  is held in the middle of the gap, as before. Now every AI aircraft it
  gives way to there that can be moved (one that gives way itself, its
  autopilot flying) is moved away from that middle: the one below to
  700 ft under it (the minimum and margin), the one above to 700 ft over
  it. A person's aircraft, or one taking off, is not moved. Nor is a layer
  that the move would take within 500 ft of the ground, or whose own limits
  it would cross.
- **Worked out twice.** The first pass finds who is squeezed. The second
  puts the room on the layers before the aircraft after them are worked
  out, so those keep clear of where the layers are going, and keeps the
  side, heading and middle the first pass chose.
- **The middle is latched** (`Squeeze::middle_ft`), so it does not drift
  as the layers move. The squeeze also stays latched while the aircraft is
  not yet clear, even once the gap is wide enough that its floor and
  ceiling no longer cross. Otherwise the room would be let go, the layers
  would close, and it would be squeezed again.
- **Only while there is still one on each side** (review of #337, (a)).
  At first the latch held it in the middle while anything at all was near.
  A gap opened because a layer had gone (landed, left, or out of the
  guard) still kept it held and turned away from nothing. Now the latch
  needs one near below it and one near above. Otherwise it is let go, and
  flies its ordinary limits.
- **The middle follows those that cannot make room** (review of #337, (b)).
  At first the latched middle stayed put while a person's aircraft below
  climbed, so the held height could sit inside that person's minimum. Now
  the middle is kept the minimum and margin from every aircraft near that
  cannot be moved: a person's, one taking off, or a layer the move would
  take within 500 ft of its ground. Between two that cannot be moved, it
  is held in the middle of what they leave. And the side a layer makes
  room on is its side of the squeezed aircraft, not of the middle, which
  may have followed an aircraft that cannot move past one that can.
- Once clear (1.5 nm from every layer it would pass through), it is
  taken above them all or below, as before, and the layers fly their own
  heights again.

**Verification.**
- `an_aircraft_handed_to_the_ai_between_two_layers_is_turned_away_and_kept_apart_from_both_after_its_arrival`
  (three C172Ps in JSBSim, built as on 2026-10-09, 36,000 of 36,000
  steps): separation from the lower layer is now lost for **12.5 s**, not
  38.9, and never after 12.5 s. Never lost from the higher (least 600 ft).
  The lower went down to 2,826 ft. Controls moved at most 0.0083 a step.
  New bound: 14 s, and none after 14 s. The lower must also have gone down
  at least 150 ft. **Not zero, and cannot be**: it is handed over 400 ft
  from the lower, inside the minimum, and is out of it only once the gap
  has opened the last 100 ft. That takes the lower's descent and its own
  climb, each started from level. **Seen to fail** with the room taken out
  (`separate` returning its first pass): lost 38.9 s, the lower only down
  to 2,997 ft, "the lower layer made room" red.
- `the_monitor_holds_..._and_no_other` asserts these:
  - squeezed, the one below is held 700 ft under the middle and the one
    above 700 ft over it, each clear of the squeezed one, and the middle
    is latched;
  - clear of the one it passes, no room is made;
  - once the room is made, it stays held in the middle and the layers stay
    apart;
  - a person's aircraft below is not moved, the middle is kept 700 ft over
    it, at 3,700, and the AI's above makes room from there, to 4,400;
  - a layer that would be taken within 500 ft of its ground is not taken
    lower, and the middle and the other's room follow it in the same way;
  - (b) the person's aircraft then climbs 200 ft: the middle follows it to
    3,900, and the room above goes to 4,600;
  - (a) latched, and then the lower layer is gone: it is let go, not
    turned, and held only below the one above, at 3,100.

  **Seen to fail**, each with its own fix taken out:
  - the room taken out: "the one below makes room";
  - the middle not following once latched: "the person's aircraft climbs
    200 ft: the middle follows it";
  - the old latch: "the lower layer gone, it is let go of the squeeze";
  - the middle following nothing: "a person's aircraft is not moved".
- **A handed aircraft flying a copilot's route, measured in a run**:
  `an_aircraft_flying_its_copilots_route_is_measured_against_every_other_ai_aircraft_and_kept_apart_from_them`
  (`server_handed_kept_apart.cmake`, `-DCOPILOT`). The client's copilot,
  played back with no key from `manly-server-anthropic.jsonl`, sends its
  route to Manly. The server hands the aircraft to the AI, says it flies
  the copilot's route, and measures it for 3,600 steps against two
  plan-file AI aircraft.

  **Coverage asserted by kind**: three pairs, one plan file + plan file
  and two copilot + plan file, each pair with the copilot's measured over
  exactly 3,600 steps. Measured: the copilot's came within 720 m of AI 2
  and 994 m of AI 1, at least 1,611 and 2,604 ft apart in height. The two
  plan-file aircraft came within 303 m, at least 993 ft apart. Separation
  was lost for 0 s, every pair.

  **Seen to fail** with a copilot-flown aircraft left out of the
  measurement: "1 pairs were measured, not the 3".
- **The copilot's against model-planned aircraft too** (review of #337):
  `an_aircraft_flying_its_copilots_route_is_measured_against_model_planned_and_plan_file_ai_aircraft_over_a_whole_run`
  (`-DPLANNED`, TIMEOUT 1500). The setup:
  - four AI aircraft: two planned from the recordings, taking off from 16R
    90 s apart, and two on the plan file;
  - the copilot's aircraft as above;
  - 30,000 steps of the hand-over, 267 s of real time.

  **Ten pairs, each kind counted**: planned + planned 1, plan file +
  planned 4, plan file + plan file 1, copilot + planned 2, copilot + plan
  file 2. Every pair with the copilot's was measured over 30,000 steps, but
  the one with the second planned aircraft, measured from its take-off
  (21,001 steps; at least 30,000 less 90 s asserted). Separation was lost
  for 0 s, every pair. **What it shows, honestly**: the copilot's route goes
  to Manly and the planned aircraft orbit the CBD, so the copilot's came
  within 1.5 nm of neither (8,972 and 7,487 m at closest). It shows those
  pairs are measured over the whole run, not that the rule was tested
  between them. Against the plan-file aircraft it did come close: 721 and
  461 m, at least 1,751 and 753 ft apart.
- **Every route kind, each in a whole run, separation lost for 0 s**,
  linux-release:
  - planned + planned, planned + plan file, plan file + plan file: the
    four-aircraft run, 108,000 steps, 6 pairs. Least within 1.5 nm 720 ft
    (AI 1 and AI 3); AI 1 and AI 2 at least 916 ft;
  - handed and holding its course: 3 pairs, 3,600 steps each with the
    handed one, least 1,991 ft;
  - handed in a spiral over two layers: 3 pairs, 14,400 steps. AI 2 and
    the handed one at least 995 ft, AI 1 and AI 2 at least 700 ft;
  - handed and flying its copilot's route: above;
  - and two landing on one runway, and two wrecked together flying again.
- **The wider run**, linux-release, `-R "server|handed|hand_over|copilot|separation|monitor|selftest|apart|layer" -LE live`,
  246 tests, held to 30 minutes: 225 run and every one passed but one
  skipped (the server window's dashboard, no display). The 21 not reached
  were the F-35B and STOVL prediction tests through a server, which fly a
  player's aircraft, which the monitor gives no limit; CI runs them.

**The owner's ruling, 2026-11-05: a conflict an aircraft inherits at a
hand-over is judged from the hand-over.** The verification now reads:
"every AI aircraft a server runs, planned or not, keeps a stated distance
from every other along the whole of its route, measured over a whole run;
one handed over already inside the minimum is out of it within 14 s and
never under it again". The tests assert exactly that:
- the between-layers case: lost at most 14 s in all, and never after the
  first 14 s (12.5 s measured);
- every run: 0 s for every pair.

So the item is ticked. Nothing keeps a person's aircraft clear: it is
given way to, not measured.

**A player joining is now put above every plan-file AI aircraft's layer**
(fix after CI, `src/frontend/server/main.cpp`). The copilot-against-planned
run failed on every CI platform with "the client never heard its aircraft
handed to the AI": its own aircraft and aircraft 5 were wrecks at once
(#337's run 41961819690, train #338's run 41971152181). The cause was
this:
- a joining player was put at the plan's start, one layer over as many
  aircraft as were in the sky;
- the plan-file aircraft are stacked by their AI number, so with four AI
  aircraft the two on the plan file are AI 3 and 4, at +2,000 and
  +3,000 ft;
- on a debug build the client joined at 0.07 s, before the first planned
  aircraft was away, with two in the sky. That put it at +3,000 ft, on
  AI 4, at the same place, and both were wrecks at once;
- release builds here joined at 0.57 s, after the first had gone, so the
  player went to +4,000 ft and the run passed.

Reproduced here in linux-debug on the old code: "aircraft 5 is a wreck,
aircraft 0 is a wreck", word for word as on CI. Now the player goes one
layer over the more of the aircraft in the sky and the plan-file layers
(`plan_layers_`). And the test now fails at once, saying so, when the
client hears a wreck.

The selftest hash does not move: the selftest flies the test pilot, which
the monitor does not touch. The new tests' entries in `tests/ci_costs/` are
estimates until CI's own replace them: the hand-over test's time plus 15 s,
and for the run against planned aircraft this machine's 267 s and a half
again.

After the review's changes, linux-release: the twelve separation and
kept-apart tests (the monitor's rules, the between-layers case, the
climb-through and head-on, the four-aircraft run, the handed, spiral and
both copilot runs, two on one runway, two wrecked together) and the
port-uniqueness test all pass. The between-layers figure is unchanged at
12.5 s.
