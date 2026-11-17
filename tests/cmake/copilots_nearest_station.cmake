# copilots_nearest_station.cmake - under the stations around them
# (`--weather around`), the server's hand-over copilot and a player's own
# copilot are told the runways the station nearest the aircraft reports wet:
# not the first slot's station, nor the last one told.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli>
#         -DSTUB=<glideslope_http_stub> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DRECORDINGS=<dir of recordings>
#         -DFORECAST_FILE=<recorded Open-Meteo answer>
#         -P copilots_nearest_station.cmake
#
# **The stations from a stub, never the network**, built so that neither
# old answer is the nearest. glideslope_http_stub answers aviationweather.gov's
# box with three made-up stations, all in calm air, about the plan's start
# off Bondi - where the server's AI aircraft starts, heading north, and the
# player's 2,000 ft over it:
#
# - YXFA, dry, 1.00 km south of the start: the nearest there, so slot 0;
# - YXNE, raining, 1.05 km north of it: slot 1, and the nearest once she has
#   flown 25 m north - she is first asked about 250 m on (`--copilot-at 5`);
# - YXZZ, dry, 30 km west: the farthest, slot 2, and the last told to the
#   client joining.
#
# The headless client joins with its own copilot (played back; what it
# answers is not what this tests), flies 20 s and leaves; the server hands
# its aircraft to the AI (`--on-leave ai`) and its hand-over copilot (played
# back) is asked. The player's copilot, the server's check of every route
# sent, and the hand-over copilot must each say they were told by YXNE, its
# rain - and none by YXFA or YXZZ. (Players are not started on final here:
# the recorded forecast's wind is across every runway near.) The Open-Meteo
# forecast is the recorded Sydney one, given three days of the same hours
# (server_stations.cmake).
#
# Without the DEM's tiles it reports itself skipped (exit 77).
#
# The client and the server run in copilots_nearest_station_run.cmake,
# started beside the stub.

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** while it runs (port_lock.cmake).
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

if(DEFINED CACHE)
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
endif()
file(MAKE_DIRECTORY "${WORK}")

set(_store "${WORK}/copilots_nearest_station.sqlite")
file(REMOVE "${_store}")
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# The box's answer: three stations, calm, one raining.
set(_rain "100600Z 00000KT 6000 -RA BKN015 14/12 Q1012")
set(_dry "100600Z 00000KT 9999 FEW030 14/08 Q1012")
set(_metars "${WORK}/metars.json")
file(WRITE "${_metars}"
     "[{\"icaoId\":\"YXFA\",\"rawOb\":\"METAR YXFA ${_dry}\",\"lat\":-33.913993,\"lon\":151.290,\"elev\":3},"
     "{\"icaoId\":\"YXNE\",\"rawOb\":\"METAR YXNE ${_rain}\",\"lat\":-33.895557,\"lon\":151.290,\"elev\":3},"
     "{\"icaoId\":\"YXZZ\",\"rawOb\":\"METAR YXZZ ${_dry}\",\"lat\":-33.905,\"lon\":150.966,\"elev\":3}]")

# **The forecast's hours, three days of them** (server_stations.cmake).
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
set(_heard "${WORK}/heard.txt")
file(REMOVE "${_port_file}" "${_result_file}" "${_heard}")
execute_process(
    COMMAND "${STUB}" 404 "${_port_file}" /api/data/metar "${_metars}" /v1/forecast "${_forecast}"
    COMMAND "${CMAKE_COMMAND}" "-DSERVER=${SERVER}" "-DCLIENT=${CLIENT}" "-DDATA=${DATA}"
            "-DCACHE=${CACHE}" "-DRECORDINGS=${RECORDINGS}" "-DSTUB_PORT_FILE=${_port_file}"
            "-DRESULT_FILE=${_result_file}" "-DHEARD=${_heard}" "-DSTORE=${_store}"
            "-DKEY=${_key}" "-DPORT=${PORT}"
            -P "${CMAKE_CURRENT_LIST_DIR}/copilots_nearest_station_run.cmake"
    RESULTS_VARIABLE _stub_rcs OUTPUT_VARIABLE _stub_out ERROR_VARIABLE _stub_said)
