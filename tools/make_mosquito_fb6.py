#!/usr/bin/env python3
"""make_mosquito_fb6.py - glideslope's de Havilland Mosquito FB Mk VI, written
from her trials and Pilot's Notes.

This script carries her airframe's and engine's text verbatim, as written for
glideslope in commit 22cf2f63 (2026-09-19, "The Mosquito FB Mk VI, written
from its trials and Pilot's Notes"), and changes it only by the transforms
listed at the end of this docstring. It is not derived from any other model:
JSBSim has no Mosquito, and no other simulator's Mosquito is used. She is
held to her published figures - fifteen, in assets/figures/mosquito-fb6.xml,
each quoted by paragraph or figure. Until 2026-10-14 the two files were
committed as written and edited by hand; now what is committed is what this
script makes. Her propeller's tables are computed by
tools/make_mosquito_propeller.py, as before.

    python3 tools/make_mosquito_fb6.py           write the files
    python3 tools/make_mosquito_fb6.py --check   exit 1 if what is committed differs

It writes assets/jsbsim/aircraft/mosquito-fb6/mosquito-fb6.xml, the airframe,
and assets/jsbsim/engine/merlin25.xml, the engine. A test fails if what is
committed differs from what it makes; to change her, change this script and
run it.

Where her numbers come from

  Each number in the text below says, where it is set, where it comes from,
  whether it is estimated and what figure it was set to meet; the files keep
  those comments. The documents, which docs/ASSETS.md records with the copies
  consulted, are:

  A&AEE Boscombe Down report 767,e, 12th part, 1 February 1944 - HX809,
    level speed performance at normal and increased boost
    Level speeds and full-throttle heights at +18 lb/sq in, which set the
    engine's rated altitudes and the drag at zero lift and its rise with Mach
    number; the loading; the propellers' type and diameter.
  A&AEE report 767,e, 7th part, 19 September 1943 - HJ679, brief performance
  trials
    Climb rates and the time to 20,000 ft, which set the radiator shutters'
    drag; the boost obtained; HJ679's normal service load, 21,020 lb, which
    sets the empty weight.
  Rolls-Royce, Merlin 24, 25, 26, 27, 224, 225 altitude performance (AVIA
  6/5817, fig. 1)
    The engine's power with height at +18 and 3,000 rpm, which sets maxhp,
    the charge's heating and the supercharger's drive; ram-air-factor 0.8,
    its own assumption; the reduction gear, 0.42:1.
  A.P. 2019E, Pilot's Notes for the Mosquito FB 6, 1950 edition
    The engine's limits (para. 51); the fuel tanks (para. 1); the supercharger
    switch (para. 23) and radiator shutters (para. 24); the stalling speeds
    (para. 43), which set the lift curve's peaks; the flaps' trim change
    (para. 42); the tail wheel's castoring (para. 40); the single-engine
    safety speeds, which set the rudder's power.
  A.P. 2019E, Pilot's Notes, 1944 edition
    The swing on take-off, which sets the slipstreams' push on the fin; the
    tail raised on the take-off run "by a light forward pressure" (para. 49);
    the stalling speeds at full load.
  Aircraft Data Sheet, Mosquito B Mk IV, card 3(b), 1 May 1944
    Span 54 ft 2 in, gross wing area 454 sq ft, length 40 ft 6 in; the
    take-off distance over 50 ft.
  Air Fighting Development Unit report 74, Mosquito VI tactical trials
    Loadings, against which the empty weight's estimate is checked.
  de Havilland, HJ679's position error, 20 September 1943, fig. 3
    That her indicated stalling speeds may be taken as calibrated.
  Bridgeman 1998, as Wikipedia's list of Merlin variants gives it
    The Packard Merlin 28's climbing power, which checks the engine's
    mixture table.

  Estimated here, from the geometry by the usual textbook methods, and
  marked so where they are set: the stability derivatives, the moments of
  inertia (keeping the one measured ratio found, pitch to roll about 0.78),
  the empty weight's split from the loading, the undercarriage, the fin's
  area and the controls' travel but the elevator's.

The text, as written

  The airframe is kept in sections, joined in order: HEADER (the sources and
  what is known and estimated), METRICS, MASS_BALANCE, GROUND_REACTIONS,
  PROPULSION, FLIGHT_CONTROL (the controls, the engines' mixture, charge
  heating, supercharger gear change and friction, and the slipstreams),
  AERODYNAMICS_HEAD (ground effect, the tailplane's dynamic pressure, the
  lift curve) and its axes LIFT, DRAG, SIDE, ROLL, PITCH and YAW, then
  TAIL. MERLIN_25 is the engine.

The changes to the text as written, and what each is for:

  The alpha-rate term on the dynamic pressure without its sideways part
                        (tools/alpha_rate.py, JSBSim issue #89, 2026-10-14):
                        braked to a stop in a crosswind, her angle of attack
                        is undefined and its rate swings, and her
                        pitch-alpha-rate term, on the whole dynamic pressure,
                        pitched her by it; it is on aero/qbarUW-psf, as the
                        other models' have been since #198.

**Her pitch terms keyed to the angle of attack on the dynamic pressure without
its sideways part** (alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15):
pitch-alpha and pitch-stall (the moment past the stall is her Cm-alpha curve
continued, keyed to the same angle). Standing or slowing in a crosswind her
angle of attack, atan2(w, u), is any angle at all - 92 to 119 degrees standing
on her brakes in 15 kt across - and on the whole dynamic pressure, which counts
the crosswind, they pitched her nose down by up to 2,940 ft-lb between them; on
aero/qbarUW-psf, the air's moment is at most 172 ft-lb nose down. Her wing and
tailplane are not swept: the case JSBSim issue #89 tested (on the C172) and the
FlightGear wiki's "JSBSim Aerodynamics" writes up, QBarUW "more appropriate for
Cm" than the whole QBar. In flight, at a few degrees of sideslip, the two
differ by its cosine squared.

**Her lift keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-15): lift-alpha, which reads
the angle of attack through aero/function/lift-coefficient-wing, her table of
it and the flaps (with_alpha_terms_on_qbar_uw follows such a function).
Standing on her brakes in 15 kt straight across, her angle of attack, atan2(w,
u), was -179 to 179 degrees, and on the whole dynamic pressure, which counts
the crosswind, the term lifted her by up to 410 lb either way; on
aero/qbarUW-psf, nothing. JSBSim issue #89's guidance, written into the
FlightGear wiki's "JSBSim Aerodynamics": QBarUW "is more appropriate for CL
than the generic QBar" - a wing's lift is made by the air in the plane of u and
w. Her wing is not swept: the case issue #89 tested. In flight, at a few
degrees of sideslip, the two differ by its cosine squared. **Not moved**:
lift-slipstream, the same coefficient on her slipstreams' own pressure, from u
and the propellers' induced velocity, which leaves out the crosswind already;
standing at idle it gives 230 to 265 lb. Her lift terms not keyed to the angle
of attack (lift-elevator and lift-pitch-rate) were not this change's: they are
moved below (2026-10-23).

**Her wing's drag keyed to the angle of attack on the same dynamic pressure**
(alpha_rate.with_alpha_terms_on_qbar_uw, 2026-10-16): drag-stall, her drag past
the stall against the angle of attack. Standing on her brakes in 15 kt straight
across, her angle of attack, atan2(w, u), was -178 to 178 degrees, and on the
whole dynamic pressure, which counts the crosswind, it drew her along the beam
wind by up to 104 lb; on aero/qbarUW-psf, nothing. The same reasoning as her
lift's, above: a wing's drag at its angle of attack is made by the air in the
plane of u and w, and the sideways part runs along its span - the drag that
rolled the J-3 Cub onto her wing standing in a beam wind (make_j3cub.py). In
flight, at a few degrees of sideslip, the two differ by its cosine squared.

**Her slipstreams' lift at the angle the air in them meets the wing**
(with_slipstream_lift_at_its_own_angle, 2026-10-20): lift-slipstream read her
wing's lift coefficient at her angle of attack, atan2(w, u). Standing on her
brakes that is any angle her creep gives, and the term was the table's -0.69
to 0.99 on the slipstreams' pressure: -2,645 to +3,790 lb, flipping as she
crept (at +9 boost from -2,570 to +2,054 lb, which moved her take-off swing
figure). The air in a slipstream moves along her x axis at u plus 1.5 times
the induced velocity - the speed the term already gives it - and across it
at w, as outside it, so it meets the wing at atan2(w, u + 1.5 vi): each
side's coefficient is now her wing's table read there, for its own
propeller. Standing it is near nothing, 0.17 flaps up, 55 to 671 lb from
closed to full throttle; in flight at low power it is her angle of attack,
and at high power and low speed less than it, as a slipstream's is. Where
one propeller gives nothing the angle may again be any, but that side's term
is multiplied by its own extra pressure, then nothing too. The table is the
wing's, copied from it, so the two cannot part.

**Her lift terms not keyed to the angle of attack on the dynamic pressure
without its sideways part** (alpha_rate.with_unkeyed_terms_on_qbar_uw,
2026-10-23, with_alpha_terms_on_qbar_uw for a lift with pitch rate whose
coefficient the angle picks): lift-elevator and lift-pitch-rate. On the whole
dynamic pressure they counted the crosswind standing. Standing on her brakes
with her engines stopped in 15 kt straight across, a lifting control set one at
a time, they lifted her by up to 25 lb (stick back, as she stands); now under
0.1 lb at every setting (the standing test in tests/unit/test_departure.cpp).
Her lift-elevator is on her tailplane's pressure, part of it her slipstreams';
it moves onto a copy of that pressure (with_elevator_lift_on_qbar_uw, below),
her elevator's pitching moment left on the first. JSBSim issue #89's guidance
puts the whole of CL on QBarUW; alpha_rate.py says why, for swept surfaces too.
In flight, at a few degrees of sideslip, the two differ by its cosine squared.
"""

