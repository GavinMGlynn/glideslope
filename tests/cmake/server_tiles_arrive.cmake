# server_tiles_arrive.cmake - a client joining a server over hills with no
# DEM tiles cached flies the server's ground lift within the sea's bound once
# its tiles arrive, and how long they took is said.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DIMPAIR=<glideslope_impair> -DSTUB=<glideslope_http_stub>
#         -DDATA=<data dir> -DCACHE=<downloads dir> -DWORK=<scratch> -DPORT=<a port>
#         -DSAME_M=<m> -DLIFT_SHARE=<n> -P server_tiles_arrive.cmake
#
# **The hills north of the Hawkesbury in a 50 kt southerly**, as
# server_ground_lift.cmake flies them: the Cessna at 1,200 ft, two predicting
# clients each through a relay holding every datagram 250 ms each way. The
# server has the DEM's tiles; **the first client has none**: its cache is
# made new for each run, with the geoid in it and no tile. The lift its air
# reaches for, 32 km up and down the wind, lies in one tile, which it fetches
# on a thread of its own as it flies (world::TilesAhead) - and until it has
# arrived, the first client's air lifts nothing there while the server's
# does. **Built, not hoped for**:
#
# - the tile is fetched, from a stand-in for Copernicus's bucket on the
#   loopback (glideslope_http_stub, named by GLIDESLOPE_DEM_SOURCE, with the
#   tile's MD5 as its ETag as S3 sends it): the downloaded copy, served once,
#   so the test needs no network but the downloads' own; any other request is
#   refused, and the client must have fetched one tile and failed none;
# - the first client stays until 300 updates have been compared after its
#   tile arrived (`--until-compared-on-ground`), however long it takes - an
#   event, not a time; the time the tile took, and when on the session's
#   clock it had it, are said;
# - once it was had, the first client's median error is within SAME_M, the
#   sea's bound (server_weather.cmake), and less than LIFT_SHARE of the
#   second client's, which flies the same air over no ground
#   (`--no-ground-lift`) - so the lift arrived with the tile, and is flown.
#   The sea's bound alone would not tell them apart. Here (2026-11-21, a
#   debug build) the first's median was 0.038 to 0.041 m and the second's
#   0.096 to 0.114: 0.36 to 0.40 of it. With the first client's lift thrown away once its tile was had it
#   was 0.070 to 0.074 m, 0.61 to 0.65 - inside server_ground_lift.cmake's
#   0.7, so this holds 0.5.
#
# Without the DEM's tiles downloaded it reports itself skipped (exit 77),
# never passed.
#
# With STUB_PORT_FILE, this is the inner half, started beside the stub: it
# waits for the stub's port, runs the server and the clients, writes what they
# said to RESULT_FILE, and stops the stub.

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake); the inner
# half, run by this, holds it already.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

set(_tile Copernicus_DSM_COG_10_S34_00_E151_00_DEM)

