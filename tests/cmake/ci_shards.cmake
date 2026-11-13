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
# must be in exactly one. The matrix rows are read from the workflow, job by
# job - one preset may be sharded by two jobs (linux-release on Ubuntu and on
# Rocky 9), each its own way: each job's rows of a preset must be shards 1 to
# n, each once, all saying "of: n", and each test job must call the dealing
# with its row's shard and of. How many tests a shard runs at once is not in
# the workflow at all: the dealing reads it from the test preset, so the two
# cannot disagree.

cmake_minimum_required(VERSION 3.28)
foreach(_v ROOT BUILD WORK)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "-D${_v}= is needed")
    endif()
endforeach()

file(READ ${ROOT}/.github/workflows/ci.yml _yml)
string(REGEX MATCHALL "- { preset: [a-z-]+, shard: [0-9]+, of: [0-9]+(, minutes: [0-9]+)? }" _rows "${_yml}")
# Each sharded job's matrix: its "include:" and the rows under it. A dealing is
# named by the job's place among them and the preset, "2_linux-release" (no
# colon: it is part of variable names).
string(REGEX MATCHALL "include:\n( *- { preset: [a-z-]+, shard: [0-9]+, of: [0-9]+(, minutes: [0-9]+)? }\n)+" _blocks "${_yml}")
set(_presets "")
set(_job 0)
foreach(_block IN LISTS _blocks)
    math(EXPR _job "${_job} + 1")
    # The job's limit: the last timeout-minutes before its rows.
    string(FIND "${_yml}" "${_block}" _at)
    string(SUBSTRING "${_yml}" 0 ${_at} _head)
    string(REGEX MATCHALL "timeout-minutes: [^\n]+" _limits "${_head}")
    list(GET _limits -1 _limit)
    string(REGEX REPLACE "^timeout-minutes: " "" _limit_${_job} "${_limit}")
    string(REGEX MATCHALL "- { preset: [a-z-]+, shard: [0-9]+, of: [0-9]+(, minutes: [0-9]+)? }" _block_rows "${_block}")
    foreach(_row IN LISTS _block_rows)
        string(REGEX MATCH "preset: ([a-z-]+), shard: ([0-9]+), of: ([0-9]+)(, minutes: ([0-9]+))?" _ "${_row}")
        set(_p ${_job}_${CMAKE_MATCH_1})
        set(_preset_${_p} ${CMAKE_MATCH_1})
        set(_job_of_${_p} ${_job})
        if(NOT DEFINED _of_${_p})
            set(_minutes_${_p} "${CMAKE_MATCH_5}")
        elseif(NOT "${_minutes_${_p}}" STREQUAL "${CMAKE_MATCH_5}")
            message(FATAL_ERROR "${_p}: rows say minutes '${_minutes_${_p}}' and '${CMAKE_MATCH_5}'")
        endif()
        if(NOT DEFINED _of_${_p})
            list(APPEND _presets ${_p})
            set(_of_${_p} ${CMAKE_MATCH_3})
            set(_shards_${_p} "")
        elseif(NOT _of_${_p} EQUAL CMAKE_MATCH_3)
            message(FATAL_ERROR "${_p}: rows say of ${_of_${_p}} and of ${CMAKE_MATCH_3}")
        endif()
        list(APPEND _shards_${_p} ${CMAKE_MATCH_2})
    endforeach()
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

# **The nightly label is asserted.** Every test labelled nightly is named, with
# its reason and runs, in nightly_tests.cmake, and nothing named there is
# unlabelled; and every test job of a pull request leaves the label out.
# -FA: a nightly test's fixture setup (the downloads) is not itself nightly.
include(${CMAKE_CURRENT_LIST_DIR}/nightly_tests.cmake)
execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} -N -L "^nightly$" -FA ".*"
    OUTPUT_VARIABLE _nl RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0 OR NOT _nl MATCHES "Total Tests: ([0-9]+)")
    message(FATAL_ERROR "ctest could not list the nightly tests of ${BUILD}")
endif()
set(_nnight ${CMAKE_MATCH_1})
string(REGEX MATCHALL "Test +#[0-9]+: [^\n]+" _nlines "${_nl}")
set(_labelled "")
foreach(_l IN LISTS _nlines)
    string(REGEX REPLACE "^Test +#[0-9]+: " "" _l "${_l}")
    string(STRIP "${_l}" _l)
    list(APPEND _labelled ${_l})
endforeach()
set(_named "")
foreach(_e IN LISTS NIGHTLY_TESTS)
    string(REPLACE "|" ";" _f "${_e}")
    list(LENGTH _f _nf)
    list(GET _f 0 _name)
    if(NOT _nf EQUAL 3)
        message(FATAL_ERROR "nightly_tests.cmake: ${_name} needs its reason and its CI runs")
    endif()
    list(APPEND _named ${_name})
