#pragma once

// **A program told to stop, stops.** SIGTERM - what `timeout`, systemd and a
// shell's `kill` send - and SIGINT, Ctrl+C; on Windows, Ctrl+C and
// Ctrl+Break. Not the console closing on Windows: the process is ended as soon
// as the handler returns, so there is no stopping in order to be had there,
// and it is left to the system.
//
// Once caught, the signal only raises a flag: a program asks `stop_requested()`
// wherever it waits, and ends in order from there, which is what lets it close
// what it holds - Cesium Native's SQLite cache among it. A transfer given the
// flag as its `HttpRequest::abandon` is given up at once.
//
// **A second signal is not caught.** The handler is installed to run once, so
// whatever the program is stuck in, the same signal sent again ends it the
// system's way.
//
// This was a tail: SDL's own handler turns a SIGTERM into a quit event, which
// the client read only between frames - and a frame waiting on terrain that
// never came, and a shutdown waiting on transfers that never ended, never got
// there. Five `--terrain ion` runs were found alive a day and a half after
// `timeout -s TERM` had fired on them.

#include <atomic>

namespace glideslope::platform {

// Catches the signals above from here on. Call it before SDL_Init, and tell
// SDL to install none of its own (SDL_HINT_NO_SIGNAL_HANDLERS). May be called
// more than once.
void catch_stop_signals();

// Whether a signal to stop has been caught.
bool stop_requested();

// The flag itself, for a transfer to watch.
const std::atomic<bool>& stop_flag();

// The status a program stopped this way ends with: 128 and the signal's
// number, as a shell reports one - 143 for SIGTERM, 130 for SIGINT.
int stop_status();

} // namespace glideslope::platform
