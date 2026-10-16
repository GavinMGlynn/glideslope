### Clients fly at the server's pace, steered by the prediction's own clocks' difference, 2026-10-08 — the tail stays open

**What is still missing first.** The window client is paced too, but has no
test of it: against a server at 0.6 of real time it was still put right by
5.8, 5.8 and 8.8 m once its clocks' difference was known (15.1 and 22.6 m
unpaced, the second over the 20 m bound), against 10.6 m with a server
keeping real time - the unexplained 9 to 13 m of the window client remains,
and with it the item's "under a metre". The prediction tests' 20 m bound is
unchanged.

**What changed.** `sim::Pacing` (`src/sim/pacing.hpp`): a proportional and
integral loop that holds the clocks' difference the prediction measures where
it was when first known. Fallen - a server slower than this client - the
client flies slower; risen, faster. It is steered by the least of the ten
newest words (`sim::Prediction::recent_clocks_difference`, `pace_window`):
the least over all fifty is ten seconds of real time from a server at a fifth
of it, and the loop swung through that lag. Gains 1 /s and 0.25 /s²
(critically damped, half a radian a second); the pace is kept between 0.1
and 2, and the integral not wound up past them. Before the difference is
first known the client flies at the session clock's fitted rate, never above
1 (`before_held`): switched at once from 1 to 0.6 at the hold, the inputs
already flown at 1 put the command-line client off by 4.1 m; and the window
client's first fit, from words heard in a burst after its slow start, said
2.0 and cost ten seconds of swing. The fitted rate is only where the loop
starts - flown on it alone, open loop, the client drifted (the entry below).

- `glideslope_cli connect --predict` flies its steps on the paced clock and
  says `paced: flown at P of this machine's clock at the end; the clocks'
  difference N steps from the one held then, M at worst`. `--unpaced` flies
  at its own pace; `--until-compared N` stays until N updates are compared.
- The window client's frame loop advances its fixed step by the frame's time
  times the pace (`client::Flight::pace_by_clocks`, from `Online::hear`).
- `glideslope_server --test-pace F` runs the simulation's clock at F of real
  time: behind by a set factor on any machine that can keep up with it, where
  `--test-step-ms` is as far behind as the machine is slow.
  `client_on_server.cmake -DSERVER_PACE=F` passes it, which is how the window
  client's figures above were measured (by hand, not a registered test).

**Verification.**
- `a_client_paced_by_its_clocks_difference_flies_at_the_servers_pace_and_holds_it`
  (unit, clocks the test sets): against servers at every tenth from 0.2 to
  1.0 of real time (9, asserted), with no fitted rate to start from, the last
  ten of 60 s flown within 0.002 of the server's pace and the difference
  within two steps of the one held; unpaced at 0.6 it fell by over a thousand
  steps. **Seen to fail** with the loop's sign turned: "against a server at
  0.2 of real time it flew at 2.0".
- `a_client_predicting_against_a_server_behind_real_time_flies_at_its_pace_and_is_off_by_centimetres`
  (`tests/cmake/server_paced_prediction.cmake`, port 24866, RUN_SERIAL): the
  server at `--test-pace 0.6`, the client through a 100 ms relay until 600
  updates are compared; it must say it flew within 2 percent of 0.6, with
  the worst error under a metre and the median under 5 cm. Here: worst
  0.560 m, median 3 mm, pace 0.601, the difference one step off at worst;
  five of five green. **Seen to fail** with `-DUNPACED=ON`: worst 7.868 m,
  median 5.211 m.
- The engine-stop test with its server slowed by sleeping
  (`server_engine_prediction.cmake -DSERVER_EXTRA=--test-step-ms;15`, new):
  paced, medians 4 mm after and 21 mm before the stop; unpaced (by a copy
  with `--unpaced`), 20.2 and 20.5 m - which its relative bound let pass.
  That bound is not changed here.

**From the review of #121.** The engine-stop test let a client off by 20 m
pass, its bound being relative: now
`a_client_predicting_its_aircraft_against_a_server_behind_real_time_stops_its_engine_and_is_off_by_centimetres`
(the same script, `-DSLOWED=ON`, port 24868, RUN_SERIAL) runs its server at
`--test-pace 0.6` and holds both medians under 5 cm: 4 mm after the stop and
23 mm before here. **Seen to fail** unpaced (`-DUNPACED=ON`): medians 13.1 m
after, 11.8 m over all. Both clients' handling of a word is now one function,
`sim::Pacing::follow` (the window client's `Flight::pace_by_clocks` and the
command line's `Predicting` call it and nothing else), pinned by
`a_client_paces_itself_by_its_words_before_and_after_its_clocks_difference_is_known_and_after_starting_again`
in all six states it can be in - unknown with no rate, with a rate, with a
rate above 1; held from the rate; fallen twelve steps and flown slower; begun
again after a take-over's `hold`, held anew at the pace flown. **Seen to
fail** with the rate not taken before the hold: "unknown, with a rate: at
it". The window client's frame loop itself is still tested by nothing paced
(the 6 to 9 m above). The completion plan's items were cut back to summaries;
from them: part of the server-behind error was once the client's clock
estimate lagging the server's catch-up (2026-09-27), and the prediction
tests' 20 m bound is unchanged, its month of CI counted under the nightly
item.