endforeach()
list(LENGTH _named _nnamed)
list(SORT _named)
list(SORT _labelled)
if(NOT _nnamed EQUAL NIGHTLY_COUNT OR NOT _nnight EQUAL NIGHTLY_COUNT OR NOT _named STREQUAL _labelled)
    message(FATAL_ERROR "${NIGHTLY_COUNT} nightly tests expected; ${_nnamed} named, ${_nnight} labelled.\nnamed: ${_named}\nlabelled: ${_labelled}")
endif()
# Every test job's ctest line ends in one of three expressions, read whole: on
# a pull request a debug preset leaves out timing, nightly and gust_sweep and
# a release preset nightly and gust_sweep, on every other event a debug preset
# leaves out timing and gust_sweep and a release preset gust_sweep; or, for a
# job of release presets alone (Rocky 9), nightly and gust_sweep on a pull
# request and gust_sweep otherwise; or, for a gust sweep job, gust_sweep alone.
# (gust_sweep: tests too long for a pull request's run, tests/CMakeLists.txt;
# the sweep jobs run them nightly and on a dispatch, in shards of their own.)
set(_gate_any [=[${{ github.event_name == 'pull_request' && (endsWith(matrix.preset, '-debug') && '-LE "timing|nightly|gust_sweep"' || '-LE "nightly|gust_sweep"') || (endsWith(matrix.preset, '-debug') && '-LE "timing|gust_sweep"' || '-LE gust_sweep') }}]=])
set(_gate_release [=[${{ github.event_name == 'pull_request' && '-LE "nightly|gust_sweep"' || '-LE gust_sweep' }}]=])
set(_gate_sweep [=[-L gust_sweep]=])
string(REGEX MATCHALL "ctest --preset [^\n]*shard.txt[^\n]*" _test_runs "${_yml}")
set(_nnr 0)
set(_run_kinds "")
foreach(_run IN LISTS _test_runs)
    # The Linux jobs keep ctest's code for a step after it.
    string(REGEX REPLACE " \\|\\| rc=\\$\\?$" "" _run "${_run}")
    set(_gated FALSE)
    foreach(_kind IN ITEMS any release sweep)
        set(_gate "${_gate_${_kind}}")
        string(LENGTH "${_run}" _len)
        string(LENGTH "${_gate}" _glen)
        if(_len GREATER_EQUAL _glen)
            math(EXPR _from "${_len} - ${_glen}")
            string(SUBSTRING "${_run}" ${_from} ${_glen} _tail)
            if(_tail STREQUAL _gate)
                set(_gated TRUE)
                list(APPEND _run_kinds ${_kind})
            endif()
        endif()
    endforeach()
    if(NOT _gated)
        message(FATAL_ERROR "a test job's ctest line does not end in a gate that leaves the nightly label out of a pull request's run (and timing out of a debug preset's):\n${_run}")
    endif()
    math(EXPR _nnr "${_nnr} + 1")
endforeach()
if(NOT _nnr EQUAL _njobs)
    message(FATAL_ERROR "${_njobs} test jobs, but ${_nnr} ctest runs of a shard")
endif()
message(STATUS "${_nnight} nightly test, named with its reason; ${_nnr} of ${_njobs} test jobs leave it out of pull requests")

# **A shard job's sizing is checked on hosts of its own operating system**:
# every host deals every preset, from the tests of the build it runs in, and
# a host of another system lacks the preset's own tests (_on_metal,
# _on_direct3d12) - so a Linux host sized macos-debug's night at 31.1 minutes
# a shard where a macOS host, with them, dealt 34.7 (train run 43446046011).
# Each preset's coverage is asserted on every host; its shards' sizing only
# where its tests are all there, so every preset is sized on one list, its
# own, whichever host runs this. A pull request's full run has every system's
# hosts (CI passed); a draft's only Linux and Windows.
get_filename_component(_host_preset ${BUILD} NAME)
string(REGEX MATCH "^[a-z]+" _host_system "${_host_preset}")
if(NOT _host_system MATCHES "^(linux|macos|windows)$")
    message(FATAL_ERROR "the build ${BUILD} is of no system a preset names (${_host_preset})")
endif()
set(_sized_here "")

# **Every dealing lists what its run leaves out**: ctest numbers -I's tests
# among those its -LE leaves, so a dealing that listed the whole build had
# each shard run, from the first test left out on, a test after the one it
# was dealt (run 41415219089). Each job's call of the dealing passes, as
# EXCLUDE, the same regex its ctest line gives -LE.
set(_exclude_any [=["-DEXCLUDE=${{ github.event_name == 'pull_request' && (endsWith(matrix.preset, '-debug') && 'timing|nightly|gust_sweep' || 'nightly|gust_sweep') || (endsWith(matrix.preset, '-debug') && 'timing|gust_sweep' || 'gust_sweep') }}" -DELSEWHERE=ON -P tools/ci_shard.cmake]=])
set(_exclude_release [=["-DEXCLUDE=${{ github.event_name == 'pull_request' && 'nightly|gust_sweep' || 'gust_sweep' }}" -DELSEWHERE=ON -P tools/ci_shard.cmake]=])
set(_exclude_sweep [=["-DINCLUDE=gust_sweep" -DDEFAULT=longest -DMINUTES=${{ matrix.minutes }} -DELSEWHERE=ON -P tools/ci_shard.cmake]=])
set(_i 0)
foreach(_call IN LISTS _calls)
    list(GET _run_kinds ${_i} _kind)
    string(LENGTH "${_call}" _len)
    string(LENGTH "${_exclude_${_kind}}" _glen)
    set(_tail "")
    if(_len GREATER_EQUAL _glen)
        math(EXPR _from "${_len} - ${_glen}")
        string(SUBSTRING "${_call}" ${_from} ${_glen} _tail)
    endif()
    if(NOT _tail STREQUAL _exclude_${_kind})
        message(FATAL_ERROR "a test job's dealing does not leave out what its ctest line does (${_kind}):\n${_call}")
    endif()
    math(EXPR _i "${_i} + 1")
