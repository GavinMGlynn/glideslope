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
# Included, it gives two functions.
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
#   - **could not download: skipped**, where the network is not required
#     (GLIDESLOPE_REQUIRE_NETWORK unset); CI requires it, and there it fails.
#   - **anything else: failed**, with what the server said.
#
#   glideslope_cannot_draw("<why>")
#
# where a client could not open its GPU device: skipped, unless
# GLIDESLOPE_REQUIRE_WINDOW is set - CI sets it - and then failed.
#
# tests/cmake/skip_or_fail_check.cmake walks every case of both.

function(glideslope_warmed_up rc err)
    if("${rc}" STREQUAL "0")
        return()
    endif()
    if("${rc}" MATCHES "timeout")
        message(FATAL_ERROR
                "the warm-up server did not get its terrain inside its limit (${rc}):\n${err}")
    endif()
    if(err MATCHES "could not download|cannot download|no network")
        if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
            message(STATUS "the server could not get its terrain, so this is skipped: ${err}")
            cmake_language(EXIT 77)
        endif()
        message(FATAL_ERROR "the server could not download its terrain, "
                            "and GLIDESLOPE_REQUIRE_NETWORK is set:\n${err}")
    endif()
    message(FATAL_ERROR "the warm-up server stopped (exit ${rc}):\n${err}")
endfunction()

function(glideslope_cannot_draw why)
    if(NOT "$ENV{GLIDESLOPE_REQUIRE_WINDOW}" STREQUAL "")
        message(FATAL_ERROR "the client could not draw, and GLIDESLOPE_REQUIRE_WINDOW is set: ${why}")
    endif()
    message(STATUS "the client cannot draw here, so this is skipped: ${why}")
    cmake_language(EXIT 77)
endfunction()
