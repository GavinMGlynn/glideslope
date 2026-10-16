### The F-35B's lift system reached by a pilot's STOVL selector and by an AI plan's vertical landing, 2026-10-14 — item open

**What is still missing, first.** **The item stays open**: her measured
rolling landing distance (9,200 ft) is not yet revisited now that a plan can
land her vertically, and **no multi-process test flies a player's STOVL
selector through a server**: the server gives such a player a controller
and the client's prediction flies her through one (below), and the two are
held together in one process, not over a socket. **Only the height is the stick's in STOVL
mode**: the left-hand inceptor's speed, the stick's sideways translation and
the pedals' heading in a hover, which the F-35B's "unified" control laws
give her pilot, are not; she slows to a hover where she stops and holds her
heading (a new Phase 9 item). **A plan given to the AI while she is in STOVL
mode is not flown**: the AI holds the hover she is in. **A vertical landing
hovers at 150 ft above the ground the flight model has**, not at a height
the plan gives. **Not built on Windows** before pushing: nothing here is
under `#ifdef` or touches sockets, windows or paths; CI compiles it.

**What it does.** `sim::Controls` has a nineteenth control, `stovl`, the
STOVL mode selector: 1 STOVL mode, 0 conventional flight. As in the
aeroplane, there is no nozzle or fan lever: Lockheed Martin's and Pratt &
Whitney's account of her control laws (Walker, Fuller and Wurth, "F-35B
Integrated Flight-Propulsion Control Development", AIAA 2013-4243, from its
abstract) has the stick command rate of climb or flight path and the
left-hand inceptor longitudinal acceleration, the flight-control computer
commanding the lift system. Here `sim::Controller` is that computer, through
the transition law:

- **The pilot's selector** at 1 converts her - wing-borne at her conversion
  speed first, if faster - and slows her to a hover at the height she had,
  no lower than her least conversion height; on the ground she stands
  converted. In STOVL mode the stick's fore-and-aft moves the height asked
  at the hover law's 10 ft/s up and 6 down; asked under 20 ft
  (`Controller::land_below_ft`) she lets down on the hover law's landing.
  The selector at 0 converts her back at her conversion speed, and once
  the fan has stopped the controls are the pilot's again at a hand's pace;
  on the ground at once.
