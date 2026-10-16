### From the review of #171: her take-off attitude from her tail-strike attitude, a STOVL envelope, the hover's wind envelope, and her hover endurance, 2026-10-13 — item still open

**What is missing, first**: **no transition between jet-borne and
wing-borne flight is flown** - nothing converts her in the air, slows her
into a hover or accelerates her out of one; she is refused STOVL mode
above 250 KCAS and nothing more. Her short take-off is 697.5 ft against 600.
No pilot's controls or AI plan reach her lift system.

- **The short take-off's attitude follows her airframe**: the flight reads
  the attitude her tail strikes at from her model's contacts
  (`Aircraft::stance`, as the departure and the lander do), 22.2 degrees
  today, and rotates to `strike_margin_deg` (2) under it (22.2 then; 21.6 since #170, see the entry above), failing if her
  pitch on the wheels reaches it. The rotation's gains, 0.14 of the fan's
  travel a degree and 8 a radian a second, are the figure's attributes.
  #170 (tail strikes from airframes) will move her contacts; on its landing
  this branch is rebased and the figure measured again. At 20.2 degrees,
  swept over the swing's speed (50, 60, 70 KCAS) and angle (48, 52, 58, 64):
  58 degrees at 70 KCAS, 697.5 ft, is taken; 64 at 70 leaves in 688.8 but
  at 60 does not climb on.
- **Her STOVL envelope**: `Aircraft::set_lift_system` refuses STOVL mode
  above `fcs/stovl-limit-kcas`, which her model states as 250 KCAS:
  Rolls-Royce's poster has the LiftFan "operate in STOVL conditions which
  result in a 288 mph crosswind to the LiftFan intake", 250 kt.
  `the_f35b_is_refused_stovl_mode_above_250_kcas_and_allowed_below`:
  refused at 260, allowed at 240, conventional never refused, a C172P
  without a lift system refused one; seen to fail with the refusal taken
  out (allowed at 260).
- **The hover's wind envelope**:
  `the_f35b_hovers_in_every_wind_to_20_kt_from_every_side_at_two_weights`
  flies 34 hovers - her vertical-landing (37,530 lb) and combat (39,750 lb)
  weights, calm and 5, 10, 15 and 20 kt from ahead, her right, behind and
  her left - each lifted from the runway to 150 ft, settled 30 s and held a
  minute. **All 34 hold**: her height strays at most 0.55 ft (20 kt from
  behind) and she drifts at most 7.2 ft (20 kt from behind, at 37,530 lb).
  So the envelope asserted is the whole space flown, to 20 kt from any
  side; above 20 kt, and in gusts or turbulence, nothing is claimed. The
  model has no fan intake ram drag and little side force at hover speeds,
  so wind pushes her less than it would the aeroplane. Seen to fail with
  the law's drift gain at zero (34 not held).
- **The nozzle's swivel makes no thrust**: swivelled sideways, the nozzle's
  axial thrust is its share times the cosine of the swivel (in the reverser
  angle that takes the shaft's power) and its side force the axial thrust
  times the tangent - the share times the sine. Conventional figures and
  the selftest hash `182dd6c996e0ee4c` unchanged.
- **Her fuel in STOVL mode follows the engine's core thrust**: JSBSim's
  turbine burns its specific consumption on the thrust it makes before the
  thruster's reverser, so the fan's and posts' share costs fuel as the
  nozzle's does. `hover_endurance`, a measured figure: lifted to 150 ft at
  the vertical-landing loading (2,330 lb of fuel), fuel burning, she holds
  her height 5.57 min after settling (5 per cent).
- **The hover's throttle top is her model's**: `fcs/stovl-throttle-max`,
  0.99, written by `tools/make_f35b.py` and read by `sim/hover.cpp`, not a
  second constant.

127 figures (was 126). Her 48 tests - figures, take-off, landing, glide,
orbit, the committed model and the three new - pass on linux-release.
