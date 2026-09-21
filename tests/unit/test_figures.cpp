#include "harness.hpp"

#include "sim/figures.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using glideslope::sim::fly_figure;
using glideslope::sim::known_figures;
using glideslope::sim::PublishedFigures;
using glideslope::sim::read_published_figures;
using glideslope::test::check;
using glideslope::test::fail;

namespace {

const char* const data_dir = GLIDESLOPE_TEST_DATA_DIR;

// Every aircraft with published figures.
const std::vector<std::string>& figured_models() {
    static const std::vector<std::string> models = {"737-300", "747-400", "787-8", "a320", "a380", "b2", "c172p", "c182", "f15c", "f22", "f35b", "j3cub", "learjet35a", "mosquito-fb6", "pa28", "short_s23"};
    return models;
}

std::string figures_file(const std::string& model) {
    return std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/" + model + ".xml";
}

// Flies one figure from an aircraft's file and fails with the measurement, the
// range and the source if it lands outside the range.
void expect_figure(const std::string& model, const std::string& name) {
    const PublishedFigures figures = read_published_figures(figures_file(model));
    const auto it = std::find_if(figures.figures.begin(), figures.figures.end(),
                                 [&](const auto& f) { return f.name == name; });
    if (it == figures.figures.end()) {
        fail("assets/figures/" + model + ".xml has no figure named " + name);
    }
    const auto result = fly_figure(data_dir, figures, *it);
    std::printf("%s: %.3f %s (range %.3f to %.3f)\n", name.c_str(), result.measured,
                it->unit.c_str(), it->low, it->high);
    if (!result.passed()) {
        fail(name + " measured " + std::to_string(result.measured) + " " + it->unit +
             ", outside " + std::to_string(it->low) + " to " +
             std::to_string(it->high) + ". Source: " + it->source);
    }
}

} // namespace

GLIDESLOPE_TEST(
    the_cessna_172p_at_full_throttle_on_the_ground_turns_within_its_static_rpm_range) {
    expect_figure("c172p", "static_rpm");
}

GLIDESLOPE_TEST(the_cessna_172p_takes_off_in_about_its_published_ground_roll) {
    expect_figure("c172p", "takeoff_ground_roll");
}

GLIDESLOPE_TEST(a_cessna_172p_at_full_power_climbs_near_its_published_rate) {
    expect_figure("c172p", "climb_rate");
}

GLIDESLOPE_TEST(the_cessna_172p_cruises_near_its_published_speed) {
    expect_figure("c172p", "cruise_speed");
}

GLIDESLOPE_TEST(the_cessna_172p_glides_near_its_published_ratio) {
    expect_figure("c172p", "glide_ratio");
}

GLIDESLOPE_TEST(the_cessna_172p_stalls_flaps_up_near_its_published_speed) {
    expect_figure("c172p", "stall_speed_flaps_up");
}

GLIDESLOPE_TEST(
    the_cessna_172p_stalls_with_10_degrees_of_flap_near_its_published_speed) {
    expect_figure("c172p", "stall_speed_flaps_10");
}

GLIDESLOPE_TEST(
    the_cessna_172p_stalls_with_30_degrees_of_flap_near_its_published_speed) {
    expect_figure("c172p", "stall_speed_flaps_30");
}

GLIDESLOPE_TEST(a_coordinated_level_turn_turns_at_the_rate_its_bank_and_speed_demand) {
    expect_figure("c172p", "turn_rate");
}

GLIDESLOPE_TEST(the_cessna_182s_at_full_throttle_on_the_ground_turns_within_its_static_rpm_range) {
    expect_figure("c182", "static_rpm");
}

GLIDESLOPE_TEST(the_cessna_182s_takes_off_in_about_its_published_ground_roll) {
    expect_figure("c182", "takeoff_ground_roll");
}

GLIDESLOPE_TEST(a_cessna_182s_at_full_power_climbs_near_its_published_rate) {
    expect_figure("c182", "climb_rate");
}

GLIDESLOPE_TEST(the_cessna_182s_cruises_at_80_percent_power_near_its_published_speed) {
    expect_figure("c182", "cruise_speed_6000_ft");
}

GLIDESLOPE_TEST(the_cessna_182s_reaches_about_its_published_maximum_speed) {
    expect_figure("c182", "maximum_speed");
}

