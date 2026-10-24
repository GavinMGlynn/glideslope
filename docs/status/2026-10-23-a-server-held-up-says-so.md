### A server held up a quarter of a second or more says so, 2026-10-23

**What is still not true first.** Nothing here makes a client's prediction
hold through a server that is held up: inputs that arrive during a pass longer
than `net::most_steps_held` (250 ms) are still flown early, by however much
the pass ran over. That is the open Phase 9 item on the engine-stop prediction
test ("when the server is held up"). Phase order keeps the fix there. This
change only makes such a failure possible to attribute.

**The failure that prompted it.**
`prediction_interpolation_and_the_player_limit_hold_at_200_ms_with_jitter_and_loss`
on windows-clang, run 39868184840 (job 120145047664, branch
take-back-on-event, which does not touch glideslope_cli). The worst prediction
error was 23.2 m against the 10 m bound, 4.2 s in. The median was 0.035 m, and
the 8 updates above the 95th percentile were all over 14 m. All 434 inputs
were applied. The client's longest frame was 63 ms, so it was not the
client's machine stall. "4.2 s in" is counted from the client's start: it
joins about 0.5 s in, hands its aircraft over at 8 s and takes it back at
14 s. So the worst error came **before** any hand-over, and the hand-over and
take-back are not the cause.

**Reproduced, in part.** On Linux release, seeds 12 to 14 and 20 to 22 pass
with worst errors of 0.40 to 0.80 m. In those runs no server pass reached
40 ms (a temporary print, since removed). The same test with the server
stopped 600 ms at 6 s on its clock (`--test-stopped-once 6 600`, the client
running on) fails with the same shape: worst 11.9 and 13.7 m in two runs, and
9 updates above the 95th percentile. The mechanism: inputs read in that pass
are stamped due at most 30 steps on (`net::HeldInputs::stamp`). They are flown
42 steps early, so the clocks' difference (the least of the last 50 words)
drops by about that much. The server's next words are then placed 42 steps
late, for one round trip.

**Not settled.** In the reproduction the worst correction is about the size
of the error (13.3 m against 13.7 m). On CI the worst correction after joining
was 8.2 m against a 23.2 m error. That looks more like several smaller drops,
a server behind real time for about a second, than one stall. CI said nothing
about the server's passes, so it could not be told. Run 39868184840 is a
likely instance of this, not a proven one.

**What now works.** `glideslope_server` prints each pass of 250 ms or more to
standard error, with how much of it was its own CPU time
(`platform::thread_cpu_s`): "a pass of 603 ms, 1 ms of it this server's CPU,
ending at 1.033 s on its clock". If the system does not report CPU time, the
line says so and gives no figure. When `server_impaired.cmake` fails its
prediction-error bound, it now prints those lines, or says there were none.
The next failure of that shape will show whether the server was held up and
whether the time was its own or the machine's.

**Verification.**
- `a_server_held_up_a_quarter_of_a_second_says_so_and_whose_time_it_was`
  (tests/cmake/server_long_pass.cmake) stops the server for 600 ms at 1 s on
  its clock. It requires a reported pass of 600 ms or more, less than half of
  it the server's own CPU, ending between 1.0 and 1.1 s. Linux release: "a pass
  of 603 ms, 1 ms of it this server's CPU, ending at 1.033 s". It skips (exit
  77) without the DEM.
- Seen red: with the print's condition made always false, the test failed with
  "did not say a pass of 600 ms or more". Reverted, it passed.
- The impaired check with the 600 ms stall built in fails as before (11.9 m)
  and now prints the server's "a pass of 602 ms, 2 ms of it this server's CPU"
  line under its prediction error.
- The selftest hash is unchanged: the simulation is not touched.