port_listened("${_stub_out}" "${_stub_said}")
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the programs were not run against the stub (${_stub_rcs}):\n"
                        "${_stub_said}")
endif()
file(READ "${_result_file}" _ran)
if(NOT _ran MATCHES "=== exits ([^\n]*)\n=== out\n(.*)\n=== err\n(.*)\n$")
    message(FATAL_ERROR "the run was not written whole:\n${_ran}")
endif()
set(_rcs "${CMAKE_MATCH_1}")
set(_out "${CMAKE_MATCH_2}")
set(_err "${CMAKE_MATCH_3}")
port_listened("${_out}" "${_err}")
glideslope_skip_when_not_downloaded("${_rcs}" "${_err}" "cannot download|no network")
if(NOT _rcs STREQUAL "0;0")
    message(FATAL_ERROR "the programs' exit codes were ${_rcs}:\n${_out}\n${_err}")
endif()
set(_heard_text "")
if(EXISTS "${_heard}")
    file(READ "${_heard}" _heard_text)
endif()
# The stations in the slots this is built on: YXFA first, YXZZ last told.
if(NOT _out MATCHES "stations at [0-9.]+ s: 3 found")
    message(FATAL_ERROR "the server did not find the three stations:\n${_out}\n${_err}")
endif()
foreach(_slot IN ITEMS 1 2)
    if(NOT _heard_text MATCHES "told the weather \\(station ${_slot}\\): METAR ")
        message(FATAL_ERROR "the client was not told slot ${_slot}'s weather:\n${_heard_text}")
    endif()
endforeach()
if(NOT _heard_text MATCHES "told the weather: METAR YXFA")
    message(FATAL_ERROR "slot 0 was not YXFA's:\n${_heard_text}")
endif()
string(REGEX MATCHALL "told the weather[^\n]*: METAR YX[A-Z][A-Z]" _told "${_heard_text}")
list(GET _told -1 _last_told)
if(NOT _last_told MATCHES "YXZZ$")
    message(FATAL_ERROR "the last weather told was not YXZZ's, but '${_last_told}':\n"
                        "${_heard_text}")
endif()
set(_meant "told by YXNE, its nearest station: the runways wet near it, and 0 runways "
           "reported contaminated")
string(CONCAT _meant ${_meant})
# **The server's hand-over copilot.**
if(NOT _out MATCHES "the hand-over's copilot for aircraft [0-9]+ ([^\n]*)\n")
    message(FATAL_ERROR "the server's hand-over copilot was not asked:\n${_out}\n${_err}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL _meant)
    message(FATAL_ERROR "the server's hand-over copilot was not ${_meant}, but "
                        "'${CMAKE_MATCH_1}'")
endif()
# **The server's check of every route sent**: the player's copilot's, and
# the hand-over's.
string(REGEX MATCHALL "the route check for aircraft [0-9]+ [^\n]*" _checked "${_out}")
list(LENGTH _checked _times)
if(_times EQUAL 0)
    message(FATAL_ERROR "no route was checked:\n${_out}")
endif()
foreach(_line IN LISTS _checked)
    if(NOT _line MATCHES " ${_meant}$")
        message(FATAL_ERROR "a route was not checked ${_meant}, but '${_line}'")
    endif()
endforeach()
# **The player's own copilot**, every time it was told: by YXNE alone.
string(REGEX MATCHALL "its copilot told by [^\n]*" _player_told "${_heard_text}")
list(LENGTH _player_told _times)
if(_times EQUAL 0)
    message(FATAL_ERROR "the client's copilot never said what it was told:\n${_heard_text}")
endif()
foreach(_line IN LISTS _player_told)
    if(NOT _line STREQUAL "its copilot ${_meant}")
        message(FATAL_ERROR "the client's copilot was not ${_meant}, but '${_line}'")
    endif()
endforeach()
message(STATUS "both copilots ${_meant}")
