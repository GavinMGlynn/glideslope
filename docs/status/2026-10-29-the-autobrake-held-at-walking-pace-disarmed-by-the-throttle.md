### The autobrake held at every pace to walking pace for half a minute, and disarmed by a throttle advanced, 2026-10-29 - item still open

**What is still missing, first**:
- **No landplane rolls at walking pace for long at idle.** Set rolling at
  0.5 to 4.5 kt and handed to the rollout with the throttle closed, every
  one stops on its brakes within 2.2 s. That is true dry, wet and in 15 kt
  across, 351 runs. She then stands on the cap for the rest of the half
  minute. There is nothing to slow, and her brake and nose hold. Rolling for
  the whole half minute was tried two ways, and both failed:
  - **Held at the pace by the hand's throttle**, for 30 s. A Mosquito was
    pushed past 5 kt, where the autobrake is not capped. It wound to 0.85
    to 0.90, and she came back under 5 kt on her nose (-6.1 to -7.1
    degrees, airframe struck). The PA-28 was wound to 0.64 and 0.76. The
    C172P and the Cub at full power do not roll against 0.3 at all. That is
    the windup the disarm (below) now ends.
  - **Held at the pace by setting her speed every step**
    (`Aircraft::set_motion`). JSBSim's state is re-initialised each time,
    and the run blew up: a 737 reached 3,000 kt.
- **The 0.8 idle-creep allowance is still not reached on the jets.** In
  none of the 351 runs does an idle thrust drive a creep against the 0.45
  standing cap. The gust test still exercises it, on the C172P, C182 and
  PA-28 only. The jets, the Mosquito and the Cub are held at 1 kt by their
  brakes there.

