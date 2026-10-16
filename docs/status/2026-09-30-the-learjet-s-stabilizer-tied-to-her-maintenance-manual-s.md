### The Learjet's stabilizer tied to her maintenance manual's travel: she trims in cruise, 2026-09-30 — tail done

**What is missing first: she can no longer be rotated early.** Her take-off
had been leaning on the stabilizer's mis-tied zero. With the AFM's take-off
setting on the rigging's scale (3.6 degrees less nose up on the model's),
the stick held fully back from 85 percent of the speed the book rotates her
at lifts her nose wheel only at about 115 knots, and she is five feet up at
130 - past her rotation speed less five (125.3 - 5), so the business-jet
take-off lesson finds nothing early in it. **The Learjet's early rotation is
left out of `a_take_off_flown_with_one_fault_has_that_fault_in_its_debrief`**,
named there with its reason, and is a tail ("The Learjet cannot be rotated
early"); her book take-off there still leaves an empty debrief (off at 135
knots, five feet up). The test now counts the classes whose early rotation
was flown, six of seven, and asserts that the business jet's lesson is the
one not flown. No published figure (a minimum unstick speed) was found to
change what the lesson expects. **Left out too of the early half of
`every_landplane_leaves_the_runway_within_ten_knots_of_its_rotation_speed_and_sooner_rotated_early`**,
where she had passed by 0.2 knots - 125.7 rotated early against her rotation
speed of 125.3 and 128.9 by the book (was 125.9 and 114.3): she is flown and
printed, judged by the book, and named, and the test asserts she alone is
left out of rotating early.

**The moment budget at the rotation**, measured (a scratch probe, not
committed): the stall-example loading (15,800 lb, 18.4% of the chord), flaps
8, full thrust, the take-off setting (stabilizer -3.64 degrees on the model's
scale), the stick fully back from 90 knots. Moments about the main wheels'
contact, ft-lb, positive nose up, when the nose wheel leaves at 114.8 knots:

| Term | ft-lb |
| --- | --- |
| Elevator, 16 degrees up (Cmde) | +22,600 |
| Stabilizer (Cmstab) | +10,500 |
| Pitching moment at no incidence (Cm0) | +5,200 |
| Incidence, flaps, pitch rate | -1,500 |
| Lift and drag, about the wheels | +3,900 |
| Thrust, 6,800 lb on its line | -27,200 |
| Weight, 1.6 ft ahead of the wheels | -25,100 |
| Acceleration, at the centre of gravity's height | +13,200 |

