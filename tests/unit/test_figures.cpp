#include "harness.hpp"

#include "sim/figures.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
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
    static const std::vector<std::string> models = {"c172p", "mosquito-fb6"};
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
    check(figures_in_files == 23,
          "twenty-three figures, one test each above; found " +
              std::to_string(figures_in_files));
}
