#pragma once

namespace glideslope::platform {

// **The CPU time the calling thread has used**, seconds, as its operating
// system counts it: user and kernel time together (CLOCK_THREAD_CPUTIME_ID
// on Linux and macOS, GetThreadTimes on Windows). Time the thread spends
// asleep, or waiting to be run, is not in it - which is what tells a
// program that worked for a long time from one its machine did not run.
// Negative when the system will not say.
double thread_cpu_s();

} // namespace glideslope::platform
