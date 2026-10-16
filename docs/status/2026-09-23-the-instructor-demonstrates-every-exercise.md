### The instructor demonstrates every exercise, 2026-09-23 — item done

All five exercises are demonstrated by the AI pilot through a
`sim::Controller`, and **every aeroplane taught an exercise flies it**, not a
sample of them: turns (16 aeroplanes), the take-off (5), the approach (6),
the climb and descent (5) and the stall (6). Each flies every stage of its
lesson to an empty debrief, hands the controls to a pilot whose hands are
nowhere near where the AI had them, and takes them back three seconds later.
All seventeen controls are measured at both swaps: **0.0083 handing over,
0.0000 taking back**, where a pilot's hand moves 0.0207 in a frame.

**What kept the item open was that two exercises were not flown by a
controller at all.** The climb and the stall drove an `Autopilot` directly and
reached into the controls it returned - the stall closed the throttle by hand
on the way in and opened it wide on the way out. There is nothing to hand over
in that: the throttle was flying the aeroplane, not the AI.

**The instructor now asks for a speed, not for a throttle.** Asking the
autopilot to hold a speed below the stall closes the throttle for it and holds
the height by raising the nose, which is the entry; asking for a speed above
the climbing speed opens it and puts the nose down, which is the recovery.
The same instruction goes to every aeroplane and each flies it with its own
controls.

**The speed asked for has to clear the one the lesson waits on.** The first
version asked for half as much again as the stall speed, which is what the old
hand-flown recovery aimed at. In a Cessna that is 72 knots and the recovery
stage ends at the climbing speed, 76: she settled just underneath it and the
stage never ended. The old code only got past it because the throttle was held
wide open by hand. The recovery now asks for the climbing speed plus ten
knots, whichever is the higher.

`demonstrate_in_the_air` is shared by the climb and the stall: they differ
only in what the instructor asks for, so the aeroplane, the lesson watching,
the two swaps and what is measured at them are written once.

- `an_instructor_demonstrates_a_climb_and_descent_and_hands_it_over`
- `an_instructor_demonstrates_a_stall_and_hands_it_over`
- `an_instructor_demonstrates_a_take_off_and_hands_it_over`,
  `an_instructor_demonstrates_an_approach_and_hands_it_over` and
  `an_instructor_hands_over_and_takes_back_with_no_step_in_any_control` each
  walked a hand-written list of four aeroplanes and now walk every aeroplane
  taught the exercise, and state how many that is.

**Seen to fail**: the stall demonstration reported `c172p flew every stage of
the stall: 1 of 2` until the recovery speed was raised.

One unrelated repair: `src/frontend/cli/main.cpp:580` promoted a `float` to a
`double` implicitly, which clang rejects and GCC does not, so
`every_first_party_source_compiles_clean_under_clang_too` was red.
