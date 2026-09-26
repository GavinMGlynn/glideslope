#pragma once

#include <cstdint>
#include <optional>

namespace glideslope::platform {

// **How much memory this process holds**, in bytes, as its operating system
// counts it: the resident set on Linux (/proc/self/statm), the private bytes
// committed on Windows (GetProcessMemoryInfo's PrivateUsage - the working set
// is trimmed at the system's whim, and would hide growth), and the resident
// size on macOS (task_info). Nothing when the system will not say.
//
// For tests that hold a program's memory level over a long run; it is a
// reading, not an accounting.
std::optional<std::uint64_t> memory_held_bytes();

} // namespace glideslope::platform
