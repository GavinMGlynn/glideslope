### The Windows exit crash found, and a state stream counted in simulated time, 2026-09-24

**The virtual-joystick test's crash on the way out is found and fixed.** It
segfaulted on Windows debug after printing "passed" (CI run 35857600253,
and a client on Direct3D 12 exited "Access violation" after writing its shot
in 35853342165). The cdb stack the job printed was forty frames of our own
crash reporter recursing, so the dump itself was read: its faulting thread's
stack, scanned from the bottom and symbolized against the run's PDB with
`llvm-symbolizer`, runs from a thread XInput's input host (`inputhost.dll`)
had just started, through `__dyn_tls_init` - MSVC running the program's
thread_local initialisers for the new thread - into JSBSim's
`thread_local FGLogger_ptr GlobalLogger = std::make_shared<FGLogConsole>()`,
`operator new`, the debug heap, and `__acrt_lock`. The main thread was in
`exit()`, already inside `ExitProcess`.

The cause is the static *debug* C runtime. `exit()` in it (UCRT `exit.cpp`,
`#if !CRTDLL && _DEBUG`) tears the runtime down - per-thread data, the heap
handle, the locks - before `ExitProcess` stops the other threads; the release
runtime skips that. Any thread that starts or ends in the gap runs the
thread_local code, and JSBSim's allocates. Windows starts threads there when
it likes, which is why it was rare and debug-only. Our reporter then faulted
in `vsnprintf` (the runtime's per-thread data was gone), the exception went to
`abort()`, whose lock faulted in turn, and so on until the stack ran out -
which is what hid the first fault.

- **Every program ends through `platform::end_process()`**
  (`src/platform/end_process.hpp`): `_cexit()` - every atexit function, static
  destructor and stream flush `exit()` does - and then `ExitProcess()`, which
  stops the other threads before anything is torn down. The runtime is never
  torn down at all, so there is no gap. The client, server, command-line tool
  and test harness run their bodies and end through it.
- **`a_program_that_has_ended_can_still_allocate_until_its_last_thread_is_gone`**
  builds the moment instead of waiting for Windows to pick it: the program's
  own TLS callback, which the loader calls at process detach after every other
  thread has stopped, allocates as JSBSim does and says so; the ctest wants the
  line and exit 0. Seen to fail on windows-debug with `main` still returning
  (run 35940490213 - the line never came; the loader swallows what the
  allocation raised); it passes on release builds either way, whose runtime
  never tears down. Skipped, not passed, elsewhere.
- **The crash reporter cannot loop and uses none of the runtime**
  (`src/platform/no_crash_dialogs.hpp`): lines are formatted by hand and
  written with `WriteFile`; a fault during a report - on any thread - is let
  go at once; the report names the thread, the function it started at and
  whether the program was exiting; and where `GLIDESLOPE_CRASH_DUMPS` names a
  directory the first fault writes a minidump from inside the handler, before
  anything can bury it.
- **CI reads every dump with Microsoft's symbols**: `.exr -1`, the faulting
  stack 200 frames deep, every thread 60, and `!analyze -v`, over WER's dumps
  and the programs' own, which are uploaded with them.

**`a_client_hears_where_every_aircraft_is` counted the runner.** It held the
state stream to 75 updates in three seconds of wall clock, and the floor had
come down from fifty to ten as debug servers on busy runners fell behind; one
sent nine (run 35857600253). The server sent once per 1/25 s of wall clock,
checked once a pass of its loop, and a pass took every step that had fallen
due in one go - so a server behind real time made fewer, longer passes and
spiralled. Now:

- **The server says where everybody is once per 1/25 s of simulated time**,
  on the first step at or after each (steps 5, 10, 15, 20, 24, ...; 120 / 25
  is 4.8, not a whole number as the old comment said).
- **It takes at most four steps between looks at the network** and owes the
  rest, catching up over the passes after while it answers its clients; four
  steps is under 4.8, so no pass crosses two updates. The owed steps are taken
  before it reports what it flew, so `--seconds N` still flies N seconds.
- **The test is exact.** The client prints the step of the first and last
  update it heard, and the test requires one update for every 1/25 s between
  them - no floor, no ceiling, no dependence on the machine's speed - plus at
  least 24 steps (a fifth of a second) of simulation heard. Seen to fail with
  the server dropping every tenth update (69 heard where 76 were due).

**Verified on Windows debug by repetition as well as by the tests.** Two runs
on the branch carried a temporary job running each test over and over on the
debug build, beside another for load (runs 35942681309 and 35946271366): the
virtual-joystick test and its neighbour
`bound_inputs_set_the_controls_they_name_and_the_aircraft_receives_them`
2,250 times each, `a_client_hears_where_every_aircraft_is` and the Direct3D 12
depth test that crashed on exit in 35853342165 100 times each - none failed,
and every Windows shard of both runs was green. The job is gone again. The
crash was never reproduced on demand (it had been seen twice in several
hundred runs), so the repetitions show only that it did not come back; the
exit-path test is the evidence that the gap it needed is closed.