GLIDESLOPE_TEST(the_cessna_182s_glides_near_its_published_ratio) {
    expect_figure("c182", "glide_ratio");
}

GLIDESLOPE_TEST(the_cessna_182s_stalls_flaps_up_near_its_published_speed) {
    expect_figure("c182", "stall_speed_flaps_up");
}

GLIDESLOPE_TEST(the_cessna_182s_stalls_with_20_degrees_of_flap_near_its_published_speed) {
    expect_figure("c182", "stall_speed_flaps_20");
}

GLIDESLOPE_TEST(the_cessna_182s_stalls_with_full_flap_near_its_published_speed) {
    expect_figure("c182", "stall_speed_flaps_full");
}

GLIDESLOPE_TEST(the_piper_pa28_at_full_throttle_on_the_ground_turns_within_its_static_rpm_range) {
    expect_figure("pa28", "static_rpm");
}

GLIDESLOPE_TEST(the_piper_pa28_takes_off_in_about_its_published_ground_roll) {
    expect_figure("pa28", "takeoff_ground_roll");
}

GLIDESLOPE_TEST(a_piper_pa28_at_full_throttle_climbs_near_its_published_rate) {
    expect_figure("pa28", "climb_rate");
}

GLIDESLOPE_TEST(the_piper_pa28_cruises_at_75_percent_power_near_its_published_speed) {
    expect_figure("pa28", "cruise_speed");
}

GLIDESLOPE_TEST(the_piper_pa28_reaches_about_its_published_top_speed) {
    expect_figure("pa28", "maximum_speed");
}

GLIDESLOPE_TEST(the_piper_pa28_stalls_flaps_up_near_its_published_speed) {
    expect_figure("pa28", "stall_speed_flaps_up");
}

GLIDESLOPE_TEST(the_piper_pa28_stalls_with_40_degrees_of_flap_near_its_published_speed) {
    expect_figure("pa28", "stall_speed_flaps_40");
}

GLIDESLOPE_TEST(the_piper_j3_cub_at_full_throttle_on_the_ground_turns_within_its_static_rpm_range) {
    expect_figure("j3cub", "static_rpm");
}

GLIDESLOPE_TEST(a_piper_j3_cub_at_full_load_climbs_near_its_published_rate) {
    expect_figure("j3cub", "climb_rate");
}

GLIDESLOPE_TEST(the_piper_j3_cub_cruises_at_2150_rpm_near_its_published_speed) {
    expect_figure("j3cub", "cruise_speed");
}

GLIDESLOPE_TEST(the_piper_j3_cub_glides_near_its_published_ratio) {
    expect_figure("j3cub", "glide_ratio");
}

GLIDESLOPE_TEST(the_piper_j3_cub_stalls_near_its_published_speed) {
    expect_figure("j3cub", "stall_speed");
}

GLIDESLOPE_TEST(the_boeing_737_300_needs_about_its_published_takeoff_runway_length_at_maximum_weight) {
    expect_figure("737-300", "takeoff_field_length");
}

GLIDESLOPE_TEST(the_boeing_737_300_climbs_with_an_engine_out_as_far_25_demands) {
    expect_figure("737-300", "climb_one_engine_out");
}

GLIDESLOPE_TEST(the_boeing_737_300_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows) {
    expect_figure("737-300", "cruise_mach");
}

GLIDESLOPE_TEST(the_boeing_737_300_still_climbs_at_its_certificated_ceiling) {
    expect_figure("737-300", "ceiling");
}

GLIDESLOPE_TEST(the_airbus_a320_needs_about_its_published_takeoff_runway_length_at_maximum_weight) {
    expect_figure("a320", "takeoff_field_length");
}

GLIDESLOPE_TEST(the_airbus_a320_climbs_with_an_engine_out_as_far_25_demands) {
    expect_figure("a320", "climb_one_engine_out");
}

GLIDESLOPE_TEST(the_airbus_a320_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows) {
    expect_figure("a320", "cruise_mach");
}

GLIDESLOPE_TEST(the_airbus_a320_still_climbs_at_its_certificated_ceiling) {
    expect_figure("a320", "ceiling");
}

GLIDESLOPE_TEST(the_boeing_747_400_needs_about_its_published_takeoff_runway_length_at_maximum_weight) {
    expect_figure("747-400", "takeoff_field_length");
}

