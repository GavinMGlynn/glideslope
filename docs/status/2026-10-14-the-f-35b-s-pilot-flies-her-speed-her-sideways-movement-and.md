### The F-35B's pilot flies her speed, her sideways movement and her heading in STOVL mode, and a plan given her hovering is flown; her rolling landing figure revisited, 2026-10-14 — one item done, one still open

**What is still missing, first.** **No multi-process test flies a
player's STOVL selector through a real server**, so the item "No pilot or
AI plan flies the F-35B between jet-borne and wing-borne flight" stays
open on that alone: the server and the client's prediction are held
together in one process (below), not over a socket. (Since closed but for
a player's vertical landing: the entry above.) **One was tried, and
found something**: the command-line client joined a real server asking for
the F-35B (started at her catalogue's 300 KCAS, 2,200 ft over Sydney
airport heading west, one AI Cessna), predicting, through a 100 ms relay,
its every input STOVL mode selected with the stick full forward and the
lever still. The server's F-35B was a wreck - "hit the ground sinking at
4364 ft/min" - before any hover. Not yet explained: the in-process tests
all begin at 300 ft or 1,000 ft and at most 250 KCAS, none above her
STOVL clearance with the stick held forward from the start and a long way
to come down. The test (a `--stovl-land` pilot for `glideslope_cli connect
--predict`, meeting the DEM as the server does, and a cmake harness) is
not committed: a test that fails would make the tree red. **She flies backward
at 20 kt, not 30**: Farley and Kjelgaard (below) give 30, but her model
flown backward at 30 kt swings 10 degrees in pitch and never settles
(between -25 and -34 kt over 40 s); at 20 kt it swings under 4 kt (a new
Phase 9 item). **Jet-borne, the stick's sideways and the pedals are not
flown**: between the hover and her conversion speed her law holds her
heading and wings level, as before (a new Phase 9 item). **A plan's
vertical landing nearer than she stops in is reached in the hover at no
more than 30 kt**: from the hover a pad 30,000 ft off would take her over a
quarter of an hour. **Not built on Windows** before pushing: nothing here
is under `#ifdef` or touches sockets, windows or paths; CI compiles it.

**Her unified control laws' inceptors** (`Transition::Inceptors`,
sim/transition.hpp, fed by `sim::Controller` from the pilot's controls in
STOVL mode). Sources, in tools/make_f35b.py: Walker, Fuller and Wurth (AIAA
2013-4243, abstract) - the left-hand inceptor commands longitudinal
acceleration; Graham Tomlinson ("Test Flying the Joint Strike Fighter",
2011) - "the throttle commands acceleration and deceleration", and with Guy
Norris (Aviation Week, 3 October 2011) translational rate command, which
"in the hover allows the pilot to make small positional corrections ... and
will then bring the aircraft to a standstill if the pilot releases the
controls"; Elan Head (The Air Current, 14 March 2024) - "the aircraft's
speed over the ground is proportional to control displacement"; John
Farley of the VAAC Harrier - "either side of the aft detent gives the pilot
a closed loop control of ground speed up to 30 kt forwards or backwards ...
the rudder pedals command yaw rate"; Chris Kjelgaard (2012) - she flies
backwards at 30 kt in STOVL mode.

- **The lever** (the throttle) is the pilot's speed once it has moved more
  than 0.05 of its travel (`Controller::lever_release`) from where it was
  at the selector's last rising edge - every one, including a 1, 0, 1
  part-way through a conversion - or when she was taken back in STOVL mode
  (`lever_at_selection`, `selector_was`): the selector alone still slows her
  to a hover where she stops, as before, and a hand resting on the lever, or
  a keyboard's or the wire's rounding, does not send her gathering speed.
  (From the review of #194: it was an exact compare, released by any
  movement, and latched only when the transition law was made.) Its travel from its detent
  (`fcs/stovl-lever-detent`, 0.25, estimated: Farley says only "aft"), within
  2 per cent of travel the detent itself, asks under 30 kt
  (`fcs/stovl-trc-kt`) a ground speed along her heading in proportion -
  forward to 30 kt, aft to 20 (`fcs/stovl-trc-aft-kt`, above) - and above
  it an acceleration, forward at her gathering rate (3 kt/s) and aft at her
  slowing rate (2), the detent holding the speed, to her conversion speed.
  Every speed asked is reached at those rates. Asked past 30 kt in the
  hover, she goes jet-borne; slowed to nothing and stopped, she hovers.
