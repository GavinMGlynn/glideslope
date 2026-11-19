### A crabbed start on final, measured on the trim in her wind - 2026-11-18

**What is still not true first.** Past her crosswind limit a player is
still not put on final: she starts where the plan does, told why (the
owner's ruling, 2026-11-11). The limit is measured at one temperature and
pressure (20 degrees, Q1015), on Sydney's 16R, at her model's own loading,
in steady wind with no gusts. The crab is capped at 4 degrees, so past
about 4 kt across (the Cub 3) she still drifts what the crab leaves; that
drift is what sets each limit.

**What now works.** A player put on final is started crabbed into the wind
(`sim::on_final_crab_deg`): the wind correction angle, asin(crosswind / true
airspeed), at most 4 degrees (`OnFinalCrab::most_deg`), a degree inside her
gate's heading limit of 5. Her true airspeed comes from a trim down the
runway in the session's air; the server then starts her again on the
crabbed heading (`frontend/server/main.cpp`, the on-final start). **Only
an aeroplane with a learnt landing is crabbed**, and only at a weight it was
trained at, because that is what was measured. Any other aircraft started on
final with approach speeds (a 737, an F-15) still starts pointing down the
runway, as before. Her crosswind limits on final (`on_final_crosswind_kts`,
in her figures) were 3 kt for every aeroplane. They are now:

| | limit across | at the limit, most off the centreline | a knot past |
| --- | --- | --- | --- |
| C172P | 7 | 43.5 m (16.5 inside) | 8: at the gate, 59.0 m - 1.0 inside, under the 2 m margin |
| C182 | 8 | 55.7 m (4.3 inside) | 9: 71.1 m, 11.1 outside |
| PA-28 | 8 | 54.3 m (5.7 inside) | 9: 69.8 m, 9.8 outside |
| J-3 Cub | 6 | 45.9 m (14.1 inside) | 7: 61.4 m, 1.4 outside (from 078 60.8, 0.8 outside) |

The gate allows 60 m across. **A limit is judged with a 2 m margin**
(`LearntGateEdge::margin_m` in the test). Every knot up to it must be at the
gate and at least 2 m inside the edge, both ways, so that a build's
arithmetic cannot move a limit by a knot. The C172P is at the gate at 8 kt,
but only 1.0 m inside, so her limit is 7, not the 8 first recorded on this
branch. The Cub at 7 fails by 0.8 to 1.4 m. Were a build to put her that
much inside, she would still be inside the margin, so her limit stays 6.

**FMA and none, alike.** The sweep was run in this machine's
linux-release build (gcc's default here, x86-64-v3 with FMA) and again in
a separate build with `-march=x86-64-v2` (no FMA, as CI builds). The two
outputs are identical, every run to the tenth of a metre.

**Why the crab is capped.** With the whole wind correction angle she holds
the centreline, but her heading leaves the gate. In the exploratory sweep
the C172P needs 5.7 degrees at 6 kt, which is past the gate's 5 at once. The
C182 crabbed the whole 4.4 degrees in 5 kt from the left had wandered to 5
off when judged; left alone she wanders up to about 0.7 of a degree in
thirty seconds. At a 4-degree cap every aeroplane is at the gate to its
limit, and at each limit plus one knot she has drifted out of the gate
sideways, not by heading. Searched first: the FAA's Airplane Flying
Handbook, chapter 9 (the crab method: a wind correction angle holding the
track down the centreline on final, taken off before touchdown), and
simulator starts on final (FlightGear and X-Plane, as on 2026-11-11). No
simulator was found that starts an aircraft crabbed. The 2026-11-11 run
that crabbed the whole angle at 15 kt was before 2026-11-12, when she still
met the wind on her first step.

**The sweep, before.** Started pointing down the runway, the in-process
sweep gives what the server runs of 2026-11-14 gave: 3 kt held, and 4 kt
did not (61 to 64 m off). It is built as the server builds her start, so it
stands for the server's start.

**Verification.**
- `every_learnt_landings_aeroplane_started_on_final_crabbed_is_at_her_gate_thirty_seconds_on_up_to_her_crosswind_limit_and_not_a_knot_past_it`
  (tests/unit/test_learnt.cpp): every aeroplane with a learnt landing, the
  catalogue walked whole and counted against the four `rl/*-landing.txt`.
  Each is flown at every whole knot from calm to the most her gate admits
  across (15, the Cub 12), from 258 and from 078, which is 122 runs, and
  the count is asserted. Each limit must equal her figures', and at the
  next knot she must be out of the gate or less than 2 m inside its edge. It takes 10 s in linux-release, so it runs on pull
  requests with no label.
  **Seen red** on the figures as they were, with each limit given as 3
  ("c172p held to 8 kt across, her figures say 3", and the same for the
  others). Also seen red with the crab taken out (`most_deg` 0): every
  limit fell to 3. The 2 m margin was seen red too: with the C172P's
  figures at 8, it reported "c172p's limit is within 2 m of the gate's
  edge: 1.04 inside". That is why her figures now say 7.
  Its cost is entered in tests/ci_costs as the 10 s measured here.
- Through the server: `a_*_is_put_on_final_only_inside_her_measured_wind_limits_...`
  for all four aeroplanes now fly each new limit (8, the C172P 7, the Cub 6) from
  either side and are handed to the learnt landing 30 s on, and are refused
  a knot past it. These are the command-line client and the server, crabbed
  by the server.
- `a_player_held_past_the_bound_whose_crossing_runway_is_past_her_crosswind_limit_but_not_her_gates_waits_told_why`:
  its wind moved from 6 kt from 168 to 9, so that 25 and 07 (about 9
  across) are still past the C172P's limit, now 7, and inside her gate's 15.
