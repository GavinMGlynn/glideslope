### The parent-imagery test's exit crash: a thread at LoadLibraryW took a lock the program had deleted, 2026-11-27

Phase 12c, "A parent-imagery unit test died at exit on the Windows
development machine".

**What is not known, first.** Which program puts the thread in is not
proved. Webroot is the likely one, but its log does not name it. The fix
is in the program, not in Webroot, so it holds whoever starts the thread.
windows-release did not crash in 300 runs before the fix.

**Open: windows-release with this branch sometimes misses the last call.**
Both exit tests sometimes failed this way on the development machine. The
program printed its pass and exited 0, but nothing came from the loader's
process-detach call. The allocation test passed 98 of 100 times and the
locks test 97 of 100; the allocation test also failed once in ctest. A
windows-release build of main from 2026-11-05 passed the allocation test
100 of 100 and 60 of 60. Whether this branch's change caused it, or
something that reached main after 2026-11-05, is not known. Building
today's main for release on its own would settle it. windows-debug passed
both tests.

## Reproduced: 65 of 300

Copies of the current windows-debug `glideslope_tests.exe` were run 300
times, with only this test. **65 runs died at exit with 0xc0000409.** The
same 300 runs of the windows-release build had no failures. Every debug run
took over a second, and every release run under one. Each crash printed
what the 2026-10-29 one did:

```
passed a_parent_imagery_tile_scaled_into_a_missing_childs_place_is_sound
glideslope: first-chance exception 0xc0000005 at ntdll.dll+0x1a12d, writing 0x24
glideslope: in thread 17812, started at KERNEL32.DLL+0x3f6c0, after the program began to exit
  #0 ntdll.dll+0x1a12d RtlRaiseException
  #1 ntdll.dll+0x1bd1f RtlSleepConditionVariableCS
```

## The cause

- **The thread started at `kernel32!LoadLibraryW`.** That is 0x3f6c0 in
  today's kernel32 (10.0.26100.9974) and 0x3ee90 in 9549, the 2026-10-29
  version. 9549 is still in `C:\Windows\WinSxS`. A thread that starts
  at LoadLibraryW is how one process loads a DLL into another
  (CreateRemoteThread). Nothing in this repository does that, and the test
  starts no thread at all. Even so, **that the thread came from another
  process is not proved**. It is what that start address usually means.
- **It faulted in `RtlpWaitOnCriticalSection+0xbd`**, called from
  `RtlpEnterCriticalSectionContended`. These names come from ntdll.pdb from
  Microsoft's symbol server. The address it wrote, 0x24, is `ContentionCount`
  in a null `RTL_CRITICAL_SECTION_DEBUG`. So the lock had been deleted.
- **The lock is the program's own: `std::mtx[3]`.** The crash handler's
  minidump (GLIDESLOPE_CRASH_DUMPS) has RCX = `debug_tests.exe+0x1e7fa08`.
  The test program's PDB puts that at `std::mtx+0x78`, the fourth
  40-byte lock. That is `_LOCK_DEBUG`, which a debug build's containers
  take whenever they are copied or destroyed.
- **The program deleted it as it exited.** `end_process` runs `_cexit()`,
  and among the static destructors that runs is MSVC's
  `std::_Init_locks`, which deletes every one of the library's locks
  (`crt/src/stl/xlock.cpp`). A deleted lock reads as held. A thread that
  starts before ExitProcess takes `_LOCK_DEBUG`, waits, and faults. What
  makes it take that lock is not proved: the stack shows no frame past
  ntdll. **Most likely** it is the program's thread_local initialisers,
  which every new thread runs (JSBSim's logger, Cesium's scheduler
  vectors). The lock and the fault are shown; the frames between them are
  not. Release builds do not take that lock there, and exit
  in under a second, so they were not seen to crash.

The same fault, `ntdll.dll+0x1a12d` under ntdll 9549, is in this machine's
Application event log for `glideslope_cli.exe` (2026-09-25, 09-26) and
`glideslope_tests.exe` (09-26, 10-01). The window client's exit crash of
2026-10-01 also names a thread started at `KERNEL32.DLL+0x3ee90`. This is
likely their cause too. It is not shown for each of them.

## Webroot's log, read first

`C:\ProgramData\WRData\WRLog.log` runs from 2023 to today, 86,739 lines.
From 2026-10-28 to 10-30 it has only "Monitoring process" lines, three
scans that found nothing, and status lines. There is no block, no
quarantine and no kill. The quarantine folder holds three files, all from
July. The test program's lines for that run:

```
Thu 29-10-2026 06:36:16.0690	Monitoring process C:\Development\glideslope\build\windows-debug\tests\glideslope_tests.exe [E8B8665BCE5EDDD0236BA88FDD0C3D9F]. Type: 3 (448884)
```

That is one of six such lines, types 3 to 8. Webroot watched the program;
it did not kill it. Webroot's `WRusr.dll` is loaded into every process
here: it was in three fresh probe programs, in PowerShell, and in the
crash's minidump. Fresh .NET probes that watched their own threads for 8 s
saw no thread start at LoadLibraryW, so which program sends it is not
shown.

## The fix, and its test

`platform/end_process.hpp` now makes one more user of the library's locks
before `_cexit()` and never destroys it. The library counts its users and
deletes the locks only when the count goes below zero, so they stay whole
until ExitProcess has stopped every thread. `std::_Init_locks` is exported
from the library but declared in no header it ships, so it is declared
there as the library's source declares it.

`a_program_that_has_ended_can_still_take_the_standard_librarys_locks_until_its_last_thread_is_gone`
(tests/unit/test_process_end.cpp) takes `_LOCK_DEBUG` from the loader's
process-detach call. That is the last code a program runs, after
`_cexit()`. It passes only if a line printed after the lock arrives.

- **Seen red** before the fix (windows-debug, `tools/windows_build.sh`):
  the program reached its last call, and the line after the lock never
  came.
- **Green** with the fix, in the same run as the parent-imagery test and
  the allocation test beside it: all three passed.
- **The crash gone**: the fixed windows-debug build ran the parent-imagery
  test 300 times, the same way as before, with no failures. The build before
  the fix failed 65 times in 300.