import pathlib
import re
import sys

import alpha_rate

ROOT = pathlib.Path(__file__).resolve().parent.parent
AIRFRAME = ROOT / "assets" / "jsbsim" / "aircraft" / "mosquito-fb6" / "mosquito-fb6.xml"
ENGINE = ROOT / "assets" / "jsbsim" / "engine" / "merlin25.xml"


# ---------------------------------------------------------------------------
# The airframe as written on 2026-09-19, section by section.
# ---------------------------------------------------------------------------

# The file's own account of her sources, what is known and what estimated,
# and its references.
HEADER = """\
<?xml version="1.0"?>
<!--
  de Havilland Mosquito FB Mk VI, two Rolls-Royce Merlin 25s.

  Written for glideslope from the sources below, which docs/ASSETS.md records;
  no other simulator's model of the aircraft is used. The published figures it
  is held to, and where each comes from, are in assets/figures/mosquito-fb6.xml;
  docs/PROJECT_STATUS.md gives what the model measures against each.

  It is the aircraft as the A&AEE flew it for its level-speed and climb trials
  - HX809 (A&AEE/767,e, 12th part, 1 February 1944) and HJ679 (7th part,
  19 September 1943) - with a small drop tank under each wing: the drag of the
  tanks is part of the airframe's, and removing them is not modelled (HJ679
  was 5 mph faster without them, low down).

  What is known and what is estimated
    From the sources: span 54 ft 2 in, gross wing area 454 sq ft, length
    40 ft 6 in (Aircraft Data Sheet, Mosquito B Mk IV, card 3(b), 1 May 1944);
    tailplane area 87.4 sq ft; the propellers 12 ft across and both turning
    the same way, clockwise seen from behind; flaps to 45 degrees; elevator
    21.5 degrees up and 12.5 down; the trial weights and the fuel tanks
    (A.P. 2019E, Pilot's Notes, 1950, para. 1: 452 gallons internal).
    Estimated, here, from the geometry by the usual textbook methods, and
    marked so where they are set: the stability derivatives, the moments of
    inertia (the one measured ratio found, pitch to roll of about 0.78, is
    kept), the empty weight's split from the loading, and the undercarriage.
    Set to meet the published figures, and marked so: the drag at zero lift
    and its rise with Mach number (the level speeds), the lift curve's peaks
    (the stalling speeds), the radiator shutters' drag (the climbs), the
    slipstream's push on the fin (the swing on take-off) and the rudder's
    power (the single-engine safety speeds) - the fin's area was not found.

    What JSBSim does not model and this file adds: the slipstreams' lift on
    the wing behind the propellers and their dynamic pressure on the
    tailplane, the fin's induced drag with the rudder over, a stopped
    propeller's drag, and - for the engines - the automatic mixture control
    with its enrichment above +9, the charge's heating, the supercharger's
    automatic gear change, and a stopped engine's friction.

  Frame: x aft from the nose, y to starboard, z up from the thrust line, in
  inches. Engine 0 is the port engine.
-->
<fdm_config name="Mosquito FB Mk VI" version="2.0" release="ALPHA">

  <fileheader>
    <author> glideslope </author>
    <description> de Havilland Mosquito FB Mk VI, Merlin 25 </description>
    <reference refID="A&amp;AEE/767,e 12th part" author="A&amp;AEE Boscombe Down"
               title="Mosquito FB Mk VI HX809: level speed performance at normal and increased boost rating"
               date="1944-02-01"/>
    <reference refID="A&amp;AEE/767,e 7th part" author="A&amp;AEE Boscombe Down"
               title="Mosquito FB Mk VI HJ679: brief performance trials" date="1943-09-19"/>
    <reference refID="A.P. 2019E P.N." author="Air Ministry"
               title="Pilot's Notes, Mosquito FB 6" date="1950"/>
    <reference refID="AVIA 6/5817 fig. 1" author="Rolls-Royce"
               title="Merlin 24, 25, 26, 27, 224, 225 altitude performance" date="1944"/>
  </fileheader>

"""

# The wing and tail: the B Mk IV data sheet's span and area; the tail's arms
# and the fin estimated.
METRICS = """\
  <metrics>
    <wingarea unit="FT2"> 454 </wingarea>
    <wingspan unit="FT"> 54.17 </wingspan>
    <!-- the mean chord, area over span -->
    <chord unit="FT"> 8.38 </chord>
    <htailarea unit="FT2"> 87.4 </htailarea>
    <!-- estimated: the wing's quarter chord to the tailplane's -->
    <htailarm unit="FT"> 22 </htailarm>
    <!-- estimated; the fin's area was not found -->
    <vtailarea unit="FT2"> 45 </vtailarea>
    <vtailarm unit="FT"> 23 </vtailarm>
    <location name="AERORP" unit="IN">
      <x> 175 </x>
      <y> 0 </y>
      <z> 0 </z>
    </location>
    <!-- the pilot's eyes: the left-hand seat, beside the navigator -->
    <location name="EYEPOINT" unit="IN">
      <x> 112 </x>
      <y> -12 </y>
      <z> 34 </z>
    </location>
    <location name="VRP" unit="IN">
      <x> 0 </x>
      <y> 0 </y>
      <z> 0 </z>
    </location>
  </metrics>

"""

# Inertia estimated; the empty weight from HJ679's service load; crew and
# bomb bay.
MASS_BALANCE = """\
  <mass_balance>
    <!-- Estimated for the aircraft without crew or fuel; JSBSim adds the crew,
         the stores and the fuel as point masses. Loaded to 20,400 lb this
         gives about 34,000, 26,500 and 58,000 slug ft^2: the radii of
         gyration of a twin with its engines and fuel in the wing, and pitch
         to roll about 0.78, as the one measured ratio found has it. -->
    <ixx unit="SLUG*FT2"> 26000 </ixx>
    <iyy unit="SLUG*FT2"> 25500 </iyy>
    <izz unit="SLUG*FT2"> 50000 </izz>
    <ixz unit="SLUG*FT2"> 0 </ixz>
    <!-- Ready to fly less crew and fuel: guns, ammunition, oil and coolant.
         No source gives it. Air Fighting Development Unit report 74's
         loadings put it near 15,800 lb; this is the weight that makes
         HJ679's normal service load, 21,020 lb, its crew, every tank full and
         two 250 lb bombs. -->
    <emptywt unit="LBS"> 16144 </emptywt>
    <location name="CG" unit="IN">
      <x> 181 </x>
      <y> 0 </y>
      <z> 0 </z>
    </location>
    <pointmass name="Pilot">
      <weight unit="LBS"> 200 </weight>
      <location unit="IN">
        <x> 112 </x>
        <y> -12 </y>
        <z> 6 </z>
      </location>
    </pointmass>
    <pointmass name="Navigator">
      <weight unit="LBS"> 200 </weight>
      <location unit="IN">
        <x> 118 </x>
        <y> 12 </y>
        <z> 2 </z>
      </location>
    </pointmass>
    <!-- the rear half of the bomb bay: two 250 or 500 lb bombs -->
    <pointmass name="Bomb bay">
      <weight unit="LBS"> 0 </weight>
      <location unit="IN">
        <x> 196 </x>
        <y> 0 </y>
        <z> -14 </z>
      </location>
    </pointmass>
  </mass_balance>

"""

