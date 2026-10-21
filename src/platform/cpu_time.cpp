#include "platform/cpu_time.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <time.h>
#endif

namespace glideslope::platform {

double thread_cpu_s() {
#if defined(_WIN32)
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) {
        return -1.0;
    }
    const auto ticks = [](const FILETIME& f) {
        return (static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime;
    };
    // In hundreds of nanoseconds.
    return static_cast<double>(ticks(kernel) + ticks(user)) * 1.0e-7;
#else
    timespec t{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) != 0) {
        return -1.0;
    }
    return static_cast<double>(t.tv_sec) + static_cast<double>(t.tv_nsec) * 1.0e-9;
#endif
}

} // namespace glideslope::platform
