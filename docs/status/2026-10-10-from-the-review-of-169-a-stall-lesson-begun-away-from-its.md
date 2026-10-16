### From the review of #169: a stall lesson begun away from its height says so, a stall lesson must give one, and the everything-down climb bounded by the clean one, 2026-10-10 — item stays done

**What the product does with a lesson's height.** Nothing in the client,
server or CLI starts a lesson: lessons are flown through the simulation's
`LessonRun` (src/sim/lesson_run.cpp), by the tests' instructor and stall
flights, and the AI's stall notice (sim/controller.cpp) is not a lesson.
So the height is made the lesson's own, where any caller meets it:
- **A stall lesson must give its height** (src/sim/lesson.cpp): one with a
  `warning` and no `height` is refused. Seen red: the check stepped over,
  "a lesson with a stall warning and no height is refused" failed.
- **A lesson begun away from its height says so, first in its debrief**
  (`LessonRun::judge_the_height`): more than 500 ft above the ground below
  or above it, "Begun at 20000 ft above the ground: this lesson is
  practised at 5000 ft, where the aeroplane can do what it asks" (or, below,
  "for the height it may need"). It neither moves the aeroplane nor refuses
  to run: the flight is the caller's, and a debrief is how a lesson tells.
  `a_stall_lesson_begun_away_from_the_height_it_is_practised_at_says_so_first`
  begins every aeroplane taught a stall at its height (nothing said), 1,000
  ft under it, and 1,000 ft over it or, for a 5,000 ft lesson, at 20,000:
  14 aeroplanes, 42 starts, 2 left out with their reasons. Seen red: the
  call stepped over, "737-300 begun at 19000 ... is told nothing".
  `the_stalls_lesson_flown_by_the_book_leaves_an_empty_debrief` holds that
  a lesson begun at its height says nothing more.

**No source for a practice height.** No height to practise a stall at was
found in the Mosquito's Pilot's Notes (1950 edition, read through), nor in a
web search for the 1944 edition's and RAF training's; 5,000 ft is the light
aeroplanes' and the flying boat's, and her Notes' climb with everything down
(para. 48) is a figure for low down, going round from a missed landing.

**The everything-down climb's upper bound is her clean climb.** The 1,740
ft/min bound (HJ679's clean climb at 10,400 ft) tested nothing. Her model's
clean climb at the same 1,000 ft, 120 kt, +9 and 2,850 rpm is 2,392 to
2,412 ft/min (the start height moves with the figure's midpoint), so the
bound is 2,400, and
`the_mosquito_fb6_climbs_faster_clean_than_with_flaps_and_wheels_down` flies
both and holds the bound within 2% of the clean climb and the
everything-down climb under it. Seen red with the bound tightened to 900 in
the build's copy of the file: the figure (935 ft/min) and the bound's test
both failed; the copy restored and compared with the committed file.

**The base.** The first commit was verified on `plan-no-later` at
1f3430cf, then rebased onto main's 6491b943, the same tree (`git diff
1f3430cf 6491b943` is empty); this one is verified on that rebase.

**Verified** (linux-release, on d1cafbe9 rebased on main 6491b943): the
tests matching stall, recover, lesson, instructor, slowed_from_cruise,
turbulence, mosquito, climb and figure, 245 of 245. The selftest hash does not move, `182dd6c996e0ee4c`.
