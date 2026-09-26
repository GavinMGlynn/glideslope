#include "platform/memory.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// After windows.h, which it needs.
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <fstream>
#include <unistd.h>
#endif

namespace glideslope::platform {

std::optional<std::uint64_t> memory_held_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof counters;
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(),
                                 reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                                 sizeof counters)) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(counters.PrivateUsage);
#elif defined(__APPLE__)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(info.resident_size);
#else
    // Its second field is the resident set, in pages.
    std::ifstream statm("/proc/self/statm");
    std::uint64_t size = 0;
    std::uint64_t resident = 0;
    const long page = sysconf(_SC_PAGESIZE);
    if (!(statm >> size >> resident) || page <= 0) {
        return std::nullopt;
    }
    return resident * static_cast<std::uint64_t>(page);
#endif
}

} // namespace glideslope::platform
