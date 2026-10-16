### The AI pilot notices a stall coming and recovers from it; a level stall entry measured and set aside, 2026-10-10 — item still open (its level entry superseded: committed in the entry above it, with a bound of 5 degrees above the nose)

**What is still missing, first.**
- **2 g left thirty seconds**: the A320 (1.95 g) and the Mosquito (2.22 g),
  unchanged. The angle-of-attack hold on the elevator, with its own
  integral and the trim backed off while it binds (the entry below says
  why), was not attempted: the time went on the entry and the hook.
- **The heights at the warning**: the F-15C (522 of 500), F-35B (773),
  Learjet 35A (385 of 350) and S.23 (184 of 200, without the 10% in hand),
  and the Mosquito never level again. A level entry closes the two fighters
  (below), but it is not committed: it turns another test red. (Committed
  with a bound that keeps that test green, the entry above.)
- **The AI's notice**, now in, has two exceptions named: the A380 dips 0.7
  kt under her published stall before her speed comes, and the Mosquito at
  20,000 ft, flaps and gear down, is never back at her approach speed and
  level. And its warning is from the stall with everything down, so in any
  other configuration it comes late, never early.

**The AI pilot notices a stall** (`Controller::notice_a_stall`,
src/sim/controller.cpp). Whenever the AI flies on the plain autopilot -
holding, navigating a plan, gliding, or round a go-around's circuit - and
it has been told how she lands (`lands_with`, which the client and the
server both do from her figures), it watches her airspeed against her
published stall with everything down. **At her warning** - the stall plus 5
knots or 5%, whichever is more, 14 CFR 25.207(c), the margin the stall
lessons give theirs - it hands the autopilot its stall recovery
(`speed_on_elevator`): the nose down until the wing is unloaded, full
power, and her approach speed flown (AFH FAA-H-8083-3C chapter 5; AC
120-109A). Five seconds at that speed and not descending, it gives back what
she was flying, from where she is: a height held is held where she has come
to, and a speed asked below her approach speed - what slowed her - is
raised to it. Nothing is noticed on the ground or the water, or while the
autopilot is already flying a recovery it was asked for. The 747-400 and
F-22A publish no stall speed and are not watched.

**Verified** (linux-release):
`the_ai_pilot_notices_a_stall_coming_and_recovers_from_it` (new,
registered): every aeroplane taught a stall (14 of 16; the 747-400 and
F-22A left out with no stall), given to the AI in her landing configuration
where she practises stalls and asked to hold her height at ten knots under
her stall. Height lost from the notice to the lowest:

| Aeroplane | slowest / stall, kt | lost, ft |
|---|---|---|
| 737-300 | 107.0 / 105.7 | 682 |
| 787-8 | 114.4 / 113.7 | 624 |
| A320 | 115.7 / 113.3 | 608 |
| A380 | **104.1 / 104.9 named** | 812 |
| B-2A | 98.3 / 95.4 | 96 |
| C172P | 49.5 / 46.0 | 75 |
| C182 | 52.8 / 49.5 | 76 |
| F-15C | 113.0 / 108.4 | 697 |
| F-35B | 125.7 / 122.2 | 892 |
| J-3 Cub | 37.0 / 33.0 | 62 |
| Learjet 35A | 97.3 / 96.5 | 374 |
| Mosquito FB.VI | 94.6 / 94.5, **never level at her approach speed, named** | 881 |
| PA-28 | 53.1 / 49.5 | 177 |
| Short S.23 | 68.4 / 66.3 | 274 |

**Seen to fail**: with the hook stepped over (`notice_a_stall` returning at
once) - "14 aeroplanes were not recovered by the AI; the first: 737-300:
the stall coming was not noticed" - and green with it restored.

**A level stall entry, measured and set aside.** The F-15C and F-35B are
handed to the recovery at their warnings already sinking because the
autopilot's altitude hold that enters the stall cannot raise the nose past
15 degrees, and in her landing configuration the F-35B needs 29 degrees of
alpha at her warning. Tried: a mode for a stall's entry in which the
envelope's 15 degrees bounds the flight path rather than the nose (the nose
may rise by the wing's angle of attack), let go once the wing is three
tenths of a degree past the angle its lift peaked at, so the entry is to
the stall and not into it. Every aeroplane at its warning:

| Aeroplane | at the warning, now -> with the level entry |
|---|---|
| F-15C | 522 -> **292 / 500**, 1.30 g |
| F-35B | 773 -> **419 / 500**, 1.13 g |
| A320 | 610 -> 584 |
| Learjet 35A, S.23, Mosquito | unchanged: 385, 184, never level |
| the other eight | within 4 ft of now |

Left thirty seconds it moved the figures both ways, all inside their
bounds: the A320 1.95 -> 1.90 g, the 787-8 1.20 -> 1.32 g, the F-35B 3,252
-> 4,502 ft (her bound 7,436), the C172P 75 -> 42 ft. Let go a degree past
the peak the A320 pulled 2.13 g; two degrees, 2.22; held level with no
letting go, the 737-300 was never recovered and the F-35B and S.23 pulled
2.9 g; holding the nose where it was at the peak, the 737-300 was never
recovered. The planted bug (the mode ignored) turned the warning check red
for the F-15C and F-35B, as it should. **Not committed because**
`a_stall_recovered_badly_is_named_in_the_debrief` goes red: the C172P left
35 seconds in her stall after a level entry finishes her lesson's recovery
stage (at her climbing speed) before she has lost 300 ft from its start, so
the late recovery is not named. Left 45 or 60 seconds the same. The
lazy recovery that test builds has to be rebuilt for a level entry, so
that it is late explicitly, before the entry can land. The patch is
described here; no copy of it is kept.

**Seen in the wider run, not this branch's**: on this base
`the_f22a_held_at_130_kt_on_the_autopilot_departs` (test_catalogue.cpp,
from #155) fails with no change of this branch's - she does not leave her
tables at 130 kt.

**The selftest hash does not move** (`182dd6c996e0ee4c`, linux-release):
it replays a pilot's inputs on the 172P, never slow enough to be noticed.
