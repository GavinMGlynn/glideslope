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
// reproduced anywhere else. A crash now prints its code, the thread it came in
// - its name, the function it was started at, and whether the program had
// begun to exit - and the stack: function, file and line wherever the
// program's PDB is beside it, the module and offset where not. Where
// GLIDESLOPE_CRASH_DUMPS names a directory, the first one also writes a
// minidump there. The process then ends with the exception's own code, which
// is what ctest reads it by.

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
#include <cstddef>
#include <cstdint>
#include <stdlib.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace glideslope::platform {

#if defined(_WIN32)
namespace detail {

// **The report calls nothing of the C runtime.** On 2026-09-23 it was asked
// about a fault in a thread that started after exit() had torn the debug
// runtime down (platform/end_process.hpp). Its vsnprintf needed the runtime's
// per-thread data, which was gone, so it faulted too; that exception went on
// to abort(), whose lock faulted in turn, and so on until the stack ran out.
// The dump's top forty frames were that loop, and the fault that started it
// was out of sight. So every line is put together here by hand and written
// with WriteFile; the rest is kernel32, ntdll and dbghelp, which do not use
// this program's runtime.
struct Line {
    char text[1024];
    std::size_t size = 0;

    Line& put(const char* s) {
        while (s != nullptr && *s != '\0' && size < sizeof(text)) {
            text[size++] = *s++;
        }
        return *this;
    }
    Line& hex(std::uint64_t v) {
        char digits[16];
        int n = 0;
        do {
            digits[n++] = "0123456789abcdef"[v & 0xf];
            v >>= 4;
        } while (v != 0 && n < 16);
        put("0x");
        while (n > 0 && size < sizeof(text)) {
            text[size++] = digits[--n];
        }
        return *this;
    }
    Line& dec(std::uint64_t v) {
        char digits[20];
        int n = 0;
        do {
            digits[n++] = static_cast<char>('0' + v % 10);
            v /= 10;
        } while (v != 0 && n < 20);
        while (n > 0 && size < sizeof(text)) {
            text[size++] = digits[--n];
        }
        return *this;
    }
    // Straight to the standard-error handle, not through the C runtime's
    // stream: a crash while the process is exiting comes after the runtime
    // has closed its streams, and anything printed through them then goes
    // nowhere.
    void say() {
        DWORD wrote = 0;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), text, static_cast<DWORD>(size), &wrote,
                  nullptr);
        size = 0;
    }
};

// The thread that called no_crash_dialogs(), which is the main thread.
inline volatile LONG main_thread = 0;

// Set by an atexit function registered first, so run last of them: from then
// on the program is on its way out, and a crash report says so.
inline volatile LONG exiting = 0;
inline void __cdecl note_exiting() {
    InterlockedExchange(&exiting, 1);
}

// Which module an address is in, as "name+0xoffset".
inline void put_address(Line& line, const void* address) {
    HMODULE module = nullptr;
    if (address != nullptr &&
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(address), &module)) {
        char path[MAX_PATH];
        const DWORD n = GetModuleFileNameA(module, path, MAX_PATH);
        const char* name = n > 0 ? path : "?";
        for (DWORD i = 0; i < n; ++i) {
            if (path[i] == '\\' || path[i] == '/') {
                name = path + i + 1;
            }
        }
        line.put(name).put("+").hex(reinterpret_cast<std::uintptr_t>(address) -
                                    reinterpret_cast<std::uintptr_t>(module));
    } else {
        line.hex(reinterpret_cast<std::uintptr_t>(address));
    }
}

// The faulting thread: its id, the name it was given, and the function it
// was started at - which says whose thread it is when the stack does not.
inline void put_thread(Line& line) {
    line.put("glideslope: in thread ").dec(GetCurrentThreadId());
    using Describe = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    const HMODULE kernel = GetModuleHandleA("kernelbase.dll");
    const auto describe =
        kernel == nullptr ? nullptr
                          : reinterpret_cast<Describe>(reinterpret_cast<void*>(
                                GetProcAddress(kernel, "GetThreadDescription")));
    PWSTR description = nullptr;
    if (describe != nullptr && describe(GetCurrentThread(), &description) >= 0 &&
        description != nullptr) {
        char name[128];
        const int n = WideCharToMultiByte(CP_UTF8, 0, description, -1, name,
                                          static_cast<int>(sizeof(name)), nullptr, nullptr);
        if (n > 1) {
            line.put(" \"").put(name).put("\"");
        }
        LocalFree(description);
    }
    using Query = LONG(WINAPI*)(HANDLE, int, PVOID, ULONG, PULONG);
    const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    const auto query =
        ntdll == nullptr ? nullptr
                         : reinterpret_cast<Query>(reinterpret_cast<void*>(
                               GetProcAddress(ntdll, "NtQueryInformationThread")));
    void* start = nullptr;
    constexpr int thread_query_set_win32_start_address = 9;
    if (query != nullptr && query(GetCurrentThread(), thread_query_set_win32_start_address,
                                  &start, static_cast<ULONG>(sizeof(start)), nullptr) >= 0) {
        line.put(", started at ");
        put_address(line, start);
    }
    if (static_cast<LONG>(GetCurrentThreadId()) == main_thread) {
        line.put(", the main thread");
    }
    line.put(InterlockedCompareExchange(&exiting, 0, 0) != 0
                 ? ", after the program began to exit\n"
                 : "\n");
    line.say();
}