endforeach()

# **Each dealing, done as its run leaves tests out, is the run's list.** Every
# preset is dealt as a pull request's run of it leaves tests out - a debug
# preset timing and nightly, a release preset nightly - and each shard's -I
# file read back by ctest with that -LE: the tests it lists must be the ones
# the dealing named. Every other event's runs leave out timing and gust_sweep
# (debug) or gust_sweep (release): those are dealt once each, for the first
# preset of each kind - the dealing does not depend on the preset beyond its
# costs. A gust sweep job's preset is dealt the tests labelled gust_sweep
# alone, read back with -L, and sized: STRICT, so a shard holding more than
# three fifths of its row's minutes, or a longest test that half as long again
# is past them, fails here (tools/ci_shard.cmake). A shard job's dealing is
# sized the same way against its job's timeout-minutes, at three quarters: a
# Windows debug shard dealt 43 of its 45 minutes finished its last test 13 s
# before it was cut off (#367's run, 2026-11-12).
function(ci_filter way out)
    set(_le "")
    if(way STREQUAL "sweep")
        set(_le -L gust_sweep)
    elseif(NOT way STREQUAL "none")
        set(_le -LE "${way}")
    endif()
    set(${out} "${_le}" PARENT_SCOPE)
endfunction()
function(ci_count way out)
    ci_filter("${way}" _le)
    execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} -N ${_le}
        OUTPUT_VARIABLE _listing RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0 OR NOT _listing MATCHES "Total Tests: ([0-9]+)")
        message(FATAL_ERROR "ctest could not list ${BUILD}")
    endif()
    set(${out} ${CMAKE_MATCH_1} PARENT_SCOPE)
endfunction()

