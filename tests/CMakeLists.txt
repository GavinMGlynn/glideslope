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
    unit/test_air_motion.cpp
    unit/test_autopilot.cpp
    unit/test_catalogue.cpp
    unit/test_checklist.cpp
    unit/test_checklist_run.cpp
    unit/test_lander.cpp
    unit/test_departure.cpp
    unit/test_fuzz.cpp
    unit/test_inputs.cpp
    unit/test_inside.cpp
    unit/test_handshake.cpp
    unit/test_keys.cpp
    unit/test_sealing.cpp
    unit/test_session_over_a_socket.cpp
    unit/test_interpolation.cpp
    unit/test_prediction.cpp
    unit/test_messages.cpp
    unit/test_protocol.cpp
    unit/test_slots.cpp
    unit/test_state.cpp
    unit/test_server_txt.cpp
    unit/test_socket.cpp
    unit/test_store.cpp
    unit/test_reliable.cpp
    unit/test_lesson.cpp
    unit/test_levers.cpp
    unit/test_wheels_up.cpp
    unit/test_crash.cpp
    unit/test_controller.cpp
    unit/test_navigator.cpp
    unit/test_dem.cpp
    unit/test_dem_surveyed.cpp
    unit/test_digest.cpp
    unit/test_download.cpp
    unit/test_http.cpp
    unit/test_input.cpp
    unit/test_json.cpp
    unit/test_lift.cpp
    unit/test_terrain_mesh.cpp
    unit/test_weather.cpp
    unit/test_figures.cpp
    unit/test_handling.cpp
    unit/test_fixed_step.cpp
    unit/test_geodesy.cpp
    unit/test_ground.cpp
    unit/test_geoid.cpp
    unit/test_geotiff.cpp
    unit/test_inflate.cpp
    unit/test_flight.cpp
    unit/test_restore.cpp
    unit/test_sky.cpp
    unit/test_model.cpp
    unit/test_imagery.cpp
    unit/test_process_end.cpp)
target_link_libraries(glideslope_tests PRIVATE glideslope_sim glideslope_net glideslope_world glideslope_platform
                                               glideslope_input glideslope_model CesiumImage)
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
    GLIDESLOPE_TEST_PLANS_DIR="${GLIDESLOPE_DATA_DIR}/plans"
    GLIDESLOPE_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    GLIDESLOPE_TEST_PROJECT_DIR="${PROJECT_SOURCE_DIR}")
glideslope_configure(glideslope_tests)

