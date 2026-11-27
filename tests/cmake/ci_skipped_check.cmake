# ci_skipped_check.cmake - tools/ci_skipped.cmake prints what every skipped
# test said, and nothing of a test that was not skipped.
#
#   cmake -DTOOL=<tools/ci_skipped.cmake> -DWORK=<dir> -P ci_skipped_check.cmake
#
# Writes tests of its own, one for each way ctest ends a test,
# runs ctest on them with --output-junit, and holds the tool's output to it.
# There are seven cases:
#
#   skipped:   by SKIP_RETURN_CODE; by SKIP_REGULAR_EXPRESSION; for a
#              required file missing; and by return code, saying more lines
#              than are printed, with ';', '[', ']', '<', '>', '&' and '"'
#              among them - each must be printed, as it was said.
#   not:       passed; failed; disabled - none may be printed.
#
# Each test says a word of its own, which is what is looked for. The check
# fails if it judged fewer than seven.

cmake_minimum_required(VERSION 3.28)

set(_project "${WORK}/project")
set(_build "${WORK}/build")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${_project}")

# What each test runs: says CASE's words, and exits as CASE does.
file(WRITE "${_project}/say.cmake" [=[
if(CASE STREQUAL "long")
    foreach(_i RANGE 1 12)
        message("long line ${_i}")
    endforeach()
    message("long-last a;b [c] <d> & \"e\"")
    cmake_language(EXIT 77)
endif()
message("${CASE}-word")
if(CASE STREQUAL "skip_code")
    cmake_language(EXIT 77)
elseif(CASE STREQUAL "fail")
    cmake_language(EXIT 1)
endif()
]=])
# The tests, as ctest reads them: a CTestTestfile.cmake written by hand, so
# that no generator - nor a compiler - is needed to make one.
set(_testfile "")
foreach(_case skip_code skip_regex skip_missing long pass fail disabled)
    string(APPEND _testfile "add_test(${_case} \"${CMAKE_COMMAND}\" -DCASE=${_case} "
                            "-P \"${_project}/say.cmake\")\n")
endforeach()
string(APPEND _testfile
       "set_tests_properties(skip_code long PROPERTIES SKIP_RETURN_CODE 77)\n"
       "set_tests_properties(skip_regex PROPERTIES SKIP_REGULAR_EXPRESSION skip_regex-word)\n"
       "set_tests_properties(skip_missing PROPERTIES REQUIRED_FILES \"${_project}/not-here\")\n"
       "set_tests_properties(disabled PROPERTIES DISABLED ON)\n")
file(MAKE_DIRECTORY "${_build}")
file(WRITE "${_build}/CTestTestfile.cmake" "${_testfile}")

# ctest exits non-zero here - a test fails, and one is not run - which is
# the point; the report is what is judged.
execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${_build}"
                        --output-junit "${WORK}/junit.xml"
                OUTPUT_QUIET ERROR_QUIET)
execute_process(COMMAND "${CMAKE_COMMAND}" -DJUNIT=${WORK}/junit.xml -DLINES=5 -P "${TOOL}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
set(_said "${_out}${_err}")
message(STATUS "the tool said:\n${_said}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "ci_skipped.cmake exited ${_rc}")
endif()

set(_judged 0)
set(_wrong "")
function(_expect case present text)
    string(FIND "${_said}" "${text}" _at)
    if(present AND _at LESS 0)
        set(_wrong "${_wrong}\n  ${case}: \"${text}\" is not printed, and should be" PARENT_SCOPE)
    elseif(NOT present AND NOT _at LESS 0)
        set(_wrong "${_wrong}\n  ${case}: \"${text}\" is printed, and should not be" PARENT_SCOPE)
    endif()
endfunction()

foreach(_case skip_code skip_regex skip_missing)
    _expect(${_case} TRUE "---- ${_case}: skipped")
    math(EXPR _judged "${_judged} + 1")
endforeach()
_expect(skip_code TRUE "    skip_code-word\n")
_expect(skip_regex TRUE "    skip_regex-word\n")
_expect(skip_missing TRUE "not-here")
# The long one: its last five lines, as said, and nothing before them.
_expect(long TRUE "---- long: skipped (SKIP_RETURN_CODE=77); the last 5 of its 13 lines:")
_expect(long TRUE "    long line 9\n")
_expect(long TRUE "    long-last a;b [c] <d> & \"e\"\n")
_expect(long FALSE "long line 8\n")
math(EXPR _judged "${_judged} + 1")
foreach(_case pass fail disabled)
    _expect(${_case} FALSE "${_case}-word")
    _expect(${_case} FALSE "---- ${_case}:")
    math(EXPR _judged "${_judged} + 1")
endforeach()
_expect(summary TRUE "4 of the 7 tests in")

if(NOT _judged EQUAL 7)
    message(FATAL_ERROR "judged ${_judged} of the 7 cases")
endif()
if(_wrong)
    message(FATAL_ERROR "ci_skipped.cmake printed the wrong things:${_wrong}")
endif()
message(STATUS "all 7 cases judged: 4 skipped printed as said, 3 not printed")