set(_dealings 0)
set(_other_debug "")
set(_other_release "")
set(_swept "")
set(_sharded "")
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
    string(REGEX REPLACE "_.*" "" _jobn ${_p})
    math(EXPR _jobi "${_jobn} - 1")
    list(GET _run_kinds ${_jobi} _job_kind)
    set(_deal_args "")
    if(_job_kind STREQUAL "sweep")
        if(_minutes_${_p} STREQUAL "")
            message(FATAL_ERROR "${_p}: a gust sweep job's rows need their minutes")
        endif()
        set(_ways "sweep")
        set(_deal_args -DINCLUDE=gust_sweep -DDEFAULT=longest -DMINUTES=${_minutes_${_p}} -DSTRICT=ON)
        list(APPEND _swept ${_preset_${_p}})
    elseif(NOT _minutes_${_p} STREQUAL "")
        message(FATAL_ERROR "${_p}: only a gust sweep job's rows say minutes")
    elseif(NOT _limit_${_jobn} MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${_p}: a shard job's limit is a number of minutes, not ${_limit_${_jobn}}")
    elseif(NOT _preset_${_p} MATCHES "^(linux|macos|windows)-")
        message(FATAL_ERROR "${_p}: preset ${_preset_${_p}} names no system, so no host would size it")
    elseif(_preset_${_p} MATCHES "-debug$")
        if(_preset_${_p} MATCHES "^${_host_system}-")
            set(_deal_args -DMINUTES=${_limit_${_jobn}} -DSHARE=75 -DSTRICT=ON)
            list(APPEND _sized_here ${_p})
        endif()
        list(APPEND _sharded ${_preset_${_p}})
        set(_ways "timing|nightly|gust_sweep")
        if(_other_debug STREQUAL "")
            set(_other_debug ${_p})
            list(APPEND _ways "timing|gust_sweep")
        endif()
    else()
        if(_preset_${_p} MATCHES "^${_host_system}-")
            set(_deal_args -DMINUTES=${_limit_${_jobn}} -DSHARE=75 -DSTRICT=ON)
            list(APPEND _sized_here ${_p})
        endif()
        list(APPEND _sharded ${_preset_${_p}})
        set(_ways "nightly|gust_sweep")
        if(_other_release STREQUAL "")
            set(_other_release ${_p})
            list(APPEND _ways "gust_sweep")
        endif()
    endif()
    # **A night's way is sized too, for every preset sized on this host**: a
    # night's run of a shard job leaves out less than a pull request's (the
    # nightly tests come in), so its shards hold more; read back above for
    # the first preset of each kind only, it is dealt here for each, at the
    # job's limit (STRICT). macOS debug, which no pull request runs, was
    # dealt 34.7 of its 33.75 minutes so on a macOS host (train run
    # 43446046011), and only a pull request's way had been checked.
    if(NOT _job_kind STREQUAL "sweep" AND _deal_args)
        if(_preset_${_p} MATCHES "-debug$")
            set(_night "timing|gust_sweep")
        else()
            set(_night "gust_sweep")
        endif()
        set(_ndir ${WORK}/${_p}_night_sized)
        file(REMOVE_RECURSE ${_ndir})
        file(MAKE_DIRECTORY ${_ndir})
        execute_process(COMMAND ${CMAKE_COMMAND} -DPRESET=${_preset_${_p}} -DBUILD=${BUILD} -DOF=${_of_${_p}}
                "-DEXCLUDE=${_night}" ${_deal_args} -DELSEWHERE=ON -DOUT_ALL=${_ndir}
                -P ${ROOT}/tools/ci_shard.cmake
            OUTPUT_VARIABLE _out ERROR_VARIABLE _out RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "${_p}, a night's way ('${_night}'): the dealing failed:\n${_out}")
        endif()
    endif()

    list(GET _ways 0 _first_way)
    foreach(_way IN LISTS _ways)
        set(_exclude "${_way}")
        if(_way STREQUAL "none" OR _way STREQUAL "sweep")
            set(_exclude "")
        endif()
        ci_count("${_way}" _count)
        string(REPLACE "|" "_" _wayname "${_way}")
        set(_dir ${WORK}/${_p}_${_wayname})
        file(REMOVE_RECURSE ${_dir})
        file(MAKE_DIRECTORY ${_dir})
        execute_process(COMMAND ${CMAKE_COMMAND} -DPRESET=${_preset_${_p}} -DBUILD=${BUILD} -DOF=${_of_${_p}}
                "-DEXCLUDE=${_exclude}" ${_deal_args} -DELSEWHERE=ON -DOUT_ALL=${_dir} -P ${ROOT}/tools/ci_shard.cmake
            OUTPUT_VARIABLE _out ERROR_VARIABLE _out RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "${_p}: the dealing failed:\n${_out}")
        endif()
        ci_filter("${_way}" _le)
        set(_dealt "")
        foreach(_k RANGE 1 ${_of_${_p}})
            file(READ ${_dir}/shard_${_k}.txt _file)
            if(NOT _file MATCHES "^0,0,0,([0-9,]+)\n$")
                message(FATAL_ERROR "${_p} shard ${_k}: not an -I file of test numbers: ${_file}")
            endif()
            string(REPLACE "," ";" _numbers "${CMAKE_MATCH_1}")
            list(APPEND _dealt ${_numbers})
            if(_way STREQUAL _first_way)
                math(EXPR _dealings "${_dealings} + 1")
            endif()
            # What ctest runs of the file, with no fixture's setup added.
            execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} -N
                    -I ${_dir}/shard_${_k}.txt ${_le} -FA ".*"
                OUTPUT_VARIABLE _ran RESULT_VARIABLE _rc)
            string(REGEX MATCHALL "Test +#[0-9]+: [^\n]+" _ran "${_ran}")
            set(_ran_names "")
            foreach(_l IN LISTS _ran)
                string(REGEX REPLACE "^Test +#[0-9]+: +" "" _l "${_l}")
                string(STRIP "${_l}" _l)
                list(APPEND _ran_names ${_l})
            endforeach()
            file(STRINGS ${_dir}/shard_${_k}.names _named)
            list(SORT _ran_names)
            list(SORT _named)
            if(NOT _ran_names STREQUAL _named)
                set(_extra ${_ran_names})
                list(REMOVE_ITEM _extra ${_named})
                set(_short ${_named})
                list(REMOVE_ITEM _short ${_ran_names})
                message(FATAL_ERROR "${_p} shard ${_k}, leaving out '${_exclude}': ctest runs "
                    "other tests than were dealt it.\nrun, not dealt: ${_extra}\ndealt, not run: ${_short}")
            endif()
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
            message(FATAL_ERROR "${_p}, leaving out '${_exclude}': ${_count} tests, ${_n} dealt, ${_nd} distinct; missing: ${_missing}")
        endif()
        set(_did_${_wayname} TRUE)
        message(STATUS "${_p}, leaving out '${_exclude}': ${_of_${_p}} shards, all ${_count} tests dealt once, each shard's run its dealing")
    endforeach()
endforeach()
# Every way a run leaves tests out, dealt and read back: four.
foreach(_wayname IN ITEMS timing_nightly_gust_sweep nightly_gust_sweep timing_gust_sweep gust_sweep sweep)
    if(NOT _did_${_wayname})
        message(FATAL_ERROR "no dealing left tests out as a run does by '${_wayname}'")
    endif()
