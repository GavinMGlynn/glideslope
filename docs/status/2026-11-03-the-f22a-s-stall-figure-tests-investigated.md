### The F-22A's stall-figure tests investigated: no name comes off yet. She pitches up past her stabilators at 23 to 24 degrees of alpha at idle, short of her stall warning - 2026-11-03

**What is still not true first.** Every naming the item lists is still on.
No change to her model, the autopilot or a test is kept. The figures each is
held to are the same as on 2026-11-02
(2026-11-02-the-f22a-s-stall-figure-and-landing-measured.md):
- the recovery at her warning: 2,964 ft against 500, 4.72 g against 2;
- left thirty seconds: named, not judged (16,295 ft and 20.64 g with FMA,
  31.21 g on CI, never recovered on Windows);
- the stall watch: noticed at 100.3 kt, slowest 59.4 kt against her 90.0,
  2,728 ft lost; and the demonstration's "Recover with the least height you
  can";
- the strike margin: she touches at 14.29 degrees, 2.24 short of her 16.54
  strike, against the 2.5 asked;
- the wet walking pace at 0.5 kt: 29.93 s rolled of 30;
- the autopilot's heading under 127 kt: 1,321 ft lost at 117 kt and 349 at
  122, against 300.

**The cause of the stall namings, measured** (linux-release, a trace of
`fly_a_stall` at her warning, not committed). She reaches the alpha her
stabilators can hold before she reaches her stall warning. With her
throttles closed for the entry and her gear down (so no thrust vectoring:
the model's `fcs/tvc-inhibit`, as the YF-22's gear handle did), she slows
under control to 120.7 kt at 19 degrees of alpha. Then she pitches up on her
own:

| time | KCAS | alpha | pitch rate | stabilator |
|---|---|---|---|---|
| 69 s | 120.7 | 19.0 | 3.6 deg/s | 0.20 rad nose down |
| 70 s | 115.3 | 24.0 | 8.1 deg/s | 0.30 |
| 71 s | 105.5 | 35.5 | 20.8 deg/s | 0.52, the stop |
| 72 s | 82.4 | 63.2 | 42 deg/s | 0.52 |

Her warning, 103.2 kt at her model's 61,570 lb (stall+8 on 95.2), comes
only after that. She is handed over at 44.7 degrees of alpha and tumbles,
alpha going round through -144 and +109 degrees. Her pitch rate is held near
-55 deg/s for tens of seconds. That is why her thirty-second figures scatter
by platform. Two things make it:
- **Her airframe is unstable in pitch.** The aerodynamic reference point
  (x = 417.6 in) is 28.1 in ahead of her empty centre of gravity (445.7 in).
  Above 25 degrees, full nose-down stabilator (0.5236 rad) leaves CmDh at
  only -0.05 to -0.06, times 1.5. Her flight controls work that stabilator
  only through the pitch-rate integrator. At low speed it winds too slowly
  to catch the pitch-up.
- **Her stall figure is a power-on one.** The 90.0 KCAS is trimmed level at
  1 g with whatever thrust holds her, at 27.5 degrees of alpha
  (src/sim/figures.cpp, `steady_flight_speed`). At that alpha the thrust's
  vertical part carries some of her weight. The lesson stalls her at idle,
  and at idle she cannot fly as slowly as her warning before she departs.
  14 CFR 25.103(b)(1) measures a stall speed with the engines at idle, or
  at zero thrust at the stall speed. Her figure was not measured that way.

**Tried, and not kept: a gear-down alpha limit in her flight controls**
(tools/make_f22.py, reverted). Past an onset alpha, a nose-down pitch rate
per degree went into the loop's rate error, and a stabilator term per degree
after the loop. Fly-by-wire fighters limit alpha; an Air Force mishap report
of April 2005 found her flight controls "allowed the aircraft to exceed set
angles of attack and G-force limits". No published value was found.
- Onset 25 degrees, rate term alone: she departs as before, 44.5 degrees at
  the warning, 19,901 ft lost.
- Onset 24 degrees, with a stabilator term of 1 per degree (full travel 5):
  she departs, 42.4 degrees at the warning.
- Onset 20 degrees (1 per degree) or 21 degrees (2.5 per degree): she no
  longer departs. Held at 22 to 23 degrees and idle she cannot slow below
  about 113 kt. The entry sinks her 20,000 ft to the ground and is handed
  over there, and the stall watch never notices a stall (its warning is
  100.3 kt). Left thirty seconds she now recovers, losing 1,989 ft against
  1,179 at 21 degrees and 4,674 at 20. After unloading, the autopilot
  dives her to 13 degrees nose down at full power, up to about 600 kt,
  before she is level.

So the window between the alpha she can be held at and the alpha her warning
needs closes on her model as it is. A limit cannot open it.

**The wet walking pace, measured.** At 0.5 kt the autobrake reaches 0.45
(its ceiling there) within 0.22 s. The test's push is rate-limited and
catches it only at 0.30 of her weight. In between her speed dips to 0.244
kt for steps 32 to 40, eight steps under the quarter knot that counts as
rolling, and she is back at 0.50 kt by 3 s. The other runs and every other
landplane hold. The test's push and her brakes meet there; nothing in her
own flying is out.

**The strike margin and the heading test** come from the same figure. At
1.3 times a 90 kt stall she flies her final at 17 degrees of alpha and 14.3
of nose. Held level there the autopilot needs more than its 15 degrees of
nose.

**What would close it, for the next round.** Her stall figure measured as
14 CFR 25.103(b)(1) measures one, at idle or zero thrust, and her alpha held
where her stabilators still control her. Either would move her approach
speed, her measured landing distance (6,300 ft), the recorded copilot
exchange `cbd-orbit-f22-anthropic.jsonl` and her gust namings. Each must
then be measured and recorded again. That did not fit this item's time box.

**Searched** (2026-11-03): the F-22's power-approach alpha limit, her
approach alpha and speed, and whether her thrust vectoring works with the
gear down. Found: a PPRuNe post relaying the YF-22 mishap report. Gear down
disables vectoring, and power-approach mode commands 5.25 deg/s at full aft
stick. Also the Air Force's April 2005 mishap release quoted above. AFMAN
11-2F-22A volume 3 refused the download (HTTP 403). No figure was found that
could be cited for her approach or limit.

**Verification.** linux-release, each test run from the test binary with no
display:
`every_aeroplane_recovered_at_the_first_sign_of_a_stall_loses_no_more_than_its_lesson_allows_within_2_g`,
`every_aeroplane_left_thirty_seconds_in_a_stall_is_recovered_within_2_g_and_the_height_its_speed_and_sink_need`,
`the_ai_pilot_notices_a_stall_coming_and_recovers_from_it` (each limit
above) and
`every_landplane_held_at_every_pace_to_walking_pace_at_idle_on_a_wet_runway_keeps_her_autobrake_bounded_and_her_nose_up`.
With every experiment reverted, `python3 tools/make_f22.py --check` passes.
This round commits only this file and the plan item's text.