- **The stick's sideways** asks, in the hover, a ground speed to the side
  in proportion, to 20 kt (`fcs/stovl-trc-side-kt`, estimated: a forum's
  quotation of the VAAC's full lateral input, not traced).
- **The pedals** ask, in the hover, a turn in proportion, to 10 degrees a
  second (`fcs/stovl-turn-degps`, estimated: none published); the hover
  law's yaw damping is against the turn asked, not against all turning.
- **The hover law flies a ground speed by her speed alone**
  (`Hover::Target::ahead_kt`, `right_kt`, `turn_degps`): while one is asked,
  and until she has stopped after it (under 1 ft/s), the point it holds
  over is where she is, and stopped, it holds over where she stopped - a
  translational rate command's standstill. Moving the point at the speed
  instead lagged her, and with the speed gone pulled her back: 2.2 kt still
  moving 30 s after the lever came back. Nothing asked, it is the law it
  was, step for step.

**A plan given the AI in STOVL mode** (`Controller::fly_lift_system`): a
plan not yet at its vertical landing - waypoints, or a pad farther than she
stops in (`Transition::stops_within_ft`) - is flown wing-borne: the
transition law converts her back at her conversion speed, no lower than
her least conversion height, the plan's waypoints she passes meanwhile
counted passed, and once the fan has stopped the autopilot is engaged from
the controls the law left and the leg begins where she is. A plan whose
vertical landing is nearer than she stops in is not converted back: before
it, waypoints within `Transition::steer_within_ft` (2,000 ft) of her are
counted passed - **only those flown by with no orbit**: a fly-over or an
orbit is flown however near it is, out of the hover. They are dropped once,
from the plan as it is given (`Controller::to_its_point`), not every step.
**The cost**: such a waypoint near her - one the plan meant as a turning
point a few hundred feet off - is not flown at all; she goes straight to the
pad in the hover. Hovering she is moved to the pad (`Transition::step`,
`Target::over` while hovering) at no more than 30 kt, slowing at her own
rate to stop over it, and let down there. A plan given her standing on the
ground in STOVL mode is not tested. Handed a plan just after lift-off in
STOVL mode, the take-off's climb-out is not given (the transition law flies
her first).

**Her rolling landing figure, revisited** (assets/figures/f35b.xml): a
plan's `land` on a runway is still flown by the approach autopilot as a
rolling landing, so her measured 9,200 ft is still what it needs; a plan's
`land_vertically` needs no runway, and the copilot's and the planner's
landing check (`copilot::landing_refusal`) judge only `land`, so no runway
length refuses it. The copilot and the planner are still not told of a
vertical landing (Phase 13).

**Verification** (linux-release, -j4, DISPLAY and WAYLAND_DISPLAY unset):

- `in_a_hover_the_f35bs_pilot_moves_her_forward_back_sideways_and_turns_her_with_the_lever_stick_and_pedals`:
  hovering at 300 ft, every direction of the table flown - forward, aft,
  right, left, turn right and turn left, six of six asserted - averaged over
  the last 12 s of each phase: the lever half forward 14.97 kt (asked 15),
  full aft -19.96 (asked 20 back), at its detent 0.00 and 0.04; the stick
  full right 20.07 kt across, full left -20.08, 0.31 ahead each, centred
  0.05, her heading moved 0.00 degrees; each pedal 9.99 degrees a second
  its way, 0.27 and 0.12 ft from where the turn began, released 0.00 off
  the heading she was left on. Height off what was asked at most 0.52 ft,
  speeds swinging at most 3.90 kt. **Her attitudes, measured: pitch 9.3
  degrees, bank 7.7** (bounds 15 and 10).