if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _stub LIMIT_COUNT 1)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
    set(_station -33.9461,151.1772,6)
    set(_south "YSSY 020600Z 18050KT 9999 FEW030 20/10 Q1012")
    set(_fetching "${WORK}/fetching.txt")
    set(_liftless "${WORK}/liftless.txt")
    file(REMOVE "${_fetching}" "${_liftless}")
    math(EXPR _relay_fetching "${PORT} + 4")
    port_lock_hold(${_relay_fetching})
    math(EXPR _relay_liftless "${PORT} + 6")
    port_lock_hold(${_relay_liftless})
    execute_process(
        # Each writes down the pipe to the next (server_impaired.cmake says
        # how). The first client alone has the empty cache and the stand-in.
        COMMAND "${CMAKE_COMMAND}" -E env "GLIDESLOPE_CACHE=${EMPTY_CACHE}"
                "GLIDESLOPE_DEM_SOURCE=http://127.0.0.1:${_stub}"
                "${CLIENT}" connect "127.0.0.1:${_relay_fetching}" "${KEY}" 280 --after 1
                --predict --until-compared-on-ground 300 --heard "${_fetching}"
        COMMAND "${CLIENT}" connect "127.0.0.1:${_relay_liftless}" "${KEY}" 25 --after 1
                --predict --no-ground-lift --heard "${_liftless}"
        COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
                --players 2 --data "${DATA}" --timeout 30 --store "${STORE}"
                --plan "${PLAN}" --metar "${_south}" --station ${_station}
        COMMAND "${IMPAIR}" ${_relay_fetching} "127.0.0.1:${PORT}" --delay 250 --jitter 0
                --loss 0 --seed 1 --until-input-ends --seconds 290
        COMMAND "${IMPAIR}" ${_relay_liftless} "127.0.0.1:${PORT}" --delay 250 --jitter 0
                --loss 0 --seed 2 --until-input-ends --seconds 290
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    port_listened("${_out}" "${_err}")
    set(_said "")
    foreach(_who IN ITEMS fetching liftless)
        set(_text "")
        if(EXISTS "${WORK}/${_who}.txt")
            file(READ "${WORK}/${_who}.txt" _text)
        endif()
        string(APPEND _said "=== ${_who}\n${_text}\n")
    endforeach()
    file(WRITE "${RESULT_FILE}" "=== exits ${_rcs}\n=== programs\n${_out}\n${_err}\n${_said}")
    file(DOWNLOAD "http://127.0.0.1:${_stub}/stop" "${RESULT_FILE}.stop")
    return()
endif()

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
file(MAKE_DIRECTORY "${WORK}")
set(_store "${WORK}/tiles.sqlite")
set(_plan "${WORK}/hills.plan")
file(REMOVE "${_store}")
file(WRITE "${_plan}" "aircraft c172p\n"
                      "start -33.4500 151.2000 1200 0 90\n"
                      "waypoint NORTH -33.3000 151.2000 1200 90\n")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}" --plan "${_plan}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
set(_tile_file "${CACHE}/copernicus-dem-30m/${_tile}.tif")
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_tile_file}" OR NOT EXISTS "${CACHE}/egm2008-5.zip")
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

# **The client's cache, empty of tiles**: made new, the geoid alone in it.
set(_empty "${WORK}/empty-cache")
file(REMOVE_RECURSE "${_empty}")
file(MAKE_DIRECTORY "${_empty}")
file(COPY "${CACHE}/egm2008-5.zip" DESTINATION "${_empty}")
file(MD5 "${_tile_file}" _md5)

set(_port_file "${WORK}/stub.port")
set(_result_file "${WORK}/result.txt")
file(REMOVE "${_port_file}" "${_result_file}")
execute_process(
    COMMAND "${STUB}" --etag "${_md5}" 404 "${_port_file}" "/${_tile}/${_tile}.tif" "${_tile_file}"
    COMMAND "${CMAKE_COMMAND}" "-DSERVER=${SERVER}" "-DCLIENT=${CLIENT}" "-DIMPAIR=${IMPAIR}"
            "-DDATA=${DATA}" "-DCACHE=${CACHE}" "-DEMPTY_CACHE=${_empty}" "-DWORK=${WORK}"
            "-DPORT=${PORT}" "-DKEY=${_key}" "-DSTORE=${_store}" "-DPLAN=${_plan}"
            "-DSTUB_PORT_FILE=${_port_file}" "-DRESULT_FILE=${_result_file}"
            -P "${CMAKE_CURRENT_LIST_DIR}/server_tiles_arrive.cmake"
    RESULTS_VARIABLE _stub_rcs OUTPUT_VARIABLE _stub_out ERROR_VARIABLE _stub_said)
port_listened("${_stub_out}" "${_stub_said}")
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the server was not run against the stub (${_stub_rcs}):\n"
                        "${_stub_said}")
endif()
file(READ "${_result_file}" _ran)
if(NOT _ran MATCHES "=== exits 0;0;0;0;0\n")
    message(FATAL_ERROR "the programs did not all exit 0:\n${_ran}\n${_stub_said}")
endif()
# **The tile was served once, from the stand-in**, and nothing else asked.
if(NOT _stub_said MATCHES "answered 0 requests 404, and 1 with FILE")
    message(FATAL_ERROR "the stand-in did not serve the tile once, and nothing else:\n"
                        "${_stub_said}")
endif()

string(FIND "${_ran}" "=== fetching\n" _at_fetching)
string(FIND "${_ran}" "=== liftless\n" _at_liftless)
string(SUBSTRING "${_ran}" ${_at_fetching} -1 _fetching)
math(EXPR _fetching_length "${_at_liftless} - ${_at_fetching}")
string(SUBSTRING "${_ran}" ${_at_fetching} ${_fetching_length} _fetching)
string(SUBSTRING "${_ran}" ${_at_liftless} -1 _liftless)

