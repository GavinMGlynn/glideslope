### The F-15C's lift from NASA's 3/8-scale drop model: she settles at 110.1 knots, not 116.6 — the item stays open

**What is still missing, first.** **She still does not meet her manual's
100 knots.** Held at full aft stick from 200 knots at 10,000 ft, clean
(36,946 lb), idle, she now settles at 110.0 to 110.1 KCAS over the last 20
of 90 s, sinking 7,236 ft/min, at 41.9 degrees (116.5 to 116.6, 7,683
ft/min and 42.4 to 42.5 before). Read from the run at its last step: q
40.95 lb/ft2, lift coefficient 1.086 and drag 1.019 in wind axes, a
resultant of 1.489, against her weight over q S, 1.480, gliding at -40.5
degrees. The lift is the table's 1.22 at 41.9 degrees less the full-aft
stabilator's download, about 0.13, which is why it is not the 1.24 the
table alone gives. 100 knots needs 1.48 x (110.1/100)^2 = 1.79, 21 per
cent more lift and drag than she makes at the angle she settles at, and
TN D-8052's data stop at 40 degrees. Two further disagreements with TN
D-8052 are named and left: **below 16 degrees** the drop model's lift is
up to 15 per cent under hers (0.48 against 0.56 at 8 degrees), and that
range is held by the full-size F-15's approach (TM-4604, 10 degrees,
which she flies at 10 to 11.5) - the drop model's inlets were blocked and
its Reynolds number a tenth of full size; and **her drag** past 32 degrees
is over figure 13's flight data by 0.04 at 36 and 0.07 at 40 (within
0.035 from 16 to 32), because the model adds its drag at zero lift, 0.025,
to the lift times the tangent of alpha, where the flight data show that
product alone. Lowering it would raise her settling speed, not lower it.

**The source.** NASA TN D-8052 (1976, NTRS 19760010068), figures 12 and
13: lift and drag of the 3/8-scale F-15 remotely piloted drop model in
stabilized flight at a Reynolds number of 4 million, inlets drooped 11
degrees and blocked, -24 to 40 degrees, against the 10-percent tunnel
model at 0.8 million. Pages 52 and 53 of the PDF were rendered at 300 dpi,
the axes located from their tick marks, and the centres of the 57 lift
and 37 drag flight points found by their hollow interiors; each curve was
faired by a straight line fitted locally, weighted over 2.5 degrees
(2.0 and 3.0 move none of the values by more than 0.006). Flight lift:
0.89 at 16 degrees, 1.035 at 20, 1.13 at 24, 1.18 at 28, 1.215 at 32,
1.23 at 36, 1.25 at 40 (the tunnel line runs about 0.05 higher from 24
to 34, as the report says, "no more than 5 percent"). Flight drag: 0.26,
0.39, 0.51, 0.63, 0.76, 0.88 and 1.0 at the same angles.

**The change** (`tools/make_f15c.py`, `with_mach_lift`, `RPRV_LIFT`;
the XML made by it, `--check` clean). The lift table's rows from 16 to
40 degrees (0.279 to 0.698 rad) are the flight data at Mach 0.2
(`RPRV_MACH`: the report flew "at low speed" and did not consider Mach;
0.2 is about where she settles). The table is still carried across Mach
in proportion to the lift slope from its Mach 0.5 curve, so the Mach 0.5
rows are the flight data over Mach 0.2's share of it (0.979), and
every column moves in the same proportion. The rows at 45 and 50 degrees,
past the data, are the model's own scaled by what the data raised its 40
degree row by, 1.110: the pinned model's fall-off is the only F-15 source
there, and scaled it meets the data without a step. Below 16 degrees and
at negative alpha nothing changed. Before and after at Mach 0.2: 0.847 ->
0.890 at 16, 0.972 -> 1.035 at 20, 1.040 -> 1.130 at 24, 1.117 -> 1.180
at 28, 1.146 -> 1.215 at 32, 1.126 -> 1.250 at 40, 1.049 -> 1.165 at 45,
1.011 -> 1.122 at 50. The lift now peaks at 40 degrees, not 32. Above
Mach 0.2 the high-alpha lift is now low-speed flight data carried up by
the lift slope's ratio, as the pinned Mach 0.5 curve was, not a high-speed
source. The
pitching moment's fit is unchanged (its scale is taken across 6 to 10
degrees), and the full-aft-stick bound past 24 degrees still holds.

