### The AI leans the mixture for best power, 2026-10-01 — tail still open

**What is still missing.** The tail's verification is that the AI climbs
each light aeroplane to within its handbook's tolerance (this project's
10%, as the climb rates' is; no handbook states one) of its service
ceiling. Only the Cherokee does. The other three are held back by their
flight models, and each is a tail of its own in COMPLETION_PLAN.md, with
what was measured below.

**The autopilot now holds the bank it asks for** (`src/sim/autopilot.cpp`,
`aileron_trim_`). The one near-ceiling test left red,
`a_cessna_182_near_its_ceiling_holds_its_height_through_a_turn_as_at_3000_ft`,
had its two left turns at 18,801 ft hold the height within 21.1 and 20.6 ft
against the 20 ft band. Traced a second at a time, the cause was not the
height loop: asked for 25 degrees, the 182 banked 28.9 to 30 to the left and
24.3 to 25 to the right, at 3,000 ft as at the ceiling. The bank-to-aileron
law was proportional alone, so it settled where its error balanced the
aeroplane's own roll - a left turn at climb power wants right aileron held,
0.2 of the travel at 5 degrees' error - and the heading's bank integral,
which is meant to trim that out, is clamped away with the rest of a large
heading error. Near the ceiling, at 0.95 throttle, the extra 4-5 degrees'
induced drag put the throttle on its stop and the turn's 3-knot energy
allowance came out as height. An integral on the bank's own error now finds
the aileron that holds it (0.004 a degree-second, a quarter of the travel at
most) - **only while the bank is at its limit**, which is a limit and must
hold, and once the bank asked for has stopped moving; otherwise it fades
out. Two wider versions failed on the way, each seen red: integrating
always, it chased turbulence (`the_autopilot_captures_a_new_heading_altitude_airspeed_and_climb`'s
climb in moderate turbulence settled in 38 s against 30), a stall's wing
drop (the Short S.23 not recovered by an autopilot engaged in its stall)
and an orbit's changing bank in wind (the navigator's orbit 196 m off its
circle against 160); integrating while rolling in, it wound up on the
command's lag and overbanked every turn by 4 degrees. Now the 182's left
turns at 18,801 ft hold the height within 17.6 and 16.8 ft, the right
within 9.1 and 9.3; at 3,000 ft within 9.7 and 7.2. The bank still
overshoots to 28.5 rolling in, before the integral has found the aileron.
**Seen to fail**: with the integral's gain at nought the law is the one
above, red by 1.1 ft. The band and the heights are unchanged.

