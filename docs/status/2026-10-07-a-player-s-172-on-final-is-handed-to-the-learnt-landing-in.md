### A player's 172 on final is handed to the learnt landing in a session; protocol version 8, 2026-10-07 — item still open

**What it is not, first.** **The server's own AI aircraft are never landed by
it**: nothing in a plan or a model's route asks for the learnt landing, and
no AI aircraft flies a final approach to be handed over at - the item's
verification asks for that and it is not built. **The window client cannot
fly a 172 into the gate from the keyboard**: the keyboard has no flap key,
and a window client joining starts its controls with the flaps up whatever
the server's aircraft has - started on final with full flap, its own inputs
ran them in, and asked two seconds after joining it was refused, "YSSY
16R: flaps at 33%; the gate is the landing flap, 100%" (a window test,
written and dropped for it). With a stick's or a quadrant's flap lever it
can; **so its L and its HUD's FLYING AI LEARNT LANDING are seen by no
test** - only the command-line client's asking is tested end to end. The
client does not predict its
aircraft while the learnt landing has it (as with any AI), and the gate's
runway is the server's to find; the client is told only that it was handed
over.

**A client refused the learnt landing is not told why**: the server says
why in its log, and the client hears nothing - no refusal reason travels.

**From the review of #119.** The gate was the held-out starts' box with the
training's airspeeds, but the held-out starts were all at the reference
speed, and the gate ignored the wind. Now every one of the box's 32 corners -
all five limits at their edges at once - is flown in calm air, 15 kt across
from either side, the most headwind and 5 kt behind, through
`LearntLander`: with 15 kt ahead all 32 corners bounced (7.0 to 7.5 ft back
into the air, past the 3 ft limit); with 10 kt ahead one did (3.008 ft, 1.6
miles out, 60 m right, 20 m low, 5 degrees right, 57 kt); with 8 kt ahead all
160 land - worst 3.90 m across and 277 ft/min - and stop on the runway. So
the gate admits the wind as the instruments estimate it - the policy's own
estimate - to 15 kt across, 8 ahead and 5 behind, asks it before the
aeroplane's place, and holds the landing flap to within 1% (out, not
moving) rather than 5%. A crab into a crosswind of more than 5 degrees is
refused by the heading limit, as the policy was never started so.
`the_learnt_policy_lands_within_its_limits_from_every_corner_of_its_gate_in_every_wind_it_admits`
(160 landings, counted) was seen red with the headwind limit at 15 and at
10; the gate test now walks sixteen refusals, the wind three of them and
the flaps 3% short one. A stray table header in TRANSPORT.md is gone.

**What works.** A player whose aeroplane has a learnt landing (today the
C172P alone) asks for it - `CONTROLLER_SWAP` to the new `LEARNT_LANDING`,
`03`; L in the window client, `--learnt-landing-after S` for tests;
`glideslope_cli connect --learnt-landing-at S [--until-landed]` - and the
server hands it to the policy only at its gate on final to a runway of the
world's: `sim::LearntGate`, the CLI's start at its middle and the policy's
held-out starts' room around it - 1.6 to 2.4 miles before the threshold,
within 60 m of the centreline, 20 m of the glidepath and 5 degrees of the
runway's heading, from 3 kt under the reference speed to 8 over, with the
landing flap out, in no more than 15 kt across, 8 ahead and 5 behind. `world::learnt_gate_runway` walks every end of every runway
in the strips the collision ground is made from, takes the threshold's
elevation from that ground, and says why not for the end she is most nearly
lined up on ("YSSY 16R: 2.9 miles out; the gate is 1.6 to 2.4 miles out").
The server says why it refused in its log; honoured, it announces the swap
to every client as `LEARNT_LANDING` (a state update still says `AI`, and a
`LEARNT_LANDING` anywhere else makes the message unreadable), flies her
through the same `sim::Controller::to_ai_learnt_approach` the CLI does, and
says where she touched and stopped. Taking her back is the ordinary
take-back, no step. `sim::learnt_landing(data, model)` is how every caller
knows an aircraft has one (the CLI's `land --learnt` too).
`--players-on-final AIRPORT/RUNWAY` starts players on final, trimmed at
their approach speed with the landing flap, the first two miles out and
each after half a mile further - how a test puts a player at the gate.

**Protocol version 8** (7, TAKE_OVER_REFUSED, landed first): the same ground; a `CONTROLLER_SWAP` may carry
`LEARNT_LANDING`. TRANSPORT.md says what the server honours, byte for byte;
the document-only client speaks 08.

**Verification run** (linux-release, the targeted suites):
- `a_players_c172p_handed_to_the_learnt_landing_on_final_is_landed_by_the_server_and_a_c182_and_one_outside_the_gate_are_refused`:
  three clients on a server with players on final to YSSY 16R; the first's
  C172P handed over two miles out and landed - 214 ft/min, +1.37 m across,
  stopped 462 m along, -2.51 m across - the client leaving once two
  simulated seconds at rest; a C182S refused for having none, and a C172P
  three miles out refused, "YSSY 16R: 2.9 miles out". Seen red with the
  server's refusal unsaid.
- `the_learnt_landing_is_offered_for_every_aircraft_that_has_one_and_for_no_other`:
  16 of 16 catalogue aircraft walked, the C172P alone offered; every policy
  file is a catalogue aircraft's. Seen red with every model given the
  C172P's file.
- `an_aeroplane_is_at_the_learnt_landings_gate_only_inside_it_and_is_told_why_not`:
  three starts inside (the CLI's and two opposite corners), twelve refusals
  (each limit either way, the flaps, another aeroplane), counted. Seen red
  with the flap check dropped.
- `a_c172p_on_final_to_a_runway_of_the_worlds_is_found_at_its_gate_and_landed_on_it`:
  YSSY 16R (167.9 true) and 34L opposite; 16R named four and 2.6 miles out,
  nothing in mid-Pacific; found at the gate and landed, 212 ft/min, +1.44 m.
  Seen red with the `he_` end's heading not turned.
- `an_aeroplane_taken_back_from_the_learnt_landing_moves_no_control_more_than_a_hand_in_a_step`:
  a gap of 1.0 closed at 0.00833 a step. Seen red with the take-back jumping.
- The message, protocol-version, document and server-flag tests, updated.
