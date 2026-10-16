### A lesson, and a debrief, 2026-09-22

**What is missing first: there is one lesson.** The take-off, for light
aircraft. No circuit, no stalls, no turns, and nothing for the other six
classes of aeroplane.

**But a lesson is data now, and a debrief exists.** A `.lesson` file is a
sequence of stages, each with what to do, what ends it, bands that must
`hold` through it and things that must be true by its end. The two are not
the same and the file says why: a `hold` is broken the moment the aeroplane
leaves the band, because an approach flown too fast is wrong while it is
happening; a `need` is judged when the stage ends, because flap down by the
time you turn final is not a fault until you have turned final. The debrief
is what to do differently, in the order it happened, and nothing else - there
is no score (`FEATURES.md`).

**Flown by the book, all four light aeroplanes leave it empty** - the Cub,
the PA-28, the 172 and the 182, each taken off by the take-off autopilot on
its own published figures. Flown on part throttle, the debrief holds exactly
one thing and it is "Open the throttle fully for the take-off".

**The attitude band was guessed, and then measured.** The first band, -2 to
20 degrees, faulted every correctly flown take-off there is. Flown by the
book the four hold between -4.5 and 19.4 degrees once off the ground - Cub
-2.5 to 18.3, PA-28 -3.0 to 18.7, 172 -3.9 to 19.1, 182 -4.5 to 19.4, the
nose-down figures being the moment after lift-off as she settles into the
climb. The band is that range with about four degrees of margin, and the
measurement is written into the lesson beside it.

**Rotating early cannot be injected by telling the autopilot to rotate
early.** Tried: with the rotation speed set to 25 knots the Cessna came off
at 80 all the same, because an aeroplane below its stall will not fly
whatever its nose is doing - she simply rolls on with the nose up. Flown the
way it actually happens, a steady touch of back stick, she is at 47 knots
where the book has her at 80.

**And the lesson does not catch it, which is worth more than hiding it.** A
lesson teaches a class; the four light aeroplanes rotate between about 34
knots and about 55; so the only speed a class-wide lesson can name is the
lowest of them, and 47 knots is nowhere near it. Catching a 172 rotating
early needs the lesson to say "her own rotation speed" rather than a number.
The figure is already in `assets/figures/<id>.xml` and `sim::departure_speeds`
already reads it - the format simply has no way to refer to it. That is named
in the lesson file, in the test, and in `COMPLETION_PLAN.md`. What catches a
badly flown rotation today is the attitude band, which it does: -9.0 degrees
against a floor of -8.

**The parser refuses sixteen ways of being wrong**, each walked and counted:
no name, no stages, a stage that never ends, a stage with nothing to do, a
command before any stage, a class there is none of, a command there is none
of, an `until` without an operator or with a bad one or without a number, two
`until`s in one stage, a `hold` without a band or with a backwards one or
without words, and a `need` without words or with a bad operator.
