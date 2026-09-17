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

# ---------------------------------------------------------------------------
# The simulation links no presentation: every forbidden include, SDL arriving in
# the link graph by every route, and what the CLI binary really depends on.
# ---------------------------------------------------------------------------
add_test(NAME every_forbidden_include_in_the_simulation_fails_the_configure
    COMMAND ${CMAKE_COMMAND} -DGLIDESLOPE_ROOT=${PROJECT_SOURCE_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/layering-forbidden -DCASES=forbidden
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/layering_includes.cmake)

add_test(NAME includes_that_only_look_like_presentation_are_allowed_in_the_simulation
    COMMAND ${CMAKE_COMMAND} -DGLIDESLOPE_ROOT=${PROJECT_SOURCE_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/layering-allowed -DCASES=allowed
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/layering_includes.cmake)

add_test(NAME sdl_linked_into_the_simulation_by_any_route_fails_the_configure
    COMMAND ${CMAKE_COMMAND} -DGLIDESLOPE_ROOT=${PROJECT_SOURCE_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/layering-links
        "-DGENERATOR=${CMAKE_GENERATOR}" "-DCXX=${CMAKE_CXX_COMPILER}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/layering_links.cmake)

add_test(NAME the_cli_links_the_simulation_and_nothing_presentational
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DSYSTEM=${CMAKE_SYSTEM_NAME}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/binary_dependencies.cmake)

# ---------------------------------------------------------------------------
# Warnings are errors: deliberate mistakes built with the project's own warning
# set, with this build's generator and compiler, in every build type.
# ---------------------------------------------------------------------------
add_test(NAME a_narrowing_or_sign_conversion_fails_the_build_in_every_build_type
    COMMAND ${CMAKE_COMMAND}
        -DROOT=${PROJECT_SOURCE_DIR} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/warnings
        "-DGENERATOR=${CMAKE_GENERATOR}" "-DCXX=${CMAKE_CXX_COMPILER}"
        -DCOMPILER_ID=${CMAKE_CXX_COMPILER_ID}
        -DFRONTEND=${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/warnings_are_errors.cmake)

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