**The change** (`sim::Lander`'s rollout brakes, lander.cpp):
- **A throttle advanced on the roll disarms the autobrake.** It then puts
  on and holds no brake of its own until it is armed again: by `resume`
  (the controller's hand-over, the learnt landing's touch) or by a new
  rollout. "Advanced" means any engine's throttle more than
  `Lander::disarm_throttle` (0.02) above the least it has been since the
  autobrake was armed. The rollout's own closing of the throttle is
  therefore not an advance. The rudder bar's steering brake (`toe_brakes`)
  is not the autobrake's, and is kept. Sources:
  - **The 737**: the autobrake disarms when "the thrust levers are advanced
    from idle to forward thrust (except during the first 3 seconds of
    landing)". This is the 737 autobrake disarm list, as given by Flaps 2
    Approach, "Autobrake System - Review and Procedures" (2015), which
    restates the FCOM's conditions:
    https://www.flaps2approach.com/journal/tag/Auto+Brake
  - **The A320 family**: "Ground spoilers extension is inhibited if one or
    more thrust levers remain above the IDLE detent", and "Autobrake is
    inhibited if the ground spoilers do not extend". This is EASA SIB
    2008-10, Airbus A318/A319/A320/A321 Standard Operating Procedures.
- **The idle-creep allowance only for a creep.** Under 2 kt, with every
  throttle closed and her speed growing, the autobrake may go to 0.8. Now
  that applies only at 1 ft/s or more, the speed at which the rollout calls
  her stopped. Standing on her brakes at idle, noise in her speed read as
  growing. Every one of the 351 runs then stood with her autobrake at 0.8
  and nothing to slow. Now standing holds at 0.45.
- **What the rollout's own throttle does is unchanged**: it only closes on
  the roll, so no AI rollout disarms itself. The controller's hand-over on a
  roll calls `resume` with the throttle applied, and blends to the
  rollout's closing at the same hand's pace.

**The tests** (tests/unit/test_lesson.cpp). They replace the six walking-pace
tests of 2026-10-15, which rolled her at 2 to 5 kt for about 2 s and named
the runs short of the rolling cap.
- `every_landplane_held_at_every_pace_to_walking_pace_at_idle_keeps_her_autobrake_bounded_and_her_nose_up`,
  and the same `..._on_a_wet_runway_...` and `..._in_a_15_kt_crosswind_...`.
  - **The setup**: every landplane (13; the flying boat named) at every pace
    from 0.5 to 4.5 kt by half a knot is set rolling at her pace once and
    handed to the rollout at idle. She is judged for 30 s, then to the stop
    (within a minute more).
  - **5 kt is named**: there the autobrake stops standing down.
  - **Asserted**:
    - the paces plus the one named are 0.5 to 5 kt by half a knot;
    - 13 by 9 = 117 runs per runway, each 30 s under the autobrake;
    - in each run her autobrake comes up to the bound at her speed;
    - she stops.
  - **Judged every step**:
    - no brake is added past the bound at her speed: 0.3 from 2 kt, rising
      to 0.45 by 1 kt, and 0.8 only while she creeps under 2 kt at idle;
    - her nose stays above -5 degrees and her wings within 15;
    - nothing but her wheels touches the runway (`sim::GroundJudge`);
    - she turns no more than 30 degrees from her heading at the hand-over.
  - **The bounds are stated in the test** and not read from the lander.
  - **Figures (linux-debug)**:

    | Runway | Rolling at most | Autobrake at most | Least pitch | Most roll | Most turn |
    | --- | --- | --- | --- | --- | --- |
    | Dry | 2.2 s | 0.450 | -2.9° | 0.1° | 0.3° |
    | Wet | 2.1 s | 0.450 | -3.2° | 0.1° | 0.3° |
    | 15 kt across | 2.1 s | 0.450 | -2.8° | 1.6° | 2.0° |

    The C172P stands on 0.45 with her nose at +0.8 to +2.2 degrees, and the
    Cub at +12.9 to +13.1. **Margin**: the least pitch is 1.8 degrees from
    -5. This was measured on this FMA (x86-64-v3) build only.
- `a_throttle_advanced_on_the_roll_disarms_every_landplanes_autobrake_until_it_is_armed_again`.
  - **The setup**: every landplane is set rolling at 3 kt at idle and
    handed to the rollout. After 10 s the throttle is opened to 0.1 over
    the rollout's for 2 s, then closed for 10 s more.
  - **Asserted**:
    - before the throttle, her autobrake is armed and at its ceiling (0.45;
      the Mosquito 0.538, let go on her idle creep);
    - from the step after it is opened, the autobrake is disarmed and at 0;
    - it is still 0 at the end, with the throttle closed again;
    - 13 of 13 landplanes.
- The Cub's walking-pace test, which holds her at taxi power before the
  hand-over, and the gust test are unchanged and pass. The gust test still
  pushes the C172P, C182 and PA-28 past 1.3 kt.
- **CI costs**: tests/ci_costs has the new names, estimated from the old
  2-3.5 kt tests (0.9 of them; a quarter for the disarm test). Here the old
  tests took about 90 s, and each new one takes 80 s.

**Seen red** (linux-debug):
- **With the walking-pace cap taken out** (`most = 1.0` in `Lander::fly`'s
  dry branch), dry and across both failed in every run, 117 of 117 each.
  Her autobrake was wound to 1.000 standing at 0 kt. No nose went down: the
  bounds catch it first.
- **With the disarm taken out** (`if (false && disarmed_)`), the disarm
  test failed for 13 of 13: the autobrake was at 1.000 after the throttle,
  wound by the power against it.
- Both were reverted, and the tests pass again.

**Other tests run** with the disarm and the creep floor:

`ctest -R` over the taken-back approaches, the wet short runway landed by
hand, taxi-off, the touch-and-go, the CLI's gate hand-overs, the learnt
landing at half fuel, steady 15 kt across, wet in 15 kt across, the steady
headwind and tailwind, and anti-skid: 15 of 15 passed (linux-debug). The
selftest and hash tests pass too, 8 of 8. The full suite was not run here.
CI runs it.

**From the review of #271**:
- **A bounce cushioned with power disarmed the autobrake for the whole
  roll.** The disarm's least throttle was cleared only by `resume`. A
  bounce sends her back to the flare, whose cushion law opens the throttle,
  so at the next touch the rollout read that throttle against the old
  least and disarmed.
  - **The fix**: the least is cleared whenever she is flown as a bounce, so
    the next touch takes it again. A disarm already made stays.
  - **Why the least is reset, not compared with the lander's own last
    command**: a throttle someone else holds steady is no advance. That
    happens in the tests' hand-overs, which call `on_its_roll` with taxi
    power on and no `resume`. Compared with the lander's command, those
    would disarm at once.
  - **New test**:
    `a_bounce_cushioned_with_power_leaves_the_autobrake_armed_for_the_roll_after_it`.
    A C172P rolls at 40 kt in air with a 10 kt gust factor and is braked
    (0.345). She is then lifted 5 ft, sinking at 15 ft/s. She is flown as a
    bounce, and the cushion opens her throttle to 0.150. She touches again
    and is braked to 0.355 to the stop, armed.
  - **Seen red** with the reset taken out: at the next touch she was
    disarmed, the autobrake stayed at 0.000, and she did not stop in 120 s.
- **The every-aircraft landing tests now also require her autobrake to be
  armed at the stop**: the lesson approach helper, the every-aeroplane
  landing helper, and the controller's taken-back landings. The AI never
  disarms it itself.
- **The disarm test arms it again** with `resume(0.0)`, as the controller
  arms it. Within 5 s every one of the 13 is armed and braked to 0.450.
