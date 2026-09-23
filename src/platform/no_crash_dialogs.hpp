#pragma once

// **A program that fails says so and stops; it never waits for a click.**
//
// Under MSVC's debug runtime a failed assert() - this project's, or JSBSim's,
// which asserts on a table looked up with a NaN - opens a dialog box and waits
// for somebody to press Abort, Retry or Ignore. On a CI runner nobody ever
// will. On 2026-09-23 four tests sat like that in the Windows debug job from
// its first minutes until the job's ninety-minute limit cancelled it, having
// run 94 of 492 tests and reported nothing about why.
//
// Called first thing in main(), this sends the runtime's assertion and error
// reports to standard error and has abort() end the program without a report
// dialog, so a failure is a failed test with its message in the log. It does
// nothing outside Windows, where an assert already prints and aborts.
//
// **And a crash says where.** A segmentation fault on Windows ended a test
// with "Exception: SegFault" and not a word more, twice on CI, and neither
// reproduced anywhere else. An unhandled exception now prints its code and
// the stack - function, file and line wherever the program's PDB is beside
// it, the module and offset where not - before the process ends with the
// exception's own code, which is what ctest reads it by.

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#include <crtdbg.h>
#include <cstdint>
#include <cstdio>
#include <stdlib.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace glideslope::platform {

#if defined(_WIN32)
namespace detail {

inline LONG WINAPI report_crash(EXCEPTION_POINTERS* info) {
    std::fprintf(stderr, "glideslope: unhandled exception 0x%08lx at %p\n",
                 static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode),
                 info->ExceptionRecord->ExceptionAddress);
#if defined(_M_X64)
    const HANDLE process = GetCurrentProcess();
    const HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);
    CONTEXT context = *info->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    for (int depth = 0; depth < 64; ++depth) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context,
                         nullptr, SymFunctionTableAccess64, SymGetModuleBase64,
                         nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }
        const DWORD64 pc = frame.AddrPC.Offset;
        char module[MAX_PATH] = "?";
        const DWORD64 base = SymGetModuleBase64(process, pc);
        if (base != 0) {
            GetModuleFileNameA(
                reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(base)), module,
                MAX_PATH);
        }
        struct {
            SYMBOL_INFO info;
            char name[256];
        } symbol{};
        symbol.info.SizeOfStruct = static_cast<ULONG>(sizeof(SYMBOL_INFO));
        symbol.info.MaxNameLen = static_cast<ULONG>(sizeof(symbol.name));
        DWORD64 displacement = 0;
        const bool named = SymFromAddr(process, pc, &displacement, &symbol.info) != FALSE;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = static_cast<DWORD>(sizeof(line));
        DWORD line_displacement = 0;
        const bool lined =
            SymGetLineFromAddr64(process, pc, &line_displacement, &line) != FALSE;
        std::fprintf(stderr, "  #%-2d %s+0x%llx", depth, module,
                     static_cast<unsigned long long>(pc - base));
        if (named) {
            std::fprintf(stderr, " %s", symbol.info.Name);
        }
        if (lined) {
            std::fprintf(stderr, " %s:%lu", line.FileName,
                         static_cast<unsigned long>(line.LineNumber));
        }
        std::fprintf(stderr, "\n");
    }
#endif
    std::fflush(stderr);
    // Ends the process with the exception's code, and no Windows Error
    // Reporting dialog.
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace detail
#endif

inline void no_crash_dialogs() {
#if defined(_WIN32)
    // assert() and the runtime's own errors, to standard error.
    _set_error_mode(_OUT_TO_STDERR);
    // abort() ends the program, without the "abnormal program termination"
    // box or a Windows Error Reporting dialog.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
    // The debug runtime's reports - failed asserts, and its own checks on
    // containers and the heap - to standard error rather than a dialog.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    // A crash prints its stack.
    SetUnhandledExceptionFilter(detail::report_crash);
#endif
}

} // namespace glideslope::platform