endforeach()

list(LENGTH _sized_here _nsized)
if(_nsized EQUAL 0)
    message(FATAL_ERROR "no shard job of a ${_host_system} preset to size on this ${_host_system} host")
endif()
message(STATUS "shard jobs sized on this ${_host_system} host, both ways, at three quarters of their limits: ${_sized_here}")

# **The debug presets' gust sweep, a day at a time** (the owner's decision of
# 2026-11-13; tests/CMakeLists.txt labels each gust_sweep test sweep_day_1 to
# sweep_day_7). Each debug sweep job's matrix is its preset, its limit, its
# shard count and its shards, and the days a run gives it. Asserted:
#   - each such job deals and runs one day's label, `^sweep_day_N$`, at its
#     limit, every shard at most three quarters of it (SHARE=75, STRICT);
#   - each day 1 to 7, dealt into its shards and every shard's -I file read
#     back by ctest with that label, runs exactly the tests dealt it;
#   - the seven days between them are every gust_sweep test, each once.
set(_day_block_re "matrix:\n        preset: \\[([a-z-]+)\\]\n        minutes: \\[([0-9]+)\\]\n        of: \\[([0-9]+)\\]\n        shard: \\[([0-9, ]+)\\]\n        day: \\\${{ fromJSON\\(needs.sweep-days.outputs.days\\) }}\n")
string(REGEX MATCHALL "${_day_block_re}" _day_blocks "${_yml}")
string(REGEX MATCHALL "cmake \"-DINCLUDE=[^\n]*-P tools/ci_shard.cmake" _day_calls "${_yml}")
string(REGEX MATCHALL "ctest -L [^\n]*shard.txt[^\n]*" _day_runs "${_yml}")
set(_day_call [=[cmake "-DINCLUDE=^sweep_day_${{ matrix.day }}$" -DPRESET=${{ matrix.preset }} -DSHARD=${{ matrix.shard }} -DOF=${{ matrix.of }} -DOUT=shard.txt -DDEFAULT=longest -DMINUTES=${{ matrix.minutes }} -DSHARE=75 -DMODEL_DEBUG=ON -DELSEWHERE=ON -P tools/ci_shard.cmake]=])
list(LENGTH _day_blocks _ndb)
list(LENGTH _day_calls _ndc)
list(LENGTH _day_runs _ndr)
string(REGEX MATCHALL "\n  [a-z-]+-debug-sweep:\n" _day_jobs "${_yml}")
list(LENGTH _day_jobs _ndj)
if(_ndb EQUAL 0 OR NOT _ndb EQUAL _ndc OR NOT _ndb EQUAL _ndr OR NOT _ndb EQUAL _ndj)
    message(FATAL_ERROR "${_ndj} debug sweep jobs, ${_ndb} matrices of a day, ${_ndc} calls of a day's dealing, ${_ndr} ctest runs of a day")
endif()
foreach(_call IN LISTS _day_calls)
    if(NOT _call STREQUAL _day_call)
        message(FATAL_ERROR "a debug sweep job deals other than its day at its limit:\n${_call}")
    endif()
endforeach()
foreach(_run IN LISTS _day_runs)
    if(NOT _run MATCHES [=[^ctest -L "\^sweep_day_\${{ matrix\.day }}\$" --preset \${{ matrix\.preset }} -I ("\$PWD/shard\.txt"|%CD%\\shard\.txt)( \|\| rc=\$\?)?$]=])
        message(FATAL_ERROR "a debug sweep job runs other than its day:\n${_run}")
    endif()
endforeach()
set(_days_all "")
foreach(_day RANGE 1 7)
    execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} -N -L "^sweep_day_${_day}$" -FA ".*"
        OUTPUT_VARIABLE _listing)
    string(REGEX MATCHALL "Test +#[0-9]+: [^\n]+" _listing "${_listing}")
    foreach(_l IN LISTS _listing)
        string(REGEX REPLACE "^Test +#[0-9]+: +" "" _l "${_l}")
        string(STRIP "${_l}" _l)
        list(APPEND _days_all ${_l})
    endforeach()
endforeach()
execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} -N -L "^gust_sweep$" -FA ".*"
    OUTPUT_VARIABLE _listing)
string(REGEX MATCHALL "Test +#[0-9]+: [^\n]+" _listing "${_listing}")
set(_sweep_all "")
foreach(_l IN LISTS _listing)
    string(REGEX REPLACE "^Test +#[0-9]+: +" "" _l "${_l}")
    string(STRIP "${_l}" _l)
    list(APPEND _sweep_all ${_l})
endforeach()
list(LENGTH _days_all _nday_tests)
list(LENGTH _sweep_all _nsweep_tests)
set(_days_once ${_days_all})
list(REMOVE_DUPLICATES _days_once)
list(LENGTH _days_once _nday_once)
list(SORT _days_once)
list(SORT _sweep_all)
if(NOT _days_once STREQUAL _sweep_all OR NOT _nday_tests EQUAL _nsweep_tests)
    set(_dropped ${_sweep_all})
    list(REMOVE_ITEM _dropped ${_days_once})
    math(EXPR _twice "${_nday_tests} - ${_nday_once}")
    message(FATAL_ERROR "the seven days: ${_nday_tests} runs of ${_nday_once} tests; "
        "${_nsweep_tests} gust_sweep tests; ${_twice} run twice; dropped: ${_dropped}")
