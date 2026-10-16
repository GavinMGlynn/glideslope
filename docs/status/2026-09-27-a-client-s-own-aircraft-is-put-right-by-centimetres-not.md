### A client's own aircraft is put right by centimetres, not metres, 2026-09-27 — tail done

**What is missing first.**
- **The network checks keep their 8 and 10 m bounds**, as a regression guard:
  CI's slow runners fall behind real time, which is the tail below. The metre
  is held by the unit test, on every machine.
- **A server that falls behind real time still puts its clients off by
  metres**, and that is a new tail. Part of it is the client's estimate of the
  clocks' difference, the least of the last two seconds, lagging a server's
  catch-up burst. It flies fewer steps than the client in
  the same time, so each input is flown for fewer, and no placing of its word
  can undo that. One run of the 200 ms take-over test on WSL linux-debug,
  with other agents building on the machine, missed its 10 m bound at 24.1 m
  while its server ran at 44% of real time for a second and a half (76 steps
  in 1.44 s). The next run of the same build was 0.077 m.
- **For its first second a client is still learning the clocks' difference**,
  and each time the least comes down it is moved by the steps it changed by:
  2.3 m, three times, 1.1 s after joining at 200 ms. Those corrections are
  hidden like any other, and the prediction error is not counted until 25
  updates have been heard (`sim::offset_settled`).
- **`TRANSPORT.md`'s one version did not change.** The wire grew by two bytes,
  as `WATCH` and the wreck did before it; there is still one version, and a
  client of the day before cannot read an update of today.

**What works.**
- **The server says how far into its newest input it had flown.** The
  client's own motion in a state update ends with a `u16`: the steps of
  1/120 s flown on the input `last_input_applied` names, saturating at
  65,535. A full update is 1,146 bytes sealed, of 1,232. `TRANSPORT.md`, the
  doc client and `test_state.cpp` (the two bytes' place and order) say so.
- **The server applies an input after the steps already owed when it was
  read.** Applied as it was read, a server held up for 50 ms flew six steps of
  a new input on catching up, time that had passed before the input arrived,
  and put the client off by 2 m for a round trip. The newest input heard in a
  pass is kept and applied once that pass's steps are flown; one waiting when
  the aircraft is taken over or handed to the AI is applied first, to the
  aircraft it was sent for.
- **The client places the server's word on its own clock.** The input named
  began on the server at `clock x 120 - steps into it`; less the step the
  client began it, that is the clocks' difference plus that input's lateness.
  The least over the last 50 updates (two seconds) is taken as the
  difference, and the motion is put back at the client's step
  `clock x 120 - difference`; every step flown since is flown again on the
  inputs it was flown on (`sim::Prediction::reconcile`). Replaying from the
  end of the applied input, as before, assumed every input lasted on the
  server as long as it did on the client; taking each input's own lateness
  as the difference would keep the jitter in.
- **An input's beginning is known only where the sequence changes** after a
  step of another. The first a prediction flies may have begun before the
  prediction did - one begun from the first update, or after a take-back -
  and taken as begun there it put the clocks' difference two steps short for
  two seconds of updates: 1.03 m at 200 ms with no jitter at all (found by the
  review). No word on input 0 is used either.
- **The prediction error is measured at that step**, not at the end of the
  input: `glideslope_cli connect --predict` keeps where each step took it and
  holds the server's word against the step before the one it was placed at.

**Verification.**
- **The tail is ticked on**
  `a_client_put_right_from_as_far_into_its_input_as_the_server_had_flown_is_off_by_centimetres`,
  which holds a metre at 100 and 200 ms, with jitter and with jitter and loss,
  on every machine, because nothing in it keeps time. Inputs at 30 Hz, each
  late by `(5 (i - 1)) mod 8` steps (every lateness from none to seven,
  counted); with loss, every seventh input from the fourth never applied (34)
  and every ninth update from the fifth never heard (21), counted; the server
  caught at every step into an input from one to seven, counted. Worst
  correction once the clocks' difference is known: 2.1 mm and 4.7 mm at
  100 ms, 4.4 mm and 9.8 mm at 200 ms (without and with loss); while it was
  being learnt, 2.1 and 2.6 m. **Seen to fail**: with the client replaying
  from the end of the applied input, 3.49 m.
- `a_prediction_begun_part_way_through_an_input_is_not_put_off_by_it`:
  begun two steps into its first input, at 200 ms with no jitter, 4.3 mm,
  held to 0.1 m. **Seen to fail** with the first input's beginning taken as
  the prediction's first step: 1.03 m.
- **The network checks** (`server_impaired.cmake`), bounds kept at 8 and
  10 m: at 200 ms, 60 ms jitter, 10% loss the worst prediction error was
  0.259 m and 0.386 m (two runs); at 100 ms, 0.282 m. With the client
  ignoring the field, 6.47 m at 200 ms.
- **The take-over test** at 200 ms: 0.077 m after the take-over (from 10.6
  and 11.3 m on 2026-09-26); at 100 ms, 0.025 m. With the field ignored,
  3.69 m. One run failed at 24.1 m with its server at 44% speed - the tail
  above.
- The unit tests of prediction and of the state packet, and the doc client's
  session, pass.