**Figures re-measured**, `glideslope_cli` on linux-debug, 2026-10-09:
- `figures f15c`: 7 of 8 unchanged in range (maximum Mach 2.40, climbs
  15,469 and 56,925 ft/min, ceilings 47,627 and 58,232 ft, sustained turn
  7.80 deg/s, the climb rate at 200 kt 24,555 against 24,556);
  `stall_speed_landing` 112.26 -> **108.40** KCAS, the lift's peak moving
  from 32 to 40 degrees - written into `assets/figures/f15c.xml`, with the
  1.3-times floor the climb-rate figure's comment names, 146 -> 141.
  From entries of 160, 200 and 240 knots (the figure's `entry_kcas` set in
  the build's copy only): 108.22, 108.40 and 108.21. `docs/ASSETS.md` and
  the comments in `src/sim/departure.hpp` and `src/sim/figures.hpp` say
  40 degrees and 108.40.
- `glide-speeds f15c`: 170 kt, unchanged (165 still stalls).
- `plan-speeds f15c`: 160 and 360 kt, unchanged.

**The test.** `the_f15c_held_at_full_aft_stick_settles_no_faster_than_117_knots_against_her_manuals_100`
is now `..._no_faster_than_112_knots_...`: `held_kcas` 112, 1.9 knots over
today's 110.1 for other platforms' arithmetic. Seen to fail: with the old
lift put back in the build's data it went red at 116.6 against 112, and
the new model was restored. Renamed in the CI cost files, same cost.

**Verification.** linux-debug ctest of every F-15C test (22, by `f15c`):
21 passed and `the_f15c_stalls_where_its_own_model_said_it_would` failed
at 108.40 against the old 112.26; with the figure re-measured, it,
the renamed stall test, `the_f15c_climbs_at_the_rate_its_own_model_gave`
and `the_committed_f15c_is_what_its_tuning_script_makes` passed, 4 of 4.
The selftest flies the 172P only; its hash, `7fbe17d5474eae31`, is
unchanged.

The all-aircraft suites that fly the F-15C, linux-debug, one run: 11 of
12 passed - `every_aeroplane_recovered_at_the_first_sign_of_a_stall_loses_no_more_than_its_lesson_allows_within_2_g`,
`every_aeroplane_lands_light_and_heavy_without_a_balloon_a_bounce_or_a_go_around`,
`every_aeroplane_the_ai_lands_taxis_off_the_runway_and_stops_clear_of_it`,
`every_landplane_leaves_the_runway_within_ten_knots_of_its_rotation_speed_and_sooner_rotated_early`,
`every_landplane_takes_off_at_every_loading_within_ten_knots_of_its_speed_for_its_weight_and_unhurt`,
`the_take_off_speeds_come_from_each_aircrafts_published_figures` (departure_speeds),
`how_she_lands_is_what_her_approach_is_flown_at_or_none_without_a_stall_speed`,
`the_approach_speed_is_a_third_above_the_published_landing_stall`,
`every_glide_of_every_aircraft_each_way_round_is_flown_by_exactly_one_glide_test`,
`every_aircraft_has_its_own_tests_of_its_tightest_orbits_and_one_step_past_them`
and `every_aircraft_is_planned_and_routed_from_its_own_speeds_with_or_without_an_approach_speed`;
with the F-15C's own glide and orbit tests above. **One failed, not on
the F-15C**:
`every_aeroplane_left_thirty_seconds_in_a_stall_is_recovered_within_2_g_and_the_height_its_speed_and_sink_need`
on the 182S, 1.82 g against 2 with 10 per cent in hand (1.8). This change
changes no file the 182S's flight reads (its model and figures, and the
code, are as on the base branch, which brought the 182S's new drag
polar), and it is reported, not fixed here. The F-15C in it was
handed over at 105.1 kt and 42.0 degrees, stalled at 40.0, and lost 1,424
ft against 3,577 at 1.57 g.
