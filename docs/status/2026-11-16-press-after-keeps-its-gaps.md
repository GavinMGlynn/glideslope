# A late client keeps the gaps between its test presses

**What changed.** The window client's `--press-after` presses are timed on the
session's clock, from the step the server gave the aircraft (since CI run
42930877921). A client that built its flight after its first press was due
then made every overdue press on consecutive passes of its frame loop. On a
WSL debug build the flight took 2.4 s to build, so R (1.0 s), L (1.3 s) and F
(1.6 s) all went at 2.5 s. L asked for the learnt landing with the flaps
already settled again, and the server handed her over instead of refusing
her. The test
`the_client_with_the_window_on_final_is_refused_the_learnt_landing_with_its_flaps_moving_told_why_and_handed_to_it_with_l`
failed three times in a row, on main and on the train. On CI the flight builds
in under 1 s, and it passed.

Now a press made late makes the presses after it as late again. Each press
still waits for its own time on the session's clock, and none comes sooner
after the one before than it was asked to (`presses_late_s` in
`src/frontend/client/main.cpp`). The tests that press once, L at 30 s and K
at 12 s, are unchanged.

**The test** no longer holds R and L to "1.x s in". It reads all four presses
and fails if any came before its time, or came sooner after the press before
it than asked. The gaps are judged to within the tenth of a second that the
client prints.

**Verification.** Without the change, it failed three times on this machine
(R, L and F all "2.5 s in"; handed over, never refused). With it, it passed
(320 s).
