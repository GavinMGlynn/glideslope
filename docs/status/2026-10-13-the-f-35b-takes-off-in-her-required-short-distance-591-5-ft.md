### The F-35B takes off in her required short distance: 591.5 ft against 600, on her thrust split and drooped flaperons, 2026-10-13 — item done

**What is still missing, first**: **nothing flies her between jet-borne
and wing-borne flight** - nothing converts her in the air, slows her into a
hover or accelerates her out of one, and no pilot's controls or AI plan
reach her lift system; that is now its own open item in Phase 9. Her short
take-off rests on two estimates nothing published gives: **how far her
flaperons droop** (20 degrees) and their lift, which are worth 108 ft of it
(699.4 ft with no droop), and **how far forward her thrust split goes**
(as far as the published rearward reach). Nothing models the fan's induced
lift on the roll, ground effect on the jets or the fan intake's ram drag.

**Her model** (`tools/make_f35b.py`, `assets/jsbsim/aircraft/f35b/f35b.xml`):

- **The thrust split**, `fcs/thrust-split-cmd-norm`, -1 to 1, 1 the most
  forward (nose up): the engine's thrust moved between the nozzle and the
  fan, the total unchanged. Wiegand et al. (AIAA 2018-3368, pinned): "Thrust
  split is defined as the ratio of main engine thrust over lift fan thrust,
  typically used to represent the propulsion system pitching moment applied
  to the aircraft", and "The 3BSM can vector up to 23,900 pounds of thrust
  at the maximum rearward thrust split" - against Rolls-Royce's 18,000, so
  the split moves 0.328 of the nozzle's share; forward, the same, estimated.
  The nozzle passes its share less the split (the reverser's cosine), and
  the fan, the posts and the split are reckoned from what the nozzle would
  pass at the split's middle (`fcs/lift-system-base-lbf`), so the fan gains
  exactly what the nozzle gives up. At the split's middle every property is
  what it was to the last bit, so the hover's flights are unchanged by it.
  `LiftSystem::split` sets it; the hover control law still pitches her on
  the fan's share alone (sim/hover.hpp).
- **The flaperons droop 20 degrees in STOVL mode**, as far as the
  conversion has gone (`fcs/flaperon-droop-deg`): the F-35 Joint Program
  Office's release on her first ski-jump launch (DVIDS, 24 June 2015,
  pinned): "The F-35B's design allows it to automatically position the
  control surfaces and nozzles for takeoff". How far is not published; 20
  degrees leaves 5 of their 25 for roll. Their lift is DATCOM's plain flap
  (Raymer): the wing's slope, 0.315 effectiveness (0.45 at a quarter-chord
  flap, 0.7 of it past 15 degrees) and half the wing behind them, 0.0082 a
  degree, 0.163 at 20; their drag 0.015 at 20; their centre of pressure a
  quarter chord behind the aerodynamic centre, nose down. The droop and an
  aileron command are summed into neither one's travel limit: an aileron
  command fully one way on top of the droop is not clipped at 25 degrees.

**The flight** (`short_takeoff`, `src/sim/figures.cpp`) is the one #171
flew, rotating on the stabilator and the thrust split in place of the fan's
share, **at the same gains** (0.14 a degree, 8 a radian a second), the
same swing (58 degrees at 70 KCAS) and the same attitude (2 degrees under
her 21.6-degree tail strike, read from her contacts): **591.5 ft**, was
708.8. She rotates to 16.9 degrees on the wheels and 19.1 in her first
three seconds airborne, under the strike. Swept again at these gains: 52
degrees at 60 KCAS 685.3 ft, 58 at 60 722.1, 64 at 60 1,329.0, 52 at 70
621.5, 64 at 70 573.8, 70 at 70 touched the runway again, 58 at 75 613.6,
64 at 75 592.3, 58 at 80 650.9, 58 at 85 700.1. The rotation's gains move
it most - 0.14 and 4, 490.8 ft; 0.28 and 4, 430.7 (and 20.3 degrees in
the air, 1.3 from the strike); 0.28 and 8, 499.6 - so the gains kept are
the ones it was flown on, not the shortest found, and the 569 ft of the
SAR's estimate sits inside what the model can do.

**Verification**: `the_f35b_takes_off_short_inside_her_required_600_ft`,
a published figure (the SAR's key performance parameter, 0 to 600 ft),
seen to fail with the droop taken out of her built model (699.4 ft),
reverted; `the_f35b_takes_off_short_in_the_distance_her_own_model_gave`
re-pinned at 591.5 ft within 5 per cent. 129 figures (was 128). `figures
f35b`: hover thrust 40,644.52 lbf, unchanged; her conventional figures
unchanged to the CLI's last digit (Mach 1.61, 1,371.92 nm, 122.24 kt,
2,007.74 ft, 20,390.55 ft/min); hover endurance 5.57 min, unchanged; **the
hover's height strays 0.02 ft (was 0.01) and she touches down at 179.54
ft/min (was 179.56)**: the droop's lift in the 10 kt crosswind and her
let-down. The selftest hash does not move, `182dd6c996e0ee4c`. 98 tests -
every F-35B test, every take-off, tail-strike and hover test, and the
committed-asset checks - pass on linux-release.

**Sources read and not obtained**: the Lockheed papers on her control laws
are behind AIAA's paywall and were not read: Walker, Fuller and Wurth,
"F-35B Integrated Flight-Propulsion Control Development", 2013
International Powered Lift Conference, AIAA 2013-4243; Walker and Allen,
"X-35B STOVL Flight Control Law Design and Flying Qualities", AIAA
2002-6018; and Bordignon and Bessolo, "Control Allocation for the X-35B",
AIAA 2002-6020. No paper by Walker and Fields on unified control was
found. Nothing
here rests on them. No public source gives her flaperons' droop, her STO
nozzle and fan settings by speed, or a "STO stop" (the Harrier's nozzle
lever stop; the F-35B's flight controls set her nozzles themselves, as the
DVIDS release says).
