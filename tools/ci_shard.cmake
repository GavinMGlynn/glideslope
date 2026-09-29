# Which tests one CI shard runs, chosen by what each test costs.
#
#   cmake -DPRESET=linux-debug -DSHARD=2 -DOF=7 -DOUT=shard.txt -P tools/ci_shard.cmake
#   ctest --preset linux-debug -I "$PWD/shard.txt"
#
# Every test of build/PRESET is dealt to one of OF shards, longest first, each
# to the shard with the least work so far (longest-processing-time first).
# What each test costs is tests/ci_costs/PRESET.txt, one "seconds name" a line,
# measured on CI by tools/ci_test_costs.py; a test it does not name costs
# DEFAULT seconds and is warned of, and a name it has that the build does not
# is reported, so the table can be measured again.
#
# - A test that holds the runner - RUN_SERIAL, or PROCESSORS above one - weighs
#   its time multiplied by what it holds, because nothing runs beside it. How
#   many tests a shard runs at once is the test preset's "jobs", read from
#   CMakePresets.json, so the two cannot disagree.
# - **A fixture and the tests that need it are dealt together**, weighing their
#   sum: ctest runs a fixture's setup in every shard that has a test needing
#   it, so dealt apart the cockpit view that the other six views hold to ran
#   six times on Linux debug. A fixture whose tests together weigh more than
#   half a shard's fair share (the downloads, the model sources: seconds to set
#   up, and needed by a third of the suite) is left to be set up again wherever
#   it is needed, since dealt as one its tests would be a shard's work or more
#   in one lump.
#
# Dealing by number (`-I k,,n`) put the slow tests where their numbers fell:
# on 2026-09-27 one Linux debug shard took 25 minutes while another took 18.
#
# **Coverage is asserted.** Every shard deals the same list the same way, so
# the OF shards between them run every test once; the script counts that
# every test was dealt exactly once and that no shard is empty, and fails if
# not. OUT is ctest's -I file: the test numbers of shard SHARD. With OUT_ALL, a
# directory, every shard's file is written there instead (shard_1.txt ...),
# for tests/cmake/ci_shards_cover_every_test.cmake. BUILD overrides the build
# tree. With OUT, the costs are also written where ctest reads them
# (Testing/Temporary/CTestCostData.txt), so inside a shard the longest tests
# start first.

cmake_minimum_required(VERSION 3.28)
get_filename_component(_source ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
foreach(_v PRESET OF)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "ci_shard: -D${_v}= is needed")
    endif()
endforeach()
if(NOT DEFINED OUT_ALL AND (NOT DEFINED OUT OR NOT DEFINED SHARD))
    message(FATAL_ERROR "ci_shard: -DSHARD= and -DOUT=, or -DOUT_ALL=, are needed")
endif()
if(NOT DEFINED BUILD)
    set(BUILD ${_source}/build/${PRESET})
endif()
if(NOT DEFINED COSTS)
    set(COSTS ${_source}/tests/ci_costs/${PRESET}.txt)
endif()
if(NOT DEFINED DEFAULT)
    set(DEFAULT 60)
endif()
if(DEFINED SHARD AND (SHARD LESS 1 OR SHARD GREATER OF))
    message(FATAL_ERROR "ci_shard: shard ${SHARD} of ${OF} does not exist")
endif()

# The test preset's jobs, through what it inherits.
file(READ ${_source}/CMakePresets.json _presets)
string(JSON _np LENGTH "${_presets}" testPresets)
math(EXPR _nplast "${_np} - 1")
set(_want ${PRESET})
set(JOBS "")
while(JOBS STREQUAL "" AND NOT _want STREQUAL "")
    set(_found "")
    foreach(_i RANGE ${_nplast})
        string(JSON _pname GET "${_presets}" testPresets ${_i} name)
        if(_pname STREQUAL _want)
            set(_found ${_i})
        endif()
    endforeach()
    if(_found STREQUAL "")
        message(FATAL_ERROR "ci_shard: CMakePresets.json has no test preset ${_want}")
    endif()
    string(JSON JOBS ERROR_VARIABLE _e GET "${_presets}" testPresets ${_found} execution jobs)
    if(_e)
        set(JOBS "")
    endif()
    string(JSON _want ERROR_VARIABLE _e GET "${_presets}" testPresets ${_found} inherits)
    if(_e)
        set(_want "")
    endif()
