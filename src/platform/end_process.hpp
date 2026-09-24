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
// unloaded. There is no window left in which a thread can start into a
// runtime that is gone. Microsoft documents _cexit for exactly this: cleanup
// without ending the process, for a caller that ends it itself.
//
// `tests/unit/test_process_end.cpp` allocates from the last code a program
// runs - the loader's process-detach call, after every other thread is gone -
// and fails if the runtime cannot.

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>
#else
#include <cstdlib>
#endif

namespace glideslope::platform {

[[noreturn]] inline void end_process(int code) {
#if defined(_WIN32)
    _cexit();
    ExitProcess(static_cast<UINT>(code));
#else
    std::exit(code);
#endif
}

} // namespace glideslope::platform
