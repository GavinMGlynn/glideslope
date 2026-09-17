# selftest_hash.cmake - the selftest's hash is a tripwire: steady run to run,
# and moved by a change to the physics.
#
#   cmake -DPROGRAM=<glideslope_cli> -DDATA=<data dir> -DWORK=<scratch dir>
#         -DCASE=<steady|physics-change|bad-log> -P selftest_hash.cmake
#
# steady          two runs print exactly the same thing, hash included
# physics-change  the selftest flown on an unchanged copy of the data prints the
#                 same hash as the data itself - so --data is really being read -
#                 and on a copy whose zero-lift drag is changed in its fourth
#                 decimal place, one line of the model, prints a different one
# bad-log         a log with a command the selftest does not know is refused,
#                 naming the file and the line

function(selftest out_var)
    execute_process(COMMAND "${PROGRAM}" ${ARGN} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out
                    ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${PROGRAM} ${ARGN} exited ${_rc}\n${_out}${_err}")
    endif()
    if(NOT _out MATCHES "hash ([0-9a-f]+)")
        message(FATAL_ERROR "no hash in the selftest's output:\n${_out}")
    endif()
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    set(${out_var}_output "${_out}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "steady")
    selftest(first selftest)
    selftest(second selftest)
    if(NOT first_output STREQUAL second_output)
        message(FATAL_ERROR "two selftest runs differ:\n${first_output}----\n${second_output}")
    endif()
    message(STATUS "hash ${first}, twice")

elseif(CASE STREQUAL "physics-change")
    file(REMOVE_RECURSE "${WORK}")
    file(COPY "${DATA}/" DESTINATION "${WORK}/unchanged")
    file(COPY "${DATA}/" DESTINATION "${WORK}/changed")
    set(_model "${WORK}/changed/jsbsim/aircraft/c172p/c172p.xml")
    file(READ "${_model}" _text)
    string(REGEX REPLACE "(<description>Drag_at_zero_lift</description>[^<]*<product>[^<]*<property>[^<]*</property>[^<]*<property>[^<]*</property>[^<]*<value>)0\\.031(</value>)"
           "\\10.0311\\2" _changed "${_text}")
    if(_changed STREQUAL _text)
        message(FATAL_ERROR "could not find the zero-lift drag of 0.031 to change in ${_model}")
    endif()
    file(WRITE "${_model}" "${_changed}")

    selftest(original selftest)
    selftest(unchanged --data "${WORK}/unchanged" selftest)
    selftest(changed --data "${WORK}/changed" selftest)
    message(STATUS "hash ${original}; unchanged copy ${unchanged}; drag 0.031 -> 0.0311 ${changed}")
    if(NOT unchanged STREQUAL original)
        message(FATAL_ERROR "an unchanged copy of the data gave a different hash")
    endif()
    if(changed STREQUAL original)
        message(FATAL_ERROR "changing the zero-lift drag did not move the hash")
    endif()

elseif(CASE STREQUAL "bad-log")
    file(REMOVE_RECURSE "${WORK}")
    file(COPY "${DATA}/" DESTINATION "${WORK}/data")
    file(APPEND "${WORK}/data/selftest/c172p.log" "")
    file(READ "${WORK}/data/selftest/c172p.log" _log)
    string(REPLACE "90   bank 20" "90   barrel_roll 1" _bad "${_log}")
    if(_bad STREQUAL _log)
        message(FATAL_ERROR "could not find the line to break in the selftest log")
    endif()
    file(WRITE "${WORK}/data/selftest/c172p.log" "${_bad}")
    execute_process(COMMAND "${PROGRAM}" --data "${WORK}/data" selftest
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 1 OR NOT _err MATCHES "c172p\\.log:[0-9]+: unknown command barrel_roll")
        message(FATAL_ERROR "the broken log was not refused with its line (exit ${_rc}):\n${_out}${_err}")
    endif()

else()
    message(FATAL_ERROR "selftest_hash.cmake: unknown CASE ${CASE}")
endif()
