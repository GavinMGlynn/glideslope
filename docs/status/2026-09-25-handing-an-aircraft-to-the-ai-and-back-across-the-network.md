### Handing an aircraft to the AI and back across the network, 2026-09-25 — Phase 7's three open items done

**What is missing first.** All of this works in `glideslope_cli`. The client
with the window does not yet hand over on a server: pressing A there does
nothing online. That is a tail.

**What works.**
- **`CONTROLLER_SWAP` travels both ways**, keyed by aircraft number. A client
  asks, over the reliable layer, for its own aircraft to be handed to the AI
  pilot or given back. The server honours the request only for that client's
  own aircraft, and tells every client what it did and when
  (`Fleet::hand`).
- **The server side.** The aircraft keeps its number and slot. The AI holds
  what it is doing (`sim::Controller::to_ai`). While the AI flies, inputs are
  not applied, and state updates give the controller as `AI`. Given back, the
  controller brings the controls to the pilot's at a hand's pace, the same
  continuity the Phase 4 swap is held to in every phase of flight.
- **The client side** (`glideslope_cli connect --predict --hand-over-at S
  --take-back-at S`).
  - Handed over, the client stops predicting and draws its own aircraft from
    the updates like any other.
  - Taken back, it predicts again from the next update carrying its motion.
  - Either way, what it shows of its own aircraft is blended across over half
    a second.
  - Until the server has applied an input sent after the take-back, and for
    the second the controls take to meet the pilot's, prediction error is not
    compared, as when joining; those inputs are counted.

**Verified** by both network checks, at 100 and 200 ms with jitter, loss and
the relay's gaps. The predicting client hands its aircraft over 8 s in and
takes it back at 14 s. Each check requires:
- both swaps said by the server;
- the largest step in what the client showed at a switch under 5 m, where
  the second difference of its shown position is the step. Three runs in a
  row gave 0.97 to 1.88 m, against 3 to 7 m of ordinary correction;
- prediction error within its bound after the take-back, and inputs not
  compared, joining and taking back together, under 90 (48 to 61).

**What the review found**, fixed before the merge. A player's aircraft handed
to the AI and taken back keeps its controller, flying the player's inputs.
Crashed afterwards, it flew again with that controller taken for an AI's: it
was given to the AI, with no `CONTROLLER_SWAP` and its client never told, so
none of its inputs were applied again. Now a player's aircraft flies again as
whoever had it, and a controller left from a swap is dropped. This is held by
`an_aircraft_taken_back_from_the_ai_and_wrecked_flies_again_as_its_players`:
it hands over, takes back, dives into the sea (`--dive-after`) and flies
again, with every input applied. With the old test put back, 718 of 1,330
were applied, and it went red.

**Seen to fail.** With the blend removed, the step at a switch was 16.5 m and
the 200 ms check went red. Before a take-back was treated like joining, the
prediction error just after it reached 11 to 13 m and failed the 10 m bound:
the server's controls were still on their way to the pilot's.
