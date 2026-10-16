### The F-35B's lift system: she hovers and lands vertically; her short take-off is not yet flown, 2026-10-10 — item still open

*Superseded in part by the two entries above (2026-10-13): her short take-off is flown, its source is the December 2014 SAR, the vertical landing's loading carries that requirement's stores, and the nozzle's swivel no longer makes thrust.*

**What is missing, first**: no short take-off is flown; the lift system is
not in `Controls`, so no pilot can fly it and a client neither sends nor
predicts it (its positions are flight-control properties, and so are in a
replay's state); no AI flight plan hovers her, and the AI still takes her
off and lands her on a runway. Nothing models the ground's effect on a jet
in a hover - no suckdown, fountain or hot gas taken back in - nor the fan
intake's ram drag in a crosswind.

**Sources**, each pinned in `docs/ASSETS.md`: Rolls-Royce's LiftSystem
poster (the LiftFan "more than 20,000 lbf", each roll post 1,950 lbf, the
3BSM 18,000 lbf, swivelling through 95 degrees in 2.5 s); Pratt & Whitney's
F135 Fast Facts, Q4 2021 (the F135-PW-600's hover thrust class, 40,650 lbs;
short take-off thrust class 40,740 lbs); Lockheed Martin's 2015 F-35B card
("40,500 lb Vertical", "without afterburner"); Wiegand et al., AIAA
2018-3368 (the fan's inlet vanes modulate its thrust; the posts about 10 per
cent of the vertical thrust); Lockheed's releases of 23 April 2009 (41,100
lb measured in the hover pit against a requirement of 40,550) and 18 March
2010 (BF-1's first vertical landing, from a hover at 150 ft of about a
minute). **No vertical-landing weight is published** in a document this
project could read: the loading flown, 37,530 lb, is the empty weight, the
pilot and 5,000 lb of fuel and two AIM-120s - the "Vertical Lift Bring Back"
secondary pages quote with no source this project could read - and is named
as this project's choice. **The short take-off distance**: 600 ft from a
flat deck with two 1,000 lb JDAMs and two AIM-120s, sea level, tropical day,
10 kt over the deck, the program's requirement as relaxed from 550 ft in
2012 (Inside Defense, 1 March 2012, as quoted; the Selected Acquisition
Report that states it refused to download, 403) - not yet read from a
primary source, and not yet flown.

**The model** (`tools/make_f35b.py`, which says where each number comes
from): the 3BSM is her engine's own thruster, vectored 0-95 degrees at 38
degrees a second by JSBSim's `pitch-angle-rad`; the fan and two posts are
JSBSim external forces pointing up, and a fourth is the nozzle's sideways
part when it swivels for yaw. Each is a fixed share of what the engine's
thruster passes, in Rolls-Royce's proportions (fan 47.7, posts 4.65 each,
nozzle 43.0 per cent) of Pratt & Whitney's 40,650 lbf; in STOVL mode the
throttle stops at 0.99, short of JSBSim's afterburner, and the shaft's power
is the thrust the nozzle no longer passes - JSBSim's reverser angle, whose
cosine is taken off the thrust (0.685 in a hover). The fan is placed at 200
in (estimated), the posts 115 in out (estimated), and the nozzle at 507.7
in, where it balances the fan about her empty centre of gravity; the
conversion takes 8 s (estimated); the fan's vanes move its thrust 10 per
cent either way (estimated), 80 per cent of one post's air may go to the
other (estimated), and the nozzle swivels 12 degrees sideways (estimated).
In conventional flight every force is zero and the nozzle straight: **her
conventional figures are unchanged to the last digit the CLI prints**, flown
by `glideslope_cli figures f35b` on the old model and the new - Mach 1.61,
1,371.92 nm, a stall at 122.24 kt, a ground roll of 2,007.74 ft and 20,390.55
ft/min.

**The hover control law** (`src/sim/hover.hpp`): height on the throttle
(a climb or sink asked of the height's error, proportional and integral on
the rate), pitch on the fan's share, roll on the posts' split, heading on
the nozzle's swivel, each on its attitude and rate with an integral for
pitch and roll; position on the attitude asked of the drift and distance
from the point it rose from, at most 6 degrees. The throttle moves at most
across its range in two seconds, the others across theirs in one. A landing
sinks at 6 ft/s to 30 ft and 3 ft/s below.

**Verification**, three new figures in `assets/figures/f35b.xml`, each a
test (`tests/unit/test_figures.cpp`), linux-release:

- `the_f35bs_lift_system_makes_pratt_and_whitneys_hover_thrust`: 40,644.5
  lbf against 40,650 within 1 per cent, standing at sea level, loaded full
  (46,300 lb) so that she stays down.
- `the_f35b_hovers_at_150_ft_for_a_minute_within_10_ft`: at 37,530 lb,
  converted on the ground, lifted to 150 ft, settled a minute, then a
  minute more as a 10 kt wind rises across her from the left: her height
  strays 0.01 ft, she drifts 1.5 ft from her point (held within 30) and her
  heading a degree; the fan trims at 0.19 for the pilot ahead of the centre
  of gravity, the throttle at 0.945.
- `the_f35b_lands_vertically_from_a_hover_under_6_ft_a_second`: let down
  from the 150 ft hover, she touches at 179.6 ft/min against the 360 her
  figures file's touchdown criterion allows.

**Each was seen to fail**: with the fan's share set to zero in the built
model the thrust measured 21,243.9 lbf and both flights failed to reach the
hover (she pitched over on the ground); with the law's let-down left at 9
ft/s to the ground the landing touched at 540.3 ft/min. Both reverted.
`every_published_figure_has_a_flight_and_every_flight_a_figure` counts 125
figures (was 122); `every_landplane_takes_off_at_every_loading...` 50
loadings (was 49), the F-35B at her new loading leaving at 187.9 kt against
183.6. Her take-off, landing, figures, glide and orbit tests re-run green;
`the_committed_f35b_is_what_its_script_writes` green. **The selftest hash
does not move**, `182dd6c996e0ee4c`.
