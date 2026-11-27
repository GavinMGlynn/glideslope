# ci_skipped.cmake - what every skipped test said, out of ctest's JUnit report.
#
#   ctest ... --output-junit <file>
#   cmake -DJUNIT=<file> [-DLINES=<n>] -P tools/ci_skipped.cmake
#
# **ctest does not print a skipped test's output**, not even with
# --output-on-failure, and LastTest.log is no help on CI: it says "Test
# Passed." of a skip, and a test that runs ctest itself writes it afresh. So
# a skip on CI said only that it was one. On macOS the HUD tick test on Metal
# skipped after 334 s in PR #413's full run, and why could not be read
# anywhere (docs/status/2026-11-28-skipped-tests-say-why.md).
#
# The JUnit report keeps every test's output. For each test in it that was
# skipped - a <skipped/> element: SKIP_RETURN_CODE, SKIP_REGULAR_EXPRESSION,
# or a required file missing - this prints its name, ctest's reason, and the
# last LINES lines (default 60) of what it said. A test that ran, passed or
# failed, is not printed: a failure's output is ctest's own to print. A
# disabled test has no <skipped/> and is not printed either; it is disabled
# by its own registration, which says why.
#
# It ends by saying how many tests the report holds and how many of them
# were skipped, and fails only if there is no report to read.
# tests/cmake/ci_skipped_check.cmake holds it to a report ctest wrote.

cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED JUNIT)
    message(FATAL_ERROR "usage: cmake -DJUNIT=<ctest's --output-junit file> -P ci_skipped.cmake")
endif()
if(NOT EXISTS "${JUNIT}")
    message(FATAL_ERROR "no JUnit report at ${JUNIT}: did ctest run?")
endif()
if(NOT DEFINED LINES)
    set(LINES 60)
endif()

file(READ "${JUNIT}" _xml)
string(ASCII 28 _open)
string(ASCII 29 _close)
string(ASCII 30 _case_mark)
string(ASCII 31 _semicolon)
# One piece per test: everything from one <testcase to the next. A ';' in
# what a test said would split a CMake list, and a '[' or ']' join its
# pieces, so each is held as a byte no XML text carries, and given back when
# printed.
string(REPLACE ";" "${_semicolon}" _xml "${_xml}")
string(REPLACE "[" "${_open}" _xml "${_xml}")
string(REPLACE "]" "${_close}" _xml "${_xml}")
string(REPLACE "<testcase " "${_case_mark}" _xml "${_xml}")
string(REPLACE "${_case_mark}" ";" _cases "${_xml}")
list(POP_FRONT _cases) # the header before the first

set(_tests 0)
set(_skipped 0)
foreach(_case IN LISTS _cases)
    math(EXPR _tests "${_tests} + 1")
    if(NOT _case MATCHES "<skipped message=\"([^\"]*)\"")
        continue()
    endif()
    set(_why "${CMAKE_MATCH_1}")
    math(EXPR _skipped "${_skipped} + 1")
    string(REGEX MATCH "^name=\"([^\"]*)\"" _ "${_case}")
    set(_name "${CMAKE_MATCH_1}")
    set(_said "")
    if(_case MATCHES "<system-out>(.*)</system-out>")
        set(_said "${CMAKE_MATCH_1}")
    endif()
    # XML's five, held as the ';' they end in was held, &amp; last so that
    # what it gives back is not read again.
    string(REPLACE "&lt${_semicolon}" "<" _said "${_said}")
    string(REPLACE "&gt${_semicolon}" ">" _said "${_said}")
    string(REPLACE "&quot${_semicolon}" "\"" _said "${_said}")
    string(REPLACE "&apos${_semicolon}" "'" _said "${_said}")
    string(REPLACE "&amp${_semicolon}" "&" _said "${_said}")
    string(REGEX REPLACE "\n+$" "" _said "${_said}")
    string(REPLACE "\n" ";" _said_lines "${_said}")
    list(LENGTH _said_lines _n)
    if(_n GREATER LINES)
        math(EXPR _from "${_n} - ${LINES}")
        list(SUBLIST _said_lines ${_from} -1 _said_lines)
        set(_head "the last ${LINES} of its ${_n} lines")
    else()
        set(_head "all it said")
    endif()
    message("---- ${_name}: skipped (${_why}); ${_head}:")
    foreach(_line IN LISTS _said_lines)
        string(REPLACE "${_semicolon}" ";" _line "${_line}")
        string(REPLACE "${_open}" "[" _line "${_line}")
        string(REPLACE "${_close}" "]" _line "${_line}")
        message("    ${_line}")
    endforeach()
endforeach()
message("${_skipped} of the ${_tests} tests in ${JUNIT} were skipped.")
