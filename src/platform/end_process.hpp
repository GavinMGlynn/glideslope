#pragma once

// **A program ends with its C runtime whole until its last thread is gone.**
//
// Every program's main returns through this: `return end_process(run(...))`
// in spirit, though it never returns. Everywhere but Windows it is exit().
//
// On Windows, exit() from a program linked with the static *debug* C runtime
// (every windows-debug build here) does one thing more than a release build
// does: after the atexit functions and the static destructors it tears the
// runtime itself down - frees its per-thread data, sets its heap handle to
// null, deletes its locks - and only then calls ExitProcess, which is what
// stops the process's other threads. The debug runtime does it so that it can
// report leaks; the release runtime skips it, because the process is ending.
//
// In between, any thread that starts or ends runs the program's thread_local
// initialisers or destructors, and JSBSim has one that allocates:
// `thread_local FGLogger_ptr GlobalLogger = std::make_shared<FGLogConsole>()`
// (src/input_output/FGLog.cpp). MSVC runs it for every thread that starts in
// the process - Windows' own threads too. A thread that XInput's input host
// started as the virtual-joystick test ended allocated through a runtime
// that no longer had a heap or a heap lock; the exception that raised found
// no per-thread data to handle it with and called abort(), whose lock faulted
// in turn, until the stack ran out (CI run 35857600253, read from its dump).
// It happened only when Windows chose that moment to start a thread, so it was
// seen once in a few hundred runs, and only in debug builds.
//
// So on Windows this does the cleanup exit() does - atexit functions, static
// destructors, the main thread's thread_locals, flushing every stream - with
// _cexit(), which returns rather than going on to tear the runtime down, and
// then ExitProcess(), which stops every other thread before anything is
// unloaded. A thread that starts in that short window still finds the
// runtime's heap and per-thread data whole. Its standard library's locks are
// kept whole too, by the step below. Microsoft documents _cexit for exactly this: cleanup
// without ending the process, for a caller that ends it itself.
//
// `tests/unit/test_process_end.cpp` allocates from the last code a program
// runs - the loader's process-detach call, after every other thread is gone -
// and fails if the runtime cannot.
//
// **And the standard library keeps its locks.** _cexit() runs MSVC's standard
// library's own static destructors too, and one of them, `std::_Init_locks`'s,
// deletes the library's locks (crt/src/stl/xlock.cpp). A deleted lock reads
// as held. A thread that starts in the window before ExitProcess and takes
// one waits on it, and faults in ntdll writing 0x24. A debug build's
// containers take `_LOCK_DEBUG` whenever they are copied or destroyed, most
// likely here in the program's thread_local initialisers. A thread started
// at `LoadLibraryW`, most likely by another process (not proved), died so on
// the Windows development machine in 65 of 300 debug runs of one test
// (docs/status/2026-11-27-the-parent-imagery-tests-exit-crash-was-a-thread-from-outside.md).
// The library counts its users and deletes the locks only when the count
// goes below zero, so one more user, made here and never destroyed, keeps
// them until the process is gone. `std::_Init_locks` is the library's own,
// exported from it but declared in no header it ships, so it is declared
// here as crt/src/stl/init_locks.hpp declares it. The test beside the one
// above takes `_LOCK_DEBUG` at the same last call.

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>

#include <new>

#if defined(_CPPLIB_VER)
namespace std {
// MSVC's standard library's count of the users of its locks (xlock.cpp).
// Older standard libraries declared this class in yvals.h. With one of those,
// this declaration is a redefinition and the build fails here. Delete it then.
class _Init_locks {
public:
    _Init_locks() noexcept;
    ~_Init_locks() noexcept;
};
} // namespace std
#endif
#else
#include <cstdlib>
#endif

namespace glideslope::platform {

#if defined(_WIN32)
// One more user of the standard library's locks, never destroyed, so that
// _cexit() leaves them whole.
inline void keep_the_standard_librarys_locks() {
#if defined(_CPPLIB_VER)
    alignas(std::_Init_locks) static unsigned char user[sizeof(std::_Init_locks)];
    ::new (static_cast<void*>(user)) std::_Init_locks;
#endif
}
#endif

[[noreturn]] inline void end_process(int code) {
#if defined(_WIN32)
    keep_the_standard_librarys_locks();
    _cexit();
    ExitProcess(static_cast<UINT>(code));
#else
    std::exit(code);
#endif
}

} // namespace glideslope::platform
