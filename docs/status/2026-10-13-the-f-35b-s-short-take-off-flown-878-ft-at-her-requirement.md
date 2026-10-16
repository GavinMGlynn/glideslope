### The F-35B's short take-off flown: 878 ft at her requirement's weight, against 600 required, 2026-10-13 — item still open

*Superseded in part by the entry above: her take-off attitude now follows her tail-strike attitude, and she leaves in 697.5 ft.*

**What is missing, first**: she does not take off in 600 ft. Swept over
where the nozzle and fan swing (50 to 90 KCAS), how far (46 to 90 degrees)
and the attitude (10 to 18 degrees), the shortest at 15 degrees is 877.6 ft
and at 18 degrees 756 ft. Her model has no flaperons drooped for a short
take-off (she has no flaps at all), no thrust split between fan and nozzle
to pitch her (only the fan's 10 per cent modulation and the stabilator; she
takes about 400 ft to rotate), and no published attitude her tail strikes
at; nothing models the fan's own induced lift on the roll, ground effect on
the jets, or the fan intake's ram drag. The lift system is still not in a
pilot's controls, and no AI plan hovers her or takes her off short.

**Source**: the Department of Defense's F-35 Selected Acquisition Report of
December 2014 (pinned in `docs/ASSETS.md`, read through the Internet
Archive): the STOVL key performance parameter, "With two 1000# JDAMs and two
internal AIM-120s, full expendables, execute a 600 foot ... STO from LHA,
LHD, and aircraft carriers (sea level, tropical day, 10 kts operational WOD)
and with a combat radius of 450 nm", its current estimate 569 ft "with 2
JDAM (internal), 2 AIM-120 (internal), fuel to fly 456nm"; and the vertical
landing "with two 1000# JDAMs and two internal AIM-120s, full expendables,
and fuel to fly the STOVL Recovery profile". The tropical day is MIL-STD-210A's
tropical atmosphere, 549.5 R at sea level against the standard's 518.7, as
Public Domain Aeronautical Software tabulates it (pinned): 17.1 C warmer.

**The model**: the fan's vane box now vectors its thrust, from 41.75 to 104
degrees from straight aft at 40 degrees a second (Wiegand et al., AIAA
2018-3368), by the direction of its JSBSim external force; the hover holds
it at 90. `LiftSystem` gains `fan_deg`. Her conventional figures are again
unchanged to the CLI's last digit, and **the selftest hash does not move**,
`182dd6c996e0ee4c`.

**The loadings**: the short take-off at 48,300 lb - empty, pilot, two 1,000
lb JDAMs and two AIM-120s (2,670 lb) and all 13,100 lb of internal fuel, the
product card's radius on internal fuel being the requirement's 450 nm. The
vertical landing's 37,530 lb now carries the requirement's stores, 2,670 lb,
and 2,330 lb of fuel - the recovery profile's fuel is not published, so the
weight stays this project's; the same total, so the hover and landing fly as
before (touching at 179.56 ft/min, was 179.57).

**The flight** (`short_takeoff`, `src/sim/figures.cpp`): into 10 kt, 17.1 C
warmer than standard; converted at idle on the brakes with nozzle and fan at
42 degrees; brakes off and full STOVL power together; at 60 KCAS both swung
to 52 degrees and the nose raised to 15 on the stabilator and the fan's
share; the distance from the brakes' release to the wheels leaving, and she
must climb on to 50 ft without touching again.

**Verification**: `the_f35b_takes_off_short_in_the_distance_her_own_model_gave`,
a measured figure, 877.6 ft within 5 per cent, so that the model still does
what it did; seen to fail with the fan's vane box held straight down in the
built model (she touched the runway again), reverted. 126 figures (was
125); 51 loadings flown by `every_landplane_takes_off_at_every_loading...`
(was 50). Her 45 tests - figures, take-off, landing, glide, orbit, the
committed model - re-run green on linux-release.
