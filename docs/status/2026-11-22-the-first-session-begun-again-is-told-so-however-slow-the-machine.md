# The first session of a player begun again on her key is told so, however slow the machine, 2026-11-22

A test fix. No plan item is ticked and no program changed.

## What failed

`a_player_begun_again_on_her_key_while_she_waits_her_turn_on_final_keeps_it_and_is_at_the_gate_thirty_seconds_after`
failed once in November, on CI run 44904071384 (Ubuntu linux-debug): "the
first session was not ended for the newer". Its first session's heard file
had state updates from step 444 to 1469, about 8.5 s, and no word that it
was taken over.

## The cause

The first session of the player begun again ran for 8 s of wall-clock time
(`connect ... 8 --no-goodbye`) and then left in silence. In those 8 s the
third client had to start and be heard, and then the second session had to
start, join on the same key and prove itself. Only then does the server let
the old session go and send it `LEAVING` ("took over from ...: a new session
for ..."). On a slow, sanitized runner that took longer than 8 s, so the
server's word went to a session that had already gone.

The server's output confirms the order. The old session is let go by the
takeover when the newer one proves itself. It is not let go by the cap on
unproven sessions or by `--timeout`. In a passing run here, the whole
sequence took under 2 s, so the first session was always still running when
it was taken over: the test never had a silent first session when it passed.

## The fix

The first session now stays for 900 s and leaves when it hears `LEAVING`,
which is the event the test asserts. It no longer leaves after a fixed time.
`--no-goodbye` is dropped because the session never reaches its end. If the
takeover never comes, the session sits until 900 s and the test fails on
the same assertion as before. The server path under test is unchanged: a
second session on a key that waits on final shares her place and takes over
when it proves itself.

## Verification

- **Seen to fail:** the first session was cut to 0.3 s (standing in for a
  slow machine). The test failed at line 311 with CI's message, and its
  heard file ended "state updates from step 116 to step 149". The server
  still said "took over from": its `LEAVING` went to nobody.
- **Seen to pass, slow:** with the fix, the third client was held back on a
  file touched 20 s after the first session was heard. That is more than
  twice the old 8 s. The test passed, and the first session heard "taken
  over by a newer session for this key".
- **Seen to pass:** with the fix as committed, the test passed.
