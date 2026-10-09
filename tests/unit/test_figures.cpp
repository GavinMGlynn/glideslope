#include "harness.hpp"

#include "sim/catalogue.hpp"
#include "sim/figures.hpp"
#include "sim/lander.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
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
    check(figures_in_files == 122,
          "a hundred and twenty-two figures, one test each above; found " +
              std::to_string(figures_in_files));
}

// **The speeds a plan may fly an aircraft at are read, and a file without
// them, or with them wrong, is refused** rather than planned at a speed it
// cannot hold. Each file tried is the C172P's own with its <plan_speeds>
// changed, so a refusal is that element's and nothing else's.
GLIDESLOPE_TEST(the_speeds_a_plan_may_fly_an_aircraft_at_are_read_and_refused_where_they_are_wrong) {
    const PublishedFigures c172p = read_published_figures(figures_file("c172p"));
    check(c172p.plan_slowest_kcas == 60.0 && c172p.plan_fastest_kcas == 110.0,
          "the C172P may be planned from 60 to 110 kt");

    std::ifstream in(figures_file("c172p"));
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const std::string element = "<plan_speeds slowest_kcas=\"60\" fastest_kcas=\"110\">";
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

    write("<plan_speeds slowest_kcas=\"55\" fastest_kcas=\"125\"></plan_speeds>");
    const PublishedFigures written = read_published_figures(file);
    check(written.plan_slowest_kcas == 55.0 && written.plan_fastest_kcas == 125.0,
          "other speeds, written the same way, are read back as written");
    const std::pair<const char*, const char*> wrong[] = {
        {"", "gives no <plan_speeds"},
        {"<plan_speeds fastest_kcas=\"120\"></plan_speeds>", "gives no <plan_speeds"},
        {"<plan_speeds slowest_kcas=\"60\"></plan_speeds>", "gives no <plan_speeds"},
        {"<plan_speeds slowest_kcas=\"0\" fastest_kcas=\"120\"></plan_speeds>", "not above 0"},
        {"<plan_speeds slowest_kcas=\"120\" fastest_kcas=\"120\"></plan_speeds>",
         "the fastest above the slowest"},
        {"<plan_speeds slowest_kcas=\"130\" fastest_kcas=\"120\"></plan_speeds>",
         "the fastest above the slowest"},
        {"<plan_speeds slowest_kcas=\"60\" fastest_kcas=\"120\"></plan_speeds>"
         "<plan_speeds slowest_kcas=\"60\" fastest_kcas=\"120\"></plan_speeds>",
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
