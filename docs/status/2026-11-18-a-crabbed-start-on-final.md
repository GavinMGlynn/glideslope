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
crabbed heading (`frontend/server/main.cpp`, the on-final start). Her
crosswind limits on final (`on_final_crosswind_kts`, in her figures) were
3 kt for every aeroplane. They are now:

| | limit across | first knot that failed |
| --- | --- | --- |
| C172P | 8 | 9: 74 m left of the centreline from the right, 73 m right from the left |
| C182 | 8 | 9: 71 m left, 66 m right |
| PA-28 | 8 | 9: 70 m left, 67 m right |
| J-3 Cub | 6 | 7: 61 m left, 61 m right |

The gate allows 60 m. Every knot from calm up to each limit held both ways.

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
  the count is asserted. Each limit must equal her figures' and the next
  knot must fail. It takes 10 s in linux-release, so it runs on pull
  requests with no label.
  **Seen red** on the figures as they were, with each limit given as 3
  ("c172p held to 8 kt across, her figures say 3", and the same for the
  others). Also seen red with the crab taken out (`most_deg` 0): every
  limit fell to 3.
- Through the server: `a_*_is_put_on_final_only_inside_her_measured_wind_limits_...`
  for all four aeroplanes now fly each new limit (8, or the Cub's 6) from
  either side and are handed to the learnt landing 30 s on, and are refused
  a knot past it. These are the command-line client and the server, crabbed
  by the server.
- `a_player_held_past_the_bound_whose_crossing_runway_is_past_her_crosswind_limit_but_not_her_gates_waits_told_why`:
  its wind moved from 6 kt from 168 to 9, so that 25 and 07 (about 9
  across) are still past the C172P's limit, now 8, and inside her gate's 15.
