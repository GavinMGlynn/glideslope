### A gear unit that has never touched the ground reports no side friction, 2026-11-27

**What is still not true first.**
- **The fix is ours, around JSBSim's unset member, not in it.** `ext/jsbsim`
  is untouched; upstream's master leaves `FGLGear::FCoeff` unset as v1.3.1
  does. Not reported upstream yet.
- **JSBSim's `orbital/` numbers are NaN on a poisoned heap until her first
  run** (as loaded, before `initialize`). Nothing reads them there; the new
  test checks after `initialize` and a step, and says so.
- **Seen once, on windows-clang; not reproduced on CI's heap.** The cause is
  shown by poisoning the heap instead, below.

**The failure.** CI run 45972866440, windows-clang tests 6/6 (job
140428855231), on #418's branch, which touches only client session code:
`every_aircraft_standing_on_her_brakes_at_idle_stays_finite_and_on_the_ground`
said "a320 went non-finite at step 1 (1 of 730 numbers), first:
gear/unit[8]/side_friction_coeff". Unit 8 is NOSE_TIP, a scraping point at
the nose that never touches when she stands.

**The cause.** `FGLGear` ties `gear/unit[N]/side_friction_coeff` to its
member `FCoeff`, which nothing sets - not the constructor, not
`ResetToIC`, no initialiser - until `ComputeSideForceCoefficient` runs at the
unit's first touch. A unit that has never touched reports whatever the heap
gave the `FGLGear` it lives in: usually zero, on that run a NaN. JSBSim reads
`FCoeff` only after setting it in the same touching step, so her forces were
never affected; the number in the property tree was, and every check that
walks the tree (this test, the CLI's NaN trap) sees it.

Searched first: JSBSim's issues and mailing list for `FCoeff`,
`side_friction_coeff` and NaN, and FGLGear's Pacejka side force at rest -
nothing reports it; the 2013 thread on the side force does not mention it.
Upstream master's `FGLGear.h` still declares `double FCoeff;` bare.

**The fix.** `Aircraft`'s constructor sets every bogey's
`side_friction_coeff` to 0 as she is loaded - what JSBSim works out for a
wheel that is not slipping, and what it overwrites at the first touch.

**The test.** `every_aircraft_loaded_where_fresh_memory_is_nan_reports_only_finite_numbers`
runs its own program, `glideslope_poisoned_heap`, whose `operator new`
fills everything it hands out with 0xFF bytes - a NaN in every double - and
checks that it does (or exits 2, not run). Each of the catalogue's 16
aircraft is loaded, put in the air at 5,000 ft and 150 kt, flown a step, and
every number in her tree must be finite; it fails unless all 16 were.

Seen red: with the fix taken out it fails on all 16 aircraft, 61 gear units
named (the A320's 12, every one of hers), every one
`side_friction_coeff` and nothing else. With the fix, 16 of 16 pass.
linux-debug (sanitized), 2026-11-27.

**How often.** Once. Every failed test job of the last 100 failed runs
(2026-11-11 to 2026-11-27, 124 jobs, every platform) was searched for
`side_friction_coeff`: only this one. The windows-debug (MSVC) build of this
branch passes both tests; clang-cl is not installed on the development
machine, so windows-clang is CI's.

The selftest hash does not move: on these builds' heaps the value was zero.
