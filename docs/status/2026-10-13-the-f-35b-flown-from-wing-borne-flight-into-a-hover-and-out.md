### The F-35B flown from wing-borne flight into a hover and out again: the transition law, 2026-10-13 — item still open

**What is still missing, first**: **no pilot's controls and no AI plan
reach her lift system**, so only tests fly the transition. The lift system
is not in `Controls`, so a client neither sends nor predicts it; the
`Controller` gives `Controls` alone, and no plan leg asks for a hover.
Wiring the law to the AI changes the controller, every caller of it
(server, client, replay) and the plan format. That is not small, and it is
the item's remaining part. The transition is flown in calm air only, at 150
and 50 ft, from and to 200 KCAS; no wind or turbulence is claimed. **Her
fuel is frozen** in the four figures, at each loading's weight. Flown once
with it burning (the figures' `fuel_frozen` taken out of the built data's
copy, then restored), she loses 3.43, 3.48, 0.37 and 0.15 ft, against
3.43, 3.48, 0.35 and 0.16 frozen. **Wing-borne flight slower than her
conversion speed is refused**: asked for 150 KCAS, she converts back at 200
and is held there. Slowing her wing-borne, the autopilot held her nose at
its 15-degree limit with the throttle closed, and she sank from 155 ft into
the ground. A 200 KCAS level flight already needs 11.6 degrees.

**Source** (pinned in `docs/ASSETS.md`): Lockheed Martin's Code One, "F-35
Lightning II Flight Test Update 1" (18 March 2010). It is the only public
account found that gives the speeds and the sequence:

- On 17 March 2010, her first hover, BF-1's lead STOVL pilot "initiates
  conversion to STOVL mode at 200 knots. He then slows the aircraft to
  sixty knots and flies a decelerating approach to a zero airspeed hover at
  150 feet above the runway".
- Her first vertical landing the next day converted "at 200 knots".
- On 23 January she flew "airborne conversions at 200 knots and 5,000
  feet".
- On 7 January, her first STOVL flight, the fan was engaged "at 210 knots,
  then slows to 180 knots with the system engaged before accelerating and
  converting back to conventional flight mode".

The speed she converts back at is not given and is taken as the same 200.
No least conversion height is published. Lockheed's own release of 18 March
(already pinned) gives the 150 ft hover but no speeds. The AIAA papers on
her control laws named in the entry below are still unread (paywalled), and
nothing here rests on them.

**Her model** (`tools/make_f35b.py`): the law reads everything it needs of
her from it.

- **Published**: `fcs/stovl-convert-kcas`, 200.
- **Her own travel**: the nozzle's and the fan's vane box's travel
  (`fcs/nozzle-max-deg`, `fcs/fan-min-deg`, `fcs/fan-max-deg`), written
  from the same constants as her actuators.
- **Estimated, and said so in the script**:
  - how fast she slows and gathers speed jet-borne, 2 and 3 kt/s;
  - the attitude she gathers speed toward, 4 degrees;
  - her least conversion height, `fcs/stovl-convert-least-ft`, 100 ft. The
    script's justification: ten times the most a transition may sink (10
    ft) and five times what the conversion balloons her (about 20 ft).
- **Reckoned**: her wing's lift slope at Mach 0.3, 2.9954 a radian, the
  slope her lift table is written with.

**The attitude she converts back to is reckoned, not typed**: her weight
over the dynamic pressure of her conversion speed, her wing's area and that
slope. This gives 11.6 degrees at 37,530 lb, against the 11.7 she flies
trimmed there. Nothing else in her model changes, and her conventional
figures are unchanged to the CLI's last digit.

**The law** (`src/sim/transition.hpp`) is asked for a hover, or for
wing-borne flight at a speed, both at a height above the ground. It has
four stages:

- **wing-borne**: the autopilot on the height and the speed. Asked for a
  hover, she is slowed to her conversion speed at her least conversion
  height or higher.
- **converting**: the fan's doors open or close, **which way decided by
  the ask alone**.
  - Opening from wing-borne flight, the autopilot still flies her, the
    nozzle aft, with the fan's share holding the attitude the autopilot
    asks. The fan is ahead of her centre of gravity, and its thrust follows
    the autopilot's throttle.
  - Closing from jet-borne flight, the jet-borne law flies her, the thrust
    all aft and the gear up. Her attitude is raised from where it was
    toward her wing-borne attitude as the fan's lift goes. The autopilot
    takes her once the doors are shut.