# The undercarriage, estimated, placed by the Pilot's Notes' handling; the
# airframe's points that can touch.
GROUND_REACTIONS = """\
  <!-- The undercarriage, estimated. The main wheels are under the nacelles,
       16 ft 4 in apart, 16 in ahead of the centre of gravity: sitting on its
       tail at 12 degrees, the aircraft's centre of gravity is 19 degrees
       behind the wheels' vertical, and the tail comes up with "a light
       forward pressure on the control column" (Pilot's Notes, 1944, para.
       49). The tail wheel castors - "taxy forward a few yards to straighten
       the tailwheel" (1950, para. 40) - so it neither steers nor holds the
       aircraft straight. All three retract, in about fifteen seconds. -->
  <ground_reactions>
    <contact type="BOGEY" name="LEFT_MAIN">
      <location unit="IN">
        <x> 165 </x>
        <y> -98 </y>
        <z> -86 </z>
      </location>
      <static_friction> 0.8 </static_friction>
      <dynamic_friction> 0.5 </dynamic_friction>
      <rolling_friction> 0.02 </rolling_friction>
      <spring_coeff unit="LBS/FT"> 20000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 3000 </damping_coeff>
      <damping_coeff_rebound unit="LBS/FT/SEC"> 6000 </damping_coeff_rebound>
      <max_steer unit="DEG"> 0 </max_steer>
      <brake_group> LEFT </brake_group>
      <retractable> 1 </retractable>
    </contact>
    <contact type="BOGEY" name="RIGHT_MAIN">
      <location unit="IN">
        <x> 165 </x>
        <y> 98 </y>
        <z> -86 </z>
      </location>
      <static_friction> 0.8 </static_friction>
      <dynamic_friction> 0.5 </dynamic_friction>
      <rolling_friction> 0.02 </rolling_friction>
      <spring_coeff unit="LBS/FT"> 20000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 3000 </damping_coeff>
      <damping_coeff_rebound unit="LBS/FT/SEC"> 6000 </damping_coeff_rebound>
      <max_steer unit="DEG"> 0 </max_steer>
      <brake_group> RIGHT </brake_group>
      <retractable> 1 </retractable>
    </contact>
    <contact type="BOGEY" name="TAIL">
      <location unit="IN">
        <x> 440 </x>
        <y> 0 </y>
        <z> -26 </z>
      </location>
      <static_friction> 0.8 </static_friction>
      <dynamic_friction> 0.5 </dynamic_friction>
      <rolling_friction> 0.03 </rolling_friction>
      <spring_coeff unit="LBS/FT"> 10000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 1500 </damping_coeff>
      <damping_coeff_rebound unit="LBS/FT/SEC"> 3000 </damping_coeff_rebound>
      <max_steer unit="DEG"> 360 </max_steer>
      <brake_group> NONE </brake_group>
      <retractable> 1 </retractable>
    </contact>
    <contact type="STRUCTURE" name="NOSE">
      <location unit="IN">
        <x> 0 </x>
        <y> 0 </y>
        <z> -10 </z>
      </location>
      <static_friction> 0.4 </static_friction>
      <dynamic_friction> 0.4 </dynamic_friction>
      <spring_coeff unit="LBS/FT"> 40000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 8000 </damping_coeff>
    </contact>
    <contact type="STRUCTURE" name="BELLY">
      <location unit="IN">
        <x> 190 </x>
        <y> 0 </y>
        <z> -30 </z>
      </location>
      <static_friction> 0.4 </static_friction>
      <dynamic_friction> 0.4 </dynamic_friction>
      <spring_coeff unit="LBS/FT"> 40000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 8000 </damping_coeff>
    </contact>
    <contact type="STRUCTURE" name="LEFT_PROP_TIP">
      <location unit="IN">
        <x> 66 </x>
        <y> -98 </y>
        <z> -72 </z>
      </location>
      <static_friction> 0.4 </static_friction>
      <dynamic_friction> 0.4 </dynamic_friction>
      <spring_coeff unit="LBS/FT"> 40000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 8000 </damping_coeff>
    </contact>
    <contact type="STRUCTURE" name="RIGHT_PROP_TIP">
      <location unit="IN">
        <x> 66 </x>
        <y> 98 </y>
        <z> -72 </z>
      </location>
      <static_friction> 0.4 </static_friction>
      <dynamic_friction> 0.4 </dynamic_friction>
      <spring_coeff unit="LBS/FT"> 40000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 8000 </damping_coeff>
    </contact>
    <contact type="STRUCTURE" name="LEFT_WING_TIP">
      <location unit="IN">
        <x> 190 </x>
        <y> -325 </y>
        <z> 12 </z>
      </location>
      <static_friction> 0.4 </static_friction>
      <dynamic_friction> 0.4 </dynamic_friction>
      <spring_coeff unit="LBS/FT"> 40000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 8000 </damping_coeff>
    </contact>
    <contact type="STRUCTURE" name="RIGHT_WING_TIP">
      <location unit="IN">
        <x> 190 </x>
        <y> 325 </y>
        <z> 12 </z>
      </location>
      <static_friction> 0.4 </static_friction>
      <dynamic_friction> 0.4 </dynamic_friction>
      <spring_coeff unit="LBS/FT"> 40000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 8000 </damping_coeff>
    </contact>
    <contact type="STRUCTURE" name="TAIL_CONE">
      <location unit="IN">
        <x> 480 </x>
        <y> 0 </y>
        <z> 0 </z>
      </location>
      <static_friction> 0.4 </static_friction>
      <dynamic_friction> 0.4 </dynamic_friction>
      <spring_coeff unit="LBS/FT"> 40000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 8000 </damping_coeff>
    </contact>
  </ground_reactions>

"""

# Two Merlin 25s and their propellers; the Pilot's Notes' tanks.
PROPULSION = """\
  <propulsion>
    <!-- Both propellers turn clockwise seen from behind (sense 1), so the
         port engine is the critical one: with the aircraft nose-up the
         starboard side of each disc gives more thrust, moving the live
         starboard engine's thrust further from the centre line. -->
    <engine file="merlin25">
      <location unit="IN">
        <x> 110 </x>
        <y> -98 </y>
        <z> 0 </z>
      </location>
      <feed> 0 </feed>
      <feed> 1 </feed>
      <feed> 2 </feed>
      <feed> 3 </feed>
      <feed> 5 </feed>
      <thruster file="prop_dh_hydromatic">
        <location unit="IN">
          <x> 66 </x>
          <y> -98 </y>
          <z> 0 </z>
        </location>
        <orient unit="DEG">
          <roll> 0 </roll>
          <pitch> 0 </pitch>
          <yaw> 0 </yaw>
        </orient>
        <sense> 1 </sense>
        <p_factor> 10 </p_factor>
      </thruster>
    </engine>
    <engine file="merlin25">
      <location unit="IN">
        <x> 110 </x>
        <y> 98 </y>
        <z> 0 </z>
      </location>
      <feed> 0 </feed>
      <feed> 1 </feed>
      <feed> 2 </feed>
      <feed> 4 </feed>
      <feed> 6 </feed>
      <thruster file="prop_dh_hydromatic">
        <location unit="IN">
          <x> 66 </x>
          <y> 98 </y>
          <z> 0 </z>
        </location>
        <orient unit="DEG">
          <roll> 0 </roll>
          <pitch> 0 </pitch>
          <yaw> 0 </yaw>
        </orient>
        <sense> 1 </sense>
        <p_factor> 10 </p_factor>
      </thruster>
    </engine>

    <!-- The Pilot's Notes' tanks (para. 1), at 7.2 lb a gallon: the centre
         tanks and the four inner wing tanks feed both engines through the
         collector box, each side's outer tanks its own engine, and each drop
         tank the engine on its side (in the aircraft it refills the outer
         tanks). -->
    <tank type="FUEL">    <!-- 0: centre tanks, 50 gal -->
      <location unit="IN">
        <x> 176 </x>
        <y> 0 </y>
        <z> 10 </z>
      </location>
      <capacity unit="LBS"> 360 </capacity>
      <contents unit="LBS"> 360 </contents>
    </tank>
    <tank type="FUEL">    <!-- 1: port inner wing tanks, 143 gal -->
      <location unit="IN">
        <x> 178 </x>
        <y> -50 </y>
        <z> 0 </z>
      </location>
      <capacity unit="LBS"> 1030 </capacity>
      <contents unit="LBS"> 1030 </contents>
    </tank>
    <tank type="FUEL">    <!-- 2: starboard inner wing tanks, 143 gal -->
      <location unit="IN">
        <x> 178 </x>
        <y> 50 </y>
        <z> 0 </z>
      </location>
      <capacity unit="LBS"> 1030 </capacity>
      <contents unit="LBS"> 1030 </contents>
    </tank>
    <tank type="FUEL">    <!-- 3: port outer wing tanks, 58 gal -->
      <location unit="IN">
        <x> 186 </x>
        <y> -160 </y>
        <z> 4 </z>
      </location>
      <capacity unit="LBS"> 418 </capacity>
      <contents unit="LBS"> 418 </contents>
    </tank>
    <tank type="FUEL">    <!-- 4: starboard outer wing tanks, 58 gal -->
      <location unit="IN">
        <x> 186 </x>
        <y> 160 </y>
        <z> 4 </z>
      </location>
      <capacity unit="LBS"> 418 </capacity>
      <contents unit="LBS"> 418 </contents>
    </tank>
    <tank type="FUEL">    <!-- 5: port drop tank, 50 gal -->
      <location unit="IN">
        <x> 180 </x>
        <y> -130 </y>
        <z> -24 </z>
      </location>
      <capacity unit="LBS"> 360 </capacity>
      <contents unit="LBS"> 0 </contents>
    </tank>
    <tank type="FUEL">    <!-- 6: starboard drop tank, 50 gal -->
      <location unit="IN">
        <x> 180 </x>
        <y> 130 </y>
        <z> -24 </z>
      </location>
      <capacity unit="LBS"> 360 </capacity>
      <contents unit="LBS"> 0 </contents>
    </tank>
  </propulsion>

"""