GLIDESLOPE_TEST(the_boeing_747_400_climbs_with_an_engine_out_as_far_25_demands) {
    expect_figure("747-400", "climb_one_engine_out");
}

GLIDESLOPE_TEST(the_boeing_747_400_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows) {
    expect_figure("747-400", "cruise_mach");
}

GLIDESLOPE_TEST(the_boeing_747_400_still_climbs_at_its_certificated_ceiling) {
    expect_figure("747-400", "ceiling");
}

GLIDESLOPE_TEST(the_boeing_787_8_needs_about_its_published_takeoff_runway_length_at_maximum_weight) {
    expect_figure("787-8", "takeoff_field_length");
}

GLIDESLOPE_TEST(the_boeing_787_8_climbs_with_an_engine_out_as_far_25_demands) {
    expect_figure("787-8", "climb_one_engine_out");
}

GLIDESLOPE_TEST(the_boeing_787_8_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows) {
    expect_figure("787-8", "cruise_mach");
}

GLIDESLOPE_TEST(the_boeing_787_8_still_climbs_at_its_certificated_ceiling) {
    expect_figure("787-8", "ceiling");
}

GLIDESLOPE_TEST(the_airbus_a380_needs_about_its_published_takeoff_field_length_at_maximum_weight) {
    expect_figure("a380", "takeoff_field_length");
}

GLIDESLOPE_TEST(the_airbus_a380_climbs_with_an_engine_out_as_jar_25_demands) {
    expect_figure("a380", "climb_one_engine_out");
}

GLIDESLOPE_TEST(the_airbus_a380_reaches_its_cruise_mach_and_no_further_than_its_drag_rise_allows) {
    expect_figure("a380", "cruise_mach");
}

GLIDESLOPE_TEST(the_airbus_a380_still_climbs_at_its_certificated_ceiling) {
    expect_figure("a380", "ceiling");
}

GLIDESLOPE_TEST(the_learjet_35a_needs_about_its_flight_manuals_takeoff_field_length_at_maximum_weight) {
    expect_figure("learjet35a", "takeoff_field_length");
}

GLIDESLOPE_TEST(the_learjet_35a_climbs_with_an_engine_out_as_its_flight_manual_demands) {
    expect_figure("learjet35a", "climb_one_engine_out");
}

GLIDESLOPE_TEST(the_learjet_35a_reaches_the_c21as_speed_at_41000_ft) {
    expect_figure("learjet35a", "cruise_mach");
}

GLIDESLOPE_TEST(the_learjet_35a_still_climbs_at_its_certificated_ceiling) {
    expect_figure("learjet35a", "ceiling");
}

GLIDESLOPE_TEST(the_learjet_35a_stalls_near_its_flight_manuals_speed_with_8_degrees_of_flap) {
    expect_figure("learjet35a", "stall_speed_flaps_8");
}

GLIDESLOPE_TEST(the_learjet_35a_stalls_near_its_flight_manuals_speed_flaps_up) {
    expect_figure("learjet35a", "stall_speed_flaps_up");
}

GLIDESLOPE_TEST(the_learjet_35a_stalls_near_its_flight_manuals_speed_with_40_degrees_of_flap) {
    expect_figure("learjet35a", "stall_speed_flaps_40");
}

GLIDESLOPE_TEST(the_f35b_reaches_mach_1_6_at_its_best_altitude) {
    expect_figure("f35b", "maximum_mach");
}

// **The F-35B is held to no ceiling, and there is no test for one.** The
// F-35A was held to the Air Force's "above 50,000 feet"; that figure is the
// A's, and Lockheed publishes no service ceiling for any F-35. Nothing
// published gives the B one, so nothing here claims it.

GLIDESLOPE_TEST(the_f35b_flies_more_than_its_published_range_on_internal_fuel) {
    expect_figure("f35b", "range");
}

GLIDESLOPE_TEST(the_b2_flies_at_high_subsonic_speed) {
    expect_figure("b2", "high_subsonic_speed");
}

GLIDESLOPE_TEST(the_b2_still_climbs_at_its_published_ceiling) {
    expect_figure("b2", "climb_rate_50000_ft");
}

GLIDESLOPE_TEST(the_b2_flies_about_its_published_range_unrefuelled) {
    expect_figure("b2", "range");
}

GLIDESLOPE_TEST(the_f15c_reaches_its_published_maximum_mach_at_45000_ft) {
    expect_figure("f15c", "maximum_mach_45000_ft");
}

