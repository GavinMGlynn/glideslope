### A person in the circuit is told to leave it for an AI with nowhere to divert, 2026-11-11

**What it does not do, first.**
- **Nothing makes the person leave.** She is told, in words, and may
  ignore them. If she stays, the AI waits until her fuel says go and then
  comes back with priority whatever the person does: behind a person's
  C172P who stays, 14 of 15 landplanes lose the minimum (all but the
  F-15C), and four do not land - the A320 and the A380 run their tanks dry
  and strike the ground short, and the 787-8 and the B-2A are not down by
  the run's end (2 h). The reserve she goes on - ten minutes at what she
  burns then, and her way back at her reference speed - is not enough for
  an airliner sent round behind a person still in the circuit.
- **The server's telling is not flown end to end.** The server asks every
  AI it flies each step, tells each person in that runway's circuit once
  (`TRAFFIC_INSTRUCTION`), and her client prints it and shows it on the HUD
  for 30 s; the tests fly the controller and the choice of who is told
  (`sim::persons_in_her_circuit`, the server's own), and the message
  byte for byte, but no multi-process test builds a hold out of reach on a
  server.
- **Only a left-hand circuit's dead side.** She is told to leave 45 degrees
  right of the runway's heading - away from a left-hand circuit, from the
  holds beside it and from the approach. Circuits here are all left-hand.
- **She is not told when to rejoin**, beyond "when she has landed".

**What it does.**
- **With nowhere to divert, she waits for the circuit to be cleared**
  (`Controller::asks_the_circuit_cleared`). Come down where she is, she
  circles at the height she left her circuit at while a person is ahead of
  her in her runway's sequence, and asks. She comes back with priority once
  the person's place is dropped (17 nm out, `sim::left_the_circuit_by_hand`),
  or once what she has lasts less than ten minutes and her way back to the
  threshold at her reference speed (`Controller::fuel_back_s`: ICAO's MAYDAY
  FUEL, her fuel on landing below the reserve). Short of fuel already, she
  comes back at once and still asks until she is down.
- **The server tells the person** (`Fleet::clear_circuits`): each person
  with a place in that runway's circuit, ahead of her or behind, once for
  each asking: "leave the circuit on the dead side, heading 205 true;
  rejoin when aircraft 3 (737-300), with priority, has landed on 16R"
  (`sim::leave_the_circuit_for`).
- **Protocol 17 (`11`): `TRAFFIC_INSTRUCTION` (`0E`)** - her aircraft's
  number and the words, at most 160 bytes of printable ASCII, written as a
  learnt landing's refusal is. TRANSPORT.md byte for byte, THREATS.md
  names it; the window client prints it and shows it on the HUD for 30 s,
  the CLI's `--heard` says it.

**Researched first** (owner's standing rule). Searched: JO 7110.65 2-1-4
operational priority (pointsixtyfive's copy; the FAA's pages refused, 403);
10-1 and 10-2 emergency handling and "provide maximum assistance"; 3-8-1
sequence and spacing and its phraseology (LA ARTCC's spacing and
sequencing guide: EXTEND DOWNWIND / TURN BASE LEG NOW, MAKE LEFT/RIGHT
THREE-SIXTY, GO AROUND with instructions); minimum fuel against emergency
fuel (the FAA's flight service order, InFO 08004, PPRuNe's minimum fuel
thread); a 2026 Miami case in which a 777, sent round twice on minimum
fuel, declared a fuel emergency and was given priority, "another aircraft
moved out of the landing pattern"; CAP 413 and IVAO's orbit and circuit
phraseology; and whether simulators' AI talk to a person (X-Plane's ATC,
FSX's multiplayer tower, SayIntentions' text ATC; IVAO's text frequency).
- Minimum fuel asks no priority; an emergency (MAYDAY FUEL) has it, and an
  aircraft in distress is the one exception to first come, first served.
  Used: she waits with nowhere to go, and goes when her fuel on landing
  would fall below the reserve.
- Other traffic is moved for her: extending downwind and three-sixties
  delay a pattern aircraft but keep her beside the final - within 1.5 nm
  of it, which the item's minimum counts. So the instruction is to leave
  the circuit, to its dead side.
- Glideslope's transport had no message a person reads for other traffic:
  only a learnt landing's refusal, to the client that asked. Option (a)
  needed a new message; (b), another runway at the same airfield, was not
  in the test's world and leaves a crossing final; (c), a final timed for
  the gap, was measured last round and no release point works.

**Verification** (tests/unit/test_lesson.cpp, the two-going-around
harness; `Then::ahead_leaves_when_told`; release figures).
- `every_landplane_with_nowhere_to_divert_lands_kept_apart_from_a_person_who_leaves_the_circuit_when_told`
  (gust_sweep) and its every-run slowest and fastest: a person's C172P
  staying in the circuit, the one behind's hold out of reach and no other
  airfield. All 15 asked, the person was told and left, every one came
  back and landed on her own runway, kept apart: least 3,245 m within
  500 ft (the 747-400), 657 ft within 1.5 nm. 14 came back for the person
  leaving, waiting 737 to 823 s; the F-22A's fuel said go after 64 s, and
  she was still kept apart (14.3 km).
- `every_landplane_with_nowhere_to_divert_behind_a_person_who_stays_when_told_to_leave_waits_until_short_of_fuel_and_lands_measured`
  (gust_sweep; the old coming-back test): the person told, and staying.
  Her tanks are set as she first asks to last 300 s past what her fuel
  needs to go, so the wait is short. Every one waited (8 to 343 s) and
  came back on her fuel; the sets are asserted exactly - 14 lose the
  minimum (least 0 m and 0 ft; the F-15C kept apart, 658 ft), and four do
  not land (above).
- `a_person_in_her_runways_circuit_ahead_or_behind_is_told_to_leave_it_in_words_a_message_carries`:
  seven cases built, counted - a person ahead, behind, an AI's, another
  runway's, no place, hers none, two people and an AI's - and the longest
  words (the longest catalogue id, a 32-byte runway) 156 bytes.
- `every_refusal_the_document_names_for_a_traffic_instruction_is_refused`,
  and every message test walking fourteen kinds: written and read back,
  196 pairs never read as one another, filled to its limits, 32,768 loss
  patterns (a fifteenth bit for the fifteenth datagram), the document's
  table, THREATS.md's list.
- Item 1's every-landplane diversion test unchanged: 14 divert; the F-22A
  waits, her fuel says go after 64 s, and she lands - not kept apart, as
  named.

**Seen to fail.**
- The wait taken out (she comes back at once, as before): "2 lost the
  minimum, not the 0 named" - the F-15C 1,573 m and 0 ft, the Cub 1 m.
- `persons_in_her_circuit` telling an AI's place too: "an AI's ahead: not
  those told".
- The reader refusing `~`: "every printable byte reads".
- The fuel-says-go rule first as ten minutes alone (`short_of_fuel`):
  drained to it, the A320 and the A380 ran dry on the way back from where
  they waited.

**Times.** linux-release: the every-landplane leaving test 184 s, the
staying test 135 to 183 s (gust_sweep; about 2,600 s on debug at fourteen
times - 14,400 s timeouts, as their neighbours'); the slowest and fastest
leaving 39 s, and 205 s on linux-debug beside other tests (1,800 s). The
message tests under 4 s on debug.