set(GLIDESLOPE_UNIT_TESTS
    every_aircraft_put_down_with_its_wheels_up_rests_on_its_airframe
    every_landplane_dropped_harder_than_its_gear_takes_is_wrecked_and_set_down_gently_is_not
    an_aircraft_that_comes_down_on_its_airframe_is_wrecked
    a_landplane_on_water_is_wrecked_and_a_flying_boat_afloat_is_not
    two_aircraft_within_their_wingspans_have_collided_and_none_can_pass_between_steps
    the_keyboard_moves_every_lever_it_names_and_leaves_it_where_it_is_put
    a_binding_moves_every_lever_the_controls_have
    the_short_s23_in_coarse_pitch_at_normal_boost_turns_within_its_rated_rpm
    the_mosquitos_rpm_follows_its_propeller_lever
    with_nothing_lost_every_reliable_message_arrives_once_and_in_order
    under_every_pattern_of_loss_every_message_arrives_exactly_once_and_in_order
    a_reliable_message_that_arrives_twice_is_handed_up_once
    a_reliable_message_that_arrives_early_waits_for_the_ones_before_it
    an_endpoint_with_nothing_to_say_still_acknowledges_what_it_has
    a_reliable_endpoint_ignores_a_datagram_that_is_not_one_of_its_own
    a_reliable_sender_refuses_once_too_many_are_waiting
    an_acknowledgement_of_a_message_that_was_never_sent_is_not_believed
    an_address_written_down_and_read_back_is_the_same_address
    anything_that_is_not_an_address_is_refused
    a_datagram_sent_to_the_loopback_arrives_whole_and_says_where_from
    a_datagram_too_large_for_the_smallest_path_is_refused
    a_socket_gets_the_port_it_asks_for_and_no_two_share_one
    every_value_written_to_the_wire_reads_back_as_itself
    the_transport_document_and_the_code_agree_byte_for_byte
    every_message_writes_and_reads_back_what_went_into_it
    no_message_reads_as_a_kind_it_is_not
    every_truncation_of_every_message_is_refused
    every_message_with_anything_trailing_is_refused
    every_single_byte_change_to_every_message_is_read_or_refused
    every_floating_point_field_of_every_message_refuses_a_nan_and_an_infinity
    the_message_kinds_and_controllers_are_the_ones_the_document_names
    every_reliable_message_arrives_exactly_once_and_in_order_under_loss
    the_transport_document_and_the_code_agree_about_the_messages
    every_message_filled_to_its_limits_fits_in_one_datagram
    every_message_the_server_accepts_is_named_in_the_threats_document
    every_control_goes_on_the_wire_and_comes_back_within_one_step
    with_nothing_lost_every_input_frame_arrives_once_and_in_order
    an_input_frame_arrives_exactly_when_a_packet_carrying_it_arrives
    losing_a_whole_packets_worth_of_datagrams_in_a_row_does_lose_an_input
    an_input_packet_that_arrives_twice_hands_up_nothing_the_second_time
    an_input_receiver_refuses_anything_that_is_not_an_input_packet
    every_control_the_flight_model_has_is_one_the_wire_carries
    the_transport_document_and_the_code_agree_about_the_inputs
    the_seed_corpus_goes_through_every_network_parser_under_sanitizers
    the_seed_corpus_is_written_where_a_fuzzer_can_take_it
    with_every_snapshot_arriving_an_aircraft_is_shown_where_it_was
    the_interpolation_error_stays_within_its_bound_under_every_pattern_of_loss
    the_interpolation_error_stays_within_its_bound_under_jitter
    an_aircraft_nobody_has_heard_from_is_not_carried_on_for_ever
    an_aircraft_turning_through_north_takes_the_short_way_round
    prediction_and_correction_stay_within_their_bounds_at_100_and_200_ms
    a_client_that_flew_different_inputs_is_put_back_where_the_server_says
    a_lesson_written_down_and_read_back_is_the_one_that_was_written
    a_lesson_file_that_is_wrong_is_refused_and_says_where
    the_take_off_lesson_flown_by_the_book_leaves_an_empty_debrief
    a_take_off_flown_with_one_fault_has_that_fault_in_its_debrief
    a_lesson_may_name_the_speeds_the_aeroplane_publishes
    rotating_early_is_caught_for_each_aeroplane_at_its_own_speed
    the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief
    an_approach_flown_fast_is_named_in_the_debrief
    the_turns_lesson_flown_by_the_book_leaves_an_empty_debrief
    a_turn_that_loses_height_is_named_in_the_debrief
    the_climb_lesson_flown_by_the_book_leaves_an_empty_debrief
    a_climb_at_the_wrong_speed_is_named_in_the_debrief
    the_stalls_lesson_flown_by_the_book_leaves_an_empty_debrief
    a_stall_recovered_badly_is_named_in_the_debrief
    every_aeroplane_flies_the_turns_lesson_of_its_own_class
    an_instructor_hands_over_and_takes_back_with_no_step_in_any_control
    an_instructor_demonstrates_a_take_off_and_hands_it_over
    an_instructor_demonstrates_an_approach_and_hands_it_over
    an_instructor_demonstrates_a_climb_and_descent_and_hands_it_over
    an_instructor_demonstrates_a_stall_and_hands_it_over
    an_instructor_demonstrates_a_circuit_and_hands_it_over
    the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief
    a_circuit_flown_low_downwind_is_named_in_the_debrief
    every_aircraft_says_which_class_it_is_and_the_requirements_agree
    a_local_velocity_becomes_the_same_velocity_in_the_earths_frame
    a_state_packet_written_and_read_back_is_the_same_packet
    a_state_packet_filled_to_its_limits_fits_in_one_datagram
    every_truncation_of_a_state_packet_is_refused
    every_single_byte_change_to_a_state_packet_is_read_or_refused
    a_state_packet_refuses_a_nan_and_an_infinity_in_every_field
    a_state_packet_carries_a_wreck_and_refuses_a_condition_it_does_not_know
    a_state_packet_with_a_controller_that_is_not_one_is_refused
    the_transport_document_and_the_code_agree_about_the_state_packet
    the_kinds_inside_a_sealed_body_are_the_five_the_document_names
    a_knock_written_and_read_back_carries_the_same_token
    a_knock_that_is_not_one_is_not_read
    the_transport_document_and_the_code_agree_about_a_sealed_body
    a_server_txt_names_a_host_a_port_and_a_key
    anything_that_is_not_a_server_line_is_refused
    a_store_gives_back_what_was_put_in_it
    what_a_store_keeps_outlives_the_store_that_kept_it
    a_store_keeps_awkward_values_unchanged
    a_store_that_cannot_be_opened_says_so_rather_than_carrying_on
    a_secret_kept_in_a_store_reads_back_as_the_same_key
    a_key_written_down_and_read_back_is_the_same_key
    a_secrets_public_half_is_the_one_rfc_7748_gives_for_it
    every_minted_key_is_its_own_and_its_halves_go_together
    two_honest_ends_complete_the_handshake_and_agree_on_their_keys
    the_handshake_is_noise_ik_byte_for_byte_as_the_cacophony_vector_has_it
    two_handshakes_with_the_same_keys_give_different_sessions
    a_client_with_the_wrong_server_key_gets_nowhere
    every_change_to_an_initiation_is_refused
    every_change_to_a_handshake_answer_is_refused
    a_replayed_initiation_makes_a_session_the_replayer_cannot_read
    what_is_sealed_opens_as_itself
    a_sealed_body_does_not_open_under_another_key
    a_sealed_body_that_arrives_twice_opens_once
    every_order_of_arrival_opens_every_sealed_body_exactly_once
    a_sealed_body_older_than_the_window_is_refused_and_the_limit_is_stated
    every_change_to_a_sealed_body_is_refused
    a_client_and_a_server_complete_a_session_over_a_socket
    a_stranger_on_the_port_cannot_complete_a_session
    slots_are_the_same_whatever_order_the_players_connect_in
    a_players_slot_is_their_keys_rank_among_those_in_the_session
    a_session_is_full_at_its_player_count_and_the_next_is_refused
    admitting_the_same_key_twice_is_one_player_and_releasing_frees_it
    a_session_is_never_fewer_than_one_player_or_more_than_four
    the_lobby_has_a_row_for_every_slot_with_the_open_ones_named
    the_wire_puts_the_least_significant_byte_first
    the_envelope_is_the_magic_then_the_version_then_the_type
    an_envelope_that_is_wrong_is_refused_with_the_reason_it_is_wrong
    every_truncation_of_a_datagram_is_refused_without_running_off_the_end
    no_single_byte_changed_anywhere_in_a_datagram_can_break_the_reader
    every_light_aircraft_takes_itself_off_and_climbs_away
    the_take_off_speeds_come_from_each_aircrafts_published_figures
    every_light_aircraft_is_flown_down_a_glidepath_and_lands_in_calm_air
    every_light_aircraft_lands_on_the_centreline_in_a_ten_knot_crosswind
    the_approach_speed_is_a_third_above_the_published_landing_stall
    every_checklist_item_the_cessna_can_see_ticks_when_its_state_first_shows_it_done
    an_item_once_ticked_stays_ticked_though_the_aeroplane_moves_on
    flown_with_the_flaps_left_up_the_landing_flaps_item_stays_unticked_and_is_flagged
    the_pilot_ticks_their_own_items_and_only_their_own
    every_aircraft_has_a_checklist_for_every_phase_of_flight
    every_checklist_band_is_one_its_aeroplanes_controls_can_reach
    no_checklist_item_rests_on_a_control_its_aircrafts_model_has_not_got
    every_checklist_item_names_a_state_its_own_aircraft_has_or_is_the_pilots
    an_item_is_done_when_the_aircrafts_state_is_inside_its_band
    a_checklist_file_that_is_wrong_is_refused_and_says_where
    the_nine_phases_of_flight_each_have_one_name_and_answer_to_it
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
    the_cessna_182s_at_full_throttle_on_the_ground_turns_within_its_static_rpm_range
    the_cessna_182s_takes_off_in_about_its_published_ground_roll
    a_cessna_182s_at_full_power_climbs_near_its_published_rate
    the_cessna_182s_cruises_at_80_percent_power_near_its_published_speed
    the_cessna_182s_reaches_about_its_published_maximum_speed
    the_cessna_182s_glides_near_its_published_ratio
    the_cessna_182s_stalls_flaps_up_near_its_published_speed
    the_cessna_182s_stalls_with_20_degrees_of_flap_near_its_published_speed
    the_cessna_182s_stalls_with_full_flap_near_its_published_speed
    the_piper_pa28_at_full_throttle_on_the_ground_turns_within_its_static_rpm_range
    the_piper_pa28_takes_off_in_about_its_published_ground_roll
    a_piper_pa28_at_full_throttle_climbs_near_its_published_rate
    the_piper_pa28_cruises_at_75_percent_power_near_its_published_speed
    the_piper_pa28_reaches_about_its_published_top_speed
    the_piper_pa28_stalls_flaps_up_near_its_published_speed
    the_piper_pa28_stalls_with_40_degrees_of_flap_near_its_published_speed
    the_piper_j3_cub_at_full_throttle_on_the_ground_turns_within_its_static_rpm_range
    a_piper_j3_cub_at_full_load_climbs_near_its_published_rate
    the_piper_j3_cub_cruises_at_2150_rpm_near_its_published_speed
    the_piper_j3_cub_glides_near_its_published_ratio
    the_piper_j3_cub_stalls_near_its_published_speed
    a_figure_asking_for_flaps_the_aircraft_does_not_have_is_refused
    the_boeing_737_300_needs_about_its_published_takeoff_runway_length_at_maximum_weight
    the_boeing_737_300_climbs_with_an_engine_out_as_far_25_demands
    the_boeing_737_300_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows
    the_boeing_737_300_still_climbs_at_its_certificated_ceiling
    the_airbus_a320_needs_about_its_published_takeoff_runway_length_at_maximum_weight
    the_airbus_a320_climbs_with_an_engine_out_as_far_25_demands
    the_airbus_a320_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows
    the_airbus_a320_still_climbs_at_its_certificated_ceiling
    the_boeing_747_400_needs_about_its_published_takeoff_runway_length_at_maximum_weight
    the_boeing_747_400_climbs_with_an_engine_out_as_far_25_demands
    the_boeing_747_400_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows
    the_boeing_747_400_still_climbs_at_its_certificated_ceiling
    the_boeing_787_8_needs_about_its_published_takeoff_runway_length_at_maximum_weight
    the_boeing_787_8_climbs_with_an_engine_out_as_far_25_demands
    the_boeing_787_8_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows
    the_boeing_787_8_still_climbs_at_its_certificated_ceiling
    the_airbus_a380_needs_about_its_published_takeoff_field_length_at_maximum_weight
    the_airbus_a380_climbs_with_an_engine_out_as_jar_25_demands
    the_airbus_a380_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows
    the_airbus_a380_still_climbs_at_its_certificated_ceiling
    the_learjet_35a_needs_about_its_flight_manuals_takeoff_field_length_at_maximum_weight
    the_learjet_35a_climbs_with_an_engine_out_as_its_flight_manual_demands
    the_learjet_35a_reaches_the_c21as_speed_at_41000_ft
    the_learjet_35a_still_climbs_at_its_certificated_ceiling
    the_learjet_35a_stalls_near_its_flight_manuals_speed_with_8_degrees_of_flap
    the_learjet_35a_stalls_near_its_flight_manuals_speed_flaps_up
    the_learjet_35a_stalls_near_its_flight_manuals_speed_with_40_degrees_of_flap
    the_short_s23_takes_off_from_water_at_45000_lb_in_about_gouges_time
    the_short_s23_takes_off_from_water_at_45000_lb_in_about_gouges_run
    the_short_s23_takes_off_from_water_at_its_standard_weight_in_its_published_time
    the_short_s23_floats_at_the_draught_its_general_arrangement_draws
    the_short_s23_reaches_its_published_speed_at_5500_ft
    the_short_s23_climbs_at_its_published_rate_at_sea_level
    the_f35b_reaches_mach_1_6_at_its_best_altitude
    the_f35b_flies_more_than_its_published_range_on_internal_fuel
    the_b2_flies_at_high_subsonic_speed
    the_b2_still_climbs_at_its_published_ceiling
    the_b2_flies_about_its_published_range_unrefuelled
    the_f15c_reaches_its_published_maximum_mach_at_45000_ft
    the_f15c_climbs_at_its_published_rate_at_sea_level_at_military_power
    the_f15c_climbs_at_its_published_rate_at_sea_level_at_maximum_power
    the_f15c_reaches_its_published_service_ceiling_at_military_power
    the_f15c_reaches_its_published_combat_ceiling_at_maximum_power
    the_f15c_sustains_its_published_turn_at_mach_0_9_and_30000_ft
    the_f22_supercruises_at_its_published_mach
    the_f22_accelerates_from_mach_0_8_to_1_5_in_its_published_time
    the_f22_sustains_its_published_turn_at_mach_0_9_and_30000_ft
    the_f22_flies_at_mach_2_at_40000_ft
    the_f22_still_climbs_at_50000_ft
    the_mosquito_fb6_flies_level_at_sea_level_at_hx809s_speed
    the_mosquito_fb6_flies_level_at_its_ms_gear_full_throttle_height_at_hx809s_speed
    the_mosquito_fb6_flies_level_at_its_fs_gear_full_throttle_height_at_hx809s_speed
    the_mosquito_fb6_flies_level_at_18000_ft_at_hx809s_speed
    the_mosquito_fb6_climbs_in_ms_gear_near_hj679s_rate
    the_mosquito_fb6_climbs_in_fs_gear_near_hj679s_rate
    the_mosquito_fb6_climbs_to_20000_ft_in_about_hj679s_time
    the_mosquito_fb6_stalls_clean_near_its_pilots_notes_speed
    the_mosquito_fb6_stalls_wheels_and_flaps_down_near_its_pilots_notes_speed
    the_mosquito_fb6_takes_off_over_50_ft_in_about_the_b_ivs_distance
    the_mosquitos_slight_swing_to_port_is_checked_by_the_port_throttle_slightly_ahead
    the_mosquito_fb6_is_held_straight_on_one_engine_down_to_its_safety_speed_at_18_boost
    the_mosquito_fb6_is_held_straight_on_one_engine_down_to_its_safety_speed_at_9_boost
    the_mosquito_fb6_holds_its_height_on_one_engine_up_to_its_pilots_notes_ceiling
    the_mosquito_drops_its_nose_at_the_stall_as_its_pilots_notes_say
    fixed_gear_stays_down_when_the_lever_is_raised
    a_failed_jet_engine_stays_failed
    frozen_fuel_is_not_burned
    the_speedbrake_lever_moves_the_spoilers
    the_boeing_737_300_stalls_where_its_own_model_said_it_would
    the_boeing_787_8_stalls_where_its_own_model_said_it_would
    the_airbus_a320_stalls_where_its_own_model_said_it_would
    the_airbus_a380_stalls_where_its_own_model_said_it_would
    the_b2_stalls_where_its_own_model_said_it_would
    the_f15c_stalls_where_its_own_model_said_it_would
    the_f35b_stalls_where_its_own_model_said_it_would
    the_boeing_737_300_stalls_at_its_take_off_flap_where_its_own_model_said_it_would
    the_boeing_787_8_stalls_at_its_take_off_flap_where_its_own_model_said_it_would
    the_airbus_a320_stalls_at_its_take_off_flap_where_its_own_model_said_it_would
    the_airbus_a380_stalls_at_its_take_off_flap_where_its_own_model_said_it_would
    the_short_s23_stalls_where_its_own_model_said_it_would
    the_boeing_737_300_climbs_at_the_rate_its_own_model_gave
    the_boeing_787_8_climbs_at_the_rate_its_own_model_gave
    the_airbus_a320_climbs_at_the_rate_its_own_model_gave
    the_airbus_a380_climbs_at_the_rate_its_own_model_gave
    the_b2_climbs_at_the_rate_its_own_model_gave
    the_f15c_climbs_at_the_rate_its_own_model_gave
    the_f35b_climbs_at_the_rate_its_own_model_gave
    the_learjet_35a_climbs_at_the_rate_its_own_model_gave
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
    the_copernicus_water_body_mask_south_of_sydney_decodes_to_the_values_an_independent_decoder_gives
    crc32_gives_the_published_check_value
    a_zip_archive_reads_back_its_stored_and_deflated_entries_and_refuses_bad_ones
    a_geoid_grid_interpolates_bilinearly_across_the_date_line_and_to_both_poles
    the_egm2008_grid_gives_geographiclibs_undulations_within_its_stated_error
    the_dem_coverage_file_marks_every_cell_the_tile_lists_have
    dem_tiles_are_named_and_found_as_the_buckets_name_them
    the_dem_surface_is_continuous_across_tiles_resolutions_and_bands
    the_dem_surface_is_continuous_across_the_antimeridian_and_to_the_poles
    the_dem_meets_the_sea_at_zero_between_the_last_sample_and_the_coast
    the_dem_says_each_place_is_the_water_of_its_nearest_mask_sample
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
    several_fetches_of_one_file_at_once_all_end_with_it
    every_visual_model_that_ships_is_named_in_assets_md_with_its_source_revision_and_licence
    every_aircraft_the_data_holds_has_a_visual_model_or_a_named_reason
    each_visual_model_is_its_aircrafts_size_and_faces_the_way_it_flies
    every_visual_model_is_aligned_to_the_aeroplane_it_draws
    each_visual_models_wheels_sit_on_the_ground_the_aeroplane_stands_on
    every_visual_model_stands_on_the_ground_rather_than_in_it
    each_visual_model_is_where_its_flight_model_says_the_aeroplane_is
    the_cockpit_view_puts_the_eye_where_the_flight_model_says_the_pilots_is
    every_view_stands_where_its_name_says_and_looks_at_the_aeroplane
    a_model_file_that_is_damaged_or_of_another_version_is_refused
    a_pinned_file_is_kept_only_if_it_arrives_with_its_pinned_hash
    level_terrain_holds_an_aircraft_exactly_as_jsbsims_own_ground_does
    an_aircraft_set_down_on_a_slope_rests_tilted_by_the_slope
    an_aircraft_with_its_brakes_off_rolls_down_a_slope_and_stays_on_level_ground
    an_aircraft_set_down_on_the_dem_rests_on_its_surface_at_sea_level_a_high_airfield_and_a_slope
    the_ground_under_an_aircraft_is_water_or_land_where_the_dem_water_mask_says
    the_short_s23_takes_off_from_the_sea_and_from_a_lake_within_its_published_run
    every_axis_button_and_hat_of_a_virtual_stick_and_throttle_moves_the_controls
    bound_inputs_set_the_controls_they_name_and_the_aircraft_receives_them
    a_bindings_file_that_cannot_be_read_is_refused_by_line
    json_documents_rfc_8259_allows_are_read_into_their_values
    json_documents_rfc_8259_does_not_allow_are_refused
    every_recorded_metar_is_read_as_aviationweather_decodes_it
    every_recorded_metars_visibility_weather_and_cloud_are_read_as_aviationweather_decodes_them
    metar_groups_are_read_in_every_form_and_trends_are_not_read_as_observations
    a_recorded_metar_sets_the_surface_wind_temperature_and_pressure_it_reports
    a_recorded_winds_aloft_response_sets_the_wind_at_every_level_it_reports_and_between
    an_aircraft_in_a_steady_crosswind_drifts_with_it_and_turbulence_disturbs_it_only_when_on
    a_new_weather_report_blends_in_over_its_interval_with_no_step_in_the_wind
    an_instant_is_named_by_the_utc_hour_it_falls_in_as_open_meteo_names_hours
    the_weather_now_at_an_airfield_is_fetched_from_aviationweather_and_open_meteo
    a_fetch_is_tried_again_after_a_server_error_or_no_answer_and_not_after_a_refusal
    a_terrain_mesh_puts_its_vertices_on_the_ground_and_meets_its_neighbour_exactly
    a_terrain_mesh_needs_a_rectangle_on_the_earth_and_a_cell
    the_air_is_the_same_wherever_and_whenever_it_is_asked_for
    an_aircraft_restored_in_a_gust_flies_on_in_the_same_air
    a_recorded_gusty_metar_gives_winds_between_its_mean_and_gust_speeds
    turbulence_has_drydens_intensity_at_each_height_and_severity
    a_recorded_response_sets_the_wind_near_the_ground_through_its_boundary_layer
    reported_wind_shear_is_read_and_gives_the_approach_the_models_shear
    a_microburst_gives_the_headwind_downdraught_and_tailwind_its_model_does
    allens_updrafts_have_the_size_and_speed_his_paper_gives
    the_convective_layer_is_as_deep_as_a_warmer_parcel_rises
    each_thermal_in_its_prime_is_allens_updraft_and_between_them_the_air_sinks
    a_thermal_grows_lives_and_fades_and_the_next_rises_elsewhere_in_its_cell
    a_ridge_in_stable_air_makes_the_mountain_waves_of_linear_theory
    without_stability_the_wind_over_a_ridge_is_potential_flow
    a_warm_afternoons_report_lifts_the_air_in_thermals_and_over_its_ridges
    an_aircraft_circling_in_a_thermal_climbs_by_as_much_as_the_model_lifts_it
    a_reports_cloud_is_drawn_as_decks_at_its_heights_and_cover
    a_decks_pattern_covers_as_much_of_the_sky_as_its_eighths_say
    a_reports_visibility_and_precipitation_are_drawn_as_it_gives_them
    the_autopilot_captures_a_new_heading_altitude_airspeed_and_climb
    the_autopilot_never_moves_a_control_faster_than_a_pilots_hand
    a_flight_plan_is_read_from_its_file_and_refused_where_it_is_wrong
    the_navigator_flies_a_plan_past_every_waypoint_in_calm_air_and_in_wind
    handing_the_aircraft_between_pilot_and_ai_steps_nothing_in_any_phase
    every_aircraft_the_data_holds_loads_and_holds_its_start_in_the_air
    every_aircraft_the_data_holds_takes_off_from_a_runway
    every_aircraft_the_data_holds_that_alights_on_water_does_not_roll_out_on_it
    an_aircraft_is_added_by_its_file_alone_and_refused_where_it_is_wrong
    no_flight_model_opens_a_network_socket
    a_download_cannot_be_killed_by_a_server_hanging_up
    a_parent_imagery_tile_scaled_into_a_missing_childs_place_is_sound
    a_program_that_has_ended_can_still_allocate_until_its_last_thread_is_gone)
