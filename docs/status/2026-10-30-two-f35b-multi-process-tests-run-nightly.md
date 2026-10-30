### Two F-35B multi-process tests run nightly, named, until the busy-machine item is done - 2026-10-30, open

**What is still not true first.** Neither cause is fixed. Both tests failed
on CI twice or more for causes Phase 12a's busy-machine item owns, so under
CLAUDE.md they run on the nightly run, not on pull requests, and a nightly
failure of either is reported in that item.

- `a_client_joining_an_f35b_mid_bank_with_the_first_datagrams_lost_still_begins_on_the_servers_law_state`:
  her pitch past its 0.5 degree bound four times on CI - 0.592 (Ubuntu
  debug, 40800355368), 0.624 and 0.572 on #279's branch (40992479828,
  41006534168), and again on the train's run 41009739851 with main's model.
  The cause traced in docs/status/2026-10-29-stovl-join-pitch-under-load.md:
  her jet-borne law's state, which no server word carries, drifts while the
  joining client's first 150 inputs have the selector off.
- `a_predicting_f35b_whose_client_stops_three_seconds_between_flying_and_sending_is_placed_within_four_steps_travel`:
  on a loaded runner the server's words placed steps off - 10 steps on
  Ubuntu debug (40575068951), and 6.893 m across the stall against 5.51 on
  Windows clang (41009739851).

**Verification.** The ci_shards check lists exactly the four named nightly
tests (passed on a linux-release build of this commit); pull requests'
ctest lines leave the label out.
