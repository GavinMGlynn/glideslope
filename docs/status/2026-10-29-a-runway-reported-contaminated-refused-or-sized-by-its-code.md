### A copilot's or planner's landing on a runway reported contaminated is sized by AC 91-79B's factors for its code, and refused at nil braking, 2026-10-29 — item still open

**What is still missing, first.** **The model is not told the runway's
condition, so the item stays open.** What it is shown is unchanged,
because a change there changes every copilot and planner prompt, and all
13 recordings would have to be made again with the owner's keys - as the
runway-length item's prompt change waits too. It learns of a contaminated
runway only when its landing on one is refused and the refusal, naming the
code, is told back to it. **The factors are the FAA's
generic ones**, not any maker's: AC 91-79B uses them where the maker gives
no advisory data, and no aircraft here publishes landing distances on snow,
slush or ice. **No turboprop row is used**, since there is no turboprop
here, and **codes 6 and 5 are not sized from the table**: they keep the
existing dry and wet rule (her factored dry need, and 1.15 times it or her
own wet figure). That rule asks less than table 3-1 does. A C172P needs
1.43 times her unfactored 390 m dry (the AC gives 1.67) and 1.64 times it
wet (the AC gives 2.6 for a smooth runway), then 2.8 times at code 4. So
the need jumps between codes 5 and 4. It is left because moving codes 6
and 5 onto the table would change every dry and wet refusal in the
recorded conversations, and so the 13 recordings, for a rule this item
did not ask about. **The four measured landing distances count an air
distance twice**: the B-2A's, F-35B's, J-3 Cub's and Mosquito's are the
simulation's own landings, threshold to stop, flown at a glidepath aimed
300 m in, not flight-manual figures. Table 3-1's factors allow for an
operational air distance on top, so for those four the contaminated need
is larger than the AC intends, which errs safe. **Code 0 never arrives from a report**:
WMO's braking-action table stops at 91 (poor), and SNOCLO is laid as code 1
(2026-10-14), so nil is refused only when something reports it, which
nothing yet does. **The server's AI planners are told what the METAR says
when the server starts**, laid on the station's runways from the runway
file before they plan; a report that changes later is not told to a plan
already made.

**Source.** FAA AC 91-79B, *Aircraft Landing Performance and Runway
Excursion Mitigation* (28 August 2023), table 3-1, *Landing Distance
Factors*: multiply the unfactored AFM dry landing distance by the factor for
the runway condition code at the time of arrival. The factors include a 15
per cent margin, an operational air distance, temperature, a faster
approach, poorer braking and whether reverse thrust is used:

| code | 6 | 5 grooved | 5 smooth | 4 | 3 | 2 | 1 |
|---|---|---|---|---|---|---|---|
| Turbojet, no reverse (and Reciprocating, figure for figure) | 1.67 | 2.3 | 2.6 | 2.8 | 3.2 | 4.0 | 5.1 |
| Turbojet, with reverse | 1.67 | 1.92 | 2.2 | 2.3 | 2.5 | 2.9 | 3.4 |

Paragraph 4.8.1.4: "A report of nil braking action on a runway requires the
closure of that runway." Code 0 is refused, and nothing is sized for it.

**What it does.** `copilot::landing_refusal`, the check a copilot's route
and a planner's plan both pass after the model answers, finds the condition
reported on the runway it lands on (`world::reported_condition`: the most
slippery code whose runway the middle of the landing end lies on, from
either end) and takes the more slippery of that and the weather's wet.
Code 0 is refused at any length, to every aircraft. Code 6 needs the dry
need and 5 the wet, as before. Codes 4 to 1 need the aircraft's unfactored
dry landing distance times the code's factor, or the wet need where that is
more. The unfactored distance (`sim::landing_distance_m`) is her figures'
`<landing_distance>` with no margin: over a 50 ft obstacle, or measured, as
given, and a landing field length times 0.6, since 14 CFR 121.195(b) made it
from the demonstrated distance over 0.6. The reverse row is used for the
five airliners whose catalogue entry gives `reversers` (737-300, 747-400,
787-8, A320, A380), which `sim::Lander` reverses at maximum on a runway that
is not dry, and the no-reverse row for every other aircraft. A C172P needs
558 m dry, 642 m wet and 1,990 m on ice (1,280 ft times 5.1). The refusal
names the code: "...needs 1990 m on a runway reported at runway condition
code 1: choose a longer one". The dry and wet refusals read exactly as they
did, so every recorded conversation plays back unchanged.

**Telling them.** `world::WetRunways` now carries the runways reported
contaminated alongside the wet radius, which is 0 where the weather wets
nothing. `frontend::wet_runways` fills it from the report's laid runways,
so the server's copilot checks see it, and so does the windowed client's,
which lays the server's `RUNWAY_CONDITIONS`. The CLI client lays what it is
told on the weather it was last told. The server's AI planners are told the
runways laid before they plan. `world::on_runway` and
`world::ContaminatedRunway` moved to `world/runways.hpp`, which the
copilot's build may see (`cmake/Copilot.cmake` names the three new names).

**Verification.**
`every_aircraft_is_refused_a_runway_reported_at_nil_braking_and_sized_by_ac_91_79b_on_every_other_code`
covers 16 aircraft and codes 0 to 6. It pins table 3-1's factors, written
in the test, and refuses every aircraft 20 km of runway at code 0. Each of
the 14 with a landing distance, at each of codes 1 to 6, is refused a
runway a metre short of the need the table gives, with the code named, and
given one a metre over, by the copilot's brief and the planner's request
alike. That is 98 judgements, counted and asserted against 14 × 7. The two
without a distance (the F-22A and the flying boat) are named and refused
only nil, and the five that reverse are named. Every runway is reported
from its other end.
`a_session_whose_metar_reports_a_runway_icy_refuses_a_landing_on_it_long_enough_wet`
lays `R29C/7901//` (ice) from a dry METAR on a 1,000 m YSBK 29C and
refuses the C172P there, through `frontend::wet_runways`, the copilot's
Situation and the planner's request. Without the group it is taken.
Neither test uses a model or a key. **Two paths are not tested**: the
CLI client laying the runways it is told on its last weather
(`frontend/cli/main.cpp`, `Heard::runway_conditions`), and the server
laying a METAR's states before its AI planners plan
(`frontend/server/main.cpp`, before the fleet is built). Both need a model
answer that lands on the icy runway. Making one needs a new recording with
a key, and a server test also needs the station's runways, so no cheap
test reaches them. Both feed `frontend::wet_runways` and
`landing_refusal`, which the tests above cover.

**Seen red.** With the new refusal taken out of `landing_refusal` (code 0
not refused, and codes 4 to 1 judged as wet), both tests failed: the walk
at "737-300 is refused 20 km of runway at nil braking: """, and the icy
session with the runway taken by the copilot and the planner alike. With
only the sizing taken out, the walk failed at "737-300 at code 1 needs
2860.2 m: a metre short """. Restored, both pass, with the 174 other
runway, landing, copilot, planner, METAR and wet tests (`ctest -R`).
