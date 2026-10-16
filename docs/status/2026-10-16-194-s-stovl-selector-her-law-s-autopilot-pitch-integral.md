### #194's STOVL selector: her law's autopilot pitch integral told too; the median bound 1 cm, 2026-10-16 — a red PR's cause shown and fixed

**What is still not true first.** **Only the wing-borne autopilot's
integral is carried**: the flag is `00` only when no wing-borne autopilot is
flying her: no law, or a law flying her jet-borne, hovering, or converting
back from jet-borne - and then the transition law's own integrals and the
hover law's are each side's own; through a server her loading holds her
wing-borne (the entry below), so none of them is flown there, and none is
measured here. **Selection before the prediction has settled is not
shown**: here it settles before the 150th input every time, so "at input
150 whatever the settling" was settled too. COMPLETION_PLAN's item is
narrowed to these, not ticked. **CI run 38692182329 failed this test on
Rocky 9 at the code before it (76b347ce)**: the median 74 mm, the 95th
percentile 0.116 m, the worst 1.433 m, against the 5 cm median bound -
twice the 37 mm this machine found, as a selector placed two steps off
would give. **CI run 38768208229, on `c27ade6d`, passed every job on every
platform** with the 1 cm bound - one run of the test on each, not a spread
like the 14 here.

**The cause, shown.** In one process - the F-35B at 45,400 lb untrimmed at
2,200 ft and 300 KCAS, diving untouched as a server holds a player's, STOVL
mode at 5 s with the stick forward, the server's word every fifth step 100
ms each way with the height asked told - the server's selector placed on
the client's step, a step early, one late and two late. On the step: the
median error 0.0000 m. One off: 21 mm, her pitch 2.9 degrees off at worst,
and 30 of the 107 numbers of the transition law and its autopilot apart and
not converging - the autopilot's pitch integral -10 against -5.60 five
seconds on, 13.17 against 12.29 at fifty; its elevator trim, pitch command,
throttle integral, trends and engaging offsets besides. Put back to the same
motion at every word, both integrate alike from there, so whatever the
selector's step seeded apart stays apart. With all of the autopilot's 56
numbers and the law's last controls copied from the server's at every word,
every placing gave 0.0000 m and 0.015 degrees: the law's state is the cause.

**The least that puts it right.** A backward search over those 57, dropping
each that could go while every placing, selected at 3, 5 and 8 s, kept the
median within a millimetre and the pitch within half a degree, left one: the
autopilot's **pitch integral**. Alone, selected at each whole second from 1
to 10 s and each placing (40 cases), the median at most 0.4 mm and the
pitch 0.044 degrees. Without it the same cases ranged from 0.1 to 21 mm -
which placing is harmful depends on the moment.

**The change.** `Autopilot::pitch_integral_deg` and `put_pitch_integral_deg`;
`Transition::Carried { std::optional<double> pitch_integral_deg; }`, which
`Transition::carried()` exports - none only when no wing-borne autopilot
is flying her: no law, or a law flying her jet-borne, hovering, or
converting back from jet-borne - and `carry()` takes, where it has one.
Version 13's own motion (unreleased, changed in place) carries it after the
height asked: a `u8` flag and an
`f32`, every bit nought when the flag is `00`; refused with a flag past
`01`, a NaN or infinity, anything but nought after `00`, or a pitch integral
with a height of -1. The full packet 1,147 bytes, 1,177 sealed. The server
fills it with the motion; `Prediction::reconcile` takes it
(`const Transition::Carried&`) and puts it on the law put back with the
height. glideslope_cli and the windowed client pass it on. **The height asked
the server sends is clamped to nought**: seeded on her wheels from her height
above the ground, a hair under it would be refused by `write_state` and the
client told nothing more. The envelope test names the versions it refuses
in hex. And glideslope_cli says, flying `--stovl-land`, her predicted pitch's
worst error against the server's word over the updates compared, which the
multi-process test holds within half a degree.

**Verification** (linux-release and linux-debug):

- `a_prediction_whose_stovl_law_was_made_a_step_or_two_apart_from_the_servers_is_put_right_by_what_the_law_carries`:
  the dive above, selected at 5 s and 9 s, the server's law made a step
  early, on the step, one and two late - eight cases, counted, each over
  1,461 words or more: the median at most 0.2 mm (bound 1 mm), the worst 0.5
  mm, her pitch within 0.020 degrees (bound 0.5). **Seen red** with the pitch
  integral not carried: the first case, the median 8.3 mm and the pitch 1.14
  degrees off.
- `a_state_packets_stovl_pitch_integral_rides_with_a_flag_and_is_read_exactly_or_refused`:
  four kept (none, 0, -10, 13.13) and 259 refused, counted; the NaN walk 25
  fields and 75 cases; the byte layout, the document, the doc client and the
  packet's size updated.
- **The multi-process test** (linux-release), every median:
  selected settled, idle 0, 2, 1, 0 and 2 mm, beside 40 busy loops (twice
  the cores, killed by their PIDs) 1, 0, 1 mm; selected at the 150th input
  whatever the settling (built for the measurement, reverted), idle 1, 1, 0
  mm, beside the loops 1, 1, 0 mm: 14 runs. Her pitch within 0.154 degrees
  at worst over all 14 (and linux-debug's one run: 0 mm, 0.006 degrees).
  Before: 0 to 37 mm here, 74 mm on CI. **The median bound is tightened
  from 5 cm to 1 cm** - five times the worst of these - and
  her pitch is now held within half a degree at every update. **Seen red**
  with a pitch integral 1 degree off the server's put on the prediction (a
  deliberate bug in glideslope_cli): her pitch 1.722 degrees off, the
  median 22 mm; reverted, green. (A prediction flown on 0.8 of the stick,
  the bug that saw the test red on 2026-10-15, now passes: in STOVL mode the
  stick asks the height, and the height is now told.)
- The net, protocol, state, transport, prediction, F-35B and STOVL tests,
  the gearstick refusal and the multi-process test pass in linux-release
  (121 of 121) and linux-debug (122 of 122, with the new multi-process
  checks; its run of the multi-process test the median 0 mm, her pitch
  within 0.006 degrees).
