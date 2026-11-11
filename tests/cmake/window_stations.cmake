# window_stations.cmake - a flight on its own flies the stations around her,
# the nearer's weather, with no step in the wind.
#
#   cmake -DPROGRAM=<glideslope> -DSTUB=<glideslope_http_stub> -DDRIVER=<gpu driver>
#         -DWORK=<scratch> -DCACHE=<downloads dir> -DMETAR_FILE=<recorded bbox answer>
#         -DFORECAST_FILE=<recorded Open-Meteo answer> -DSTEP_MPS=<m/s>
#         -P window_stations.cmake
#
# **The stations around her, from recordings, with no server.** The window's
# flight is told `--weather around`: it asks aviationweather.gov for the
# METARs in a box around her (`bbox=`) and Open-Meteo for the winds aloft
# over each station it keeps - both answered by glideslope_http_stub from
# files, never the network: aviationweather.gov's answer around Sydney
# recorded at 2100Z on 2026-11-10, and Open-Meteo's Sydney forecast of
# 2026-09-17 given three days of the same hours (client_quits_mid_refresh.cmake
# says why). The stations are kept, slotted and blended by the same code as
# glideslope_server's (world::fetch_stations_weather, world::slot_changes,
# world::StationSlots), and the air she flew said by the same as the headless
# client's (frontend::AirFlown).
#
# **A flight from one station's air into the next's.** Her plan, written
# here, starts 10.5 km from YSSY on the line to YSWS, 42.8 km apart, and
# flies along it towards YSWS at 100 kt: at the start YSWS is 21.8 km
# farther, so the air is YSSY's alone. She flies 450 s, about 24 km: through
# the band where the two blend (world::station_blend_m), across the
# midpoint, its steepest part, and out into YSWS's alone. She looks for the
# stations again every 10 km (`--restation`, 50 km but for this).
# **Built, not hoped for**:
#
# - she keeps four stations, YSSY, YSWS, YSRI and YWLM (YSCB, 248 km, not),
#   and looks again for them as she flies on;
# - the air she flew: YSSY's alone at the first step, YSWS's alone at the
#   last, YSSY's share down to nought between, and the wind changing by no
#   more than STEP_MPS in any step - where the worst came is said.
#
# Without the DEM it is skipped (exit 77), never passed.
#
# With STUB_PORT_FILE, this is the inner half, started beside the stub: it
# waits for the stub's port, flies her against it, writes what she said to
# RESULT_FILE, and stops the stub.

cmake_minimum_required(VERSION 3.28)