- **A plan's vertical landing**, `land_vertically NAME LATITUDE LONGITUDE
  HOVER_S` after the last waypoint (sim/plan.hpp; refused after `land`,
  before it, twice, or with anything after it). The point is flown to as a
  last waypoint at the height and speed of the one before; from
  `Transition::stops_within_ft` of it (the distance she stops in at her own
  slowing rate from her speed, half again, and half a minute at her
  conversion speed) the transition law is asked for a hover over it at 150
  ft, holds it HOVER_S seconds and lets her down. An aeroplane without a
  lift system flies such a plan's last waypoint on. The copilot's grammar
  is not told of it: its requests do not move.
- **The transition law takes a point** (`Transition::Target::over`): she is
  steered at it wing-borne only - jet-borne her law flies speed along her
  heading and nothing across it, and steered there too she stopped 330 ft
  abeam and drifted off at 6 kt, never qualifying for the hover; jet-borne
  the speed asked is no more than she stops in by the distance to it along
  her heading; and the hover law holds over it (`Hover::hold_over`), not
  over where she stopped. Hovering, `Target::land` is passed to the hover
  law's landing. Without a point nothing it did before changes: every
  transition test is as it was.
- **Handed over**, the transition law goes with her: given to the AI in
  STOVL mode, the AI holds the hover at the height asked; taken back in it,
  she stays in STOVL mode until the pilot's selector moves from where it was.
- **On the wire**, protocol version `0C` (12): an input frame is nineteen
  controls, the selector last (docs/TRANSPORT.md, and the doc client). A
  peer on `0B` is refused at the envelope with `WRONG_VERSION`, as at every
  version change (server_gearstick's refusal now expects `474c44530c0401`).
- **The server**: a player whose selector reaches 0.5 in a lift-system
  aeroplane is given a controller (`controller_for`); every caller puts a
  controller's controls and lift system on the aircraft with `sim::apply`,
  which never throws for her envelope: STOVL mode asked above her cleared
  airspeed (`fcs/stovl-limit-kcas`, which the transition law never asks
  but a dive while converting could meet) is flown with the fan's doors
  closing and the rest as asked (`sim::put_lift_system`). THREATS.md says
  the selector builds a controller and is read as a threshold.
- **The client's prediction** (from the review of #190): `sim::Prediction`
  flew the client's controls raw, so a networked client's F-35B never
  converted while the server's did. An aircraft with a lift system is now
  predicted through a pilot's `Controller`, and its state - the controls
  applied, the mixture held, the transition law with its autopilot, hover
  law and their integrals, the height asked (`Controller::PilotsLaws`) -
  is kept with each step held and put back before the steps are flown
  again, by the motion's correction, the snapshot's and an adoption alike.
  `Transition` is copyable for it (its autopilot and hover law are held by
  value). Every other aircraft is predicted as before.
- **The height asked is the plan's hover's** while the AI flies a vertical
  landing, so a pilot taking her back in the hover holds it - before, the
  height she was at when the transition began, 1,000 ft in the test.
- **The keyboard**: H selects STOVL mode and N conventional flight, a new
  `select` binding mode (thrown at a press, left where it is thrown), in the
  help with the rest; `stovl` is a control a bindings file may name.

**Verification.**

- `an_ai_plan_flies_the_f35b_to_a_point_converts_hovers_and_lands_vertically_on_it`:
  from 1,000 ft and 250 KCAS, a waypoint 11 km on and the pad 16 km beyond
  it, hovering 30 s - hovering from 280.7 s, touched at 358.7 s, 0.1 ft
  from the pad at 2.99 ft/s; pitch at most 9.4 degrees, bank 7.7; a minute
  on still standing on it.
- `a_pilots_stovl_selector_and_stick_fly_the_f35b_to_a_hover_onto_the_ground_up_and_back_to_wing_borne`:
  from 300 ft and 200 KCAS - hovered at 300.0 ft, the stick forward touched
  her down at 3.00 ft/s, the stick back lifted her to 150 ft and she held
  it within 10 ft, and the selector back handed her to the pilot at 204.2
  KCAS with the fan closed; pitch 12.3, bank 1.2, one touch.
- `a_vertical_landing_is_read_from_a_plan_and_refused_where_it_is_wrong`:
  read, and eleven ways wrong refused.
- `the_keyboard_selects_stovl_mode_with_h_and_conventional_flight_with_n`:
  both keys from both positions, held two seconds each, four throws; the
  help lists ten keyboard bindings.
- `every_control_the_flight_model_has_is_one_the_wire_carries` holds
  nineteen doubles; `the_protocol_version_moves_with_the_collision_ground`
  holds version 12 and the doc client's `0x0C`.
- `a_predicting_client_and_the_server_fed_the_same_stovl_inputs_agree_through_a_conversion_a_hover_and_a_landing`:
  the server's controller flying each input 100 ms late, its motion back
  100 ms later every fifth step; both convert, hover and land, and the
  worst of 6,212 corrections once settled is 0.0256 m (bound 0.1 m). Seen
  red on the code before this: the client never converted.
- `a_stovl_prediction_put_back_to_its_own_state_flies_again_to_exactly_where_it_was`:
  at 60, 115, 165 and 185 s a second flown again from her own state lands
  0.3 to 11 micrometres from where it was (JSBSim's restore is not
  bitwise), the fan's and nozzle's commands within a thousandth; with the
  controller not put back, 0.5 mm to 23 cm and the fan's command 0.02 off -
  seen red so, the 0.1 mm bound broken.
- `an_f35b_taken_back_by_her_pilot_in_a_plans_hover_holds_the_hovers_height`:
  taken back 10 s into the plan's hover, she holds 150 ft within 0.48 ft
  for a minute; seen red with the height not written back (asked 1,000 ft,
  588 ft off).
- `the_f35bs_stovl_selector_at_its_edges_is_flown_and_never_throws`: six
  edges, the count asserted - selected at 270 KCAS, at 60 ft (the doors
  begin no lower than 98 ft), on the ground (converted, still down, lifted
  by the stick), toggled every half second for 20 s, a hovering F-35B
  handed to the AI (held within 10 ft), and STOVL mode put on her at 300
  KCAS (flown closed, not thrown; seen red with the guard removed: it
  threw).
- `an_envelope_that_is_wrong_is_refused_with_the_reason_it_is_wrong`: a `0B` datagram is refused as `WRONG_VERSION`
  by name.
- **Seen failing**: the plan's point not passed to the transition law, she
  touched 17,542 ft from the pad; the stick's height rate made nothing, she
  was not down five minutes after the stick went forward. Both reverted.
- After the review of #190, rebased onto #191: 311 tests matching f35b, stovl,
  prediction, protocol, inputs, keyboard, levers, plan, copilot, transport,
  gearstick, selftest, vertical and controller (not the windowed ones), all
  green (linux-release), -j4.
- Before it, green (linux-release): 299 tests matching f35b, stovl, protocol,
  input, plan, copilot, transport, gearstick and selftest - every F-35B
  test, her hover, transition, short take-off and vertical landing, and
  the copilot playbacks - and 121 matching keyboard, help, bindings, levers,
  lesson, controller and hand. Of the second, three multi-process tests
  failed under -j4: two passed alone, and the third
  (`the_client_with_the_window_on_final_is_refused_the_learnt_landing...`)
  could not listen on its fixed port 24870, held by another working copy's
  run. No live model was asked; no recording's request moved.
- **The selftest hash does not move**, `182dd6c996e0ee4c`: the C172P has no
  lift system.
