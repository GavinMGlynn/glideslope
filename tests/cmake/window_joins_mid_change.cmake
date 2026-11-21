# window_joins_mid_change.cmake - the client with the window, joining a
# server while its stations change, flies the server's air to the last bit.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope>
#         -DSTUB=<glideslope_http_stub> -DDATA=<data dir> -DCACHE=<downloads dir>
#         -DWORK=<scratch> -DPORT=<a port> -DDRIVER=<gpu driver>
#         -DMETAR_FILE=<recorded bbox answer> -DFORECAST_FILE=<recorded Open-Meteo answer>
#         -P window_joins_mid_change.cmake
#
# **The stations change three times, from recordings.** The server is told
# `--weather around` (server_stations.cmake), looking again every 2 s
# (`--weather-refresh`) and blending each change over 600 s. Its box around
# the aircraft is answered by glideslope_http_stub in turn, made here from
# aviationweather.gov's answer around Sydney of 2026-11-10 2100Z:
#
#   A, before the clock starts: YSSY in rain, YSWS and YWLM, in slots 0 to 2;
#   B: YSSY's rain over and a new report of YSWS - each blended from its own;
#   C: YSWS let go for YSRI, in slot 1 - the stations change;
#   D: YWLM let go, slot 2 emptied - the stations change again, while C's
#      change still blends.
#
# Every look after D is a transfer begun and never finished (`--stall`):
# nothing changes again, and the stub says, by a file, that the server has
# looked after D (`--answered`) - D has been flown. The window client
# (`glideslope --headless`) connects only then (`--after-ready`): joining
# inside the blends of both changes, slot 1 told three weathers - YSWS,
# its new report, YSRI - **built, and checked** from what it says.
#
# **The air compared, to the last bit**: at 5 places (YSSY, between it and
# YSWS, north of Sydney, south-west of it, YWLM), 3 heights and 10 moments
# from 120 s - after the join, checked - to 1,000 s, past every blend's end,
# 150 points, written to a file of points both read. The client says, as it
# quits, the air its flight flies of the server's stations there
# (`--test-say-air`); the server says its own as it stops, after the client
# has gone. Every field of the air, each double as C's "%a": every line the
# same, counted, and how many differ said.
#
# **The server's stations changed only as built**: three changes said after
# the first stations - B, C, D - and none after. A look after D ends only
# when its stalled transfer is abandoned, 60 s on (platform::HttpRequest): a
# client that has not quit by then is said as that, not as other air.
#
# It needs a GPU driver, and the DEM's tiles for the server; without either
# it reports itself skipped (exit 77), never passed.
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
    set(ENV{LSAN_OPTIONS} "exitcode=0")
    # The client says what it says to a file: the server's standard output
    # is what is read here.
    set(_client_said "${WORK}/client-said.txt")
    file(REMOVE "${_client_said}")
    execute_process(
        COMMAND "${CLIENT}" --headless --gpu-driver "${DRIVER}" --size 320x200
                --imagery off --shot "${WORK}/joined.bmp" --shot-at 999999 --quit-at 600
                --stdout "${_client_said}" --after-ready "${LOOKED_AFTER}"
                --server 127.0.0.1 ${PORT} --server-key ${KEY}
                --test-say-air "${POINTS}" "${WORK}/client-air.txt"
        COMMAND "${SERVER}" --port ${PORT} --seconds 600 --until-empty --ai 1 --headless
                --players 1 --data "${DATA}" --timeout 30 --store "${STORE}"
                --plan "${PLAN}" --weather around --weather-refresh 2 --weather-blend 600
                --test-say-air "${POINTS}" "${WORK}/server-air.txt"
        RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    port_listened("${_out}" "${_err}")
    set(_client_text "")
    if(EXISTS "${_client_said}")
        file(READ "${_client_said}" _client_text)
    endif()
    file(WRITE "${RESULT_FILE}" "=== exits ${_rcs}\n=== server\n${_out}\n=== client\n"
                                "${_client_text}\n=== errors\n${_err}\n")
    file(DOWNLOAD "http://127.0.0.1:${_stub}/stop" "${RESULT_FILE}.stop")
    return()
endif()

foreach(_needed SERVER CLIENT STUB DATA CACHE WORK PORT DRIVER METAR_FILE FORECAST_FILE)
    if(NOT DEFINED ${_needed})
        message(FATAL_ERROR "window_joins_mid_change.cmake needs ${_needed}")
    endif()
endforeach()
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
file(MAKE_DIRECTORY "${WORK}")
set(_store "${WORK}/joins.sqlite")
file(REMOVE "${_store}" "${WORK}/server-air.txt" "${WORK}/client-air.txt" "${WORK}/joined.bmp")

execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${_store}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _out MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    message(FATAL_ERROR "the server did not print a key:\n${_out}${_err}")
endif()
set(_key "${CMAKE_MATCH_1}")

# server_stations.cmake's plan: 10.5 km from YSSY on the line to YSWS.
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

# **The box's four answers**, each station's entry from the recording, its
# METAR changed where the story says.
file(READ "${METAR_FILE}" _recorded)
string(JSON _count LENGTH "${_recorded}")
math(EXPR _last "${_count} - 1")
foreach(_i RANGE ${_last})
    string(JSON _entry GET "${_recorded}" ${_i})
    string(JSON _id GET "${_entry}" icaoId)
    set(_station_${_id} "${_entry}")
endforeach()
foreach(_id IN ITEMS YSSY YSWS YSRI YWLM)
    if(NOT DEFINED _station_${_id})
        message(FATAL_ERROR "${METAR_FILE} has no ${_id}")
    endif()
endforeach()
function(_reported _id _metar _out)
    string(JSON _entry SET "${_station_${_id}}" rawOb "\"METAR ${_metar}\"")
    set(${_out} "${_entry}" PARENT_SCOPE)
endfunction()
_reported(YSSY "YSSY 102100Z 28016G28KT 4000 RA BKN010 20/13 Q1015" _yssy_rain)
_reported(YSSY "YSSY 102130Z 18025KT CAVOK 19/13 Q1011" _yssy_after)
set(_ysws "${_station_YSWS}")
_reported(YSWS "YSWS 102130Z 24012KT 9999 17/11 Q1013" _ysws_new)
set(_ysri "${_station_YSRI}")
set(_ywlm "${_station_YWLM}")
set(_answers "")
foreach(_answer IN ITEMS "A:_yssy_rain,_ysws,_ywlm" "B:_yssy_after,_ysws_new,_ywlm"
                         "C:_yssy_after,_ysri,_ywlm" "D:_yssy_after,_ysri")
    string(REPLACE ":" ";" _answer "${_answer}")
    list(GET _answer 0 _name)
    list(GET _answer 1 _which)
    string(REPLACE "," ";" _which "${_which}")
    set(_body "")
    foreach(_v IN LISTS _which)
        list(APPEND _body "${${_v}}")
    endforeach()
    list(JOIN _body "," _body)
    file(WRITE "${WORK}/box-${_name}.json" "[${_body}]")
    list(APPEND _answers /api/data/metar "${WORK}/box-${_name}.json")
endforeach()

# **The points**: 5 places, 3 heights, 10 moments - 150, each a line.
set(_places "-33.946 151.173" "-33.75 150.80" "-33.25 151.40" "-34.60 150.0"
            "-32.794 151.836")
set(_heights 9 150 1200)
set(_moments 120 150 200 300 400 500 590 605 650 1000)
set(_points "${WORK}/points.txt")
set(_lines "")
set(_wanted 0)
foreach(_t IN LISTS _moments)
    foreach(_p IN LISTS _places)
        foreach(_h IN LISTS _heights)
            string(APPEND _lines "${_t} ${_p} ${_h}\n")
            math(EXPR _wanted "${_wanted} + 1")
        endforeach()
    endforeach()
endforeach()
list(LENGTH _places _np)
list(LENGTH _heights _nh)
list(LENGTH _moments _nt)
math(EXPR _space "${_np} * ${_nh} * ${_nt}")
if(NOT _wanted EQUAL _space OR NOT _wanted EQUAL 150)
    message(FATAL_ERROR "${_wanted} points written, of ${_space}; 150 were meant")
endif()
file(WRITE "${_points}" "# TIME LAT LON HEIGHT\n${_lines}")

set(_port_file "${WORK}/stub.port")
set(_looked_after "${WORK}/looked-after-d")
set(_result_file "${WORK}/result.txt")
file(REMOVE "${_port_file}" "${_result_file}" "${_looked_after}")
execute_process(
    COMMAND "${STUB}" --stall --answered "${_looked_after}" 200 "${_port_file}"
            ${_answers} /v1/forecast "${_forecast}"
    COMMAND "${CMAKE_COMMAND}" "-DSERVER=${SERVER}" "-DCLIENT=${CLIENT}" "-DDATA=${DATA}"
            "-DCACHE=${CACHE}" "-DWORK=${WORK}" "-DPORT=${PORT}" "-DKEY=${_key}"
            "-DSTORE=${_store}" "-DPLAN=${_plan}" "-DSTUB_PORT_FILE=${_port_file}"
            "-DRESULT_FILE=${_result_file}" "-DDRIVER=${DRIVER}" "-DPOINTS=${_points}"
            "-DLOOKED_AFTER=${_looked_after}"
            -P "${CMAKE_CURRENT_LIST_DIR}/window_joins_mid_change.cmake"
    RESULTS_VARIABLE _stub_rcs OUTPUT_VARIABLE _stub_out ERROR_VARIABLE _stub_said)
port_listened("${_stub_out}" "${_stub_said}")
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the programs were not run against the stub (${_stub_rcs}):\n"
                        "${_stub_said}")
