# server_stations.cmake - a client flies the server's stations around it, the
# nearer's weather, with no step in the wind.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DSTUB=<glideslope_http_stub> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DMETAR_FILE=<recorded bbox answer>
#         -DFORECAST_FILE=<recorded Open-Meteo answer> -DSAME_M=<m> -DSTEP_MPS=<m/s>
#         -P server_stations.cmake
#
# **The stations around the aircraft, from recordings.** The server is told
# `--weather around`: it asks aviationweather.gov for the METARs in a box
# around every aircraft (`bbox=`) and Open-Meteo for the winds aloft over
# each station it keeps - both answered by glideslope_http_stub from files,
# never the network: aviationweather.gov's answer around Sydney recorded at
# 2100Z on 2026-11-10 (YSSY, YSWS, YSRI, YWLM and YSCB), and Open-Meteo's
# Sydney forecast of 2026-09-17, given three days of the same hours - as
# client_quits_mid_refresh.cmake does - so that a run crossing midnight still
# finds its hour. YSCB is 248 km away and is not kept.
#
# **A flight between one station and the next.** The AI's plan, written here,
# starts 11.6 km from YSSY on the line to YSWS, 42.8 km apart, pointing at
# YSWS: YSWS is 19.6 km farther, inside the band where the two blend
# (world::station_blend_m), so the air is most of it YSSY's, and a little
# YSWS's, and every way the aircraft flies moves the shares. A player joins
# and predicts its own aircraft, starting there (2,000 ft above it), for
# 40 s; the headless client's controls bank it hard both ways, so where it
# goes is not a straight line, and starting inside the band is what makes
# the blend certain to be flown. (That the nearer's air is flown alone
# outside the band, and the whole way from one station to the next, is
# world's unit tests'.) **Built, not hoped for**:
#
# - the server says it found four stations and keeps them;
# - the client is told a weather in each of four slots;
# - the client says the air it flew: most of it YSSY's, the nearer, at the
#   first step, YSSY's share below one - in the blend - and the wind
#   changing by no more than STEP_MPS in any step;
# - its prediction error's median, against where the server says it was, is
#   within SAME_M: the client flies the stations the server does.
#
# Without the DEM's tiles it reports itself skipped (exit 77), never passed.
#
# With STUB_PORT_FILE, this is the inner half, started beside the stub: it
# waits for the stub's port, runs the server and the client against it,
# writes what they said to RESULT_FILE, and stops the stub.

cmake_minimum_required(VERSION 3.28)

if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _stub LIMIT_COUNT 1)
    set(ENV{GLIDESLOPE_WEATHER_SERVICE} "http://127.0.0.1:${_stub}")
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
    include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
    set(_heard "${WORK}/heard.txt")
    file(REMOVE "${_heard}")
    execute_process(
        COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${KEY}" 40 --after 1 --predict
                --heard "${_heard}"
        COMMAND "${SERVER}" --port ${PORT} --seconds 300 --until-empty --ai 1 --headless
                --players 1 --data "${DATA}" --timeout 30 --store "${STORE}"
                --plan "${PLAN}" --weather around
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    port_listened("${_out}" "${_err}")
    set(_heard_text "")
    if(EXISTS "${_heard}")
        file(READ "${_heard}" _heard_text)
    endif()
    file(WRITE "${RESULT_FILE}"
         "=== exits ${_rcs}\n=== server\n${_out}\n${_err}\n=== client\n${_heard_text}")
    file(DOWNLOAD "http://127.0.0.1:${_stub}/stop" "${RESULT_FILE}.stop")
    return()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
file(MAKE_DIRECTORY "${WORK}")
set(_store "${WORK}/stations.sqlite")
file(REMOVE "${_store}")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# The plan: from 11.6 km out of YSSY on the line to YSWS, at 2,500 ft,
# heading 279 true - along that line - at 100 kt, to a point beyond YSWS.
set(_plan "${WORK}/towards-ysws.plan")
file(WRITE "${_plan}" "aircraft c172p\nstart -33.93026 151.04873 2500 279 100\n"
                      "waypoint WEST -33.880 150.600 2500 100 flyover\n")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 1 --ai 1 --data "${DATA}" --plan "${_plan}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _warm ERROR_VARIABLE _err TIMEOUT 300)
if(NOT _rc EQUAL 0)
    message(STATUS "the server could not get its terrain: ${_err}")
    cmake_language(EXIT 77)
endif()

# **The forecast's hours, three days of them** (client_quits_mid_refresh.cmake).
set(_had_epoch "")
if(DEFINED ENV{SOURCE_DATE_EPOCH})
    set(_had_epoch "$ENV{SOURCE_DATE_EPOCH}")
    unset(ENV{SOURCE_DATE_EPOCH})
endif()
string(TIMESTAMP _now "%s" UTC)
set(_days "")
foreach(_offset -86400 0 86400)
    math(EXPR _at "${_now} + ${_offset}")
    set(ENV{SOURCE_DATE_EPOCH} "${_at}")
    string(TIMESTAMP _day "%Y-%m-%d" UTC)
    list(APPEND _days "${_day}")
endforeach()
unset(ENV{SOURCE_DATE_EPOCH})
if(NOT _had_epoch STREQUAL "")
    set(ENV{SOURCE_DATE_EPOCH} "${_had_epoch}")
endif()
file(READ "${FORECAST_FILE}" _text)
if(NOT _text MATCHES "\"time\":\\[([^]]*)\\]")
    message(FATAL_ERROR "${FORECAST_FILE} has no hourly times")
endif()
set(_hours "${CMAKE_MATCH_1}")
set(_times "")
foreach(_day IN LISTS _days)
    string(REPLACE "\"2026-09-17T" "\"${_day}T" _these "${_hours}")
    list(APPEND _times "${_these}")
endforeach()
list(JOIN _times "," _times)
string(REPLACE "\"time\":[${_hours}]" "\"time\":@TIMES@" _text "${_text}")
string(REGEX REPLACE "\\[([^]]*)\\]" "[\\1,\\1,\\1]" _text "${_text}")
string(REPLACE "@TIMES@" "[${_times}]" _text "${_text}")
set(_forecast "${WORK}/forecast.json")
file(WRITE "${_forecast}" "${_text}")

set(_port_file "${WORK}/stub.port")
set(_result_file "${WORK}/result.txt")
file(REMOVE "${_port_file}" "${_result_file}")
execute_process(
    COMMAND "${STUB}" 404 "${_port_file}"
            /api/data/metar "${METAR_FILE}" /v1/forecast "${_forecast}"
    COMMAND "${CMAKE_COMMAND}" "-DSERVER=${SERVER}" "-DCLIENT=${CLIENT}" "-DDATA=${DATA}"
            "-DCACHE=${CACHE}" "-DWORK=${WORK}" "-DPORT=${PORT}" "-DKEY=${_key}"
            "-DSTORE=${_store}" "-DPLAN=${_plan}" "-DSTUB_PORT_FILE=${_port_file}"
            "-DRESULT_FILE=${_result_file}" -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULTS_VARIABLE _stub_rcs ERROR_VARIABLE _stub_said)
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the server was not run against the stub (${_stub_rcs}):\n"
                        "${_stub_said}")
