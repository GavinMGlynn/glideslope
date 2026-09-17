# registered_tests.cmake - every test compiled into glideslope_tests is a ctest,
# and every ctest name given to it is a test it has.
#
#   cmake -DPROGRAM=<glideslope_tests> -DREGISTERED="<name;name;...>"
#         -P registered_tests.cmake
#
# A test written and never registered never runs, and nothing would say so.
cmake_minimum_required(VERSION 3.28)
execute_process(COMMAND "${PROGRAM}" --list RESULT_VARIABLE _rc OUTPUT_VARIABLE _out)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${PROGRAM} --list exited ${_rc}")
endif()
string(REPLACE "\r" "" _out "${_out}")
string(STRIP "${_out}" _out)
string(REPLACE "\n" ";" _compiled "${_out}")

set(_failures "")
foreach(_t IN LISTS _compiled)
    if(NOT _t IN_LIST REGISTERED)
        string(APPEND _failures "\n  compiled but not registered with ctest: ${_t}")
    endif()
endforeach()
foreach(_t IN LISTS REGISTERED)
    if(NOT _t IN_LIST _compiled)
        string(APPEND _failures "\n  registered with ctest but not compiled: ${_t}")
    endif()
endforeach()
list(LENGTH _compiled _n)
message(STATUS "${_n} compiled tests, each registered")
if(_failures)
    message(FATAL_ERROR "the unit tests and ctest disagree:${_failures}")
endif()
