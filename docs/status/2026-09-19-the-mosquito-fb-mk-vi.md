### The Mosquito FB Mk VI, 2026-09-19 — item done (CI run 35409102752)

**What is missing first:** the Mosquito flies only from the catalogue - nothing
yet lets a pilot choose it at the start (a Phase 5 item of its own) or work
its extra controls from the keyboard: the propeller levers, the supercharger's
gear change switch, the radiator shutters and the undercarriage are in
`sim::Controls` and the model, but only the figure flights and the autopilot's
throttle move them. It has no visual model yet. Its handling at the stall and
on one engine is set from the Pilot's Notes' words and numbers, not from any
flight test of its derivatives, which were not found.

**The model** - `assets/jsbsim/aircraft/mosquito-fb6/`, the Merlin 25 in
`assets/jsbsim/engine/merlin25.xml` and the propeller in
`assets/jsbsim/engine/prop_dh_hydromatic.xml` - is written here from the
aircraft's trials and Pilot's Notes (`docs/ASSETS.md` records each source);
no other simulator's Mosquito is used. Every number in it says where it comes
from, what was estimated, and what was set to meet a figure. The propeller's
tables are computed by blade-element momentum theory in
`tools/make_mosquito_propeller.py`, and a test fails if the committed file
differs from what it makes.

| Figure | Published | Range | glideslope |
| --- | --- | --- | --- |
| Level, sea level, +18, MS gear | 332 mph TAS (HX809) | ±2% | 331.0 |
| Level, 5,100 ft, MS full-throttle height | 353 mph | ±2% | 353.4 |
| Level, 12,500 ft, FS full-throttle height | 363 mph | ±2% | 363.8 |
| Level, 18,000 ft, FS full throttle | 357 mph (read from fig. 1) | ±2% | 359.9 |
| Climb, 10,400 ft, MS gear, radiators open | 1,740 ft/min (HJ679) | ±10% | 1,747 |
| Climb, 17,000 ft, FS gear | 1,440 ft/min | ±10% | 1,346 |
| Time to 20,000 ft | 12.85 min | ±10% | 13.81 |
| Stall, clean, power off, 18,000 lb | 105 KIAS (Pilot's Notes 1950) | 100 to 110 | 105.6 |
| Stall, wheels and flaps down | 95-100 KIAS (1950), 96 at full load (1944) | 89 to 100 | 92.0 |
| Take-off over 50 ft, B Mk IV weight and boost | 795 yd, 2,385 ft (B IV data sheet) | ±10% | 2,516 |
| Swing on take-off: the port throttle's lead | "slightly ahead" (1944) | 0.2 to 3 lb/sq in | 2.59 |
| Safety speed, +18, 17,000 lb | 170 KIAS (1950) | ±10 kt | 174.9 |
| Safety speed, +9 | 155 KIAS | ±10 kt | 147.5 |
| Single-engine ceiling, 20,500 lb | 12,000 ft (1950) | ±2,000 ft | 12,480 |

The engine against Rolls-Royce's own curve (AVIA 6/5817 fig. 1, +18 and 3,000
rpm at 400 mph): MS gear 1,542 bhp at sea level (1,540) and 1,613 at 6,000 ft
(about 1,600); FS gear 1,446 at 10,000 ft (1,450), 1,469 at 12,000 (1,468),
1,378 at 16,000 (1,376) and 1,194 at 20,000 (1,164). Full-throttle heights at
HX809's speeds: about 5,200 ft and 12,700 ft, where it measured 5,100 and
12,500.

**What JSBSim does not model, and the Mosquito needed.** Each of these was
found by flying a figure and asking why it missed:

- *The propeller.* JSBSim's P-51D tables, the first basis, gave an efficiency
  above 1 at the Mosquito's speeds. The tables are now computed for a
  three-bladed 12 ft blade of estimated planform, with tip Mach tables and a
  5% installation loss; a wide "paddle" blade (activity factor about 140),
  because narrower ones took the take-off power only by stalling.
- *The Merlin's power with boost and height.* JSBSim's mixture table makes the
  most power at a fuel-air ratio of 0.10, so the Merlin's enrichment above +9
  would have made more power, not less; a table with its best power at 0.08
  gives +9 the climbing power the engine family was rated at. JSBSim takes the
  charge at the outside air's temperature; the ram and the impeller heat it,
  which makes the power rise and fall with height as Rolls-Royce's curve does.
  The supercharger changes gear by aneroid at 7,000 ft, as the Notes say, and
  in low gear the figure flights climb until the boost has fallen 2 lb/sq in,
  as the Notes tell the pilot to.
- *A dead engine.* JSBSim charges a running engine its friction but not a
  stopped one, so a failed engine's windmilling propeller dragged about 100
  lbf; charged the same friction, about 550 lbf at 195 knots - "the drag of a
  windmilling propeller is very high".
- *The slipstreams.* The propellers wash half the wing: at take-off power
  that lifts the wing behind them, which is what lets the model leave the
  ground as the B IV's figure needs while its power-off stalls stay at the
  Notes' speeds, and the Notes stall it "power on under typical approach
  conditions" 5 knots slower. Over the tailplane's outer parts it raises the
  tail on the take-off run, and on the fin it pushes the nose to port.
- *The swing.* On the ground with the tail down, the propellers' torque has a
  component about the vertical that turns the nose to starboard, and the tail
  wheel castors and does not resist it. The slipstream on the fin outweighs
  it slightly, and the swing is to port, as the Notes say.
- *The stall.* Without a nose-down moment past the stall the model locked
  into a deep stall at 49 degrees; with one, "the nose drops gently".

**Set to meet a figure, and so not checked by it:** the drag at zero lift
(the level speeds), the lift curve's peaks (the stalls), the radiator
shutters' drag (between the two climbs), the slipstream's push on the fin (the
swing), and the rudder's power (between the two safety speeds - the fin's
area was not found). The model puts the two safety speeds further apart than
the Notes do: with the rudder set between them, +18 is 5 knots fast and +9 8
knots slow. The take-off is 5.5% long against a different mark's figure.

**How the stalling speeds were taken.** The Notes give indicated airspeeds;
the model reports calibrated. HJ679's measured position error (de Havilland,
20 September 1943, fig. 3) is under a mile an hour at the lowest speed it
reached, 180 mph, so the stalls are taken as calibrated.

**The tests:** one per figure, fourteen more (`tests/unit/test_figures.cpp`),
flown by the same flights as the Cessna's where they fit and by new ones -
level speed, time to height, take-off distance to 50 ft, the swing, the safety
speed, the single-engine ceiling - where they do not. A figure names its
flight and its loading; the Cessna's nine measure exactly what they did. The
catalogue test holds the Mosquito at 3,000 ft and 220 KCAS on the autopilot.
**Watched to fail**, each by breaking one thing in a copy of the model:
without the slipstreams' lift the take-off is 2,966 ft, out of range; with
both propellers turning the other way the swing is to starboard (a lead of
-2.88 lb/sq in); without a stopped engine's friction the safety speed at +9
is 136 knots; and without the nose-down moment past the stall the aircraft
settles at 49 degrees of incidence, nose up, which a test of its own catches
(`tests/unit/test_handling.cpp`).

**Found on the way:** JSBSim starts a running engine by stepping it half a
second at a time, which a constant-speed propeller's governor cannot follow -
it drove the blades to full coarse and stalled the engine. `Aircraft::initialize`
now starts those engines itself. A failed engine is stopped with its ignition
off, since JSBSim restarts one that windmills with spark and fuel.
