### An approach taken back on its landing roll is landed to a stop, 2026-09-27 — tail done

**What is missing first.**
- The AI finishes a landing only after an approach it was given
  (`to_ai_approach`): an aeroplane the pilot lands with no approach given
  to the AI, taken back on its roll, is still given the plain autopilot,
  which never stops it - and today only the lessons give the AI an
  approach (a tail).
- An A320 taken at the touch by a pilot whose stick goes to neutral rises
  **4.1 ft** after the take-back, from 9.1 ft over the ground (0.8 ft above
  where she stands) - a named bound of 4.5 ft in the test, and a tail.
- The flying boat is not tested: afloat she is never still.

**The direction.** The tail is the AI taking the aeroplane back from the
pilot (`Controller::to_ai()`) on the landing roll, as the instructor does
in the approach demonstration: `to_ai()` always gave the plain autopilot,
and `to_pilot()` threw the approach's lander away, so an aeroplane taken
back at 130 knots on the runway held what it was doing and rolled on for
ten to twenty kilometres.

**What changed** (`sim/controller.*`, `sim/lander.*`):
- **The approach's lander is kept through the hand-over to the pilot**,
  and watches while the pilot flies (`Lander::watch`: where she is, and
  where and how she touched - nothing it flies with moves).
- **One way, and this landing only.** The first step the pilot has her
  that she is not still landing, the kept lander is dropped, and a
  take-back after it is the plain autopilot. Still landing
  (`Lander::still_landing`) is: moving; on this runway - within 30 m of
  its centreline (runways carry no width; half the widest, 60 m), from
  400 m short of the threshold (where the lander's flare may begin) to the
  far end, and within 30 degrees of its heading; and either on the ground
  or the water, whoever put her there, or in the air no higher than 50 ft
  over the ground beneath her (over where she touched, once she has), and
  not climbing with the pilot's throttle more than half open - a go-around,
  not a bounce. Fifty feet, not the flare's own height: a 737 and a
  Mosquito whose pilot let the stick go in the flare floated above it with
  the power off and were landing all the same; the 737 then zoomed to 50 ft
  at 1,100 ft/min, and the lander was rightly let go.
- **Taken back while still landing, the AI is given that lander back** and
  goes on to the stop, keeping where and how she touched and the autobrake
  it set for the runway left. What its loops carry step to step starts
  again (`Lander::resume`): the rates, the pitch, rudder and aileron trims,
  the brake, the throttle (from the pilot's) and the flare's attitude (from
  hers). Starting the pitch trim afresh took the A320's rise from 5.2 ft to
  4.1; kept, it also let a Mosquito bounce less, but that case is no
  longer flown (below).
- **Its controls come from where the pilot left them at a hand's pace**,
  as the pilot's come from the AI's on a hand-over: taking her back steps
  nothing either. The same one-step-towards is now one function for both.

**Verification**: three tests, one for each take-back point, adjacent in
the list so they are sharded side by side, each 40 to 60 s on Linux debug:
`an_approach_taken_back_at_the_touch_is_landed_to_a_stop`,
`an_approach_taken_back_at_half_speed_is_landed_to_a_stop` and
`an_approach_the_pilot_puts_down_from_the_flare_and_hands_back_is_landed_to_a_stop`.
Every landplane taught the approach (13; the Short S.23 left out and named,
the 747-400 and F-22A having no reference speed) is landed by the AI and
taken by the pilot at the touch, once half its groundspeed has gone, or in
the flare, and taken back half a second later - after the pilot's own touch,
in the third. The pilot's hands: throttle closed, the landing flap, the
stick central, no brakes; in the flare, the controls of a second lander
flown alongside, so that only who had her when she touched differs (held
where the AI had it instead, a Mosquito touches at 120 knots, flying, and
bounces: the rollout's matter, not this). Each is given her landing back,
the AI has her touching within 5 m of where she did, and she stops on the
runway within a metre of the centreline, upright, with nothing but wheels on
it, unwrecked by the server's crash rule from the take-back, rising less
than 3 ft over her height at the take-back (the A320 at the touch aside,
named), and with no control stepping more than a hand's 1/120. Coverage is
asserted: 13 of 13 in each.

Seen red: on main every one rolled on, 51 things wrong over the first two
points; with `still_landing` made false, 56; with the hand's pace taken out,
26 steps of a whole control's travel; with `watch` doing nothing, all 13 of
the flare case wrong, the AI having her touching further on (the 737-300
34 m, the 787-8 38, the A320 37),
where it first saw her down.

Where each stopped, metres past the threshold of a 3,000 m runway, taken
back at the touch / at half speed / after the pilot's touch: 737-300 1,918 /
1,918 / 1,912; 787-8 2,137 / 2,140 / 2,132; A320 2,236 / 2,131 / 2,123;
A380 1,917 / 1,918 / 1,911; B-2A 2,093 / 2,102 / 2,112; C172P 686 / 680 /
685; C182 654 / 657 / 650; F-15C 2,755 / 2,724 / 2,756; F-35B 2,179 / 2,184
/ 2,171; J-3 Cub 488 / 490 / 489; Learjet 35A 1,633 / 1,634 / 1,630;
Mosquito FB.VI 1,466 / 1,472 / 1,464; PA-28 631 / 632 / 631.

**The A320 at the touch**: 9.1 ft over the ground at the take-back, and
4.1 ft higher after it; every other case rises 1.7 ft or less (the B-2A
after the pilot's touch). In the pilot's half second her rotation carries
on under a neutral stick and her ground spoilers, which deploy only with
weight on a wheel, stay in; she is climbing when the AI has her back, and
its nose-down stick, at a hand's pace, arrests it there.

**Found on the way** (a tail): **the AI's own touchdown in the 787-8, the
F-15C and the F-35B is a crash by the server's rule** - they meet the runway
sinking at 708, 978 and 1,090 ft/min, past the 600 (10 ft/s) the gear is
judged to take. No landing test judged the AI's touchdown with the server's
rule; these do so only from the take-back.

**Verified**: the three tests, the approach lesson, the approach and
circuit hand-overs, and
`handing_the_aircraft_between_pilot_and_ai_steps_nothing_in_any_phase`
pass on Linux debug.
