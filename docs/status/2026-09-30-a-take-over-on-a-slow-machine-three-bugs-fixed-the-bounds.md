### A take-over on a slow machine: three bugs fixed, the bounds claimed at 20 fps and asserted, 2026-09-30 — not yet seen on CI

**What is still missing, first**: this has not been seen passing on CI's
release presets, and the take-over item stays open until it has. CI has
failed the floor on a release preset: windows-release, the hand-over test,
"frames of 102 ms around the switch" (run 36638803155), and again at 77 ms
(run 36678421057, on af61c52, before the change below), and it failed the
same way here in linux-debug. Here the first switch read the Cessna's model
from disk (below), and with that moved the test passes here; the 42 ms that
read cost is measured in the sanitized linux-debug build only, so what the
rest of windows-release's frame costs is not known, and that the change
holds there is not seen: run 36681942297, on b684035 with the model loaded
before the first frame, windows-release failed the hand-over test again,
"frames of 75 ms around the switch", its longest frame 108 ms. So on
Windows something else in that frame is long, and what is not yet known:
the client now says where the longest pass around a switch spent its time
- "the longest pass around a switch N ms: ticks, hearing, scene, terrain,
sky and draws, HUD, render" - and the window tests print it, failing or
passing (here, linux-debug: 8.5 ms - scene 0.7, sky and draws 2.8, HUD 2.1,
render 2.8), for the next windows-release run to say. Under a load
average of 12 from other work on this machine it failed twice more
(305 ms and 66 ms around a switch) where it passed ten times at 8 to 9 and
quiet. A client whose every
frame is slow - CI's sanitized software Vulkan, 0.95 to 2.4 s - is not yet
shown to stay under the 20 m correction bound: run beside two other window
tests in the sanitized build here, with every frame 0.8 s and more, the
slow-frames test was put right by 21.4 m once, cause not found; and before
it was labelled `timing`, CI's debug presets failed it in run 36674576086
(on b20eaf8): linux-debug 20.624 m, windows-debug 29.491 m, each "1 too
large to hide" in 11 corrections. The plain on-server window test runs on
the debug presets in the same regime, every frame slow, and may fail there
the same way. The step
bounds are **not** claimed on a slow machine: with 1.3 s frames the
take-over test steps 3.3 to 3.6 m, and it now fails there as testing
nothing rather than as a step too large. No simulated frame clock or
lockstep harness was built, and none will be (the owner, 2026-09-30).

**The decision, and the floor asserted.** The window client's step bounds -
under 2.5 m at a take-over, under 5 m at a hand-over or take-back - are a
claim about what a player sees at a playable frame rate, 20 fps and above
(REQUIREMENTS.md, section 8.3 and "Closed 2026-09-30"). The client now says,
beside the steps, how long its frames were: "own aircraft's frames: the
longest within four of a switch N ms, and the largest step otherwise in one
M ms long" (client/shown.hpp - the four frames before each switch and the
four after, the window a switch's steps are measured in). Both window tests
call `glideslope_require_playable_frames` (tests/cmake/client.cmake, where
`GLIDESLOPE_PLAYABLE_FRAME_MS`, 50, is kept with its reason) before they
believe a bound; the take-over test checks the frame of its largest step
away from a switch as well, since it bounds that too. A longer frame fails
the test - it never passes or skips - as the "carried at ... m/s: too slow"
guard does. The hand-over script takes `SLOW_FRAMES` now, as the take-over
one does.
- **Seen to fire**, linux-release, `-DSLOW_FRAMES=250`: the take-over test
  failed with "frames of 263 ms around the switch: slower than the 20 fps
  this bound is claimed for, so the bound tests nothing" (its step 0.350 m,
  inside the bound, which is the point: a pass there would have said
  nothing), and the hand-over test with "frames of 265 ms around the
  switch: ..." (its step 0.848 m).
