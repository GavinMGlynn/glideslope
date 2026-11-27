### Window-client tests run on macOS, on Metal; a warm-up that times out fails, 2026-11-27

Phase 12c, "On macOS a dozen window-client tests report themselves skipped
after 300 s".

**What is still not true, first.**
- **The item is not done.** On macOS release the HUD tick test on Metal
  (the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_metal) still
  skips, after 334 s in PR #413's run and 155 s in run 43845148929. It
  draws on GLIDESLOPE_GPU_DRIVERS, already Metal on macOS, so the Vulkan
  cause is not its cause. Its only exits 77 are glideslope_skip_when_not_downloaded's,
  and under GLIDESLOPE_REQUIRE_NETWORK only "the weather could not be had"
  skips. But ctest does not print a skipped test's output, so that is not
  shown. Its siblings on live weather passed in the same run. The cause is
  not yet found.
- The 300 s limit on a warm-up server is still a wall-clock limit. A warm-up
  that reaches it now fails the test, and says so, instead of skipping. It
  does not wait longer on a slow machine.
- A warm-up that could not download its terrain still skips where the
  network is not required (GLIDESLOPE_REQUIRE_NETWORK unset), as on a
  developer's machine offline. CI requires the network, so there it fails.
- macOS's cost table (tests/ci_costs/macos-release.txt) still has these
  tests at the 303 s they took to skip. That is longer than they take to
  run, so the shards are dealt cautiously until it is measured again.

## The cause: Vulkan asked for on macOS, not terrain

The plan item said the warm-up server never got its terrain inside its
300 s. It did. Every warm-up finished in seconds. What took 300 s was the
run after it:
1. Thirty-three window-client tests were registered with `-DDRIVER=vulkan`.
   macOS has Metal and no Vulkan, so the client failed at once with "no GPU
   device".
2. The server, started `--seconds 300 --until-empty`, waited out its 300 s
   for a client that never came.
3. The script then found no frame, matched "no GPU|could not|device" in what
   the client said, and reported itself skipped.

The 0.1 to 0.3 s skips were the same failure in tests that do not start
their server first. The 30 s, 121 s and 217 s skips were the same failure
after servers with shorter limits. In run 43845148929 not one
`the_client_with_the_window_*` test passed on macOS. Every one passed on
Linux, Windows and Rocky 9.

Two blocks of these tests were registered only `if("vulkan" IN_LIST
GLIDESLOPE_GPU_DRIVERS)`, so on macOS they did not even exist: the learnt
landing pair, and the joins-again, dropped, lost, restart and NaN tests
with their neighbours.

## The fix

- **`GLIDESLOPE_CLIENT_DRIVER`** in tests/CMakeLists.txt is `metal` on
  macOS and `vulkan` elsewhere. Every window-client test draws on it. The
  two Vulkan-only blocks are now registered on every platform.
- **tests/cmake/skip_or_fail.cmake** holds two rules, and every such script
  calls them instead of its own `cmake_language(EXIT 77)`:
  - `glideslope_warmed_up(rc err)`, called after the warm-up server in each
    of 77 scripts:
    - a warm-up that timed out fails;
    - otherwise it asks glideslope_skip_when_not_downloaded, moved here
      from client.cmake so the two cannot drift: weather not to be had skips,
      and so does a download where the network is not required;
    - one that stopped for anything else fails, with what it said.
    server_tiles_arrive.cmake's warm-up that ran well but left its tile or
    geoid out of the cache now fails as well.
  - `glideslope_cannot_draw(why)`, called at each of the 21 places a client
    could not open its GPU device: it skips, unless
    GLIDESLOPE_REQUIRE_WINDOW is set, as CI sets it, and then it fails. That
    is the rule that would have caught this the first night.

## Verification

- **a_warm_up_that_times_out_fails_and_a_test_skips_only_for_what_its_machine_has_not_got**
  runs every case of both functions in a cmake of its own. Each case is
  judged as passed, skipped or failed, and by what it said. There are 9
  cases: the warm-up ran, timed out (network required or not), could not
  download (required or not), had no weather, or stopped; and a client that cannot draw,
  with a window required or not. The test fails if it walks fewer than 9.
  The timeout is a real one: a process given 1 s and asked to sleep for 10.
- **Seen red**: with the timeout case put back to `cmake_language(EXIT 77)`,
  the test failed with "warm_timed_out skipped (exit 77), where it should
  have failed". Reverted, it passed all 8 of 8. The weather case, added after review, makes 9 of 9.
- **On macOS release** (PR #413's full run, run 45918072973, all six test
  shards green): 45 tests draw on GLIDESLOPE_CLIENT_DRIVER, read from
  tests/CMakeLists.txt with its loops expanded. All 45 ran, counted from
  the six shards' logs, and all 45 passed; none was skipped and none was
  missing. Before this change, in run 43845148929, none of them passed.
  The skips left on macOS are for a missing key or token, for code that is
  Windows' alone, and one more named below.
- **Not run**: the HUD tick test on Metal, above.