foreach(_t IN LISTS GLIDESLOPE_UNIT_TESTS)
    # Registered below: its result is what the program says after main.
    if(WIN32 AND _t STREQUAL "a_program_that_has_ended_can_still_allocate_until_its_last_thread_is_gone")
        continue()
    endif()
    add_test(NAME ${_t} COMMAND glideslope_tests ${_t})
    set_tests_properties(${_t} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()

# **A program that has ended can still allocate until its last thread is gone**
# (tests/unit/test_process_end.cpp, src/platform/end_process.hpp). What it
# checks happens after main has returned, in the loader's last call into the
# program, so the ctest reads the line that call writes and the exit code both.
# The timeout is for a runtime whose heap lock is gone: that can hang rather
# than crash.
if(WIN32)
    add_test(NAME a_program_that_has_ended_can_still_allocate_until_its_last_thread_is_gone
        COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:glideslope_tests>
            -DARGS=a_program_that_has_ended_can_still_allocate_until_its_last_thread_is_gone
            -DEXIT=0 "-DSTDERR_MATCHES=the runtime still allocates as the process ends"
            -P ${GLIDESLOPE_EXPECT_RUN})
    set_tests_properties(a_program_that_has_ended_can_still_allocate_until_its_last_thread_is_gone
        PROPERTIES TIMEOUT 120)
endif()

# The files the tests download - DEM tiles, the geoid - fetched once, before the
# tests that read them, and checked against their pinned SHA-256.
add_test(NAME the_files_the_tests_download_arrive_with_their_pinned_hashes
    COMMAND ${CMAKE_COMMAND} -DLIST=${CMAKE_CURRENT_SOURCE_DIR}/data/downloads/files.txt
        -DDIR=${GLIDESLOPE_DOWNLOADS_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch.cmake)
set_tests_properties(the_files_the_tests_download_arrive_with_their_pinned_hashes
    PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_SETUP downloads)
set_tests_properties(
    the_copernicus_dem_tile_south_of_sydney_decodes_to_the_heights_an_independent_decoder_gives
    the_copernicus_water_body_mask_south_of_sydney_decodes_to_the_values_an_independent_decoder_gives
    the_ground_under_an_aircraft_is_water_or_land_where_the_dem_water_mask_says
    the_short_s23_takes_off_from_the_sea_and_from_a_lake_within_its_published_run
    the_egm2008_grid_gives_geographiclibs_undulations_within_its_stated_error
    the_dem_gives_the_sydney_tiles_heights_at_its_samples_and_between_them
    the_dem_meets_surveyed_airfields_and_coastlines_within_its_stated_accuracy
    the_dem_holds_summits_below_their_surveyed_heights_by_its_resolution
    an_aircraft_set_down_on_the_dem_rests_on_its_surface_at_sea_level_a_high_airfield_and_a_slope
    PROPERTIES FIXTURES_REQUIRED downloads)
# Twelve tiles, fetched by the first to want them; two tests fetching the same
# tile at once is safe (src/world/download.cpp), so none of these waits on
# another.
set_tests_properties(
    the_dem_meets_surveyed_airfields_and_coastlines_within_its_stated_accuracy
    the_dem_holds_summits_below_their_surveyed_heights_by_its_resolution
    an_aircraft_set_down_on_the_dem_rests_on_its_surface_at_sea_level_a_high_airfield_and_a_slope
    PROPERTIES TIMEOUT 1800)

# The FlightGear files the visual models are made from, fetched once and
# checked against their pinned SHA-256, so the check below converts exactly
# what tools/make_models.py converted.
add_test(NAME the_files_the_visual_models_are_made_from_arrive_with_their_pinned_hashes
    COMMAND ${CMAKE_COMMAND} -DLIST=${PROJECT_SOURCE_DIR}/assets/models/sources.txt
        -DDIR=${GLIDESLOPE_DOWNLOADS_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch.cmake)
set_tests_properties(the_files_the_visual_models_are_made_from_arrive_with_their_pinned_hashes
    PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_SETUP model_sources TIMEOUT 1800)

add_test(NAME the_committed_visual_models_are_what_their_script_writes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_models.py "-DARGS=--cache;${GLIDESLOPE_DOWNLOADS_DIR}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_visual_models_are_what_their_script_writes
    PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED model_sources TIMEOUT 900)

# Where each model sits on the aeroplane it draws, measured from the committed
# meshes and the flight models. It reads only what is in the tree, so it needs
# neither the network nor the fetched sources - but it measures the meshes, so
# it is stale if they change and this says so.
add_test(NAME the_committed_model_alignment_is_what_its_script_measures
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/align_models.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_model_alignment_is_what_its_script_measures
    PROPERTIES SKIP_RETURN_CODE 77)

# The airframe contacts a wheels-up landing comes down on are measured from
# the visual meshes by tools/ground.py. This holds what that measurement
# claims: that each mesh is the aeroplane it says it is, within 1.5% of its
# published length and span; that no airframe contact sits at or below the
# wheels, where it would drag on every ordinary landing; and that the method
# agrees, within 10 in, with FlightGear's own 737-300 - the one flight model
# found anywhere that carries a belly of its own to compare with.
add_test(NAME the_airframe_contacts_measured_from_the_meshes_agree_with_what_is_published
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/ground.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_airframe_contacts_measured_from_the_meshes_agree_with_what_is_published
    PROPERTIES SKIP_RETURN_CODE 77)

# The committed DEM coverage is what the pinned tile lists make.
add_test(NAME the_committed_dem_coverage_is_what_the_pinned_tile_lists_make
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DLISTS=${GLIDESLOPE_DOWNLOADS_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dem_coverage.cmake)
set_tests_properties(the_committed_dem_coverage_is_what_the_pinned_tile_lists_make
    PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)

add_test(NAME the_committed_cessna_172p_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_c172p.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_cessna_172p_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_cessna_182s_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_c182.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_cessna_182s_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_piper_pa28_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_pa28.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_piper_pa28_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_piper_j3_cub_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_j3cub.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_piper_j3_cub_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_boeing_737_300_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_737_300.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_boeing_737_300_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_airbus_a320_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_a320.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_airbus_a320_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_boeing_747_400_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_747_400.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_boeing_747_400_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_boeing_787_8_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_787_8.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_boeing_787_8_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_airbus_a380_is_what_its_script_writes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_a380.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_airbus_a380_is_what_its_script_writes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_short_s23_is_what_its_script_writes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_short_s23.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_short_s23_is_what_its_script_writes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_learjet_35a_is_what_its_script_writes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_learjet35a.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_learjet_35a_is_what_its_script_writes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_f35b_is_what_its_script_writes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_f35b.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_f35b_is_what_its_script_writes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_b2_is_what_its_script_writes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_b2.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_b2_is_what_its_script_writes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_f15c_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_f15c.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_f15c_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_f22_is_what_its_tuning_script_makes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_f22.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_f22_is_what_its_tuning_script_makes
    PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME the_committed_mosquito_propeller_is_what_its_script_computes
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} "-DPYTHON=${Python3_EXECUTABLE}"
        -DSCRIPT=tools/make_mosquito_propeller.py
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/committed_assets.cmake)
set_tests_properties(the_committed_mosquito_propeller_is_what_its_script_computes
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
add_executable(glideslope_checklist_check tools/checklist_check.cpp)
target_link_libraries(glideslope_checklist_check PRIVATE glideslope_gfx)
glideslope_configure(glideslope_checklist_check)

add_executable(glideslope_hud_check tools/hud_check.cpp)
target_link_libraries(glideslope_hud_check PRIVATE glideslope_gfx)
glideslope_configure(glideslope_hud_check)
add_executable(glideslope_sky_check tools/sky_check.cpp)
target_link_libraries(glideslope_sky_check PRIVATE glideslope_gfx)
glideslope_configure(glideslope_sky_check)
add_executable(glideslope_view_check tools/view_check.cpp)
target_link_libraries(glideslope_view_check PRIVATE glideslope_gfx)
glideslope_configure(glideslope_view_check)
add_executable(glideslope_terrain_check tools/terrain_check.cpp)
# Cesium Native's image decoder reads the imagery's JPEG tiles.
target_link_libraries(glideslope_terrain_check PRIVATE glideslope_gfx CesiumImage)
glideslope_configure(glideslope_terrain_check)

foreach(_driver IN LISTS GLIDESLOPE_GPU_DRIVERS)
    add_test(NAME the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DCHECK=$<TARGET_FILE:glideslope_hud_check>
            -DCLI=$<TARGET_FILE:glideslope_cli>
            -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_hud.cmake)
    set_tests_properties(the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)
    add_test(NAME the_checklist_on_screen_is_the_one_the_flight_is_working_through_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DCHECK=$<TARGET_FILE:glideslope_checklist_check>
            -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_checklist.cmake)
    set_tests_properties(the_checklist_on_screen_is_the_one_the_flight_is_working_through_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)
    add_test(NAME the_hud_shows_a_jets_mach_and_flight_level_at_the_tick_it_was_shot_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DCHECK=$<TARGET_FILE:glideslope_hud_check>
            -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_hud_jet.cmake)
    set_tests_properties(the_hud_shows_a_jets_mach_and_flight_level_at_the_tick_it_was_shot_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)
    add_test(NAME the_client_flies_the_aircraft_it_is_given_in_the_air_or_on_the_ground_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DDRIVER=${_driver}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/client_aircraft.cmake)
    set_tests_properties(the_client_flies_the_aircraft_it_is_given_in_the_air_or_on_the_ground_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)
    # **One view a test.** Shooting all seven in one test took ten minutes,
    # the longest in the suite by four times, and no CI shard could be
    # shorter than it. Each view is its own test now, and each outside view
    # holds its flight to the cockpit's, which is their fixture. It was a
    # separate test holding all seven to one another, with the seven as its
    # fixture - and a CI shard that ran it ran all seven: 19 minutes.
    foreach(_view cockpit ahead behind left right above orbit)
        add_test(NAME the_client_draws_the_aeroplane_where_the_${_view}_view_puts_it_on_${_driver}
            COMMAND ${CMAKE_COMMAND}
                -DPROGRAM=$<TARGET_FILE:glideslope> -DCHECK=$<TARGET_FILE:glideslope_view_check>
                -DDRIVER=${_driver} -DVIEW=${_view} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
                -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR} -DMODELS=${GLIDESLOPE_DATA_DIR}/models
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/client_views.cmake)
        if(_view STREQUAL "cockpit")
            set_tests_properties(the_client_draws_the_aeroplane_where_the_${_view}_view_puts_it_on_${_driver}
                PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads
                           FIXTURES_SETUP cockpit_view_${_driver})
        else()
            set_tests_properties(the_client_draws_the_aeroplane_where_the_${_view}_view_puts_it_on_${_driver}
                PROPERTIES SKIP_RETURN_CODE 77
                           FIXTURES_REQUIRED "downloads;cockpit_view_${_driver}")
        endif()
    endforeach()
    # The open provider and Cesium ion, each in both its states: with the
    # user's key and without one. A provider whose key this machine has not
    # got reports itself skipped - skipped, never passed.
    #
    # Google draws its own photographs and its attribution, and is held to
    # both. Its item is not ticked because the other way in to it - a Google
    # Maps Platform key used directly rather than an ion token - has no key
    # on any machine here to prove it with.
    foreach(_provider open ion google)
        add_test(NAME the_${_provider}_terrain_draws_with_its_attribution_or_says_why_not_on_${_driver}
            COMMAND ${CMAKE_COMMAND}
                -DPROGRAM=$<TARGET_FILE:glideslope> -DPROVIDER=${_provider}
                -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
                -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/terrain_provider.cmake)
        set_tests_properties(the_${_provider}_terrain_draws_with_its_attribution_or_says_why_not_on_${_driver}
            PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads TIMEOUT 1800)
    endforeach()
    # How far each provider's drawn terrain is from the ground the aircraft
    # meets, at the twelve surveyed runway ends. The bound each is held to is
    # the one measured and stated in docs/PROJECT_STATUS.md.
    # In millimetres: CMake's arithmetic is whole numbers only.
    foreach(_measured open:250 ion:12000 google:12000)
        string(REPLACE ":" ";" _pair "${_measured}")
        list(GET _pair 0 _provider)
        list(GET _pair 1 _bound_mm)
        add_test(NAME the_${_provider}_terrain_is_within_its_stated_distance_of_the_ground_flown_on_${_driver}
            COMMAND ${CMAKE_COMMAND}
                -DPROGRAM=$<TARGET_FILE:glideslope> -DPROVIDER=${_provider}
                -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
                -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR} -DBOUND_MM=${_bound_mm}
                -DPLACES=${CMAKE_CURRENT_SOURCE_DIR}/data/dem/surveyed.txt
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/terrain_mismatch.cmake)
        set_tests_properties(the_${_provider}_terrain_is_within_its_stated_distance_of_the_ground_flown_on_${_driver}
            PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads TIMEOUT 1800)
    endforeach()
    add_test(NAME the_clients_ai_flies_a_flight_plan_past_every_waypoint_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DDRIVER=${_driver}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/client_autopilot.cmake)
    set_tests_properties(the_clients_ai_flies_a_flight_plan_past_every_waypoint_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)
    add_test(NAME a_shot_of_mount_taranaki_matches_the_dem_ray_cast_from_the_same_eye_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DCHECK=$<TARGET_FILE:glideslope_terrain_check>
            -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR} -DDATA=${GLIDESLOPE_DATA_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_terrain.cmake)
    set_tests_properties(a_shot_of_mount_taranaki_matches_the_dem_ray_cast_from_the_same_eye_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)
    add_test(NAME a_shot_of_mount_taranaki_drapes_the_open_imagery_where_it_belongs_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DCHECK=$<TARGET_FILE:glideslope_terrain_check>
            -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR} -DDATA=${GLIDESLOPE_DATA_DIR} -DIMAGERY=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_terrain.cmake)
    set_tests_properties(a_shot_of_mount_taranaki_drapes_the_open_imagery_where_it_belongs_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)
    add_test(NAME a_reports_cloud_visibility_and_rain_are_drawn_where_it_puts_them_on_${_driver}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope> -DCLI=$<TARGET_FILE:glideslope_cli>
            -DSKY=$<TARGET_FILE:glideslope_sky_check>
            -DTERRAIN=$<TARGET_FILE:glideslope_terrain_check>
            -DDRIVER=${_driver} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/frames
            -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR} -DDATA=${GLIDESLOPE_DATA_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/frame_weather.cmake)
    set_tests_properties(a_reports_cloud_visibility_and_rain_are_drawn_where_it_puts_them_on_${_driver}
        PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED downloads)
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

# **What GCC cannot see.** Both Linux builds are GCC, which does not warn
# about an unused constant at namespace scope in C++ at any level; clang does,
# and reds macOS and Windows clang-cl for it. This compiles every first-party
# source under clang with the same flags the build used, and reports itself
# skipped where clang is not installed.
add_test(NAME every_first_party_source_compiles_clean_under_clang_too
    COMMAND ${CMAKE_COMMAND} -DBUILD=${CMAKE_BINARY_DIR}
        -DSOURCE=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/clang_warnings.cmake)
set_tests_properties(every_first_party_source_compiles_clean_under_clang_too
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 900)

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
# **The server's flags, every one of them.** The item names six - --headless,
# --players, --port, --store, --key and --timeout - and this exercises each,
# alone and together, along with the two that every program here has and the
# --dry-run that lets a server be asked what it would do without listening.
add_test(NAME the_server_reports_the_project_version
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> -DARGS=--version -DEXIT=0
        "-DSTDOUT_IS=glideslope_server ${PROJECT_VERSION}"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_prints_its_usage_when_asked
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> -DARGS=--help -DEXIT=0
        "-DSTDOUT_MATCHES=usage: glideslope_server.*--players N.*--port N.*--store FILE.*--key HEX.*--timeout SECONDS.*--headless.*--dry-run"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_with_no_flags_takes_its_defaults
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> -DARGS=--dry-run -DEXIT=0
        "-DSTDOUT_MATCHES=players +4.*port +47801.*store +\\(none.*key +\\(none.*timeout +10\\.000 s.*dashboard +in the terminal"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_takes_every_flag_the_item_names_at_once
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server>
        "-DARGS=--players 2 --port 47999 --store /tmp/glideslope-session.sqlite --key 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef --timeout 2.5 --headless --dry-run"
        -DEXIT=0
        "-DSTDOUT_MATCHES=players +2.*port +47999.*store +/tmp/glideslope-session\\.sqlite.*key +given, 64 hexadecimal.*timeout +2\\.500 s.*dashboard +no \\(--headless\\)"
        -P ${GLIDESLOPE_EXPECT_RUN})

# **The window is a choice made at start, and a clear one.** It is not with
# --headless or --plain, and its test flags are nothing without it.
add_test(NAME the_server_draws_its_dashboard_in_a_window_when_asked
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--window --dry-run" -DEXIT=0
        "-DSTDOUT_MATCHES=dashboard +in a window \\(--window\\)"
        -P ${GLIDESLOPE_EXPECT_RUN})
add_test(NAME the_server_refuses_a_window_with_no_dashboard
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--window --headless --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--window draws the dashboard in a window and --headless has none"
        -P ${GLIDESLOPE_EXPECT_RUN})
add_test(NAME the_server_refuses_a_window_drawn_as_plain_text
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--window --plain --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--plain is how the terminal's dashboard is drawn"
        -P ${GLIDESLOPE_EXPECT_RUN})
add_test(NAME the_server_refuses_the_windows_test_flags_without_a_window
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--window-dump --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=there is none without --window"
        -P ${GLIDESLOPE_EXPECT_RUN})

# **An aircraft flown by hand points where it is told**, and a heading off the
# compass is refused.
add_test(NAME the_server_refuses_a_heading_off_the_compass
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--fly c172p@-33.9,151.3,360 --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--fly wants a heading from 0 to 360"
        -P ${GLIDESLOPE_EXPECT_RUN})

# **A player count outside 1 to 4 is refused**, which is the item's own
# verification. Both ends of the range and both sides of them are tried.
add_test(NAME the_server_refuses_a_session_of_no_players
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--players 0 --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--players is 0, and a session is 1 to 4 players"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_session_of_five_players
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--players 5 --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--players is 5, and a session is 1 to 4 players"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_accepts_a_session_of_one_player
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--players 1 --dry-run"
        -DEXIT=0 "-DSTDOUT_MATCHES=players +1" -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_accepts_a_session_of_four_players
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--players 4 --dry-run"
        -DEXIT=0 "-DSTDOUT_MATCHES=players +4" -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_key_that_is_not_a_server_secret
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--key abc --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--key is 3 characters, and a server secret is 64"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_key_that_is_not_hexadecimal
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server>
        "-DARGS=--key z123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--key has 'z' in it, which is not a hexadecimal digit"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_timeout_that_is_not_a_length_of_time
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--timeout 0 --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--timeout is 0.*and it must be more than nothing"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_plain_and_headless_together
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--plain --headless --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--plain draws the dashboard and --headless has none"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_says_when_its_dashboard_is_plain_text
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--plain --dry-run"
        -DEXIT=0 "-DSTDOUT_MATCHES=dashboard +in the terminal, plain text \\(--plain\\)"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_port_that_is_not_one
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--port 70000 --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--port wants a number from 0 to 65535"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_flag_with_nothing_after_it
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--players"
        -DEXIT=2 "-DSTDERR_MATCHES=--players wants a value after it"
        -P ${GLIDESLOPE_EXPECT_RUN})

# **The server flies every aircraft, each over its own terrain.** Two aircraft
# **AI aircraft keep flying with nobody connected**, and a client joining later
# finds them mid-flight. See cmake/server_late.cmake for why its own aircraft
# is the control in that measurement.
add_test(NAME ai_aircraft_keep_flying_with_nobody_connected_and_a_late_client_finds_them_mid_flight
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_late -DPORT=47863
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_late.cmake)
set_tests_properties(ai_aircraft_keep_flying_with_nobody_connected_and_a_late_client_finds_them_mid_flight
    PROPERTIES SKIP_RETURN_CODE 77)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_late)