- **Passing without holds**: linux-release, the take-over test's frames 18 ms
  at most around the switch and 18 ms at its largest step otherwise, steps
  0.328 m at the switch and 0.586 m otherwise, carried at 52.8 m/s; the
  hand-over test's frames 18 ms and 17 ms, steps 0.289 m and 0.798 m, at
  33.9 m/s. linux-debug (sanitized, locally), each test alone: the take-over
  test's frames 19 and 18 ms, steps 0.373 m and 0.928 m at 53.6 m/s; the
  hand-over test's frames **50 ms around a switch, at the floor** (twice),
  steps 0.415 and 0.454 m at the switches. Run beside the take-over test
  (the preset's four jobs), the hand-over test's frames around a switch
  reached 71 ms and it **failed, as testing nothing** - the guard working,
  and a sign that a switch costs the client a long frame of its own in a
  debug build (18 ms in release): which frame, and why, is not found yet.
- **Where they run.** With the floor asserted, a sanitized debug run on a
  software renderer - frames of 250 ms and more - fails loudly as not
  testing, so the `timing` label (#61) keeps these tests out of CI's debug
  presets and nightly.yml's repeats, and in the release presets - where the
  floor was expected to hold, and did not on windows-release (102 ms, the
  first switch's own frame, below). The comments in tests/CMakeLists.txt and ci.yml no longer
  wait on a simulated frame clock.

- **What CI's step was**, from run 36586959960's own lines: "frame 3 after a
  switch, 26 ms long, predicted: 0.000 m from where it was carried at
  53.8 m/s, the blend moving 3.472 m". The prediction did not stray; the
  switch's blend moved that far in a sixtieth of a second, which, eased over
  0.5 s, is a blend of about 70 m: the aircraft taken over was put 1.3 s of
  flight from where it had been shown.
- **A test flag to make passes slow**: `--slow-frames MS` holds a pass of the
  frame loop MS milliseconds longer after each tenth of a second at full
  speed, as a slow machine's mixed frames are (`SLOW_FRAMES` in
  client_rides_along.cmake). At 1300 ms it reproduces CI's failure: 8.492 m
  at the take-over, "frame 3 after a switch, 1952 ms long, predicted: 0.000 m
  from where it was carried at 45.8 m/s, the blend moving 8.492 m" - the
  same signature. A passing run of the take-over test now also prints the
  corrections and what made its largest step at the switch.
- **Fixed: ticks dropped.** The client flew at most 24 ticks a pass and
  dropped the rest, so a pass over 0.2 s put its prediction behind the
  server unflagged. This change carried what it owed to the next pass, as
  the server does, at most four seconds' ticks a pass (a second's was tried,
  and at 1.3 s passes fell further behind each pass); since the review,
  nothing is carried - a pass on a server flies up to four seconds' and
  lets the rest go (below). With a debug
  print of every correction (not kept), at 250 ms passes: before, no server
  word replayed any of the client's inputs, and each pass's first
  correction was 5.0 to 5.4 m; after, each replays 2 to 35 and moves it
  0.000 m.
- **Fixed: a take-over put where a stale word said.** At a take-over the
  flight took the server's motion as of the word that gave it and was not
  flown on, so a word heard at the end of a long pass was that pass behind.
  `sim::Prediction::adopt` now puts it there and flies it through the
  client's inputs since the step the clocks' difference places the word
  at, keeping the difference (it is the connection's, not the aircraft's);
  the client uses it when its own was predicted up to the take-over and the
  aircraft taken is the same aeroplane, and otherwise only puts it there as
  before. Test:
  `an_aircraft_taken_over_is_flown_on_to_now_from_the_word_that_gave_it`
  (tests/unit/test_prediction.cpp) - client and server at 200 ms, another
  Cessna taken over at 3 s and its word heard a second late: flown on
  through 145 steps, 0.066 m from where the server has it. **Seen to fail**:
  with the fly-on taken out of `adopt`, 64.157 m and 0 steps flown on;
  reverted, it passes.
- **Fixed: the old aircraft put right by the new one.** A debug print of the
  snapped corrections (not kept) showed the 433 to 446 m correction "your 4
  mine 4": the word that gave the take-over had set the client's own number
  to the taken one, and a later word in the same pass put the flight - still
  the aircraft left behind, until the caller adopts - right by the one taken,
  half a kilometre off. Now, while a take-over or a join is waiting to be
  taken up, later words replace the motion it will be built from and put
  nothing right. Seen: at 250 ms passes, 1 correction too large to hide
  (435.236 m) before; 0, the worst 12.895 m, after.
- **The take-over test, before and after, locally (linux-debug)**, the
  largest step at the switch / otherwise: no holds 0.363 / 0.492 m before,
  0.682 / 0.492 m after; 250 ms holds 1.148 / 1.469 m before, 0.357 /
  1.666 m after; 1300 ms holds 8.492 m (failed) before, 3.261 and 3.579 m
  (failed) after - what is left there is the blend from where the aircraft
  was drawn as another, still about 70 m, not yet explained. The hand-over,
  rejoin and dropped-client window tests and the prediction network checks
  pass with the fixes.
- **Fixed: put right before the ticks it was owed, and from every word.**
  CI run 36638803155 (linux-debug) failed the on-server window test: "the
  worst 26.751 m, 1 too large to hide", the largest step otherwise in a
  frame 1655 ms long. Two causes, both made worse by flying every tick owed.
  (1) The client heard the server before flying the pass's ticks: at the end
  of a long frame the newest word is about a moment those ticks have not
  reached, so it was put where the server was with nothing to replay, and
  the ticks flew it on past. (2) It was put right from every word heard in a
  pass in turn, each replaying every input since - up to 240 steps, 25 times
  a second of frame - which in a sanitized build took seconds (a debug
  print, not kept: hearing took 1.2, 4.4, then 6.2 s at 0.7 s holds, the
  passes growing to four seconds' ticks). Now `Online::fly` only sends, and
  `Online::hear`, after the ticks, reads everything and puts the flight right
  once, from the newest word; each older one gives only its clocks'
  difference (`sim::Prediction::hear_clock`). The first word after joining
  or a take-back is adopted from the newest too, not from the oldest waiting.
  And `sim::most_unacknowledged` is four seconds of steps, as its comment
  always said: it was 240, four seconds at 60 Hz and two at 120, and at
  1.6 s holds one run was put right by 72 m with 240 and by 2.9 m with 480.
  Corrections are now one for each frame that heard a word, so the on-server
  test's count is 250 here, not about 380; the test's floors are now held
  to frames and words, not to a count (below, from the review).
- **Test**: `a_client_whose_frames_are_held_most_of_a_second_is_never_put_right_too_far_to_hide`,
  the on-server window test with `-DSLOW_FRAMES=700`.
  Locally, linux-debug: 30 corrections, the worst 2.2 m. **Seen to fail**:
  the same script run against the client as this branch had it before
  (hearing before the ticks, from every word, 240 held): "387 corrections,
  the worst 33.567 m, 12 too large to hide". Without holds, 250 corrections,
  the worst 0.6 m; at 250 ms holds 87, the worst 0.9 m; at 1600 ms, 12, the
  worst 2.9 m (by hand, `--slow-frames`). The prediction unit and network
  tests and the on-server, join, ride-along, take-over, hand-over, stall and
  dropped window tests pass; the hand-over test failed once run beside two
  others, as testing nothing (frames of 94 ms around its switch), and passed
  alone - the floor guard, not a step.
- **From the review of #59**, each fixed on the branch:
  - **Time away is not flown.** Owed ticks were carried without end and
    offline too: a laptop's hour asleep was 432,000 ticks flown unattended,
    480 a pass, and a held key moved its lever four seconds' worth in one
    pass. Now nothing is carried: a pass flies at most four seconds' ticks on
    a server and a fifth of a second's alone, as it always did, and keys
    move for at most a fifth of a second (`sim::steps_to_fly`,
    `sim::key_seconds`). Test:
    `a_frame_loop_back_from_an_hour_away_flies_four_seconds_on_a_server_and_a_fifth_alone`,
    nine passes from one step to an hour, on a server and alone. **Seen to
    fail** with a server's due flown uncapped: "on a server, 481 due flies
    481, not 480"; reverted, it passes.
  - **Floors that do not depend on the frame rate.** The client says "heard
    N words on its own aircraft over S s of the server's time, in F frames";
    the on-server tests hold corrections to F (one a frame that heard a
    word), and the words to 20 at least and to four in five of 25 a second
    of S. Locally: 27 corrections in 27 frames from 261 words with holds of
    0.7 s, the worst 1.317 m; 250 in 250 from 250 without, the worst
    1.238 m. A count floor of 10 would have failed on CI's frames.
  - **Holds checked.** `--slow-frames` takes milliseconds above nought and at
    most a minute, or is refused saying so - five tests,
    `the_client_refuses_slow_frames_of_...` (a word, a number with a word
    after it, nought, a negative, more than a minute); and the slow test
    checks the client's longest frame ("own aircraft's longest frame N ms")
    was at least its holds - **seen to fail** with the script holding 1 ms
    and asserting 700: "its longest frame was 22 ms, shorter than the 700 ms
    every hold was"; reverted, it passes. The slow test is labelled
    `timing`: beside two other window tests here, every frame 0.8 s and
    more, it was put right by 21.4 m once - the open part named first. Its
    worst alone, three runs: 0.9, 3.7 and 1.3 m.
  - **Put right once, as well as from each.** Unit test
    `a_seconds_frame_heard_at_once_is_put_right_once_from_its_newest_word`:
    a second's frame at 100 ms each way, its 24 words heard at once - put
    right once by 0.323 m, replaying 25 steps, at the step putting it right
    from each placed it, 0.2 mm from where that left it, and the clocks'
    difference settled from the frame's words. **Seen to fail** with
    `hear_clock` doing nothing (and `reconcile` sampling for itself): "the
    clocks' difference was known after the frame, from its words" failed;
    reverted, it passes. And `three_seconds_of_unacknowledged_inputs_are_held`
    - **seen to fail** at 240: "held 240 of the 360 steps flown".
  - **The switch's long frame: the model of the aircraft ridden in, read
    from disk at the switch.** A shot on a server draws only its own frame
    and a switch's. Frames around each switch in the hand-over test,
    linux-debug, alone, before: the frame the first switch's draw is in -
    handed to the AI, riding along in its own seat - 48.0, 70.3, 53.1, 61.7,
    51 and 52 ms in six runs, five failing the floor; every other frame
    17 to 18 ms. Timing the sections of that pass with a steady clock (a
    debug print, not kept): ticks 0.0 ms, hearing 0.1, scene and HUD text
    3.0, render 4.4 - and 42.3 ms finding the aircraft ridden in, where
    `model_of` read the Cessna's visual model and geometry the first time
    anything asked for them; 0.0 ms at the second switch, and at every other
    pass. The change loads its own aircraft's model before the first frame,
    on a server only. (Other aircraft's models are still read when first
    drawn or ridden in: loading them when first heard of was tried and taken
    out, being untested and able to lengthen an early frame of a mixed
    fleet's while the clocks' difference is learned.) After, the switch's
    pass 12.8 to 16.7 ms, the model lookup 0.0 ms, and the hand-over test
    alone passed ten runs in ten: frames around the switches 22, 25, 25, 26
    and 27 ms at most on a quiet machine (with the models loaded on first
    sight in the frame loop, own included), and 39, 34, 33, 41 and 26 ms
    under a load average of 9 from other work (the fix as committed); at a
    load average of 12, 305 and 66 ms, which is that load, not the switch -
    the timed switch passes there were 13 to 17 ms. Loading its own model in
    the first frames rather than before them lengthened one while the
    clocks' difference was learned and the slow-frames test was put right by
    20.5 m once, so it is loaded before the first frame. An earlier try,
    drawing a frame as soon as the flight was built, did not help (17, 70,
    53, 62 ms) and was taken out. The floor is not loosened, and no frame is
    left out of the window.
  - **Where a switch's pass spent its time**, said by the client: "the
    longest pass around a switch N ms: ticks, hearing, scene, terrain, sky
    and draws, HUD, render", the longest pass among the four before each
    switch, its own and the four after; the window tests print it beside
    their frames, and first in a failure. A report, not a check: nothing
    asserts it.
  - **Two lesson tests timed out on windows-debug in run 36685209843** (on
    48ade7b): `a_circuit_flown_low_downwind_is_named_in_the_debrief` and
    `an_approach_taken_back_at_half_speed_is_landed_to_a_stop`, 900 s each,
    and the shard was cancelled. Not this change: they fly the lessons in
    `sim/` and use nothing it touched (the prediction, and the client's frame
    loop). That shard's runner was slow at everything - the approach lesson
    886 s against its 200 s cost, the cockpit and orbit view shots 447 and
    569 s against 119 and 150 - about four times its cost on average, where
    the same run's shards 1 and 3 took 0.81 and 1.14 times theirs. On this
    branch's four other runs (36638803155, 36674576086, 36678421057,
    36681942297) the circuit test took 362, 443, 420 and 324 s and the
    half-speed approach 174, 211, 201 and 167 s, as on other branches (323
    to 501 s and 150 to 272 s in the last 30 runs). Here in linux-debug,
    rebased, under a load average of 8 to 12: 364 and 159 s, inside their
    452 and 204 s costs.
  - **Counting, and what is held.** Frames put right are now counted where
    the correction is made, so a frame whose last word handed the aircraft
    to the AI is not counted. Unit test
    `no_more_than_four_seconds_of_unacknowledged_inputs_are_held`: five
    seconds flown with nothing acknowledged leaves 480 held - **seen to
    fail** with the limit six seconds; reverted, it passes.
  - **A waiting take-over kept in step.** A newer word replacing it now also
    reads who flies it, and gives the clocks' difference, as an older word
    of a frame does.
  - `OwnShown`'s comments say what its window is: the four frames before a
    switch, its own and the four after.
- **Registered**: the take-over unit test above was compiled but not
  registered with ctest, which `every_compiled_unit_test_is_registered_with_ctest`
  caught on every CI platform; it is now.
- **Why no simulated frame clock.** A client clock stepping by a fixed
  amount each pass does not take the machine out on its own: the server
  steps on its wall clock, so a client slower than its simulated frames
  falls behind it, and the clocks' difference - the thing measured - moves.
  Lockstep needs the server stepped by the client's clock in test mode as
  well, which reads close to the deterministic simulation this project does
  not have; the owner chose the asserted floor instead (2026-09-30).
