#include "harness.hpp"

// **A program that has ended can still allocate until its last thread is
// gone.** See src/platform/end_process.hpp for the crash this pins: a thread
// Windows started while a debug-built program was exiting ran JSBSim's
// thread_local initialiser, which allocates, after exit() had torn the C
// runtime's heap and locks down - and the process died on its way out, once in
// a few hundred runs.
//
// Waiting for Windows to start a thread at that moment would be a test that
// only sometimes tests its rule. This builds the moment instead: the loader's
// process-detach call to the program's own TLS callback is the last code a
// program runs, after ExitProcess has stopped every other thread, so it comes
// after any teardown the runtime does. There it allocates as JSBSim's
// thread_local does - make_shared - and says so on standard error, straight to
// the handle. The ctest (tests/CMakeLists.txt, through expect_run.cmake) passes
// only if that line arrives and the program exits 0. Ended through exit(), a
// debug build has no heap by then: on CI (run 35940490213, the harness still
// returning from main) the line never came, the loader swallowing whatever the
// allocation raised, and the test failed.
//
// It is Windows' runtime and Windows' loader, so elsewhere the test says it is
// skipped, never passed.
//
// **Nor can the standard library's locks be gone.** The second test here
// takes, at the same last call, the lock a debug build's containers take
// whenever they are copied or destroyed (`_LOCK_DEBUG`). MSVC's standard
// library deletes its locks as one of its own static destructors, which
// `_cexit()` runs, and a deleted lock reads as held: the next thread to take
// it waits on a lock nobody will free, and faults in ntdll writing 0x24. On
// the Windows development machine a thread started at `LoadLibraryW`, between
// the static destructors and ExitProcess, took that lock and died so in 65 of
// 300 runs of a debug-built test. Its start address most likely means another
// process started it, and it most likely took the lock in the program's
// thread_local initialisers; neither is proved. The test was
// a_parent_imagery_tile_scaled_into_a_missing_childs_place_is_sound
// (docs/status/2026-11-27-the-parent-imagery-tests-exit-crash-was-a-thread-from-outside.md).

#include <atomic>
#include <cstring>
#include <memory>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(_WIN32)
namespace {

std::atomic<bool> armed{false};
std::atomic<bool> locks_armed{false};

} // namespace

namespace {

void say(const char* line) {
    DWORD wrote = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), line, static_cast<DWORD>(std::strlen(line)),
              &wrote, nullptr);
}

} // namespace

extern "C" void NTAPI glideslope_process_end_probe(PVOID, DWORD reason, PVOID) {
#if defined(_CPPLIB_VER)
    // MSVC's standard library's lock: guarded as platform/end_process.hpp is.
    if (reason == DLL_PROCESS_DETACH && locks_armed.load()) {
        say("the process's end reached the program's last call\n");
        {
            // What a debug build's vector or string does as it is copied or
            // destroyed, in a thread_local's constructor or destructor.
            const std::_Lockit lock(_LOCK_DEBUG);
        }
        say("the standard library locks still lock as the process ends\n");
        return;
    }
#endif
    if (reason != DLL_PROCESS_DETACH || !armed.load()) {
        return;
    }
    // First, that the call came at all - so that a failure says which of the
    // two it was.
    say("the process's end reached the program's last call\n");
    // What JSBSim's GlobalLogger does in every thread that starts.
    const auto allocated = std::make_shared<std::string>(64, 'x');
    say(allocated->size() == 64 ? "the runtime still allocates as the process ends\n"
                                : "an allocation at the process's end came back wrong\n");
}

// Into the image's TLS callback list, which the loader calls on every thread's
// start and end and on the process's.
#pragma comment(linker, "/INCLUDE:_tls_used")
#pragma comment(linker, "/INCLUDE:glideslope_process_end_probe_entry")
#pragma section(".CRT$XLY", long, read)
extern "C" __declspec(allocate(".CRT$XLY")) const PIMAGE_TLS_CALLBACK
    glideslope_process_end_probe_entry = glideslope_process_end_probe;
#endif

GLIDESLOPE_TEST(a_program_that_has_ended_can_still_allocate_until_its_last_thread_is_gone) {
#if defined(_WIN32)
    armed.store(true);
#else
    glideslope::test::skip("this is Windows' C runtime and loader; nothing here "
                           "tears the runtime down before the threads stop");
#endif
}

GLIDESLOPE_TEST(a_program_that_has_ended_can_still_take_the_standard_librarys_locks_until_its_last_thread_is_gone) {
#if defined(_WIN32) && defined(_CPPLIB_VER)
    locks_armed.store(true);
#else
    glideslope::test::skip("this is MSVC's standard library and Windows' loader; "
                           "elsewhere nothing deletes the library's locks before "
                           "the threads stop");
#endif
}
