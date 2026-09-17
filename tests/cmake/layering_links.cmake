# layering_links.cmake - SDL reaching the simulation target by every route there
# is, each of which the link check must refuse, and one tree with no SDL in it,
# which it must accept.
#
#   cmake -DGLIDESLOPE_ROOT=<source dir> -DWORK=<scratch dir>
#         -DGENERATOR=<generator> -DCXX=<compiler> -P layering_links.cmake
#
# One build directory for every case: only the first configure pays for
# detecting the compiler.

set(_refused direct through-a-public-dependency through-a-private-dependency
             through-an-interface-library by-library-name by-file-path)
set(_accepted none)
list(LENGTH _refused _nr)
list(LENGTH _accepted _na)
math(EXPR _expected "${_nr} + ${_na}")

set(_failures "")
set(_walked 0)
file(REMOVE_RECURSE "${WORK}")

foreach(_case IN LISTS _refused _accepted)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${GLIDESLOPE_ROOT}/tests/layering_link"
                -B "${WORK}" -G "${GENERATOR}" "-DCMAKE_CXX_COMPILER=${CXX}"
                "-DGLIDESLOPE_ROOT=${GLIDESLOPE_ROOT}" "-DCASE=${_case}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_all "${_out}${_err}")
    math(EXPR _walked "${_walked} + 1")
    list(FIND _refused "${_case}" _should_refuse)
    if(NOT _should_refuse EQUAL -1)
        if(_rc EQUAL 0)
            string(APPEND _failures "\n  ${_case}: was accepted, should have been refused")
        elseif(NOT _all MATCHES "glideslope_sim links SDL")
            string(APPEND _failures "\n  ${_case}: failed, but not with the link check\n${_all}")
        endif()
    elseif(NOT _rc EQUAL 0)
        string(APPEND _failures "\n  ${_case}: was refused, should have been accepted\n${_all}")
    endif()
endforeach()

message(STATUS "walked ${_walked} of ${_expected} cases")
if(NOT _walked EQUAL _expected)
    string(APPEND _failures "\n  walked ${_walked} cases, expected ${_expected}")
endif()
if(_failures)
    message(FATAL_ERROR "the link check is wrong:${_failures}")
endif()
