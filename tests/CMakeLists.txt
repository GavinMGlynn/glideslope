# Every test is named as a sentence stating the fact it pins.

set(GLIDESLOPE_EXPECT_RUN "${CMAKE_CURRENT_SOURCE_DIR}/cmake/expect_run.cmake")

# For the tests that check a committed asset is what its tool makes.
find_package(Python3 COMPONENTS Interpreter QUIET)

# ---------------------------------------------------------------------------
# Unit tests: one executable, one ctest per test, named as sentences. The list
# below is checked against what the executable holds, both ways.
# ---------------------------------------------------------------------------
add_executable(glideslope_tests
    unit/harness.cpp
    unit/tiff_writer.cpp
    unit/test_dem.cpp
    unit/test_dem_surveyed.cpp
    unit/test_digest.cpp
    unit/test_download.cpp
    unit/test_http.cpp
    unit/test_input.cpp
    unit/test_json.cpp
    unit/test_terrain_mesh.cpp
    unit/test_weather.cpp
    unit/test_figures.cpp
    unit/test_fixed_step.cpp
    unit/test_geodesy.cpp
    unit/test_ground.cpp
    unit/test_geoid.cpp
    unit/test_geotiff.cpp
    unit/test_inflate.cpp
    unit/test_flight.cpp
    unit/test_restore.cpp)
target_link_libraries(glideslope_tests PRIVATE glideslope_sim glideslope_world glideslope_platform
                                               glideslope_input)
# Where tests/cmake/fetch.cmake puts the files the tests download. A directory
# named by GLIDESLOPE_DOWNLOADS when configuring outlives the build tree, which
# is how CI keeps them between runs.
if(DEFINED ENV{GLIDESLOPE_DOWNLOADS})
    file(TO_CMAKE_PATH "$ENV{GLIDESLOPE_DOWNLOADS}" GLIDESLOPE_DOWNLOADS_DIR)
    # Relative to the source tree, as CI names it.
    get_filename_component(GLIDESLOPE_DOWNLOADS_DIR "${GLIDESLOPE_DOWNLOADS_DIR}" ABSOLUTE
                           BASE_DIR "${PROJECT_SOURCE_DIR}")
else()
    set(GLIDESLOPE_DOWNLOADS_DIR "${CMAKE_BINARY_DIR}/downloads")
