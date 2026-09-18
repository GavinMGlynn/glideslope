# frame_weather.cmake - weather you can see: cloud where a report puts it,
# haze that hides what is beyond its visibility, and rain.
#
#   cmake -DPROGRAM=<glideslope> -DCLI=<glideslope_cli>
#         -DSKY=<glideslope_sky_check> -DTERRAIN=<glideslope_terrain_check>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir> -DDATA=<data dir>
#         -P frame_weather.cmake
#
# **Cloud.** A report of broken cloud at 1,500 ft, observed at Hawera on the
# Taranaki plain. `glideslope_cli sky` gives the deck's base from the DEM's
# ground there, and the nearest places where the deck is thick cloud and
# where it has a gap. Over thick cloud, from 30 m below the base, the frame
# looking straight up is cloud and looking down is the ground; from 30 m above
# the base, looking level, it is a whiteout. In the gap, from 30 m above the
# base, looking up, it is clear sky. So the base is drawn within 30 m - 100 ft - of where the report puts it,
# and broken: cloud and gaps where the deck has them.
#
# **Visibility.** A report of 3,000 m in mist, observed on Taranaki's eastern
# slope, seen from 465 m above the slope looking down it: glideslope_terrain_
# check holds the frame to the DEM ray-cast faded by Koschmieder's law, the
# ground beyond 3,000 m hidden and the ground within 1,000 m seen.
#
# **Rain.** The same report in heavy rain, and dry: streaks, lighter than what
# is behind them.
#
# The DEM's tiles and the geoid come from CACHE: without the network the test
# is skipped (exit 77) unless GLIDESLOPE_REQUIRE_NETWORK is set.

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

set(_station_lat -39.553)
set(_station_lon 174.267)
set(_broken "METAR NZHA 180800Z 27012KT 9999 BKN015 14/10 Q1016")
set(_rain "METAR NZHA 180800Z 27012KT 6000 +RA BKN015 14/10 Q1016")
set(_dry "METAR NZHA 180800Z 27012KT 6000 BKN015 14/10 Q1016")
set(_mist "METAR NZNP 180800Z 27008KT 3000 BR NSC 12/11 Q1015")

