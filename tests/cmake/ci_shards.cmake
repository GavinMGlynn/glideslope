# Every CI test shard's dealing, for every preset CI shards, runs every test
# of a build once, and the workflow's matrix has one row for each shard.
#
#   -DROOT=<source> -DBUILD=<a configured build> -DWORK=<scratch> -P ci_shards.cmake
#
# CI deals each preset's tests with tools/ci_shard.cmake (see it). Here, for
# each preset the workflow shards, the dealing is done for every one of its
# shards, from that preset's cost table, over this build's tests - which are
# not that preset's list, but the dealing does not care whose list it is
# dealt - and the shards' -I files read back: every test number of the build
# must be in exactly one. The matrix rows are read from the workflow: each
# preset's rows must be shards 1 to n, each once, all saying "of: n", and each
# test job must call the dealing with its row's shard and of. How many tests a
# shard runs at once is not in the workflow at all: the dealing reads it from
# the test preset, so the two cannot disagree.

cmake_minimum_required(VERSION 3.28)
foreach(_v ROOT BUILD WORK)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "-D${_v}= is needed")
    endif()
endforeach()

file(READ ${ROOT}/.github/workflows/ci.yml _yml)
string(REGEX MATCHALL "- { preset: [a-z-]+, shard: [0-9]+, of: [0-9]+ }" _rows "${_yml}")
set(_presets "")
foreach(_row IN LISTS _rows)
    string(REGEX MATCH "preset: ([a-z-]+), shard: ([0-9]+), of: ([0-9]+)" _ "${_row}")
    set(_p ${CMAKE_MATCH_1})
    if(NOT DEFINED _of_${_p})
        list(APPEND _presets ${_p})
        set(_of_${_p} ${CMAKE_MATCH_3})
        set(_shards_${_p} "")
    elseif(NOT _of_${_p} EQUAL CMAKE_MATCH_3)
        message(FATAL_ERROR "${_p}: rows say of ${_of_${_p}} and of ${CMAKE_MATCH_3}")
    endif()
    list(APPEND _shards_${_p} ${CMAKE_MATCH_2})
endforeach()
if(NOT _presets)
    message(FATAL_ERROR "no shard rows in ci.yml")
endif()

# Every test job deals with its own row's figures, and passes the file to ctest.
string(REGEX MATCHALL "cmake -DPRESET=[^\n]*-P tools/ci_shard.cmake" _calls "${_yml}")
string(REGEX MATCHALL "ctest --preset \\\${{ matrix.preset }} -I [^\n]*shard.txt" _ctests "${_yml}")
string(REGEX MATCHALL "include:\n *- { preset: [a-z-]+, shard:" _jobs "${_yml}")
list(LENGTH _calls _ncalls)
list(LENGTH _ctests _nctests)
list(LENGTH _jobs _njobs)
if(NOT _ncalls EQUAL _njobs OR NOT _nctests EQUAL _njobs)
    message(FATAL_ERROR "${_njobs} sharded jobs, but ${_ncalls} calls of the dealing and ${_nctests} ctest runs of its file")
endif()
foreach(_call IN LISTS _calls)
    if(NOT _call MATCHES "-DPRESET=\\\${{ matrix.preset }} -DSHARD=\\\${{ matrix.shard }} -DOF=\\\${{ matrix.of }} -DOUT=shard.txt ")
        message(FATAL_ERROR "a test job deals other than its own row: ${_call}")
    endif()
endforeach()

execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} -N
    OUTPUT_VARIABLE _listing RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0 OR NOT _listing MATCHES "Total Tests: ([0-9]+)")
    message(FATAL_ERROR "ctest could not list ${BUILD}")
endif()
set(_count ${CMAKE_MATCH_1})

set(_dealings 0)
foreach(_p IN LISTS _presets)
    set(_want "")
    foreach(_k RANGE 1 ${_of_${_p}})
        list(APPEND _want ${_k})
    endforeach()
    set(_have ${_shards_${_p}})
    list(SORT _have COMPARE NATURAL)
    if(NOT _have STREQUAL _want)
        message(FATAL_ERROR "${_p}: the matrix has shards ${_have}, not 1 to ${_of_${_p}} once each")
    endif()

    set(_dir ${WORK}/${_p})
    file(REMOVE_RECURSE ${_dir})
    file(MAKE_DIRECTORY ${_dir})
    execute_process(COMMAND ${CMAKE_COMMAND} -DPRESET=${_p} -DBUILD=${BUILD} -DOF=${_of_${_p}}
            -DOUT_ALL=${_dir} -P ${ROOT}/tools/ci_shard.cmake
        OUTPUT_VARIABLE _out ERROR_VARIABLE _out RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${_p}: the dealing failed:\n${_out}")
    endif()
    set(_dealt "")
    foreach(_k RANGE 1 ${_of_${_p}})
        file(READ ${_dir}/shard_${_k}.txt _file)
        if(NOT _file MATCHES "^0,0,0,([0-9,]+)\n$")
            message(FATAL_ERROR "${_p} shard ${_k}: not an -I file of test numbers: ${_file}")
        endif()
        string(REPLACE "," ";" _numbers "${CMAKE_MATCH_1}")
        list(APPEND _dealt ${_numbers})
        math(EXPR _dealings "${_dealings} + 1")
    endforeach()
    list(LENGTH _dealt _n)
    set(_distinct ${_dealt})
    list(REMOVE_DUPLICATES _distinct)
    list(LENGTH _distinct _nd)
    set(_all "")
    foreach(_t RANGE 1 ${_count})
        list(APPEND _all ${_t})
    endforeach()
    set(_missing ${_all})
    list(REMOVE_ITEM _missing ${_dealt})
    if(NOT _n EQUAL _count OR NOT _nd EQUAL _count OR _missing)
        message(FATAL_ERROR "${_p}: ${_count} tests, ${_n} dealt, ${_nd} distinct; missing: ${_missing}")
    endif()
    message(STATUS "${_p}: ${_of_${_p}} shards, all ${_count} tests dealt once")
endforeach()

list(LENGTH _presets _np)
list(LENGTH _rows _nrows)
if(NOT _dealings EQUAL _nrows)
    message(FATAL_ERROR "${_nrows} matrix rows, but ${_dealings} shards dealt")
endif()
message(STATUS "${_np} presets, ${_nrows} shards: every shard dealt, every test in exactly one")
