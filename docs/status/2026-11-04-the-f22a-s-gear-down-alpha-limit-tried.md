### The F-22A's alpha limited with her gear down, and her stall measured there: both stall recoveries and her rise after the touch come off; the stall watch, a wet walking pace, a wet crosswind roll-out and her gusts still named - 2026-11-04

**Third round first.** It supersedes the second round's list of names,
which follows. The first round's note comes last.

**What is still not true.** The item stays open. Still named, with their
figures (linux-release with FMA, and the same in an x86-64-v2 build):
- **the stall watch**: noticed at 126.8 kt, she slows to 118.2 against her
  120.8 at her weight, and loses 303 ft. At her limit she sinks at about
  0.8 g, so she flies under her 1-g stall there;
- **the wet walking pace** at 0.5 kt: 29.93 s of 30, unchanged from main;
- **the wet crosswind roll-out**: 6.13 m from the centreline (2.35 dry).
  It is named as it was on main, where it was 7.37 m;
- **her gusts**, on forty seeds:
  - she goes round for touching past the zone on nine;
  - she is wrecked on seventeen of the thirty-one she lands;
  - she touches at up to 980 ft/min.

  No balloon now.

**Taken off this round**: the rise after her touch at her model's own
weight. It was 0.59 ft and is now 0.00.

**The roll-out regression of the second round is fixed.** That round
named three roll-out tests: the crosswind roll-out, the wet side grip and
the wet stays-on-runway. In each she lifted off and struck her airframe.
The cause is in two parts:

