### The F-22A's wet crosswind roll-out traced: her side force outruns the roll-out's track rudder, not her nose wheel; still named at 6.13 m - 2026-11-04

**What is still not true.** The item ("The F-22A flies her stall-figure
tests") stays open. Her wet crosswind roll-out is still named, at 6.13 m
from the centreline wet (2.35 dry), as on main. Her gust landings are still
named too. No model, code or figure is changed. Only this note, the test's
comment and the plan's text are.

**What the test means.** `every_landplane_rolled_out_dry_or_wet_as_15_kt_of_crosswind_arrives_stays_within_5_m_of_the_centreline`
hands each landplane to the landing's roll-out (`Lander::on_its_roll`) and
holds her to a stop within 5 m of the centreline. So the AI is to stop her,
not take off.

**Why she goes 6.13 m.** Traced on linux-release, every quarter second of
her wet roll-out:
- handed over at 133.6 kt, her nose 4 degrees up from her take-off's
  rotation, her nose wheel off the runway for 3.0 s;
- as 15 kt arrives across, her sideslip is 6.4 degrees and her side force
  is 11,000 lb, while her wheels bear 15,000 lb of her 61,400. Her model's
  CYb is -1.79 a radian (the pinned JSBSim f22's);
- the F-15C, handed over in the same way with her nose 5.7 degrees up, has
  2,100 lb of side force against 16,300 on her wheels. Her rudder comes in
  at once, about -0.5, and holds her heading;
- the F-22A's rudder stays under 0.1. The roll-out steers by her track, at
  0.5 of rudder for each degree of track error, and that turns her into the
  wind too slowly. She slides across at up to 22.8 ft/s, and her tyres'
  side force takes 5 s to build to 8,000 lb.

**Lowering her nose sooner is not it.** Tried for her alone through
`lower_the_nose`:

| nose lowered in | to | wet, m | dry, m |
|---|---|---|---|
| 3 s (as on main) | -1 deg | 6.13 | 2.35 |
| 1 s | -1 deg | 5.99 | 2.29 |
| 0.5 s | -1 deg | 6.14 | 2.28 |
| 3 s | -5 deg | 5.67 | 2.10 |
| 0.3 s | -10 deg | 5.27 | 1.83 |

Her nose wheel is down sooner, but the side force is already acting on her.

**A track rudder of her own clears this test, and breaks another.** Tried
as a figure of hers, tuned, not sourced. Every other aircraft kept the
lander's 0.5, and in this test their lines were the same to the hundredth.
- In this test, wet and dry:

  | her gain | wet, m | dry, m |
  |---|---|---|
  | 0.5 (main) | 6.13 | 2.35 |
  | 0.7 | 5.14 | 1.99 |
  | 0.8 | 4.75 | 1.85 |
  | 1.0 | 4.11 | 1.62 |
  | 1.5 | 3.06 | 1.24 |
  | 2 | 2.42 | 1.00 |
  | 3 | 1.69 | 0.73 |
  | 5 | 1.03 | 1.64 |
  | 10 | 20.01 | struck her airframe |

- In `an_approach_taken_back_at_the_touch_is_landed_to_a_stop`, in calm
  air at 145 kt:

  | her gain | bank after touching |
  |---|---|
  | 0.5 (main) | 0.0 |
  | 0.8 | 15.6, fails |
  | 1.0 | 1.9 |
  | 1.2 | 1.8 |
  | 1.3 | 3.0 |
  | 1.5 | 16.9, struck her airframe |
  | 2 | 16.8, struck her airframe |

  At 2, `an_aeroplane_landed_by_hand_and_handed_over_on_its_roll_is_landed_to_a_stop`
  failed the same way.
- The bank does not follow the gain. 0.8 fails and 1.0 to 1.3 pass, so
  passing at 1.0 would be luck, and a platform's drift could undo it. It
  was not committed.
- A stronger gain only while her nose wheel is off the runway was tried
  too: 4 gives 3.79 m wet, 8 gives 4.19, and her bank at the touch is 2.9
  to 5.0 degrees, against none.

**What is left, for the next round.** At 145 kt on her wheels, any extra
rudder rocks her wings. That fragility comes first; a stronger track rudder
can follow it. The bank was not traced in this round.

**Sources searched (2026-11-04).** For an F-22 crosswind limit, her
wet-runway ground handling and her rejected take-off. Found only:
- Air Force photo captions of Raptor 4001's wet-runway tests at 30, 60 and
  90 kt, with no figures;
- general rejected take-off guidance (SKYbrary; the FAA's Airplane Flying
  Handbook as relayed by Boldmethod), nothing of hers.

So every gain above is tuned, not sourced.

**Verification.**
- On main's code, the test passes with her name on: 6.13 m wet, 2.35 dry,
  on linux-release (with FMA) and in an x86-64-v2 build: see below.
- The sweeps were run from the test binary, her lines read, with each gain
  set in the build's copy of her figures. The copy was restored after each
  sweep.
- `python3 tools/make_f22.py --check` passes. Her model is not changed.
