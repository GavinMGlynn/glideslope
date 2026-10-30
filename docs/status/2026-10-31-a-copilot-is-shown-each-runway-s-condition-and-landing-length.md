### A copilot and a planner are shown each runway's condition, landing length and need, and every recording made again - 2026-10-31

**What is still not true first.** The F-22A has no landing figure: she has
no approach speed (sim::publishes_approach_speed, false for her and the
747-400), so every landing is refused her and a model is told "the aircraft
does not land" on every runway. No figure for her is published or measured,
so "A runway too short refused for the five" stays open for her alone.

**What now works.** Every runway a model may land on - the copilot's
"Runways nearby" and the planner's "Runways it may land on" - is followed,
after a semicolon, by what `copilot::landing_refusal` will judge
(`copilot::runway_told`, the same sizing, shared through one helper):

- its condition: "reported at runway condition code N, braking action ..."
  where a runway-state group reports it (the braking action the FAA's
  Runway Condition Assessment Matrix gives the code, AC 91-79B), "wet"
  within the weather's wet radius, "dry" otherwise; code 0 is "closed";
- the metres it has to land on, its length less any displaced threshold;
- what the aircraft needs there, dry, wet or by AC 91-79B's factor for the
  code, and "too short" where it is.

An aircraft with no approach speed is told it does not land; the flying
boat, with no need, is told the length alone. The runway line to copy is
unchanged, so a model's `land` line is read as before.

**Recordings made again** (owner, 2026-10-31): every recording in
`tests/data/copilot` - 15, not the 13 the items said - asked live of Claude
Haiku 4.5 and GPT-5.5 by the `live` tests with `GLIDESLOPE_LIVE_MODEL=1`,
all 15 passing live, then played back. Keys read from `~/.config/glideslope`
at run time; none is in any recording (each key's first 24 characters and
`sk-`, `x-api-key`, `Bearer` searched for: none). One answer is written in by
hand, as before: `take_back-server-anthropic.jsonl`'s second, where Haiku
answered `keep` in all ten live runs, which tests nothing; it is the route
the old recording had, with the same "note" line
(tests/cmake/server_copilot.cmake says why).

**Verification.**

- `a_copilot_and_a_planner_are_told_a_runway_is_reported_icy_and_what_the_aircraft_needs_on_it`:
  the session of R29C/7901// (ice) on a 1,000 m runway; both the copilot's
  situation and the planner's request end its line "; reported at runway
  condition code 1, braking action poor, 1000 m to land on, the aircraft
  needs 1990 m: too short", and without the group "; dry ... needs 558 m".
- `every_aircraft_is_told_each_runways_condition_landing_length_and_need_as_its_landing_is_judged`:
  16 aircraft times codes 0 to 6 and unreported dry and wet, a metre short
  of the refused need and a metre over, on a runway with 200 m displaced:
  288 of 288 told exactly, "too short" where and only where refused.
- Both seen red: "too short" misspelt, and the planner given no dry need,
  each turned them red, and was reverted.
- The full linux-release suite (`ctest -j4`), with the new recordings
  played back: 1,416 of 1,419 passed. Of the three, the registration check
  was right - the two tests had not been listed, and are now - and the
  spiral hand-over and the learnt landing on a server, both multi-process,
  passed run again alone (load, not this change).