1. **Her speedbrake lifted her.**
   - The pinned model's speedbrake channel is "borrowed from F-16". Its
     dCLsb added 0.35 to her lift coefficient at full deflection, the same
     0.35 as her flaps' dCLflap.
   - Glideslope's roll-out opens a jet's speedbrake on the roll as her
     spoilers (sim::Lander).
   - Handed over at 133.6 kt (0.9 times her new 148.3 kt approach, the
     test's rule), that extra lift took her off the runway. At main's 123.8
     kt it had not.
   - It is also what lifted her 0.6 to 3.4 ft after her touch at her new
     approach speed. The second round blamed her stabilators for that and
     tuned `nose_lowered_degps` against it. That attribute is gone.
   - **Fixed in tools/make_f22.py**: dCLsb is 0, and CDsb, the
     speedbrake's drag, is kept.
   - Sources: f-16.net forum accounts describe her speedbrake as flaperons
     down, ailerons up and rudders flared. They are not citable as figures,
     so the value is labelled not sourced. No Lockheed or Air Force
     description was found. Searched 2026-11-04 for F-22 landing technique,
     aerobraking, nose lowering and her speedbrake.
2. **Her nose swung under the roll-out's attitude law.**
   - She is handed over with her nose 4 degrees up, because her take-off
     run begins lifting it from 111 kt.
   - The roll-out's attitude law, through her rate-command stick, beat
     her stick from stop to stop and swung her nose from 0 to 10 degrees.
     Even without the speedbrake's lift, she lifted off wet and hit the
     ground at 638 ft/min.
   - **Fixed in sim::Lander, `lower_the_nose`**: where her figures give
     `roll_out_stick_per_degps`, her stick is asked a pitch rate. That rate
     is the one wanted, plus a degree a second per degree off the attitude
     wanted.
   - **Tuned, not sourced**: 0.07. At 0.05 to 0.1 she stays on her wheels,
     6.3 to 5.9 m wet. At 0.12 to 0.2 she lifts off wet again.
   - Every other aircraft gives none, and flies the attitude law as
     before.

**What else moved**: her landing figure stays 8,400 ft. She now stops at
2,490 m, against 2,492 before. The recorded copilot exchange plays back
unchanged.

**Verification (third round)**:
- 234 tests, in the linux-release build and in an x86-64-v2 build: the
  stall, landing, roll-out, speedbrake, heading, plan floor, recording,
  selftest and committed-file tests. All pass, and the live recording
  test is skipped.
- Her forty gust seeds, flown for her alone (a local test, not committed).
- `python3 tools/make_f22.py --check` passes.
- **The full linux-release suite was not run**: another agent's ctest held the machine for the whole round.
- Windows (`tools/windows_build.sh windows-debug`, this commit): the 50 tests matching her stall, landing and roll-out tests all pass, and the live recording test is skipped.

---

### Second round: the limit taken through; its list of names is superseded above

**What is still not true.** The item stays open.
- **Still named, with figures** (linux-release, FMA):
  - the stall watch: noticed at 126.8 kt, she slows to 118.2 against her
    120.8 at her weight, and loses 303 ft;
  - the rise after her touch at her model's own weight: 0.59 ft against
    0.5;
  - the wet walking pace at 0.5 kt: 29.93 s of 30, unchanged, as the
    first round found (the test's push and her brakes, not her stall);
  - the crosswind roll-out, now **named whole, dry and wet, and not
    judged**. Handed over at 133.6 kt, nine tenths of her new approach
    speed, her nose swings on her wheels under the roll-out's pitch law.
    She lifts off, pitches up past 80 degrees and strikes her airframe:
    29.06 m across dry, 30.50 wet. Before, she was 7.37 m wet. The same
    roll-out names her, for the wreck, in two more tests: the wet side grip
    (25.32 ft/s wet against 25.40 held dry) and the wet runway's
    stays-on-it (30.50 m). Each fails "take her name off" once she rolls
    out whole.
  - her gusts, measured again on forty seeds: she fails all forty, as
    she did before.
- **Not done in this round**: Windows (`tools/windows_build.sh`) and the
  full suite (see Verification).

**Taken off, each passing with her name gone**:
- the recovery at her warning: handed over at 122.1 kt and 17.8 degrees
  of alpha, she loses 187 ft against 500, at 1.25 g (it was 3,275 ft and
  3.74 g);
- left thirty seconds: judged as every aeroplane is, 639 ft against
  2,144, at 1.62 g. It was named and not judged, its figures scattering by
  platform. `an_autopilot_engaged_on_an_aeroplane_already_stalled_recovers_it`
  and `engaging_the_stall_recovery_and_letting_it_go...` judge her again
  too (the latter said "f22 recovered: take her name off" until the name
  went).

**The limit** (tools/make_f22.py, `with_gear_down_alpha_limit`). **Tuned,
not sourced.** Searched again 2026-11-04, for flight test papers, AIAA
and her flight controls' power-approach law. Found: NASA's HARV high-alpha
work, an Eidetics two-stage alpha limiter concept (NTRS), and a Korean T-50
power-approach study. Nothing gives the F-22's limit or her approach speed.
How it works:
- past 17 degrees of alpha, with her gear down, her pitch loop is asked
  0.06 rad/s nose down per degree in its rate error;
- her stabilators are given 0.1 of their travel nose down per degree after
  the loop.

The first round's table says how those values were chosen. At those values
she reaches 20.8 degrees at most in the stall lesson, against the 22
degrees where she departed.

**Disarmed while glideslope trims her** (`fcs/alpha-limit-armed`,
sim/aircraft.cpp). JSBSim's trim runs her flight controls at every angle
it tries, up to 45 degrees. Armed through the trim, the limit wound her
pitch loop's integrator to its stop and set her held trim a full stabilator
out (-0.854 against 0.146). Started on her final, she held her stick fully
back and touched at 814 ft/min. With the limit disarmed for the trim she
touches at 154.

**Her stall, measured at the limit as 14 CFR 25.103(a) has one**
(src/sim/figures.cpp, `limited_stall_speed`, figure `alpha_limited="1"`).
The calibrated airspeed at her greatest lift coefficient is divided by the
root of her load factor there. At her limit she sinks at about 0.8 g, her
stick at its aft stop and her nose rising no further: 25.201(d)(3)'s stall.
- **114.1 KCAS**, the figure: 113.78, 114.13, 114.41 and 114.54 from 160,
  200, 240 and 280 KCAS.
- **Seen to fail**: measured by the old `departure="1"` instead, it throws
  "she never pitched up of her own accord".

**What moved, measured again**:
- her approach speed: 148.3 kt at her combat loading, 157.0 at her model's
  61,570 lb;
- her landing figure: **8,400 ft** (`tools/measured_landing.py`; it was
  7,300). She touched 348 m in at 154 ft/min, nose 8.4 degrees up, and
  stopped 2,492 m past the threshold. The copilot test's table is changed
  to match;
- her plan speeds' slowest: **157 kt**, her approach speed at that weight
  (`glideslope_cli plan-speeds f22`; it was 146). The fastest is unchanged,
  360;
- **her nose lowered faster after the touch**: her figures'
  `nose_lowered_degps="6"`, read by sim::Lander. Tuned, not sourced. At
  146 kt, lowered at 3 degrees a second, her stabilators lifted her 2.3 to
  3.4 ft off her main wheels. That broke the approach lesson, the
  instructor's demonstration and five hand-landing tests. At 4 degrees a
  second the rise was 1.1 ft; at 5 and 6 there was none. Every other
  aircraft keeps the lander's 1.5.

**The thirty-second dive** that the first round found did not appear at
these values: she recovers in 639 ft. It hangs on the autopilot's learnt
stall alpha, and it was not fixed.

**Verification (second round).** linux-release, no display:
- each test above was run from the test binary, with its f22 line read;
- 89 tests matching her stall, landing, roll-out, recording, plan floor,
  selftest and committed-file tests (`ctest -LE nightly -j4 -R ...`): all
  pass, and the live recording test skipped. Run in the linux-release
  build and in an **x86-64-v2 build (no FMA, as CI)**. Her figures in the
  v2 build are the FMA build's:
  - her stall: 113.78 to 114.54 KCAS;
  - at her warning: 188 ft and 1.26 g;
  - left thirty seconds: 639 ft and 1.62 g;
- her forty gust seeds, flown for her alone (a local test, not
  committed): the figures in her gust naming;
- the recording `cbd-orbit-f22-anthropic.jsonl`, made again with
  `GLIDESLOPE_LIVE_MODEL=1` because its prompt carries her plan speeds and
  landing figure. Claude Haiku 4.5 planned a take-off, the CBD
  waypoint at 225 kt and an orbit at 300 kt, 18,752 m round. The
  playback passes, and the recording holds no key;
- `the_f22a_held_at_130_kt_on_the_autopilot_departs` turned red with the
  limit: she keeps inside her tables at 130 kt with her gear down. The
  test is renamed, `..._with_her_gear_down_keeps_inside_her_tables`, and
  holds that. Its CI cost lines are renamed with it;
- **Not run**: the full suite, and Windows (`tools/windows_build.sh`).
  Both are left for the next round, inside its time box. A wide ctest run
  was begun and stopped at 68 tests, because it would not finish in time.
  Its failures are each fixed or named above.
- `python3 tools/make_f22.py --check` passes.
- The selftest flies the C172P alone, and nothing it flies changed.

---

### First round: a gear-down alpha limit tried, nothing committed but this note

**What is still not true.** The item stays open, and every name it lists is
still on, at the figures of
2026-11-03-the-f22a-s-stall-figure-tests-investigated.md. This round stopped
at its hour without a change that keeps the tree green. No model, test or
figure is changed; this file and nothing else is committed.

**What was tried** (linux-release with FMA only; no x86-64-v2 build and no
Windows build were made). A gear-down alpha limit in her flight controls
(tools/make_f22.py, not committed): past an onset alpha, with her gear down,
a nose-down pitch rate per degree in her pitch loop's rate error, and a
nose-down stabilator per degree after the loop. **Tuned, not sourced**: no
published limit or gain was found. Searched again on 2026-11-04 (her power
approach alpha limit and her gear-down control law): only the YF-22 mishap
report as relayed on PPRuNe, and the Air Force's April 2005 mishap release,
both already cited on 2026-11-03.

Her stall at idle was then measured as 14 CFR 25.103(a) measures one, not by
the project's own departure rule: the same flight as `departure_speed`
(throttles closed, gear down, her autopilot holding her height), but at the
greatest lift coefficient, its calibrated airspeed divided by the root of the
load factor normal to the flight path. At her limit she sinks at 0.72 to
0.89 g with her stick at its aft stop. That is 25.201(d)(3)'s stall: the
pitch control at its stop and the nose rising no further. Each entry speed
from 160 to 280 KCAS gave the figure within 0.8 kt.

| onset, rate per degree, stabilator per degree | her stall, KCAS | at her warning: lost / g | thirty seconds: lost (bound) / g | most alpha before the recovery |
|---|---|---|---|---|
| 18, 0.05, 0.1 | 109.4 to 109.9 | 247 ft / 1.27 | 637 (2,084) / 1.61 | 22.6 |
| 17, 0.06, 0.1 | 113.8 to 114.5 | 187 ft / 1.25 | 639 (2,144) / 1.62 | 20.8 |
| 17.5, 0.06, 0.1 | 112.4 to 113.0 | 188 ft / 1.24 | 635 (2,102) / 1.62 | 21.4 |
| 17, 0.06, 0.15 | 114.7 to 115.0 | 185 ft / 1.26 | 4,511 (2,194) / 1.60 | 20.6 |
| 16, 0.05, 0.1 | 114.7 to 114.8 | 222 ft / 1.31 | 5,250 (2,147) / 1.65 | 20.4 |
| 18, 0.08, 0.1 | 114.6 to 115.4 | 2,602 ft / 1.51 | 4,738 (2,237) / 1.62 | 21.0 |
| 18, 0.10, 0.05 | departed from 160 KCAS | 231 ft / 1.27 | not recovered | departed |
| 19.5, 0.15, 0.1 | departed: a limit cycle | - | - | - |

So a limit of about 17 degrees holds her clear of her 22-degree pitch-up,
and with her stall measured at it, both stall recoveries come within their
bounds: 17/0.06/0.1 and its neighbour 17.5 pass both. A stiffer loop
oscillates and departs.

**Why some thirty-second recoveries lose 4,500 ft or more.** The
recovery reaches its speed within about 650 ft. Then, pulling out at
1.6 g near 156 kt, she is at 17 to 18 degrees of alpha. The autopilot's
stall alpha is learnt from where her lift peaked, and under the limit that is
the limit itself. So it takes her for stalled again
(sim/autopilot.cpp, `seen_to_stall_`) and unloads her to 1 degree of alpha
at full power. She dives from 160 to 600 kt before she is level. That is
the generic autopilot meeting a lift peak that the limit made, not the
wing's.

**Why it was not committed.** Her stall at the limit is about 114 KCAS,
not 105.8. Her approach speed would then be about 148 kt, 1.3 times it.
That moves everything that the change of 2026-11-03 moved, all to be
measured and recorded again:
- her measured landing distance and the copilot's table;
- her plan speeds;
- the recording `cbd-orbit-f22-anthropic.jsonl`;
- her gust namings, which another item holds.

None of the five landing and roll-out names was measured under the limit.
Nor were the x86-64-v2 build or Windows, where her stall figures have
scattered before. That did not fit the hour.

**For the next round**: the limit at 17, 0.06 and 0.1, and the 25.103(a)
measurement (a figure attribute beside `departure`, which throws if she
pitches up past her limit); then the cascade above. Then the stall watch,
the five landing names and an x86-64-v2 build. Watch the thirty-second
recovery's dive: it hangs on the autopilot's learnt stall alpha.

**Verification.** This round's figures are each from
`glideslope_tests the_f22a_s_stall_at_idle_landing_configured_does_not_hang_on_its_entry_speed`
(with its bounds widened, locally) and from a local trace of `fly_a_stall` for
her alone at the warning and at thirty seconds, linux-release, no display.
Nothing measured here is committed; `python3 tools/make_f22.py --check`
passes on what is.