endwhile()
if(JOBS STREQUAL "")
    message(FATAL_ERROR "ci_shard: test preset ${PRESET} says no jobs")
endif()

execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} --show-only=json-v1
    OUTPUT_VARIABLE _json RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "ci_shard: ctest could not list the tests of ${BUILD}")
endif()
string(JSON _tests GET "${_json}" tests)
set(_json "")
string(JSON _count LENGTH "${_tests}")
if(_count EQUAL 0)
    message(FATAL_ERROR "ci_shard: ${BUILD} has no tests")
endif()

file(STRINGS ${COSTS} _lines)
set(_tabled "")
foreach(_line IN LISTS _lines)
    if(_line MATCHES "^([0-9]+) ([^ ]+)$")
        set(_cost_${CMAKE_MATCH_2} ${CMAKE_MATCH_1})
        list(APPEND _tabled ${CMAKE_MATCH_2})
    endif()
endforeach()

# Each test: its cost, what it holds, and the fixtures it sets up or needs.
set(_unmeasured "")
set(_costdata "")
set(_names "")
set(_total 0)
math(EXPR _last "${_count} - 1")
foreach(_i RANGE ${_last})
    math(EXPR _n "${_i} + 1")
    string(JSON _t GET "${_tests}" ${_i})
    string(JSON _name GET "${_t}" name)
    list(APPEND _names ${_name})
    if(DEFINED _cost_${_name})
        set(_c ${_cost_${_name}})
    else()
        set(_c ${DEFAULT})
        list(APPEND _unmeasured ${_name})
    endif()
    string(APPEND _costdata "${_name} 1 ${_c}\n")
    set(_holds 1)
    set(_fix_${_n} "")
    string(JSON _props ERROR_VARIABLE _e GET "${_t}" properties)
    if(NOT _e)
        string(JSON _nprops LENGTH "${_props}")
        math(EXPR _plast "${_nprops} - 1")
        foreach(_p RANGE ${_plast})
            string(JSON _pn GET "${_props}" ${_p} name)
            string(JSON _pv GET "${_props}" ${_p} value)
            if(_pn STREQUAL "RUN_SERIAL" AND _pv)
                set(_holds ${JOBS})
            elseif(_pn STREQUAL "PROCESSORS" AND _pv GREATER _holds)
                set(_holds ${_pv})
            elseif(_pn MATCHES "^FIXTURES_(SETUP|REQUIRED|CLEANUP)$")
                string(JSON _nv LENGTH "${_pv}")
                math(EXPR _vlast "${_nv} - 1")
                foreach(_v RANGE ${_vlast})
                    string(JSON _f GET "${_pv}" ${_v})
                    list(APPEND _fix_${_n} ${_f})
                endforeach()
            endif()
        endforeach()
    endif()
    if(_holds GREATER JOBS)
        set(_holds ${JOBS})
    endif()
    math(EXPR _w_${_n} "${_c} * ${_holds}")
    math(EXPR _total "${_total} + ${_w_${_n}}")
    list(REMOVE_DUPLICATES _fix_${_n})
    foreach(_f IN LISTS _fix_${_n})
        if(NOT DEFINED _fweight_${_f})
            set(_fweight_${_f} 0)
        endif()
        math(EXPR _fweight_${_f} "${_fweight_${_f}} + ${_w_${_n}}")
    endforeach()
endforeach()

# The units dealt: a test alone, or a fixture not too wide to deal whole with
# every test that sets it up, needs it or cleans it up. Two such fixtures that
# share a test are one unit.
set(_fixtures "")
foreach(_n RANGE 1 ${_count})
    set(_whole "")
    foreach(_f IN LISTS _fix_${_n})
        math(EXPR _wide "${_fweight_${_f}} * 2 * ${OF}")
        if(_wide LESS_EQUAL _total)
            if(NOT DEFINED _label_${_f})
                set(_label_${_f} ${_f})
                list(APPEND _fixtures ${_f})
            endif()
            list(APPEND _whole ${_label_${_f}})
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _whole)
    list(LENGTH _whole _nw)
    if(_nw GREATER 1)
        list(GET _whole 0 _keep)
        foreach(_g IN LISTS _fixtures)
            if(_label_${_g} IN_LIST _whole)
                set(_label_${_g} ${_keep})
            endif()
        endforeach()
    endif()
