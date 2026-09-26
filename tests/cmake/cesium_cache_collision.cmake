# cesium_cache_collision.cmake - two programs writing one Cesium cache at once
# both store everything they are given.
#
#   cmake -DTOOL=<glideslope_cache_collision> -DWORK=<dir> -DCOUNT=<n>
#         -P cesium_cache_collision.cmake
#
# **Why**: CesiumAsync::SqliteCache sets no busy timeout, and leaves a read
# transaction open after a cache hit, so a second program writing the same
# file had its stores refused at once - "database is locked" - and the rendering
# tests kept tripping over it however their caches were named
# (src/gfx/cesium_cache.hpp). The collision is built, not hoped for: a third
# program holds the file's write lock, and makes every read taken before it
# stale, until both writers are waiting on it; then the two write COUNT
# entries each, against each other, reading each back between stores.
# tests/tools/cache_collision.cpp says how each step waits for the last.

foreach(_v TOOL WORK COUNT)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "cesium_cache_collision.cmake needs -D${_v}")
    endif()
endforeach()

set(_dir "${WORK}/cesium-cache-collision")
file(REMOVE_RECURSE "${_dir}")
file(MAKE_DIRECTORY "${_dir}")
set(_cache "${_dir}/cache.sqlite")

# One execute_process with three commands runs all three at once.
execute_process(
    COMMAND "${TOOL}" hold "${_cache}" "${_dir}" a b
    COMMAND "${TOOL}" write "${_cache}" "${_dir}" a ${COUNT}
    COMMAND "${TOOL}" write "${_cache}" "${_dir}" b ${COUNT}
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rcs STREQUAL "0;0;0")
    message(FATAL_ERROR "the holder and the two writers exited ${_rcs}:\n${_err}${_out}")
endif()

foreach(_who a b)
    execute_process(COMMAND "${TOOL}" count "${_cache}" ${_who} ${COUNT}
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    string(APPEND _err "${_e}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "writer ${_who}'s entries are not all in the cache:\n${_err}")
    endif()
endforeach()

# Cesium Native logs a refused write and carries on; a line saying so is a
# failure whatever the counts say.
if(_err MATCHES "database is locked" OR _out MATCHES "database is locked")
    message(FATAL_ERROR "a write was refused with \"database is locked\":\n${_err}${_out}")
endif()
message(STATUS "${_err}")