# **What becomes of an aircraft when its player goes**, both settings walked and
# counted. See cmake/server_leave.cmake.
add_test(NAME a_player_leaving_has_their_aircraft_removed_or_flown_on_by_an_ai_as_the_session_says
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_leave -DPORT=47864
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_leave.cmake)
set_tests_properties(a_player_leaving_has_their_aircraft_removed_or_flown_on_by_an_ai_as_the_session_says
    PROPERTIES SKIP_RETURN_CODE 77)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_leave)

# **A client flies an aircraft on the server**, by sending inputs and nothing
# else. Full aileron rolls it inverted, which no AI pilot on a plan would do;
# see cmake/server_fly.cmake for the two ways this has been broken before.
add_test(NAME a_clients_inputs_fly_its_aircraft_on_the_server
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_fly -DPORT=47862
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_fly.cmake)
set_tests_properties(a_clients_inputs_fly_its_aircraft_on_the_server PROPERTIES SKIP_RETURN_CODE 77)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_fly)

# **A client hears where every aircraft is**: the whole state stream, end to
# end over a real socket, with the positions held to the place the flight plan
# starts over. See cmake/server_state.cmake for why the places are checked.
add_test(NAME a_client_hears_where_every_aircraft_is
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_state -DPORT=47860
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_state.cmake)
set_tests_properties(a_client_hears_where_every_aircraft_is PROPERTIES SKIP_RETURN_CODE 77)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_state)

