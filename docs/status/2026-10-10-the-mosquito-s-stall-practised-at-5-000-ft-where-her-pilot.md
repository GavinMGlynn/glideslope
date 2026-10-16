### The Mosquito's stall practised at 5,000 ft, where her Pilot's Notes have her climb with everything down; a figure for that climb, 2026-10-10 — item done

**What is still missing, first.** Practised at 5,000 ft, two things the
20,000 ft flights hid are named, not yet (a new Phase 9 item): left thirty
seconds in the stall she pulls 1.88 g, inside 2 g but not with the 10% in
hand (at 20,000 ft, 1.71); and the AI's notice lets her 0.5 kt under her
stall (94.0 against 94.5) before the speed comes, as it does the A380 -
she is level again 122 ft lower.

**It was not her pitching moment.** Traced at her warning at 20,000 ft
(a probe, not kept): handed over at 99.5 kt, alpha 7.5 degrees against a
peak at 12, she is at 1.0 g with her elevator steady at 0.53 nose-down -
the trim of 45 degrees of flap, which her Notes give as "Flaps down ...
Nose up" (para. 42) - and she gathers speed at 0.15 kt a second while
sinking 300 to 600 ft/min: at full throttle with everything down there is
not the power at 20,000 ft to be level at 130 kt. The 2.1 g at 140 kt the
item quoted was the left-thirty-seconds flight before the elevator's trim
stopped being wound nose-up past the wing's peak (2026-10-10, below); it has
been 1.71 g since.

**Her model checked against her Notes with everything down** (A.P. 2019E,
1950, para. 48, "Mislanding and going round again"): "The aircraft will
climb satisfactorily at approximately 120 knots with flaps and
undercarriage down at climbing power." A new figure, `climb_flaps_and_gear_down`
(assets/figures/mosquito-fb6.xml): at 1,000 ft, 120 kt, +9 boost and 2,850
rpm, 45 degrees of flap and the wheels down, 18,000 lb: **928 ft/min**,
held above nothing ("satisfactorily" is no number; its upper bound, first
HJ679's clean 1,740, is now her clean climb there - the entry above). The `climb_rate` flight (src/sim/figures.cpp) now takes a figure's
`flaps_deg` and `gear`; no other climb figure states either, so none moved.
The same flight at 20,000 ft, 130 kt, full throttle in high gear **sinks
341 ft/min** (`the_mosquito_fb6_sinks_at_20000_ft_on_full_throttle_with_flaps_and_wheels_down`).
So the model agrees with her Notes low down, and nothing in it was changed.

**Her lesson in her Notes' configuration**: the Notes give no height to
practise a stall at, and no period or RAF source for one was found; they
give a climb with everything down only low down.
A stall lesson now says its height (`height FEET`, src/sim/lesson.hpp), and
each of the seven does - the light aeroplanes and the flying boat 5,000 ft,
the jets 20,000, as the test helper had them by class - and the wartime
twin's is **5,000 ft** (assets/lessons/second-world-war-stalls.lesson says
why). The tests' `stalls_are_practised_at` reads it, and is used also by the
climb-capture and turbulence tests, whose Mosquito now flies at 5,000 ft:
both pass.

**The figures** (linux-release, her height 5,000 -> others unchanged):

| Mosquito FB.VI | at 20,000 ft (before) | at 5,000 ft |
|---|---|---|
| at the warning | never level, 3,095 ft lost, named | **46 / 600 ft, 1.20 g** |
| left 30 s | 904 / 1,484 ft, 1.71 g | 493 / 1,020 ft, **1.88 g named** |
| the AI's notice | not level, named | 94.0 kt against 94.5, **named**; level 122 ft lower |

Tried at 8,000, 10,000, 12,000 and 15,000 ft (the lesson's copy in the
build): at the warning 52, 61, 79 and 125 ft lost; left thirty seconds 1.83,
1.85, 1.81 and 1.76 g; the AI 0.2 to 0.6 kt under her stall at each. A
height chosen to pass would be 12,000; the light aeroplanes' 5,000 was kept
and the two named.

**Seen to fail**: her lesson's copy in the build set back to 20,000 ft,
the warning check red ("mosquito-fb6 not recovered: 2358.9 ft") and the
left-thirty-seconds check red on her stale name ("now meets 2 g ... take
its name off"). The new figure red with the flaps' drag at 45 degrees
doubled in the build's copy of her model (-37 ft/min), the 20,000 ft test
red with it halved (+449 ft/min); each restored and compared with the
committed file.

**Verified** (linux-release, on `plan-no-later` 1f3430cf; main 6491b943, which this branch is on, is the same tree): the tests
matching stall, recover, lesson, instructor, slowed_from_cruise,
turbulence and mosquito, 166 of 166; and climb, figure, ceiling, glide,
orbit and land, 370 of 370. The selftest hash does not move,
`182dd6c996e0ee4c`.
