# githooks_pre_push_quick.cmake - the pre-push hook runs every quick test,
# however many there are.
#
#   cmake -DHOOKS=<.githooks> -DWORK=<scratch> -P githooks_pre_push_quick.cmake
#
# **Built, not hoped for.** The hook once handed ctest every quick test's name
# in one -R regular expression; after a full suite run there were so many that
# ctest answered "Expression too big" and nobody could push with the quick
# tests (2026-11-28). Here a scratch repository holds a real CMake project of
# 3000 trivial tests with long sentence names - more than that regex can hold -
# a configure of it, a cost file naming them all quick, and one test labelled
# live that fails if it is run. The hook is run as git runs it, and must report
# all 3000 passed, and must not have run the live one.

cmake_minimum_required(VERSION 3.28)

# The hook is a bash script run from Linux or WSL, and uses ctest
# --tests-from-file, CMake 3.29's (the project's minimum is 3.28; Rocky 10's own
# CMake and the runners' are newer). Skipped, never passed, where it cannot run.
find_program(_ninja ninja)
if(CMAKE_HOST_WIN32 OR CMAKE_VERSION VERSION_LESS 3.29 OR NOT _ninja)
    message(STATUS "SKIPPED: the pre-push hook runs from Linux or WSL, with CMake 3.29 "
                   "and Ninja (this is ${CMAKE_HOST_SYSTEM_NAME}, CMake ${CMAKE_VERSION})")
    cmake_language(EXIT 77)
endif()
unset(ENV{GLIDESLOPE_PUSH_QUICK})  # a 0 in the environment would skip the hook's tests

foreach(_var GIT_ALTERNATE_OBJECT_DIRECTORIES GIT_CONFIG GIT_CONFIG_PARAMETERS
        GIT_CONFIG_COUNT GIT_OBJECT_DIRECTORY GIT_DIR GIT_WORK_TREE GIT_IMPLICIT_WORK_TREE
        GIT_GRAFT_FILE GIT_INDEX_FILE GIT_NO_REPLACE_OBJECTS GIT_REPLACE_REF_BASE GIT_PREFIX
        GIT_SHALLOW_FILE GIT_COMMON_DIR)
    unset(ENV{${_var}})
endforeach()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(_count 3000)
set(_cmakelists "cmake_minimum_required(VERSION 3.28)\nproject(scratch NONE)\nenable_testing()\n")
set(_costs "")
foreach(_i RANGE 1 ${_count})
    set(_name "a_quick_test_with_a_long_sentence_for_its_name_number_${_i}")
    string(APPEND _cmakelists "add_test(NAME ${_name} COMMAND \"${CMAKE_COMMAND}\" -E true)\n")
    string(APPEND _costs "${_name} 1 0.01\n")
endforeach()
string(APPEND _cmakelists
    "add_test(NAME a_live_test_that_must_not_run COMMAND \"${CMAKE_COMMAND}\" -E false)\n"
    "set_tests_properties(a_live_test_that_must_not_run PROPERTIES LABELS live)\n")
string(APPEND _costs "a_live_test_that_must_not_run 1 0.01\n")
file(WRITE "${WORK}/CMakeLists.txt" "${_cmakelists}")
file(WRITE "${WORK}/CMakePresets.json" [[{
  "version": 6,
  "configurePresets": [{"name": "linux-debug", "generator": "Ninja", "binaryDir": "${sourceDir}/build/linux-debug"}],
  "buildPresets": [{"name": "linux-debug", "configurePreset": "linux-debug"}],
  "testPresets": [{"name": "linux-debug", "configurePreset": "linux-debug"}]
}]])

function(run what)
    execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${WORK}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${what} failed:\n${_out}${_err}")
    endif()
endfunction()
run("git init" git init -q -b main)
run("the scratch configure" ${CMAKE_COMMAND} --preset linux-debug)
file(WRITE "${WORK}/build/linux-debug/Testing/Temporary/CTestCostData.txt"
     "${_costs}---\n")

# As git runs it: no refs to push on its standard input.
execute_process(
    COMMAND bash "${HOOKS}/pre-push" origin "https://example.invalid/scratch.git"
    WORKING_DIRECTORY "${WORK}" INPUT_FILE /dev/null
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the pre-push hook refused ${_count} quick tests (exit ${_rc}):\n${_out}${_err}")
endif()
if(NOT _err MATCHES "100% tests passed, 0 tests failed out of ${_count}\n")
    message(FATAL_ERROR "the hook did not report all ${_count} quick tests passed, and the "
                        "live one left out:\n${_out}${_err}")
endif()
message(STATUS "the pre-push hook ran all ${_count} quick tests, and not the live one")
