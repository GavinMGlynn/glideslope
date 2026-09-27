# Which tests one CI shard runs, chosen by what each test costs.
#
#   cmake -DBUILD=build/linux-debug -DCOSTS=tests/ci_costs/linux-debug.txt
#         -DSHARD=2 -DOF=7 -DJOBS=4 -DOUT=shard.txt -P tools/ci_shard.cmake
#   ctest --preset linux-debug -I shard.txt
#
# Every test of the build is dealt to one of OF shards, longest first, each to
# the shard with the least work so far (longest-processing-time first). A test
# that holds the whole runner - RUN_SERIAL, or PROCESSORS above one - weighs
# its time multiplied by what it holds, because nothing runs beside it. What
# each test costs is COSTS, one "seconds name" a line, measured on CI by
# tools/ci_test_costs.py; a test it does not name costs DEFAULT seconds, and is
# named here so the costs can be measured again.
#
# Dealing by number (`-I k,,n`) put the slow tests where their numbers fell:
# on 2026-09-27 one Linux debug shard took 25 minutes while another took 18.
#
# **Coverage is asserted.** Every shard deals the same list the same way, so
# the OF shards between them run every test once; the script counts that
# every test was dealt exactly once and fails if not. OUT is ctest's -I file:
# the test numbers of shard SHARD. The costs are also written where ctest
# reads them (Testing/Temporary/CTestCostData.txt), so inside the shard the
# longest tests start first. A fixture's setup is pulled in by ctest wherever
# a test needs it, as with -I k,,n.

foreach(_v BUILD COSTS SHARD OF JOBS OUT)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "ci_shard: -D${_v}= is needed")
    endif()
endforeach()
if(NOT DEFINED DEFAULT)
    set(DEFAULT 60)
endif()
if(SHARD LESS 1 OR SHARD GREATER OF)
    message(FATAL_ERROR "ci_shard: shard ${SHARD} of ${OF} does not exist")
endif()

execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} --show-only=json-v1
    OUTPUT_VARIABLE _json RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "ci_shard: ctest could not list the tests of ${BUILD}")
endif()
string(JSON _count LENGTH "${_json}" tests)
if(_count EQUAL 0)
    message(FATAL_ERROR "ci_shard: ${BUILD} has no tests")
endif()

file(STRINGS ${COSTS} _lines)
foreach(_line IN LISTS _lines)
    if(_line MATCHES "^([0-9]+) ([^ ]+)$")
        set(_cost_${CMAKE_MATCH_2} ${CMAKE_MATCH_1})
    endif()
endforeach()

# Each test's weight, as "weight number" padded so that sorting the text sorts
# the numbers: heaviest first, and between equals the lower number first.
set(_keys "")
set(_unmeasured "")
set(_costdata "")
math(EXPR _last "${_count} - 1")
foreach(_i RANGE ${_last})
    math(EXPR _number "${_i} + 1")
    string(JSON _name GET "${_json}" tests ${_i} name)
    if(DEFINED _cost_${_name})
        set(_cost ${_cost_${_name}})
    else()
        set(_cost ${DEFAULT})
        list(APPEND _unmeasured ${_name})
    endif()
    string(APPEND _costdata "${_name} 1 ${_cost}\n")
    set(_holds 1)
    string(JSON _props ERROR_VARIABLE _none GET "${_json}" tests ${_i} properties)
    if(NOT _none)
        string(JSON _np LENGTH "${_props}")
        if(_np GREATER 0)
            math(EXPR _plast "${_np} - 1")
            foreach(_p RANGE ${_plast})
                string(JSON _pn GET "${_props}" ${_p} name)
                string(JSON _pv GET "${_props}" ${_p} value)
                if(_pn STREQUAL "RUN_SERIAL" AND _pv)
                    set(_holds ${JOBS})
                elseif(_pn STREQUAL "PROCESSORS" AND _pv GREATER 1 AND _holds LESS _pv)
                    set(_holds ${_pv})
                endif()
            endforeach()
        endif()
    endif()
    if(_holds GREATER JOBS)
        set(_holds ${JOBS})
    endif()
    math(EXPR _weight "${_cost} * ${_holds}")
    math(EXPR _rank "999999 - ${_number}")
    string(LENGTH "${_weight}" _wl)
    string(LENGTH "${_rank}" _rl)
    math(EXPR _wpad "12 - ${_wl}")
    math(EXPR _rpad "6 - ${_rl}")
    string(REPEAT "0" ${_wpad} _wz)
    string(REPEAT "0" ${_rpad} _rz)
    list(APPEND _keys "${_wz}${_weight}.${_rz}${_rank}.${_number}")
endforeach()
list(SORT _keys ORDER DESCENDING)

math(EXPR _olast "${OF} - 1")
foreach(_s RANGE ${_olast})
    set(_load_${_s} 0)
    set(_tests_${_s} "")
endforeach()
foreach(_key IN LISTS _keys)
    string(REGEX MATCH "^0*([0-9]+)\\.[0-9]+\\.([0-9]+)$" _ "${_key}")
    set(_weight ${CMAKE_MATCH_1})
    set(_number ${CMAKE_MATCH_2})
    set(_least 0)
    foreach(_s RANGE ${_olast})
        if(_load_${_s} LESS _load_${_least})
            set(_least ${_s})
        endif()
    endforeach()
    math(EXPR _load_${_least} "${_load_${_least}} + ${_weight}")
    list(APPEND _tests_${_least} ${_number})
endforeach()

# Every test dealt, and to one shard only.
set(_dealt "")
foreach(_s RANGE ${_olast})
    list(APPEND _dealt ${_tests_${_s}})
    list(LENGTH _tests_${_s} _n)
    math(EXPR _k "${_s} + 1")
    math(EXPR _minutes10 "${_load_${_s}} * 10 / (${JOBS} * 60)")
    string(REGEX REPLACE "([0-9])$" ".\\1" _minutes "${_minutes10}")
    message(STATUS "ci_shard: shard ${_k} of ${OF}: ${_n} tests, about ${_minutes} min of work over ${JOBS} at a time")
    if(_n EQUAL 0)
        message(FATAL_ERROR "ci_shard: shard ${_k} of ${OF} was dealt no tests")
    endif()
endforeach()
list(LENGTH _dealt _dealt_n)
list(REMOVE_DUPLICATES _dealt)
list(LENGTH _dealt _distinct_n)
if(NOT _dealt_n EQUAL _count OR NOT _distinct_n EQUAL _count)
    message(FATAL_ERROR "ci_shard: ${_count} tests, but ${_dealt_n} dealt and ${_distinct_n} distinct")
endif()
message(STATUS "ci_shard: all ${_count} tests dealt, each to one of ${OF} shards")

if(_unmeasured)
    list(LENGTH _unmeasured _un)
    list(JOIN _unmeasured "\n  " _names)
    message(STATUS "ci_shard: ${_un} tests have no measured cost in ${COSTS} and count ${DEFAULT} s:\n  ${_names}")
endif()

math(EXPR _mine "${SHARD} - 1")
list(SORT _tests_${_mine} COMPARE NATURAL)
list(JOIN _tests_${_mine} "," _numbers)
file(WRITE ${OUT} "0,0,0,${_numbers}\n")
file(WRITE ${BUILD}/Testing/Temporary/CTestCostData.txt "${_costdata}---\n")
