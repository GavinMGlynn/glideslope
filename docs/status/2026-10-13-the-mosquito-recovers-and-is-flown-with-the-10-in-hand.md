### The Mosquito recovers and is flown with the 10% in hand: power once the wing is unstalled, and her own stall warning, 2026-10-13 — item done

**Left thirty seconds in her stall, 1.88 g -> 1.69.** Traced (a probe, not
kept): handed over at 120.5 kt, 20.5 degrees of alpha against a peak at 12,
nose 7.5 down and sinking 6,000 ft/min, her elevator still +0.60. The 1.88 g
was not the pull-out: it came in the first second, as the elevator went
nose-down at a hand's pace and the wing came back through its lift's peak
(alpha 13 to 10) at 123 knots - and her propellers' slipstream, at the full
power the flight gave her at the hand-over, lifted the wing behind them
(her model's `lift-slipstream`; her Notes' power-on stall is 90-95 kt
against 95-100 off). Not her limit load, nor her pitching moment with flap:
the pull-out after it stayed under 1.45 g. Opening the throttle half a
second later gave 1.77 g, a second 1.69, two 1.68.
**The fix, general**: the FAA's order (Airplane Flying Handbook,
FAA-H-8083-3C, chapter 5: reduce the angle of attack, then add power) - the
autopilot's stall recovery holds the throttle where it is while the wing,
seen to stall, is still past its peak (`Autopilot::unloading`,
src/sim/autopilot.cpp), and opens it once unstalled. Her lesson already says
"Full power as the nose comes down"; the tests' stall flight gives full
power at that moment rather than at the hand-over. "Seen to stall", not "at
the greatest angle seen yet": a wing slowing to its stall is at that on
every step, and with it the AI's notice held the B-2A's and the Learjet's
throttles. Her Pilot's Notes (A.P. 2019E, 1950, para. 43) give no technique
beyond "Recovery is easy and normal in all cases".

**The AI's notice, 0.5 kt under her stall -> 4.0 kt over it.** Traced:
noticed at 99.5 kt (the rule's 5 kt over 94.5), she was slowing at 2.8 kt a
second with her throttles shut and her propellers windmilling; the throttle
opens at its 0.25 a second and the nose comes down, and the speed bottomed
at 94.0 three seconds later at 6.3 degrees of alpha - nowhere near her
wing's peak at 12. Her Notes give her own warning (para. 43 (ii)):
"Warning of the approach of the stall is given by pronounced buffeting of
the control surfaces, the onset of which can be felt some 10 knots before
the stall itself." So a figures file may now give `stall_warning_kts`, its
aeroplane's own warning over its stall (`PublishedFigures`,
`ApproachSpeeds::warning_margin_kts`), and `sim::stall_warning_kts` takes
the more of it and the rule's 5 kt or 5%; hers is 10
(assets/figures/mosquito-fb6.xml). The notice, a plan's slowest and the
turbulence test read it; no other file gives one, so nothing else moved,
and her plan's slowest (129 kt, her approach speed) is above it. Her stall
lesson's `warning stall+5` is unchanged: it is the latest a warning may
come, where the recovery is held to its height.

**The tests, with the 10% in hand**:
`every_aeroplane_left_thirty_seconds_in_a_stall_is_recovered_within_2_g_and_the_height_its_speed_and_sink_need`
names nobody now (the Mosquito 1.69 g against 2 with 10% in hand); its
other figures moved little and all inside their bounds - lost against the
bound, 737-300 996/1,928 ft (was 968), B-2A 1,012/1,906 (908), F-15C
1,619/3,467 (1,431), F-35B 3,774/6,295 (3,565), J-3 Cub 56/307 (38); peak g
no higher for any (A320 1.81 as before).
`the_ai_pilot_notices_a_stall_coming_and_recovers_from_it` now also holds
how far each slows under her warning, a tenth over, to her warning's margin
over her stall at her weight, and names only the A380 (104.1 against 104.9,
her own item): the Mosquito is slowest at 98.5 against a warning at 104.5;
the closest of the rest, the Learjet 4.3 under against 5 and the 787-8 5.0
against 5.7. Every other aeroplane's figures there are the same to the
tenth.

**Seen to fail**: the hold stepped over (`unloading_` never set), the
left-thirty-seconds test red on "mosquito-fb6 load: 1.881053 g, against
2.000000 with 10% in hand"; the build's copy of her figures without
`stall_warning_kts`, the AI test red on "mosquito-fb6: slowed under her
stall" (94.0); with it at 5.6, red on the new check, "slowed 5.580382 kt
under her warning, against its margin of 5.600000 with 10% in hand". Each
restored, and the copy compared with the committed file.

**Verified** (linux-release): the tests matching stall, recover, lesson,
instructor, mosquito, figure, turbulence, slowed_from_cruise, climb,
plan_speed, orbit, ai_pilot, autopilot, glide and land, 431 passed and 15
skipped (no key) of 457 - the other 11 needed the client or server, not
built; with the server built, its 7 of those passed. The selftest hash does
not move, `182dd6c996e0ee4c`.
