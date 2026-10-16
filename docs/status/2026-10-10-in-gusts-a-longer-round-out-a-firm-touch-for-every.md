### In gusts a longer round out, a firm touch for every aeroplane, and a balloon met ahead; five of fourteen land within limits on three seeds, nine are named, 2026-10-10 — item still open

**What is missing first.** Nine of the fourteen are still named in the
gusts test (`not_yet_landed_in_gusts`): the 737-300 balloons at 292 ft/min
and meets the runway at 425 (seed 2); the 787-8 meets it at 365 (seed 1);
the A380 rises 2.6 ft after touching (seed 1) and balloons at 205 (seed 2);
the Learjet 35A, judged before this round, now balloons at 108 against the
100 allowed (seed 1); the C182 rises 1.14 ft (seed 1); the Mosquito bounces
3.7 ft (seed 2); the S.23 meets the water at 571 (her flare flown to her
centre of gravity, the tail in Later); and the A320 and F-15C pass all three
seeds but by only 22 and 13 ft/min of sink, too narrowly to hold on another
platform's gusts. Named before this round: 737-300, 787-8, A380, F-15C,
F-35B, A320, C172P, C182, Mosquito, S.23. Now judged and passing every seed:
B-2A, C172P, F-35B, J-3 Cub, PA-28 (the C172P and F-35B newly).

**Traced first** (787-8, seed 3, the worst at 634 ft/min; then the 737-300,
seeds 1 and 2): held on her raised speed to within 3 kt down to 50 ft, she
entered her flare at 45 ft sinking 944 ft/min against the path's 740 - a
down-gust, `atmosphere/turb-down-fps` reading 100 to 340 ft/min down through
her round out - with her throttle at full by the cushion but her N1 at 53%,
rising a point every tenth of a second. Her nose came up two degrees a
second, a degree above level when her wheels met the runway. The 737-300's
nose, trimmed nose-down for her raised speed, did not move in her flare's
first second while its gain came in. And on seed 2 the cushion's full
throttle reached her engines - 92% N1 - just as the sink was arrested, and
she climbed at 300 ft/min with her lever closing at half its travel a
second.

**What changed** (sim/lander.cpp; in gusts alone, so calm and steady-wind
landings fly as before):
- **A longer round out, begun higher**: the flare's height from 0.07 g
  rather than 0.1, so a gust's added sink at its start is arrested in the
  height there is. This was the change that mattered: the 737-300 seed 1 from
  571 to 209 ft/min, the 787-8 seed 3 from 639 to 30, the F-15C and F-35B
  under 360 on two seeds of three. (0.06 was tried: no better, and the
  F-15C's seed 1 went to 371.)
- **Every aeroplane flown on at 150 ft/min** (`gusty_touchdown_fpm`), a
  jet's 200 softened to it: the last second's gusts add up to 200 ft/min
  either way, and aimed at 200 the F-15C and F-35B met the runway at 450 to
  520. This put the F-15C and F-35B within limits on every seed, and the
  Learjet's seed 1 over (a balloon of 108).
- **The flare's attitude gain comes in over an eighth of a second**, not a
  second.
- **A balloon judged a moment ahead**: the nose is held once the climb 0.3 s
  ahead (the sink's trend) is above level, not once she climbs, and let
  down a degree for each 100 ft/min of it, no lower than where the flare
  began - the AFH's (ch. 9, "Ballooning") hold the attitude and let her
  settle. This un-named the C172P (seed 3, from 140 to 57 ft/min).
- **The cushion's power off as soon as the sink is arrested**, at a full
  travel a second, to the flare's half-throttle floor.
- **A jet's last-feet attitude hold waived while she still sinks more than
  150 ft/min past what the flare wants.** Alone it moved the 787-8 by
  5 ft/min; it stays, as the hold's reason (a nose still rotating at the
  touch) does not apply to a nose that has not yet come up.
- Tried and not kept: flying the glidepath's sink from her groundspeed
  rather than her airspeed in gusts (the 787-8 sank 80 ft/min faster than
  the path into a 15 kt wind) - the 737-300 and 787-8 a little better, the
  Learjet and F-15C landing longer and the Mosquito bouncing 7.6 ft; and
  capping a jet's cushion at 0.3 over the flare's throttle - the F-15C back
  over 360 on two seeds, the 737-300's balloon unchanged.

**Verification:**
- The six gust tests,
  `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_{1,2,3}_{first,second}_half`,
  pass with five judged; any aeroplane failing a seed or passing one by
  under 25 ft/min is named. Least margins of those judged over the three
  seeds: sink 47 ft/min (C172P, seed 1: 313), touchdown zone 219 m (F-35B),
  rise 0.62 ft (PA-28), flare climb 43 ft/min (C172P, seed 3: 57). **Seen
  red**: the C172P and F-35B, now judged, failed on the commit before
  (140 ft/min climbing in her flare, seed 3; 465 ft/min, seed 2).
- In a local linux-debug, seed 2: the first half 13.4 s, the second 27.2 s, both passing
  (two at once, ctest -j4) - well under the 150 s asked.
- 158 tests matching land, lander, learnt, go_around, approach, gust, flare,
  circuit, taken_back and handed pass (linux-release).
- The selftest hash does not move: 182dd6c996e0ee4c.