- `the_f35bs_lever_is_the_pilots_only_once_moved_past_its_dead_band_from_each_selection`:
  selected at the forward stop and nudged 0.001 - she hovers; at the aft
  stop and nudged 0.001 - she hovers, 0.71 kt at most a minute on; the
  lever moved to its forward stop after selection and the selector thrown
  back with the doors half open (4.0 s) and forward a second later (0.38
  open) - latched again, she hovers. Three cases counted.
- `jet_borne_the_f35bs_pilots_lever_gathers_holds_and_sheds_her_speed_and_brings_her_back_to_a_hover`:
  the lever forward, jet-borne and 100 kt in 37.1 s; at the detent 30 s
  within 3.75 kt of 100; aft to under 30 kt asked, and at the detent she
  hovers again; height within 0.40 ft, pitch 7.7, bank 0.0.
- `a_plan_given_the_ai_while_the_f35b_hovers_is_flown_converted_back_or_moved_in_the_hover`:
  two waypoints 10 and 20 km on - converted back, passed 15 ft and 999 ft
  off them; a pad 26 km on after a waypoint - converted back, converted
  again, touched 0.5 ft from it; a pad 1,200 ft ahead and 900 right - not
  converted back, moved to it in the hover and touched 0.1 ft from it; a
  fly-over waypoint 1,000 ft ahead before a pad 1,500 ft ahead - flown out
  of the hover, jet-borne, over it within 2 ft, and down 2.1 ft from the
  pad. **Her attitudes, measured: pitch 8.0 to 12.6 degrees, bank 0.7 to
  7.7** (bounds 15 and 30); four cases counted.
- `a_predicting_client_and_the_server_agree_while_her_pilot_moves_the_f35b_in_the_hover`:
  every inceptor every way - the lever forward, aft, and past 30 kt
  jet-borne and back, the stick right and left, each pedal - then the stick
  forward to land; both sides seen ahead, back, right, left, turning right
  and left, jet-borne past 40 kt and down, sixteen stages asserted, and the
  worst of 12,932 settled corrections 0.0256 m (bound 0.1 m). The STOVL
  agreement test it shares its harness with
  (`stovl_agreement`, test_prediction.cpp) gives what it gave: 6,212
  corrections, worst 0.0256 m.
- `a_stovl_prediction_moved_by_her_pilot_in_the_hover_put_back_flies_again_to_exactly_where_it_was`:
  at 169.5, 180, 225 and 250 s, flown again 1.8 to 24 micrometres from
  where it was. **Seen red** with the lever's state not put back
  (`lever_at_selection`): 3.1 cm at 169.5 s, the lever taken as moved
  before it was. The agreement test above stayed green without it - the
  put-back test is what catches it.
- `the_f35b_is_held_to_her_rolling_figure_on_a_runway_and_refused_no_vertical_landing_for_length`:
  a runway a metre short of 9,200 ft refused by the copilot's check and
  the planner's, a metre over given; a vertical landing refused by neither,
  with no runway told or only a short one; four judgements counted. It pins
  what the check already did. **Seen red** with the check refusing a plan
  with a vertical landing and no runway: "a vertical landing with no runway
  told refused".
- **Seen failing**, each reverted: the stick's sideways not passed to the
  hover law, 0.00 kt across, not 20; a plan given her hovering never
  converting her back, "she was not converted back for the plan"; the
  lever's ground speed made nothing, -0.0002 kt ahead, not 15; the pedals
  passed nothing, 0.0000 degrees a second, not 10; the hover's path to a
  pad cut, "she did not land on the near pad"; the lever released by any
  movement (an exact compare), "selected at full and nudged 0.001, she
  never hovered"; no latch at the selector's rising edge, "selected again
  with the lever at its stop, she never hovered"; a fly-over counted
  passed, "she was not flown out of the hover for the fly-over waypoint".
- Green, with the rest: 67 tests matching f35b, stovl, hover, transition,
  vertical, prediction, the committed F-35B, controller, keyboard and
  selftest (not windowed), and 354 matching plan, lands, landing, hands and
  take (not windowed; the live-model ones skipped without keys).
- **The selftest hash does not move**, `de9bf01f96427593` on main after #198: the C172P has no
  lift system.