foreach(_who IN ITEMS fetching liftless)
    foreach(_told IN ITEMS
            "told the collision ground: [^\n]*, this client's too\n"
            "told the weather: YSSY 020600Z 18050KT [^\n]*\\(weather 1, ")
        if(NOT _${_who} MATCHES "${_told}")
            message(FATAL_ERROR "the ${_who} client was not told '${_told}':\n${_${_who}}")
        endif()
    endforeach()
endforeach()
if(NOT _fetching MATCHES "the air's ground: 1 tiles fetched in ([0-9.]+) s, 0 failed, 0 waiting\n")
    message(FATAL_ERROR "the fetching client did not fetch its one tile, and fail none:\n"
                        "${_fetching}")
endif()
set(_fetched_in ${CMAKE_MATCH_1})
if(NOT _fetching MATCHES
       "the air's ground had at ([0-9.]+) s on the session's clock, (-?[0-9.]+) s after the first update compared\n")
    message(FATAL_ERROR "the fetching client did not say when its ground was had:\n${_fetching}")
endif()
set(_had_at ${CMAKE_MATCH_1})
set(_had_after ${CMAKE_MATCH_2})
if(NOT _fetching MATCHES
       "prediction error median before the air's ground was had: ([0-9.]+) m over ([0-9]+) updates; once it was had: ([0-9.]+) m over ([0-9]+) updates\n")
    message(FATAL_ERROR "the fetching client did not say its errors before and after:\n"
                        "${_fetching}")
endif()
set(_median_before ${CMAKE_MATCH_1})
set(_before ${CMAKE_MATCH_2})
set(_median_lift ${CMAKE_MATCH_3})
set(_after ${CMAKE_MATCH_4})
if(_after LESS 300)
    message(FATAL_ERROR "the fetching client compared ${_after} updates once its ground was "
                        "had, fewer than the 300 it stays for:\n${_fetching}")
endif()
if(NOT _liftless MATCHES "prediction error median: ([0-9.]+) m over ([0-9]+) updates")
    message(FATAL_ERROR "the liftless client did not say its median error:\n${_liftless}")
endif()
set(_median_liftless ${CMAKE_MATCH_1})
if(CMAKE_MATCH_2 LESS 300)
    message(FATAL_ERROR "the liftless client compared ${CMAKE_MATCH_2} updates, fewer than "
                        "the 300 a flight of 20 s gives:\n${_liftless}")
endif()
message(STATUS "the tile took ${_fetched_in} s to fetch, and was had at ${_had_at} s on the "
               "session's clock, ${_had_after} s after the first update compared; the "
               "prediction's median error before it ${_median_before} m over ${_before} "
               "updates, once it was had ${_median_lift} m over ${_after}; over no ground, "
               "${_median_liftless} m")
if(_median_lift GREATER SAME_M)
    message(FATAL_ERROR "once its tile arrived, the fetching client's median error was "
                        "${_median_lift} m, more than the sea's ${SAME_M}")
endif()
# In millimetres, for math(): the medians are said to the millimetre.
foreach(_who IN ITEMS lift liftless)
    if(NOT _median_${_who} MATCHES "^([0-9]+)\\.([0-9][0-9][0-9])$")
        message(FATAL_ERROR "the ${_who} median, ${_median_${_who}}, is not in millimetres")
    endif()
    math(EXPR _mm_${_who} "${CMAKE_MATCH_1} * 1000 + 1${CMAKE_MATCH_2} - 1000")
endforeach()
if(NOT LIFT_SHARE MATCHES "^0\\.([0-9])$")
    message(FATAL_ERROR "LIFT_SHARE is a tenth, 0.1 to 0.9, not ${LIFT_SHARE}")
endif()
math(EXPR _share_tenths "${CMAKE_MATCH_1}")
math(EXPR _ten_lift "10 * ${_mm_lift}")
math(EXPR _share_of_liftless "${_share_tenths} * ${_mm_liftless}")
if(NOT _ten_lift LESS _share_of_liftless)
    message(FATAL_ERROR "once its tile arrived, the fetching client's median error was "
                        "${_median_lift} m, not less than ${LIFT_SHARE} of the "
                        "${_median_liftless} m flying the same air over no ground: the lift "
                        "did not arrive with the tile")
endif()
