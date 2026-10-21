### The J-3 Cub's nose-over brake level pinned and named, 2026-10-21 — item done

**What is still not true first.**
- **The level is unmeasured on a real Cub.** Nothing found gives a Cub's
  brake force or deceleration against pedal travel. 0.55 solo, 0.50 trainer
  and 0.60 gross are this model's figures, not a Cub's.
- **The NTSB record is qualitative support only.** LAX07CA166 (J3C-65
  N98041, Quincy, California, 12 May 2007; the final report fetched from
  data.ntsb.gov and read): after a wheel landing on 5 in grass "he continued
  to apply brake pressure and as the airplane slowed, the tail rose upward.
  The airplane nosed over to inverted"; probable cause "the pilot's
  excessive use of the brakes, which resulted in a nose over". She was
  holding her tail up with the elevator on grass, not stick central on a
  runway, and no brake level is given.
- **Calm air, 10 kt, stick central only.** The crosswind test
  (`the_j3_cub_flown_stick_central_in_a_15_kt_crosswind_...`) keeps its own
  pins at tenths, unchanged and consistent with these.

**The decision (owner, 2026-10-20).** Her going over on her brakes is taken
as her real behaviour, her model unchanged, as #220 did for the Mosquito.

**The new test**,
`the_j3_cub_braked_stick_central_to_a_stop_from_10_kt_in_calm_air_noses_over_from_one_brake_level_at_every_loading`
(tests/unit/test_departure.cpp): at each of her figures' three loadings, 60
runs at 0.05 to 1.00 in twentieths of brake, stood on full brakes five
seconds, run up stick central to 10 kt by the take-off steering, braked
stick central, throttle closed, to a stop and five seconds standing. Went
over is GroundJudge's strike, past 45 degrees or no number; tail lifted is
her tail wheel off the runway. Both sides of each pinned level asserted at
every level; every run stops or goes over; 60 runs, 30 over and 2 more
lifted counted. Her model's own loading (no one aboard) is named as left
out: no pilot to hold her stick or brake her.

**Measured (linux-release, her main gear springs 2,093 lb/ft):**

| Loading | Total (lb) | Tail lifts at | Goes over from | Least pitch at the lift |
|---|---|---|---|---|
| solo | 922 | 0.50 | 0.55 | 9.5 deg |
| trainer | 1,092 | — | 0.50 | — |
| gross | 1,220 | 0.55 | 0.60 | 4.6 deg |

Below the levels she stands at 11.4 to 12.6 degrees, tail down. With her
main springs put back to the pinned 1,000 lb/ft in the build's data copy the
test still passed: #223's springs did not move her levels.

**Verification.** Seen red twice: pins moved (solo over at 0.60, gross tail
at 0.50) failed at "solo at brake 0.55: went over below" and "gross at brake
0.50: her tail stayed down at"; her main wheels' static friction lowered
from 0.80 to 0.70 in the build's data copy failed six runs at every
loading (she stood at the pinned levels). Both reverted, the data copy
restored by reconfiguring and compared. In linux-release the 63 tests named
cub, j3, committed, selftest or figures pass; `glideslope_cli selftest`
hash `521cb12d3a1c5f0c` (no model change). In a `-march=x86-64-v2` release
build (the levels are edge-sensitive) the new test and the crosswind test
pass.