# **A repeated handshake initiation does not take a live session away.** The
# bug was here and is fixed; see cmake/server_duplicate.cmake for what it was.
add_test(NAME a_repeated_handshake_initiation_does_not_take_a_live_session_away
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli>
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_duplicate -DPORT=47859
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_duplicate.cmake)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_duplicate)

# **A gearstick client is refused.** gearstick's first datagram, made from its
# own TRANSPORT.md by tools/make_gearstick_hello.py, sent to a running server;
# the answer must be NOT_THIS_PROTOCOL. See cmake/server_gearstick.cmake.
add_executable(glideslope_datagram_check tools/datagram_check.cpp)
target_link_libraries(glideslope_datagram_check PRIVATE glideslope_platform)
glideslope_configure(glideslope_datagram_check)
add_test(NAME a_gearstick_client_is_refused_as_not_this_protocol
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCHECK=$<TARGET_FILE:glideslope_datagram_check>
        -DHELLO=${CMAKE_CURRENT_SOURCE_DIR}/data/gearstick_hello.hex -DPORT=47856
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_gearstick.cmake)

# **A client written from docs/TRANSPORT.md alone completes a session.** It
# links libsodium and the system's sockets, nothing of this project's; see
# cmake/server_doc_client.cmake, which also holds its includes to that.
add_executable(glideslope_doc_client doc_client/doc_client.cpp)
target_link_libraries(glideslope_doc_client PRIVATE unofficial-sodium::sodium)
if(WIN32)
    target_link_libraries(glideslope_doc_client PRIVATE ws2_32)