// **The first crash is kept whole.** Where GLIDESLOPE_CRASH_DUMPS names a
// directory, the first fault reported writes a minidump there from inside the
// handler, of the process as it was at the fault - before anything that
// follows can bury it. CI names one, and keeps what is written.
inline void write_dump(EXCEPTION_POINTERS* info, Line& line) {
    static volatile LONG written = 0;
    if (InterlockedExchange(&written, 1) != 0) {
        return;
    }
    wchar_t path[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"GLIDESLOPE_CRASH_DUMPS", path, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 64) {
        return;
    }
    const auto append = [&](const wchar_t* s) {
        while (*s != L'\0' && n < MAX_PATH - 1) {
            path[n++] = *s++;
        }
    };
    const auto append_number = [&](DWORD v) {
        wchar_t digits[12];
        int d = 0;
        do {
            digits[d++] = static_cast<wchar_t>(L'0' + v % 10);
            v /= 10;
        } while (v != 0 && d < 12);
        while (d > 0 && n < MAX_PATH - 1) {
            path[n++] = digits[--d];
        }
    };
    append(L"\\glideslope-");
    append_number(GetCurrentProcessId());
    append(L"-");
    append_number(GetCurrentThreadId());
    append(L".dmp");
    path[n] = L'\0';
    const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        line.put("glideslope: could not create the crash dump, error ")
            .dec(GetLastError())
            .put("\n")
            .say();
        return;
    }
    MINIDUMP_EXCEPTION_INFORMATION fault{};
    fault.ThreadId = GetCurrentThreadId();
    fault.ExceptionPointers = info;
    fault.ClientPointers = FALSE;
    const auto type = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpWithUnloadedModules | MiniDumpWithDataSegs);
    const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type,
                                      &fault, nullptr, nullptr);
    CloseHandle(file);
    line.put(ok ? "glideslope: wrote a crash dump of this fault\n"
                : "glideslope: the crash dump could not be written\n")
        .say();
}

// The exception and the stack it was raised on, to standard error - at most
// three times in the process, so that an exception something handles in the
// ordinary course cannot use up the report the crash after it needs.
//
// **Never inside itself.** A fault while a report is being made - in this
// thread or another - is let go at once, without a word: a report that
// faults would otherwise report itself until the stack ran out.
inline void print_stack(EXCEPTION_POINTERS* info, const char* how) {
    static volatile LONG reporting = 0;
    static volatile LONG printed = 0;
    if (InterlockedCompareExchange(&reporting, 1, 0) != 0) {
        return;
    }
    if (InterlockedIncrement(&printed) > 3) {
        InterlockedExchange(&reporting, 0);
        return;
    }
    const EXCEPTION_RECORD& record = *info->ExceptionRecord;
    Line line;
    line.put("glideslope: ").put(how).put(" exception ").hex(record.ExceptionCode).put(" at ");
    put_address(line, record.ExceptionAddress);
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2) {
        const ULONG_PTR kind = record.ExceptionInformation[0];
        line.put(kind == 0 ? ", reading " : kind == 1 ? ", writing " : ", executing ")
            .hex(record.ExceptionInformation[1]);
    }
    line.put("\n").say();
    put_thread(line);
    write_dump(info, line);
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
    for (unsigned depth = 0; depth < 96; ++depth) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context,
                         nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }
        const DWORD64 pc = frame.AddrPC.Offset;
        line.put("  #").dec(depth).put(" ");
        put_address(line, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(pc)));
        struct {
            SYMBOL_INFO info;
            char name[256];
        } symbol{};
        symbol.info.SizeOfStruct = static_cast<ULONG>(sizeof(SYMBOL_INFO));
        symbol.info.MaxNameLen = static_cast<ULONG>(sizeof(symbol.name));
        DWORD64 displacement = 0;
        if (SymFromAddr(process, pc, &displacement, &symbol.info) != FALSE) {
            line.put(" ").put(symbol.info.Name);
        }
        IMAGEHLP_LINE64 source{};
        source.SizeOfStruct = static_cast<DWORD>(sizeof(source));
        DWORD line_displacement = 0;
        if (SymGetLineFromAddr64(process, pc, &line_displacement, &source) != FALSE) {
            line.put(" ").put(source.FileName).put(":").dec(source.LineNumber);
        }
        line.put("\n").say();
    }
#endif
    InterlockedExchange(&reporting, 0);
}

// **Seen first, before anything else in the process can handle it.** The
// unhandled-exception filter alone printed nothing when the virtual-joystick
// test next segfaulted on Windows: something handled the crash, or it came
// where a top-level filter is never asked. A vectored handler is asked about
// every exception first, so it reports the kinds that are crashes - and only
// those, since exceptions are also raised and handled in the ordinary course
// - and lets the search go on exactly as it would have.
inline LONG WINAPI report_first_chance(EXCEPTION_POINTERS* info) {
    switch (info->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case 0xC0000194: // STATUS_POSSIBLE_DEADLOCK, from a critical section
    case 0xC0000374: // STATUS_HEAP_CORRUPTION
        print_stack(info, "first-chance");
        break;
    default:
        break;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

inline LONG WINAPI report_crash(EXCEPTION_POINTERS* info) {
    print_stack(info, "unhandled");
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
    // A crash prints its stack, and says whether the program was exiting.
    InterlockedExchange(&detail::main_thread, static_cast<LONG>(GetCurrentThreadId()));
    atexit(detail::note_exiting);
    AddVectoredExceptionHandler(1, detail::report_first_chance);
    SetUnhandledExceptionFilter(detail::report_crash);
#endif
}

} // namespace glideslope::platform
