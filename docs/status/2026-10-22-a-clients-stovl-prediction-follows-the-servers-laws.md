### A client's STOVL prediction follows the server's flight-control laws, 2026-10-22

**What is still not true first.** A client joining while her law already
flies her - mid-bank jet-borne, or hovering - is not covered: its
prediction begins on a model just loaded, the fan's doors shut, so the law
it makes is wing-borne whatever the server's is. Telling the law's state
alone would not put that right; the lift system's positions and the law's
stage would have to be told at the join. That item stays open
("A client joining while an F-35B banks jet-borne...").

**What was open.** Since 2026-10-16 the server tells the law's height
asked and its wing-borne autopilot's pitch integral. Not shown: selection
before the prediction has settled; and the converting, jet-borne and hover
laws' own integrals were not told.

**Measured: the other laws need no telling.** In one process, the F-35B at
her vertical-landing loading from 150 ft and 200 KCAS, the server's law
made a step early, on the step, a step late and two late, selected at 0.5 s
and 3 s, flown 130 s through converting, jet-borne (the stick back, right,
left, the pedals, forward) and the hover (the stick right, the pedals, the
stick back). With what the word carries now: the median at most 0.4 mm, the
worst 6 mm, the pitch within 0.012 degrees in every stage. With all 39
numbers of the transition and hover laws told besides (their integrals,
heading, bank held, speeds and attitudes reckoned from, the point held, the
last commands - a search harness, not kept): 0.007 degrees and under 0.05
mm. Nothing a bound could tell apart, so nothing more goes on the wire. The
reason: each later law is seeded at its stage's change from the controls the
law before it left, and once both sides change stage on the same step
(this branch's first commit: the replay keeps the lift system's commands)
those agree. Before that fix the same flight gave a median of 16.8 mm and
the pitch 0.377 degrees off jet-borne.

**The tests.**

- `a_prediction_whose_stovl_law_was_made_a_step_or_two_apart_agrees_with_the_server_converting_jet_borne_and_hovering`
  (tests/unit/test_prediction.cpp): the flight above, eight cases counted,
  each with at least 150 words compared converting, 2,000 jet-borne and 500
  hovering; the median within 1 mm and the pitch within half a degree at
  every word in every stage. **Seen red** with the lift system's commands not
  kept by a replay (first case: median 16.8 mm, pitch 0.377 degrees). Not
  red with the pitch integral not carried: from 200 KCAS she converts at
  once and the wing-borne autopilot has no time to wander; the 2026-10-16
  wing-borne test pins that.
- `a_client_selecting_stovl_on_its_first_input_seeds_its_transition_law_with_the_servers_throttle_integral`
  (tests/cmake/server_stovl_at_once.cmake), through a real server and a
  100 ms relay: the selector on her very first input, before any word has
  placed it on the client's clock, now held for 3,000 updates (two minutes
  wing-borne, she is too heavy to hover) with her predicted pitch within half
  a degree of the server's at every update. Measured: 0.004 degrees. Run
  serially now, as server_stovl_landing.cmake is. **Seen red** with the
  prediction's pitch integral put a degree off the server's: 1.730 degrees.
  (Not red with the pitch integral not carried, nor with the levers the
  server held her on not told: 0.227 and 0.024 degrees - selected on the
  first input both laws are seeded alike, #232's work.)
- Settled selection through a real server is server_stovl_landing.cmake's
  (0.154 degrees, 2026-10-16), unchanged.

**Verification** (linux-release): both tests green; the stick test, the
2026-10-16 wing-borne test and the unit-test registration check green.