file(MAKE_DIRECTORY "${WORK}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

function(skip_without_network rc err)
    if(NOT rc EQUAL 0 AND err MATCHES "could not download")
        if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
            message(STATUS "the DEM could not be had: ${err}")
            cmake_language(EXIT 77)
        endif()
    endif()
endfunction()

# "581.518", with three decimals, as whole millimetres; and back.
function(millimetres text out)
    if(NOT text MATCHES "^(-?)([0-9]+)\\.([0-9][0-9][0-9])$")
        message(FATAL_ERROR "not a height to the millimetre: ${text}")
    endif()
    set(_sign "${CMAKE_MATCH_1}")
    set(_whole "${CMAKE_MATCH_2}")
    set(_part "${CMAKE_MATCH_3}")
    string(REGEX REPLACE "^0+([0-9])" "\\1" _whole "${_whole}")
    string(REGEX REPLACE "^0+([0-9])" "\\1" _part "${_part}")
    math(EXPR _v "${_sign}(${_whole} * 1000 + ${_part})")
    set(${out} ${_v} PARENT_SCOPE)
endfunction()
function(metres value out)
    set(_sign "")
    if(value LESS 0)
        set(_sign "-")
        math(EXPR value "-(${value})")
    endif()
    math(EXPR _whole "${value} / 1000")
    math(EXPR _part "${value} % 1000")
    string(LENGTH "${_part}" _digits)
    while(_digits LESS 3)
        set(_part "0${_part}")
        string(LENGTH "${_part}" _digits)
    endwhile()
    set(${out} "${_sign}${_whole}.${_part}" PARENT_SCOPE)
endfunction()

# Where the broken deck should be: 1,500 ft - 457.2 m - above the station's
# ground, which the DEM gives.
execute_process(COMMAND "${CLI}" height ${_station_lat} ${_station_lon}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _height ERROR_VARIABLE _err)
skip_without_network(${_rc} "${_err}")
if(NOT _rc EQUAL 0 OR NOT _height MATCHES "above the WGS84 ellipsoid +([0-9]+\\.[0-9][0-9][0-9]) m")
    message(FATAL_ERROR "glideslope_cli height exited ${_rc}\n${_height}${_err}")
endif()
millimetres("${CMAKE_MATCH_1}" _ground)
math(EXPR _base "${_ground} + 457200")
metres(${_base} _base_text)

# Where it is thick and where it has gaps.
execute_process(COMMAND "${CLI}" sky "${_broken}" ${_station_lat} ${_station_lon}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _sky ERROR_VARIABLE _err)
skip_without_network(${_rc} "${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "glideslope_cli sky exited ${_rc}\n${_err}")
endif()
message(STATUS "${_sky}")
if(NOT _sky MATCHES "deck 1 cover 0\\.750 base ([0-9.]+) [^\n]*base ([0-9.]+) m above the ellipsoid")
    message(FATAL_ERROR "no broken deck in:\n${_sky}")
endif()
if(NOT CMAKE_MATCH_2 STREQUAL _base_text)
    message(FATAL_ERROR "the deck's base is ${CMAKE_MATCH_2} m above the ellipsoid, not "
                        "1,500 ft above the ground there, ${_base_text} m")
endif()
if(NOT _sky MATCHES "cloudy at (-?[0-9.]+,-?[0-9.]+)")
    message(FATAL_ERROR "no thick cloud near the station:\n${_sky}")
endif()
set(_cloudy "${CMAKE_MATCH_1}")
if(NOT _sky MATCHES "clear at (-?[0-9.]+,-?[0-9.]+)")
    message(FATAL_ERROR "no gap near the station:\n${_sky}")
endif()
set(_clear "${CMAKE_MATCH_1}")
math(EXPR _below_mm "${_base} - 30000")
math(EXPR _inside_mm "${_base} + 30000")
math(EXPR _below_up_mm "${_below_mm} + 1000000")
math(EXPR _inside_up_mm "${_inside_mm} + 1000000")
metres(${_below_mm} _below)
metres(${_inside_mm} _inside)
metres(${_below_up_mm} _below_up)
metres(${_inside_up_mm} _inside_up)

# Shoots the terrain screen, tinted, in a report's weather.
function(shoot name at toward report)
    set(_shot "${WORK}/weather-${name}-${DRIVER}.bmp")
    file(REMOVE "${_shot}")
    execute_process(COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 320x240
                            --screen terrain --at "${at}" --toward "${toward}"
                            --imagery off --metar "${report}" ${ARGN}
                            --shot "${_shot}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    skip_without_network(${_rc} "${_err}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope exited ${_rc}\n${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
endfunction()

# Holds a shot to what it should show.
function(judge name)
    execute_process(COMMAND "${SKY}" "${WORK}/weather-${name}-${DRIVER}.bmp" ${ARGN}
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    message(STATUS "${name}: ${_out}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${name}: ${_out}${_err}")
    endif()
endfunction()

set(_at_station --station "${_station_lat},${_station_lon}")
shoot(below-up "${_cloudy},${_below}" "${_cloudy},${_below_up}" "${_broken}" ${_at_station})
judge(below-up cloud)
shoot(below-down "${_cloudy},${_below}" "-39.56,174.28,300" "${_broken}" ${_at_station})
judge(below-down ground)
# Level, toward the gap: inside cloud nothing is seen whichever way one looks,
# where between the deck's sheets one would see its base below and its top
# above.
shoot(inside "${_cloudy},${_inside}" "${_clear},${_inside}" "${_broken}" ${_at_station})
judge(inside whiteout)
shoot(gap "${_clear},${_inside}" "${_clear},${_inside_up}" "${_broken}" ${_at_station})
judge(gap sky)

# Visibility.
set(_eye "-39.33,174.155,1100")
set(_down_the_slope "-39.33636,174.16321,634")
shoot(mist "${_eye}" "${_down_the_slope}" "${_mist}")
execute_process(COMMAND "${TERRAIN}" "${WORK}/weather-mist-${DRIVER}.bmp"
                        "${WORK}/weather-mist-${DRIVER}-reference.bmp"
                        "${WORK}/weather-mist-${DRIVER}-difference.bmp" "${_eye}"
                        "${_down_the_slope}" "${CACHE}" "${DATA}" visibility=3000
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "mist: ${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mist: ${_out}${_err}")
endif()

# Rain.
set(_low "${_station_lat},${_station_lon},300")
set(_across "-39.54,174.24,500")
shoot(rain "${_low}" "${_across}" "${_rain}")
shoot(dry "${_low}" "${_across}" "${_dry}")
judge(rain rain "${WORK}/weather-dry-${DRIVER}.bmp")