At the old setting (-7.24 on the model's scale) the stabilizer's term was
about +20,900 - some 10,400 more - and the nose came up as the stick did, at
90 knots. The maximum loading (18,300 lb, 24.6%) is the same: nose off at
114.5 knots. **The suspects, each checked against a source**:
- The elevator's travel, 16 degrees up: TCDS A10CE, as the model has it; its
  moment, -1.036 a radian: TN D-7647's flight identification.
- The main wheels: FS 396.82 static (MM 6-00-03, figure 1 sheet 2, Major
  Structural Member Locations), as the model has them (396.83, the AFM's).
- The thrust line: the model's engines are at WL 22 (estimated). MM 6-00-03
  puts the engine beams that carry the pylons at WL 40.00 (FS 443.23) and WL
  39.82 (FS 473.00); the nacelles' centreline is not given. Raised to the
  beams, the thrust's moment about the wheels would grow by about 10,000
  ft-lb nose down: the wrong way, so not the cause. Left as it was.
- The centre of gravity's height: the model has it at WL 0, which MM 6-00-02
  (figure 1 sheet 2) shows is the bottom of the fuselage (the floorboards are
  at WL 5.87). It is surely too low, but no source gives it; raised, the
  acceleration's term grows by about 5,000 ft-lb a foot, which would help.
  With the thrust line raised too, the two nearly cancel. Left as it was.
- The tail in ground effect: the model has ground effect on the wing's lift
  only (kCLge 1.168 on the roll); nothing published gives the T-tail's.

**The cause of the tail.** The stabilizer's scale was the flight model's:
zero where TN D-7647's pitching moment holds, the nose-down stop the tunnel
aircraft's 0.4 degrees, and the AFM's take-off settings (degrees leading edge
down from the rigging's neutral) read as if that neutral were the model's
zero. From 250 to 350 knots JSBSim's trim found no setting on 20 of the 24
cases below. The maintenance manual (Learjet 35/35A/36/36A MM 27-40-00,
Adjustment/Test, and its figure 202) gives the travel: nose-down stop 1 deg
30' to 1 deg 55' leading edge down from neutral, nose-up stop 8 deg 30' to 9
deg 00', take-off limits 4 deg 39' to 7 deg 54' on the same scale.

**What changed** (`tools/make_learjet35a.py`, its docstring says why). The
stops are the manual's, the middles of its tolerances, 1.71 and 8.75 degrees,
and the rigging's neutral is put 3.6 degrees nose up of the model's zero -
**an estimate: no source ties the two, and no one offset fits everything.**
The take-off table is the AFM's setting less 3.6, over the nose-up half.

**Tying the zero to a source, tried (the review of #72).**
- The manual's neutral: the stabilizer measured by clinometer on a contour
  board, the aircraft levelled by plumb bob (MM 27-40-00 Adjustment/Test, MM
  8-10-00) - level with the aircraft's own reference.
- The model's zero is TN D-6573's tail incidence of nought: the tunnel's
  pitching moment at no incidence there, 0.051 + 0.0368 x 0.4, is 0.066,
  TN D-7647's. TN D-6573 ("Tail") gives the Learjet 23's stabilizer travel,
  0.4 to -7.0 degrees, as an incidence without its reference line; TN D-7647
  gives no stabilizer settings (its Cm0 is one constant for whatever trim
  its flights flew, and their thrust moment was an unknown beside it).
- Read as one reference (offset nought), the 35A cannot cruise - she needs
  1.81 degrees nose down of the model's zero and her stop is 1.71 nose up -
  and the AFM's take-off settings are 2.3 to 3.3 degrees nose up of her trim
  at V2. The Learjet 23's pitching moment does not carry to the 35A on one
  reference. The one sourced alternative, TN D-7647 table II's wind-tunnel
  values (Cm of incidence -1.12, of the elevator -1.34, against the flight's
  -0.81 and -1.036), spreads her trims wider, not narrower.

**What one offset can and cannot fit**, measured with the travel unbounded
(a scratch probe; each loading her figures name; flaps 40 at its reference
speed, 1.3 times its stall from the AFM's; flaps 8 at V2, 1.2 times its
stall):

| Condition | Model's trim, degrees (+ nose down) | Offset it allows |
| --- | --- | --- |
| Cruise, 350 knots at 10,000 ft, light (28.8%) - the most nose down | +1.81 | at least 3.52 |
| Approach, flaps 40, landing loading (18.1%) at 125.4 knots - the most nose up | -5.08 | at most 3.67 |
| AFM take-off setting trims her at V2, gear down: landing / stall-example / maximum / light | -5.00 / -4.91 / -3.06 / -1.71 | 2.25 / 2.33 / 2.88 / 3.29 |
| The same at V2 and ten, gear up | -3.80 / -3.73 / -2.18 / -0.87 | 3.45 / 3.51 / 3.76 / 4.13 |

The review's upper bound from the approach, 3.46, was flown at 125 knots at
15,800 lb - below that loading's reference speed, 127.4, and above the
maximum landing weight, 15,300. At the reference speeds the cruise and the
approach leave a band, 3.52 to 3.67, and 3.6 is in it. **The take-off anchor
is not used**: at V2 it gives 2.25 to 3.29, none of which can trim the
cruise, and it spreads over a degree with the centre of gravity because the
model's trim moves 3.3 degrees from the forward loading to the light one
where the AFM's setting moves 2.25. At 3.6 the take-off setting is 0.3 to 1.4
degrees nose down of her trim at V2, which is why she cannot be rotated
early.

**The test.** `the_learjet_35a_trims_level_from_250_to_350_knots_with_her_elevator_at_neutral`
starts every loading her figures name (four) level at 250, 300 and 350 KCAS
at 10,000, 20,000, 30,000 and 40,000 ft, and at 230 KCAS at 40,000 (Mach
0.76) so that the ceiling's height is flown - 52 cases, counted - and leaves
out, named, the sixteen past her Mach 0.81: 350 knots at 30,000 ft and all
three speeds at 40,000 (250 knots there is Mach 0.82). Each of the 36 is
trimmed by JSBSim on the pitch trim alone, the elevator left at neutral,
then flown thirty seconds hands off, wings held level: she must have trimmed
short of the stabilizer's stop, and hold her height within 100 ft and her
speed within 3 knots. The worst case's share of the nose-down travel is
printed. **Seen failing on the old model** (the first 24 cases, at 10,000
and 20,000 ft): 20 did not trim (all but 250 knots at the landing and
stall-example loadings, which trimmed at +0.24 to +0.28 degrees against the
0.4 stop). Now all 36 trim, the stabilizer from -0.37 (230 knots at 40,000
ft, stall-example) to +1.81 degrees; the worst, 350 knots at 10,000 ft
light, uses 0.956 of the nose-down travel. They hold their height within 12
ft and speed within 0.2 knots.

**Stale words put right**: the script's note of what the take-off setting
did (it had said 129 by the book and 114 pulled early), and the business-jet
take-off lesson's header: its bands re-measured by
`the_take_off_lesson_flown_by_the_book_leaves_an_empty_debrief` - she holds
8.8 to 18.5 degrees in the initial climb (was 12.6 to 17.9) and climbs away
at 162 to 172 knots, 37 to 47 above her rotation speed (was 45 to 54), both
inside the lesson's bands (5 to 25 degrees; rotate+5 to rotate+70), which
are unchanged - and that she cannot now be rotated early. `docs/ASSETS.md`
names the maintenance manual's chapters, with their SHA-256s, and no longer
says the model takes its nose-down travel from TN D-6573.

**Unchanged**: the C172P's model is untouched, and the selftest's hash is
d36123c1eecc3e23 twice on this build. The committed model is what its script
writes. Every other Learjet test passes: her figures (field length, climbs,
ceiling, cruise Mach, stalls), the take-off, approach, circuit, landing and
stall lessons, and the autopilot's walks of the catalogue.