endif()
file(READ "${_result_file}" _ran)
if(NOT EXISTS "${WORK}/joined.bmp" AND _ran MATCHES "no GPU|could not create|no device")
    message(STATUS "the client cannot draw here:\n${_ran}")
    cmake_language(EXIT 77)
endif()
if(NOT _ran MATCHES "=== exits 0;0\n")
    message(FATAL_ERROR "the programs did not both exit 0:\n${_ran}\n${_stub_said}")
endif()

# **The stations changed as built**: the first, and B, C and D after them -
# and nothing after D.
string(REGEX MATCHALL "stations at [0-9.]+ s: [0-9]+ found" _changes "${_ran}")
list(LENGTH _changes _changed)
if(NOT _changed EQUAL 4)
    message(FATAL_ERROR "the server said ${_changed} findings of its stations, not the "
                        "first and three after it (${_changes}) - a client slower than the "
                        "stalled look's 60 s sees one more:\n${_ran}")
endif()
list(GET _changes 3 _d)
if(NOT _d MATCHES ": 2 found$")
    message(FATAL_ERROR "the last stations found were not D's two: ${_d}\n${_ran}")
endif()
foreach(_id IN ITEMS YSSY YSWS YSRI YWLM)
    if(NOT _ran MATCHES "station ${_id} at ")
        message(FATAL_ERROR "the server never flew ${_id}:\n${_ran}")
    endif()