endif()
message(STATUS "the seven days of the debug sweep are the ${_nsweep_tests} gust_sweep tests, each once")
foreach(_block IN LISTS _day_blocks)
    string(REGEX MATCH "${_day_block_re}" _ "${_block}")
    set(_dp ${CMAKE_MATCH_1})
    set(_dmin ${CMAKE_MATCH_2})
    set(_dof ${CMAKE_MATCH_3})
    string(REPLACE " " "" _dshards "${CMAKE_MATCH_4}")
    string(REPLACE "," ";" _dshards "${_dshards}")
    set(_want "")
    foreach(_k RANGE 1 ${_dof})
        list(APPEND _want ${_k})
    endforeach()
    if(NOT _dshards STREQUAL _want)
        message(FATAL_ERROR "${_dp}'s debug sweep: shards ${_dshards}, not 1 to ${_dof}")
    endif()
    if(_dmin GREATER 120)
        message(FATAL_ERROR "${_dp}'s debug sweep: a limit of ${_dmin} minutes, past 120")
    endif()
    list(APPEND _swept ${_dp})
    foreach(_day RANGE 1 7)
        set(_dir ${WORK}/day_${_dp}_${_day})
        file(REMOVE_RECURSE ${_dir})
        file(MAKE_DIRECTORY ${_dir})
        execute_process(COMMAND ${CMAKE_COMMAND} "-DINCLUDE=^sweep_day_${_day}$" -DPRESET=${_dp}
                -DBUILD=${BUILD} -DOF=${_dof} -DDEFAULT=longest -DMINUTES=${_dmin} -DSHARE=75 -DSTRICT=ON
                -DMODEL_DEBUG=ON
                -DELSEWHERE=ON -DOUT_ALL=${_dir} -P ${ROOT}/tools/ci_shard.cmake
            OUTPUT_VARIABLE _out ERROR_VARIABLE _out RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "${_dp}, day ${_day}: the dealing failed:\n${_out}")
        endif()
        foreach(_k RANGE 1 ${_dof})
            execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} -N
                    -I ${_dir}/shard_${_k}.txt -L "^sweep_day_${_day}$" -FA ".*"
                OUTPUT_VARIABLE _ran)
            string(REGEX MATCHALL "Test +#[0-9]+: [^\n]+" _ran "${_ran}")
            set(_ran_names "")
            foreach(_l IN LISTS _ran)
                string(REGEX REPLACE "^Test +#[0-9]+: +" "" _l "${_l}")
                string(STRIP "${_l}" _l)
                list(APPEND _ran_names ${_l})
            endforeach()
            file(STRINGS ${_dir}/shard_${_k}.names _named)
            list(SORT _ran_names)
            list(SORT _named)
            if(NOT _ran_names STREQUAL _named)
                message(FATAL_ERROR "${_dp}, day ${_day}, shard ${_k}: ctest runs other tests than were dealt it")
            endif()
        endforeach()
    endforeach()
    message(STATUS "${_dp}'s debug sweep: each of the seven days in ${_dof} shards of ${_dmin} min, each shard at most three quarters of it, each shard's run its dealing")
endforeach()

# **Every preset sharded is swept**: what the shard jobs leave out on a
# schedule or a dispatch - gust_sweep - a sweep job of the same preset runs,
# one for each (linux-release twice: Ubuntu and Rocky 9), a debug preset's a
# day at a time. And every sweep
# job's limit is its row's minutes.
list(SORT _swept)
list(SORT _sharded)
list(REMOVE_DUPLICATES _sharded)
set(_swept_once ${_swept})
list(REMOVE_DUPLICATES _swept_once)
if(NOT _swept_once STREQUAL _sharded)
    message(FATAL_ERROR "presets sharded: ${_sharded}; presets swept: ${_swept_once}")
endif()
string(REGEX MATCHALL "\n  [a-z-]+-sweep:\n" _sweep_jobs "${_yml}")
string(REGEX MATCHALL "timeout-minutes: \\\${{ matrix.minutes }}\n" _sweep_limits "${_yml}")
list(LENGTH _sweep_jobs _nsj)
list(LENGTH _sweep_limits _nsl)
# The debug sweep jobs, a day at a time, above: named -sweep and limited to
# their matrix's minutes too.
math(EXPR _nsj "${_nsj} - ${_ndj}")
math(EXPR _nsl "${_nsl} - ${_ndj}")
set(_nsk 0)
foreach(_k IN LISTS _run_kinds)
    if(_k STREQUAL "sweep")
        math(EXPR _nsk "${_nsk} + 1")
    endif()
