### A damaged tile's fetch again is held until the test has seen it gone, 2026-11-27 — a race in a test

**What was wrong.** `tiles_fetched_ahead_are_never_waited_for_and_a_failed_fetch_is_asked_again_a_minute_on`
failed on CI's Windows debug (run 45937912374, #413's full run) at "and it
is had no more". In its last block, a tile in the cache that the Dem finds
damaged is taken away and fetched again on the fetching thread. The test's
bucket had been let answer in the block before, so that fetch could land
before the test asked whether the tile was gone. The race has been there since
#388.

**What changed.** The bucket is held closed for that block. The test waits
until the fetch again has begun (held), asks, and only then lets it answer.
Nothing in `src/` changed.

**Verification.**
- **Seen red**, with the losing order forced: a wait for the fetch to land
  before the asking failed it with "and it is had no more", CI's own message.
- With the bucket held, it passed 3 of 3 runs on linux-debug. The fetch again
  cannot land before the asking, because the bucket answers only when the
  test lets it.

**Left as it was.** The test takes 60 s on every machine (CI 60.4 s, here
60.05 s), before and after this change.
