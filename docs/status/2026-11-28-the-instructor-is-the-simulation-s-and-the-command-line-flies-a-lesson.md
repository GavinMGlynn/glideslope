### The instructor is the simulation's, and the command line flies a lesson, 2026-11-28 — first part of an open item

**Still open: no window client or server starts a lesson.** The Phase 12c
item "No player can fly a lesson or watch the instructor" asks for a player
choosing a lesson in the window client, the instructor flying it first and
handing over, and the debrief shown. This is its first part, the one every
frontend needs: until now the instructor was test code alone
(`tests/unit/test_lesson.cpp`'s `demonstrate_*`), so nothing outside the
tests could demonstrate a lesson. Missing still: the window client's lessons
screen and its states, and the server running the lesson's aircraft (the
server owns every aircraft), with the window-client test over every lesson.

**What now works**

- **Every lesson says how it is demonstrated**: `instructor EXERCISE` in its
  file (`take-off`, `approach-and-landing`, `circuit`, `climb-and-descent`,
  `stalls`, `turns`), read by `sim::parse_lesson` and refused at its line
  when it names no exercise. All 42 files carry it.
- **`sim::Instructor`** (`src/sim/instructor.{hpp,cpp}`): built from a
  lesson, a catalogue entry, a runway and terrain, it puts the aeroplane where
  the lesson begins (on the threshold, two miles out on the glidepath, or in
  the air at the lesson's height or 3,000 ft), loads her as her lesson's
  figures were measured, flies the demonstration through a `Controller` -
  the take-off and approach autopilots, the go-around's circuit, and the plain
  autopilot asked for heights, headings and speeds, as the tests' demonstrations
  asked them - judges it with the lesson's `LessonRun`, and hands the controls
  to the pilot the step after the lesson ends. The loading and figure helpers
  it needs (`lesson_speeds`, `book_departure_speeds`, `load_for_the_take_off`,
  `load_for_the_approach`, `load_as_its_figures_were_measured`,
  `a_climb_it_can_manage`) are the tests' own, moved into the simulation; the
  tests still keep their copies (a tail below).
- **`glideslope_cli lessons`** lists the lessons and who teaches each;
  **`glideslope_cli lesson LESSON [AIRCRAFT]`** flies one: each stage said as
  it begins with what to do, "handed over: you have control", three seconds
  with the pilot's hands, then the debrief.

**Verification**

- `every_lesson_says_how_the_instructor_demonstrates_it_and_has_an_aeroplane_to_teach_it`:
  42 lessons, six exercises, every lesson's file named for its class and
  exercise, and 42 of 42 with an aeroplane of their class able to be taught it.
- `the_instructor_demonstrates_every_classs_<exercise>_lesson_and_hands_it_over`,
  six tests: each of the seven classes' lessons, flown by the first aeroplane
  of its class that can be taught it, flies every stage to an empty debrief
  and is handed over, the worst control step 0.0083 at the hand-over and
  after it, against a hand's 0.0207; 7 of 7 each, so 42 of 42 lessons. On
  Linux debug the circuit test takes 74 s, the others 10 to 29 s.
- `a_lesson_the_instructor_cannot_demonstrate_is_refused_and_says_why`.
- `a_lesson_flown_from_the_command_line_is_demonstrated_handed_over_and_debriefed`:
  `glideslope_cli lesson light-aircraft-turns` - the C172P, stages 1 to 3,
  handed over at 41.2 s, "flown to the book".
- **Seen to fail**: with the turn the instructor asks for taken out, the turns
  test reported `airliner-turns: 737-300 flew 0 of 3 stages` and the command's
  test `debrief: 0 of 3 stages flown`, exit 1; reverted, both green.
- The existing instructor tests (`an_instructor_*`) and the lesson parse tests
  pass unchanged.

**Tails**

- The tests' `demonstrate_*` in `tests/unit/test_lesson.cpp` still fly their
  own copies of the recipes and helpers now in `sim::Instructor`; they should
  fly through it, keeping their take-back checks.
