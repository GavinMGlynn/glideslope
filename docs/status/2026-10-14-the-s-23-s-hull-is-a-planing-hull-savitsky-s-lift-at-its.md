### The S.23's hull is a planing hull: Savitsky's lift at its centre of pressure, the afterbody planing, and she holds the step in calm air; in gusts she touches flat and skips, 2026-10-14 — item done, and the headwind item

**What is still missing, first**: **in gusts the S.23 skips off the water,
and worse than on main.** On main (#184's hull, its pinned planing clip)
her eight seeds rose 1.26, 0.00, 0.00, 0.00, 0.70, 0.81, 0.16 and 1.57 ft,
three over the limit (0.81 to 1.57); on this hull all eight are over it,
1.73 to 8.09 ft, and seed 1 is wrecked. Two things make it so, both
measured: **the hull's centre-of-pressure moment** - with that one moment
taken out of the built model (A/B on this tree), seeds 1 and 5 rose 2.38
and 1.98 ft against 8.09 and 4.43 with it - **and the flat touch in
gusts**, below. It is worse than on the wip branch too. The gust test's eight seeds rise 1.73 to
8.09 ft after she touches, against the gust limit's 1.0 less its 0.25
margin, and on seed 1 she comes back down at 726 ft/min. She stays named
in the gust tests and Phase 10b's "The flying boat lands in gusts without
skipping" stays open. Traced on seed 1: the lander's gust flare puts her on
the water at 0.75 degrees and 88 kt, below a hull's lower trim limit (NACA
TN 923 has it about 4.5 degrees at 50 kt for the Navy's boats). At that
trim the forebody's wetted length runs to Savitsky's cap of four beams, its
centre of pressure about 23 ft ahead of the reference point, and its lift
pitches her up 4 degrees in a third of a second off the water - low-angle
porpoising, which the hull's centre of pressure now models and which the
flat touch invites. What fixes it is in the gust flare, not the hull: touch
her within the stable range, as she touches in calm air (4.5 degrees).

**The hull** (`tools/make_short_s23.py`, its docstring's "Hydrodynamics";
nothing hand-edited in assets/jsbsim), on the wip branch's four commits,
rebased on main (its first, #184's flare to the hull, was already merged
and is dropped):
- **Savitsky's planing lift** (1964), as on the wip branch, and now **at
  its centre of pressure**: lambda b (0.75 - 1 / (5.21 C_V^2 / lambda^2 +
  2.39)) forward of the step (`hydro/glideslope/forebody-pitch-moment-
  lbsft`). JSBSim applies the water's forces at the model's hydrodynamic
  reference point, 1.4 ft ahead of her centre of gravity, so the lift
  pitched her nose up; at the step's centre of pressure it pitches her
  down by about as much as the model's own hull moment (`C_M`, from the
  Sikorsky S-40's tank data) pitches her up. Traced in calm air before it:
  her trim rose from 4.5 to 9 degrees on the step with the hold's elevator
  at full nose-down. After it: 4.5 to 5, the elevator within 0.1 of
  neutral.
- **The afterbody planes** (`hydro/glideslope/afterbody-*`): past the trim
  at which her afterbody keel meets the water - 10.81 degrees, its 1.26 m
  rise over the 6.6 m from the step to the stern, from the keel skids'
  geometry - Savitsky's lift at the afterbody's own trim (hers less 10.81),
  its wetted length from the stern forward (the stern's immersion over the
  sine of that trim, capped at the afterbody's 21.7 ft), acting at its own
  centre of pressure 27.6 ft aft of the reference point less l_p: nose
  down. The source is NACA's porpoising work: Benson and Klein, NACA ARR
  3F30 (1943), a hull as "two planing surfaces in tandem", the afterbody's
  keel inclined to the forebody's, and the upper trim limit where the trim
  brings the afterbody into the forebody's wake; Benson, NACA TN 923
  (1943), skipping where the trim at contact reaches that of the
  afterbody's keel horizontal. Not modelled: the forebody's wake, which
  lowers the water the afterbody meets (so this wets it a little early),
  and the afterbody's narrowing (its beam is taken as the hull's). Seen
  working before the forebody's centre of pressure was added: with the stick full back off the step
  her trim reached 12.7 degrees and the afterbody's 7,800 to 9,900 lb
  brought it back to 9.5.
- Heave and pitch damping and the buoyancy fade, the wip branch's, as
  recorded in the 2026-10-13 entry below.

**The step-attitude hold is kept, its gains derived** (`sim::Lander::fly_laws`).
The FAA's Seaplane Handbook (FAA-H-8083-23, ch. 6) holds the touchdown
attitude until she comes off the step, and NACA TN 923 holds the trim
"within the stable range"; without it, eased back as a wheel's rollout
eases, her trim rose to 9.4 degrees on the step and she skipped 0.48 ft,
just inside the 0.5. Its gains are no longer fitted:
- **The attitude gain**: the stick is fully forward by the trim her
  afterbody meets the water at, so one degree asks for one part in her
  margin to it - 1/(10.81 - 4.55), 0.16 a degree, where the wip branch's
  fitted 0.2. The angle is found from her model's hull contacts
  (`Lander::afterbody_deg_`): the step the aftmost of her lowest keel
  points, and the shallowest angle up from it to a keel point aft.
- **The damping**: 0.7 of critical on her elevator's authority now, the
  pitch acceleration a full stick gives her, q S c Cm_full / Iyy - 37 deg/s^2
  at her 70 kt touch, 0.09 stick a degree a second, rising to 0.13 at 50 kt
  as her dynamic pressure falls. Cm_full, 0.28, is her model's DATCOM
  `CmDe` at her 18 degrees of elevator, given in her figures as
  `elevator_full_cm` (new, `sim::PublishedFigures`, refused outside 0 to
  2); q, S, c and Iyy are her model's at run time.

**Measured** (linux-release):

| | wip branch, 9c67bec1 | this item | range |
|---|---|---|---|
| take-off time, 45,000 lb | 30.99 s | 34.09 s | 25.93-35.08 |
| take-off run, 45,000 lb | 682 yd | 797.5 yd | 675.75-914.25 |
| take-off time, standard | 24.13 s | 26.82 s | 21-27 |
| draught | 3.69 ft | 3.69 ft | 3.5-4.5 |
| calm hop on the water | 0.50 ft | 0.32 ft | 0.5 |

All seven of her figures are in range (`glideslope_cli figures short_s23`:
level speed 201.8 mph, climb 912 ft/min, landing stall 66.3 KCAS too), but
**the standard take-off is only 0.18 s inside its range** (26.82 s against
27.00), and the 45,000 lb one 0.99 s (34.09 against 35.08). **The cause is
the forebody's centre-of-pressure moment**: with it taken out of the built
model (A/B on this tree), the take-offs are 23.35 s and 29.31 s - and the
45,000 lb run 627.8 yd, short of its range's 675.75. Moved aft to its
centre of pressure the lift pitches her nose down on the step, her trim
sits lower than her best for resistance, and she takes longer to unstick.
Gouge's own 30.5 s at 45,000 lb is the range's centre. A small change to
the hull - Savitsky's hydrostatic term, below - takes the 45,000 lb
take-off out of range.

The gust test's eight seeds (rise limit 1.0 ft, less 0.25):

| seed | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| touched, ft/min | 213 | 130 | 119 | 175 | 189 | 284 | 224 | 166 |
| rose, ft | 8.09 | 4.44 | 2.57 | 6.97 | 4.43 | 2.60 | 2.03 | 1.73 |

Seed 1 came back down at 726 ft/min. Without the forebody's centre of
pressure (on this tree, the moment's input removed from the built copy),
seeds 1 and 5 rose 2.38 and 1.98 ft, and the calm hop was 0.54.

**And Phase 10b's "In a steady headwind the S.23 skips off the water after
alighting" is done with it.** In a steady 15 kt headwind she now rises
0.20 ft after touching (1.0 on her pinned hull, 7.58 judged by her
groundspeed, both seen red 2026-10-13), and in a 10 kt tailwind 0.23,
both under those tests' 0.5; she touches at 87 and 89 ft/min, her least
pitch 4.3 and 4.2 degrees, and slows to taxi speed. She is no longer named
in either: both tests judge all fourteen (coverage asserted, 14 judged and
none named), and the tailwind test is renamed for it,
`in_a_steady_10_kt_tailwind_every_aeroplane_lands_and_rolls_out_without_lifting_off_again`
(it was `..._every_landplane_...`).

**Savitsky's hydrostatic term, tried for the review of #191 and not
kept.** Savitsky's C_L0 is tau^1.1 (0.012 lambda^0.5 + 0.0055 lambda^2.5 /
C_V^2); the hull uses the first, dynamic, term alone, and its centre of
pressure the full C_V form. Added to both the forebody's and the
afterbody's lift (on 1/2 rho V^2 b^2 it is 0.0055 lambda^2.5 tau^1.1 on
1/2 rho g b^3), measured on linux-release: the 45,000 lb take-off 35.64 s,
**out of range** (35.08), its run 847.6 yd; the standard take-off 26.87 s;
**the draught 3.30 ft, out of range** (3.5 to 4.5); the calm hop 0.32 ft;
the headwind 0.20 ft and the tailwind 0.23; the gust seeds 9.26 (wrecked,
607 ft/min), 4.77, 2.96, 14.44 (wrecked, 848 ft/min), 4.71, 2.62, 2.07 and
1.74 ft. The term is the hydrostatic lift of the water displaced, which
her model's buoyancy table already carries: added, it lifts her at rest
too, so she floats 0.4 ft high. Kept untuned on branch
`s23-savitsky-hydrostatic-wip` (3dd5b3e3). Using it means taking the
hydrostatics from Savitsky and the buoyancy table off the planing hull,
which is a change to the hull model, not a tweak.

