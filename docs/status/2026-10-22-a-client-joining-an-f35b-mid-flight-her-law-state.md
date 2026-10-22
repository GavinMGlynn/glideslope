### A client joining an F-35B mid-flight: her law's state, the prediction's side, 2026-10-22

**What is still not true first.** Nothing goes on the wire yet: the server
does not tell a joining client her law's state, so through a real server a
client joining her mid-bank or hovering still predicts her from a law made
afresh, its model's fan doors shut. What is done is the prediction's side
and the state the server will tell, shown in one process. The item stays
open.

**The cause.** A client joining her - joining again on its key, or taking
her over - builds its prediction on a model just loaded: the fan's doors
shut, the nozzle aft. Its law, made on its first STOVL input, finds the
doors shut and is made wing-borne whatever the server's is, and level where
the server's holds her pilot's bank.

**The change** (sim only):

- `Aircraft::lift_positions()` and `seat_lift_system(at, commands)`: the
  doors, nozzle and vanes put where the server's stand. The flight-control
  system is run once as a trim runs it, which lets the rate-limited
  actuators start from there. Then the commands go on.
- `Hover::State`, `Transition::State`: everything each law carries from step
  to step, and constructors that make the law again from it. A wing-borne
  autopilot is made again from the law's last controls with its pitch
  integral (the one number of its own measured to matter, 2026-10-16).
- `Controller::LawState` (the law's state, the height asked, the lift
  system's commands and positions, the lever latched at the selection),
  `law_state()` and `take_law()`.
- `Prediction::reconcile` takes an optional `LawState`. With it, the law is
  made again from it, her lift system is seated, and the steps since are
  flown again from there.

**The test.**
`a_client_joining_an_f35b_mid_bank_or_hovering_predicts_her_from_the_servers_law_state`
covers a client joining 40 s in, jet-borne with 20 degrees of bank held, and
125 s in, hovering. It is told the law with the server's first word and
compared from that word for 20 s: the median within 1 mm, her pitch and roll
within half a degree at every word. Measured: mid-bank the median 0.0 mm, the
pitch 0.008 and roll 0.014 degrees; hovering 0.0 mm and 0.005 degrees.
**Seen red** in four ways:

- not told: mid-bank 74 mm, pitch 0.88 and roll 0.96 degrees; hovering pitch
  0.97 degrees;
- the law told but the lift system not seated: 2.7 mm;
- the bank held told as nought: roll 0.89 degrees.

**Verification** (linux-release): the test green. The registration check and
the STOVL, transition, hover and replay tests are green as well.

**Next**: a reliable message carrying `LawState` to a client given an
aircraft a law flies (join, joining again, take-over). It would be its own
kind, because a full state update has 49 bytes to spare and the state is
about 160. TRANSPORT.md and THREATS.md need entries, and the protocol
version moves. Then the multi-process test: a second client on the same key
joining her mid-bank.