# The controls' travel, flaps and undercarriage; and what JSBSim leaves out of
# the engines: the automatic mixture, the charge's heating, the supercharger's
# gear change, a stopped engine's friction.
FLIGHT_CONTROL = """\
  <flight_control name="Mosquito FB Mk VI">

    <!-- The superchargers' gear change switch: 1 AUTO, 0 MOD (low gear at
         all heights). Pilot's Notes para. 23. -->
    <property value="1">fcs/supercharger-cmd-norm</property>
    <!-- Each engine's radiator shutters: 0 shut, 1 open; there is nothing
         between (Pilot's Notes para. 24). Port first. -->
    <property value="0">fcs/cooling-flaps-cmd-norm[0]</property>
    <property value="0">fcs/cooling-flaps-cmd-norm[1]</property>

    <channel name="Pitch">
      <summer name="fcs/pitch-trim-sum">
        <input>fcs/elevator-cmd-norm</input>
        <input>fcs/pitch-trim-cmd-norm</input>
        <clipto>
          <min>-1</min>
          <max>1</max>
        </clipto>
      </summer>
      <!-- 21.5 degrees up (nose up, negative here), 12.5 down -->
      <aerosurface_scale name="fcs/elevator-control">
        <input>fcs/pitch-trim-sum</input>
        <range>
          <min>-0.3752</min>
          <max>0.2182</max>
        </range>
        <output>fcs/elevator-pos-rad</output>
      </aerosurface_scale>
    </channel>

    <channel name="Roll">
      <!-- estimated: 15 degrees each way -->
      <aerosurface_scale name="fcs/left-aileron-control">
        <input>fcs/aileron-cmd-norm</input>
        <range>
          <min>-0.2618</min>
          <max>0.2618</max>
        </range>
        <output>fcs/left-aileron-pos-rad</output>
      </aerosurface_scale>
      <aerosurface_scale name="fcs/right-aileron-control">
        <input>fcs/aileron-cmd-norm</input>
        <range>
          <min>0.2618</min>
          <max>-0.2618</max>
        </range>
        <output>fcs/right-aileron-pos-rad</output>
      </aerosurface_scale>
    </channel>

    <channel name="Yaw">
      <!-- estimated: 25 degrees each way; the rudder's power, below, is what
           the safety speed sets -->
      <aerosurface_scale name="fcs/rudder-control">
        <input>fcs/rudder-cmd-norm</input>
        <range>
          <min>-0.4363</min>
          <max>0.4363</max>
        </range>
        <output>fcs/rudder-pos-rad</output>
      </aerosurface_scale>
    </channel>

    <channel name="Flaps">
      <!-- 0 to 45 degrees over the lever's travel, hydraulically, in about
           ten seconds -->
      <kinematic name="fcs/flap-control">
        <input>fcs/flap-cmd-norm</input>
        <traverse>
          <setting>
            <position>0</position>
            <time>0</time>
          </setting>
          <setting>
            <position>45</position>
            <time>10</time>
          </setting>
        </traverse>
        <output>fcs/flap-pos-deg</output>
      </kinematic>
    </channel>

    <channel name="Undercarriage">
      <kinematic name="fcs/gear-control">
        <input>gear/gear-cmd-norm</input>
        <traverse>
          <setting>
            <position>0</position>
            <time>0</time>
          </setting>
          <setting>
            <position>1</position>
            <time>15</time>
          </setting>
        </traverse>
        <output>gear/gear-pos-norm</output>
      </kinematic>
    </channel>

    <channel name="Engines">
      <!-- The carburettors' automatic mixture control: the mixture follows
           the air's pressure, so the lever at 1 is the automatic setting and
           at 0 cuts the engine off. Up to +9 lb/sq in it holds an
           equivalence ratio of 1.22, rich as the Merlin ran at climbing
           power; above it the carburettor enriches the mixture against
           detonation, to 1.49 at +18 (a fuel-air ratio of 0.101), which
           costs the engine 7% of the power the same air would otherwise
           make - so +18 gives proportionally less than +9, as the engine's
           ratings do. Per engine, from its own manifold pressure; the
           constant is 1 / 1.3 / 2116.22, JSBSim's full-rich equivalence
           ratio and the sea-level pressure. -->
      <fcs_function name="fcs/mixture-port">
        <function>
          <product>
            <property>fcs/mixture-cmd-norm[0]</property>
            <property>atmosphere/P-psf</property>
            <value>0.00036349</value>
            <sum>
              <value>1.22</value>
              <product>
                <value>0.27</value>
                <min>
                  <max>
                    <quotient>
                      <difference>
                        <property>propulsion/engine[0]/map-inhg</property>
                        <value>48.25</value>
                      </difference>
                      <value>18.33</value>
                    </quotient>
                    <value>0</value>
                  </max>
                  <value>1</value>
                </min>
              </product>
            </sum>
          </product>
        </function>
        <output>fcs/mixture-pos-norm[0]</output>
      </fcs_function>
      <fcs_function name="fcs/mixture-starboard">
        <function>
          <product>
            <property>fcs/mixture-cmd-norm[1]</property>
            <property>atmosphere/P-psf</property>
            <value>0.00036349</value>
            <sum>
              <value>1.22</value>
              <product>
                <value>0.27</value>
                <min>
                  <max>
                    <quotient>
                      <difference>
                        <property>propulsion/engine[1]/map-inhg</property>
                        <value>48.25</value>
                      </difference>
                      <value>18.33</value>
                    </quotient>
                    <value>0</value>
                  </max>
                  <value>1</value>
                </min>
              </product>
            </sum>
          </product>
        </function>
        <output>fcs/mixture-pos-norm[1]</output>
      </fcs_function>

      <!-- The supercharger gear change. On AUTO an aneroid engages FS gear
           climbing through about 7,000 ft (Pilot's Notes para. 23, Merlin
           25) and MS gear again "at slightly lower altitudes" descending -
           500 ft lower here. On MOD, MS gear. -->
      <switch name="fcs/supercharger-fs-gear">
        <default value="0"/>
        <test logic="AND" value="1">
          fcs/supercharger-cmd-norm gt 0.5
          <test logic="OR">
            atmosphere/pressure-altitude gt 7000
            <test logic="AND">
              atmosphere/pressure-altitude gt 6500
              fcs/supercharger-fs-gear eq 1
            </test>
          </test>
        </test>
        <output>propulsion/engine[0]/boost-speed</output>
        <output>propulsion/engine[1]/boost-speed</output>
      </switch>

      <!-- The charge's temperature. JSBSim takes the air the cylinders
           breathe to be at the outside air's temperature; it is hotter, by
           the ram's heating (the total temperature) and by the impeller's,
           about 90 K at 3,000 rpm in MS gear and 121 K in FS gear (its tip
           speed 16% higher), as the square of the rpm. So the charge is
           less dense than JSBSim reckons, by less in colder air: the
           engine's volumetric efficiency, 0.85, is scaled by
             T (T0 + dT0) / (T0 (Ttotal + dT)),  T0 = 288.15 K, dT0 = 90 K,
           which leaves it at 0.85 at 15 C and rest in MS gear at full rpm,
           where maxhp calibrates it. In Rankine: 518.67 and 162. -->
      <fcs_function name="fcs/impeller-heating-port-r">
        <function>
          <product>
            <sum>
              <value>162</value>
              <product>
                <value>55.8</value>
                <property>fcs/supercharger-fs-gear</property>
              </product>
            </sum>
            <pow>
              <quotient>
                <property>propulsion/engine[0]/engine-rpm</property>
                <value>3000</value>
              </quotient>
              <value>2</value>
            </pow>
          </product>
        </function>
      </fcs_function>
      <fcs_function name="fcs/impeller-heating-starboard-r">
        <function>
          <product>
            <sum>
              <value>162</value>
              <product>
                <value>55.8</value>
                <property>fcs/supercharger-fs-gear</property>
              </product>
            </sum>
            <pow>
              <quotient>
                <property>propulsion/engine[1]/engine-rpm</property>
                <value>3000</value>
              </quotient>
              <value>2</value>
            </pow>
          </product>
        </function>
      </fcs_function>
      <fcs_function name="fcs/charge-port">
        <function>
          <quotient>
            <product>
              <value>0.85</value>
              <property>atmosphere/T-R</property>
              <value>680.67</value>
            </product>
            <product>
              <value>518.67</value>
              <sum>
                <property>propulsion/tat-r</property>
                <property>fcs/impeller-heating-port-r</property>
              </sum>
            </product>
          </quotient>
        </function>
        <output>propulsion/engine[0]/volumetric-efficiency</output>
      </fcs_function>
      <fcs_function name="fcs/charge-starboard">
        <function>
          <quotient>
            <product>
              <value>0.85</value>
              <property>atmosphere/T-R</property>
              <value>680.67</value>
            </product>
            <product>
              <value>518.67</value>
              <sum>
                <property>propulsion/tat-r</property>
                <property>fcs/impeller-heating-starboard-r</property>
              </sum>
            </product>
          </quotient>
        </function>
        <output>propulsion/engine[1]/volumetric-efficiency</output>
      </fcs_function>

      <!-- The engines' friction. In FS gear the impeller turns faster and
           takes more of the engine's power, as the cube of its speed (see
           engine/merlin25.xml). And an engine that has stopped firing still
           has its pistons, bearings and supercharger to turn: JSBSim charges
           a running engine for them (its friction mean effective pressure,
           about 296 hp at 3,000 rpm here) but not one that has stopped, so a
           dead engine windmilling would drag its propeller hardly at all -
           where "the drag of a windmilling propeller is very high" (Pilot's
           Notes para. 55). This charges it the same, by rpm: (18,400 x the
           mean piston speed in m/s + 46,500) Pa over the displacement, a
           quarter of the revolutions. 1.5 hp is JSBSim's own static
           friction, which this replaces. -->
      <fcs_function name="fcs/friction-port-hp">
        <function>
          <sum>
            <value>1.5</value>
            <product>
              <property>fcs/supercharger-fs-gear</property>
              <value>77</value>
              <pow>
                <quotient>
                  <property>propulsion/engine[0]/engine-rpm</property>
                  <value>3000</value>
                </quotient>
                <value>3</value>
              </pow>
            </product>
            <product>
              <difference>
                <value>1</value>
                <property>propulsion/engine[0]/set-running</property>
              </difference>
              <property>propulsion/engine[0]/engine-rpm</property>
              <sum>
                <product>
                  <value>0.00002822</value>
                  <property>propulsion/engine[0]/engine-rpm</property>
                </product>
                <value>0.01404</value>
              </sum>
            </product>
          </sum>
        </function>
        <output>propulsion/engine[0]/friction-hp</output>
      </fcs_function>
      <fcs_function name="fcs/friction-starboard-hp">
        <function>
          <sum>
            <value>1.5</value>
            <product>
              <property>fcs/supercharger-fs-gear</property>
              <value>77</value>
              <pow>
                <quotient>
                  <property>propulsion/engine[1]/engine-rpm</property>
                  <value>3000</value>
                </quotient>
                <value>3</value>
              </pow>
            </product>
            <product>
              <difference>
                <value>1</value>
                <property>propulsion/engine[1]/set-running</property>
              </difference>
              <property>propulsion/engine[1]/engine-rpm</property>
              <sum>
                <product>
                  <value>0.00002822</value>
                  <property>propulsion/engine[1]/engine-rpm</property>
                </product>
                <value>0.01404</value>
              </sum>
            </product>
          </sum>
        </function>
        <output>propulsion/engine[1]/friction-hp</output>
      </fcs_function>
    </channel>
  </flight_control>

"""