endforeach()

# **Joined mid-change**: slot 1 told three weathers - YSWS, its new report,
# YSRI - all on joining, the stub having held every look after D.
foreach(_told IN ITEMS "1, station 1\\): METAR YSWS 102100Z" "2, station 1\\): METAR YSWS 102130Z"
                        "3, station 1\\): METAR YSRI ")
    if(NOT _ran MATCHES "glideslope: flying the server's weather \\(${_told}")
        message(FATAL_ERROR "the client was not told slot 1's weather '${_told}' on "
                            "joining:\n${_ran}")
    endif()
endforeach()

# **The air, to the last bit.**
foreach(_who IN ITEMS server client)
    if(NOT EXISTS "${WORK}/${_who}-air.txt")
        message(FATAL_ERROR "the ${_who} did not say its air:\n${_ran}")
    endif()
    file(STRINGS "${WORK}/${_who}-air.txt" _${_who})
endforeach()
list(GET _client 0 _when)
list(REMOVE_AT _client 0)
if(NOT _when MATCHES "^said at ([0-9.]+) s$")
    message(FATAL_ERROR "the client did not say when it said its air: ${_when}")
endif()
set(_said_at "${CMAKE_MATCH_1}")
list(GET _moments 0 _first_moment)
if(NOT _said_at LESS _first_moment)
    message(FATAL_ERROR "the client said its air at ${_said_at} s of the session, after "
                        "the first moment compared, ${_first_moment} s")
endif()
list(LENGTH _server _server_lines)
list(LENGTH _client _client_lines)
if(NOT _server_lines EQUAL _wanted OR NOT _client_lines EQUAL _wanted)
    message(FATAL_ERROR "the server said ${_server_lines} points and the client "
                        "${_client_lines}, of ${_wanted}:\n${_ran}")
endif()
# **The wet runway among them**: YSSY's rain over, its runways still wet.
set(_wet 0)
foreach(_line IN LISTS _server)
    if(_line MATCHES "^[^:]*: [^ ]+ [^ ]+ [^ ]+ [^ ]+ [^ ]+ [0-9]+ [^ ]+ 5 [^ ]+$")
        math(EXPR _wet "${_wet} + 1")
    endif()
endforeach()
if(_wet EQUAL 0)
    message(FATAL_ERROR "no point compared is on YSSY's wet runway, which A's rain leaves")
endif()
set(_same 0)
set(_first_different "")
math(EXPR _last "${_wanted} - 1")
foreach(_i RANGE ${_last})
    list(GET _server ${_i} _s)
    list(GET _client ${_i} _c)
    if(_s STREQUAL _c)
        math(EXPR _same "${_same} + 1")
    elseif(_first_different STREQUAL "")
        set(_first_different "server ${_s}\n  client ${_c}")
    endif()
endforeach()
message(STATUS "the client, said at ${_said_at} s, flies the server's air at ${_same} of "
               "${_wanted} points, ${_wet} of them wet")
if(NOT _same EQUAL _wanted)
    message(FATAL_ERROR "the window client joined mid-change flies the server's air at "
                        "${_same} of ${_wanted} points; the first that differs:\n"
                        "  ${_first_different}")
endif()
