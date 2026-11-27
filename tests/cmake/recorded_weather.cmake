# recorded_weather.cmake - recorded weather, made ready for glideslope_http_stub
# to answer a flight with in place of aviationweather.gov and Open-Meteo.
#
#   include(recorded_weather.cmake)
#   glideslope_recorded_weather(<METARs recorded> <station> <forecast recorded>
#                               <METAR file out> <forecast file out>)
#
# **The station's METAR alone.** aviationweather.gov, asked `ids=YSSY`,
# answers YSSY's report and nothing else, and the client takes the first it is
# given. A recording of several stations is cut to the station's own object:
# from its `{"icaoId":"<station>"` to the next station's, or the end.
#
# **The forecast's hours, three days of them**, as client_quits_mid_refresh.cmake
# makes them: a flight asks Open-Meteo for the hour it is now, UTC, so the
# recorded day's times are given as yesterday's, today's and tomorrow's, and
# each series three times over - a run crossing midnight still finds its hour.
# string(TIMESTAMP) reads SOURCE_DATE_EPOCH, if set, for the moment it
# formats, so any the environment brought is put aside first, and put back
# after.

function(glideslope_recorded_weather metars station forecast metar_out forecast_out)
    file(READ "${metars}" _text)
    string(FIND "${_text}" "{\"icaoId\":\"${station}\"" _from)
    if(_from LESS 0)
        message(FATAL_ERROR "${metars} has no METAR from ${station}")
    endif()
    string(SUBSTRING "${_text}" ${_from} -1 _rest)
    string(FIND "${_rest}" ",{\"icaoId\":" _to)
    if(_to LESS 0)
        string(FIND "${_rest}" "]" _to REVERSE)
    endif()
    string(SUBSTRING "${_rest}" 0 ${_to} _one)
    file(WRITE "${metar_out}" "[${_one}]")

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

    file(READ "${forecast}" _text)
    if(NOT _text MATCHES "\"time\":\\[(\"([0-9-]+)T[^]]*)\\]")
        message(FATAL_ERROR "${forecast} has no hourly times")
    endif()
    set(_hours "${CMAKE_MATCH_1}")
    set(_recorded_day "${CMAKE_MATCH_2}")
    set(_times "")
    foreach(_day IN LISTS _days)
        string(REPLACE "\"${_recorded_day}T" "\"${_day}T" _these "${_hours}")
        list(APPEND _times "${_these}")
    endforeach()
    list(JOIN _times "," _times)
    string(REPLACE "\"time\":[${_hours}]" "\"time\":@TIMES@" _text "${_text}")
    string(REGEX REPLACE "\\[([^]]*)\\]" "[\\1,\\1,\\1]" _text "${_text}")
    string(REPLACE "@TIMES@" "[${_times}]" _text "${_text}")
    file(WRITE "${forecast_out}" "${_text}")
endfunction()