endforeach()
if(NOT _nsj EQUAL _nsk OR NOT _nsl EQUAL _nsk)
    message(FATAL_ERROR "${_nsk} gust sweep runs, ${_nsj} jobs named -sweep, ${_nsl} limits of the row's minutes")
endif()
message(STATUS "every preset sharded is swept: ${_sharded}; ${_nsk} sweep jobs, each limited to its row's minutes")

list(LENGTH _presets _np)
list(LENGTH _rows _nrows)
if(NOT _dealings EQUAL _nrows)
    message(FATAL_ERROR "${_nrows} matrix rows, but ${_dealings} shards dealt")
endif()
message(STATUS "${_np} sharded jobs and presets, ${_nrows} shards: every shard dealt, every test in exactly one")

# **The nightly's multi-process tests, five times each, in shards**
# (nightly.yml's repeat job). In one job they outgrew its 120 minutes on
# 2026-09-28 and were cut off every night for six weeks, unnoticed in the
# end of nothing failing. Asserted, the same way as ci.yml's shard jobs:
#   - its rows are one preset's shards 1 to n, each once, all "of: n";
#   - its dealing is its own row's, picks the tests by the job's MULTIPROCESS
#     names and leaves out timing as its ctest line does, repeats them as
#     often as its ctest line does, and is told the job's limit;
#   - each shard's -I file, read back by ctest with that -R and -LE, runs
#     exactly the tests dealt it, and between them every test once;
#   - on a host of the preset's own system, every shard's work - each test
#     at its measured cost, five times - is at most three quarters of the
#     job's timeout-minutes (STRICT), so a job that grows fails here, on a
#     pull request, before it is cut off at night.
file(READ ${ROOT}/.github/workflows/nightly.yml _nyml)
string(REGEX MATCHALL "include:\n( *- { preset: [a-z-]+, shard: [0-9]+, of: [0-9]+ }\n)+" _nblocks "${_nyml}")
list(LENGTH _nblocks _nnb)
if(NOT _nnb EQUAL 1)
    message(FATAL_ERROR "nightly.yml: ${_nnb} sharded jobs, not the one repeat job")
endif()
string(FIND "${_nyml}" "${_nblocks}" _at)
string(SUBSTRING "${_nyml}" 0 ${_at} _head)
string(REGEX MATCHALL "timeout-minutes: [^\n]+" _limits "${_head}")
list(GET _limits -1 _nlimit)
string(REGEX REPLACE "^timeout-minutes: " "" _nlimit "${_nlimit}")
if(NOT _nlimit MATCHES "^[0-9]+$")
    message(FATAL_ERROR "nightly.yml: the repeat job's limit is a number of minutes, not ${_nlimit}")
endif()
string(REGEX MATCHALL "- { preset: [a-z-]+, shard: [0-9]+, of: [0-9]+ }" _nrows_l "${_nblocks}")
set(_npreset "")
set(_nof "")
set(_nshards "")
foreach(_row IN LISTS _nrows_l)
    string(REGEX MATCH "preset: ([a-z-]+), shard: ([0-9]+), of: ([0-9]+)" _ "${_row}")
    if(_npreset STREQUAL "")
        set(_npreset ${CMAKE_MATCH_1})
        set(_nof ${CMAKE_MATCH_3})
    elseif(NOT _npreset STREQUAL CMAKE_MATCH_1 OR NOT _nof EQUAL CMAKE_MATCH_3)
        message(FATAL_ERROR "nightly.yml: rows of ${_npreset} of ${_nof} and of ${CMAKE_MATCH_1} of ${CMAKE_MATCH_3}")
    endif()
    list(APPEND _nshards ${CMAKE_MATCH_2})
endforeach()
set(_want "")
foreach(_k RANGE 1 ${_nof})
    list(APPEND _want ${_k})
endforeach()
list(SORT _nshards COMPARE NATURAL)
if(NOT _nshards STREQUAL _want)
    message(FATAL_ERROR "nightly.yml: the repeat job has shards ${_nshards}, not 1 to ${_nof} once each")
endif()
if(NOT _nyml MATCHES "\n      MULTIPROCESS: '([^'\n]+)'\n")
    message(FATAL_ERROR "nightly.yml: the repeat job names no MULTIPROCESS tests")
endif()
set(_nregex "${CMAKE_MATCH_1}")
string(REGEX MATCHALL "cmake -DPRESET=[^\n]*-P tools/ci_shard.cmake" _ncalls "${_nyml}")
string(REGEX MATCHALL "ctest --preset [^\n]*shard.txt[^\n]*" _nruns "${_nyml}")
list(LENGTH _ncalls _nnc)
list(LENGTH _nruns _nnr)
if(NOT _nnc EQUAL 1 OR NOT _nnr EQUAL 1)
    message(FATAL_ERROR "nightly.yml: ${_nnc} calls of the dealing and ${_nnr} ctest runs of its file, not one each")
endif()
if(NOT _ncalls MATCHES " -DREPEAT=([0-9]+) ")
    message(FATAL_ERROR "nightly.yml: the repeat job's dealing says no REPEAT:\n${_ncalls}")
