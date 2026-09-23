#pragma once

// **A program that fails says so and stops; it never waits for a click.**
//
// Under MSVC's debug runtime a failed assert() - this project's, or JSBSim's,
// which asserts on a table looked up with a NaN - opens a dialog box and waits
// for somebody to press Abort, Retry or Ignore. On a CI runner nobody ever
// will. On 2026-09-23 four tests sat like that in the Windows debug job from
// its first minutes until the job's ninety-minute limit cancelled it, having
// run 94 of 492 tests and reported nothing about why.
//
// Called first thing in main(), this sends the runtime's assertion and error
// reports to standard error and has abort() end the program without a report
// dialog, so a failure is a failed test with its message in the log. It does
// nothing outside Windows, where an assert already prints and aborts.

#if defined(_WIN32)
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace glideslope::platform {

inline void no_crash_dialogs() {
#if defined(_WIN32)
    // assert() and the runtime's own errors, to standard error.
    _set_error_mode(_OUT_TO_STDERR);
    // abort() ends the program, without the "abnormal program termination"
    // box or a Windows Error Reporting dialog.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
    // The debug runtime's reports - failed asserts, and its own checks on
    // containers and the heap - to standard error rather than a dialog.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
#endif
}

} // namespace glideslope::platform
