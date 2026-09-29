#pragma once

// **A program whose reader has gone carries on.**
//
// Writing to a pipe nobody reads any more raises SIGPIPE, and by default that
// ends the process where it stands. A test's programs run in one pipeline, each
// one's output the next one's input, and a client that has said goodbye then
// prints what it did - to the server its goodbye has just ended. On macOS CI a
// client was killed like that, twice, in runs that had otherwise gone well
// (tests/cmake/closed_pipes.cmake). The same is true of a person's
// `glideslope_server | head`.
//
// Called first thing in main(), this ignores SIGPIPE, so such a write fails
// with EPIPE instead - which is what it does on Windows, where there is no
// SIGPIPE - and what was said to nobody is lost and nothing else. The programs
// already go on through a failed write: nothing they print is checked, and
// what matters goes to files and to the socket. libcurl's setup ignored it
// too (http_curl.cpp), which is why Linux never saw the kill; the other HTTP
// backends leave it be.

#if !defined(_WIN32)
#include <csignal>
#endif

namespace glideslope::platform {

inline void outlive_closed_pipes() {
#if !defined(_WIN32)
    std::signal(SIGPIPE, SIG_IGN);
#endif
}

} // namespace glideslope::platform