GLIDESLOPE_TEST(the_f15c_climbs_at_its_published_rate_at_sea_level_at_military_power) {
    expect_figure("f15c", "climb_rate_sea_level_military");
}

GLIDESLOPE_TEST(the_f15c_climbs_at_its_published_rate_at_sea_level_at_maximum_power) {
    expect_figure("f15c", "climb_rate_sea_level_maximum");
}

GLIDESLOPE_TEST(the_f15c_reaches_its_published_service_ceiling_at_military_power) {
    expect_figure("f15c", "service_ceiling_military");
}

GLIDESLOPE_TEST(the_f15c_reaches_its_published_combat_ceiling_at_maximum_power) {
    expect_figure("f15c", "combat_ceiling_maximum");
}

GLIDESLOPE_TEST(the_f15c_sustains_its_published_turn_at_mach_0_9_and_30000_ft) {
    expect_figure("f15c", "sustained_turn_mach_0.9_30000_ft");
}

GLIDESLOPE_TEST(the_f22_supercruises_at_its_published_mach) {
    expect_figure("f22", "supercruise_mach");
}

GLIDESLOPE_TEST(the_f22_accelerates_from_mach_0_8_to_1_5_in_its_published_time) {
    expect_figure("f22", "acceleration_0.8_to_1.5_30000_ft");
}

GLIDESLOPE_TEST(the_f22_sustains_its_published_turn_at_mach_0_9_and_30000_ft) {
    expect_figure("f22", "sustained_turn_mach_0.9_30000_ft");
}

GLIDESLOPE_TEST(the_f22_flies_at_mach_2_at_40000_ft) {
    expect_figure("f22", "maximum_mach_40000_ft");
}

GLIDESLOPE_TEST(the_f22_still_climbs_at_50000_ft) {
    expect_figure("f22", "climb_rate_50000_ft");
}

// A figure that asks for flaps of an aircraft without them - the Cub has none,
// and its file says so with a travel of 0 - is refused, not flown with a
// flap command divided by nothing.
GLIDESLOPE_TEST(a_figure_asking_for_flaps_the_aircraft_does_not_have_is_refused) {
    const PublishedFigures figures = read_published_figures(figures_file("j3cub"));
    check(figures.flaps_full_deg == 0.0, "the Cub's file gives it no flaps");
    auto it = std::find_if(figures.figures.begin(), figures.figures.end(),
                           [](const auto& f) { return f.name == "stall_speed"; });
    if (it == figures.figures.end()) {
        fail("assets/figures/j3cub.xml has no figure named stall_speed");
    }
    auto spec = *it;
    spec.conditions["flaps_deg"] = 10.0;
    try {
        fly_figure(data_dir, figures, spec);
        fail("a stall with 10 degrees of flap was flown on an aircraft without flaps");
    } catch (const std::runtime_error& e) {
        check(std::string(e.what()).find("has none") != std::string::npos,
              std::string("refused as having no flaps: ") + e.what());
    }
}

GLIDESLOPE_TEST(the_mosquito_fb6_flies_level_at_sea_level_at_hx809s_speed) {
    expect_figure("mosquito-fb6", "level_speed_sea_level");
}

GLIDESLOPE_TEST(the_mosquito_fb6_flies_level_at_its_ms_gear_full_throttle_height_at_hx809s_speed) {
    expect_figure("mosquito-fb6", "level_speed_ms_gear");
}

GLIDESLOPE_TEST(the_mosquito_fb6_flies_level_at_its_fs_gear_full_throttle_height_at_hx809s_speed) {
    expect_figure("mosquito-fb6", "level_speed_fs_gear");
}

GLIDESLOPE_TEST(the_mosquito_fb6_flies_level_at_18000_ft_at_hx809s_speed) {
    expect_figure("mosquito-fb6", "level_speed_18000_ft");
}

GLIDESLOPE_TEST(the_mosquito_fb6_climbs_in_ms_gear_near_hj679s_rate) {
    expect_figure("mosquito-fb6", "climb_rate_ms_gear");
}

GLIDESLOPE_TEST(the_mosquito_fb6_climbs_in_fs_gear_near_hj679s_rate) {
    expect_figure("mosquito-fb6", "climb_rate_fs_gear");
}

