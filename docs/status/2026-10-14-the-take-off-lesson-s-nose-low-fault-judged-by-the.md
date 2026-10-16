### The take-off lesson's nose-low fault judged by the porpoising it makes, not by a later lift-off: the S.23 is thrown off the water early, as the handbook says, 2026-10-14 — item stays done

**What was wrong**: on the planing hull (#191), `a_take_off_flown_with_one_fault_has_that_fault_in_its_debrief`
failed on the S.23, "held nose low on the step did not come off sooner".
It asked that, flown nose low on the step (forward stick, -0.3, from 85%
of the book's rotation speed), she come off later than by the book, or
not at all. She came off at 75.1 kt against the book's 79.2.

**Traced** (linux-release, every quarter second): held nose low she
porpoises from the moment the stick goes forward at 51 kt - her pitch
swinging 2.5 to 5.5 degrees, the step's immersion 0.1 to 0.4 ft, her
planing lift 18,000 to 31,000 lb, about every 1.5 s - and she gains only
5 kt in 23 s against the book's steady 8 to 10 degrees. The swings grow;
at 74 kt her bow digs in (pitch -1.1 degrees, the step 1.03 ft deep, her
hull's lift spiking to 75,700 lb) and she is thrown into the air at 15
degrees, 74.5 kt, and climbs away. **That is what the FAA's Seaplane
Handbook says the fault does** (FAA-H-8083-23, ch. 4, "Porpoising", p.
4-9): held too low, a crest builds under the bow, she rides over it and
digs in again, each oscillation more severe; and "porpoising can also
cause a premature lift-off with an extremely high angle of attack"; ch. 5
has it "throw the seaplane into the air with inadequate airspeed for
flight". The hull was right; the test asked for the wrong sign.

**The test now asks what the handbook says** (`Flown::step_swings`,
`Flown::off_at_s`): by the book, no swing of her pitch on the step (past
40 kt, before she is off) - each reversal of a degree or more from the
last extreme counts one; nose low, at least four, a porpoise of two full
cycles; and nose low, off the water later than by the book, or not at
all - the run the porpoising costs her. Measured: by the book off at 79.2
kt after 38.6 s, 0 swings; nose low off at 75.1 kt after 75.8 s, 61
swings. The debrief's naming of her attitude fault is unchanged. The
handbook's part 1 is pinned in docs/ASSETS.md.

**Verification**: green. **Seen red** with the fault not flown (the
forward stick replaced by the take-off autopilot's own): "held nose low
on the step swung her pitch only 0 times: she did not porpoise". The
take-off tests (9, `ctest -R` over the one-fault, lesson and from-water
take-offs) pass. No change to the simulation; the selftest hash does not
move (182dd6c996e0ee4c).