- **jet-borne**, her own law:
  - the height on the throttle (the hover law's gains);
  - the speed along her heading on the thrust's angle: the nozzle and the
    fan's vane box are swung to one angle, each within its travel, at 2
    degrees a knot too fast and 0.3 a knot-second;
  - the speed asked moving at her rates toward a hover, or up to 20 kt
    past her conversion speed;
  - the attitude on the stabilator and the fan's share together: slowing,
    from the attitude she had when asked down to level as the speed goes;
    gathering speed, toward 4 degrees by her conversion speed;
  - the wings on the ailerons and the posts, the heading on the nozzle's
    swivel, and the ball on the rudder.
- **hovering**: the hover law, taken over in the air with nothing stepped.
  A new `Hover` constructor seeds its throttle, fan and post integrals from
  the command it is handed, and holds the point, heading and height where
  she is. It takes her once the speed asked has reached nothing, her ground
  speed is under 3 kt and the nozzle is down.

**Reversals**: asked the other way part-way, she turns back from where she
is. Jet-borne, or with the jet-borne law flying a conversion, every
integral and the speed and attitude it reckons from are taken afresh, so
nothing steps.

**Her least conversion height is kept both ways.** Asked for a hover lower
than it, she climbs to it to convert, and lets down jet-borne after.
Gathering speed lower than it, she climbs to it to convert back, and lets
down wing-borne after.

**Why each choice, from what was flown**:

- **Gathering attitude**: gathering speed at the 11.5 degrees she is
  trimmed at, the wing carried her and the throttle that holds her height
  fell to 0.39. With the thrust all aft she stuck at 166 KCAS; at 8 degrees
  she stuck at 195 KCAS. At 4 she reaches 200.
- **Who flies the conversion back**: handed to the autopilot, its altitude
  loop raised her nose from 4 to 12 degrees too slowly as the fan's lift
  went, and she lost 46.0 ft. Handed to it earlier, as the thrust reached
  aft at 166 KCAS, it asked her past 15 degrees.

**Verification**, linux-release; every flight is held within 15 degrees of
pitch and 10 of bank, with her wheels off the ground.

- **Four figures** in `assets/figures/f35b.xml`, each a test. Each holds
  the height lost within **10 ft** and the height risen within **25 ft**
  (the conversion balloons her about 20); the bounds are this project's.
  - `the_f35b_slows_from_200_kt_level_to_a_hover_at_150_ft_within_10_ft_and_her_attitudes`
    and `..._slows_to_a_hover_at_her_combat_weight_...`: begun level and
    trimmed at 200 KCAS at 150 ft over flat ground, asked for a hover
    there, and held 30 s more by the hover law within 30 ft of where it
    took her.
    - **She loses 3.43 ft (37,530 lb) and 3.48 ft (39,750 lb).**
    - The conversion balloons her 19.5 and 20.9 ft.
    - Her pitch runs from 11.6 and 12.2 degrees (trimmed) to -1.3 and -1.4
      in the hover, and her bank is at most 1.3 degrees.
    - The hover law has her 108.4 s after the ask.
  - `the_f35b_accelerates_from_a_hover_to_200_kt_and_converts_back_within_10_ft_and_her_attitudes`
    and `..._out_of_a_hover_at_her_combat_weight_...`: lifted to 150 ft and
    settled a minute, then asked for wing-borne flight at 200 KCAS, until
    she is converted back and within 5 kt of it for ten seconds.
    - **She loses 0.35 ft and 0.16 ft** (0.18 and 3.96 with the 11.7
      degrees typed at both weights).
- **Four tests of the stage machine**, at 37,530 lb:
  - `the_f35b_asked_back_to_wing_borne_half_way_through_converting_closes_the_fan_and_flies_on_at_200_kt`:
    the doors open no further than half and are shut within 10 s. A minute
    later she is within 5 kt of 200 and 10 ft of 150. She loses 3.19 ft and
    rises 13.3.
  - `the_f35b_asked_for_a_hover_again_half_way_through_converting_back_hovers_at_150_ft`:
    hovering within four minutes and held 30 s. She loses 3.15 ft.
  - `the_f35b_asked_for_wing_borne_flight_at_150_kt_converts_back_at_200_and_is_held_there`:
    she begins converting back at 199 KCAS or more, and two minutes on she
    is within 5 kt of 200 and 10 ft of 150 ft.
  - `the_f35b_converts_no_lower_than_100_ft_either_way_when_asked_at_50_ft`:
    from 200 KCAS at 50 ft, the doors open and later close no lower than
    98 ft, she hovers within 5 ft of 50 ft, and two minutes after
    converting back she is within 10 ft of 50 ft.

**Seen red**:

- **The four stage-machine tests, on the law as first pushed**:
  - the doors opened on to 0.75 after the abort;
  - she sank 29.4 ft when the hover was re-asked;
  - asked for 150 KCAS she flew into the ground;
  - the doors opened at 50 ft.
- **The four figures**:
  - with the jet-borne law's speed loop taken out, all four fail;
  - with the conversion back flown at the gathering attitude, the
    accelerations lose 61.3 and 74.1 ft;
  - with the rise bound set to 15 ft in the built data, the combat
    slowdown fails at 15.0 ft.

Every one was reverted.

**Re-run, unchanged**:

- `every_published_figure_has_a_flight_and_every_flight_a_figure` counts
  133 (was 129).
- These figures are unchanged:
  - hover thrust 40,644.52 lbf;
  - hover height 0.02 ft;
  - vertical landing 179.54 ft/min;
  - hover endurance 5.57 min;
  - short take-off 592.53 ft;
  - her conventional figures to the CLI's last digit (Mach 1.61, 1,371.92
    nm, 122.24 kt, 2,007.74 ft, 20,390.55 ft/min).
- 36 tests pass: every F-35B test (the hover's wind envelope, her STOVL
  envelope, thrust split, take-off, landing, glide and orbit tests,
  `the_committed_f35b_is_what_its_script_writes`) and the figures' count.
- The selftest hash does not move, `182dd6c996e0ee4c`.