endif()
set(_nrepeat ${CMAKE_MATCH_1})
set(_ncall_want [=[cmake -DPRESET=${{ matrix.preset }} -DSHARD=${{ matrix.shard }} -DOF=${{ matrix.of }} -DOUT=shard.txt -DEXCLUDE=timing "-DREGEX=$MULTIPROCESS" -DREPEAT=@R@ -DMINUTES=@M@ -DSHARE=75 -DELSEWHERE=ON -P tools/ci_shard.cmake]=])
string(REPLACE "@R@" "${_nrepeat}" _ncall_want "${_ncall_want}")
string(REPLACE "@M@" "${_nlimit}" _ncall_want "${_ncall_want}")
if(NOT _ncalls STREQUAL _ncall_want)
    message(FATAL_ERROR "nightly.yml: the repeat job deals other than its own row, its names and its limit ('${_ncall_want}'):\n${_ncalls}")
endif()
set(_nrun_want "ctest --preset \${{ matrix.preset }} -I \"\$PWD/shard.txt\" -R \"\$MULTIPROCESS\" -LE timing --repeat until-fail:${_nrepeat} --output-on-failure || rc=\$?")
if(NOT _nruns STREQUAL _nrun_want)
    message(FATAL_ERROR "nightly.yml: the repeat job runs other than its dealing ('${_nrun_want}'):\n${_nruns}")
endif()
set(_nsize "")
if(_npreset MATCHES "^${_host_system}-")
    set(_nsize -DMINUTES=${_nlimit} -DSHARE=75 -DSTRICT=ON)
endif()
set(_dir ${WORK}/nightly_multiprocess)
file(REMOVE_RECURSE ${_dir})
file(MAKE_DIRECTORY ${_dir})
execute_process(COMMAND ${CMAKE_COMMAND} -DPRESET=${_npreset} -DBUILD=${BUILD} -DOF=${_nof} -DEXCLUDE=timing
        "-DREGEX=${_nregex}" -DREPEAT=${_nrepeat} ${_nsize} -DELSEWHERE=ON -DOUT_ALL=${_dir}
        -P ${ROOT}/tools/ci_shard.cmake
    OUTPUT_VARIABLE _out ERROR_VARIABLE _out RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "nightly.yml's repeat job: the dealing failed:\n${_out}")
endif()
# The tests the names pick: a fixture's setup ctest adds for them (the
# downloads) is not dealt, but run in every shard that needs it.
execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} -N -R "${_nregex}" -LE timing -FA ".*"
    OUTPUT_VARIABLE _listing RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0 OR NOT _listing MATCHES "Total Tests: ([0-9]+)")
    message(FATAL_ERROR "ctest could not list the nightly's multi-process tests of ${BUILD}")
endif()
set(_ncount ${CMAKE_MATCH_1})
set(_ndealt "")
foreach(_k RANGE 1 ${_nof})
    file(READ ${_dir}/shard_${_k}.txt _file)
    if(NOT _file MATCHES "^0,0,0,([0-9,]+)\n$")
        message(FATAL_ERROR "nightly.yml shard ${_k}: not an -I file of test numbers: ${_file}")
    endif()
    string(REPLACE "," ";" _numbers "${CMAKE_MATCH_1}")
    list(APPEND _ndealt ${_numbers})
    execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUILD} -N
            -I ${_dir}/shard_${_k}.txt -R "${_nregex}" -LE timing -FA ".*"
        OUTPUT_VARIABLE _ran)
    string(REGEX MATCHALL "Test +#[0-9]+: [^\n]+" _ran "${_ran}")
    set(_ran_names "")
    foreach(_l IN LISTS _ran)
        string(REGEX REPLACE "^Test +#[0-9]+: +" "" _l "${_l}")
        string(STRIP "${_l}" _l)
        list(APPEND _ran_names ${_l})
    endforeach()
    file(STRINGS ${_dir}/shard_${_k}.names _named)
    list(SORT _ran_names)
    list(SORT _named)
    if(NOT _ran_names STREQUAL _named)
        message(FATAL_ERROR "nightly.yml shard ${_k}: ctest runs other tests than were dealt it")
    endif()
endforeach()
list(LENGTH _ndealt _n)
set(_distinct ${_ndealt})
list(REMOVE_DUPLICATES _distinct)
list(LENGTH _distinct _nd)
if(NOT _n EQUAL _ncount OR NOT _nd EQUAL _ncount)
    message(FATAL_ERROR "nightly.yml's repeat job: ${_ncount} tests, ${_n} dealt, ${_nd} distinct")
endif()
if(_nsize)
    set(_nsized "each at most three quarters of ${_nlimit} min with every test ${_nrepeat} times")
else()
    set(_nsized "sized on a ${_npreset} host only")
endif()
message(STATUS "nightly.yml's multi-process tests: all ${_ncount} dealt once into ${_nof} shards of ${_npreset}, each shard's run its dealing, ${_nsized}")
