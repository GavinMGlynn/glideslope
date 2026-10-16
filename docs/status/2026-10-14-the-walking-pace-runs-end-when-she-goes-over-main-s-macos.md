### The walking-pace runs end when she goes over: main's macOS debug abort, 2026-10-14 — a red main fixed; one found

**What is still not true first.** **The abort is not reproduced here**: on
main after #195, CI run 38534931958's macOS debug job aborted
`every_landplane_rolling_at_walking_pace_in_a_15_kt_crosswind_...` on JSBSim's
`FGTable.cpp:618` assertion as the J-3 Cub, named for going over in 15 kt
across, was flown on over into the runway until her numbers went to NaN. No
build here takes her over in debug (linux-debug and `-march=x86-64-v2` stop
her at -1.0 degrees); linux-release does, to -45.1. PR CI runs macOS in release
only, so #195's runs did not meet it. **Found**: judging every strike, as tried
first, finds the C172P sitting on her tail in all three walking-pace tests -
the test's hand on the throttle, holding her at 3 kt against the autobrake,
winds to full power with her stick full back, and she rears to 11.2 degrees
2.2 s after the brakes come on. That is the test's hand, not her rollout; a
new Phase 9 item, and strikes are not judged here until it is fixed.

**The fix.** `walking_pace_then_stop` (tests/unit/test_lesson.cpp) ends an
aircraft's run the step her pitch or roll passes 45 degrees or is NaN
(`gone_over`, written so a NaN counts), and says so: a named aircraft's "went
over" is shown with her other named faults, anyone else's is wrong; what the
ended run did not get to (her walking time, her stop) is not reported as a
fault of its own. Her least pitch starts from the hand-over, not 1e9.
Nothing else it judges changes. `gone_over` is pinned by
`a_walking_pace_run_counts_her_gone_over_past_45_degrees_or_at_an_attitude_that_is_no_number`,
twelve constructed attitudes, because in the runs it fires only where a
platform's flight takes her over; seen red with a plain `>`, whose NaN cases
passed as upright. **Confirmed on macOS debug only by** a dispatched CI run
of this branch (the PR's own runs build macOS in release).

**Verification.** The new test and the four walking-pace and gust tests pass in linux-release,
`-march=x86-64-v2` and linux-debug. In linux-release the Cub's crosswind run
now ends at pitch -45.1, roll 26.1 ("went over ... her run ended there"),
where main flew her on to -87.8 and rolled 180. The selftest hash does not
move: no simulation code changed.