endforeach()
set(_units "")
foreach(_n RANGE 1 ${_count})
    set(_unit t${_n})
    foreach(_f IN LISTS _fix_${_n})
        if(DEFINED _label_${_f})
            set(_unit f${_label_${_f}})
        endif()
    endforeach()
    if(NOT DEFINED _members_${_unit})
        list(APPEND _units ${_unit})
        set(_members_${_unit} "")
        set(_weight_${_unit} 0)
        set(_first_${_unit} ${_n})
    endif()
    list(APPEND _members_${_unit} ${_n})
    math(EXPR _weight_${_unit} "${_weight_${_unit}} + ${_w_${_n}}")
endforeach()

# Sorted as text, padded: heaviest first, and between equals the first test first.
set(_keys "")
foreach(_u IN LISTS _units)
    math(EXPR _rank "999999 - ${_first_${_u}}")
    string(LENGTH "${_weight_${_u}}" _wl)
    string(LENGTH "${_rank}" _rl)
    math(EXPR _wpad "12 - ${_wl}")
    math(EXPR _rpad "6 - ${_rl}")
    string(REPEAT "0" ${_wpad} _wz)
    string(REPEAT "0" ${_rpad} _rz)
    list(APPEND _keys "${_wz}${_weight_${_u}}.${_rz}${_rank}.${_u}")
endforeach()
list(SORT _keys ORDER DESCENDING)

math(EXPR _olast "${OF} - 1")
foreach(_s RANGE ${_olast})
    set(_load_${_s} 0)
    set(_tests_${_s} "")
endforeach()
foreach(_key IN LISTS _keys)
    string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.(.+)$" _ "${_key}")
    set(_u ${CMAKE_MATCH_1})
    set(_least 0)
    foreach(_s RANGE ${_olast})
        if(_load_${_s} LESS _load_${_least})
            set(_least ${_s})
        endif()
    endforeach()
    math(EXPR _load_${_least} "${_load_${_least}} + ${_weight_${_u}}")
    list(APPEND _tests_${_least} ${_members_${_u}})
endforeach()

# Every test dealt, and to one shard only.
set(_dealt "")
foreach(_s RANGE ${_olast})
    list(APPEND _dealt ${_tests_${_s}})
    list(LENGTH _tests_${_s} _n)
    math(EXPR _k "${_s} + 1")
    math(EXPR _minutes10 "${_load_${_s}} * 10 / (${JOBS} * 60)")
    string(REGEX REPLACE "([0-9])$" ".\\1" _minutes "${_minutes10}")
    string(REGEX REPLACE "^\\." "0." _minutes "${_minutes}")
    message(STATUS "ci_shard: ${PRESET} shard ${_k} of ${OF}: ${_n} tests, about ${_minutes} min of work at ${JOBS} at a time")
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

# What the table does not know, and what it knows that is gone: a warning on
# the run's page, so the table is measured again.
if(_unmeasured)
    list(LENGTH _unmeasured _un)
    list(JOIN _unmeasured ", " _list)
    execute_process(COMMAND ${CMAKE_COMMAND} -E echo
        "::warning title=Unmeasured tests::${_un} tests of ${PRESET} are not in tests/ci_costs/${PRESET}.txt and count ${DEFAULT} s each (tools/ci_test_costs.py): ${_list}")
endif()
set(_gone ${_tabled})
list(REMOVE_ITEM _gone ${_names})
if(_gone)
    list(LENGTH _gone _gn)
    list(JOIN _gone ", " _list)
    execute_process(COMMAND ${CMAKE_COMMAND} -E echo
        "::warning title=Gone tests::${_gn} tests in tests/ci_costs/${PRESET}.txt are no longer tests of ${PRESET}: ${_list}")
endif()

if(DEFINED OUT_ALL)
    foreach(_s RANGE ${_olast})
        math(EXPR _k "${_s} + 1")
        list(SORT _tests_${_s} COMPARE NATURAL)
        list(JOIN _tests_${_s} "," _numbers)
        file(WRITE ${OUT_ALL}/shard_${_k}.txt "0,0,0,${_numbers}\n")
    endforeach()
else()
    math(EXPR _mine "${SHARD} - 1")
    list(SORT _tests_${_mine} COMPARE NATURAL)
    list(JOIN _tests_${_mine} "," _numbers)
    file(WRITE ${OUT} "0,0,0,${_numbers}\n")
    file(WRITE ${BUILD}/Testing/Temporary/CTestCostData.txt "${_costdata}---\n")
endif()
