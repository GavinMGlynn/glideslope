### The Mosquito's nose-over brake level pinned and named, 2026-10-17 — item done

**What is still not true first.**
- **The level is unmeasured on a real aeroplane.** Nothing found gives a
  Mosquito's brake lever travel against deceleration, or a recorded nose-over
  on the brakes. 0.60 (0.65 at the two heaviest loadings) is this model's
  figure, from her estimated undercarriage (`tools/make_mosquito_fb6.py`),
  not a Mosquito's.
- **The Pilot's Notes warning against harsh braking was not found.** The
  decision cited it; the edition checked (the FB 6 Pilot's Flight Operating
  Instructions, zenoswarbirdvideos.com's scan, whose text is A.P. 2019E-PN)
  has no such sentence: its only words on the brakes are para. 19, the lever
  on the control column, differential braking by the rudder pedals, 80 to 100
  lb/sq in at each brake. A warning in another edition (1944, 1950, the T.3's)
  may exist; nothing here relies on it, and the test's comment says it is
  unconfirmed.
- **Calm air, 10 kt, stick fully back only.** The existing crosswind test
  (`every_landplane_braked_to_a_stop_in_a_15_kt_crosswind_...`) names her
  tips at 0.6 to 1.0 at her model's own loading and is unchanged.

**The decision (owner, 2026-10-17).** Her tipping on the brakes is treated as
her real behaviour, her model left unchanged. A tail-wheel aeroplane with
her centre of gravity 16 in behind her main wheels and high above them
pitches over them once her braking friction passes the ratio of her
distance aft to her height; below the pinned level she stops with a pitch
rate under 13 deg/s at every loading, so no gear impulse shows there.

**The new test**,
`the_mosquito_braked_to_a_stop_from_10_kt_in_calm_air_tips_onto_her_nose_from_one_brake_level_at_every_loading`:
at each of seven loadings (her model's own and the six her figures give),
140 runs at 0.05 to 1.00 in twentieths of brake, run up to 10 kt by the
take-off steering with the stick fully back, braked, throttle closed, to a
stop and five seconds standing. Tipped is her pitch going below the
horizontal (she stands at 11 to 12 degrees nose up). Both sides asserted at
every level: below the pinned level she does not tip, at or above it she
does; every run stops; 140 runs and 61 tips counted.

**Measured (linux-release), the first level that tips:**

| Loading | Total (lb) | Tips from | Pitch at 0.55 / 0.60 / 0.65 (deg) |
|---|---|---|---|
| her model's own | (her model's) | 0.60 | 9.5 / -3.1 / -6.4 |
| HX809 | 20,400 | 0.60 | 9.6 / -2.5 / -6.3 |
| HJ679 | 21,020 | 0.65 | 10.3 / 2.4 / -6.1 |
| B.IV maximum | 21,462 | 0.65 | 10.7 / 5.3 / -5.8 |
| Pilot's Notes full | 20,500 | 0.60 | 9.6 / -2.4 / -6.3 |
| Pilot's Notes 18,000 | 18,000 | 0.60 | 9.2 / -4.8 / -6.6 |
| Pilot's Notes 17,000 | 17,000 | 0.60 | 9.6 / -1.5 / -6.7 |

At 0.55 she dips 1 to 2.5 degrees at 5 to 13 deg/s and stays on her
wheels. Past the level her pitch rate runs 28 to 62 deg/s, her nose goes to
-5.8 to -9.6 degrees, and she stops.

**Verification.** Seen red twice: the own loading pinned at 0.65 failed at
"own at brake 0.60: tipped below her pinned level", and HX809 pinned at 0.55
failed at "hx809 at brake 0.55: did not tip at her pinned level"; both
reverted. Also run in a `-march=x86-64-v2` build (see the pull request),
since tipping is edge-sensitive.