**Every AI pilot now leans, and hands the mixture over gently** (from the
review of #80, 2026-10-02):
- **The approach and the departure lean too** (`sim/lander.cpp`,
  `sim/departure.cpp`). Both set the mixture full rich every step, so an
  approach or a take-off handed to the AI from a leaned engine high up
  stepped the mixture to full rich in one step - and at 10,000 ft a Cessna
  172P full rich runs past the eight parts of air to one of fuel JSBSim's
  piston engine burns, and stopped. Each now has a leaner, from the mixture
  the controller hands over (`hand_mixture`).
- **The leaner starts by holding the ratio it is handed**, where it once
  held nothing until it had felt for a peak; so throttled back from the
  start it richens on the way down from the mixture it was given.
- **Taken back, the mixture stays where the AI left it until the pilot
  moves the lever** (`sim/controller.cpp`). The take-back walked every
  control to the pilot's, the mixture included, and a pilot who never
  touched the lever has it full rich: at 10,000 ft the engine stopped. Now
  the mixture holds the ratio the AI left (the lever over the pressure
  ratio, as JSBSim meters it), richening as a descent thickens the air,
  until the pilot's lever moves from where it was at the take-back, or the
  held ratio comes within the leaner's two hundredths of it; then it is the
  lever's, at the hand's pace.
- **Never leaner than 14.7 to 1, as a bound** (`sim/leaner.cpp`): leaner
  than that the lever goes richer at a tenth of its travel a second,
  whatever the power says, and a ratio held throttled back is held at 14.5
  at the leanest. It was a tendency: lean of 14.7 it moved richer only as
  fast as the power's slope, and throttled back, where the answer is not
  felt for, a ratio of 15.1 handed over was held for good.
- **An engine stopped while being leaned is richened** to full rich at the
  same pace, so that windmilling it can fire again; one handed over stopped
  is left alone. (Tested, and changed, 2026-10-06, above.)

**Verified**, each seen red:
- `handing_the_aircraft_between_pilot_and_ai_steps_nothing_in_any_phase`
  now also hands over three times high and leaned, a 172P on 0.6 of the
  mixture: an approach from a cruise at 10,000 ft, a take-off from a field
  at 9,500 ft, and a take-back from the AI's cruise at 10,000 ft by a pilot
  whose lever is full rich. Each flown a minute: the mixture moves 0.0041,
  0.0043 and 0.0005 at most in a step, and the engine runs throughout.
  With the lander's, the departure's and the take-back's old mixture, each
  in turn: a step of 0.4 and the engine stopped, the same, and the engine
  stopped.
- `the_leaner_richens_an_engine_lean_of_chemically_correct_within_ten_seconds`
  (new, test_leaner.cpp): handed over at 15.1 to 1 at 6,000 ft, at full
  throttle 9.7-13.2 from ten seconds on, and on a quarter throttle 14.5;
  with the bound taken out, 15.1 for good on the quarter throttle.
- `the_leaner_holds_the_ratio_it_was_handed_below_four_tenths_of_the_throttle`:
  from 8,000 ft down to 4,759 on a quarter throttle, 10.50 handed over and
  10.50-10.51 throughout; red with the hold taken out.
- `the_leaner_rests_at_full_rich_at_full_throttle_low_down`: level at 2,000
  ft at full throttle, the lever at 0.998 at the least; red with the
  lever's way reversed.
- `every_aircraft_turns_ninety_degrees_without_overbanking_or_overshooting`
  (new): every aircraft, both ways, at 10,000 ft, the bank integral acting on
  all of them: the most bank is 27.6 (the Mosquito turning left; 28.0
  without the integral), the airliners' 25.2-27.2 (25.1-26.2 without), the
  most overshoot of the heading 3.45 (the Mosquito, with or without). Held
  to 28 and 4; an integral left to run while the bank lags its command took
  the 747 to 29.6 and turned it red.

**For whoever restarts engines** (the copilot, #64, merged meanwhile): on an
aircraft with a mixture lever, `Autopilot::fly`, the lander and the
departure now set `c.mixture` every step from the leaner, so a mixture set
before them is overwritten; a restart that wants the mixture must hold it
outside them, or hand it to the leaner (it starts from the mixture it is
given, and richens an engine that stops under it).

**The copilot's coast recording is made again** (`tests/data/copilot/coast-openai.jsonl`).
Rebased onto main with the copilot (#64), `the_copilot_follows_the_coast_as_told_by_openai_as_recorded`
went red: played back, its fourth question no longer matched the one
recorded. The flight flies a little differently with the leaner and the
bank integral, and the two YSSY runways, listed by distance, came in the
other order - the playback disregards numbers, not order. Asked of
gpt-5.5-2026-04-23 again (`..._asking_openai_now`, GLIDESLOPE_LIVE_MODEL=1),
it passed, and the recording it made is committed; played back, green. A
recording that breaks when two runways swap places is fragile; a tail (done
2026-10-06).

**Smaller**: the catalogue is read once per aircraft for both its climb
floor and its mixture lever (`from_catalogue` in aircraft.cpp), and the
near-ceiling tests take the ceiling and its mixture from one call
(`near_the_ceiling`), not a static map.

**The Cherokee's stall recovery is named no longer.**
`every_aeroplane_recovered_at_the_first_sign_of_a_stall_loses_no_more_than_its_lesson_allows_within_2_g`
named the Cherokee for losing 328 ft against its lesson's 300; leaned, it
loses 202 ft (with or without the bank integral), and the test asked for
its name to be taken off, which it is.

**The handbook ceilings**, each read from the handbook this time:
- Cessna 172P: 13,000 ft (Pilot's Operating Handbook, 1981, section 1).
  Figure 5-6's climb, standard day, falls from 700 ft/min at sea level to
  about 150 at 12,000 ft, at 76 KIAS at sea level and 70 at 12,000.
- Cessna 182S: 18,100 ft (Information Manual, 1997, page ii). Figure 5-7,
  standard day: 924 ft/min at sea level, about 375 at 12,000 and 280 at
  14,000, at 80 KIAS falling to 72.
- Cherokee 180 E: 13,000 ft, absolute 15,000 (Owner's Handbook, section I).
- J-3 Cub: 14,000 ft solo (assets/figures/j3cub.xml's manual).

**Brought over from #35** (closed unmerged 2026-09-27 to keep pull requests
short): the leaner (`sim/leaner.hpp`, an extremum-seeking loop on the
engine's power per unit of air, never leaner than 14.7:1, holding the ratio
below 0.4 throttle), the catalogue's `mixture-lever`, the
`ceiling_on_the_autopilot` flight and the Cherokee's ceiling figure. #35's
log entry is not repeated here; its pull request comment has it.

**The Cherokee, verified.** `the_ai_climbs_a_cherokee_180_to_its_published_service_ceiling`:
handed over at 6,000 ft full rich, the autopilot climbs it to 13,560 ft at
100 ft/min (13,000 ± 10%); with the leaner off it stops at 7,346 ft (#35).

**Near-ceiling tests at the leaned ceilings.** The near-ceiling heights the
test pilot finds, leaning, are now 23,200 ft (172P), 18,800 (182) and
20,200 (Cherokee). Two changes, from #35's last uncommitted state and this
branch:
- **Handed over at the throttle that holds level there** (`level_throttle`
  in test_autopilot.cpp), not the throttle a flight starts at: high up, the
  start throttle sank the aeroplane 42-90 ft while the throttle came up to
  what the height needs - a mismatch in the hand-over, not anything the
  autopilot does. With it the 172P, Cherokee and Cub turns are green.
- **The flaps test descends at 1,500 ft/min.** At the default 700 the
  Cherokee with 40 degrees of flap at 20,200 ft needs all its throttle to
  hold 73.9 knots coming down, so the throttle stayed at its stop for that
  reason and the test said nothing about a hold left over. Now: off the stop
  after 18.2 s (172P), 19.1 (182), 31.6 (Cherokee). **Seen to fail**: with
  the flaps' release of the hold (`holding_speed_ = false` for an aeroplane
  not clean) taken out, all three stayed at the stop for the three minutes;
  put back, green.
- The 182's left turns were the one red; see the bank integral above.

**The two engine ratings, tried for landing and kept out** (2026-10-01):
- **The Cessna 172P without its `<bsfc>`**, through `make_c172p.py` (the
  handbook, section 1: the O-320-D2J's "160 rated BHP at 2700 RPM"), and
  its propeller retuned: with cp_factor 0.88 and ct_factor from 0.93 to 0.96
  all nine figures are in range (at 0.96: static 2,327 rpm, take-off 906 ft,
  climb 760 ft/min, cruise 119.7 KTAS; at 0.93 the cruise is 118.4 against
  118 - the cruise is the figure that binds, and its source, figure 5-8's
  121 KTAS at 8,000 ft and 2,650 rpm, is sound). The learnt-landing tests
  were run with 0.88 and 0.95. The learnt landing's 27 starts all still land within
  the limits, but `the_learnt_policy_touches_down_in_the_simulation_where_it_did_in_training`
  goes red: up to 37 ft/min harder and 61 m shorter than the touch-downs
  tools/rl/export.py recorded flying the policy on the old engine, against
  10 ft/min and 5 m. **So the engine fix is kept out**: the policy and its
  parity fixture were made on the 209 hp engine, and need retraining, or at
  least re-exporting, with it. The script as tried is not committed.
- **The Cessna 182S rated at 2,400 rpm**, through `make_c182.py` (the
  handbook, section 1: "230 rated BHP at 2400 RPM"): all nine figures stay
  in range, but every powered one moves away from the handbook, to the edge
  of its range: climb 895 -> 968 ft/min (924), cruise at 6,000 ft 138.8 ->
  142.9 KTAS (140, range to 143), top speed 144.0 -> 147.3 (145, to 148);
  the ceiling 13,639 -> 14,578 (18,100). The old tuning compensated for the
  missing 13 hp, so the rating alone is not a fix; kept out with the rest of
  the 182's tail.

**What holds the other three back, measured** (glideslope_cli figures with
the ceiling flight added and a trace, in a scratch copy of the data; none of
it is committed):
- **The Cessna 172P's engine makes 209 hp from 160.** Its engine file is
  the only light aeroplane's that sets `<bsfc>` (0.32 lb/hp/h, below what
  any petrol engine burns at full rich); JSBSim's piston engine makes power
  as the fuel burnt over that, so at full throttle at sea level it made
  209 hp at 2,558 rpm, and 178 hp at 7,500 ft. `make_c172p.py`'s propeller
  cp_factor 1.22 soaked up the surplus. Without the `<bsfc>` (JSBSim then
  sizes it to the rated 160 hp) and with cp_factor 0.85 and ct_factor 0.90,
  all nine figures but the cruise are in range (static 2,364 rpm, climb
  715 ft/min) and it climbs to 16,054 ft. Flown at the handbook's 12,000 ft
  climb speed instead of its sea-level one it reaches 13,709 ft: the model's
  climb is very sensitive to the speed (65 KCAS: 12,361; 75.4: 14,886, both
  with the mixture curve below), and the AI holds the sea-level best-climb
  speed all the way up, because that is the autopilot's speed floor. A
  climb speed falling with height, as both Cessna handbooks give it, is a
  change to the floor and to every test that reads it.
- **The Cessna 182S's engine is rated at 2,575 rpm**; its type certificate
  (3A13) rates the IO-540-AB1A5 at 230 hp at 2,400, and JSBSim makes the
  rated power at maxrpm, so at the governed 2,400 it made 217 hp. At 2,400
  its figures stay in range (climb 962 ft/min, cruise 139.8 KTAS, top speed
  147.3) and it climbs to 13,789 ft. Its make script's Drag_due_to_alpha
  x1.15 stands in for a windmilling propeller's drag in the glide, and
  charges the climb for it too: the induced drag at the climb is 180 lb
  where a span efficiency of 0.75 gives 137. Moved to a drag term only while
  the engine is stopped (0.010 of the wing area, for the glide's 8.9), it
  climbs to 15,208 ft, but climbs 1,027 ft/min at sea level and runs 148.2
  KTAS, both just out. The climb still falls 60 ft/min per 1,000 ft against
  the handbook's 45, with the engine's power (136 hp at 14,200 ft, the
  Gagg-Farrar lapse) and the propeller's efficiency (0.73 to 0.78) as they
  should be; what is left is not found.
- **JSBSim's default mixture curve makes most power at 9.9 to 1** (power is
  the fuel burnt times its MIXTURE table, so it rises as the mixture
  richens until the table falls), where the FAA gives "approximately 12
  parts of air to 1 part of gasoline", the power "essentially constant"
  from 0.0725 to 0.080 fuel/air and falling "gradually at first, then more
  rapidly" richer (Aviation Maintenance Technician Handbook - Powerplant,
  FAA-H-8083-32, volume 1, chapter 2, page 2-4). So the leaner finds up to
  6.4% more power than full rich gives at sea level - at the heights where
  leaning can reach the model's peak, above about 4,000 ft - which the real
  engine has not got.
  A MIXTURE table drawn to the FAA's description (best power 0.0725-0.080,
  0.98 at full rich, 0.92 at 0.100, 0.80 at 0.110, nothing at 8:1) has the
  leaner sit at 13.1 to 1 and gives Cherokee 12,370 ft, Cub (carburettor
  below) 12,332, 172P (as committed) 16,083, 182 (2,400 rpm) 13,685; with it
  the 182's cruise at 6,000 ft falls out of range (135.7 KTAS) unless rated
  at 2,400 rpm too. It also leans at sea level - full rich is always rich of
  best power - against the handbooks' "full rich below 3,000 ft" (172P) and
  "under 5,000 feet" (Cherokee), so it wants a height below which the AI
  does not lean, by aeroplane.
- **The Cub's carburettor.** JSBSim meters fuel as the mixture lever times
  sea-level over ambient pressure, so full rich every engine richens as the
  pressure falls; a float carburettor's venturi meters fuel and air by the
  same depression, so the ratio richens only as the square root of the
  density ratio (and the FAA, page 2-3: carburettors "run richer at
  altitude ... because of the decreased density"). A channel setting the
  Cub's mixture position to the lever times delta over the square root of
  sigma, flown solo (922 lb: the empty weight, a 170 lb pilot in the rear
  seat and full fuel) at 55 mph, climbs it to 15,615 ft with JSBSim's
  mixture curve and 12,332 with the FAA's - either side of 14,000 ± 10% by
  about 200 ft. Full rich with JSBSim's metering, solo, it stops at 7,974.

**The selftest hash is unchanged**: 30ac70b84cab7d7c in linux-debug on this
branch rebased onto main (2026-10-01), as #35 measured on main. It replays a
pilot's inputs, never flies the autopilot, and no model file moved.

**Verified on this branch, rebased onto main**: linux-release, 701 of 701
tests; linux-debug, 679 of 680 before the rebase, the one a timeout of
`the_open_terrain_is_within_its_stated_distance_of_the_ground_flown_on_vulkan`
at 1,800 s under another agent's suite, which passed alone in 57 s.