**Sources pinned** (docs/ASSETS.md, "The Short S.23's hull"): NACA ARR
3F30 and NACA TN 923. Cited, not pinned: Savitsky (1964), Marine
Technology 1(4), pp. 71-95, SNAME's copy paywalled; and Zarnick,
DTNSRDC-78/032 (1978), not looked for.

**Verification:**
- `a_flying_boat_alighting_in_calm_air_holds_the_step_without_porpoising_down_to_taxi_speed`:
  green, 0.32 ft against 0.5. **Seen red** on this tree without the
  forebody's centre of pressure: 0.54 ft. And on #184's hull, 4.10 ft
  (2026-10-13).
- `glideslope_cli figures short_s23`: 7 of 7 in range, as above.
- New for the review: `an_elevators_full_travel_pitching_moment_is_read_and_refused_outside_0_to_2`
  (the S.23's 0.28 read, the A320's none, 2 read back, 0, -0.28 and 2.5
  refused; seen red with the limit raised to 3: "refused, not 2") and
  `no_landplane_has_an_afterbody_and_the_flying_boats_meets_the_water_at_her_keels_angle`
  (13 landplanes at 0, the S.23 at 10.81; seen red with the angle's
  default 1: "14 wrong, the first: 737-300"). Both green.
- 57 tests over the S.23, a flying boat, a seaplane, water, the made
  models, the calm landings at both weights, the headwind and tailwind
  rollouts and the published figures pass (linux-release, `ctest -R`);
  the eight gust seeds pass with her named, their S.23 figures above.
- `python3 tools/make_short_s23.py --check`: the committed model is what
  the script makes.
- `in_a_steady_15_kt_headwind_every_aeroplane_lands_and_rolls_out_without_lifting_off_again`
  and `in_a_steady_10_kt_tailwind_every_aeroplane_lands_and_rolls_out_without_lifting_off_again`:
  green with the S.23 judged, 14 of 14.
- The selftest hash does not move: 182dd6c996e0ee4c (it flies a C172P).