endif()
glideslope_configure(glideslope_doc_client)
add_test(NAME a_client_written_from_the_transport_document_alone_completes_a_session
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DDOC_CLIENT=$<TARGET_FILE:glideslope_doc_client>
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/doc_client/doc_client.cpp
        -DDATA=${GLIDESLOPE_DATA_DIR} -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_doc_client -DPORT=47855
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_doc_client.cmake)
set_tests_properties(a_client_written_from_the_transport_document_alone_completes_a_session
    PROPERTIES SKIP_RETURN_CODE 77)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_doc_client)

# **Four players joining in the worst order each fly their own aircraft**,
# under four numbers. See cmake/server_slots.cmake.
add_test(NAME four_players_joining_in_the_reverse_of_their_keys_order_each_fly_their_own_aircraft
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_slots -DPORT=47854
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_slots.cmake)
set_tests_properties(four_players_joining_in_the_reverse_of_their_keys_order_each_fly_their_own_aircraft
    PROPERTIES SKIP_RETURN_CODE 77)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_slots)

# **The server's window**: it shows the dashboard the terminal would, its drop
# button drops a player, and it writes a frame. See cmake/server_window.cmake.
add_test(NAME the_servers_window_shows_the_terminals_dashboard_and_its_drop_button_drops_a_player
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_window -DPORT=47853
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_window.cmake)
set_tests_properties(the_servers_window_shows_the_terminals_dashboard_and_its_drop_button_drops_a_player
    PROPERTIES SKIP_RETURN_CODE 77)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_window)

