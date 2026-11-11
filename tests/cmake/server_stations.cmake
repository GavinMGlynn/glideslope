# server_stations.cmake - a client flies the server's stations around it, the
# nearer's weather, with no step in the wind.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DSTUB=<glideslope_http_stub> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DMETAR_FILE=<recorded bbox answer>
#         -DFORECAST_FILE=<recorded Open-Meteo answer> -DSAME_M=<m> -DSTEP_MPS=<m/s>
#         -DCASE=<recorded|204|404> -P server_stations.cmake
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
# **A flight from one station's air into the next's.** The plan, written
# here, is a 737-300's: it starts 10.5 km from YSSY on the line to YSWS,
# 42.8 km apart, pointing along it at YSWS at 250 kt. YSWS is 21.8 km
# farther there, so the air is YSSY's alone. A player joins and predicts her
# own 737, starting there (2,000 ft above it), for 190 s, held on the trim
# and levers the server holds her on (`--from-held`) - straight on along the
# line, about 25 km: through the band where the two blend
# (world::station_blend_m), across the midpoint, its steepest part, and out
# into YSWS's alone. **Built, not hoped for**:
#
# - the server says it found four stations and keeps them;
# - the client is told a weather in each of four slots;
# - the client says the air it flew: YSSY's alone at the first step, YSWS's
#   alone at the last, YSSY's share down to nought between, and the wind
#   changing by no more than STEP_MPS in any step - where the worst came is
#   said;
# - its prediction error's median, against where the server says it was, is
#   within SAME_M: the client flies the stations the server does.
#
# **CASE**: `recorded`, every request answered from the files; `204` and
# `404`, the first five - the box and the four stations' forecasts the
# server asks for before its clock starts - answered from them, and every one
# after with no content (204) or refused (404). The player's joining moves
# the server to look again (an aircraft more), and the box then answers
# nothing: **the stations are kept**, said by the server, and the client is
# told none emptied - it flies the four still, in the blend, as above.
#
# Without the DEM's tiles it reports itself skipped (exit 77), never passed.
#
# With STUB_PORT_FILE, this is the inner half, started beside the stub: it
# waits for the stub's port, runs the server and the client against it,
# writes what they said to RESULT_FILE, and stops the stub.

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake); the inner
# half, run by this, holds it already.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _stub LIMIT_COUNT 1)
    set(ENV{GLIDESLOPE_WEATHER_SERVICE} "http://127.0.0.1:${_stub}")
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
    set(_heard "${WORK}/heard.txt")
    file(REMOVE "${_heard}")
    execute_process(
        COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${KEY}" 190 --after 1 --predict
                --from-held
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

# The plan: from 10.5 km out of YSSY on the line to YSWS, at 2,500 ft,
# heading 279 true - along that line - at 250 kt, to a point beyond YSWS.
set(_plan "${WORK}/towards-ysws.plan")
file(WRITE "${_plan}" "aircraft 737-300\nstart -33.93176 151.06051 2500 279 250\n"
                      "waypoint WEST -33.880 150.600 2500 250 flyover\n")

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

if(CASE STREQUAL "recorded")
    set(_stub_case 404)
elseif(CASE STREQUAL "204" OR CASE STREQUAL "404")
    set(_stub_case --files-for 5 ${CASE})
else()
    message(FATAL_ERROR "CASE is recorded, 204 or 404, not '${CASE}'")
endif()
set(_port_file "${WORK}/stub.port")
set(_result_file "${WORK}/result.txt")
file(REMOVE "${_port_file}" "${_result_file}")
execute_process(
    COMMAND "${STUB}" ${_stub_case} "${_port_file}"
            /api/data/metar "${METAR_FILE}" /v1/forecast "${_forecast}"
    COMMAND "${CMAKE_COMMAND}" "-DSERVER=${SERVER}" "-DCLIENT=${CLIENT}" "-DDATA=${DATA}"
            "-DCACHE=${CACHE}" "-DWORK=${WORK}" "-DPORT=${PORT}" "-DKEY=${_key}"
            "-DSTORE=${_store}" "-DPLAN=${_plan}" "-DSTUB_PORT_FILE=${_port_file}"
            "-DRESULT_FILE=${_result_file}" -P "${CMAKE_CURRENT_LIST_DIR}/server_stations.cmake"
    RESULTS_VARIABLE _stub_rcs OUTPUT_VARIABLE _stub_out ERROR_VARIABLE _stub_said)
port_listened("${_stub_out}" "${_stub_said}")
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
# **The stations kept** when the box answered nothing, and no slot emptied.
if(NOT CASE STREQUAL "recorded")
    if(NOT _ran MATCHES "stations kept: around [^\n]*\nstations at [0-9.]+ s: 4 found\n")
        message(FATAL_ERROR "the server did not keep its four stations when the box "
                            "answered ${CASE}:\n${_ran}")
    endif()
endif()
if(_ran MATCHES "told the weather[^\n]*: still air")
    message(FATAL_ERROR "the client was told a slot of still air:\n${_ran}")
endif()
# The client was told each slot's weather.
foreach(_slot IN ITEMS 1 2 3)
    if(NOT _ran MATCHES "told the weather \\(station ${_slot}\\): METAR ")
        message(FATAL_ERROR "the client was not told slot ${_slot}'s weather:\n${_ran}")
    endif()
endforeach()
# The air it flew.
set(_said_air "air flown: ([0-9]+) steps, the wind changing by at most ([0-9.]+) m/s in one, ")
string(APPEND _said_air "at step ([0-9]+), where the first's share was ([0-9.]+); most of it ")
string(APPEND _said_air "([A-Z0-9]+)'s at the first \\(([0-9.]+) of it\\), ([A-Z0-9]+)'s at the last ")
string(APPEND _said_air "\\(([0-9.]+) of it\\), the first's share at least ([0-9.]+)")
if(NOT _ran MATCHES "${_said_air}")
    message(FATAL_ERROR "the client did not say the air flown:\n${_ran}")
endif()
set(_steps ${CMAKE_MATCH_1})
set(_step ${CMAKE_MATCH_2})
set(_worst_at ${CMAKE_MATCH_3})
set(_worst_share ${CMAKE_MATCH_4})
set(_first ${CMAKE_MATCH_5})
set(_first_share ${CMAKE_MATCH_6})
set(_last ${CMAKE_MATCH_7})
set(_last_share ${CMAKE_MATCH_8})
set(_least ${CMAKE_MATCH_9})
# **From one station's air alone to the other's alone**: the whole band
# crossed, its steepest part - the middle, where the shares change fastest -
# with it, so that the step bound held across all of it.
if(NOT _first STREQUAL "YSSY" OR NOT _first_share STREQUAL "1.000")
    message(FATAL_ERROR "the air at the first step was not YSSY's alone, the nearer's by "
                        "more than the band, but ${_first_share} ${_first}'s:\n${_ran}")
endif()
if(NOT _last STREQUAL "YSWS" OR NOT _last_share STREQUAL "1.000")
    message(FATAL_ERROR "the air at the last step was not YSWS's alone, the nearer's then, "
                        "but ${_last_share} ${_last}'s - the band was not crossed:\n${_ran}")
endif()
if(NOT _least STREQUAL "0.000")
    message(FATAL_ERROR "YSSY's share fell only to ${_least}, not to nought:\n${_ran}")
endif()
message(STATUS "over ${_steps} steps from YSSY's air alone to YSWS's alone the wind changed "
               "by at most ${_step} m/s in one, at step ${_worst_at}, where YSSY's share was "
               "${_worst_share}")
if(_step GREATER STEP_MPS)
    message(FATAL_ERROR "the wind changed by ${_step} m/s in a step, more than ${STEP_MPS}:\n"
                        "${_ran}")
endif()
if(NOT _ran MATCHES "prediction error median: ([0-9.]+) m over ([0-9]+) updates")
    message(FATAL_ERROR "the client did not say its median error:\n${_ran}")
endif()
set(_median ${CMAKE_MATCH_1})
set(_updates ${CMAKE_MATCH_2})
if(_updates LESS 3000)
    message(FATAL_ERROR "the client compared ${_updates} updates, fewer than the 3,000 a "
                        "flight of 190 s gives:\n${_ran}")
endif()
message(STATUS "the prediction error's median was ${_median} m over ${_updates} updates")
if(_median GREATER SAME_M)
    message(FATAL_ERROR "the prediction's median error was ${_median} m, more than ${SAME_M}:"
                        "\n${_ran}")
endif()
