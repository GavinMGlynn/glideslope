// hold_open - hold the next program's standard input open until it has gone.
//
//   glideslope_hold_open
//
// First in a pipeline, it writes a newline down it every tenth of a second
// and exits 0 once a write fails - which is when the program it writes to has
// gone and nobody reads the pipe any more. So that program's input ends
// because it ended, never because a clock ran out first: however late a slow
// machine starts it, its input is still open (tests/cmake/impair_gives_up.cmake).
//
// SIGPIPE is ignored, or the first write after the reader had gone would end
// it by that instead of by failing (platform/closed_pipes.hpp). Windows has no
// SIGPIPE; a write to a pipe with no reader simply fails there.

#include "platform/closed_pipes.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    glideslope::platform::outlive_closed_pipes();
    while (std::fputc('\n', stdout) != EOF && std::fflush(stdout) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}