# Ground effect, the tailplane's dynamic pressure in the slipstreams, and the
# lift curve set to the Pilot's Notes' stalling speeds.
AERODYNAMICS_HEAD = """\
  <aerodynamics>
    <alphalimits unit="DEG">
      <min>-10</min>
      <max>25</max>
    </alphalimits>

    <!-- Ground effect on the induced drag (McCormick's approximation, with h
         the height over the span) and a little more lift near the ground. -->
    <function name="aero/function/ground-effect-induced-drag">
      <table>
        <independentVar>aero/h_b-mac-ft</independentVar>
        <tableData>
          0.00  0.00
          0.05  0.39
          0.10  0.72
          0.20  0.91
          0.30  0.96
          0.50  0.99
          1.00  1.00
        </tableData>
      </table>
    </function>
    <function name="aero/function/ground-effect-lift">
      <table>
        <independentVar>aero/h_b-mac-ft</independentVar>
        <tableData>
          0.00  1.10
          0.10  1.06
          0.20  1.03
          0.40  1.01
          1.00  1.00
        </tableData>
      </table>
    </function>

    <!-- The tailplane's dynamic pressure. It spans about 19 ft, and its outer
         parts sit in the propellers' slipstreams, 8 ft either side of the
         centre line; taken as 40% in them. A slipstream's far wake moves at
         the aircraft's speed plus twice the induced velocity at the disc
         (JSBSim's, by momentum theory). At high power and low speed it is
         what raises the tail on the take-off run "by a light forward
         pressure" (Pilot's Notes, 1944, para. 49); in flight it adds little.
    -->
    <function name="aero/function/qbar-tail-psf">
      <sum>
        <product>
          <value>0.6</value>
          <property>aero/qbar-psf</property>
        </product>
        <product>
          <value>0.4</value>
          <value>0.5</value>
          <property>atmosphere/rho-slugs_ft3</property>
          <pow>
            <sum>
              <property>velocities/u-aero-fps</property>
              <property>propulsion/engine[0]/prop-induced-velocity_fps</property>
              <property>propulsion/engine[1]/prop-induced-velocity_fps</property>
            </sum>
            <value>2</value>
          </pow>
        </product>
      </sum>
    </function>

    <!-- The whole aircraft's lift with angle of attack and flap. The
         slope, 4.9 a radian, is the wing's for an aspect ratio of 6.46
         with the tailplane's added; zero lift at -2 degrees for the wing's
         camber. The peaks are set to the stalling speeds of the Pilot's
         Notes (para. 43), power off at 18,000 lb: 105 knots clean and
         95-100 knots with the flaps and wheels down, as the figures' stall
         flight measures them. Those are indicated speeds; HJ679's measured
         position error (de Havilland, 20 September 1943, fig. 3) is within
         a few mph of nothing at the lowest speeds it reaches, so they are
         taken as calibrated. -->
    <function name="aero/function/lift-coefficient-wing">
      <table>
        <independentVar lookup="row">aero/alpha-deg</independentVar>
        <independentVar lookup="column">fcs/flap-pos-deg</independentVar>
        <tableData>
                  0      15      25      45
          -10  -0.69   -0.52   -0.40   -0.20
           -2   0.00    0.15    0.25    0.40
            0   0.17    0.32    0.42    0.57
            8   0.85    0.99    1.08    1.19
           10   0.97    1.11    1.18    1.28
           12   0.99    1.17    1.23    1.30
           14   0.95    1.10    1.14    1.18
           16   0.88    0.98    1.02    1.05
           20   0.85    0.88    0.90    0.92
           25   0.80    0.82    0.84    0.86
        </tableData>
      </table>
    </function>

"""

# Lift, with the slipstreams' lift on the wing.
LIFT = """\
    <axis name="LIFT">
      <function name="aero/force/lift-alpha">
        <description>Lift with angle of attack and flap</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/function/ground-effect-lift</property>
          <property>aero/function/lift-coefficient-wing</property>
        </product>
      </function>
      <function name="aero/force/lift-slipstream">
        <description>The propellers' slipstreams over the wing behind them.
          Each disc spans from 2 ft to 14 ft out from the centre line, so the
          slipstreams wash about 90 sq ft of wing each, flaps included, where
          the air moves at the aircraft's speed plus 1.5 times JSBSim's
          induced velocity (the slipstream half developed by the wing). The
          extra dynamic pressure lifts that wing at the wing's own lift
          coefficient. At take-off power it adds a sixth to the lift; with
          the engines throttled back the windmilling propellers take a
          little away; in cruise it is nothing. The Pilot's Notes (para. 43)
          stall "power on under typical approach conditions" at 90-95
          knots, against 95-100 power off.</description>
        <product>
          <value>0.5</value>
          <property>atmosphere/rho-slugs_ft3</property>
          <value>90</value>
          <property>aero/function/lift-coefficient-wing</property>
          <sum>
            <difference>
              <pow>
                <sum>
                  <property>velocities/u-aero-fps</property>
                  <product>
                    <value>1.5</value>
                    <property>propulsion/engine[0]/prop-induced-velocity_fps</property>
                  </product>
                </sum>
                <value>2</value>
              </pow>
              <pow>
                <property>velocities/u-aero-fps</property>
                <value>2</value>
              </pow>
            </difference>
            <difference>
              <pow>
                <sum>
                  <property>velocities/u-aero-fps</property>
                  <product>
                    <value>1.5</value>
                    <property>propulsion/engine[1]/prop-induced-velocity_fps</property>
                  </product>
                </sum>
                <value>2</value>
              </pow>
              <pow>
                <property>velocities/u-aero-fps</property>
                <value>2</value>
              </pow>
            </difference>
          </sum>
        </product>
      </function>
      <function name="aero/force/lift-elevator">
        <description>Lift from the elevator: the tailplane's share, 87.4 of
          454 sq ft, with the elevator's effectiveness, in the tailplane's
          dynamic pressure</description>
        <product>
          <property>aero/function/qbar-tail-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>fcs/elevator-pos-rad</property>
          <value>0.31</value>
        </product>
      </function>
      <function name="aero/force/lift-pitch-rate">
        <description>Lift from pitch rate, the tailplane's</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/ci2vel</property>
          <property>velocities/q-aero-rad_sec</property>
          <value>3.8</value>
        </product>
      </function>
    </axis>

"""