endif()
file(READ "${_result_file}" _ran)
if(NOT _ran MATCHES "=== exits 0;0\n")
    message(FATAL_ERROR "the programs did not both exit 0:\n${_ran}\n${_stub_said}")
endif()

# The server found the four within 200 km, and kept them.
if(NOT _ran MATCHES "stations at 0.000 s: 4 found\n")
    message(FATAL_ERROR "the server did not find the four stations:\n${_ran}")
endif()
foreach(_station IN ITEMS YSSY YSWS YSRI YWLM)
    if(NOT _ran MATCHES "station ${_station} at ")
        message(FATAL_ERROR "the server did not keep ${_station}:\n${_ran}")
    endif()
endforeach()
# The client was told each slot's weather.
foreach(_slot IN ITEMS 1 2 3)
    if(NOT _ran MATCHES "told the weather \\(station ${_slot}\\): METAR ")
        message(FATAL_ERROR "the client was not told slot ${_slot}'s weather:\n${_ran}")
    endif()
endforeach()
# The air it flew.
set(_said_air "air flown: ([0-9]+) steps, the wind changing by at most ([0-9.]+) m/s in one; ")
string(APPEND _said_air "most of it ([A-Z0-9]+)'s at the first, ([A-Z0-9]+)'s at the last ")
string(APPEND _said_air "\\(([0-9.]+) of it\\), the first's share at least ([0-9.]+)")
if(NOT _ran MATCHES "${_said_air}")
    message(FATAL_ERROR "the client did not say the air it flew:\n${_ran}")
endif()
set(_steps ${CMAKE_MATCH_1})
set(_step ${CMAKE_MATCH_2})
set(_first ${CMAKE_MATCH_3})
set(_least ${CMAKE_MATCH_6})
if(NOT _first STREQUAL "YSSY")
    message(FATAL_ERROR "the air at the first step was ${_first}'s, not the nearer YSSY's:\n"
                        "${_ran}")
endif()
if(NOT _least LESS 1)
    message(FATAL_ERROR "YSSY's share was never below one: the flight was not in the "
                        "blend, and this tested nothing of it:\n${_ran}")
endif()
if(_step GREATER STEP_MPS)
    message(FATAL_ERROR "the wind changed by ${_step} m/s in a step, more than ${STEP_MPS}:\n"
                        "${_ran}")
endif()
if(NOT _ran MATCHES "prediction error median: ([0-9.]+) m over ([0-9]+) updates")
    message(FATAL_ERROR "the client did not say its median error:\n${_ran}")
endif()
set(_median ${CMAKE_MATCH_1})
set(_updates ${CMAKE_MATCH_2})
if(_updates LESS 500)
    message(FATAL_ERROR "the client compared ${_updates} updates, fewer than the 500 a flight "
                        "of 40 s gives:\n${_ran}")
endif()
message(STATUS "over ${_steps} steps the wind changed by at most ${_step} m/s in one; YSSY's "
               "share fell to ${_least}; the prediction error's median was ${_median} m over "
               "${_updates} updates")
if(_median GREATER SAME_M)
    message(FATAL_ERROR "the prediction's median error was ${_median} m, more than ${SAME_M}:"
                        "\n${_ran}")
endif()