GLIDESLOPE_TEST(the_mosquito_fb6_climbs_to_20000_ft_in_about_hj679s_time) {
    expect_figure("mosquito-fb6", "time_to_20000_ft");
}

GLIDESLOPE_TEST(the_mosquito_fb6_stalls_clean_near_its_pilots_notes_speed) {
    expect_figure("mosquito-fb6", "stall_speed_clean");
}

GLIDESLOPE_TEST(the_mosquito_fb6_stalls_wheels_and_flaps_down_near_its_pilots_notes_speed) {
    expect_figure("mosquito-fb6", "stall_speed_landing");
}

GLIDESLOPE_TEST(the_mosquito_fb6_takes_off_over_50_ft_in_about_the_b_ivs_distance) {
    expect_figure("mosquito-fb6", "takeoff_distance_b4");
}

GLIDESLOPE_TEST(the_mosquitos_slight_swing_to_port_is_checked_by_the_port_throttle_slightly_ahead) {
    expect_figure("mosquito-fb6", "takeoff_swing");
}

GLIDESLOPE_TEST(the_mosquito_fb6_is_held_straight_on_one_engine_down_to_its_safety_speed_at_18_boost) {
    expect_figure("mosquito-fb6", "safety_speed_18");
}

GLIDESLOPE_TEST(the_mosquito_fb6_is_held_straight_on_one_engine_down_to_its_safety_speed_at_9_boost) {
    expect_figure("mosquito-fb6", "safety_speed_9");
}

GLIDESLOPE_TEST(the_mosquito_fb6_holds_its_height_on_one_engine_up_to_its_pilots_notes_ceiling) {
    expect_figure("mosquito-fb6", "single_engine_ceiling");
}

GLIDESLOPE_TEST(the_short_s23_takes_off_from_water_at_45000_lb_in_about_gouges_time) {
    expect_figure("short_s23", "water_takeoff_time_long_range");
}

GLIDESLOPE_TEST(the_short_s23_takes_off_from_water_at_45000_lb_in_about_gouges_run) {
    expect_figure("short_s23", "water_takeoff_run_long_range");
}

GLIDESLOPE_TEST(the_short_s23_takes_off_from_water_at_its_standard_weight_in_its_published_time) {
    expect_figure("short_s23", "water_takeoff_time_standard");
}

GLIDESLOPE_TEST(the_short_s23_floats_at_the_draught_its_general_arrangement_draws) {
    expect_figure("short_s23", "draught");
}

GLIDESLOPE_TEST(the_short_s23_reaches_its_published_speed_at_5500_ft) {
    expect_figure("short_s23", "level_speed");
}

GLIDESLOPE_TEST(the_short_s23_climbs_at_its_published_rate_at_sea_level) {
    expect_figure("short_s23", "climb_rate");
}

// Every flight the checks can fly measures a figure in some aircraft's file,
// and every figure in every file names a flight - so a figure added to a file
// with no flight, or a flight written and never given a figure, fails here
// rather than never running. Each figure also has one test above;
// registered_tests.cmake keeps those registered, and this keeps the two lists
// the same size.
GLIDESLOPE_TEST(every_published_figure_has_a_flight_and_every_flight_a_figure) {
    std::set<std::string> used;
    std::size_t figures_in_files = 0;
    for (const std::string& model : figured_models()) {
        const PublishedFigures figures = read_published_figures(figures_file(model));
        std::set<std::string> names;
        for (const auto& f : figures.figures) {
            check(names.insert(f.name).second,
                  "figure " + f.name + " is in " + model + ".xml twice");
            used.insert(f.flight);
        }
        figures_in_files += figures.figures.size();
    }
    const auto flights = known_figures();
    const std::set<std::string> flown(flights.begin(), flights.end());
    for (const auto& name : flown) {
        check(used.count(name) == 1, "flight " + name + " measures no figure");
    }
    for (const auto& name : used) {
        check(flown.count(name) == 1, "figures name a flight " + name +
                                          " that does not exist");
    }
    // Ninety-three, where there were ninety-four: the F-35A's ceiling went
    // when it became the F-35B, because Lockheed Martin publishes no service
    // ceiling for any F-35 and the "above 50,000 feet" the A was held to is
    // the Air Force's, for the A alone.
    check(figures_in_files == 93,
          "ninety-three figures, one test each above; found " +
              std::to_string(figures_in_files));
}
