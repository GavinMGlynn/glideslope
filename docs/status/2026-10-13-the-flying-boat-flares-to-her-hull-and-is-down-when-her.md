### The flying boat flares to her hull, and is down when her keel is in the water; in gusts her planing hull still throws her off it, 2026-10-13 — item still open

**What is still missing, first**: in moderate turbulence the Short S.23's
planing hull throws her off the water after she touches, on three of the
gust test's eight seeds - she rises 1.26 ft (seed 1), 0.81 (seed 6) and
1.57 (seed 8) over where the water first bore her, against the gust
limit's 1.0 less its 0.25 margin - so she stays named in the gust tests.
She no longer touches hard (the most is 312 ft/min, seed 8, against 571
before). And in calm air, judged and passing, her hull still porpoises on
the step after the touch: the model's planing lift (`hydro/hull-lift-lbs`,
its tweak factor 20, clipped at 20,000 lb) rises from nothing to its clip
in about 0.3 ft of immersion with nothing to damp it, and she skips in
diminishing hops of 1 to 2.5 ft, down to 35 kt, her elevator fully back -
all of them below the height the water first felt her, which is what the
test measures a rise from. A flight-model property, measured, not fixed.

**The calm-air rise, traced** (linux-release, the S.23 at her model's
34,717 lb, calm, every step logged). Her model's water forces begin with
her keel still about three feet above the surface: `hydro/active-norm`
goes to 1 on a few hundred pounds of buoyancy with her hull reference
point 8.95 ft up and her centre of gravity 13.1, while her planing lift
begins with the CG at 9.1. `Aircraft::in_water` is that first trace, and
the lander took it for the touch. So the earlier attempt's flare to her
keel ended three feet early: the rollout closed her throttle and held her
attitude, she sank on to the water from 115 to 150 ft/min, her planing lift
rose to its 20,000 lb clip and pitched her nose up 4 to 8 degrees, and she
was thrown 5.6 ft back up, 1.26 ft above where the water first felt her
(`rose 1.260367 ft`, against the calm limit's 0.5). It was neither the
flare's power (closed, 0.00, from 29 ft) nor the keel height used too late;
it was the touch taken too early.

**How a flying boat is landed** (FAA, Seaplane, Skiplane, and Float/Ski
Equipped Helicopter Operations Handbook, FAA-H-8083-23, ch. 6, "Normal
Landing"): the nose raised smoothly to the touchdown attitude - "almost the
same as for taxiing on the step" - back pressure against the nose's
dropping as the hull meets the water; "when the seaplane is definitely on
the water, close the throttle and maintain the touchdown attitude until
the seaplane begins to come off the step". The lander's flare already
closes the power by its end in calm air and its rollout holds the attitude
she touched at; what was wrong was where it thought the water was.

**What changed** (sim/lander.cpp, sim/aircraft.cpp):
- **The flare's reference point comes from her contacts.** Her main wheels,
  as before; where her model has no wheel and has JSBSim's hydrodynamics
  (`Aircraft::alights_on_water`), every undercarriage contact
  (JSBSim's BOGEY: `Aircraft::ContactPoint::bogey`, new) - for the S.23 her
  keel's forward, step and aft points and her floats' keels - the lowest of
  them at her attitude and bank now. Before, a flying boat had no wheels,
  `wheels_hang_ft` was 0, and her flare was flown to her centre of gravity,
  nine feet over her keel. No name: any model that can be in the water and
  has no wheel gets it. A landplane never does: a model naming no main
  wheel is a fault, and a fixed skid written as a BOGEY would otherwise
  hang her flare from it unnoticed (`Lander::lands_on_wheels`,
  `lands_on_hull`).
- **Down is the keel in the water** (`Lander::down`): weight on a wheel, or
  the water's forces on her with the lowest of what she lands on at or
  below the surface. Two places ask it: the touch (`notice_the_touch`, and
  so the rollout that follows it) and a pilot's hand-back (`still_landing`).
  So `touchdown_agl_ft_`, the height a bounce is judged from, is now her
  centre of gravity's height when her keel reaches the water, no longer
  where the water's forces first acted, about 3 ft higher. Her height,
  JSBSim's `position/h-agl-ft`, is measured to the water's surface: the
  terrain's elevation there, which is the water's.
- The gust test's naming of the S.23 now says what is wrong.
- A new test, `every_landplane_flares_to_her_main_wheels_and_a_flying_boat_
  to_her_hull`: the lander finds main wheels in each of the thirteen
  landplanes taught the approach, and her hull in the one flying boat
  (coverage asserted, 14). Seen red: with `lands_on_wheels` forced false,
  "13 wrong, the first: 737-300, a landplane, is not flared to her main
  wheels".

**Measured** (linux-release). Calm, her model's weight: the hull meets the
water at 75 ft/min, 463 m along, and she rises nothing over where the
water first felt her (before, the keel law: 1.26 ft, red). In gusts, eight
seeds, touched at 37, 181, 105, 180, 121, 180, 51 and 312 ft/min, 256 to
426 m along, flare climbs no faster than 71 ft/min; risen 1.26, 0.00,
0.00, 0.00, 0.70, 0.81, 0.16 and 1.57 ft. Before, flown to her centre of
gravity, she met the water at 571 ft/min on seed 1.

**Verification:**
- **Seen red on the old law**: with the flare flown to her keel and the
  first trace of the water taken for the touch,
  `the_ai_flies_every_approach_at_the_speed_for_its_models_own_weight_and_lands_within_its_limits`
  failed on the S.23 alone, "rose 1.260367 ft ... more than 0.500000";
  green with the touch at her keel. Seen again on this tree: `Lander::down`
  reverted to `in_water()` alone, the same test failed the same way, 1.260367
  ft; restored, green.
- Seed 1's rise in gusts, 1.26 ft, is its own run's (1.257249 ft in the
  gust test's output), not the calm figure's 1.260367.
- The landing, water, lander, gust, flare and lesson tests pass
  (linux-release, `ctest -R` over every test naming the S.23, water, a
  landing, the lander, gusts, the flare, a hull, an instructor or a
  lesson), 136 tests in linux-release with the server built: 135 passed and
  one skipped (it asks Anthropic live, with no key). The one client test
  named for the learnt landing on final, which needs the client built, is
  left to CI.
- The selftest hash does not move: 182dd6c996e0ee4c (it flies a C172P).
