#pragma once

// **A NaN that ends a program says where it came from** (`--trap-nan`).
//
// CI's Ubuntu debug run 39883628869 ended a predicting client on JSBSim's
// assertion that a table was looked up at a NaN (FGTable.cpp:618), after two
// water wrecks, and said nothing more: which table, and what made its key a
// NaN, were never found, and nothing reproduced it here (docs/status,
// 2026-10-23). This is what a recurrence says instead.
//
// Where glibc can (Linux on x86 or Arm that traps), the floating-point
// *invalid* exception is unmasked, so the first NaN made from numbers that
// were not NaN - 0/0, the square root of a negative, a NaN compared - stops
// the program at the instruction that made it, with SIGFPE: the backtrace
// printed is where the NaN was born, before any table took it. Everywhere,
// an abort - JSBSim's assertion among them - prints the same. Either way the
// hook given then says what it can of the program's own state (the client:
// every number of the aircraft it was predicting that is not finite, by its
// JSBSim property name), and the signal ends the program as it would have.
// All of it goes to standard error, which the multi-process tests print when
// a program fails, so it lands in the test's log.
//
// Called before any thread is started: a thread takes its floating-point
// environment from the one that starts it. It is a diagnostic for tests,
// not for players: a model that makes a NaN it never uses would be stopped
// by it.

#include <csignal>
#include <cstdio>

#if defined(__GLIBC__)
#include <cfenv>
#include <execinfo.h>
#endif

namespace glideslope::platform {

using NanTrapHook = void (*)();

namespace nan_trap_detail {

inline NanTrapHook& hook() {
    static NanTrapHook h = nullptr;
    return h;
}

inline void on_signal(int sig) {
    std::signal(sig, SIG_DFL);
    std::fputs(sig == SIGFPE ? "nan trap: a NaN made here (an invalid floating-point operation)\n"
                             : "nan trap: aborted\n",
               stderr);
#if defined(__GLIBC__)
    void* frames[64];
    const int n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, 2);
#endif
    if (const NanTrapHook h = hook()) {
        h();
    }
    std::fflush(stderr);
    std::raise(sig);
}

} // namespace nan_trap_detail

// Sets the trap, with `hook` to say what the program knows; whether the
// invalid exception itself is trapped here (glibc), and not only aborts.
inline bool trap_nan(NanTrapHook hook) {
    nan_trap_detail::hook() = hook;
    std::signal(SIGABRT, nan_trap_detail::on_signal);
#if defined(__GLIBC__)
    std::signal(SIGFPE, nan_trap_detail::on_signal);
    return feenableexcept(FE_INVALID) != -1;
#else
    return false;
#endif
}

} // namespace glideslope::platform
