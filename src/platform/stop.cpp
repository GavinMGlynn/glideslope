#include "platform/stop.hpp"

#include <csignal>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace glideslope::platform {

namespace {

// Written from a signal handler, which may touch only what is lock-free.
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);

std::atomic<bool> stopping{false};
std::atomic<int> which{0};

#if defined(_WIN32)

BOOL WINAPI on_console_event(DWORD event) {
    if (stopping.load()) {
        return FALSE; // the second: the system's own handling ends the process
    }
    switch (event) {
    case CTRL_C_EVENT:
        which.store(SIGINT);
        break;
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        which.store(SIGTERM);
        break;
    default:
        return FALSE;
    }
    stopping.store(true);
    return TRUE;
}

#else

extern "C" void on_stop_signal(int signal) {
    which.store(signal);
    stopping.store(true);
}

#endif

} // namespace

void catch_stop_signals() {
#if defined(_WIN32)
    SetConsoleCtrlHandler(on_console_event, TRUE);
#else
    struct sigaction action {};
    action.sa_handler = on_stop_signal;
    sigemptyset(&action.sa_mask);
    // Once: the handler is reset to the default as it runs, so the same
    // signal again ends the program whatever it is doing.
    action.sa_flags = static_cast<int>(SA_RESETHAND);
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);
#endif
}

bool stop_requested() {
    return stopping.load();
}

const std::atomic<bool>& stop_flag() {
    return stopping;
}

int stop_status() {
    const int signal = which.load();
    return 128 + (signal != 0 ? signal : SIGTERM);
}

} // namespace glideslope::platform
