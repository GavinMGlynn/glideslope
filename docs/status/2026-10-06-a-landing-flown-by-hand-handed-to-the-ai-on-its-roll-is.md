### A landing flown by hand, handed to the AI on its roll, is landed to a stop, 2026-10-06 — tail done

**What is missing first.** The AI's runway for such a landing is the line
she is rolling along from where it is handed her, not the runway itself:
nothing in the simulation knows which runway she landed on, so the
centreline held is her track at the hand-over, and the brakes are set to
autobrake 3 (the 737's 7.2 ft/s squared) rather than for the runway left.
The 747-400 and F-22A, publishing no stall speed, are still given the plain
autopilot there. A taxiing aeroplane handed over with the throttle at most
half open is now stopped too.

**What changed.** `Controller::lands_with(ApproachSpeeds)` tells a
controller how she lands; the server tells every controller it makes
(`controller_for`, from the model's figures, once a model) and the client
its own. `Controller::to_ai()`, given no approach's landing to resume, now
finds her rolling on her wheels (not afloat) with the pilot's throttle no
more than half open - not a take-off - and gives her a lander made for the
roll, `Lander::on_its_roll`: its runway her track through where she is, at
the ground's elevation; touched there, at the attitude she has (a jet's
nose lowered from two degrees under it, as at her own touch); braked to
autobrake 3. It is `resume`d with the pilot's throttle and reached at a
hand's pace, as a landing given back is.

**Verification**:
`an_aeroplane_landed_by_hand_and_handed_over_on_its_roll_is_landed_to_a_stop`,
the take-back tests' fourth case: every landplane taught the approach (13 of
13, the flying boat named) is flown by the pilot from the start - the
controls of a lander flown alongside, the AI given no approach, only told
how she lands - and handed to the AI half a second after the touch with
the throttle closed and the stick central. Each is given a landing, stops on
the runway (the furthest 4.6 m off its centreline, the Mosquito), upright, unwrecked, rising under
three feet, with no control stepping more than a hand's 1/120: the 737-300
stops 1,518 m past the threshold, the F-15C 1,805, the C172P 557. **Seen to
fail** with the roll's lander never given: 48 things wrong, every one
rolling on - the 737-300 20.8 km past the threshold. The selftest hash does
not move (`d36123c1eecc3e23`). Passed with it (linux-release): every test
matching taken back, take-over, handed, hands, hand-over, to the AI and
landed to a stop (46), server hand-over and take-over tests among them.
