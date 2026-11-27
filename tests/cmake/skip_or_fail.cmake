# skip_or_fail.cmake - when a test that cannot do its work reports itself
# skipped, and when that is a failure.
#
# A skip is for what is missing from a machine - no network, no GPU, no
# display - never for a fault: a test that skips on a fault turns green
# while it tests nothing. On macOS a dozen window-client tests said
# "skipped" after 300 s run after run (train #379's run 43845148929), and
# nobody saw they had never run: they asked for Vulkan, which macOS has not
# got, and each one's server waited for a client that never came.
#
# Included, it gives three functions.
#
#   glideslope_warmed_up("${_rc}" "${_err}")
#
# after the warm-up server's execute_process: the server run for a second
# (`--seconds 1`) so that the DEM's tiles around its aircraft are fetched
# and cached before the run a test judges, with `rc` its RESULT_VARIABLE and
# `err` its ERROR_VARIABLE. It returns when the server ran well; otherwise
#
#   - **timed out: failed.** A warm-up still running at its limit is a
#     server that did not get its terrain, which is a fault to be seen.
#   - **otherwise as glideslope_skip_when_not_downloaded says**, below:
#     skipped for weather not to be had, and for a download where the
#     network is not required (GLIDESLOPE_REQUIRE_NETWORK unset);
#   - **and anything else failed**, with what the server said.
#
#   glideslope_cannot_draw("<why>")
#
# where a client could not open its GPU device: skipped, unless
# GLIDESLOPE_REQUIRE_WINDOW is set - CI sets it - and then failed.
#
# tests/cmake/skip_or_fail_check.cmake walks every case of the warm-up and
# drawing rules.

# **The one rule for a run that could not download what it needed**, given
# its exit code and standard error, and any other words (ALSO, a regular
# expression) that say the same of this test's own downloads. It reports the
# test skipped (exit 77), never passed:
#
# - "the weather could not be had": always. The weather is live, somebody
#   else's and never kept, so a service having a bad minute is not a fault
#   here, even where the network is required.
# - "could not download", or ALSO: unless GLIDESLOPE_REQUIRE_NETWORK is set.
#   What else is downloaded - the DEM, the geoid, imagery - is pinned and
#   kept, so CI requires it and a failure there is a failure.
#
# Anything else, or a run that exited 0, it leaves to the caller.
#
# **A run whose imagery could not be fetched draws its frame anyway**, on
# the DEM's colours, exits 0 and says so; a test that is about the imagery
# itself asks glideslope_skip_when_imagery_not_fetched, below, as well.
function(glideslope_skip_when_not_downloaded rc err)
    if(rc EQUAL 0)
        return()
    endif()
    if(err MATCHES "the weather could not be had")
        message(STATUS "there is no weather to fly in, so this is skipped: ${err}")
        cmake_language(EXIT 77)
    endif()
    set(_pattern "could not download")
    if(ARGC GREATER 2)
        string(APPEND _pattern "|${ARGV2}")
    endif()
    if(err MATCHES "${_pattern}" AND "$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "what this needs could not be downloaded, so it is skipped: ${err}")
        cmake_language(EXIT 77)
    endif()
endfunction()

function(glideslope_warmed_up rc err)
    if("${rc}" STREQUAL "0")
        return()
    endif()
    if("${rc}" MATCHES "timeout")
        message(FATAL_ERROR
                "the warm-up server did not get its terrain inside its limit (${rc}):\n${err}")
    endif()
    glideslope_skip_when_not_downloaded("${rc}" "${err}" "cannot download|no network")
    message(FATAL_ERROR "the warm-up server stopped (exit ${rc}):\n${err}")
endfunction()

function(glideslope_cannot_draw why)
    if(NOT "$ENV{GLIDESLOPE_REQUIRE_WINDOW}" STREQUAL "")
        message(FATAL_ERROR "the client could not draw, and GLIDESLOPE_REQUIRE_WINDOW is set: ${why}")
    endif()
    message(STATUS "the client cannot draw here, so this is skipped: ${why}")
    cmake_language(EXIT 77)
endfunction()
