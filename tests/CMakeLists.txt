# Every test is named as a sentence stating the fact it pins.

set(GLIDESLOPE_EXPECT_RUN "${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_run.cmake")

# ---------------------------------------------------------------------------
# The platform gate, walked in script mode with toolchains described by -D
# flags - a 32-bit toolchain and old compilers are not things any CI machine
# has, so they are described rather than installed.
# ---------------------------------------------------------------------------
add_test(NAME a_32_bit_toolchain_is_refused_on_every_platform
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -DCASES=pointer-size
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_gate.cmake)

add_test(NAME every_compiler_is_refused_below_its_floor_and_accepted_at_it
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -DCASES=compiler-floors
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_gate.cmake)

# ---------------------------------------------------------------------------
# glideslope_cli's whole surface, today: every argument it accepts, none, and
# one it does not.
# ---------------------------------------------------------------------------
add_test(NAME the_cli_reports_the_project_version
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DARGS=--version -DEXIT=0
        "-DSTDOUT_IS=glideslope_cli ${PROJECT_VERSION}"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_cli_prints_its_usage_when_asked
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DARGS=--help -DEXIT=0
        -DSTDOUT_MATCHES=^usage:
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_cli_with_no_arguments_prints_its_usage_and_fails
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DEXIT=2
        -DSTDERR_MATCHES=^usage:
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_cli_refuses_an_argument_it_does_not_know
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DARGS=--fly -DEXIT=2
        -DSTDERR_MATCHES=^usage:
        -P ${GLIDESLOPE_EXPECT_RUN})
