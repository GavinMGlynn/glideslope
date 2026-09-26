# ion_stalled.cmake - a `--terrain ion` run whose tiles never arrive still
# ends, and leaves its cache free for the next.
#
#   cmake -DPROGRAM=<glideslope> -DSTALL=<glideslope_ion_stall> -DDRIVER=<driver>
#         -DWORK=<dir> -DCACHE=<downloads dir> -DHOW=<itself|term>
#         -P ion_stalled.cmake
#
# **Built on purpose**: glideslope_ion_stall stands in for Cesium ion on the
# loopback. It says where the terrain and the imagery are - itself - and then
# answers every request for them with a status of 200 and a byte every quarter
# of a second, for ever. Nothing about such a transfer is stalled, so no stall
# timeout ends it; it simply never finishes. No network and no token are
# needed: the run is pointed at it by GLIDESLOPE_CESIUM_ION_API, with a token
# that is not one.
#
# Two ways a run ends, each held to it:
#
#   - **itself**: the shot's own wait for the terrain to settle gives up after
#     its three quarters of a minute, the frame is written, and the run ends.
#     It must be gone within RUN_LIMIT_S of starting. Before the fix it wrote
#     its frame and then waited for ever, at exit, on the transfers.
#   - **term** - what `timeout` sends: once a transfer is being held, the run
#     is sent SIGTERM, and must be gone within TERM_LIMIT_S of it. Before the
#     fix SDL turned the signal into a quit event nothing read until the
#     terrain had settled, and the run then waited for ever as above. Not on
#     Windows, which has no SIGTERM: a timed-out run is ended there by
#     TerminateProcess, which nothing can outlive.
#
# Either way the cache must then take a write at once - opened by another
# process with no busy timeout, as Cesium Native opens it - which it cannot
# while a run that is still alive holds it.
#
# With STALL_PORT_FILE, this is the inner half, started beside the stand-in:
# it waits for the port, does the run, keeps its verdict in RESULT_FILE, and
# stops the stand-in.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

# The shot's own wait for a streamed provider is 45 s (TerrainTiles::update);
# the rest of the limit is the run's start and end on a slow machine - a
# debug, sanitized build on a shared runner - measured in PROJECT_STATUS.md.
set(RUN_LIMIT_S 180)
# From the signal to the process being gone.
set(TERM_LIMIT_S 30)

set(_stem "${WORK}/ion-stalled-${DRIVER}-${HOW}")