# **A server in the cloud has no display**, and needs none: on Linux, where it
# is hosted, it names no display library, runs with no display, and refuses
# --window there rather than running without it. See
# cmake/server_no_display.cmake.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME the_server_links_no_display_library
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:glideslope_server> -DSYSTEM=${CMAKE_SYSTEM_NAME}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/binary_dependencies.cmake)
    add_test(NAME the_server_runs_with_no_display_and_refuses_a_window_there
        COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_no_display.cmake)
endif()

# **Two aircraft on a collision course collide on the server, every client is
# told, and both fly again.** See cmake/server_collision.cmake.
add_test(NAME two_aircraft_on_a_collision_course_collide_on_the_server_and_every_client_is_told
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_collision -DPORT=47852
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_collision.cmake)
set_tests_properties(two_aircraft_on_a_collision_course_collide_on_the_server_and_every_client_is_told
    PROPERTIES SKIP_RETURN_CODE 77)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_collision)

# **The pre-commit hook refuses what it says it refuses**, and nothing else.
# See cmake/githooks.cmake.
add_test(NAME the_pre_commit_hook_refuses_an_unfinished_commit_and_nothing_else
    COMMAND ${CMAKE_COMMAND} -DHOOKS=${PROJECT_SOURCE_DIR}/.githooks
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/githooks
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/githooks.cmake)

# **Every flag the server prints in its usage is one it takes.** A flag added
# to the usage and not to the parser is documented and silently ignored; see
# cmake/server_flags.cmake, which walks the usage text itself.
add_test(NAME every_flag_the_server_prints_in_its_usage_is_one_it_takes
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_flags.cmake)

# **The dashboard shows a real round trip and real traffic.** REQUIREMENTS.md
# 6.6 asks for ping and traffic per client; this holds them to numbers that
# came off a socket. See cmake/server_ping.cmake.
add_test(NAME the_dashboard_shows_the_round_trip_and_the_traffic_of_a_client_that_stays
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli>
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_ping -DPORT=47858
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_ping.cmake)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_ping)