endif()
target_compile_definitions(glideslope_tests PRIVATE
    GLIDESLOPE_TEST_DOWNLOADS_DIR="${GLIDESLOPE_DOWNLOADS_DIR}"
    GLIDESLOPE_TEST_DATA_DIR="${GLIDESLOPE_DATA_DIR}/jsbsim"
    GLIDESLOPE_TEST_FIGURES_DIR="${GLIDESLOPE_DATA_DIR}/figures"
    GLIDESLOPE_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
glideslope_configure(glideslope_tests)

set(GLIDESLOPE_UNIT_TESTS
    the_fixed_step_counts_steps_from_the_total_time_alone
    the_fixed_step_reports_how_far_it_is_into_the_next_step
    the_fixed_step_refuses_time_running_backwards
    the_fixed_step_counts_a_century_without_overflowing
    a_flight_fed_its_time_in_any_chunks_ends_in_the_same_state
    the_elevator_pulled_back_pitches_the_nose_up
    the_cessna_172p_at_full_throttle_on_the_ground_turns_within_its_static_rpm_range
    the_cessna_172p_takes_off_in_about_its_published_ground_roll
    a_cessna_172p_at_full_power_climbs_near_its_published_rate
    the_cessna_172p_cruises_near_its_published_speed
    the_cessna_172p_glides_near_its_published_ratio
    the_cessna_172p_stalls_flaps_up_near_its_published_speed
    the_cessna_172p_stalls_with_10_degrees_of_flap_near_its_published_speed
    the_cessna_172p_stalls_with_30_degrees_of_flap_near_its_published_speed
    a_coordinated_level_turn_turns_at_the_rate_its_bank_and_speed_demand
    every_published_figure_has_a_flight_and_every_flight_a_figure
    an_aircraft_restored_in_every_phase_of_flight_tracks_the_original
    an_aircraft_put_back_to_an_earlier_snapshot_flies_on_as_a_fresh_restore_does
    a_snapshot_of_one_model_cannot_be_restored_into_another
    wgs84_reference_points_convert_exactly
    geodetic_positions_round_trip_through_ecef_within_a_millimetre
    the_geodetic_conversion_agrees_with_jsbsims_within_a_millimetre
    every_zlib_fixture_inflates_to_the_payload_it_was_made_from
    stored_blocks_of_the_largest_size_inflate
    a_stream_that_fails_its_checksum_is_refused
    a_stream_cut_short_at_any_length_is_refused
    a_stream_that_would_decompress_past_its_limit_is_refused
    malformed_zlib_headers_and_deflate_blocks_are_refused
    corrupted_streams_are_refused_or_decoded_without_reading_or_writing_out_of_bounds
    adler32_gives_the_published_checksums
    a_geotiff_in_every_layout_the_reader_takes_reads_back_every_sample
    a_geotiff_the_reader_does_not_take_is_refused_saying_why
    the_copernicus_dem_tile_south_of_sydney_decodes_to_the_heights_an_independent_decoder_gives
    crc32_gives_the_published_check_value
    a_zip_archive_reads_back_its_stored_and_deflated_entries_and_refuses_bad_ones
    a_geoid_grid_interpolates_bilinearly_across_the_date_line_and_to_both_poles
    the_egm2008_grid_gives_geographiclibs_undulations_within_its_stated_error
    the_dem_coverage_file_marks_every_cell_the_tile_lists_have
    dem_tiles_are_named_and_found_as_the_buckets_name_them
    the_dem_surface_is_continuous_across_tiles_resolutions_and_bands
    the_dem_surface_is_continuous_across_the_antimeridian_and_to_the_poles
    the_dem_meets_the_sea_at_zero_between_the_last_sample_and_the_coast
    the_dem_gives_the_sydney_tiles_heights_at_its_samples_and_between_them
    sha256_and_md5_give_the_published_digests
    sha256_and_md5_pad_every_length_around_a_block_boundary_and_hash_in_pieces
    an_https_get_fetches_a_pinned_file_byte_for_byte
    an_https_get_follows_redirects_to_the_file
    an_http_error_status_is_a_response_and_a_failed_request_is_an_error
    the_dem_meets_surveyed_airfields_and_coastlines_within_its_stated_accuracy
    the_dem_holds_summits_below_their_surveyed_heights_by_its_resolution
    a_dem_tile_is_fetched_once_checked_against_its_etag_and_read_from_the_cache_after
    a_dem_tile_that_arrives_damaged_missing_or_not_at_all_is_refused_and_not_kept
    a_pinned_file_is_kept_only_if_it_arrives_with_its_pinned_hash
    level_terrain_holds_an_aircraft_exactly_as_jsbsims_own_ground_does
    an_aircraft_set_down_on_a_slope_rests_tilted_by_the_slope
    an_aircraft_with_its_brakes_off_rolls_down_a_slope_and_stays_on_level_ground
    an_aircraft_set_down_on_the_dem_rests_on_its_surface_at_sea_level_a_high_airfield_and_a_slope
    every_axis_button_and_hat_of_a_virtual_stick_and_throttle_moves_the_controls
    bound_inputs_set_the_controls_they_name_and_the_aircraft_receives_them
    a_bindings_file_that_cannot_be_read_is_refused_by_line
    json_documents_rfc_8259_allows_are_read_into_their_values
    json_documents_rfc_8259_does_not_allow_are_refused
    every_recorded_metar_is_read_as_aviationweather_decodes_it
    metar_groups_are_read_in_every_form_and_trends_are_not_read_as_observations
    a_recorded_metar_sets_the_surface_wind_temperature_and_pressure_it_reports
    a_recorded_winds_aloft_response_sets_the_wind_at_every_level_it_reports_and_between
    an_aircraft_in_a_steady_crosswind_drifts_with_it_and_turbulence_disturbs_it_only_when_on
    a_new_weather_report_blends_in_over_its_interval_with_no_step_in_the_wind
    an_instant_is_named_by_the_utc_hour_it_falls_in_as_open_meteo_names_hours
    the_weather_now_at_an_airfield_is_fetched_from_aviationweather_and_open_meteo
    a_fetch_is_tried_again_after_a_server_error_or_no_answer_and_not_after_a_refusal
    a_terrain_mesh_puts_its_vertices_on_the_ground_and_meets_its_neighbour_exactly
    a_terrain_mesh_needs_a_rectangle_on_the_earth_and_a_cell)
foreach(_t IN LISTS GLIDESLOPE_UNIT_TESTS)
    add_test(NAME ${_t} COMMAND glideslope_tests ${_t})
    set_tests_properties(${_t} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()

# The files the tests download - DEM tiles, the geoid - fetched once, before the
# tests that read them, and checked against their pinned SHA-256.
add_test(NAME the_files_the_tests_download_arrive_with_their_pinned_hashes
    COMMAND ${CMAKE_COMMAND} -DLIST=${CMAKE_CURRENT_SOURCE_DIR}/data/downloads/files.txt
        -DDIR=${GLIDESLOPE_DOWNLOADS_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch.cmake)
set_tests_properties(the_files_the_tests_download_arrive_with_their_pinned_hashes
    PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_SETUP downloads)
set_tests_properties(
    the_copernicus_dem_tile_south_of_sydney_decodes_to_the_heights_an_independent_decoder_gives
    the_egm2008_grid_gives_geographiclibs_undulations_within_its_stated_error
    the_dem_gives_the_sydney_tiles_heights_at_its_samples_and_between_them
    the_dem_meets_surveyed_airfields_and_coastlines_within_its_stated_accuracy
    the_dem_holds_summits_below_their_surveyed_heights_by_its_resolution
    an_aircraft_set_down_on_the_dem_rests_on_its_surface_at_sea_level_a_high_airfield_and_a_slope
    PROPERTIES FIXTURES_REQUIRED downloads)
# Twelve tiles, fetched by the first of the two the first time.
set_tests_properties(
    the_dem_meets_surveyed_airfields_and_coastlines_within_its_stated_accuracy
    the_dem_holds_summits_below_their_surveyed_heights_by_its_resolution
    an_aircraft_set_down_on_the_dem_rests_on_its_surface_at_sea_level_a_high_airfield_and_a_slope
    PROPERTIES TIMEOUT 1800 RESOURCE_LOCK dem_downloads)

# The committed DEM coverage is what the pinned tile lists make.
add_test(NAME the_committed_dem_coverage_is_what_the_pinned_tile_lists_make
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DLISTS=${GLIDESLOPE_DOWNLOADS_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dem_coverage.cmake)
set_tests_properties(the_committed_dem_coverage_is_what_the_pinned_tile_lists_make
    PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)

add_test(NAME the_committed_cessna_172p_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_cessna_172p_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME every_compiled_unit_test_is_registered_with_ctest
    COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:glideslope_tests>
        "-DREGISTERED=${GLIDESLOPE_UNIT_TESTS}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/registered_tests.cmake)

# ---------------------------------------------------------------------------
# The platform gate, walked in script mode with toolchains described by -D
# flags - a 32-bit toolchain and old compilers are not things any CI machine
# has, so they are described rather than installed.
# ---------------------------------------------------------------------------
add_test(NAME a_32_bit_toolchain_is_refused_on_every_platform
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -DCASES=pointer-size
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_gate.cmake)

# ---------------------------------------------------------------------------
# The client renders on every GPU driver its platform has - headless and in a
# window, the sky, reversed depth and the floating origin - and the frames it
# writes are read back pixel by pixel.
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(GLIDESLOPE_GPU_DRIVERS direct3d12 vulkan)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(GLIDESLOPE_GPU_DRIVERS metal)
else()
    set(GLIDESLOPE_GPU_DRIVERS vulkan)
endif()
add_executable(glideslope_hud_check tools/hud_check.cpp)
target_link_libraries(glideslope_hud_check PRIVATE glideslope_gfx)
glideslope_configure(glideslope_hud_check)
add_executable(glideslope_terrain_check tools/terrain_check.cpp)
# Cesium Native's image decoder reads the imagery's JPEG tiles.
target_link_libraries(glideslope_terrain_check PRIVATE glideslope_gfx CesiumImage)
glideslope_configure(glideslope_terrain_check)

foreach(_driver IN LISTS GLIDESLOPE_GPU_DRIVERS)
    add_test(NAME the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DCHECK=$<TARGET_FILE:glideslope_hud_check>
            -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_hud.cmake)
    set_tests_properties(the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads RESOURCE_LOCK dem_downloads)
    add_test(NAME a_shot_of_mount_taranaki_matches_the_dem_ray_cast_from_the_same_eye_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DCHECK=$<TARGET_FILE:glideslope_terrain_check>
            -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR} -DDATA=${GLIDESLOPE_DATA_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_terrain.cmake)
    set_tests_properties(a_shot_of_mount_taranaki_matches_the_dem_ray_cast_from_the_same_eye_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads RESOURCE_LOCK dem_downloads)
    add_test(NAME a_shot_of_mount_taranaki_drapes_the_open_imagery_where_it_belongs_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DCHECK=$<TARGET_FILE:glideslope_terrain_check>
            -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR} -DDATA=${GLIDESLOPE_DATA_DIR} -DIMAGERY=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_terrain.cmake)
    set_tests_properties(a_shot_of_mount_taranaki_drapes_the_open_imagery_where_it_belongs_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads RESOURCE_LOCK dem_downloads)
    add_test(NAME a_distant_mountain_and_a_nearby_aircraft_draw_with_no_z_fighting_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DDRIVER=${_driver}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_depth.cmake)
    add_test(NAME a_scene_draws_the_same_at_the_earths_centre_and_far_from_it_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DDRIVER=${_driver}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_origin.cmake)
    add_test(NAME the_client_renders_the_sky_headless_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DDRIVER=${_driver}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_is_sky.cmake)
    add_test(NAME the_client_renders_the_sky_in_a_window_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DDRIVER=${_driver} -DWINDOW=ON
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_is_sky.cmake)
    set_tests_properties(the_client_renders_the_sky_in_a_window_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77)
endforeach()

# ---------------------------------------------------------------------------
# The shader compiler keeps SDL_GPU's resource layout: a shader that follows it
# compiles, and one that breaks it fails with the resource named.
# ---------------------------------------------------------------------------
set(_shaderc_cases
    "a_shader_that_keeps_sdl_gpus_layout_compiles|fragment|textured.frag|0|"
    "a_uniform_buffer_outside_its_set_fails_the_shader_build|vertex|uniform_in_resource_set.vert|1|Transform is in set 0. in a vertex shader SDL_GPU wants it in set 1"
    "a_texture_not_bound_from_zero_fails_the_shader_build|fragment|sampler_not_from_zero.frag|1|image is bound at 1. SDL_GPU's layout puts it at 0"
    "a_separate_sampler_fails_the_shader_build|fragment|separate_sampler.frag|1|separate texture"
    "push_constants_fail_the_shader_build|vertex|push_constants.vert|1|push constant block"
    "a_shader_with_a_glsl_error_fails_the_shader_build|fragment|does_not_compile.frag|1|undeclared")
foreach(_case IN LISTS _shaderc_cases)
    string(REPLACE "|" ";" _fields "${_case}")
    list(GET _fields 0 _name)
    list(GET _fields 1 _stage)
    list(GET _fields 2 _file)
    list(GET _fields 3 _exit)
    list(GET _fields 4 _says)
    set(_expect)
    if(NOT _says STREQUAL "")
        set(_expect "-DSTDERR_MATCHES=${_says}")
    endif()
    add_test(NAME ${_name}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope_shaderc>
            "-DARGS=--stage ${_stage} --name case --in ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_file} --out ${CMAKE_CURRENT_BINARY_DIR}/shaderc-${_name}.cpp"
            -DEXIT=${_exit} ${_expect}
            -P ${GLIDESLOPE_EXPECT_RUN})
endforeach()

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
# glideslope_cli's whole surface, today: every command it accepts, none, one it
# does not, and an aircraft with no files.
# ---------------------------------------------------------------------------
add_test(NAME the_cli_reports_the_project_version
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DARGS=--version -DEXIT=0
        "-DSTDOUT_IS=glideslope_cli ${PROJECT_VERSION}"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_cli_refuses_a_height_asked_for_off_the_earth
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> "-DARGS=height 91 0" -DEXIT=2
        "-DSTDERR_MATCHES=the latitude must be a number from -90 to 90"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_cli_refuses_weather_for_what_is_not_a_station
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> "-DARGS=weather Y&SY" -DEXIT=1
        "-DSTDERR_MATCHES=is not a station's four-character ICAO code"
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

add_test(NAME the_cli_prints_the_figures_jsbsim_read_from_the_cessna_172_files
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DMODEL=c172p
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/aircraft_figures.cmake)

add_test(NAME the_cli_refuses_an_aircraft_it_has_no_files_for
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> "-DARGS=aircraft no_such_aircraft"
        -DEXIT=1 "-DSTDERR_MATCHES=could not load aircraft 'no_such_aircraft'"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_cli_flies_one_published_figure_when_it_is_named
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> "-DARGS=figures c172p static_rpm" -DEXIT=0
        "-DSTDOUT_MATCHES=static_rpm +[0-9.]+ +2300.00 .. 2420.00 +RPM +ok.*1 of 1 in range"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_cli_refuses_a_figure_the_aircraft_does_not_have
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> "-DARGS=figures c172p no_such_figure" -DEXIT=2
        "-DSTDERR_MATCHES=c172p has no figure named no_such_figure"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_cli_refuses_figures_for_an_aircraft_with_no_figures_file
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> "-DARGS=figures no_such_aircraft" -DEXIT=1
        "-DSTDERR_MATCHES=no_such_aircraft.xml"
        -P ${GLIDESLOPE_EXPECT_RUN})

# ---------------------------------------------------------------------------
# The selftest: a tripwire that holds still, and moves when the physics does.
# ---------------------------------------------------------------------------
foreach(_case steady physics-change bad-log)
    if(_case STREQUAL "steady")
        set(_name the_selftest_prints_the_same_hash_every_run)
    elseif(_case STREQUAL "physics-change")
        set(_name a_one_line_change_to_the_physics_moves_the_selftest_hash)
    else()
        set(_name a_selftest_log_with_a_command_it_does_not_know_is_refused_at_its_line)
    endif()
    add_test(NAME ${_name}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/selftest-${_case} -DCASE=${_case}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/selftest_hash.cmake)
endforeach()

add_test(NAME the_cross_platform_check_accepts_platforms_that_agree_and_refuses_one_that_does_not
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DROOT=${PROJECT_SOURCE_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cross-platform
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cross_platform_check.cmake)

add_test(NAME the_cli_refuses_a_selftest_for_an_aircraft_with_no_log
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> "-DARGS=selftest no_such_aircraft" -DEXIT=1
        "-DSTDERR_MATCHES=no_such_aircraft.log"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_cli_refuses_an_argument_it_does_not_know
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DARGS=--fly -DEXIT=2
        -DSTDERR_MATCHES=^usage:
        -P ${GLIDESLOPE_EXPECT_RUN})
