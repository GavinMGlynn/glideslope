#include "harness.hpp"

#include "sim/catalogue.hpp"
#include "sim/controller.hpp"
#include "sim/crash.hpp"
#include "sim/plan.hpp"
#include "sim/figures.hpp"
#include "sim/hover.hpp"
#include "sim/learnt.hpp"
#include "sim/transition.hpp"
#include "sim/lander.hpp"
#include "sim/weather.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <memory>
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

// **The F-35B's lift system**: its thrust at hover power is Pratt &
// Whitney's; held by the hover control law at 150 ft for a minute, as BF-1
// first did, it keeps its height within 10 ft; and let down from there it
// touches under 6 ft/s. Each was seen to fail with the lift system taken out
// of her model (PROJECT_STATUS, 2026-10-10).
GLIDESLOPE_TEST(the_f35bs_lift_system_makes_pratt_and_whitneys_hover_thrust) {
    expect_figure("f35b", "hover_thrust");
}

GLIDESLOPE_TEST(the_f35b_hovers_at_150_ft_for_a_minute_within_10_ft) {
    expect_figure("f35b", "hover_height_held");
}

GLIDESLOPE_TEST(the_f35b_lands_vertically_from_a_hover_under_6_ft_a_second) {
    expect_figure("f35b", "vertical_landing");
}

// **She converts between wing-borne flight and a hover, both ways**, at her
// vertical-landing and her combat loadings (sim/transition.hpp): from 200
// KCAS level at 150 ft to a hover there, as BF-1's first hover was flown,
// and from the hover back to 200 KCAS converted; never losing more than 10
// ft nor rising more than 25, nor passing 15 degrees of pitch or 10 of bank,
// nor touching the ground.
GLIDESLOPE_TEST(
    the_f35b_slows_from_200_kt_level_to_a_hover_at_150_ft_within_10_ft_and_her_attitudes) {
    expect_figure("f35b", "transition_to_hover");
}

GLIDESLOPE_TEST(
    the_f35b_slows_to_a_hover_at_her_combat_weight_within_10_ft_and_her_attitudes) {
    expect_figure("f35b", "transition_to_hover_combat");
}

GLIDESLOPE_TEST(
    the_f35b_accelerates_from_a_hover_to_200_kt_and_converts_back_within_10_ft_and_her_attitudes) {
    expect_figure("f35b", "transition_from_hover");
}

GLIDESLOPE_TEST(
    the_f35b_accelerates_out_of_a_hover_at_her_combat_weight_within_10_ft_and_her_attitudes) {
    expect_figure("f35b", "transition_from_hover_combat");
}

// **Her short take-off is measured**: the model's own, held so that it still
// does what it did. Seen to fail with the fan's thrust held straight down.
GLIDESLOPE_TEST(the_f35b_takes_off_short_in_the_distance_her_own_model_gave) {
    expect_figure("f35b", "short_takeoff");
}

