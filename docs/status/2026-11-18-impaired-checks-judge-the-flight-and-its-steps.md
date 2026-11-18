# The impaired network checks judge the flight, and count steps across a take-back

**From nightly 43988660972.** `prediction_interpolation_and_the_player_limit_hold_at_200_ms_with_jitter_and_loss`
failed once on macOS, on the fifth runner's first run of five. It failed with
"only 165 corrections: its own aircraft was hardly flown", and every other
bound in it held:
- all 503 inputs it sent were applied;
- the worst prediction error was 0.419 m;
- no correction was too large to hide.

This was the first such failure in twelve nights.

**The floor was the fault.** The corrections are as many as the words heard
on its own aircraft that moved it. Those are fewer from a server not keeping
real time, and with words lost or superseded: 252 to 306 here, 165 there.
- "Flown" is now judged by its inputs: every one it sent was applied, and at
  least 250 were sent over the twenty seconds. That is half the 25 a second
  its frames send; it sent 565 to 587 here and 503 on macOS.
- The corrections are held to two a second, 40, as the test's updates
  compared already are (`tests/cmake/server_impaired.cmake`).

**A count that ran backwards.** The same run said "paced in steps:
18446744073709551541 flown here while the server flew 1559".
- glideslope_cli counted its steps from the first update compared to the
  last, as its prediction numbers them. A take-back begins the prediction
  again, numbering from its own start, so the subtraction went below zero
  and wrapped.
- Here it read 335 to the server's 2011 without wrapping.
- A prediction begun again now adds the run before it to the sum, as a
  machine's stall's run is added, and counts afresh. Beginning on the
  server's law state, which counts its errors afresh, clears the sum too.
- The impaired checks now hold the steps to within a hundredth of the
  server's, plus two, as the paced test holds them.

**Verification.** Linux debug, both impaired tests, three runs each, with
the fix:
- 984 to 1224 steps, 0 to 1 apart from the server's;
- 259 to 306 corrections;
- 565 to 587 inputs, all applied.

Without the fix, the 200 ms test fails with "330 steps flown here while
the server flew 2016".
