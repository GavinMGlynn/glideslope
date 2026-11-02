#pragma once

// **A line a program writes to standard error goes out in one write.**
//
// The programs of a multi-process test share one standard error, and a test
// reads what they said there. A line written in pieces can have another
// program's line written into its middle, and then the test does not find it:
// "...after 3.0 s; 0 dropped glideslope: https://tiles.maps.eox.at/...: given
// up before it began\n...past its cap", impair's summary torn by the client's
// line on CI's linux-release (run 41397416052).
//
// Standard error is unbuffered, and since 2.37 glibc's printf to an
// unbuffered stream stages what it formats in 128 bytes and writes each 128
// as it fills (include/printf_buffer.h, PRINTF_BUFFER_SIZE_TO_FILE_STAGE): a
// line longer than that is two writes or more, whoever wrote it - strace
// shows impair's summary, 146 bytes, as a write of 128 and one of 18.
// Line buffering puts the whole line in the stream's buffer and writes it
// once at its newline; the stream's lock is held for the whole call, so a
// line from one thread is not mixed with another's in the buffer either. One
// write of a line is not split by another writer's: POSIX says so of a pipe up
// to PIPE_BUF (4096 on Linux, 512 on macOS), and lines here are shorter.
//
// Not on Windows: the C runtime takes _IOLBF there as full buffering, which
// would hold every line back until the buffer filled. The UCRT's printf to
// an unbuffered standard error already buffers the whole call for its
// duration and writes it in one _write, as its fwrite does a line
// (tests/tools/impair.cpp, relaying its standard input).
//
// Called first thing in main(), before anything is written to the stream -
// setvbuf may be called only then. Nothing is lost at exit (exit() flushes);
// a line still without its newline when a process crashes is, and a crash's
// own report is written by the runtime or the sanitizer, not through stdio.

#include <cstdio>

namespace glideslope::platform {

// `stream` gets a buffer of `size` bytes of its own, written at each newline.
// False when the C library refused it.
inline bool whole_lines(std::FILE* stream, char* buffer, std::size_t size) {
#if defined(_WIN32)
    (void)stream;
    (void)buffer;
    (void)size;
    return true;
#else
    return std::setvbuf(stream, buffer, _IOLBF, size) == 0;
#endif
}

// Standard error's lines, each in one write.
inline void whole_lines_to_stderr() {
    static char buffer[4096];
    (void)whole_lines(stderr, buffer, sizeof buffer);
}

} // namespace glideslope::platform