// **And inside her requirement's 600 ft** (the Selected Acquisition Report's
// key performance parameter). Seen to fail with her flaperons' droop taken
// out of her built model (699.4 ft).
GLIDESLOPE_TEST(the_f35b_takes_off_short_inside_her_required_600_ft) {
    expect_figure("f35b", "short_takeoff_required");
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

// **The speedbrake an aeroplane is flown down an approach with is read from
// its figures**, is none where they give none, and is refused outside the
// lever's travel rather than flown: every speed and setting is data. Each
// file tried is the B-2A's own with that one attribute changed, so a refusal
// is the attribute's and nothing else's - and a setting inside the travel,
// written the same way, is read back.
GLIDESLOPE_TEST(the_speedbrake_an_approach_is_flown_with_is_read_and_refused_outside_its_travel) {
    check(read_published_figures(figures_file("b2")).approach_speedbrake == 0.5,
          "the B-2A is flown down with its drag rudders half open");
    check(read_published_figures(figures_file("737-300")).approach_speedbrake == 0.0,
          "the 737-300 gives none, so none is used");

    std::ifstream in(figures_file("b2"));
    const std::string b2((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string attribute = "approach_speedbrake=\"0.5\"";
    const auto at = b2.find(attribute);
    if (at == std::string::npos) {
        fail("assets/figures/b2.xml does not give approach_speedbrake=\"0.5\"");
    }
    // A name no other run of this test, in this build or another, shares.
    const auto file =
        std::filesystem::temp_directory_path() /
        ("glideslope_approach_speedbrake_" + std::to_string(std::random_device{}()) + "_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".xml");
    const auto write = [&](const std::string& value) {
        std::string text = b2;
        text.replace(at, attribute.size(), "approach_speedbrake=\"" + value + "\"");
        std::ofstream out(file);
        out << text;
    };

    write("0.25");
    check(read_published_figures(file).approach_speedbrake == 0.25,
          "a setting inside the lever's travel is read back as written");
    const char* const wrong[] = {"-0.1", "1.5"};
    std::size_t refused = 0;
    for (const char* value : wrong) {
        write(value);
        try {
            read_published_figures(file);
        } catch (const std::runtime_error& e) {
            const std::string said = e.what();
            check(said.find("approach_speedbrake") != std::string::npos,
                  std::string("the refusal of ") + value + " names approach_speedbrake: " + said);
            ++refused;
        }
    }
    std::filesystem::remove(file);
    check(refused == std::size(wrong), "both settings outside 0 to 1 were refused, not " +
                                           std::to_string(refused));
}

// **The sink a flare brings the wheels to the runway at is read from the
// figures**, is none where they give none - and the approach autopilot's
// forty then - and is refused where the gear could not take it, rather than
// flown at: every speed and setting is data. Each file tried is the 737-300's
// own with that one attribute changed, so a refusal is the attribute's and
// nothing else's; a sink the gear takes, written the same way, is read back.
GLIDESLOPE_TEST(the_sink_a_flare_touches_down_at_is_read_and_refused_past_what_the_gear_takes) {
    check(read_published_figures(figures_file("737-300")).touchdown_fpm == 200.0,
          "the 737-300's flare touches down at 200 ft/min");
    check(read_published_figures(figures_file("c172p")).touchdown_fpm == 0.0,
          "the C172P gives none, so the approach autopilot's own is used");

    std::ifstream in(figures_file("737-300"));
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const std::string attribute = "touchdown_fpm=\"200\"";
    const auto at = text.find(attribute);
    if (at == std::string::npos) {
        fail("assets/figures/737-300.xml does not give touchdown_fpm=\"200\"");
    }
    // A name no other run of this test, in this build or another, shares.
    const auto file =
        std::filesystem::temp_directory_path() /
        ("glideslope_touchdown_fpm_" + std::to_string(std::random_device{}()) + "_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".xml");
    const auto write = [&](const std::string& value) {
        std::string changed = text;
        changed.replace(at, attribute.size(), "touchdown_fpm=\"" + value + "\"");
        std::ofstream out(file);
        out << changed;
    };

    write("599");
    check(read_published_figures(file).touchdown_fpm == 599.0,
          "a sink just under what the gear takes is read back as written");
    const char* const wrong[] = {"0", "-40", "600", "900"};
    std::size_t refused = 0;
    for (const char* value : wrong) {
        write(value);
        try {
            read_published_figures(file);
        } catch (const std::runtime_error& e) {
            const std::string said = e.what();
            check(said.find("touchdown_fpm") != std::string::npos,
                  std::string("the refusal of ") + value + " names touchdown_fpm: " + said);
            ++refused;
        }
    }
    std::filesystem::remove(file);
    check(refused == std::size(wrong), "all four sinks outside 0 to 600 were refused, not " +
                                           std::to_string(refused));
}

// **The rate her rotation is held to is read, and one that is no rotation or
// a snatch is refused** (`rotation_degps`, sim::Departure): the A320's file
// read as it is, a J-3 Cub's - a tail-wheel aeroplane, which gives none,
// and is not held - as nought,
// and the A320's own with that one attribute changed: ten, the most allowed,
// read back; nought, less and more than ten refused, each naming it.
GLIDESLOPE_TEST(the_rotation_rate_a_take_off_is_held_to_is_read_and_refused_outside_0_to_10) {
    check(read_published_figures(figures_file("a320")).rotation_degps == 3.0,
          "the A320 is rotated at three degrees a second");
    check(read_published_figures(figures_file("j3cub")).rotation_degps == 0.0,
          "the J-3 Cub gives none, so her rotation is not held");

    std::ifstream in(figures_file("a320"));
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const std::string attribute = "rotation_degps=\"3\"";
    const auto at = text.find(attribute);
    if (at == std::string::npos) {
        fail("assets/figures/a320.xml does not give rotation_degps=\"3\"");
    }
    // A name no other run of this test, in this build or another, shares.
    const auto file =
        std::filesystem::temp_directory_path() /
        ("glideslope_rotation_degps_" + std::to_string(std::random_device{}()) + "_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".xml");
    const auto write = [&](const std::string& value) {
        std::string changed = text;
        changed.replace(at, attribute.size(), "rotation_degps=\"" + value + "\"");
        std::ofstream out(file);
        out << changed;
    };

    write("10");
    check(read_published_figures(file).rotation_degps == 10.0,
          "a rate of ten, the most allowed, is read back as written");
    const char* const wrong[] = {"0", "-3", "10.5", "40"};
    std::size_t refused = 0;
    for (const char* value : wrong) {
        write(value);
        try {
            read_published_figures(file);
        } catch (const std::runtime_error& e) {
            const std::string said = e.what();
            check(said.find("rotation_degps") != std::string::npos,
                  std::string("the refusal of ") + value + " names rotation_degps: " + said);
            ++refused;
        }
    }
    std::filesystem::remove(file);
    check(refused == std::size(wrong), "all four rates outside 0 to 10 were refused, not " +
                                           std::to_string(refused));
}

// **The stick her rate loop asks per degree a second is read, and one that
// is none or more than the whole stick is refused**
// (`rotation_stick_per_degps`, sim::Departure): the C172P's tenth read as
// written, the A320's - which gives none, and takes the loop's 0.3 - as
// nought, and the A320's file given one: 1, the most allowed, read back;
// nought, less and more than 1 refused, each naming it.
GLIDESLOPE_TEST(the_rotation_loops_stick_per_degree_a_second_is_read_and_refused_outside_0_to_1) {
    check(read_published_figures(figures_file("c172p")).rotation_stick_per_degps == 0.1,
          "the C172P's rate loop asks a tenth of the stick per degree a second");
    check(read_published_figures(figures_file("a320")).rotation_stick_per_degps == 0.0,
          "the A320 gives none");

    std::ifstream in(figures_file("a320"));
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const std::string attribute = "rotation_degps=\"3\"";
    const auto at = text.find(attribute);
    if (at == std::string::npos) {
        fail("assets/figures/a320.xml does not give rotation_degps=\"3\"");
    }
    const auto file =
        std::filesystem::temp_directory_path() /
        ("glideslope_rotation_stick_" + std::to_string(std::random_device{}()) + "_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".xml");
    const auto write = [&](const std::string& value) {
        std::string changed = text;
        changed.replace(at, attribute.size(),
                        attribute + " rotation_stick_per_degps=\"" + value + "\"");
        std::ofstream out(file);
        out << changed;
    };

    write("1");
    check(read_published_figures(file).rotation_stick_per_degps == 1.0,
          "a gain of 1, the most allowed, is read back as written");
    const char* const wrong[] = {"0", "-0.1", "1.5"};
    std::size_t refused = 0;
    for (const char* value : wrong) {
        write(value);
        try {
            read_published_figures(file);
        } catch (const std::runtime_error& e) {
            const std::string said = e.what();
            check(said.find("rotation_stick_per_degps") != std::string::npos,
                  std::string("the refusal of ") + value + " names rotation_stick_per_degps: " +
                      said);
            ++refused;
        }
    }
    std::filesystem::remove(file);
    check(refused == std::size(wrong), "all three gains outside 0 to 1 were refused, not " +
                                           std::to_string(refused));
}

// **Her elevator's pitching moment at full travel is read, and one that is
// none or past any aeroplane's is refused** (`elevator_full_cm`, which damps
// the lander's hold of a hull's trim on the step): the S.23's 0.28 read as
// written, the A320's - which gives none - as nought, and the A320's file
// given one: 2, the most allowed, read back; nought, less and more than 2
// refused, each naming it.
GLIDESLOPE_TEST(an_elevators_full_travel_pitching_moment_is_read_and_refused_outside_0_to_2) {
    check(read_published_figures(figures_file("short_s23")).elevator_full_cm == 0.28,
          "the S.23's elevator gives a pitching moment coefficient of 0.28 at full travel");
    check(read_published_figures(figures_file("a320")).elevator_full_cm == 0.0,
          "the A320 gives none");

    std::ifstream in(figures_file("a320"));
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const std::string attribute = "rotation_degps=\"3\"";
    const auto at = text.find(attribute);
    if (at == std::string::npos) {
        fail("assets/figures/a320.xml does not give rotation_degps=\"3\"");
    }
    const auto file =
        std::filesystem::temp_directory_path() /
        ("glideslope_elevator_cm_" + std::to_string(std::random_device{}()) + "_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".xml");
    const auto write = [&](const std::string& value) {
        std::string changed = text;
        changed.replace(at, attribute.size(), attribute + " elevator_full_cm=\"" + value + "\"");
        std::ofstream out(file);
        out << changed;
    };

    write("2");
    check(read_published_figures(file).elevator_full_cm == 2.0,
          "a coefficient of 2, the most allowed, is read back as written");
    const char* const wrong[] = {"0", "-0.28", "2.5"};
    std::size_t refused = 0;
    for (const char* value : wrong) {
        write(value);
        try {
            read_published_figures(file);
        } catch (const std::runtime_error& e) {
            const std::string said = e.what();
            check(said.find("elevator_full_cm") != std::string::npos,
                  std::string("the refusal of ") + value + " names elevator_full_cm: " + said);
            ++refused;
        }
    }
    std::filesystem::remove(file);
    check(refused == std::size(wrong), "all three coefficients outside 0 to 2 were refused, not " +
                                           std::to_string(refused));
}

// **Every aircraft's flare touches down within NASA's criterion.** Zaal et
// al., "Go-Around Criteria Refinement for Transport Category Aircraft"
// (AIAA Journal of Air Transportation, NTRS 20205010611), take a touchdown
// sink of 6 ft/s, 360 ft/min, as the most a landing may come down at - an
// upper bound, not a typical figure. Every aircraft in the catalogue is walked: its
// target is its figures' `touchdown_fpm`, or the approach autopilot's own
// where they give none, and where it is landed by the AI (`landing_speeds`)
// that is the sink it is told.
GLIDESLOPE_TEST(every_aircrafts_touchdown_sink_is_within_nasas_go_around_criterion) {
    const double criterion_fpm = 6.0 * 60.0;
    const auto data = std::filesystem::path(data_dir).parent_path();
    const auto catalogue = glideslope::sim::read_catalogue(data);
    std::size_t walked = 0;
    std::size_t landed = 0;
    for (const auto& e : catalogue) {
        ++walked;
        const PublishedFigures figures = read_published_figures(figures_file(e.model));
        const double target = figures.touchdown_fpm > 0.0
                                  ? figures.touchdown_fpm
                                  : glideslope::sim::ApproachSpeeds{}.touchdown_fpm;
        check(target > 0.0 && target <= criterion_fpm,
              e.id + "'s flare touches down at " + std::to_string(target) +
                  " ft/min, not within NASA's " + std::to_string(criterion_fpm));
        const auto speeds = glideslope::sim::landing_speeds(data, e.model);
        if (speeds) {
            check(speeds->touchdown_fpm == target,
                  e.id + " is landed at " + std::to_string(speeds->touchdown_fpm) +
                      " ft/min, not its target " + std::to_string(target));
            ++landed;
        }
        std::printf("  %-14s %4.0f ft/min%s\n", e.id.c_str(), target,
                    speeds ? "" : " (no stall speed: not landed by the AI)");
    }
    std::printf("%zu aircraft walked, %zu of them landed by the AI\n", walked, landed);
    check(walked == catalogue.size() && walked == figured_models().size(),
          "every aircraft walked: " + std::to_string(walked) + " of " +
              std::to_string(figured_models().size()));
}

// **Only the Mosquito's figures give her own stall warning**
// (`stall_warning_kts`): her Pilot's Notes' buffet "some 10 knots before the
// stall" (A.P. 2019E, 1950, para. 43 (ii)). Every other aircraft is warned at
// the rule's 5 knots or 5% (sim::stall_warning_kts), so a file that gains one
// turns this red, to be added here with its source. Every aircraft in the
// catalogue is walked, and its landing speeds carry the margin its file gives.
GLIDESLOPE_TEST(only_the_mosquitos_figures_give_her_own_stall_warning_ten_knots_over_her_stall) {
    const std::map<std::string, double> expected = {{"mosquito-fb6", 10.0}};
    const auto data = std::filesystem::path(data_dir).parent_path();
    const auto catalogue = glideslope::sim::read_catalogue(data);
    std::size_t walked = 0;
    std::map<std::string, double> given;
    for (const auto& e : catalogue) {
        ++walked;
        const PublishedFigures figures = read_published_figures(figures_file(e.model));
        if (figures.stall_warning_kts > 0.0) {
            given[e.model] = figures.stall_warning_kts;
        }
        if (const auto speeds = glideslope::sim::landing_speeds(data, e.model)) {
            check(speeds->warning_margin_kts == figures.stall_warning_kts,
                  e.id + "'s landing speeds carry a warning margin of " +
                      std::to_string(speeds->warning_margin_kts) + ", not its file's " +
                      std::to_string(figures.stall_warning_kts));
        }
    }
    for (const auto& [model, kts] : given) {
        std::printf("  %-14s her own stall warning %4.1f kt over her stall\n", model.c_str(), kts);
    }
    std::printf("%zu aircraft walked, %zu give their own stall warning\n", walked, given.size());
    check(given == expected, std::to_string(given.size()) +
                                 " aircraft give their own stall warning; only the Mosquito's, "
                                 "10 kt, is expected");
    check(walked == catalogue.size() && walked == figured_models().size(),
          "every aircraft walked: " + std::to_string(walked) + " of " +
              std::to_string(figured_models().size()));
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

GLIDESLOPE_TEST(the_mosquito_fb6_climbs_with_flaps_and_wheels_down_at_climbing_power_as_her_pilots_notes_say) {
    expect_figure("mosquito-fb6", "climb_flaps_and_gear_down");
}

// **High up she cannot be level with her flaps and wheels down**, which is
// why her stall lesson is practised at 5,000 ft and not the jets' 20,000
// (assets/lessons/second-world-war-stalls.lesson). The same flight as her
// Pilot's Notes' climb with everything down (para. 48), flown at 20,000 ft
// on full throttle, the supercharger in high gear, at 130 knots - her stall
// lesson's recovery speed, 35 over her 95-knot stall - sinks: the Merlins
// have not the power there that the Notes' climb has low down.
GLIDESLOPE_TEST(the_mosquito_fb6_sinks_at_20000_ft_on_full_throttle_with_flaps_and_wheels_down) {
    const PublishedFigures figures = read_published_figures(figures_file("mosquito-fb6"));
    const auto it = std::find_if(figures.figures.begin(), figures.figures.end(),
                                 [](const auto& f) { return f.name == "climb_flaps_and_gear_down"; });
    check(it != figures.figures.end(), "her figures give her climb with everything down");
    auto high = *it;
    high.conditions["altitude_ft"] = 20000.0;
    high.conditions["speed_kcas"] = 130.0;
    high.conditions["rpm"] = 3000.0;
    high.conditions["fs_gear_above_ft"] = 0.0;
    high.conditions.erase("boost_psi");
    high.published = 0.0;
    const double fpm = fly_figure(data_dir, figures, high).measured;
    std::printf("at 20,000 ft, 130 kt, full throttle, flaps 45 and wheels down: %.0f ft/min\n",
                fpm);
    check(fpm < 0.0, "she sinks there, " + std::to_string(fpm) + " ft/min");
}

// **Clean she climbs faster than with everything down**, at the same height,
// speed and power: the upper bound of her figure for the climb with flaps
// and wheels down is this clean climb, measured from her model, and this
// holds it there - the everything-down climb's figure must stay under the
// clean one the model flies now.
GLIDESLOPE_TEST(the_mosquito_fb6_climbs_faster_clean_than_with_flaps_and_wheels_down) {
    const PublishedFigures figures = read_published_figures(figures_file("mosquito-fb6"));
    const auto it = std::find_if(figures.figures.begin(), figures.figures.end(),
                                 [](const auto& f) { return f.name == "climb_flaps_and_gear_down"; });
    check(it != figures.figures.end(), "her figures give her climb with everything down");
    const double down = fly_figure(data_dir, figures, *it).measured;
    auto clean = *it;
    clean.conditions["flaps_deg"] = 0.0;
    clean.conditions["gear"] = 0.0;
    const double clean_fpm = fly_figure(data_dir, figures, clean).measured;
    std::printf("at 1,000 ft, 120 kt, +9 and 2,850 rpm: clean %.0f ft/min, everything down "
                "%.0f, the figure's upper bound %.0f\n",
                clean_fpm, down, it->high);
    check(down < clean_fpm, "with everything down she climbs slower than clean");
    // Within 2%, what another machine's floating point may move it by.
    check(std::abs(it->high - clean_fpm) <= 0.02 * clean_fpm,
          "the figure's upper bound, " + std::to_string(it->high) +
              ", is her clean climb, " + std::to_string(clean_fpm));
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

// **These aeroplanes publish nothing to hold their models to.** No flight
// manual for any of them is public, so their stall speeds and their rates of
// climb are measured from the models themselves and written down
// (docs/ASSETS.md). What each of these checks is that the model still does
// what it did when the number was taken - not that the aeroplane does it.

GLIDESLOPE_TEST(the_boeing_737_300_stalls_where_its_own_model_said_it_would) {
    expect_figure("737-300", "stall_speed_landing");
}

GLIDESLOPE_TEST(the_boeing_787_8_stalls_where_its_own_model_said_it_would) {
    expect_figure("787-8", "stall_speed_landing");
}

GLIDESLOPE_TEST(the_airbus_a320_stalls_where_its_own_model_said_it_would) {
    expect_figure("a320", "stall_speed_landing");
}

GLIDESLOPE_TEST(the_airbus_a380_stalls_where_its_own_model_said_it_would) {
    expect_figure("a380", "stall_speed_landing");
}

GLIDESLOPE_TEST(the_b2_stalls_where_its_own_model_said_it_would) {
    expect_figure("b2", "stall_speed_landing");
}

GLIDESLOPE_TEST(the_f15c_stalls_where_its_own_model_said_it_would) {
    expect_figure("f15c", "stall_speed_landing");
}

GLIDESLOPE_TEST(the_f35b_stalls_where_its_own_model_said_it_would) {
    expect_figure("f35b", "stall_speed_landing");
}

GLIDESLOPE_TEST(the_short_s23_stalls_where_its_own_model_said_it_would) {
    expect_figure("short_s23", "stall_speed_landing");
}

// The same four airliners' stalls at their take-off flaps, which a take-off
// lesson rotates from (sim::departure_speeds).

GLIDESLOPE_TEST(the_boeing_737_300_stalls_at_its_take_off_flap_where_its_own_model_said_it_would) {
    expect_figure("737-300", "stall_speed_takeoff");
}

GLIDESLOPE_TEST(the_boeing_787_8_stalls_at_its_take_off_flap_where_its_own_model_said_it_would) {
    expect_figure("787-8", "stall_speed_takeoff");
}

GLIDESLOPE_TEST(the_airbus_a320_stalls_at_its_take_off_flap_where_its_own_model_said_it_would) {
    expect_figure("a320", "stall_speed_takeoff");
}

GLIDESLOPE_TEST(the_airbus_a380_stalls_at_its_take_off_flap_where_its_own_model_said_it_would) {
    expect_figure("a380", "stall_speed_takeoff");
}

GLIDESLOPE_TEST(the_boeing_737_300_climbs_at_the_rate_its_own_model_gave) {
    expect_figure("737-300", "climb_rate");
}

GLIDESLOPE_TEST(the_boeing_787_8_climbs_at_the_rate_its_own_model_gave) {
    expect_figure("787-8", "climb_rate");
}

GLIDESLOPE_TEST(the_airbus_a320_climbs_at_the_rate_its_own_model_gave) {
    expect_figure("a320", "climb_rate");
}

GLIDESLOPE_TEST(the_airbus_a380_climbs_at_the_rate_its_own_model_gave) {
    expect_figure("a380", "climb_rate");
}

GLIDESLOPE_TEST(the_b2_climbs_at_the_rate_its_own_model_gave) {
    expect_figure("b2", "climb_rate");
}

GLIDESLOPE_TEST(the_f15c_climbs_at_the_rate_its_own_model_gave) {
    expect_figure("f15c", "climb_rate");
}

GLIDESLOPE_TEST(the_f35b_climbs_at_the_rate_its_own_model_gave) {
    expect_figure("f35b", "climb_rate");
}

GLIDESLOPE_TEST(the_f35b_takes_off_in_the_ground_roll_its_own_model_gave) {
    expect_figure("f35b", "takeoff_ground_roll");
}

GLIDESLOPE_TEST(the_learjet_35a_climbs_at_the_rate_its_own_model_gave) {
    expect_figure("learjet35a", "climb_rate");
}

// **The AI climbs each light aeroplane to its published service ceiling**:
// the autopilot, handed the aeroplane at 6,000 ft full rich and asked for a
// height it cannot reach at its best-climb speed, leans the mixture for best
// power as it climbs (sim/leaner.hpp) and goes on climbing until its rate
// has fallen to 100 ft/min. Full rich, as the AI used to fly it, the Cherokee
// was held to about 7,300 ft.
GLIDESLOPE_TEST(the_ai_climbs_a_cherokee_180_to_its_published_service_ceiling) {
    expect_figure("pa28", "service_ceiling");
}

// **The Cub, flown solo, climbs to its manual's ceiling** on its float
// carburettor, which richens its mixture only as the square root of the
// density (tools/piston_mixture.py); with JSBSim's metering, which richens it
// as the pressure, it stopped climbing at about 8,000 ft.
GLIDESLOPE_TEST(the_ai_climbs_a_cub_flown_solo_to_its_published_service_ceiling) {
    expect_figure("j3cub", "service_ceiling");
}

// **The Cessna 182S, climbed at its handbook's speeds**: figure 5-7's best
// climb falls from 82 KCAS at sea level by 0.51 kt a thousand feet, leaned
// above 3,000 ft. With JSBSim's drag due to lift (an Oswald efficiency of
// 0.45 with the glide's share) it stopped at 12,886 ft against 18,100; with
// the wing's 0.69 and the windmilling propeller's drag charged only in the
// glide (tools/make_c182.py), about 17,000.
GLIDESLOPE_TEST(the_ai_climbs_a_cessna_182s_at_its_handbooks_speeds_to_its_published_service_ceiling) {
    expect_figure("c182", "service_ceiling");
}

// **The Cessna 172P, climbed at its handbook's speeds**: figure 5-6's best
// climb falls from 76 KIAS at sea level to 70 at 12,000 ft, and the AI is
// asked for that speed at each height, leaning as it climbs. Held at its
// sea-level speed on JSBSim's mixture curve it climbed to 17,200 ft.
GLIDESLOPE_TEST(the_ai_climbs_a_cessna_172p_at_its_handbooks_speeds_to_its_published_service_ceiling) {
    expect_figure("c172p", "service_ceiling");
}

// **Each light aeroplane's engine makes its rated power at its rated rpm**,
// full throttle and full rich at sea level, as its handbook or type
// certificate rates it. The Cessna 172P's made 222 hp at 2,700 rpm until its
// learnt landing was trained again on 160 (docs/PROJECT_STATUS.md).
GLIDESLOPE_TEST(the_cessna_172ps_engine_makes_160_hp_at_2700_rpm) {
    expect_figure("c172p", "rated_power");
}

GLIDESLOPE_TEST(the_cessna_182ss_engine_makes_230_hp_at_2400_rpm) {
    expect_figure("c182", "rated_power");
}

GLIDESLOPE_TEST(the_cherokee_180s_engine_makes_180_hp_at_2700_rpm) {
    expect_figure("pa28", "rated_power");
}

GLIDESLOPE_TEST(the_cubs_engine_makes_65_hp_at_2300_rpm) {
    expect_figure("j3cub", "rated_power");
}

// **Every light aeroplane the catalogue holds is climbed to its ceiling by
// the tests above, or named here with the reason it is not.** The light
// aeroplanes are counted from the catalogue, so a fifth one added without a
// ceiling, or without being named, turns this red.
// None is left out.
//
// The Cub is climbed with no mixture lever: its carburettor meters its
// mixture (tools/piston_mixture.py), and is the one named in `no_lever`.
GLIDESLOPE_TEST(every_light_aeroplane_is_climbed_to_its_published_ceiling_or_named_with_its_reason) {
    const std::map<std::string, std::string> left_out = {};
    const std::set<std::string> no_lever = {"j3cub"};
    const auto catalogue =
        glideslope::sim::read_catalogue(std::filesystem::path(data_dir).parent_path());
    std::size_t light = 0;
    std::size_t climbed = 0;
    std::size_t named = 0;
    for (const auto& e : catalogue) {
        if (e.aircraft_class != glideslope::sim::AircraftClass::light_aircraft) {
            continue;
        }
        ++light;
        const PublishedFigures figures = read_published_figures(figures_file(e.model));
        const bool has = std::any_of(figures.figures.begin(), figures.figures.end(),
                                     [](const auto& f) {
                                         return f.name == "service_ceiling" &&
                                                f.flight == "ceiling_on_the_autopilot";
                                     });
        if (left_out.count(e.id) != 0) {
            check(!has, e.id + " is named as left out, and has a ceiling on the autopilot");
            ++named;
        } else {
            check(has, e.id + " is a light aeroplane with no ceiling on the autopilot, "
                              "and is not named as left out");
            check(e.mixture_lever || no_lever.count(e.id) != 0,
                  e.id + " is climbed to its ceiling with no mixture lever for the "
                         "autopilot to lean, and is not named as having none");
            ++climbed;
        }
    }
    std::printf("%zu light aeroplanes: %zu climbed to their ceilings, %zu named\n", light,
                climbed, named);
    check(light == 4, "four light aeroplanes, found " + std::to_string(light));
    check(climbed + named == light && named == left_out.size(),
          "every light aeroplane climbed or named: " + std::to_string(climbed) + " + " +
              std::to_string(named) + " of " + std::to_string(light));
}

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
    // A hundred and nine, where there were ninety-three: sixteen measured
    // figures were added on 2026-09-23 - a stall speed for eight aeroplanes
    // that publish none and a rate of climb for eight - so that the classes
    // they belong to can be taught something beyond turns.
    // A hundred and thirteen: four airliners' stalls at their take-off flaps,
    // measured the same way on the same day, for their take-off lessons.
    // A hundred and fourteen: the F-35B's ground roll to the speed it can
    // lift off at, measured on 2026-09-24, which gives its take-off its
    // rotation speed (its stall's cannot be flown on a runway).
    // A hundred and fifteen: the Cherokee's service ceiling, as the AI
    // climbs to it, on 2026-09-27.
    // A hundred and nineteen: the Cessna 182S's, the Cherokee's and the Cub's
    // rated power at their rated rpm, and the Cub's ceiling, on 2026-10-06.
    // A hundred and twenty: the Cessna 172P's ceiling, as the AI climbs to
    // it at its handbook's speeds, on 2026-10-08.
    // A hundred and twenty-one: the Cessna 182S's ceiling, as the AI climbs
    // to it at its handbook's speeds, on 2026-10-09.
    // A hundred and twenty-two: the Cessna 172P's rated power, on 2026-10-09.
    // A hundred and twenty-three: the Mosquito's climb with her flaps and
    // wheels down, her Pilot's Notes' para. 48, on 2026-10-10.
    // A hundred and twenty-six: the F-35B's hover thrust, hover and
    // vertical landing, on 2026-10-10.
    // A hundred and twenty-seven: the F-35B's short take-off, measured, on
    // 2026-10-13.
    // A hundred and twenty-eight: her hover endurance, measured, the same day.
    // A hundred and twenty-nine: her short take-off inside her required 600
    // ft, the same day.
    // A hundred and thirty-three: her transitions to a hover and out of one,
    // each at two loadings, on 2026-10-13.
    check(figures_in_files == 133,
          "a hundred and thirty-three figures, one test each above; found " +
              std::to_string(figures_in_files));
}

// **The speeds a plan may fly an aircraft at are read, and a file without
// them, or with them wrong, is refused** rather than planned at a speed it
// cannot hold. Each file tried is the C172P's own with its <plan_speeds>
// changed, so a refusal is that element's and nothing else's.
GLIDESLOPE_TEST(the_speeds_a_plan_may_fly_an_aircraft_at_are_read_and_refused_where_they_are_wrong) {
    const PublishedFigures c172p = read_published_figures(figures_file("c172p"));
    check(c172p.plan_slowest_kcas == 60.0 && c172p.plan_fastest_kcas == 110.0 &&
              c172p.plan_weight_lbs == 1880.0,
          "the C172P may be planned from 60 to 110 kt, at her model's 1,880 lb");

    std::ifstream in(figures_file("c172p"));
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const std::string element = "<plan_speeds slowest_kcas=\"60\" fastest_kcas=\"110\" weight_lbs=\"1880\">";
    const auto at = text.find(element);
    if (at == std::string::npos) {
        fail("assets/figures/c172p.xml does not give " + element);
    }
    const auto end = text.find("</plan_speeds>", at);
    const std::string whole = text.substr(at, end + std::string("</plan_speeds>").size() - at);
    const auto file =
        std::filesystem::temp_directory_path() /
        ("glideslope_plan_speeds_" + std::to_string(std::random_device{}()) + "_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".xml");
    const auto write = [&](const std::string& in_its_place) {
        std::string changed = text;
        changed.replace(at, whole.size(), in_its_place);
        std::ofstream out(file);
        out << changed;
    };

    write("<plan_speeds slowest_kcas=\"55\" fastest_kcas=\"125\" weight_lbs=\"2000\">"
          "</plan_speeds>");
    const PublishedFigures written = read_published_figures(file);
    check(written.plan_slowest_kcas == 55.0 && written.plan_fastest_kcas == 125.0 &&
              written.plan_weight_lbs == 2000.0,
          "other speeds, written the same way, are read back as written");
    const std::pair<const char*, const char*> wrong[] = {
        {"", "gives no <plan_speeds"},
        {"<plan_speeds fastest_kcas=\"120\" weight_lbs=\"1880\"></plan_speeds>",
         "gives no <plan_speeds"},
        {"<plan_speeds slowest_kcas=\"60\" weight_lbs=\"1880\"></plan_speeds>",
         "gives no <plan_speeds"},
        // **Nor without the weight they were measured at** (sim::for_weight).
        {"<plan_speeds slowest_kcas=\"60\" fastest_kcas=\"120\"></plan_speeds>",
         "gives no <plan_speeds"},
        {"<plan_speeds slowest_kcas=\"60\" fastest_kcas=\"120\" weight_lbs=\"0\"></plan_speeds>",
         "not above 0"},
        {"<plan_speeds slowest_kcas=\"0\" fastest_kcas=\"120\" weight_lbs=\"1880\"></plan_speeds>",
         "not above 0"},
        {"<plan_speeds slowest_kcas=\"120\" fastest_kcas=\"120\" weight_lbs=\"1880\">"
         "</plan_speeds>",
         "the fastest above the slowest"},
        {"<plan_speeds slowest_kcas=\"130\" fastest_kcas=\"120\" weight_lbs=\"1880\">"
         "</plan_speeds>",
         "the fastest above the slowest"},
        {"<plan_speeds slowest_kcas=\"60\" fastest_kcas=\"120\" weight_lbs=\"1880\">"
         "</plan_speeds>"
         "<plan_speeds slowest_kcas=\"60\" fastest_kcas=\"120\" weight_lbs=\"1880\">"
         "</plan_speeds>",
         "gives <plan_speeds> twice"},
    };
    std::string failures;
    for (const auto& [in_its_place, says] : wrong) {
        write(in_its_place);
        try {
            read_published_figures(file);
            failures += std::string("\n  taken: ") + in_its_place;
        } catch (const std::runtime_error& e) {
            const std::string said = e.what();
            if (said.find(says) == std::string::npos) {
                failures += std::string("\n  ") + in_its_place + " refused, but not saying \"" +
                            says + "\": " + said;
            }
        }
    }
    std::filesystem::remove(file);
    check(failures.empty(), "every one wrong is refused, saying why:" + failures);
}

// **The slowest a glide may fly an aircraft at is read, and a file without
// it, or with it wrong, is refused**, as its plan speeds are: a copilot's
// glide slower than it stalls. The C172P's own file with its <glide_speeds>
// changed.
GLIDESLOPE_TEST(the_slowest_a_glide_may_fly_an_aircraft_at_is_read_and_refused_where_it_is_wrong) {
    const PublishedFigures c172p = read_published_figures(figures_file("c172p"));
    check(c172p.glide_slowest_kcas == 60.0, "the C172P may glide from 60 kt");
    std::ifstream in(figures_file("c172p"));
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const auto at = text.find("<glide_speeds slowest_kcas=\"60\">");
    if (at == std::string::npos) {
        fail("assets/figures/c172p.xml gives no <glide_speeds slowest_kcas=\"60\">");
    }
    const auto end = text.find("</glide_speeds>", at);
    const std::size_t length = end + std::string("</glide_speeds>").size() - at;
    const auto file =
        std::filesystem::temp_directory_path() /
        ("glideslope_glide_speeds_" + std::to_string(std::random_device{}()) + "_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".xml");
    const auto write = [&](const std::string& in_its_place) {
        std::string changed = text;
        changed.replace(at, length, in_its_place);
        std::ofstream out(file);
        out << changed;
    };
    write("<glide_speeds slowest_kcas=\"65\"></glide_speeds>");
    check(read_published_figures(file).glide_slowest_kcas == 65.0,
          "another, written the same way, is read back as written");
    const std::pair<const char*, const char*> wrong[] = {
        {"", "gives no <glide_speeds"},
        {"<glide_speeds></glide_speeds>", "gives no <glide_speeds"},
        {"<glide_speeds slowest_kcas=\"0\"></glide_speeds>", "not above 0"},
        {"<glide_speeds slowest_kcas=\"60\"></glide_speeds>"
         "<glide_speeds slowest_kcas=\"60\"></glide_speeds>",
         "gives <glide_speeds> twice"},
    };
    std::string failures;
    for (const auto& [in_its_place, says] : wrong) {
        write(in_its_place);
        try {
            read_published_figures(file);
            failures += std::string("\n  taken: ") + in_its_place;
        } catch (const std::runtime_error& e) {
            if (std::string(e.what()).find(says) == std::string::npos) {
                failures += std::string("\n  ") + in_its_place + " refused, but not saying \"" +
                            says + "\": " + e.what();
            }
        }
    }
    std::filesystem::remove(file);
    check(failures.empty(), "every one wrong is refused, saying why:" + failures);
}

GLIDESLOPE_TEST(the_f35b_hovers_five_and_a_half_minutes_on_her_vertical_landing_fuel) {
    expect_figure("f35b", "hover_endurance");
}

namespace {

// The F-35B loaded as her figures file's `loading` says, standing on the
// runway the figures are flown from, in a steady wind of `wind_kt` blowing
// from `from_deg` off her nose, clockwise.
std::unique_ptr<glideslope::sim::Aircraft> f35b_standing(const std::string& loading, double wind_kt,
                                                          double from_deg) {
    using namespace glideslope::sim;
    const PublishedFigures figures = read_published_figures(figures_file("f35b"));
    auto a = std::make_unique<Aircraft>(data_dir, "f35b");
    a->load(figures.loadings.at(loading).loading);
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 70.0;
    a->initialize(ic);
    // The air moves away from where the wind blows from.
    const double towards = (ic.heading_deg + from_deg + 180.0) * 3.14159265358979323846 / 180.0;
    Conditions air;
    air.wind_north_mps = wind_kt * 0.514444 * std::cos(towards);
    air.wind_east_mps = wind_kt * 0.514444 * std::sin(towards);
    a->set_weather(std::make_shared<SteadyWeather>(air));
    return a;
}

} // namespace

// **Her STOVL envelope**: converting is refused above the airspeed her model
// clears her lift system to, 250 KCAS, and allowed below it; conventional
// flight is never refused; an aircraft with no lift system refuses one.
GLIDESLOPE_TEST(the_f35b_is_refused_stovl_mode_above_250_kcas_and_allowed_below) {
    using namespace glideslope::sim;
    for (const double kcas : {240.0, 260.0}) {
        Aircraft a(data_dir, "f35b");
        InitialConditions ic;
        ic.latitude_deg = -33.9461;
        ic.longitude_deg = 151.1772;
        ic.altitude_ft = 5000.0;
        ic.terrain_elevation_ft = 0.0;
        ic.airspeed_kts = kcas;
        ic.gear = 0.0;
        a.initialize(ic);
        check(std::abs(a.property("fcs/stovl-limit-kcas") - 250.0) < 1e-9,
              "her model clears STOVL mode to 250 KCAS");
        check(std::abs(a.property("velocities/vc-kts") - kcas) < 1.0,
              "she is flying at " + std::to_string(kcas) + " KCAS");
        LiftSystem lift;
        lift.stovl = 1.0;
        bool refused = false;
        try {
            a.set_lift_system(lift);
        } catch (const std::invalid_argument&) {
            refused = true;
        }
        check(refused == (kcas > 250.0),
              "STOVL mode at " + std::to_string(kcas) + " KCAS was " + (refused ? "refused" : "allowed"));
        a.set_lift_system(LiftSystem{});
    }
    Aircraft c172p(data_dir, "c172p");
    bool refused = false;
    try {
        c172p.set_lift_system(LiftSystem{});
    } catch (const std::logic_error&) {
        refused = true;
    }
    check(refused, "an aircraft without a lift system refuses one");
}

// **The thrust split moves thrust and never makes or loses it**: at the
// split's -1, 0 and 1, standing on the brakes spooled up to her STOVL top
// and converting, every step of the conversion from stovl-pos-norm 0 to 1 -
// the nozzle's, the fan's and each post's thrust, the nozzle's side force,
// the reverser angle and the base they are reckoned from finite, and their
// total, as a share of the engine's own thrust, what it is at the split's
// middle at the same step, to rounding. Seen red without the split's clip in her model:
// at -1 early in the conversion the reverser's cosine passed 1 and every
// thrust was NaN.
GLIDESLOPE_TEST(the_f35bs_thrust_split_at_either_end_and_the_middle_keeps_her_thrust_finite_and_its_total_the_engines) {
    using namespace glideslope::sim;
    const std::vector<double> splits = {0.0, -1.0, 1.0}; // the middle first, the reference
    const int steps = 9 * 120;                          // the conversion takes 8 s
    const std::vector<std::string> thrusts = {
        "propulsion/engine/thrust-lbs", "propulsion/engine/reverser-angle-rad",
        "external_reactions/lift-fan/magnitude", "external_reactions/left-roll-post/magnitude",
        "external_reactions/right-roll-post/magnitude", "external_reactions/nozzle-side/magnitude",
        "fcs/lift-system-base-lbf", "fcs/thrust-split-share"};
    std::vector<double> middle(static_cast<std::size_t>(steps), 0.0);
    for (const double split : splits) {
        auto a = f35b_standing("full", 0.0, 0.0);
        Controls c;
        c.left_brake = c.right_brake = 1.0;
        c.throttle = 0.99; // her STOVL top, so the engine is the same either side of converting
        // Spooled up first, so that what she makes is the engine's steady thrust.
        for (int i = 0; i < 15 * 120; ++i) {
            a->set_lift_system(LiftSystem{});
            a->set_controls(c);
            a->step();
        }
        LiftSystem lift;
        lift.stovl = 1.0;
        lift.nozzle_deg = 90.0;
        lift.split = split;
        int converting = 0;
        int converted = 0;
        double engine_before = a->property("propulsion/engine/thrust-lbs") /
                               std::cos(a->property("propulsion/engine/reverser-angle-rad"));
        for (int i = 0; i < steps; ++i) {
            a->set_lift_system(lift);
            a->set_controls(c);
            a->step();
            const double s = a->property("fcs/stovl-pos-norm");
            converting += s > 0.0 && s < 1.0 ? 1 : 0;
            converted += s >= 1.0 ? 1 : 0;
            for (const std::string& p : thrusts) {
                check(std::isfinite(a->property(p)), p + " is not finite at split " + std::to_string(split) +
                                                         ", stovl-pos-norm " + std::to_string(s));
            }
            // Her lift system's thrust over the engine's own - the nozzle's
            // over the reverser's cosine - so that how fast she creeps on
            // her brakes, which moves the engine's thrust, is out of it: the
            // nozzle's over this step's, and the fan's and posts' over the
            // step before's, which they are reckoned from.
            const double engine = a->property("propulsion/engine/thrust-lbs") /
                                  std::cos(a->property("propulsion/engine/reverser-angle-rad"));
            const double total = a->property("propulsion/engine/thrust-lbs") / engine +
                                 (a->property("external_reactions/lift-fan/magnitude") +
                                  a->property("external_reactions/left-roll-post/magnitude") +
                                  a->property("external_reactions/right-roll-post/magnitude")) /
                                     engine_before;
            engine_before = engine;
            const auto at = static_cast<std::size_t>(i);
            if (split == 0.0) {
                middle[at] = total;
            } else {
                check(std::abs(total - middle[at]) <= 1e-9 * middle[at],
                      "at split " + std::to_string(split) + ", stovl-pos-norm " + std::to_string(s) +
                          " her thrust is " + std::to_string(total) + " of the engine's against " +
                          std::to_string(middle[at]) + " at the middle");
            }
        }
        // The whole conversion was walked: 8 s of it between 0 and 1, and
        // converted after.
        check(converting >= 8 * 120 - 2 && converted > 0,
              "the conversion walked at split " + std::to_string(split) + ": " + std::to_string(converting) +
                  " steps converting, " + std::to_string(converted) + " converted");
    }
}

// **Where the hover control law holds her**: lifted from the runway to 150 ft
// in a steady wind, settled 30 s and held a minute, in 0, 5, 10, 15 and 20
// kt from ahead, her right, behind and her left, at her vertical-landing
// weight and at her combat weight. Held: her height within 10 ft of 150 and
// her point within 30 ft, for the minute. The space: 2 weights x (calm + 4
// winds x 4 sides) = 34 hovers, every one flown.
GLIDESLOPE_TEST(the_f35b_hovers_in_every_wind_to_20_kt_from_every_side_at_two_weights) {
    using namespace glideslope::sim;
    const std::vector<std::string> loadings = {"vertical_landing", "combat"};
    const std::vector<double> winds = {0.0, 5.0, 10.0, 15.0, 20.0};
    const std::vector<double> sides = {0.0, 90.0, 180.0, 270.0};
    const int per_second = 120;
    std::size_t flown = 0;
    std::vector<std::string> not_held;
    for (const std::string& loading : loadings) {
        for (const double kt : winds) {
            for (const double side : sides) {
                if (kt == 0.0 && side != 0.0) {
                    continue; // calm has no side
                }
                ++flown;
                auto a = f35b_standing(loading, kt, side);
                Hover hover(*a);
                double worst_ft = 0.0;
                double drift_ft = 0.0;
                bool settled = false;
                int settled_steps = 0;
                int held_steps = 0;
                for (int i = 0; i < 300 * per_second && held_steps < 60 * per_second; ++i) {
                    const Hover::Command c = hover.step({150.0, false, 0.0, 0.0, 0.0});
                    a->set_lift_system(c.lift);
                    a->set_controls(c.controls);
                    a->step();
                    if (!settled) {
                        settled_steps = std::abs(hover.height_ft() - 150.0) < 2.0 ? settled_steps + 1 : 0;
                        settled = settled_steps == 30 * per_second;
                        continue;
                    }
                    ++held_steps;
                    worst_ft = std::max(worst_ft, std::abs(hover.height_ft() - 150.0));
                    drift_ft = std::max(drift_ft, hover.drift_ft());
                }
                const bool held =
                    settled && held_steps == 60 * per_second && worst_ft <= 10.0 && drift_ft <= 30.0;
                std::printf("  %-17s %4.0f kt from %3.0f: %-13s height %.2f ft, drift %.1f ft\n",
                            loading.c_str(), kt, side, held ? "held" : (settled ? "NOT HELD" : "never settled"),
                            worst_ft, drift_ft);
                if (!held) {
                    not_held.push_back(loading + " " + std::to_string(kt) + " kt from " + std::to_string(side));
                }
            }
        }
    }
    check(flown == 34, "34 hovers flown, not " + std::to_string(flown));
    std::string all;
    for (const std::string& n : not_held) {
        all += "\n    " + n;
    }
    check(not_held.empty(), std::to_string(not_held.size()) + " not held:" + all);
}

namespace {

// **Her transitions asked to change their minds** (sim/transition.hpp), each
// flown on one aircraft at her vertical-landing loading over flat ground in
// calm air, watched every step: the most she sinks below and rises above the
// height asked, her pitch and bank, and whether her wheels touch.
struct TransitionFlight {
    std::unique_ptr<glideslope::sim::Aircraft> a;
    std::unique_ptr<glideslope::sim::Transition> t;
    double height_ft = 150.0;
    double most_lost_ft = 0.0;
    double most_risen_ft = 0.0;
    double most_pitch_deg = 0.0;
    double most_bank_deg = 0.0;
    bool touched = false;
    int steps = 0;

    double conversion() const {
        return a->property("fcs/stovl-pos-norm");
    }
    double kcas() const {
        return a->property("velocities/vc-kts");
    }
    double agl() const {
        return a->state().height_above_ground_ft;
    }
    glideslope::sim::Transition::Stage stage() const {
        return t->stage();
    }

    void step(const glideslope::sim::Transition::Target& target) {
        const glideslope::sim::Hover::Command c = t->step(target);
        a->set_lift_system(c.lift);
        a->set_controls(c.controls);
        a->step();
        ++steps;
        const glideslope::sim::AircraftState s = a->state();
        most_lost_ft = std::max(most_lost_ft, height_ft - s.height_above_ground_ft);
        most_risen_ft = std::max(most_risen_ft, s.height_above_ground_ft - height_ft);
        most_pitch_deg = std::max(most_pitch_deg, std::abs(s.pitch_deg));
        most_bank_deg = std::max(most_bank_deg, std::abs(s.roll_deg));
        touched = touched || a->property("gear/wow") > 0.5;
    }

    // Flies `target` until `done` holds, at most `seconds`; whether it did.
    template <typename Done>
    bool until(const glideslope::sim::Transition::Target& target, double seconds,
               Done done) {
        for (int i = 0; i < static_cast<int>(seconds * 120.0); ++i) {
            step(target);
            if (done()) {
                return true;
            }
        }
        return false;
    }

    void fly(const glideslope::sim::Transition::Target& target, double seconds) {
        for (int i = 0; i < static_cast<int>(seconds * 120.0); ++i) {
            step(target);
        }
    }

    // Her attitudes within 15 degrees of pitch and 10 of bank, her wheels off
    // the ground, and she no more than 10 ft below the height asked.
    void within_bounds(const std::string& what) const {
        check(!touched, what + ": her wheels touched");
        check(most_pitch_deg <= 15.0,
              what + ": her pitch reached " + std::to_string(most_pitch_deg));
        check(most_bank_deg <= 10.0,
              what + ": her bank reached " + std::to_string(most_bank_deg));
        check(most_lost_ft <= 10.0,
              what + ": she sank " + std::to_string(most_lost_ft) + " ft");
        std::printf("  %s: lost %.2f ft, rose %.2f ft, pitch %.2f, bank %.2f, %.1f s\n",
                    what.c_str(), most_lost_ft, most_risen_ft, most_pitch_deg,
                    most_bank_deg, steps / 120.0);
    }
};

// Level at `height_ft` over flat ground at `kcas`, trimmed, the transition
// law taking her as she is.
TransitionFlight f35b_level_at(double height_ft, double kcas) {
    using namespace glideslope::sim;
    const PublishedFigures figures = read_published_figures(figures_file("f35b"));
    TransitionFlight f;
    f.height_ft = height_ft;
    f.a = std::make_unique<Aircraft>(data_dir, "f35b");
    f.a->load(figures.loadings.at("vertical_landing").loading);
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 70.0;
    ic.altitude_ft = height_ft;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = kcas;
    ic.gear = 0.0;
    ic.trim = true;
    f.a->initialize(ic);
    f.a->freeze_fuel(true);
    Hover::Command flying;
    flying.controls = trimmed_controls(*f.a);
    f.t = std::make_unique<Transition>(*f.a, flying);
    return f;
}

// Standing on flat ground, lifted by the transition law's hover to
// `height_ft` and settled there 30 s.
TransitionFlight f35b_hovering_at(double height_ft) {
    using namespace glideslope::sim;
    TransitionFlight f;
    f.height_ft = height_ft;
    f.a = f35b_standing("vertical_landing", 0.0, 0.0);
    f.a->freeze_fuel(true);
    f.t = std::make_unique<Transition>(*f.a, Hover::Command{});
    int settled = 0;
    const bool up = f.until({true, height_ft, 200.0, false, {}, {}}, 180.0, [&] {
        settled = std::abs(f.agl() - height_ft) < 2.0 ? settled + 1 : 0;
        return settled == 30 * 120;
    });
    check(up, "not hovering at " + std::to_string(height_ft) +
                  " ft three minutes after lifting");
    f.most_lost_ft = f.most_risen_ft = f.most_pitch_deg = f.most_bank_deg = 0.0;
    f.touched = false;
    f.steps = 0;
    return f;
}

} // namespace

// **Asked back to wing-borne flight half-way through converting to a
// hover**, she closes the fan's doors from where they are - they open no
// further - and flies on wing-borne at 200 KCAS: converted back within 10 s
// of the ask, and a minute later within 5 kt of 200 and 10 ft of 150.
GLIDESLOPE_TEST(
    the_f35b_asked_back_to_wing_borne_half_way_through_converting_closes_the_fan_and_flies_on_at_200_kt) {
    using namespace glideslope::sim;
    TransitionFlight f = f35b_level_at(150.0, 200.0);
    check(f.until({true, 150.0, 200.0, false, {}, {}}, 60.0, [&] { return f.conversion() >= 0.5; }),
          "not half converted a minute after a hover was asked");
    const double at_ask = f.conversion();
    double most_open = at_ask;
    const bool back = f.until({false, 150.0, 200.0, false, {}, {}}, 10.0, [&] {
        most_open = std::max(most_open, f.conversion());
        return f.stage() == Transition::Stage::wing_borne && f.conversion() <= 0.0;
    });
    check(most_open <= at_ask + 0.01, "the fan's doors opened on to " +
                                          std::to_string(most_open) + " from " +
                                          std::to_string(at_ask));
    check(back, "not converted back 10 s after the ask: stage " +
                    std::to_string(static_cast<int>(f.stage())) + ", conversion " +
                    std::to_string(f.conversion()));
    f.fly({false, 150.0, 200.0, false, {}, {}}, 60.0);
    check(f.stage() == Transition::Stage::wing_borne, "not wing-borne a minute on");
    check(std::abs(f.kcas() - 200.0) <= 5.0,
          "at " + std::to_string(f.kcas()) + " KCAS a minute on");
    check(std::abs(f.agl() - 150.0) <= 10.0,
          "at " + std::to_string(f.agl()) + " ft a minute on");
    f.within_bounds("asked back half converted");
}

// **Asked for a hover again half-way through converting back**, she opens
// the fan's doors again, slows and hovers at 150 ft: hovering within four
// minutes of the second ask, held 30 s.
GLIDESLOPE_TEST(
    the_f35b_asked_for_a_hover_again_half_way_through_converting_back_hovers_at_150_ft) {
    using namespace glideslope::sim;
    TransitionFlight f = f35b_hovering_at(150.0);
    check(f.until({false, 150.0, 200.0, false, {}, {}}, 180.0,
                  [&] {
                      return f.stage() == Transition::Stage::converting &&
                             f.conversion() <= 0.5;
                  }),
          "not half converted back three minutes after wing-borne flight was asked");
    const bool hovering = f.until({true, 150.0, 200.0, false, {}, {}}, 240.0, [&] {
        return f.stage() == Transition::Stage::hovering;
    });
    check(hovering,
          "not hovering four minutes after the hover was asked again: stage " +
              std::to_string(static_cast<int>(f.stage())));
    f.fly({true, 150.0, 200.0, false, {}, {}}, 30.0);
    check(f.stage() == Transition::Stage::hovering, "not hovering 30 s on");
    check(std::abs(f.agl() - 150.0) <= 10.0,
          "at " + std::to_string(f.agl()) + " ft 30 s on");
    f.within_bounds("hover asked again converting back");
}

// **Asked for wing-borne flight slower than her conversion speed**, at 150
// KCAS, she gathers speed to her conversion speed, 200 KCAS, converts back
// there and is held there: the slower part of the ask is refused
// (sim/transition.hpp says why). The conversion back begun no slower than
// 199 KCAS, and two minutes after it she is wing-borne within 5 kt of 200
// and 10 ft of 150 ft.
GLIDESLOPE_TEST(
    the_f35b_asked_for_wing_borne_flight_at_150_kt_converts_back_at_200_and_is_held_there) {
    using namespace glideslope::sim;
    TransitionFlight f = f35b_hovering_at(150.0);
    double began_kcas = 0.0;
    check(f.until({false, 150.0, 150.0, false, {}, {}}, 180.0,
                  [&] {
                      began_kcas = f.kcas();
                      return f.conversion() < 1.0;
                  }),
          "not converting back three minutes after wing-borne flight at 150 KCAS was "
          "asked");
    check(began_kcas >= 199.0,
          "converting back begun at " + std::to_string(began_kcas) + " KCAS");
    f.fly({false, 150.0, 150.0, false, {}, {}}, 120.0);
    check(f.stage() == Transition::Stage::wing_borne && f.conversion() <= 0.0,
          "not wing-borne two minutes on");
    check(std::abs(f.kcas() - 200.0) <= 5.0,
          "at " + std::to_string(f.kcas()) + " KCAS two minutes on");
    check(std::abs(f.agl() - 150.0) <= 10.0,
          "at " + std::to_string(f.agl()) + " ft two minutes on");
    f.within_bounds("wing-borne asked at 150 KCAS");
}

// **She converts no lower than her model's least conversion height**
// (`fcs/stovl-convert-least-ft`, 100 ft; tools/make_f35b.py), either way.
// Asked from 200 KCAS at 50 ft for a hover at 50 ft, she climbs to it before
// the fan's doors open, then lets down to 50 ft jet-borne and hovers there;
// asked from that hover for wing-borne flight at 50 ft, she climbs to it
// before they close, and lets down wing-borne after.
GLIDESLOPE_TEST(the_f35b_converts_no_lower_than_100_ft_either_way_when_asked_at_50_ft) {
    using namespace glideslope::sim;
    TransitionFlight f = f35b_level_at(50.0, 200.0);
    const double least = f.a->property("fcs/stovl-convert-least-ft");
    check(least == 100.0,
          "her least conversion height is " + std::to_string(least) + " ft, not 100");
    double lowest_open = 1e9;
    const bool hovering = f.until({true, 50.0, 200.0, false, {}, {}}, 300.0, [&] {
        if (f.conversion() > 0.0 && f.conversion() < 1.0) {
            lowest_open = std::min(lowest_open, f.agl());
        }
        return f.stage() == Transition::Stage::hovering;
    });
    check(hovering, "not hovering five minutes after a hover at 50 ft was asked");
    check(lowest_open >= least - 2.0,
          "the fan's doors opened at " + std::to_string(lowest_open) + " ft");
    f.fly({true, 50.0, 200.0, false, {}, {}}, 60.0);
    check(std::abs(f.agl() - 50.0) <= 5.0,
          "hovering at " + std::to_string(f.agl()) + " ft, not 50");
    double lowest_closing = 1e9;
    const bool back = f.until({false, 50.0, 200.0, false, {}, {}}, 300.0, [&] {
        if (f.conversion() > 0.0 && f.conversion() < 1.0) {
            lowest_closing = std::min(lowest_closing, f.agl());
        }
        return f.stage() == Transition::Stage::wing_borne && f.conversion() <= 0.0;
    });
    check(back,
          "not wing-borne five minutes after wing-borne flight at 50 ft was asked");
    check(lowest_closing >= least - 2.0,
          "the fan's doors closed at " + std::to_string(lowest_closing) + " ft");
    f.fly({false, 50.0, 200.0, false, {}, {}}, 120.0);
    check(std::abs(f.agl() - 50.0) <= 10.0,
          "at " + std::to_string(f.agl()) + " ft two minutes on, not 50");
    f.within_bounds("asked at 50 ft");
}

namespace {

// Feet between where she is and a point, on a sphere.
double feet_from(const glideslope::sim::Aircraft& a, double lat_deg, double lon_deg) {
    const glideslope::sim::AircraftState s = a.state();
    return glideslope::sim::distance_m(s.latitude_deg, s.longitude_deg, lat_deg, lon_deg) /
           0.3048;
}

} // namespace

// **An AI plan lands her vertically on a point** (FlightPlan::vertical_landing,
// sim::Controller): begun level at 1,000 ft and 250 KCAS over flat ground in
// calm air, at her vertical-landing loading, a plan of one waypoint and then
// `land_vertically` on a point 16 km beyond it, hovering there 30 s. She is
// flown there, converts, stops over it, hovers, and lets down: her wheels
// touch only once the hover's 30 s are flown, within 30 ft of the point and
// under her hover law's 6 ft/s; and every step her attitudes are within 15
// degrees of pitch and 10 of bank.
GLIDESLOPE_TEST(an_ai_plan_flies_the_f35b_to_a_point_converts_hovers_and_lands_vertically_on_it) {
    using namespace glideslope::sim;
    TransitionFlight f = f35b_level_at(1000.0, 250.0);
    const FlightPlan plan = parse_flight_plan(
        "aircraft f35b\n"
        "start -33.9461 151.1772 1000 70 250\n"
        "waypoint EAST -33.9122 151.2895 1000 250\n"
        "land_vertically PAD -33.8629 151.4527 30\n");
    check(plan.vertical_landing && plan.vertical_landing->hover_s == 30.0,
          "the plan lands vertically after 30 s of hover");
    const double pad_lat = plan.vertical_landing->latitude_deg;
    const double pad_lon = plan.vertical_landing->longitude_deg;
    Controller controller(*f.a, trimmed_controls(*f.a));
    controller.to_ai(plan);
    std::optional<double> hovering_from_s;
    std::optional<double> touched_s;
    double touch_sink_fps = 0.0;
    double touch_from_pad_ft = 0.0;
    double most_pitch = 0.0;
    double most_bank = 0.0;
    double nearest_hover_ft = 1e9;
    for (int i = 0; i < 20 * 60 * 120 && !touched_s; ++i) {
        apply(*f.a, controller);
        f.a->step();
        const AircraftState s = f.a->state();
        most_pitch = std::max(most_pitch, std::abs(s.pitch_deg));
        most_bank = std::max(most_bank, std::abs(s.roll_deg));
        const Transition* t = controller.transition();
        if (t && t->stage() == Transition::Stage::hovering && !hovering_from_s) {
            hovering_from_s = s.sim_time_s;
        }
        if (hovering_from_s) {
            nearest_hover_ft = std::min(nearest_hover_ft, feet_from(*f.a, pad_lat, pad_lon));
        }
        if (f.a->property("gear/wow") > 0.5) {
            touched_s = s.sim_time_s;
            touch_sink_fps = -s.climb_rate_fpm / 60.0;
            touch_from_pad_ft = feet_from(*f.a, pad_lat, pad_lon);
        }
    }
    check(hovering_from_s.has_value(), "she never came to a hover");
    check(touched_s.has_value(), "she had not landed twenty minutes on");
    std::printf("  hovering from %.1f s, touched at %.1f s, %.1f ft from the pad at %.2f "
                "ft/s; pitch %.1f, bank %.1f\n",
                *hovering_from_s, *touched_s, touch_from_pad_ft, touch_sink_fps, most_pitch,
                most_bank);
    check(*touched_s - *hovering_from_s >= 30.0,
          "her wheels touched " + std::to_string(*touched_s - *hovering_from_s) +
              " s into the hover, before its 30 s");
    check(touch_from_pad_ft <= 30.0,
          "she touched " + std::to_string(touch_from_pad_ft) + " ft from the pad");
    check(touch_sink_fps < 6.0,
          "she touched at " + std::to_string(touch_sink_fps) + " ft/s");
    check(most_pitch <= 15.0, "her pitch reached " + std::to_string(most_pitch));
    check(most_bank <= 10.0, "her bank reached " + std::to_string(most_bank));
    // **And stays down**: a minute on she is still on her wheels, and still.
    for (int i = 0; i < 60 * 120; ++i) {
        apply(*f.a, controller);
        f.a->step();
    }
    check(f.a->property("gear/wow") > 0.5 &&
              std::abs(f.a->property("velocities/vg-fps")) < 1.0,
          "a minute on she is not standing on the pad");
}

// **A pilot converts her with the STOVL mode selector** (Controls::stovl,
// sim::Controller's "unified control"): begun level at 300 ft and 200 KCAS
// over flat ground in calm air, the selector to STOVL brings her to a hover
// at the height she had; the stick full forward lets her down onto the
// ground under 6 ft/s; full back lifts her off again to 150 ft; and the
// selector back to conventional flight converts her back to wing-borne
// flight, where the controls are the pilot's again. Her wheels touch only
// on the landing, and her attitudes stay within 15 degrees of pitch and 10
// of bank throughout.
GLIDESLOPE_TEST(a_pilots_stovl_selector_and_stick_fly_the_f35b_to_a_hover_onto_the_ground_up_and_back_to_wing_borne) {
    using namespace glideslope::sim;
    TransitionFlight f = f35b_level_at(300.0, 200.0);
    Controls pilot = trimmed_controls(*f.a);
    Controller controller(*f.a, pilot);
    double most_pitch = 0.0;
    double most_bank = 0.0;
    int touches = 0;
    bool was_down = false;
    const auto fly = [&](double seconds, auto done) {
        for (int i = 0; i < static_cast<int>(seconds * 120.0); ++i) {
            controller.set_pilot(pilot);
            apply(*f.a, controller);
            f.a->step();
            const AircraftState s = f.a->state();
            most_pitch = std::max(most_pitch, std::abs(s.pitch_deg));
            most_bank = std::max(most_bank, std::abs(s.roll_deg));
            const bool down = f.a->property("gear/wow") > 0.5;
            touches += down && !was_down ? 1 : 0;
            was_down = down;
            if (done()) {
                return true;
            }
        }
        return false;
    };
    pilot.stovl = 1.0;
    const bool hovering = fly(600.0, [&] {
        const Transition* t = controller.transition();
        return t && t->stage() == Transition::Stage::hovering;
    });
    check(hovering, "not hovering ten minutes after STOVL mode was selected");
    fly(30.0, [] { return false; });
    const double hover_agl = f.agl();
    check(std::abs(hover_agl - 300.0) <= 10.0,
          "hovering at " + std::to_string(hover_agl) + " ft, not the 300 she had");
    pilot.elevator = -1.0;
    double touch_sink_fps = 0.0;
    const bool landed = fly(300.0, [&] {
        touch_sink_fps = -f.a->state().climb_rate_fpm / 60.0;
        return f.a->property("gear/wow") > 0.5;
    });
    check(landed, "not down five minutes after the stick went forward");
    check(touch_sink_fps < 6.0, "she touched at " + std::to_string(touch_sink_fps) + " ft/s");
    pilot.elevator = 0.0;
    fly(10.0, [] { return false; });
    check(f.a->property("gear/wow") > 0.5, "she did not stay down with the stick centred");
    pilot.elevator = 1.0;
    const bool up = fly(120.0, [&] { return controller.stovl_height_asked_ft() >= 150.0; });
    check(up, "the stick back did not ask her up to 150 ft");
    pilot.elevator = 0.0;
    fly(60.0, [] { return false; });
    check(std::abs(f.agl() - controller.stovl_height_asked_ft()) <= 10.0,
          "hovering at " + std::to_string(f.agl()) + " ft, not the " +
              std::to_string(controller.stovl_height_asked_ft()) + " asked");
    pilot.stovl = 0.0;
    double kcas_released = 0.0;
    const bool back = fly(300.0, [&] {
        kcas_released = f.kcas();
        return controller.transition() == nullptr;
    });
    check(back, "not wing-borne five minutes after conventional flight was selected");
    check(f.conversion() <= 0.0 && kcas_released >= 190.0,
          "handed back at " + std::to_string(kcas_released) + " KCAS, converted " +
              std::to_string(f.conversion()));
    fly(5.0, [] { return false; });
    check(!controller.catching_up(), "the controls were not the pilot's five seconds on");
    std::printf("  hovered at %.1f ft, touched at %.2f ft/s, handed back at %.1f KCAS; "
                "pitch %.1f, bank %.1f, %d touch\n",
                hover_agl, touch_sink_fps, kcas_released, most_pitch, most_bank, touches);
    check(touches == 1, std::to_string(touches) + " touches, not the one landing");
    check(most_pitch <= 15.0, "her pitch reached " + std::to_string(most_pitch));
    check(most_bank <= 10.0, "her bank reached " + std::to_string(most_bank));
}

namespace {

constexpr double test_pi = 3.14159265358979323846;

// Her ground speed along `heading_deg` and across it to the right, knots.
std::pair<double, double> ahead_right_kt(const glideslope::sim::Aircraft& a, double heading_deg) {
    const double psi = heading_deg * test_pi / 180.0;
    const double n = a.property("velocities/v-north-fps");
    const double e = a.property("velocities/v-east-fps");
    const double kt = 3600.0 / 6076.12;
    return {(n * std::cos(psi) + e * std::sin(psi)) * kt,
            (-n * std::sin(psi) + e * std::cos(psi)) * kt};
}

// A pilot's F-35B (begun as `f`), her selector to STOVL and flown to a hover,
// settled 30 s: the controller, the pilot's controls, and what is watched.
struct PilotsHover {
    glideslope::sim::Controls pilot;
    std::unique_ptr<glideslope::sim::Controller> controller;
    double most_pitch = 0.0;
    double most_bank = 0.0;
    double most_off_ft = 0.0;
    bool touched = false;

    // Flies `seconds` with the pilot's controls; `ai` flies without them.
    void fly(TransitionFlight& f, double seconds, bool ai = false) {
        for (int i = 0; i < static_cast<int>(seconds * 120.0 + 0.5); ++i) {
            if (!ai) {
                controller->set_pilot(pilot);
            }
            glideslope::sim::apply(*f.a, *controller);
            f.a->step();
            const glideslope::sim::AircraftState s = f.a->state();
            most_pitch = std::max(most_pitch, std::abs(s.pitch_deg));
            most_bank = std::max(most_bank, std::abs(s.roll_deg));
            if (!ai) {
                most_off_ft =
                    std::max(most_off_ft, std::abs(f.agl() - controller->stovl_height_asked_ft()));
            }
            touched = touched || f.a->property("gear/wow") > 0.5;
        }
    }
};

PilotsHover pilots_hover(TransitionFlight& f) {
    using namespace glideslope::sim;
    PilotsHover h;
    h.pilot = trimmed_controls(*f.a);
    h.controller = std::make_unique<Controller>(*f.a, h.pilot);
    h.pilot.stovl = 1.0;
    bool hovering = false;
    for (int i = 0; i < 600 * 120 && !hovering; ++i) {
        h.fly(f, 1.0 / 120.0);
        const Transition* t = h.controller->transition();
        hovering = t && t->stage() == Transition::Stage::hovering;
    }
    check(hovering, "not hovering ten minutes after STOVL mode was selected");
    h.fly(f, 30.0);
    h.most_pitch = h.most_bank = h.most_off_ft = 0.0;
    return h;
}

void near(double got, double wanted, double within, const std::string& what) {
    std::printf("  %s: %.2f (asked %.1f)\n", what.c_str(), got, wanted);
    check(std::abs(got - wanted) <= within,
          what + " " + std::to_string(got) + ", not within " + std::to_string(within) + " of " +
              std::to_string(wanted));
}

} // namespace

// **In a hover her pilot moves her with the lever, the stick and the
// pedals** (Transition::Inceptors, her unified control laws' translational
// rate command): hovering at 300 ft on the selector, begun level at 200
// KCAS, every direction the table has is flown - six, asserted:
//
//   forward   the lever half way from its detent to its forward stop, 40 s:
//             15 kt ahead within 2, under 3 across;
//   aft       the lever at its aft stop, 40 s: 30 kt back within 2, her
//             published limit (tools/make_f35b.py), flown in air met from
//             behind, her angle of attack either side of 180 degrees;
//   right     the stick full right, 40 s: 20 kt right within 2, under 3
//             ahead;
//   left      the stick full left, 40 s: 20 kt left within 2;
//   turn right, turn left   each pedal full, 18 s: 10 degrees a second
//             within 1 over the last 10, her place held within 50 ft.
//
// Speeds are averaged over each phase's last 12 s, two of her hover's
// swings, and swing no more than 6 kt. Between them the lever at its detent
// or the stick centred, 30 s, stops her within 1 kt; flown sideways her
// heading moves under 3 degrees; a pedal released, she holds the heading
// she was left on within 3. Her height is held within 10 ft of what is
// asked, her pitch within 15 degrees and her bank within 10 throughout,
// her wheels never touching. Measured: pitch 7.7, bank 7.7, speeds swung
// 2.3 kt; with her Cm-alpha jumping at 180 degrees (tools/make_f35b.py) she
// swung 7.3 kt and 19 degrees in pitch flown back at 30.
GLIDESLOPE_TEST(in_a_hover_the_f35bs_pilot_moves_her_forward_back_sideways_and_turns_her_with_the_lever_stick_and_pedals) {
    using namespace glideslope::sim;
    TransitionFlight f = f35b_level_at(300.0, 200.0);
    PilotsHover h = pilots_hover(f);
    const double detent = f.a->property("fcs/stovl-lever-detent");
    const std::set<std::string> table{"forward", "aft", "right", "left", "turn right",
                                      "turn left"};
    std::set<std::string> covered;
    double most_swing_kt = 0.0;
    const auto heading = [&] { return h.controller->transition()->hover()->heading_deg(); };
    // Holds `lever`, `side` and `pedals` for `seconds`; her speed averaged
    // over its last 12 s along and across the heading held.
    const auto hold = [&](double lever, double side, double pedals, double seconds) {
        h.pilot.throttle = lever;
        h.pilot.aileron = side;
        h.pilot.rudder = pedals;
        h.fly(f, seconds - 12.0);
        double sum_ahead = 0.0;
        double sum_right = 0.0;
        double low = 1e9;
        double high = -1e9;
        for (int i = 0; i < 12 * 120; ++i) {
            h.fly(f, 1.0 / 120.0);
            const auto [a1, r1] = ahead_right_kt(*f.a, heading());
            sum_ahead += a1;
            sum_right += r1;
            const double along = side != 0.0 ? r1 : a1;
            low = std::min(low, along);
            high = std::max(high, along);
        }
        most_swing_kt = std::max(most_swing_kt, high - low);
        check(h.controller->transition() && h.controller->transition()->hover(),
              "she left the hover");
        return std::pair{sum_ahead / (12.0 * 120.0), sum_right / (12.0 * 120.0)};
    };
    const auto stops = [&](const std::string& what) {
        const auto [a0, r0] = hold(detent, 0.0, 0.0, 30.0);
        near(std::hypot(a0, r0), 0.0, 1.0, what + ", kt");
    };
    auto [ahead, right] = hold(detent + 0.5 * (1.0 - detent), 0.0, 0.0, 40.0);
    near(ahead, 15.0, 2.0, "lever half forward, kt ahead");
    near(right, 0.0, 3.0, "lever half forward, kt across");
    covered.insert("forward");
    stops("lever at its detent");
    std::tie(ahead, right) = hold(0.0, 0.0, 0.0, 40.0);
    near(ahead, -30.0, 2.0, "lever full aft, kt ahead");
    near(right, 0.0, 3.0, "lever full aft, kt across");
    covered.insert("aft");
    stops("lever at its detent again");
    for (const double side : {1.0, -1.0}) {
        const std::string name = side > 0.0 ? "right" : "left";
        const double heading_before = f.a->state().heading_deg;
        std::tie(ahead, right) = hold(detent, side, 0.0, 40.0);
        near(right, 20.0 * side, 2.0, "stick full " + name + ", kt to the right");
        near(ahead, 0.0, 3.0, "stick full " + name + ", kt ahead");
        stops("stick centred from the " + name);
        near(std::remainder(f.a->state().heading_deg - heading_before, 360.0), 0.0, 3.0,
             "flown " + name + ", degrees her heading moved");
        covered.insert(name);
    }
    for (const double pedal : {1.0, -1.0}) {
        const std::string name = pedal > 0.0 ? "turn right" : "turn left";
        const AircraftState before_turn = f.a->state();
        h.pilot.rudder = pedal;
        h.fly(f, 8.0);
        const double from_deg = f.a->state().heading_deg;
        h.fly(f, 10.0);
        near(std::remainder(f.a->state().heading_deg - from_deg, 360.0) / 10.0, 10.0 * pedal,
             1.0, name + ", degrees a second");
        near(distance_m(before_turn.latitude_deg, before_turn.longitude_deg,
                        f.a->state().latitude_deg, f.a->state().longitude_deg) /
                 0.3048,
             0.0, 50.0, name + ", ft from where she began it");
        hold(detent, 0.0, 0.0, 20.0);
        near(std::remainder(f.a->state().heading_deg - heading(), 360.0), 0.0, 3.0,
             name + " released, degrees off the heading she was left on");
        covered.insert(name);
    }
    std::printf("  covered %zu of the table's %zu; height off what was asked by at most %.2f "
                "ft; pitch %.1f, bank %.1f; speeds swung %.2f kt at most\n",
                covered.size(), table.size(), h.most_off_ft, h.most_pitch, h.most_bank,
                most_swing_kt);
    check(covered == table, std::to_string(covered.size()) + " of the table's " +
                                std::to_string(table.size()) + " directions flown");
    check(most_swing_kt <= 6.0, "her speed swung " + std::to_string(most_swing_kt) + " kt");
    check(h.most_off_ft <= 10.0, "her height strayed " + std::to_string(h.most_off_ft) + " ft");
    check(h.most_pitch <= 15.0, "her pitch reached " + std::to_string(h.most_pitch));
    check(h.most_bank <= 10.0, "her bank reached " + std::to_string(h.most_bank));
    check(!h.touched, "her wheels touched");
}

// **Her lever is the pilot's only once moved past its dead band from where
// it was at each selection** (Controller::lever_release, 0.05): begun level
// at 300 ft and 200 KCAS, three cases, each counted:
//
//   at full     selected with the lever at its forward stop, nudged 0.001
//               back 10 s on: not sent gathering speed, she hovers within
//               ten minutes;
//   at idle     selected with the lever at its aft stop, nudged 0.001
//               forward 10 s on: not sent 30 kt back, she hovers within ten
//               minutes and, a minute on, has not moved faster than 1 kt;
//   toggled     selected with the lever where it was, moved to its forward
//               stop a second on (the pilot's from there), and the selector
//               thrown back to conventional flight the step the fan's doors
//               are half open and to STOVL mode a second later: latched
//               again at the forward stop there, she hovers within ten
//               minutes.
GLIDESLOPE_TEST(the_f35bs_lever_is_the_pilots_only_once_moved_past_its_dead_band_from_each_selection) {
    using namespace glideslope::sim;
    std::size_t cases = 0;
    const auto hovers_within = [](TransitionFlight& f, PilotsHover& h, double seconds) {
        for (int i = 0; i < static_cast<int>(seconds * 120.0); ++i) {
            h.fly(f, 1.0 / 120.0);
            const Transition* t = h.controller->transition();
            if (t && t->stage() == Transition::Stage::hovering) {
                return true;
            }
        }
        return false;
    };
    const auto begun = [](TransitionFlight& f, double lever) {
        PilotsHover h;
        h.pilot = trimmed_controls(*f.a);
        h.controller = std::make_unique<Controller>(*f.a, h.pilot);
        h.pilot.throttle = lever;
        h.pilot.stovl = 1.0;
        return h;
    };
    {
        TransitionFlight f = f35b_level_at(300.0, 200.0);
        PilotsHover h = begun(f, 1.0);
        h.fly(f, 10.0);
        h.pilot.throttle = 0.999;
        const bool hovering = hovers_within(f, h, 600.0);
        std::printf("  at full, nudged: %s\n", hovering ? "hovers" : "never hovers");
        check(hovering, "selected at full and nudged 0.001, she never hovered");
        ++cases;
    }
    {
        TransitionFlight f = f35b_level_at(300.0, 200.0);
        PilotsHover h = begun(f, 0.0);
        h.fly(f, 10.0);
        h.pilot.throttle = 0.001;
        const bool hovering = hovers_within(f, h, 600.0);
        double fastest_kt = 0.0;
        for (int i = 0; i < 60 * 120 && hovering; ++i) {
            h.fly(f, 1.0 / 120.0);
            fastest_kt = std::max(fastest_kt, std::hypot(f.a->property("velocities/v-north-fps"),
                                                         f.a->property("velocities/v-east-fps")) *
                                                  3600.0 / 6076.12);
        }
        std::printf("  at idle, nudged: %s, fastest a minute on %.2f kt\n",
                    hovering ? "hovers" : "never hovers", fastest_kt);
        check(hovering, "selected at idle and nudged 0.001, she never hovered");
        check(fastest_kt <= 1.0, "nudged at idle she moved at " + std::to_string(fastest_kt) +
                                     " kt in the hover");
        ++cases;
    }
    {
        TransitionFlight f = f35b_level_at(300.0, 200.0);
        PilotsHover h = begun(f, trimmed_controls(*f.a).throttle);
        h.fly(f, 1.0);
        h.pilot.throttle = 1.0;
        double half_open_at = -1.0;
        for (int i = 0; i < 300 * 120 && half_open_at < 0.0; ++i) {
            h.fly(f, 1.0 / 120.0);
            if (f.conversion() >= 0.5) {
                half_open_at = f.a->state().sim_time_s;
            }
        }
        check(half_open_at >= 0.0, "the fan's doors were never half open");
        h.pilot.stovl = 0.0;
        h.fly(f, 1.0);
        const double between = f.conversion();
        h.pilot.stovl = 1.0;
        const bool hovering = hovers_within(f, h, 600.0);
        std::printf("  toggled at %.1f s, the doors %.2f open a second on: %s\n", half_open_at,
                    between, hovering ? "hovers" : "never hovers");
        check(between > 0.0 && between < 1.0, "the selector was not thrown back mid-conversion");
        check(hovering, "selected again with the lever at its stop, she never hovered");
        ++cases;
    }
    check(cases == 3, std::to_string(cases) + " cases, not 3");
}

// **Jet-borne her pilot's lever is an acceleration**: hovering as above, the
// lever full forward takes her past her translational rate command's 30 kt
// into jet-borne flight, gathering speed to 100 kt along her heading within
// three minutes; the lever at its detent then holds her speed within 5 kt
// for 30 s; full aft slows her under 30 kt asked within three minutes; and
// back at its detent she comes to a hover within two. Her height within 15
// ft of what is asked, her attitudes within 15 degrees of pitch and 10 of
// bank, her wheels never touching.
GLIDESLOPE_TEST(jet_borne_the_f35bs_pilots_lever_gathers_holds_and_sheds_her_speed_and_brings_her_back_to_a_hover) {
    using namespace glideslope::sim;
    TransitionFlight f = f35b_level_at(300.0, 200.0);
    PilotsHover h = pilots_hover(f);
    const double detent = f.a->property("fcs/stovl-lever-detent");
    const double heading = h.controller->transition()->hover()->heading_deg();
    const auto speed = [&] { return ahead_right_kt(*f.a, heading).first; };
    h.pilot.throttle = 1.0;
    bool jet_borne = false;
    double reached_s = -1.0;
    for (int i = 0; i < 180 * 120 && reached_s < 0.0; ++i) {
        h.fly(f, 1.0 / 120.0);
        jet_borne = jet_borne ||
                    h.controller->transition()->stage() == Transition::Stage::jet_borne;
        if (speed() >= 100.0) {
            reached_s = i / 120.0;
        }
    }
    check(jet_borne, "she never left the hover for jet-borne flight");
    check(reached_s >= 0.0, "not at 100 kt three minutes after the lever went forward");
    h.pilot.throttle = detent;
    h.fly(f, 1.0 / 120.0);
    const double held_from = speed();
    double most_off_kt = 0.0;
    for (int i = 0; i < 30 * 120; ++i) {
        h.fly(f, 1.0 / 120.0);
        most_off_kt = std::max(most_off_kt, std::abs(speed() - held_from));
    }
    std::printf("  at 100 kt in %.1f s; at the detent from %.1f kt, off it by at most %.2f kt\n",
                reached_s, held_from, most_off_kt);
    check(most_off_kt <= 5.0,
          "at the detent her speed moved " + std::to_string(most_off_kt) + " kt");
    h.pilot.throttle = 0.0;
    bool slow = false;
    for (int i = 0; i < 180 * 120 && !slow; ++i) {
        h.fly(f, 1.0 / 120.0);
        slow = h.controller->transition()->speed_asked_kt() < 30.0;
    }
    check(slow, "the lever aft did not slow her under 30 kt in three minutes");
    h.pilot.throttle = detent;
    bool hovering = false;
    for (int i = 0; i < 120 * 120 && !hovering; ++i) {
        h.fly(f, 1.0 / 120.0);
        hovering = h.controller->transition()->stage() == Transition::Stage::hovering;
    }
    std::printf("  height off what was asked by at most %.2f ft; pitch %.1f, bank %.1f\n",
                h.most_off_ft, h.most_pitch, h.most_bank);
    check(hovering, "not hovering two minutes after the lever came back to its detent");
    check(h.most_off_ft <= 15.0, "her height strayed " + std::to_string(h.most_off_ft) + " ft");
    check(h.most_pitch <= 15.0, "her pitch reached " + std::to_string(h.most_pitch));
    check(h.most_bank <= 10.0, "her bank reached " + std::to_string(h.most_bank));
    check(!h.touched, "her wheels touched");
}

namespace {

// The point `ahead_m` along her heading and `right_m` to its right.
std::pair<double, double> off_her_nose(const glideslope::sim::Aircraft& a, double ahead_m,
                                       double right_m) {
    const glideslope::sim::AircraftState s = a.state();
    const double psi = s.heading_deg * test_pi / 180.0;
    const double n = ahead_m * std::cos(psi) - right_m * std::sin(psi);
    const double e = ahead_m * std::sin(psi) + right_m * std::cos(psi);
    const double r = 6371000.0;
    return {s.latitude_deg + n / r * 180.0 / test_pi,
            s.longitude_deg + e / (r * std::cos(s.latitude_deg * test_pi / 180.0)) * 180.0 / test_pi};
}

std::string plan_line(const char* kind, const char* name, std::pair<double, double> at,
                      const char* rest) {
    char line[160];
    std::snprintf(line, sizeof line, "%s %s %.6f %.6f %s\n", kind, name, at.first, at.second,
                  rest);
    return line;
}

} // namespace

// **A plan given the AI while she hovers is flown**: hovering at 300 ft on
// her pilot's selector, as above, she is handed to the AI with a plan
// (Controller::to_ai):
//
//   waypoints      two waypoints 10 and 20 km on at 1,500 ft and 250 KCAS: she
//                  converts back to wing-borne flight, the transition law
//                  let go, and passes within 1,000 ft of each, the second
//                  within 15 minutes, her wheels never touching;
//   a far pad      a waypoint 10 km on and a vertical landing 16 km beyond
//                  it: she converts back, flies to it, converts again and
//                  lands on it within 30 ft, within 20 minutes;
//   a near pad     a waypoint where she hovers and a vertical landing 1,200
//                  ft ahead and 900 ft right, closer than she stops in: the
//                  waypoint, flown by with no orbit and within 2,000 ft, is
//                  counted passed; she is not converted back but moved to
//                  the pad in the hover, and lands on it within 30 ft,
//                  within 10 minutes;
//   a near fly-over  a waypoint to be flown over 1,000 ft ahead, then a
//                  vertical landing 1,500 ft ahead and 300 right: the
//                  fly-over is not counted passed - she is flown out of the
//                  hover, passes within 200 ft of it, and lands on the pad
//                  within 30 ft, within 15 minutes.
//
// Her attitudes within 15 degrees of pitch and 30 of bank (the plan's
// turns are the autopilot's). Four cases, each counted.
GLIDESLOPE_TEST(a_plan_given_the_ai_while_the_f35b_hovers_is_flown_converted_back_or_moved_in_the_hover) {
    using namespace glideslope::sim;
    std::size_t cases = 0;
    const auto attitudes = [](const PilotsHover& h, const std::string& which) {
        std::printf("  %s: pitch %.1f, bank %.1f\n", which.c_str(), h.most_pitch, h.most_bank);
        check(h.most_pitch <= 15.0 && h.most_bank <= 30.0,
              which + ": pitch " + std::to_string(h.most_pitch) + ", bank " +
                  std::to_string(h.most_bank));
    };
    {
        TransitionFlight f = f35b_level_at(300.0, 200.0);
        PilotsHover h = pilots_hover(f);
        const auto one = off_her_nose(*f.a, 10000.0, 0.0);
        const auto two = off_her_nose(*f.a, 20000.0, 0.0);
        h.controller->to_ai(parse_flight_plan("aircraft f35b\n" +
                                              plan_line("waypoint", "ONE", one, "1500 250") +
                                              plan_line("waypoint", "TWO", two, "1500 250")));
        double nearest_one = 1e9;
        double nearest_two = 1e9;
        bool released = false;
        for (int i = 0; i < 15 * 60 * 120 && nearest_two > 1000.0; ++i) {
            h.fly(f, 1.0 / 120.0, true);
            released = released || h.controller->transition() == nullptr;
            nearest_one = std::min(nearest_one, feet_from(*f.a, one.first, one.second));
            nearest_two = std::min(nearest_two, feet_from(*f.a, two.first, two.second));
        }
        std::printf("  waypoints: converted back %s, nearest %.0f ft and %.0f ft; %.1f KCAS, "
                    "%.0f ft\n",
                    released ? "yes" : "no", nearest_one, nearest_two, f.kcas(), f.agl());
        check(released && f.conversion() <= 0.0, "she was not converted back for the plan");
        check(nearest_one <= 1000.0 && nearest_two <= 1000.0, "she did not pass her waypoints");
        check(!h.touched, "her wheels touched flying the plan");
        attitudes(h, "waypoints");
        ++cases;
    }
    // Flown until her wheels touch, at most `seconds`: how far from `pad`
    // they touched, or none.
    const auto lands = [](TransitionFlight& f, PilotsHover& h, std::pair<double, double> pad,
                          double seconds, bool& released) {
        for (int i = 0; i < static_cast<int>(seconds * 120.0); ++i) {
            h.fly(f, 1.0 / 120.0, true);
            released = released || h.controller->transition() == nullptr;
            if (f.a->property("gear/wow") > 0.5) {
                return std::optional<double>(feet_from(*f.a, pad.first, pad.second));
            }
        }
        return std::optional<double>();
    };
    {
        TransitionFlight f = f35b_level_at(300.0, 200.0);
        PilotsHover h = pilots_hover(f);
        const auto one = off_her_nose(*f.a, 10000.0, 0.0);
        const auto pad = off_her_nose(*f.a, 26000.0, 0.0);
        h.controller->to_ai(parse_flight_plan("aircraft f35b\n" +
                                              plan_line("waypoint", "ONE", one, "1000 250") +
                                              plan_line("land_vertically", "PAD", pad, "10")));
        bool released = false;
        const std::optional<double> touched = lands(f, h, pad, 20.0 * 60.0, released);
        std::printf("  a far pad: converted back %s, touched %.1f ft from it\n",
                    released ? "yes" : "no", touched.value_or(-1.0));
        check(released, "she was not converted back to fly to the far pad");
        check(touched && *touched <= 30.0, "she did not land on the far pad");
        attitudes(h, "a far pad");
        ++cases;
    }
    {
        TransitionFlight f = f35b_level_at(300.0, 200.0);
        PilotsHover h = pilots_hover(f);
        const auto here = off_her_nose(*f.a, 0.0, 0.0);
        const auto pad = off_her_nose(*f.a, 1200.0 * 0.3048, 900.0 * 0.3048);
        h.controller->to_ai(parse_flight_plan("aircraft f35b\n" +
                                              plan_line("waypoint", "HERE", here, "300 200") +
                                              plan_line("land_vertically", "PAD", pad, "10")));
        bool released = false;
        const std::optional<double> touched = lands(f, h, pad, 10.0 * 60.0, released);
        std::printf("  a near pad: converted back %s, touched %.1f ft from it\n",
                    released ? "yes" : "no", touched.value_or(-1.0));
        check(!released, "she was converted back for a pad closer than she stops in");
        check(touched && *touched <= 30.0, "she did not land on the near pad");
        attitudes(h, "a near pad");
        ++cases;
    }
    {
        TransitionFlight f = f35b_level_at(300.0, 200.0);
        PilotsHover h = pilots_hover(f);
        const auto over = off_her_nose(*f.a, 1000.0 * 0.3048, 0.0);
        const auto pad = off_her_nose(*f.a, 1500.0 * 0.3048, 300.0 * 0.3048);
        h.controller->to_ai(parse_flight_plan(
            "aircraft f35b\n" + plan_line("waypoint", "OVER", over, "300 200 flyover") +
            plan_line("land_vertically", "PAD", pad, "10")));
        double nearest_over = 1e9;
        bool released = false;
        bool jet_borne = false;
        std::optional<double> touched;
        for (int i = 0; i < 15 * 60 * 120 && !touched; ++i) {
            h.fly(f, 1.0 / 120.0, true);
            const Transition* t = h.controller->transition();
            released = released || t == nullptr;
            jet_borne = jet_borne || (t && t->stage() == Transition::Stage::jet_borne);
            nearest_over = std::min(nearest_over, feet_from(*f.a, over.first, over.second));
            if (f.a->property("gear/wow") > 0.5) {
                touched = feet_from(*f.a, pad.first, pad.second);
            }
        }
        std::printf("  a near fly-over: jet-borne %s, converted back %s, passed %.0f ft from it, "
                    "touched %.1f ft from the pad\n",
                    jet_borne ? "yes" : "no", released ? "yes" : "no", nearest_over,
                    touched.value_or(-1.0));
        check(jet_borne, "she was not flown out of the hover for the fly-over waypoint");
        check(nearest_over <= 200.0,
              "she passed the fly-over waypoint " + std::to_string(nearest_over) + " ft off");
        check(touched && *touched <= 30.0, "she did not land on the pad after the fly-over");
        attitudes(h, "a near fly-over");
        ++cases;
    }
    check(cases == 4, std::to_string(cases) + " cases, not 4");
}

// **Taken back by her pilot in a plan's hover, she holds the hover's
// height**: the AI's vertical landing (as above) hovering 10 s over the pad
// at 150 ft, of the 30 it would hold, is handed to the pilot, whose selector
// is at conventional flight and stick centred. She stays in STOVL mode -
// the selector has not moved since the take-back - and holds the 150 ft the
// hover asked, within 10 ft, for a minute, her wheels off the ground; not
// the 1,000 ft she was at when the plan's transition began.
GLIDESLOPE_TEST(an_f35b_taken_back_by_her_pilot_in_a_plans_hover_holds_the_hovers_height) {
    using namespace glideslope::sim;
    TransitionFlight f = f35b_level_at(1000.0, 250.0);
    const FlightPlan plan = parse_flight_plan(
        "aircraft f35b\n"
        "start -33.9461 151.1772 1000 70 250\n"
        "waypoint EAST -33.9122 151.2895 1000 250\n"
        "land_vertically PAD -33.8629 151.4527 30\n");
    const Controls pilot = trimmed_controls(*f.a);
    Controller controller(*f.a, pilot);
    controller.to_ai(plan);
    std::optional<double> hovering_from_s;
    for (int i = 0; i < 20 * 60 * 120; ++i) {
        apply(*f.a, controller);
        f.a->step();
        const Transition* t = controller.transition();
        if (t && t->stage() == Transition::Stage::hovering && !hovering_from_s) {
            hovering_from_s = f.a->state().sim_time_s;
        }
        if (hovering_from_s && f.a->state().sim_time_s - *hovering_from_s >= 10.0) {
            break;
        }
    }
    check(hovering_from_s.has_value(), "she never came to the plan's hover");
    controller.to_pilot();
    double most_off_ft = 0.0;
    bool touched = false;
    for (int i = 0; i < 60 * 120; ++i) {
        controller.set_pilot(pilot);
        apply(*f.a, controller);
        f.a->step();
        most_off_ft = std::max(most_off_ft, std::abs(f.agl() - 150.0));
        touched = touched || f.a->property("gear/wow") > 0.5;
    }
    std::printf("  taken back at %.0f ft asked: off 150 ft by at most %.2f ft in a minute\n",
                controller.stovl_height_asked_ft(), most_off_ft);
    check(controller.transition() != nullptr, "she left STOVL mode with the selector unmoved");
    check(controller.stovl_height_asked_ft() == 150.0,
          "the height asked is " + std::to_string(controller.stovl_height_asked_ft()) +
              " ft, not the hover's 150");
    check(most_off_ft <= 10.0, "she strayed " + std::to_string(most_off_ft) + " ft from 150");
    check(!touched, "her wheels touched");
}

// **Her STOVL selector at its edges, each built and flown**, none throwing:
//
//   above 250 KCAS     selected level at 1,000 ft and 270 KCAS, past her
//                      STOVL clearance: slowed wing-borne first, converted,
//                      hovering within ten minutes;
//   below 100 ft       selected level at 60 ft and 200 KCAS: the fan's doors
//                      begin to open no lower than her least conversion
//                      height, less the law's 2 ft, and she hovers with her
//                      wheels off the ground;
//   on the ground      selected standing: converted where she stands, still
//                      down 30 s on, and lifted off by the stick;
//   toggled quickly    at 300 ft and 200 KCAS, the selector thrown every
//                      half second for twenty seconds, then left at STOVL:
//                      hovering within ten minutes, her wheels off the ground;
//   handed to the AI   hovering at 300 ft on the selector, handed to the AI:
//                      held within 10 ft of it for a minute;
//   refused above it   STOVL mode put on her at 300 KCAS (sim::put_lift_system):
//                      flown with the fan's doors closed, not thrown.
GLIDESLOPE_TEST(the_f35bs_stovl_selector_at_its_edges_is_flown_and_never_throws) {
    using namespace glideslope::sim;
    std::size_t cases = 0;
    std::vector<std::string> failed;
    const auto fail_if = [&](bool bad, const std::string& what) {
        if (bad) {
            failed.push_back(what);
        }
    };
    // Flies `pilot` (changed by `script` each step) through a controller for
    // up to `seconds`, until `done`; whether it was done.
    const auto fly = [](Aircraft& a, Controller& c, Controls& pilot, double seconds,
                        const std::function<void(int)>& script,
                        const std::function<bool()>& done) {
        for (int i = 0; i < static_cast<int>(seconds * 120.0); ++i) {
            script(i);
            c.set_pilot(pilot);
            apply(a, c);
            a.step();
            if (done()) {
                return true;
            }
        }
        return false;
    };
    const auto hovering = [](const Controller& c) {
        return c.transition() && c.transition()->stage() == Transition::Stage::hovering;
    };
    const auto none = [](int) {};
    try {
        {
            TransitionFlight f = f35b_level_at(1000.0, 270.0);
            Controls pilot = trimmed_controls(*f.a);
            Controller c(*f.a, pilot);
            pilot.stovl = 1.0;
            const bool ok = fly(*f.a, c, pilot, 600.0, none, [&] { return hovering(c); });
            fail_if(!ok, "selected above 250 KCAS: not hovering in ten minutes");
            ++cases;
        }
        {
            TransitionFlight f = f35b_level_at(60.0, 200.0);
            Controls pilot = trimmed_controls(*f.a);
            Controller c(*f.a, pilot);
            pilot.stovl = 1.0;
            double lowest_converting = 1e9;
            bool touched = false;
            const bool ok = fly(*f.a, c, pilot, 600.0, none, [&] {
                // Where the fan's doors began to open.
                if (f.conversion() > 0.0 && lowest_converting > 1e8) {
                    lowest_converting = f.agl();
                }
                touched = touched || f.a->property("gear/wow") > 0.5;
                return hovering(c);
            });
            fail_if(!ok, "selected at 60 ft: not hovering in ten minutes");
            fail_if(lowest_converting < 98.0,
                    "selected at 60 ft: converting from " + std::to_string(lowest_converting) + " ft");
            fail_if(touched, "selected at 60 ft: her wheels touched");
            ++cases;
        }
        {
            auto a = f35b_standing("vertical_landing", 0.0, 0.0);
            Controls pilot;
            pilot.gear = 1.0;
            Controller c(*a, pilot);
            pilot.stovl = 1.0;
            fly(*a, c, pilot, 30.0, none, [] { return false; });
            fail_if(a->property("fcs/stovl-pos-norm") < 1.0,
                    "selected on the ground: not converted 30 s on");
            fail_if(a->property("gear/wow") < 0.5, "selected on the ground: not still down");
            pilot.elevator = 1.0;
            const bool up = fly(*a, c, pilot, 60.0, none,
                                [&] { return a->state().height_above_ground_ft > 50.0; });
            fail_if(!up, "selected on the ground: the stick back did not lift her 50 ft");
            ++cases;
        }
        {
            TransitionFlight f = f35b_level_at(300.0, 200.0);
            Controls pilot = trimmed_controls(*f.a);
            Controller c(*f.a, pilot);
            bool touched = false;
            fly(*f.a, c, pilot, 20.0, [&](int i) { pilot.stovl = (i / 60) % 2 == 0 ? 1.0 : 0.0; },
                [] { return false; });
            pilot.stovl = 1.0;
            const bool ok = fly(*f.a, c, pilot, 600.0, none, [&] {
                touched = touched || f.a->property("gear/wow") > 0.5;
                return hovering(c);
            });
            fail_if(!ok, "toggled quickly: not hovering in ten minutes");
            fail_if(touched, "toggled quickly: her wheels touched");
            ++cases;
        }
        {
            TransitionFlight f = f35b_level_at(300.0, 200.0);
            Controls pilot = trimmed_controls(*f.a);
            Controller c(*f.a, pilot);
            pilot.stovl = 1.0;
            const bool ok = fly(*f.a, c, pilot, 600.0, none, [&] { return hovering(c); });
            fly(*f.a, c, pilot, 30.0, none, [] { return false; });
            c.to_ai();
            double most_off = 0.0;
            fly(*f.a, c, pilot, 60.0, none, [&] {
                most_off = std::max(most_off, std::abs(f.agl() - 300.0));
                return false;
            });
            fail_if(!ok, "handed to the AI: never hovering on the selector");
            fail_if(most_off > 10.0, "handed to the AI: strayed " + std::to_string(most_off) +
                                         " ft from 300");
            ++cases;
        }
        {
            TransitionFlight f = f35b_level_at(3000.0, 300.0);
            LiftSystem lift;
            lift.stovl = 1.0;
            const bool as_asked = put_lift_system(*f.a, lift);
            fail_if(as_asked, "refused above it: put on as asked at 300 KCAS");
            fail_if(f.a->property("fcs/stovl-cmd-norm") != 0.0,
                    "refused above it: STOVL mode commanded at 300 KCAS");
            ++cases;
        }
    } catch (const std::exception& e) {
        failed.push_back(std::string("threw: ") + e.what());
    }
    std::string all;
    for (const std::string& w : failed) {
        all += "\n    " + w;
    }
    std::printf("  %zu of 6 edges flown\n", cases);
    check(failed.empty(), std::to_string(failed.size()) + " wrong:" + all);
    check(cases == 6, "every edge was flown: " + std::to_string(cases) + " of 6");
}


// **STOVL mode selected with the stick held forward from the first step,
// the stick asks a bounded descent and never the surfaces**: her
// controller made as the server makes it, on the step the selector
// arrives, holding the pilot's controls - STOVL mode, the stick full
// forward, the lever at 0.7 and never moved - from two starts, each
// counted:
//
//   300 KCAS at 2,200 ft   above her STOVL clearance and high: what a
//                          player is given through a server;
//   250 KCAS at 1,000 ft   at her clearance.
//
// Each converts, hovers and lands within fifteen minutes, her wheels
// touching only after the hover, under the hover law's 6 ft/s; until
// she hovers she is never lower than her least conversion height (100 ft)
// less 10 ft, never sinks faster than 600 ft/min - the stick's 6 ft/s and
// the laws' tracking - and her pitch stays within 15 degrees of level. On
// the code before, the transition law's autopilot was seeded with the
// stick's full nose-down elevator: from 300 KCAS she dived 334 ft in five
// seconds, sinking past 600 ft/min.
GLIDESLOPE_TEST(stovl_selected_with_the_stick_forward_from_the_first_step_descends_at_a_bounded_rate_converts_hovers_and_lands_the_f35b) {
    using namespace glideslope::sim;
    struct Start {
        double height_ft;
        double kcas;
    };
    const std::vector<Start> starts{{2200.0, 300.0}, {1000.0, 250.0}};
    std::size_t flown = 0;
    for (const Start& start : starts) {
        TransitionFlight f = f35b_level_at(start.height_ft, start.kcas);
        const double least = f.a->property("fcs/stovl-convert-least-ft");
        Controls pilot;
        pilot.stovl = 1.0;
        pilot.throttle = 0.7;
        pilot.elevator = -1.0;
        Controller controller(*f.a, pilot);
        std::optional<double> hovering_s;
        std::optional<double> touched_s;
        double touch_sink_fps = 0.0;
        double most_sink_fpm = 0.0;
        double lowest_before_ft = 1e9;
        double most_pitch = 0.0;
        for (int i = 0; i < 15 * 60 * 120 && !touched_s; ++i) {
            controller.set_pilot(pilot);
            apply(*f.a, controller);
            f.a->step();
            const AircraftState s = f.a->state();
            const Transition* t = controller.transition();
            if (!hovering_s && t && t->stage() == Transition::Stage::hovering) {
                hovering_s = s.sim_time_s;
            }
            if (!hovering_s) {
                most_sink_fpm = std::max(most_sink_fpm, -s.climb_rate_fpm);
                lowest_before_ft = std::min(lowest_before_ft, s.height_above_ground_ft);
                most_pitch = std::max(most_pitch, std::abs(s.pitch_deg));
            }
            if (f.a->property("gear/wow") > 0.5) {
                touched_s = s.sim_time_s;
                touch_sink_fps = -s.climb_rate_fpm / 60.0;
            }
        }
        const std::string at = std::to_string(static_cast<int>(start.kcas)) + " KCAS, " +
                               std::to_string(static_cast<int>(start.height_ft)) + " ft";
        std::printf("  from %s: hovering at %.1f s, down at %.1f s at %.2f ft/s; before the "
                    "hover sinking at most %.0f ft/min, lowest %.0f ft, pitch %.1f\n",
                    at.c_str(), hovering_s.value_or(-1.0), touched_s.value_or(-1.0),
                    touch_sink_fps, most_sink_fpm, lowest_before_ft, most_pitch);
        check(hovering_s.has_value(), "from " + at + " she never hovered");
        check(touched_s.has_value() && hovering_s && *touched_s > *hovering_s,
              "from " + at + " her wheels did not touch after the hover");
        check(touch_sink_fps < 6.0, "from " + at + " she touched at " +
                                        std::to_string(touch_sink_fps) + " ft/s");
        check(most_sink_fpm <= 600.0, "from " + at + " she sank at " +
                                          std::to_string(most_sink_fpm) + " ft/min");
        check(lowest_before_ft >= least - 10.0, "from " + at + " she was down to " +
                                                    std::to_string(lowest_before_ft) +
                                                    " ft before the hover");
        check(most_pitch <= 15.0, "from " + at + " her pitch reached " +
                                      std::to_string(most_pitch));
        ++flown;
    }
    check(flown == starts.size() && flown == 2, std::to_string(flown) + " starts of 2 flown");
}

// **Heavier than she can hover, STOVL mode holds her wing-borne**: at her
// plan weight - her model's own loading, 45,400 lb, which the server gives
// a player, over her 39,837 lb hover limit (`fcs/stovl-hover-most-lbs`) -
// begun level at 2,200 ft and 300 KCAS, her controller made as the server
// makes it with the pilot's STOVL mode selected, the stick full forward and
// the lever never moved, flown ten minutes: never slowed into jet-borne
// flight or a hover, held at her conversion speed within 10 kt once there,
// let down by the stick no lower than her least conversion height less 10
// ft, never sinking faster than 600 ft/min, her wheels never touching. On
// the code before, she was slowed to a hover she could not hold and sank
// into the ground at 4,369 ft/min - what a player's F-35B did through a
// server.
GLIDESLOPE_TEST(heavier_than_she_can_hover_the_f35bs_stovl_mode_holds_her_wing_borne_at_her_conversion_speed) {
    using namespace glideslope::sim;
    Aircraft a(data_dir, "f35b");
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 270.0;
    ic.altitude_ft = 2200.0;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = 300.0;
    ic.gear = 0.0;
    ic.engine_running = true;
    ic.trim = true;
    a.initialize(ic);
    a.freeze_fuel(true);
    const double weight = a.property("inertia/weight-lbs");
    const double most = a.property("fcs/stovl-hover-most-lbs");
    const double convert = a.property("fcs/stovl-convert-kcas");
    const double least = a.property("fcs/stovl-convert-least-ft");
    check(weight > most, "her model's weight " + std::to_string(weight) +
                             " lb is not over her hover limit " + std::to_string(most));
    Controls pilot;
    pilot.stovl = 1.0;
    pilot.throttle = 0.7;
    pilot.elevator = -1.0;
    Controller controller(a, pilot);
    bool slowed = false;
    bool touched = false;
    double most_sink_fpm = 0.0;
    double lowest_ft = 1e9;
    double most_off_kt = 0.0;
    bool at_speed = false;
    for (int i = 0; i < 600 * 120; ++i) {
        controller.set_pilot(pilot);
        apply(a, controller);
        a.step();
        const AircraftState s = a.state();
        const Transition* t = controller.transition();
        slowed = slowed || (t && (t->stage() == Transition::Stage::jet_borne ||
                                  t->stage() == Transition::Stage::hovering));
        touched = touched || a.property("gear/wow") > 0.5;
        most_sink_fpm = std::max(most_sink_fpm, -s.climb_rate_fpm);
        lowest_ft = std::min(lowest_ft, s.height_above_ground_ft);
        const double kcas = a.property("velocities/vc-kts");
        at_speed = at_speed || kcas <= convert + 2.0;
        if (at_speed) {
            most_off_kt = std::max(most_off_kt, std::abs(kcas - convert));
        }
    }
    std::printf("  %.0f lb over %.0f: slowed %s, lowest %.0f ft, sinking at most %.0f ft/min, "
                "off %.0f KCAS by at most %.1f kt, converted %.2f, transition law %s\n",
                weight, most, slowed ? "yes" : "no", lowest_ft, most_sink_fpm, convert,
                most_off_kt, a.property("fcs/stovl-pos-norm"),
                controller.transition() ? "flying" : "let go");
    check(!slowed, "heavier than she can hover she was slowed into jet-borne flight");
    check(!touched, "her wheels touched");
    check(controller.transition() != nullptr, "STOVL mode was let go with the selector at it");
    check(lowest_ft >= least - 10.0, "she was down to " + std::to_string(lowest_ft) + " ft");
    check(most_sink_fpm <= 600.0, "she sank at " + std::to_string(most_sink_fpm) + " ft/min");
    check(at_speed && most_off_kt <= 10.0,
          "at her conversion speed she strayed " + std::to_string(most_off_kt) + " kt");
}

// **Selected at any moment of a dive her untouched controls fly her into, a
// heavy F-35B's STOVL mode pulls her out above her least conversion height.**
// What a server gives a player - her model's own 45,400 lb, untrimmed at
// 2,200 ft and 300 KCAS, fuel burning - flown as the server flies her until
// her first input says otherwise: the throttle at the 0.6 it holds and the
// stick centred, which in her dives her, 5,600 ft/min by five seconds and
// 10,400 by ten. Then, at every 20th step from the first to ten seconds (61
// moments, every one flown: coverage asserted), STOVL mode selected with the
// stick full forward and the lever still, her controller made on that step
// from those controls as the server makes it, and flown a minute on: never
// lower than her least conversion height less 10 ft, her wheels never
// touching, never slowed into jet-borne flight or a hover, and climbing
// back toward the height the stick asks by the minute's end. Selected
// through a server, this is when the player's input happens to arrive - a
// slower machine or a longer join, later in the dive. **Seen red** before
// the autopilot's vertical-speed integral was seeded within its pitch
// envelope: taken over diving, it was seeded 45 degrees nose-down and held
// the pull-out back - selected at 7.7 s she went down to 72 ft (and, flown
// alone, at 8.3 s to 1 ft and at 10 s into the ground). **Not covered**: later than ten seconds - by 12.5 s she
// is at 780 ft sinking 12,800 ft/min, past what a pull at the autopilot's
// 3 degrees a second can save.
GLIDESLOPE_TEST(a_heavy_f35b_selected_into_stovl_mode_at_any_moment_of_ten_seconds_untouched_is_pulled_out_above_her_least_conversion_height) {
    using namespace glideslope::sim;
    constexpr int every_steps = 20;
    constexpr int last_step = 10 * 120;
    constexpr int moments = last_step / every_steps + 1;
    int flown = 0;
    double lowest_of_all = 1e9;
    int lowest_at = -1;
    for (int at = 0; at <= last_step; at += every_steps) {
        Aircraft a(data_dir, "f35b");
        InitialConditions ic;
        ic.latitude_deg = -33.9461;
        ic.longitude_deg = 151.1772;
        ic.heading_deg = 270.0;
        ic.altitude_ft = 2200.0;
        ic.terrain_elevation_ft = 20.0;
        ic.airspeed_kts = 300.0;
        ic.gear = 0.0;
        ic.engine_running = true;
        a.initialize(ic);
        const double least = a.property("fcs/stovl-convert-least-ft");
        check(a.property("inertia/weight-lbs") > a.property("fcs/stovl-hover-most-lbs"),
              "she is not over her hover limit");
        Controls untouched;
        untouched.throttle = 0.6;
        for (int i = 0; i < at; ++i) {
            a.set_controls(untouched);
            a.step();
        }
        const AircraftState selected = a.state();
        Controls pilot;
        pilot.stovl = 1.0;
        pilot.throttle = 0.7;
        pilot.elevator = -1.0;
        Controller controller(a, pilot);
        double lowest_ft = 1e9;
        bool touched = false;
        bool slowed = false;
        for (int i = 0; i < 60 * 120; ++i) {
            controller.set_pilot(pilot);
            apply(a, controller);
            a.step();
            const AircraftState s = a.state();
            lowest_ft = std::min(lowest_ft, s.height_above_ground_ft);
            touched = touched || a.property("gear/wow") > 0.5;
            const Transition* t = controller.transition();
            slowed = slowed || (t && (t->stage() == Transition::Stage::jet_borne ||
                                      t->stage() == Transition::Stage::hovering));
        }
        const AircraftState end = a.state();
        const double asked_ft = controller.stovl_height_asked_ft();
        const std::string when = "selected at step " + std::to_string(at) + " (" +
                                 std::to_string(static_cast<int>(selected.height_above_ground_ft)) +
                                 " ft, " + std::to_string(static_cast<int>(selected.climb_rate_fpm)) +
                                 " ft/min)";
        if (at % 240 == 0) {
            std::printf("  %s: lowest %.0f ft; a minute on %.0f ft, asked %.0f, %.0f ft/min\n",
                        when.c_str(), lowest_ft, end.height_above_ground_ft, asked_ft,
                        end.climb_rate_fpm);
        }
        check(lowest_ft >= least - 10.0, when + ": she was down to " + std::to_string(lowest_ft) + " ft");
        check(!touched, when + ": her wheels touched");
        check(!slowed, when + ": she was slowed into jet-borne flight");
        check(end.height_above_ground_ft >= asked_ft - 100.0 || end.climb_rate_fpm > 0.0,
              when + ": a minute on she was at " + std::to_string(end.height_above_ground_ft) +
                  " ft, asked " + std::to_string(asked_ft) + ", and not climbing back");
        if (lowest_ft < lowest_of_all) {
            lowest_of_all = lowest_ft;
            lowest_at = at;
        }
        ++flown;
    }
    std::printf("  %d of %d moments flown; the lowest %.0f ft, selected at step %d\n", flown,
                moments, lowest_of_all, lowest_at);
    check(flown == moments && moments == 61,
          std::to_string(flown) + " of " + std::to_string(moments) + " moments flown, not 61");
}

// **On the ground, heavier than she can hover, STOVL mode does nothing**:
// standing at her model's own 45,400 lb, the brakes on and the throttle at
// idle, the pilot selects STOVL mode and holds the stick full back for a
// minute. Her lift system is never converted, no transition law is made,
// her wheels never leave the ground and no ground judge (sim::GroundJudge)
// calls her a wreck. On the code before, the transition law, made on her
// wheels, stood her converted for a vertical take-off she could not hold.
GLIDESLOPE_TEST(on_the_ground_heavier_than_she_can_hover_the_f35bs_stovl_selector_does_nothing) {
    using namespace glideslope::sim;
    Aircraft a(data_dir, "f35b");
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 70.0;
    a.initialize(ic);
    const double weight = a.property("inertia/weight-lbs");
    check(weight > a.property("fcs/stovl-hover-most-lbs"),
          "standing at " + std::to_string(weight) + " lb, not over her hover limit");
    Controls pilot;
    pilot.stovl = 1.0;
    pilot.throttle = 0.0;
    pilot.elevator = 1.0;
    pilot.left_brake = pilot.right_brake = 1.0;
    Controller controller(a, pilot);
    GroundJudge judge(false);
    std::optional<std::string> wrecked;
    bool lifted = false;
    double most_conversion = 0.0;
    bool law = false;
    for (int i = 0; i < 60 * 120; ++i) {
        controller.set_pilot(pilot);
        apply(a, controller);
        a.step();
        lifted = lifted || a.property("gear/wow") < 0.5;
        most_conversion = std::max(most_conversion, a.property("fcs/stovl-pos-norm"));
        law = law || controller.transition() != nullptr || controller.lift() != nullptr;
        if (const auto why = judge.judge(a); why && !wrecked) {
            wrecked = why;
        }
    }
    std::printf("  %.0f lb standing: converted %.2f at most, law made %s, lifted %s, %s\n",
                weight, most_conversion, law ? "yes" : "no", lifted ? "yes" : "no",
                wrecked ? wrecked->c_str() : "not a wreck");
    check(most_conversion <= 0.0, "her lift system converted to " + std::to_string(most_conversion));
    check(!law, "a transition law was made for her on the ground");
    check(!lifted, "her wheels left the ground");
    check(!wrecked, "she was a wreck: " + wrecked.value_or(""));
}

// **Burned below her hover limit, she is converted and hovered**: at her
// combat loading with 120 lb more in each tank - 39,990 lb, 153 over her
// 39,837 lb limit - fuel burning, begun level at 1,000 ft and 200 KCAS, the
// pilot's STOVL mode selected from the first step, the stick centred and
// the lever never moved. Above the limit she is held wing-borne (never
// jet-borne or hovering); once fuel has burned her under it she is
// converted and slows to a hover, within ten minutes of crossing it.
// Throughout, no lower than her least conversion height less 10 ft, never
// sinking faster than 600 ft/min, her pitch within 15 degrees.
GLIDESLOPE_TEST(burned_below_her_hover_limit_in_stovl_mode_the_f35b_converts_and_hovers) {
    using namespace glideslope::sim;
    const PublishedFigures figures = read_published_figures(figures_file("f35b"));
    Loading loading = figures.loadings.at("combat").loading;
    loading.tank_lbs.at(0) += 120.0;
    loading.tank_lbs.at(1) += 120.0;
    Aircraft a(data_dir, "f35b");
    a.load(loading);
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 70.0;
    ic.altitude_ft = 1000.0;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = 200.0;
    ic.gear = 0.0;
    ic.trim = true;
    a.initialize(ic);
    const double most = a.property("fcs/stovl-hover-most-lbs");
    const double least = a.property("fcs/stovl-convert-least-ft");
    const double began_lbs = a.property("inertia/weight-lbs");
    check(began_lbs > most, "begun at " + std::to_string(began_lbs) + " lb, not over the limit");
    Controls pilot;
    pilot.stovl = 1.0;
    pilot.throttle = 0.7;
    Controller controller(a, pilot);
    std::optional<double> crossed_s;
    std::optional<double> hovering_s;
    bool slowed_heavy = false;
    double lowest_ft = 1e9;
    double most_sink_fpm = 0.0;
    double most_pitch = 0.0;
    for (int i = 0; i < 40 * 60 * 120 && !hovering_s; ++i) {
        controller.set_pilot(pilot);
        apply(a, controller);
        a.step();
        const AircraftState s = a.state();
        const Transition* t = controller.transition();
        const bool heavy = a.property("inertia/weight-lbs") > most;
        if (!heavy && !crossed_s) {
            crossed_s = s.sim_time_s;
        }
        if (heavy && t && (t->stage() == Transition::Stage::jet_borne ||
                           t->stage() == Transition::Stage::hovering)) {
            slowed_heavy = true;
        }
        if (t && t->stage() == Transition::Stage::hovering) {
            hovering_s = s.sim_time_s;
        }
        lowest_ft = std::min(lowest_ft, s.height_above_ground_ft);
        most_sink_fpm = std::max(most_sink_fpm, -s.climb_rate_fpm);
        most_pitch = std::max(most_pitch, std::abs(s.pitch_deg));
    }
    std::printf("  begun at %.0f lb over %.0f: under it at %.1f s, hovering at %.1f s; lowest "
                "%.0f ft, sinking at most %.0f ft/min, pitch %.1f\n",
                began_lbs, most, crossed_s.value_or(-1.0), hovering_s.value_or(-1.0), lowest_ft,
                most_sink_fpm, most_pitch);
    check(!slowed_heavy, "above her hover limit she was slowed into jet-borne flight");
    check(crossed_s.has_value(), "her fuel never burned her under the limit in 40 minutes");
    check(hovering_s && *hovering_s - *crossed_s <= 600.0,
          "not hovering ten minutes after she was under the limit");
    check(lowest_ft >= least - 10.0, "she was down to " + std::to_string(lowest_ft) + " ft");
    check(most_sink_fpm <= 600.0, "she sank at " + std::to_string(most_sink_fpm) + " ft/min");
    check(most_pitch <= 15.0, "her pitch reached " + std::to_string(most_pitch));
}

// **Every loading a figures file names is found by its name**, as a server
// gives a player one she asks for (net::AskedAircraft) and her prediction is
// loaded as: the same pointmasses and tanks, and loaded so she weighs the
// file's total within half a per cent. A name the file does not hold, an
// empty one and a model with no figures file are none. Every model with
// figures and every loading in each is walked, and the count is stated -
// **less the loadings with no name**, a file's single unnamed loading that
// its figures are flown at, which no player can ask for by name and which
// `named_loading` holds as none.
GLIDESLOPE_TEST(every_loading_a_figures_file_names_is_found_by_its_name_and_weighs_its_total) {
    const std::filesystem::path data = std::filesystem::path(data_dir).parent_path();
    std::size_t loadings = 0;
    std::size_t walked = 0;
    std::size_t unnamed = 0;
    for (const std::string& model : figured_models()) {
        const PublishedFigures figures = read_published_figures(figures_file(model));
        loadings += figures.loadings.size();
        for (const auto& [name, want] : figures.loadings) {
            if (name.empty()) {
                ++unnamed;
                check(!glideslope::sim::named_loading(data, model, name),
                      model + "'s unnamed loading is not found by an empty name");
                continue;
            }
            const auto got = glideslope::sim::named_loading(data, model, name);
            check(got.has_value(), model + "'s " + name + " is found by its name");
            check(got->total_lbs == want.total_lbs &&
                      got->loading.pointmass_lbs == want.loading.pointmass_lbs &&
                      got->loading.tank_lbs == want.loading.tank_lbs,
                  model + "'s " + name + " is the file's");
            glideslope::sim::Aircraft aircraft(data / "jsbsim", model);
            aircraft.load(got->loading);
            const double lbs = aircraft.loaded_weight_lbs();
            check(std::abs(lbs - want.total_lbs) <= 0.005 * want.total_lbs,
                  model + "'s " + name + " weighs " + std::to_string(lbs) + " lb loaded, not " +
                      std::to_string(want.total_lbs));
            ++walked;
        }
        check(!glideslope::sim::named_loading(data, model, "no_such_loading"),
              model + " has no loading no_such_loading");
        check(!glideslope::sim::named_loading(data, model, ""), model + " has no loading ''");
    }
    check(!glideslope::sim::named_loading(data, "no-such-model", "full"),
          "a model with no figures file has no loading");
    check(figured_models().size() == 16 && walked + unnamed == loadings && walked > 0,
          "every loading of 16 models was walked: " + std::to_string(walked) + " named and " +
              std::to_string(unnamed) + " unnamed of " + std::to_string(loadings));
    std::printf("%zu loadings of %zu models found by name; %zu unnamed, found by none\n", walked,
                figured_models().size(), unnamed);
}
