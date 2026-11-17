### Inputs name their step, and a server held up flies those that came together as far apart as they were sent, 2026-11-17

**What is still not true first.** The Phase 12a item stays open: the window
client is still put right 6 to 9 m against a slowed server and 10 to 13 m
against one keeping time, untested and unexplained; and on Windows one
update in a hundred is off by two steps (1.1 m). Neither is touched here.
Stopped alone 3 s, the worst update is still over a metre in two runs of
three (1.29 and 1.37 m). Windows and macOS have not run the held-up tests on
this code yet; their figures come from this pull request's CI.

**The change.** Protocol 19 (`13`): every input frame carries a `u32`, the
client's step it was sent at - the step both clients already stamp for the
clocks' difference (frontend::stamp_sent), or `FFFFFFFF` where no prediction
counts steps. The server (net::HeldInputs::stamp) still flies the newest
input read in a pass from the step due when it was read; each older one read
in the same pass is now due as many steps before the input after it as their
steps are apart, never before the step the server has flown to nor before
the input stamped before them. Before, the older ones were overtaken and
never flown: a server stopped 3 s read the 3 s of inputs in one pass and
flew none of the stick's moves in them. Only differences of the client's
count are used, so it may start anywhere and start again (a pair whose step
does not rise is flown as one, as before). This is the command frame of
Overwatch (Ford, GDC 2017) and the user command's tick of Source: the client
says which of its steps an input belongs to. Searched: "client-side
prediction input sequence number server reconciliation tick number server
buffers inputs catch up", "Overwatch GDC netcode command frame server input
buffer client tick stamped inputs".

**Verification** (Linux debug, this machine, the paced test's flight through
a 100 ms relay; one run at a time, nothing else running; main's binaries and
this branch's run alternately).

| server | main: worst / 95th / 99th | now |
| --- | --- | --- |
| stopped alone 3 s, three runs | 2.57, 3.57, 4.66 m / 0.58, 0.55, 0.72 m / 1.76, 2.54, 3.50 m | 0.99, 1.37, 0.90 m / 0.44, 0.58, 0.38 m / 0.71, 0.88, 0.79 m |
| held up 150-600 ms every second, one run | 0.59 / 0.09 / 0.56 m | 0.49 / 0.06 / 0.30 m |

A fourth run of each was made with a build beside it and is not counted:
main 8.33 m worst, this branch 1.29 m.

- `inputs_read_together_are_flown_as_far_apart_as_their_client_sent_them`
  (test_held_inputs.cpp): six ways of naming steps, counted - ninety of a
  stall's, four apart, each flown from its own step and none let go; unevenly
  spaced; spaced wider than owed; spaced back past an input stamped before;
  not said; a count begun again. Seen red with each older input due with the
  one after it (main's rule): of ninety, one flown.
- `every_input_frame_carries_the_step_it_was_sent_at_through_the_wire`
  (test_inputs.cpp): five frames, nought, not said and the most a step can
  be among them, each again in the packets after it. Seen red with the step
  written as nought.
- The stopped-alone test's worst bound is now 3 m (was 10 m, for the
  named limit this closes): twice the most measured since. The held-up-
  unevenly test's 10 m stays until CI has run it on this code.
- The version moved with its tests (`the_protocol_version_moves_...`,
  `an_envelope_that_is_wrong_...`, the gearstick refusal's bytes, the client
  written from the document, which sends its steps as not said).
- docs/TRANSPORT.md: the frame's new field, what a step is and what the
  server does with it, and version `13` in the list.
