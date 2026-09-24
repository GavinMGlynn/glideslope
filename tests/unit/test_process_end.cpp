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

} // namespace

namespace {

void say(const char* line) {
    DWORD wrote = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), line, static_cast<DWORD>(std::strlen(line)),
              &wrote, nullptr);
}

} // namespace

extern "C" void NTAPI glideslope_process_end_probe(PVOID, DWORD reason, PVOID) {
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
