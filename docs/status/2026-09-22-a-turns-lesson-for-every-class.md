### A turns lesson for every class, 2026-09-22

**What is still missing: the other four exercises for the other six
classes.** The take-off, the approach, the climb and the stall all name
speeds an aeroplane publishes, and eleven of the sixteen publish none - so
those four exist for light aircraft and nowhere else.

**A turn names no speed, which is why it exists for everything.** `vref`,
`stall` and `climb` come from `assets/figures/<id>.xml`, and only five
aeroplanes publish a stall speed: the two Cessnas, the PA-28, the Learjet and
the Mosquito. Bank and height the simulation can see on anything that flies.

**All sixteen aeroplanes fly their own class's turns lesson**, not one of
each class - a lesson that suited the A320 and not the A380 would be teaching
one of them wrongly. Every one gets round all three stages with an empty
debrief, and the worst height lost is 55 feet of three thousand, by the
747-400. The test counts the aeroplanes and the classes and fails if either
falls short.

**Lessons are named `<class>-<exercise>`, and that is a rule now.** The five
light aircraft lessons were `light-take-off` and so on while the class is
`light-aircraft`, so the runner looked for `light-aircraft-turns` and found
nothing. They are renamed rather than special-cased.

**An aeroplane that publishes no figures still has lessons.**
`departure_speeds` and `approach_speeds` throw rather than guess, which is
right - a reference speed invented means nothing - so the test now flies an
aeroplane whose figures are missing and lets a lesson that names none of them
be flown all the same.

**And the plan had a warning the wrong way round.** It said the autopilot's
"gains are the Cessna's and will not suit a jet". Probed through a ninety
degree turn at 10,000 feet, every jet, the flying boat, the Mosquito and the
bomber hold their height to within sixteen feet - and the **Cessna** loses
2,076 feet and decays to 46 knots. At 3,000 feet the same Cessna is exact. It
is not a jet problem: it is an aeroplane near its ceiling being asked for a
thirty degree bank it has not the power to sustain, with nothing reducing the
bank. That is now a tail of its own.