# Drag: zero-lift and its Mach rise set to HX809's level speeds, radiator
# shutters to HJ679's climbs, the fin's induced drag, a stopped propeller's.
DRAG = """\
    <axis name="DRAG">
      <!-- At zero lift: set to HX809's level speeds with the engines'
           power as Rolls-Royce gave it. Drop tanks on. -->
      <function name="aero/force/drag-zero-lift">
        <description>Drag at zero lift</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <value>0.0210</value>
        </product>
      </function>
      <function name="aero/force/drag-induced">
        <description>Induced drag: lift squared over pi, the aspect ratio of
          6.46 and an efficiency of 0.8; less near the ground</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/cl-squared</property>
          <property>aero/function/ground-effect-induced-drag</property>
          <value>0.0616</value>
        </product>
      </function>
      <function name="aero/force/drag-stall">
        <description>The separated flow past the stall</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <table>
            <independentVar>aero/alpha-deg</independentVar>
            <tableData>
              12   0.00
              14   0.03
              16   0.08
              20   0.18
              25   0.30
            </tableData>
          </table>
        </product>
      </function>
      <function name="aero/force/drag-flaps">
        <description>Flaps</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <table>
            <independentVar>fcs/flap-pos-deg</independentVar>
            <tableData>
               0  0.000
              15  0.010
              25  0.022
              45  0.065
            </tableData>
          </table>
        </product>
      </function>
      <function name="aero/force/drag-gear">
        <description>The undercarriage, down: "its high drag" (Pilot's Notes
          para. 57)</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>gear/gear-pos-norm</property>
          <value>0.020</value>
        </product>
      </function>
      <function name="aero/force/drag-radiators">
        <description>The radiator shutters, open. Set between HJ679's two
          climb rates, in MS gear at 10,400 ft and FS gear at 17,000 ft, both
          flown with them "fully open"; the level speeds were flown with them
          shut.</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <sum>
            <property>fcs/cooling-flaps-cmd-norm[0]</property>
            <property>fcs/cooling-flaps-cmd-norm[1]</property>
          </sum>
          <value>0.0019</value>
        </product>
      </function>
      <function name="aero/force/drag-rudder">
        <description>The fin's induced drag with the rudder over, as it is
          holding a failed engine: its side force squared over its span
          (about 8 ft) and the dynamic pressure</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>fcs/rudder-pos-rad</property>
          <property>fcs/rudder-pos-rad</property>
          <value>0.03</value>
        </product>
      </function>
      <function name="aero/force/drag-stopped-propellers">
        <description>A stopped propeller - feathered, or seized - edge on to
          the air: JSBSim gives it none. About 0.4 sq ft of flat plate
          each.</description>
        <product>
          <property>aero/qbar-psf</property>
          <value>0.4</value>
          <sum>
            <lt>
              <property>propulsion/engine[0]/propeller-rpm</property>
              <value>50</value>
            </lt>
            <lt>
              <property>propulsion/engine[1]/propeller-rpm</property>
              <value>50</value>
            </lt>
          </sum>
        </product>
      </function>
      <function name="aero/force/drag-sideslip">
        <description>Sideslip</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/mag-beta-rad</property>
          <value>0.15</value>
        </product>
      </function>
      <function name="aero/force/drag-mach">
        <description>Compressibility. The wing is thick, and the drag begins
          to rise gently from Mach 0.45 - set so that the trials' speeds at
          height and at sea level agree - and steeply towards the diving
          limits</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <table>
            <independentVar>velocities/mach</independentVar>
            <tableData>
              0.45  0.0000
              0.50  0.0006
              0.55  0.0012
              0.60  0.0020
              0.68  0.0040
              0.72  0.0080
              0.76  0.0200
              0.80  0.0450
              0.90  0.1000
            </tableData>
          </table>
        </product>
      </function>
    </axis>

"""

# Side force.
SIDE = """\
    <!-- The lateral and directional derivatives are estimated from the
         geometry, with the fin taken at 45 sq ft on a 23 ft arm, except the
         rudder's power, which the single-engine safety speed sets. Signs as
         JSBSim's: positive rudder is trailing edge left. -->
    <axis name="SIDE">
      <function name="aero/force/side-sideslip">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/beta-rad</property>
          <value>-0.60</value>
        </product>
      </function>
      <function name="aero/force/side-rudder">
        <description>The fin's side force from the rudder: its yawing moment
          over the fin's arm, 23 ft of the 54.17 ft span</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>fcs/rudder-pos-rad</property>
          <value>0.0443</value>
        </product>
      </function>
    </axis>

"""

# Roll.
ROLL = """\
    <axis name="ROLL">
      <function name="aero/moment/roll-sideslip">
        <description>Dihedral effect</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>aero/beta-rad</property>
          <value>-0.08</value>
        </product>
      </function>
      <function name="aero/moment/roll-damping">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>aero/bi2vel</property>
          <property>velocities/p-aero-rad_sec</property>
          <value>-0.50</value>
        </product>
      </function>
      <function name="aero/moment/roll-yaw-rate">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>aero/bi2vel</property>
          <property>velocities/r-aero-rad_sec</property>
          <value>0.12</value>
        </product>
      </function>
      <function name="aero/moment/roll-aileron">
        <description>"The controls are light and effective" (Pilot's Notes
          para. 42): about 50 degrees a second at 250 mph</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>fcs/left-aileron-pos-rad</property>
          <value>0.12</value>
        </product>
      </function>
      <function name="aero/moment/roll-rudder">
        <description>The same side force, 4 ft above the centre of
          gravity</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>fcs/rudder-pos-rad</property>
          <value>0.0032</value>
        </product>
      </function>
    </axis>

"""

# Pitch, with the elevator's and flaps' moments and the nose-down break
# past the stall.
PITCH = """\
    <axis name="PITCH">
      <!-- A small static margin, about 4% of the chord: "fore-and-aft
           stability is satisfactory on the climb and neutral on the glide.
           At low speeds in level flight the aircraft is slightly unstable"
           (Pilot's Notes, 1944, para. 51), and R&M 2597 found Mosquito
           DZ294 behind its manoeuvre point, stick fixed, at its aft centre
           of gravity, 0.335 of the chord. -->
      <function name="aero/moment/pitch-zero">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property>
          <value>0.02</value>
        </product>
      </function>
      <function name="aero/moment/pitch-alpha">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property>
          <property>aero/alpha-rad</property>
          <value>-0.20</value>
        </product>
      </function>
      <function name="aero/moment/pitch-stall">
        <description>Past the stall the wing's flow separates from the root
          outward and the tailplane loses its downwash: the nose drops. "At
          the stall the aircraft pitches, the A.S.I. fluctuates and the nose
          drops gently" (Pilot's Notes para. 43).</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property>
          <table>
            <independentVar>aero/alpha-deg</independentVar>
            <tableData>
              12    0.00
              14   -0.04
              16   -0.10
              20   -0.22
              30   -0.40
              50   -0.60
            </tableData>
          </table>
        </product>
      </function>
      <function name="aero/moment/pitch-elevator">
        <description>The tailplane's volume, 0.51, with the elevator's
          effectiveness, in the tailplane's dynamic pressure</description>
        <product>
          <property>aero/function/qbar-tail-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property>
          <property>fcs/elevator-pos-rad</property>
          <value>-0.85</value>
        </product>
      </function>
      <function name="aero/moment/pitch-damping">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property>
          <property>aero/ci2vel</property>
          <property>velocities/q-aero-rad_sec</property>
          <value>-10.0</value>
        </product>
      </function>
      <function name="aero/moment/pitch-alpha-rate">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property>
          <property>aero/ci2vel</property>
          <property>aero/alphadot-rad_sec</property>
          <value>-4.3</value>
        </product>
      </function>
      <function name="aero/moment/pitch-flaps">
        <description>"Flaps down: nose up" (Pilot's Notes para. 42)</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property>
          <property>fcs/flap-pos-deg</property>
          <value>0.0015</value>
        </product>
      </function>
      <function name="aero/moment/pitch-radiators">
        <description>"Radiator shutters open: nose up" (Pilot's Notes
          para. 42)</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property>
          <sum>
            <property>fcs/cooling-flaps-cmd-norm[0]</property>
            <property>fcs/cooling-flaps-cmd-norm[1]</property>
          </sum>
          <value>0.002</value>
        </product>
      </function>
      <function name="aero/moment/pitch-gear">
        <description>"Undercarriage down: nose down"</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property>
          <property>gear/gear-pos-norm</property>
          <value>-0.012</value>
        </product>
      </function>
    </axis>

"""

