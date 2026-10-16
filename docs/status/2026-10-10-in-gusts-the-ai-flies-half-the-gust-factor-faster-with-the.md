### In gusts the AI flies half the gust factor faster with the power on to the touch; four of fourteen land within limits on three seeds, ten are named, 2026-10-10 — item still open

**What is missing first.** Ten of the fourteen aeroplanes taught the
approach do not yet land in moderate turbulence within the limits below, on
at least one of three turbulence seeds, and are named in the test: the
737-300, 787-8, A380, F-15C and F-35B meet the runway harder than 360 ft/min
(a jet's flare does not arrest a gust's added sink: 568, 634, 443, 474 and
465 at their worst); the A320 and C172P are lifted in their flares (107 and
140 ft/min, seed 3); the C182 rises 1.18 ft after touching (seed 1); the
Mosquito bounces 3.4 ft (seed 2); and the S.23's flare is flown to her
centre of gravity, six feet over the hull that meets the water (she has no
wheel, `Aircraft::contact_points` counts none), and she meets it at 577
ft/min (seed 1). None of the 42 cases goes around any more. The light and
heavy loadings' two balloons are as they were. Two tails found on the way
are in COMPLETION_PLAN.md's Later.

**What changed** (sim/lander.cpp, from the FAA's Airplane Flying Handbook,
FAA-H-8083-3C, ch. 9, "Turbulent Air Approach and Landing", "Ballooning"
and "Bouncing During Touchdown"). None of it acts in air with no gust
factor: calm and steady-wind landings fly as before.
- **The approach is flown at her reference speed plus half the air's gust
  factor** (`Lander::approach_kts`, from `Aircraft::gust_factor_kt`, #161's),
  the additive capped at 15 kt (`most_gust_additive_kts`: Airbus limits its
  increment over VLS to 15 kt, and Boeing's study cut its wind additive from
  20 to 15, as code7700.com's "Approach Speed Additives" quotes them). The
  flare's height is worked from the speed flown.
- **The stabilized gate is widened by the additive**: fast is judged over
  the raised speed, slow under the bare reference (`unstabilized`'s
  `gust_additive_kts`). Judged round the raised speed, the C172P and C182
  were sent round "8 kt slow" and "7 kt slow" on seed 2 - slow of the raised
  speed, at it of the reference: the additive is the margin a lull eats.
- **The power stays on to the touch** - "the throttle retarded to idling
  position only after the main wheels contact the landing surface" - eased
  in the flare to no less than half what the flare found; held at all of
  it, the F-35B ballooned above her flare's height and the Mosquito floated
  past the touchdown zone. A jet is kept spooled (`jet_approach_idle`) into
  the flare, not let back to idle at twice the flare height: a 737-300 had
  0.03 of her throttle in a flare that put her on the runway at 638 ft/min.
- **A sink in the flare is cushioned with power** from 150 ft/min past what
  the flare wants, whatever the nose ("applying sufficient power to cushion
  the subsequent touchdown"); in calm air from 300, and only with the nose
  out, as before.
- **A firmer touch**: the flare's touchdown sink no softer than 150 ft/min
  (`gusty_touchdown_fpm`); a jet's own 200 kept.
- **The rollout judges whether she still flies by her airspeed along her
  nose, or her groundspeed where that is more.** On her groundspeed, a
  C172P touching at 55 kt into the 15 kt wind was rolling at 40 - "slow" -
  and had her stick most of the way back with her wing still carrying her:
  her nose rose from 4.5 to 10 degrees on the runway, she flew off it
  5.6 ft and came down on her tail. This alone ended every light
  aeroplane's bounce and airframe strike in the gusts test. **In gusts
  only**: so judged in steady air as well, the learnt landing from its
  gate's left corner in its most crosswind did not stop, not yet understood
  (a tail, Later).
- **Bounces and balloons** are as the lander already flew them, which is the
  AFH's: a balloon above the flare's height goes around ("when ballooning is
  excessive ... execute a go-around immediately"), and a bounce below it is
  flown down again by the flare's law - now with the power on in gusts.
- Tried and not kept: holding all the flare's power (above); and beginning
  the flare in gusts at the height her present sink needs (v^2 / 0.2 g), which
  moved the jets' hard touches about without removing them (A380 seed 1 443
  to 337, 787-8 seed 3 634 to 538, Learjet seed 3 then ballooned at 265).
  Flying the S.23's flare to her keel touched her at 61 ft/min in gusts, but
  in calm air at her model's weight her hull rose 1.27 ft after touching,
  against the calm test's 0.5: a tail, Later.

**Measured** (linux-release), 14 aeroplanes x 3 seeds in the gusts test's
air. Before, on seed 1, nine of fourteen failed: 737-300 473 ft/min, 787-8
534, C172P and C182 and PA-28 struck their airframes (the C172P 5.6 ft back
in the air), F-15C wrecked at 767, F-35B 360.4, Mosquito 458 and a balloon
at 169 ft/min, S.23 sent round by a balloon above her flare. After, the four
judged (B-2A, J-3 Cub, Learjet 35A, PA-28) on every seed, least margins:
sink 66 ft/min (B-2A, seed 1: 294), touchdown zone 447 m, rise 0.61 ft
(PA-28, seed 3), flare climb 26 ft/min (Learjet, seed 1: 74).

**Verification:**
- New, six tests (each seed in two halves of seven, each 1 to 11 s in
  linux-release; the test they replace took 220 s on CI's linux-debug for
  all fourteen, so about 110 s a half there - not measured in a local
  linux-debug):
  `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_{1,2,3}_{first,second}_half`.
  Fifteen knots down the runway, severity-3 turbulence, gust factor 15 kt
  (`world::gust_spread_of_severity(3)`), the turbulence seeded 1, 2 and 3
  (`Aircraft::set_weather`'s new `turbulence_seed`, 1 by default as
  before): each aeroplane not named must land - not go around - inside the
  touchdown zone, no harder than 360 ft/min (NASA's go-around criteria
  study's 6 ft/s, the calm tests' bound), unwrecked, upright, risen no more
  than 1 ft (the AFH's "very slight" bounce), climbing no faster than
  100 ft/min in her flare, and stop. Coverage asserted: fourteen taught,
  each half's judged and named making up its seven. An aeroplane that fails
  any seed is named whole, since another platform's floating point gives
  her other gusts on a seed she passes here. **Seen red** on the lander
  before (seed 1): the PA-28, judged now, struck her airframe.
- Replaced: `every_aeroplane_flown_down_in_gusts_reaches_the_runway_or_goes_around`,
  which judged nothing. tests/ci_costs carry the six at half its cost each.
- `an_approach_flown_well_in_gusts_is_not_sent_round_by_the_stabilized_gate`
  now has the gust factor flown by the lander (no 5 kt added by the test):
  all 14 pass.
- 158 tests matching land, lander, learnt, go_around, approach, gust, flare,
  circuit, taken_back and handed pass (linux-release; the live ones skip).
- The selftest hash does not move: 182dd6c996e0ee4c (it flies no landing).
