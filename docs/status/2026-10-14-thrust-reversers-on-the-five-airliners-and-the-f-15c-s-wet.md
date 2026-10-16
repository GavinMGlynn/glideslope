### Thrust reversers on the five airliners, and the F-15C's wet runway from her own manual: every landplane stops on the wet runway, 2026-10-14 — item done

**What is still not done, first.** **The F-15C's wet distance is read from
a chart**, to perhaps 500 ft: her flight manual's figure A8-3 through its
RCR 16 (WET) reflector, about 8,200 ft over 50 ft at her weight, sea level,
standard day, no wind (the dry 4,200 ft read through the same panels gives a
ground roll of about 3,200 ft, which the RCR 16 line turns into about
7,000). Her runway is now that times the same 1.67 as her dry figure, 4,174
m, in place of 1.15 on the dry (2,459 m), which was shorter than her own
manual says she needs unfactored; she stops at 2,478 m. **A pilot has no binding for the reverse lever**: it is in the controls and
on the wire, and only the AI moves it (a new Phase 10b item). **The J-3 Cub's flight went NaN standing on macOS release - found, and
fixed in the tests that stood her; macOS itself is still to confirm it.**
The finite-check named her ratio of air to fuel going infinite at step
1,452 (CI run 38447161811), but she was by then a million feet up with
10^25 lb on her: the blow-up came first. **Reproduced on Linux** by building
with fused multiply-adds (`-mfma -ffp-contract=fast`, as Apple's arm64
clang contracts): braked fully, stick central, from JSBSim's level start
with her engine still at 1,790 rpm, she went over on her nose - 47 degrees
nose down after two seconds, 10 ft/s at step 281 - where the plain Linux
build rocks her back on to her tail. So it was on the edge, and the tests
stood her as no pilot would. `stand_on_her_brakes` now stands a tail-wheel
aeroplane as the wet stopping test brakes one - stick fully back, pedals
half on - and both the reversers test and
`every_aircraft_standing_on_her_brakes_at_idle_stays_finite_and_on_the_ground`
use it; on the FMA build both now pass, the Cub standing. The latter checks
every floating-point number in her JSBSim property tree (549 to 775 a
model, `Aircraft::numbers`) at every step, and that she stays within ten
feet of the height she started at and under ten feet a second; on the first
breach it fails naming the step and printing her trace every half second
(height, speeds, attitude, engine, fuel flow, mixture ratio, gear forces).
**Seen to fail** with a NaN pitch trim ("737-300 went non-finite at step 1
(53 of 668 numbers), first: fcs/pitch-trim-cmd-norm, ...") and, on the FMA
build before the fix, the Cub "standing left the ground or moved at step
281: 1.15 ft up, 10.12 ft/s". What is not fixed is JSBSim's: an aeroplane
on her nose goes on to blow up to NaN rather than lying still - the crash
judge ends a flight long before that. The Phase 9 item stays open until
macOS runs it. The reversers test no longer leaves her out. **A client
built before this, sending seventeen controls, is let in and its every input
dropped** as the wrong length (`net::InputReceiver` reads eighteen and
refuses what is left short), as TRANSPORT.md's pre-release rule allows - the
version does not move with a layout - so mixed builds do not fly together
and are not refused by name either (a new Phase 12b item). **Two numbers
are estimates**: the sleeves' two-second travel and the A320's pivoting doors
taken at the cascade's 0.31. **The reverse spool is slow**: 7.2 to 7.8 s from
the lever to nine tenths of full reverse, against the TSB's A340 at about
five, because each engine spools to its full throttle where a real one's
reverse limit is a lower N1. The Learjet 35A is given none: her reversers
are an option her flight manual leaves to a supplement not to hand, and it
warns against them on slippery runways.

**The fast touch was not fast.** Against each aircraft's reference speed the
hand landing touches at or under it: 737-300 133.8 kt (Vref 137.4), 787-8
146.5 (147.8), A320 144.1 (147.3), A380 131.7 (136.3), F-15C 157.8 (160.0),
F-35B 159.2 (158.9). No test-pilot problem; the overruns were stopping force.

**What changed.**
- `tools/airliner.py`: `reverser_channel` / `with_reversers`. The reverse
  lever `fcs/reverse-cmd-norm` (0 stowed, 0.1 idle reverse, 1 maximum)
  deploys the reversers only with weight on the wheels, the sleeves taking
  2 s; their angle is acos(-0.31) at full travel, JSBSim's direct thruster
  scaling its thrust by the cosine; the power is held at idle until the
  sleeves are out (AC 25-32 10.1.4), then follows the lever past the detent;
  and the reversing engines' forward throttles are idle while it is out of
  stowed. Made into the 737-300, 747-400 and 787-8 (cascades), A320
  (pivoting doors) and A380 (the inboard pair only) by their make scripts.
- `sim::Controls::reverse`, the eighteenth control: on the wire (TRANSPORT.md,
  control 17; the count eighteen), the doc-only client and the controller's
  hand's pace. Protocol version not moved: the document's "It does not keep
  the version with the layout, yet".
- The catalogue's `reversers IDLE_BY_KT`: 60 for the Boeings (the 737 FCTM,
  "Reverse thrust is reduced to idle between 60 KIAS and taxi speed"), 70 for
  the Airbuses (the TSB's A05H0002, 1.17.6, "At 70 knots ... idle reverse").
  `Aircraft::has_reversers()`, `reverse_idle_kts()`.
- `Lander::reverse_thrust`: idle reverse a second after the main wheels are
  down (AC 25-32 10.1.3); on a runway that is not dry, maximum until the
  type's speed, then idle; on a dry one idle only - a certified dry distance
  takes no reverse (IFALPA 12ADOBL03, with Boeing); stowed for good at taxi
  speed, taken as 20 kt over the ground. The lever at a hand's pace, begun
  early by the knots she loses while it moves, so that it is at idle by the
  type's speed and stowed by taxi speed, not at them. Off the roll again - a
  bounce, a balloon gone around from - the lever is stowed at a hand's pace
  and the roll's count and stow begun again (`Lander::fly`, from the
  review of #188).
- The reversing engines' throttles are held at idle until the sleeves are
  home, not only while the lever is out: off the ground in a bounce the
  sleeves stow on their own, and a sleeve half out at full throttle was
  reverse thrust in the air (from the review of #188).
- Every source is in `docs/ASSETS.md`, "Thrust reversers".

**The F-15C's wet runway.** `<wet_landing_distance ft basis factor>` in
a figures file, read as `<landing_distance>` is and only beside one
(`PublishedFigures::own_wet_landing_need_m`); `sim::wet_landing_need_m` is
it where given and the dry need times 1.15 otherwise, and the copilot's
brief and the wet short-runway test both use it. Only the F-15C gives one.

**The F-15C's dry ground roll** in her figures file was 2,500 ft; read
again from the scan at 400 dpi the 4,200 ft total meets the dry line at
3,180 ft, and the text says about 3,200 (a comment: no number used reads it).

**Figures.** On the wet short runway, landed by hand and taken back,
maximum reverse (before the lead, below): 737-300 1,552 m to 1,338 (of 1,725); 787-8 1,911 to 1,552
(of 1,893); A320 1,809 to 1,478 (of 1,725); A380 1,860 to 1,679 (of 2,225).
Dry (the short runway, idle reverse only): 737-300 1,214 to 1,211 m, 787-8
1,272 to 1,250, A320 1,219 to 1,216, A380 1,310 to 1,295. **The published
dry distances do not move**: the airliners' are their documents' figures,
which no flight measures, and the three measured ones (B-2A, Mosquito,
F-35B) have no reversers.

**Verification.**
- `every_landplane_landed_by_hand_on_a_wet_short_runway_is_stopped_on_it_by_the_ai`:
  no aircraft named now; all 14 stop on their wet runways, the F-15C at
  2,478 m of 4,174. **Seen to fail** with the AI never reversing: "787-8 ...
  stopped off the runway, 1911.3 m past the threshold"; and with the F-15C's
  own wet figure ignored: "f15c ... stopped off the runway, 2478.2 m past the
  threshold"; each reverted.
- `every_aircraft_is_refused_a_runway_shorter_than_it_needs_to_land_on_and_given_a_longer_one`:
  wet, the F-15C is briefed her own wet need, every other 1.15 times her dry.
  Run with the figures, published, brief and wet tests (99 by name): all
  passed.
- `every_aircraft_with_reversers_is_pushed_back_by_them_on_the_ground_and_every_other_ignores_the_lever`:
  the 15 landplanes (the flying boat named) stand braked; the five whose
  catalogue says `reversers` say so themselves, each reversing engine pushes
  back 0.310 of its full-throttle thrust (737-300 6,200 of 19,998 lb), the
  A380's outboard pair stay at idle forward, idle reverse pushes back, full
  reverse within 8 s; the ten without stand bit-identical with the lever out.
  **Seen to fail** with the effectiveness set to 0.40: "737-300 engine 0
  reversed 0.399988 of her thrust, not 0.31"; reverted.
- The roll-out tests (`every_landplane_taken_back`, every take-back, dry
  and wet) ask every step of each reversing aircraft's lever: none out
  before a second after the mains are down; dry, idle reverse exactly; wet,
  maximum; at idle by her IDLE_BY_KT; stowed below 20 kt; and a landplane
  without reversers never moves it. **Each seen to fail** on its own seeded
  bug, reverted: selected at once ("0.016667, before a second after the
  mains were down"); maximum dry ("dry, the reverse went to 1.000000");
  half wet ("wet, the reverse went to 0.500000, not maximum"); no lead
  ("0.991667 at 59.95 kt, not idle by 60"); stowed at 10 kt ("0.100000, at
  19.95 kt over the ground, below taxi speed"). With the lead the wet stops
  are 737-300 1,349 m, 787-8 1,563, A320 1,494, A380 1,687. Run again
  after the review's fixes, 175 tests by name (linux-release): all passed,
  4 that ask a live model skipped; the selftest hash still 182dd6c996e0ee4c.
- `bounced_with_her_reversers_out_she_gets_her_thrust_back_once_they_are_stowed_and_climbs_away`:
  each of the five run to her rotation speed, reversed fully on her wheels
  for four seconds, then the runway dropped ten feet with the lever stowed
  and full throttle: every reversing engine's throttle is idle until the
  sleeves are in and full after, and 40 s on she is 402 (A380) to 1,163 ft
  (A320) up and climbing. **Seen to fail** with the throttle released as the
  lever is stowed: "a320: a reversing engine had power before its sleeves
  were in"; reverted.
- `no_aircraft_deploys_her_reversers_in_the_air`: all 16 fly bit-identical
  at 5,000 ft with the lever out. **Seen to fail** with the weight-on-wheels
  test made always true: "5 aircraft flew differently with the reverse lever
  out, the first: 737-300"; reverted.
- `every_control_the_flight_model_has_is_one_the_wire_carries` and
  `the_transport_document_and_the_code_agree_about_the_inputs` at eighteen.
- Run (linux-release, 151 tests by name - taken back, landed, roll-outs,
  short runways, wet, stops, brakes, the made models, the transport, the
  doc-only client, inputs, controls, prediction, levers, the catalogue,
  selftest, the lander, approaches, reverse): all passed, 4 that ask a live
  model skipped.
- **The selftest hash does not move**, `182dd6c996e0ee4c`: the C172P has no
  reversers.