# Yaw: the slipstreams' push on the fin set to the swing on take-off, the
# rudder's power to the single-engine safety speeds.
YAW = """\
    <axis name="YAW">
      <function name="aero/moment/yaw-sideslip">
        <description>Weathercock stability: "the directional and lateral
          stability is satisfactory"</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>aero/beta-rad</property>
          <value>0.09</value>
        </product>
      </function>
      <function name="aero/moment/yaw-damping">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>aero/bi2vel</property>
          <property>velocities/r-aero-rad_sec</property>
          <value>-0.11</value>
        </product>
      </function>
      <function name="aero/moment/yaw-roll-rate">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>aero/bi2vel</property>
          <property>velocities/p-aero-rad_sec</property>
          <value>-0.03</value>
        </product>
      </function>
      <function name="aero/moment/yaw-aileron">
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>fcs/left-aileron-pos-rad</property>
          <value>-0.008</value>
        </product>
      </function>
      <function name="aero/moment/yaw-slipstream">
        <description>The propellers' slipstream on the fin. Both turn
          clockwise seen from behind, and their wakes turn with them: above
          the propellers' axes the air moves to starboard, so it strikes the
          fin from port and pushes the tail to starboard, the nose to port.
          JSBSim models the propellers' torque, gyroscopic moments and
          P-factor but not this, which on the ground with the tail down is
          what outweighs the torque's own tendency to turn the nose to
          starboard. In proportion to the thrust, and to each propeller's
          sense (JSBSim's: 1 clockwise from behind), set so that the take-off
          swing is to port and as slight as the Pilot's Notes say (para. 49,
          1944).</description>
        <product>
          <sum>
            <product>
              <property>propulsion/engine[0]/thrust-lbs</property>
              <property>propulsion/engine[0]/propeller-sense</property>
            </product>
            <product>
              <property>propulsion/engine[1]/thrust-lbs</property>
              <property>propulsion/engine[1]/propeller-sense</property>
            </product>
          </sum>
          <value>-0.38</value>
        </product>
      </function>
      <function name="aero/moment/yaw-rudder">
        <description>The rudder's power: set so that, with one engine failed
          and its propeller windmilling, the speeds at which holding the
          aircraft straight takes full rudder fall as near as they can to
          both the Pilot's Notes' safety speeds, 170 knots at +18 boost and
          155 at +9 - the model has them further apart, as the live engine's
          thrust at the two boosts puts them, and meets each within 6
          knots</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property>
          <property>fcs/rudder-pos-rad</property>
          <value>-0.0188</value>
        </product>
      </function>
    </axis>
"""

TAIL = """\
  </aerodynamics>
</fdm_config>
"""


# ---------------------------------------------------------------------------
# The engine as written on 2026-09-19: Rolls-Royce's curve, the Pilot's
# Notes' limits, HX809's full-throttle heights.
# ---------------------------------------------------------------------------

MERLIN_25 = """\
<?xml version="1.0"?>
<!--
  Rolls-Royce Merlin 25, as fitted to the de Havilland Mosquito FB Mk VI.

  Written for glideslope from the sources below; not derived from any JSBSim
  engine file. docs/ASSETS.md records the sources, and the Mosquito's figures
  (assets/figures/mosquito-fb6.xml) are the checks it is held to.

  The engine
    Twelve cylinders, 5.4 in bore and 6.0 in stroke: 1,649 cubic inches. A
    single-stage supercharger with two speeds, moderate (MS, low gear) and full
    (FS, high gear), which the Mosquito's aneroid changes automatically at
    about 7,000 ft - the aircraft file does that, through boost-speed, so this
    file leaves the change to it (boostmanual). The propeller reduction gear
    (0.42:1) is the propeller file's.

  Its limits (A.P. 2019E, Pilot's Notes, 1950 edition, para. 51)
    take-off, and operational necessity for 5 minutes   3,000 rpm   +18 lb/sq in
    climbing, 1 hour                                     2,850 rpm    +9
    maximum continuous                                   2,650 rpm    +7
  +18 is the most the automatic boost control gives, so it is the rated boost
  here: the throttle sets the boost below it, and the boost control holds it
  there. (HX809 was cleared for +25 on 150-grade fuel for its trial; the
  Pilot's Notes give +18, and so does this file.)

  How much the supercharger gives: the full-throttle heights
    JSBSim's supercharger multiplies the pressure at the intake by a fixed
    ratio for each gear, in proportion to rpm, set by ratedaltitude: at
    3,000 rpm and no forward speed, the boost reaches +18 at full throttle at
    the rated altitude. In flight the intake's ram pressure raises the full-
    throttle height; ram-air-factor 0.8 is Rolls-Royce's own assumption for
    its curves ("ram 80% press."). The rated heights were chosen so that +18
    is reached at full throttle where Mosquito FB VI HX809 reached it in
    level flight (A&AEE/767,e, 12th part, 1 February 1944):
      MS gear   5,100 ft at 353 mph    the model: about 5,200 ft
      FS gear  12,500 ft at 363 mph    the model: about 12,700 ft
    and at 400 mph where Rolls-Royce's curves put it: 6,300 ft and 13,600
    ft (the model: 5,900 and 13,600).

  How much power: maxhp, the charge and the supercharger's drive
    JSBSim's piston engine burns fuel in proportion to the air it breathes and
    makes power from the fuel. maxhp calibrates that: its value gives the
    Rolls-Royce curve (Merlin 24, 25, 26, 27, 224, 225 altitude performance,
    AVIA 6/5817 fig. 1, 3,000 rpm and 400 mph of ram) its 1,540 bhp at +18 at
    sea level. The aircraft file adds what JSBSim leaves out:
      the charge's heating, by the ram and by the impeller (90 K at full rpm
        in MS gear, 121 K in FS), which makes the power at +18 rise with
        height as the curve does, to about 1,600 at the MS full-throttle
        height, and fall above the full-throttle height as the curve does;
      the power the impeller takes in FS gear beyond MS gear, 77 hp at
        3,000 rpm, which with the hotter charge gives the curve's 1,450 bhp
        at +18 at 9,500 ft in FS gear;
      a stopped engine's own friction, which JSBSim charges only a running
        one, so that a windmilling propeller drags as it should.
    The model against the curve at +18 and 3,000 rpm, 400 mph: MS 1,542 bhp
    at sea level, 1,613 at 6,000 ft; FS 1,446 at 10,000 ft, 1,469 at
    12,000, 1,378 at 16,000 and 1,194 at 20,000 (the curve: 1,540, 1,600;
    1,450, 1,468, 1,376 and 1,164).

  Mixture
    The Merlin's carburettor sets its own mixture (an automatic mixture
    control), so the aircraft file sets mixture from the air's pressure and
    the pilot's lever only cuts the engine off; above +9 it enriches it, as
    the carburettor did against detonation.

    JSBSim makes power in proportion to the fuel burnt, times an efficiency
    that depends on the fuel-air ratio (the MIXTURE table). Its own table
    makes the most power at a ratio of about 0.10, so a richer mixture would
    make more; an engine makes its best power nearer 0.08, burning all the
    air, and fuel beyond that only cools it, costing a little power - the
    familiar shape of an engine's power against its mixture. The table here
    is such a curve (1.0 of best power at 0.08, 0.94 at 0.10, 0.89 at 0.11),
    divided by the fuel-air ratio and scaled to 1 at 0.08; its values are
    this file's estimate. With it +9 gives about 1,230 bhp at 2,850 rpm at
    the MS full-throttle height, near the climbing power of the same engine
    family (the Packard Merlin 28: 1,240 hp at 3,000 rpm and +9 at 11,500 ft,
    Bridgeman 1998 as Wikipedia's list of Merlin variants gives it), where
    JSBSim's own table left it some 10% short.

  Idle
    minmp is the manifold pressure at idle rpm with the throttle closed: at
    10 inHg the engine idles at about 800 rpm against the propeller at its
    fine-pitch stop. Lower, it ran down and stopped.
-->
<piston_engine name="Merlin 25">
  <displacement unit="IN3"> 1649 </displacement>
  <bore unit="IN"> 5.4 </bore>
  <stroke unit="IN"> 6.0 </stroke>
  <cylinders> 12 </cylinders>
  <compression-ratio> 6.0 </compression-ratio>
  <cylinder-head-mass unit="LBS"> 40 </cylinder-head-mass>
  <cycles> 4 </cycles>

  <maxhp> 633 </maxhp>
  <maxrpm> 3000 </maxrpm>
  <idlerpm> 650 </idlerpm>
  <minmp unit="INHG"> 10.0 </minmp>
  <maxmp unit="INHG"> 29.92 </maxmp>
  <maxthrottle> 1.0 </maxthrottle>
  <minthrottle> 0.05 </minthrottle>
  <sparkfaildrop> 0.1 </sparkfaildrop>

  <ram-air-factor> 0.8 </ram-air-factor>

  <numboostspeeds> 2 </numboostspeeds>
  <boostmanual> 1 </boostmanual>
  <boostoverride> 0 </boostoverride>

  <table name="MIXTURE" type="internal">
    <tableData>
      0.050   0.000
      0.055   0.727
      0.060   1.067
      0.065   1.108
      0.070   1.097
      0.075   1.056
      0.080   1.000
      0.085   0.936
      0.090   0.871
      0.100   0.752
      0.110   0.647
      0.120   0.553
      0.125   0.000
    </tableData>
  </table>

  <ratedboost1 unit="PSI"> 18 </ratedboost1>
  <ratedpower1 unit="HP"> 1540 </ratedpower1>
  <ratedrpm1> 3000 </ratedrpm1>
  <ratedaltitude1 unit="FT"> 1800 </ratedaltitude1>

  <ratedboost2 unit="PSI"> 18 </ratedboost2>
  <ratedpower2 unit="HP"> 1450 </ratedpower2>
  <ratedrpm2> 3000 </ratedrpm2>
  <ratedaltitude2 unit="FT"> 9260 </ratedaltitude2>
</piston_engine>
"""