# **A server lets go a client it has not heard from**, which is the only way a
# slot comes back when somebody's machine goes away. Two processes at once; see
# cmake/server_timeout.cmake for how, and why the client is the first of them.
add_test(NAME a_server_lets_go_a_client_it_has_not_heard_from_for_its_timeout
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli>
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_timeout -DPORT=47857
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_timeout.cmake)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_timeout)

# **A server started by systemd accepts a client**, which is half the
# deployment item: the unit that ships, with its paths pointed at the build.
# The Dockerfile half needs a Docker daemon and is not tested here.
add_test(NAME a_server_started_by_systemd_accepts_a_client
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DUNIT=${PROJECT_SOURCE_DIR}/deploy/glideslope-server.service
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/deploy -DPORT=47870
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_systemd.cmake)
set_tests_properties(a_server_started_by_systemd_accepts_a_client PROPERTIES SKIP_RETURN_CODE 77)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/deploy)

# **And so does the client with the window**, which is the one a person flies.
# Needs a GPU, so it skips where there is none; see cmake/client_online_window.cmake.
add_test(NAME the_client_with_the_window_joins_the_server_server_txt_names
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope> -DDATA=${GLIDESLOPE_DATA_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/online -DPORT=47874 -DDRIVER=vulkan
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/client_online_window.cmake)
set_tests_properties(the_client_with_the_window_joins_the_server_server_txt_names
    PROPERTIES SKIP_RETURN_CODE 77)

# **A client started with --online reaches the server server.txt names.** See
# cmake/client_online.cmake, which names the file by the environment rather
# than writing into the person's own config directory.
add_test(NAME a_client_started_with_online_reaches_the_server_server_txt_names
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DCLIENT=$<TARGET_FILE:glideslope_cli>
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/online -DPORT=47871
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/client_online.cmake)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/online)

# **A server given a store keeps its key across restarts**, which is what makes
# the public key a client was given out of band still good after a restart. See
# cmake/server_store.cmake for the four ways a server comes by a key.
add_test(NAME a_server_given_a_store_keeps_its_key_across_restarts
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/server_store
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_store.cmake)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/server_store)

# on opposite sides of the world in one session; see cmake/server_flies.cmake
# for why Sydney and Denver, and what makes it more than a smoke test.
add_test(NAME the_server_flies_aircraft_on_opposite_sides_of_the_world_each_over_its_own_terrain
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DDATA=${GLIDESLOPE_DATA_DIR} -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_flies.cmake)
set_tests_properties(the_server_flies_aircraft_on_opposite_sides_of_the_world_each_over_its_own_terrain
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

# **Server-run AI aircraft, how many a server setting.**
add_test(NAME the_server_runs_the_number_of_ai_aircraft_it_is_given_and_four_when_given_none
    COMMAND ${CMAKE_COMMAND} -DSERVER=$<TARGET_FILE:glideslope_server>
        -DDATA=${GLIDESLOPE_DATA_DIR} -DCACHE=${GLIDESLOPE_DOWNLOADS_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/server_ai.cmake)
set_tests_properties(the_server_runs_the_number_of_ai_aircraft_it_is_given_and_four_when_given_none
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 900)

add_test(NAME the_server_refuses_more_ai_aircraft_than_it_will_run
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--ai 17 --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--ai is 17, and a server runs 0 to 16 AI aircraft"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_defaults_to_four_ai_aircraft
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--dry-run"
        -DEXIT=0 "-DSTDOUT_MATCHES=ai +4 aircraft" -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_flight_that_is_not_an_aircraft_and_a_place
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--fly c172p --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--fly wants ID@LAT,LON"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_flight_from_somewhere_that_is_not_on_earth
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--fly c172p@91.0,0.0 --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--fly wants a latitude and longitude on Earth"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_fifth_aircraft_to_fly
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server>
        "-DARGS=--fly c172p@0,0 --fly c172p@1,1 --fly c172p@2,2 --fly c172p@3,3 --fly c172p@4,4 --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--fly was given 5 times, and a session holds at most 4"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_a_length_of_time_that_is_negative
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--seconds -1 --dry-run"
        -DEXIT=2 "-DSTDERR_MATCHES=--seconds is -1.*a length of time is not negative"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_says_what_it_would_fly_and_for_how_long
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server>
        "-DARGS=--fly c172p@-33.9,151.2 --fly f35b@40.7,-74.0 --seconds 5 --dry-run"
        -DEXIT=0
        "-DSTDOUT_MATCHES=seconds +5.*flying +c172p from -33\\.900000, 151\\.200000.*flying +f35b from 40\\.700000, -74\\.000000"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_server_refuses_an_option_it_does_not_know
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_server> "-DARGS=--fly-itself"
        -DEXIT=2 "-DSTDERR_MATCHES=there is no option '--fly-itself'"
        -P ${GLIDESLOPE_EXPECT_RUN})

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

add_test(NAME the_cli_refuses_a_sky_for_what_is_not_a_metar
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> "-DARGS=sky nonsense -39.5 174.3" -DEXIT=1
        "-DSTDERR_MATCHES=a METAR must start with its station"
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

add_test(NAME the_cli_lists_the_aircraft_the_data_holds
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope_cli> -DARGS=aircraft -DEXIT=0
        "-DSTDOUT_MATCHES=737-300 +Boeing 737-300 +model 737-300, starting at 250 KCAS, published figures.*747-400 +Boeing 747-400 +model 747-400, starting at 250 KCAS, published figures.*787-8 +Boeing 787-8 +model 787-8, starting at 250 KCAS, published figures.*a320 +Airbus A320 +model a320, starting at 250 KCAS, published figures.*a380 +Airbus A380-841 +model a380, starting at 250 KCAS, published figures.*b2 +Northrop Grumman B-2A Spirit +model b2, starting at 250 KCAS, published figures.*c172p +Cessna 172P Skyhawk +model c172p, starting at 100 KCAS, published figures, a selftest.*c182 +Cessna 182S Skylane +model c182, starting at 120 KCAS, published figures.*f15c +McDonnell Douglas F-15C Eagle +model f15c, starting at 300 KCAS, published figures.*f22 +Lockheed Martin F-22A Raptor +model f22, starting at 300 KCAS, published figures.*f35b +Lockheed Martin F-35B Lightning II +model f35b, starting at 300 KCAS, published figures.*j3cub +Piper J-3 Cub +model j3cub, starting at 60 KCAS, published figures.*learjet35a +Gates Learjet 35A +model learjet35a, starting at 250 KCAS, published figures.*mosquito-fb6 +de Havilland Mosquito FB Mk VI +model mosquito-fb6, starting at 220 KCAS, published figures.*pa28 +Piper PA-28-180 Cherokee +model pa28, starting at 110 KCAS, published figures.*short_s23 +Short S.23 Empire Flying Boat +model short_s23, starting at 130 KCAS, a seaplane, published figures"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_client_refuses_an_aircraft_the_data_does_not_hold
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope> "-DARGS=--aircraft no-such-aircraft" -DEXIT=2
        "-DSTDERR_MATCHES=the data holds no aircraft no-such-aircraft. it holds 737-300, .*c172p"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_client_refuses_a_terrain_provider_there_is_none_of
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope> "-DARGS=--terrain nonesuch" -DEXIT=2
        "-DSTDERR_MATCHES=there is no terrain nonesuch. there is open, ion, google"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_client_refuses_a_view_there_is_none_of
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope> "-DARGS=--view nonesuch" -DEXIT=2
        "-DSTDERR_MATCHES=there is no view nonesuch. there is cockpit, ahead, behind, left, right, above, orbit"
        -P ${GLIDESLOPE_EXPECT_RUN})

add_test(NAME the_client_will_not_have_the_ai_take_off
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:glideslope> "-DARGS=--on-ground --autopilot" -DEXIT=2
        "-DSTDERR_MATCHES=the AI cannot take off"
        -P ${GLIDESLOPE_EXPECT_RUN})

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