if(DEFINED STALL_PORT_FILE)
    while(NOT EXISTS "${STALL_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STALL_PORT_FILE}" _port LIMIT_COUNT 1)
    set(_cache "${_stem}.sqlite")
    file(REMOVE "${_cache}" "${_cache}-wal" "${_cache}-shm" "${_stem}.bmp"
                "${_stem}.pid" "${_stem}.ended")
    set(ENV{LSAN_OPTIONS} "exitcode=0")
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
    set(ENV{GLIDESLOPE_CESIUM_CACHE} "${_cache}")
    set(ENV{GLIDESLOPE_CESIUM_ION_API} "http://127.0.0.1:${_port}")
    set(ENV{GLIDESLOPE_CESIUM_ION_TOKEN} "not-a-token")
    set(_run "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 160x120
             --screen terrain --terrain ion --at -39.0,174.3,4000
             --toward -39.296,174.064,2518 --shot-at 2 --shot "${_stem}.bmp")
    set(_verdict "")
    if(HOW STREQUAL "itself")
        string(TIMESTAMP _t0 "%s")
        execute_process(COMMAND ${_run} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out
                        ERROR_VARIABLE _err TIMEOUT ${RUN_LIMIT_S})
        string(TIMESTAMP _t1 "%s")
        math(EXPR _took "${_t1} - ${_t0}")
        if(NOT _rc MATCHES "^[0-9]+$")
            string(CONCAT _verdict "FAIL: the run was still alive ${RUN_LIMIT_S} s after it "
                         "started (${_rc}), having written its frame: ${_out}")
        elseif(_err MATCHES "could not download")
            string(CONCAT _verdict "SKIP: the DEM could not be had: ${_err}")
        elseif(NOT _rc EQUAL 0)
            string(CONCAT _verdict "FAIL: the run exited ${_rc}:\n${_err}")
        else()
            string(CONCAT _verdict "ENDED: in ${_took} s, exit ${_rc}")
        endif()
    else()
        # A shell, for `kill`: the run started in the background, its own
        # process id kept, and its exit status written when it ends.
        set(_script [=[
pidfile="$1"; ended="$2"; stalled="$3"; out="$4"; err="$5"; limit="$6"; shift 6
( sh -c 'echo $$ >"$0"; exec "$@"' "$pidfile" "$@" >"$out" 2>"$err"; echo $? >"$ended" ) &
while [ ! -s "$pidfile" ] || { [ ! -e "$stalled" ] && [ ! -e "$ended" ]; }; do sleep 0.1; done
if [ -e "$ended" ]; then echo "ended-first $(cat "$ended")"; exit 0; fi
pid=$(cat "$pidfile")
from=$(date +%s)
kill -TERM "$pid"
while [ ! -e "$ended" ]; do
  now=$(date +%s)
  if [ $((now - from)) -gt "$limit" ]; then
    echo "outlived $((now - from))"
    kill -KILL "$pid"
    while [ ! -e "$ended" ]; do sleep 0.1; done
    exit 0
  fi
  sleep 0.1
done
now=$(date +%s)
echo "gone $((now - from)) $(cat "$ended")"
]=])
        execute_process(COMMAND sh -c "${_script}" sh "${_stem}.pid" "${_stem}.ended"
                                "${STALL_PORT_FILE}.stalled" "${_stem}.out" "${_stem}.err"
                                ${TERM_LIMIT_S} ${_run}
                        OUTPUT_VARIABLE _said ERROR_VARIABLE _shell_err
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        file(READ "${_stem}.err" _err)
        if(_said MATCHES "^gone ([0-9]+) ([0-9]+)")
            set(_took "${CMAKE_MATCH_1}")
            set(_rc "${CMAKE_MATCH_2}")
            # 143 is 128 and SIGTERM's 15: it ended because it was told to.
            if(NOT _rc EQUAL 143)
                string(CONCAT _verdict "FAIL: gone ${_took} s after SIGTERM, but it exited "
                             "${_rc}, not 143:\n${_err}")
            else()
                string(CONCAT _verdict "ENDED: ${_took} s after SIGTERM, exit ${_rc}")
            endif()
        elseif(_said MATCHES "^outlived")
            string(CONCAT _verdict "FAIL: still alive ${TERM_LIMIT_S} s after SIGTERM, "
                         "and killed:\n${_err}")
        elseif(_said MATCHES "^ended-first" AND _err MATCHES "could not download")
            string(CONCAT _verdict "SKIP: the DEM could not be had: ${_err}")
        else()
            string(CONCAT _verdict "FAIL: the run ended before any transfer was held "
                         "(${_said}${_shell_err}):\n${_err}")
        endif()
    endif()
    # Leaks in the sanitized build are judged as every client test's are.
    if(_verdict MATCHES "^ENDED")
        glideslope_judge_leaks("${_err}")
        execute_process(COMMAND "${STALL}" write "${_cache}" RESULT_VARIABLE _wrc
                        OUTPUT_VARIABLE _wout ERROR_VARIABLE _werr
                        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
        if(_wrc EQUAL 0)
            string(CONCAT _verdict "${_verdict}; ${_wout}")
        else()
            string(CONCAT _verdict "FAIL: ${_verdict}, but ${_werr}")
        endif()
    endif()
    file(WRITE "${RESULT_FILE}" "${_verdict}")
    file(DOWNLOAD "http://127.0.0.1:${_port}/stop" "${RESULT_FILE}.stop")
    return()
endif()

file(MAKE_DIRECTORY "${WORK}")
set(_port_file "${_stem}.port")
set(_result_file "${_stem}.result")
file(REMOVE "${_port_file}" "${_port_file}.stalled" "${_result_file}")
execute_process(COMMAND "${STALL}" serve "${_port_file}"
                COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=${PROGRAM}" "-DSTALL=${STALL}"
                        "-DDRIVER=${DRIVER}" "-DWORK=${WORK}" "-DCACHE=${CACHE}"
                        "-DHOW=${HOW}" "-DSTALL_PORT_FILE=${_port_file}"
                        "-DRESULT_FILE=${_result_file}"
                        -P "${CMAKE_CURRENT_LIST_FILE}"
                RESULTS_VARIABLE _rcs ERROR_VARIABLE _stall_said)
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the run was never made against the stand-in (${_rcs}):\n"
                        "${_stall_said}")
endif()
file(READ "${_result_file}" _verdict)
if(_verdict MATCHES "^SKIP: (.*)")
    message(STATUS "${CMAKE_MATCH_1}")
    cmake_language(EXIT 77)
endif()
if(NOT _verdict MATCHES "^ENDED")
    message(FATAL_ERROR "${_verdict}\n${_stall_said}")
endif()
# The run really was stuck in a held transfer, not merely refused.
if(NOT _stall_said MATCHES "answered 2 endpoints and held ([1-9][0-9]*) transfers")
    message(FATAL_ERROR "the stand-in held no transfer, so nothing was tested:\n"
                        "${_stall_said}\n${_verdict}")
endif()
message(STATUS "${_verdict}; the stand-in held ${CMAKE_MATCH_1} transfers")