SLIPSTREAM_SIDES = (("port", 0), ("starboard", 1))

SLIPSTREAM_ANGLE = """\
    <!-- The angle the air in the {side} propeller's slipstream meets the
         wing at: its speed along her x axis is the aircraft's plus 1.5
         times the induced velocity, as in lift-slipstream; across it, w.
         Standing, it is near nothing whichever way she creeps. -->
    <function name="aero/function/slipstream-alpha-deg-{side}">
      <product>
        <value>57.29577951308232</value>
        <atan2>
          <property>velocities/w-aero-fps</property>
          <sum>
            <property>velocities/u-aero-fps</property>
            <product>
              <value>1.5</value>
              <property>propulsion/engine[{engine}]/prop-induced-velocity_fps</property>
            </product>
          </sum>
        </atan2>
      </product>
    </function>
    <function name="aero/function/lift-coefficient-slipstream-{side}">
      <table>
        <independentVar lookup="row">aero/function/slipstream-alpha-deg-{side}</independentVar>
        <independentVar lookup="column">fcs/flap-pos-deg</independentVar>
        <tableData>{table}</tableData>
      </table>
    </function>

"""

SLIPSTREAM_SIDE_LIFT = """\
            <product>
              <property>aero/function/lift-coefficient-slipstream-{side}</property>
              <difference>
                <pow>
                  <sum>
                    <property>velocities/u-aero-fps</property>
                    <product>
                      <value>1.5</value>
                      <property>propulsion/engine[{engine}]/prop-induced-velocity_fps</property>
                    </product>
                  </sum>
                  <value>2</value>
                </pow>
                <pow>
                  <property>velocities/u-aero-fps</property>
                  <value>2</value>
                </pow>
              </difference>
            </product>
"""


def with_slipstream_lift_at_its_own_angle(text):
    """`text` with lift-slipstream's lift coefficient read, for each
    slipstream, at the angle the air in it meets the wing, atan2(w, u + 1.5
    vi), on the same table as her wing's - not at her angle of attack,
    atan2(w, u). The docstring's entry of 2026-10-20 says why. Fails if the
    table or the term is not as written."""
    wing = re.search(r'<function name="aero/function/lift-coefficient-wing">.*?'
                     r'<tableData>(.*?)</tableData>', text, re.S)
    if wing is None:
        raise SystemExit("make_mosquito_fb6: no lift-coefficient-wing table - has the model changed?")
    angles = "".join(SLIPSTREAM_ANGLE.format(side=s, engine=e, table=wing.group(1))
                     for s, e in SLIPSTREAM_SIDES)
    head = '    <axis name="LIFT">\n'
    if text.count(head) != 1:
        raise SystemExit("make_mosquito_fb6: not one LIFT axis - has the model changed?")
    text = text.replace(head, angles + head)
    term = re.compile(r'(<function name="aero/force/lift-slipstream">.*?</description>\n)'
                      r'(.*?)(      </function>\n)', re.S)
    found = term.findall(text)
    if len(found) != 1 or "aero/function/lift-coefficient-wing" not in found[0][1]:
        raise SystemExit("make_mosquito_fb6: lift-slipstream is not as written - has the model changed?")
    body = ("        <product>\n"
            "          <value>0.5</value>\n"
            "          <property>atmosphere/rho-slugs_ft3</property>\n"
            "          <value>90</value>\n"
            "          <sum>\n"
            + "".join(SLIPSTREAM_SIDE_LIFT.format(side=s, engine=e) for s, e in SLIPSTREAM_SIDES)
            + "          </sum>\n"
            "        </product>\n")
    return term.sub(lambda m: m.group(1) + body + m.group(3), text)


def with_elevator_lift_on_qbar_uw(text):
    """`text` with her elevator's lift on her tailplane's pressure made from
    the dynamic pressure without its sideways part (2026-10-23).

    Her lift-elevator is on aero/function/qbar-tail-psf: 0.6 of the whole
    dynamic pressure, which counts the crosswind, and 0.4 of her
    slipstreams', which reads u alone. Standing in 15 kt straight across with
    her stick back the first part lifted her by the crosswind, as every
    model's elevator lift did (alpha_rate.with_unkeyed_terms_on_qbar_uw says
    why it should not: her tailplane is not swept, the case of JSBSim issue
    #89). The same pressure is her elevator's pitching moment's, which is
    not this change's, so her lift-elevator alone is put on a copy of it,
    aero/function/qbarUW-tail-psf, its 0.6 on aero/qbarUW-psf.
    """
    m = re.search(r'    <function name="aero/function/qbar-tail-psf">.*?</function>\n', text, re.S)
    if not m or text.count('<function name="aero/function/qbar-tail-psf">') != 1:
        raise SystemExit("make_mosquito_fb6: her tailplane's pressure is not where it was")
    whole = m.group(0)
    uw = whole.replace("aero/function/qbar-tail-psf", "aero/function/qbarUW-tail-psf")
    uw = uw.replace("<property>aero/qbar-psf</property>", "<property>aero/qbarUW-psf</property>")
    if uw.count("aero/qbarUW-psf") != 1:
        raise SystemExit("make_mosquito_fb6: her tailplane's pressure has changed")
    note = ("\n    <!-- The same pressure with its whole-air part leaving out the sideways\n"
            "         air, for her elevator's lift (tools/make_mosquito_fb6.py,\n"
            "         with_elevator_lift_on_qbar_uw). -->\n")
    text = text[:m.end()] + note + uw + text[m.end():]
    f = re.search(r'<function name="aero/force/lift-elevator">.*?</function>', text, re.S)
    if not f or f.group(0).count("<property>aero/function/qbar-tail-psf</property>") != 1:
        raise SystemExit("make_mosquito_fb6: her elevator's lift is not where it was")
    moved = f.group(0).replace("<property>aero/function/qbar-tail-psf</property>",
                               "<property>aero/function/qbarUW-tail-psf</property>")
    return text[:f.start()] + moved + text[f.end():]


def airframe():
    """The airframe, assets/jsbsim/aircraft/mosquito-fb6/mosquito-fb6.xml."""
    text = "".join([HEADER, METRICS, MASS_BALANCE, GROUND_REACTIONS, PROPULSION,
                    FLIGHT_CONTROL, AERODYNAMICS_HEAD, LIFT, DRAG, SIDE, ROLL, PITCH,
                    YAW, TAIL])
    text = with_slipstream_lift_at_its_own_angle(text)
    return with_elevator_lift_on_qbar_uw(alpha_rate.with_unkeyed_terms_on_qbar_uw(
        alpha_rate.with_alpha_terms_on_qbar_uw(
            alpha_rate.with_qbar_uw(text, "make_mosquito_fb6"), "make_mosquito_fb6",
            {"PITCH": ["aero/moment/pitch-alpha", "aero/moment/pitch-stall"],
             "LIFT": ["aero/force/lift-alpha"],
             "DRAG": ["aero/force/drag-stall"]}), "make_mosquito_fb6",
        {"LIFT": ["aero/force/lift-pitch-rate"]}))


def outputs():
    return {AIRFRAME: airframe(), ENGINE: MERLIN_25}


def main():
    check = "--check" in sys.argv[1:]
    stale = []
    for path, text in outputs().items():
        if check:
            if not path.exists() or path.read_text() != text:
                stale.append(path.relative_to(ROOT))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            print(f"wrote {path.relative_to(ROOT)}")
    if stale:
        print("assets/jsbsim/ is not what tools/make_mosquito_fb6.py makes:", file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print("run: python3 tools/make_mosquito_fb6.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
