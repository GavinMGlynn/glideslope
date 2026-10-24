### A Learjet 35A given no loading flies inside her limits, 2026-10-24

**What is still not true first.** Ten of the sixteen aircraft have no
envelope a public document gives, so their own loadings are checked against
nothing. The re-measured plan and glide speeds were measured on linux-debug,
not linux-release as before.

**Where a player given no loading gets her loading.** From her model's own
pointmasses and tanks. The catalogue names no loading, and the server and
client load one only when a player asks for one by name
(`sim::named_loading`). So each fix is made in her model, through her make
script.

**The Learjet.** Her model carried no one and had every tank full: 16,411 lb
at FS 392.0, 36.1% of her mean chord. Her flight manual's aft limit is 30% at
every weight (FM-108 section 1, Weight and C.G. Limits, change 10, the scan
at manualslib.com/manual/2944140, page 23). Her forward limit is 5% (FS
366.31) up to 10,000 lb, then runs in a straight line to 18.83% at 18,300 lb
(16.66% at 17,000 lb and 18.33% at 18,000 lb lie on that line). Her chord,
82.75 in, has its leading edge at FS 362.17, from FlightSafety's Learjet 30
series PTM (WB-10) and A10CE; her model's frame is the AFM's stations.

Two things put her aft. Her empty centre of gravity is estimated at 32%, and
her fuselage tank, at FS 440.2, sits at 94% of her chord. Her own loading is
now a crew of two, 400 lb (as her figures' light loading carries), with her
wing and tip tanks full and her fuselage tank empty: 15,471 lb at FS 385.46,
28.1%. Her forward limit at that weight is 14.1%. With a crew of two and
every tank full she would lie at 33.6%, still aft of her limit.
`make_learjet35a.py --check` is clean.

**The J-3 Cub.** Her model carried no one: 752 lb at station 8.96, forward of
A-691's 10.6 to 22.7 in (datum the wing's leading edge, her model's x the
sheet's less 15.75). A Cub is flown solo from the rear seat (A-691's "solo
from rear seat" placard; her figures' solo loading). Her own loading is now a
170 lb pilot there, the standard occupant her figures use (14 CFR 23.25), and
her tank full: 922 lb at station 13.94, through `tools/make_j3cub.py`.

**Plan and glide speeds measured again at the new loadings**
(`glideslope_cli plan-speeds` and `glide-speeds`, linux-debug):

| Aircraft | Plan speeds, was | Plan speeds, now | Glide slowest, was | now |
|---|---|---|---|---|
| Learjet 35A, 15,471 lb | 140 to 300 kt at 16,411 | 137 to 300 | 140 | 142 |
| J-3 Cub, 922 lb | 43 to 62 kt at 752 | 46 to 62 | 43 | 46 |

The Learjet at 137 kt glided both ways stalled (alpha 27.1 degrees); 142 went
round. The glide tests pinning each band were re-banded and renamed: the
Learjet's from 142 to 167, 172 to 197, 202 to 227 and 232 to 240 kt each way;
the Cub's 46 to 48. `the_controller_holds_a_plans_slowest_for_what_she_weighs`
now asks the Cub's 46 kt at 922 lb, and 52.9 at her gross.

**Every aircraft's own loading against her books.** Each figures file now
gives one of two things, and the reader refuses a file with neither, or with
both:

- `<cg_limits>`: limits by weight, in inches from the source's datum
  (`offset_in` to her model's frame) or in per cent of her chord (`lemac_in`,
  `mac_in`).
- `<no_cg_limits>`: the reason no envelope is given.

The walk is
`every_aircraft_given_no_loading_lies_inside_her_published_weight_and_centre_of_gravity_envelope`
(tests/unit/test_catalogue.cpp). Its space is the catalogue's 16 aircraft.

Six lie inside their envelopes:

| Aircraft | Own loading | Envelope |
|---|---|---|
| C172P | 1,880 lb at 42.12 in | TCDS 3A12 rev 84 |
| C182 | 2,505 lb at 43.04 in | 3A13 rev 72, the 182S |
| PA-28-180 | 1,790 lb at 88.76 in | 2A13 rev 48 |
| Learjet 35A | 15,471 lb at 385.46 in | — |
| S.23 | 34,717 lb at 360.02 in | 359.75 to 371.05 in |
| J-3 Cub | 922 lb at -1.81 in (station 13.94) | A-691, 10.6 to 22.7 in |

The S.23's limits are Short's RN-3-1-37, as her model's own mass balance
quotes them and places them on its frame. That report gives one range at
every weight.

Ten are named unpublished, each with the reason in her file. They were
searched on 2026-10-24, and the type certificates read again for the five
airliners: FAA A16WE (rev 57, p. 17), FAA A20WE (rev 58: "the appropriate FAA
Approved Airplane Flight Manual", D6-13703), EASA IM.A.115 (issue 30, p.
18), EASA A.064 (issue 62, section 1 item 14: "See approved Airplane Flight
Manual") and the UK CAA's UK.TC.A.00158 for the A380 ("Refer to approved
Airplane Flight Manual"). None gives limits.

- **737-300, 747-400, 787-8, A320 and A380:** their type certificates say
  "see AFM", and their flight manuals and weight and balance manuals are not
  public. For the A320 and A380, Airbus's airport planning documents give
  only the centres of gravity at which their pavement loads are worked, at a
  few weights, not a flight envelope.
- **B-2A, F-15C, F-22A and F-35B:** military, not public.
- **Mosquito FB.VI:** her pilot's notes found give no limits, and her load
  sheets are in no public copy.

The walk counts 6 inside and 10 named, which is 16 of 16.

- Seen red on the Learjet's old model: "learjet35a (16411 lb at 392.01 in)";
  the Cub's old model lay outside in the same run (752 lb at -6.79 in).
- Green with both new models.

**The trim walk.** `every_aircraft_at_every_loading_at_three_heights_and_three_speeds_is_trimmed_as_a_players_is`
passes with the Learjet's three points at 300 kt at her own loading (1,000,
5,000 and 10,000 ft) taken off its left-out list. Its "moved in" check would
fail if they still trimmed while listed. The Cub's own loading at 62 kt at
10,000 ft is now on it: flown solo, she weighs what her solo loading does,
already left out there by derivation (she holds 61.2 to 61.5 KCAS). 540
points: 526 trimmed, 14 left out, each named and failing.

**Flight figures.** The Learjet's flight-manual figures are flown at her
figures' named loadings, which set every tank and the payload, so her new
own loading leaves them as they were. On linux-debug, 125 of 125 tests
passed: every test naming the Learjet, a figure, a loading, every aircraft,
or the selftest, among them her seven flight-manual figures and
`the_selftest_prints_the_same_hash_every_run`.