if(DEFINED STUB_PORT_FILE)
    while(NOT EXISTS "${STUB_PORT_FILE}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
    endwhile()
    file(STRINGS "${STUB_PORT_FILE}" _stub LIMIT_COUNT 1)
    set(ENV{GLIDESLOPE_WEATHER_SERVICE} "http://127.0.0.1:${_stub}")
    set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
    set(ENV{LSAN_OPTIONS} "exitcode=0")
    execute_process(
        COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 320x240
                --screen flight --weather around --restation 10000 --plan "${PLAN}"
                --imagery off --shot-at 54100 --quit-at 54000
                --shot "${WORK}/stations-${DRIVER}.bmp"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    file(WRITE "${RESULT_FILE}" "=== exited ${_rc}\n${_out}\n${_err}\n")
    file(DOWNLOAD "http://127.0.0.1:${_stub}/stop" "${RESULT_FILE}.stop")
    return()
endif()

foreach(_needed PROGRAM STUB DRIVER WORK CACHE METAR_FILE FORECAST_FILE STEP_MPS HOUR)
    if(NOT DEFINED ${_needed})
        message(FATAL_ERROR "window_stations.cmake needs ${_needed}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${WORK}")

# Her plan: from 10.5 km out of YSSY on the line to YSWS, at 2,500 ft,
# heading 279 true - along that line - at 100 kt, to a point beyond YSWS.
set(_plan "${WORK}/towards-ysws.plan")
file(WRITE "${_plan}" "aircraft c172p\nstart -33.93176 151.06051 2500 279 100\n"
                      "waypoint WEST -33.880 150.600 2500 100 flyover\n")

# **One hour's air, whatever the hour she flies** (`HOUR`, of the recorded
# day). The flight asks Open-Meteo for the hour it is when she flies, UTC,
# and the forecast's temperatures aloft at that hour decide the thermals over
# the ground (world::lift_of): given every hour of the recording, her air - and
# so the wind's worst step - would be the clock's. So every hour of yesterday,
# today and tomorrow, UTC, is answered with the recording's hour `HOUR`: the
# same air at any hour, a run crossing midnight still finding its hour
# (client_quits_mid_refresh.cmake). The hour is 13Z: in its air her height
# above the ground crosses the top of YSWS's convective layer (679.7 m) late
# in her flight, where the thermals once stepped the wind 0.141 m/s
# (docs/status/2026-11-11-window-stations-wind-step.md), so the bound holds
# there too.
if(NOT HOUR MATCHES "^[0-9]+$" OR HOUR GREATER 23)
    message(FATAL_ERROR "HOUR is an hour of the recorded day, 0 to 23, not '${HOUR}'")
endif()
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
string(REPLACE "," ";" _hours "${_hours}")
list(LENGTH _hours _count)
if(NOT _count EQUAL 24)
    message(FATAL_ERROR "${FORECAST_FILE} has ${_count} hours, not a day's 24")
endif()
set(_times "")
foreach(_day IN LISTS _days)
    foreach(_hour RANGE 23)
        if(_hour LESS 10)
            set(_hour "0${_hour}")
        endif()
        list(APPEND _times "\"${_day}T${_hour}:00\"")
    endforeach()
endforeach()
list(JOIN _times "," _times)
string(REPLACE "\"time\":[${CMAKE_MATCH_1}]" "\"time\":[${_times}]" _text "${_text}")
string(REGEX MATCHALL "\"[A-Za-z0-9_]+\":\\[[^]\"]*\\]" _series "${_text}")
set(_replaced 0)
foreach(_one IN LISTS _series)
    string(REGEX MATCH "^(\"[A-Za-z0-9_]+\":)\\[([^]]*)\\]$" _ "${_one}")
    set(_name "${CMAKE_MATCH_1}")
    string(REPLACE "," ";" _values "${CMAKE_MATCH_2}")
    list(LENGTH _values _n)
    if(NOT _n EQUAL 24)
        message(FATAL_ERROR "${_name} has ${_n} values in ${FORECAST_FILE}, not 24")
    endif()
    list(GET _values ${HOUR} _value)
    set(_repeated "")
    foreach(_i RANGE 71)
        list(APPEND _repeated "${_value}")
    endforeach()
    list(JOIN _repeated "," _repeated)
    string(REPLACE "${_one}" "${_name}[${_repeated}]" _text "${_text}")
    math(EXPR _replaced "${_replaced} + 1")
endforeach()
# Every series but the times, each a level's wind speed, direction, height or
# temperature: none left with the recording's other hours.
if(_replaced EQUAL 0)
    message(FATAL_ERROR "${FORECAST_FILE} has no hourly series")
endif()
set(_forecast "${WORK}/forecast-${DRIVER}.json")
file(WRITE "${_forecast}" "${_text}")

set(_port_file "${WORK}/stub-${DRIVER}.port")
set(_result_file "${WORK}/result-${DRIVER}.txt")
file(REMOVE "${_port_file}" "${_result_file}")
execute_process(
    COMMAND "${STUB}" 404 "${_port_file}"
            /api/data/metar "${METAR_FILE}" /v1/forecast "${_forecast}"
    COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=${PROGRAM}" "-DDRIVER=${DRIVER}" "-DCACHE=${CACHE}"
            "-DWORK=${WORK}" "-DPLAN=${_plan}" "-DSTUB_PORT_FILE=${_port_file}"
            "-DRESULT_FILE=${_result_file}" -P "${CMAKE_CURRENT_LIST_DIR}/window_stations.cmake"
    RESULTS_VARIABLE _stub_rcs ERROR_VARIABLE _stub_said)
if(NOT EXISTS "${_result_file}")
    message(FATAL_ERROR "the flight was not flown against the stub (${_stub_rcs}):\n"
                        "${_stub_said}")
endif()
file(READ "${_result_file}" _ran)
if(NOT _ran MATCHES "=== exited 0\n")
    if(_ran MATCHES "could not download" AND NOT _ran MATCHES "127\\.0\\.0\\.1" AND
       "$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "the DEM could not be had here, so this is skipped: ${_ran}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "the flight did not exit 0:\n${_ran}\n${_stub_said}")
endif()

foreach(_station IN ITEMS YSSY YSWS YSRI YWLM)
    if(NOT _ran MATCHES "glideslope: station ${_station} at ")
        message(FATAL_ERROR "she did not keep ${_station}:\n${_ran}")
    endif()
endforeach()
if(_ran MATCHES "glideslope: station YSCB at ")
    message(FATAL_ERROR "she kept YSCB, 248 km away:\n${_ran}")
endif()
# **She looked again** as she flew on: at 10 km (`--restation`), and 20.
if(NOT _ran MATCHES "glideslope: stations at [0-9.]+ s: 4 found\n")
    message(FATAL_ERROR "she never looked again for the stations around her:\n${_ran}")
endif()
set(_said_air "air flown: ([0-9]+) steps, the wind changing by at most ([0-9.]+) m/s in one, ")
string(APPEND _said_air "at step ([0-9]+), where the first's share was ([0-9.]+); most of it ")
string(APPEND _said_air "([A-Z0-9]+)'s at the first \\(([0-9.]+) of it\\), ([A-Z0-9]+)'s at the last ")
string(APPEND _said_air "\\(([0-9.]+) of it\\), the first's share at least ([0-9.]+)")
if(NOT _ran MATCHES "${_said_air}")
    message(FATAL_ERROR "she did not say the air flown:\n${_ran}")
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
