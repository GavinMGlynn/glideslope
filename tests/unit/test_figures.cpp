#include "harness.hpp"

#include "sim/figures.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>

using glideslope::sim::fly_figure;
using glideslope::sim::known_figures;
using glideslope::sim::PublishedFigures;
using glideslope::sim::read_published_figures;
using glideslope::test::check;
using glideslope::test::fail;

namespace {

const char* const data_dir = GLIDESLOPE_TEST_DATA_DIR;
const char* const figures_file = GLIDESLOPE_TEST_FIGURES_DIR "/c172p.xml";

// Flies one figure from the Cessna's file and fails with the measurement, the
// range and the source if it lands outside the range.
void expect_figure(const std::string& name) {
    const PublishedFigures figures = read_published_figures(figures_file);
    const auto it = std::find_if(figures.figures.begin(), figures.figures.end(),
                                 [&](const auto& f) { return f.name == name; });
    if (it == figures.figures.end()) {
        fail("assets/figures/c172p.xml has no figure named " + name);
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
    expect_figure("static_rpm");
}

GLIDESLOPE_TEST(the_cessna_172p_takes_off_in_about_its_published_ground_roll) {
    expect_figure("takeoff_ground_roll");
}

GLIDESLOPE_TEST(a_cessna_172p_at_full_power_climbs_near_its_published_rate) {
    expect_figure("climb_rate");
}

GLIDESLOPE_TEST(the_cessna_172p_cruises_near_its_published_speed) {
    expect_figure("cruise_speed");
}

GLIDESLOPE_TEST(the_cessna_172p_glides_near_its_published_ratio) {
    expect_figure("glide_ratio");
}

GLIDESLOPE_TEST(the_cessna_172p_stalls_flaps_up_near_its_published_speed) {
    expect_figure("stall_speed_flaps_up");
}

GLIDESLOPE_TEST(
    the_cessna_172p_stalls_with_10_degrees_of_flap_near_its_published_speed) {
    expect_figure("stall_speed_flaps_10");
}

GLIDESLOPE_TEST(
    the_cessna_172p_stalls_with_30_degrees_of_flap_near_its_published_speed) {
    expect_figure("stall_speed_flaps_30");
}

GLIDESLOPE_TEST(a_coordinated_level_turn_turns_at_the_rate_its_bank_and_speed_demand) {
    expect_figure("turn_rate");
}

// Every flight the checks can fly is a figure in the file, and every figure in
// the file is a flight - so a figure added to the file with no flight, or a
// flight written and never given a figure, fails here rather than never running.
// Each figure also has one test above; registered_tests.cmake keeps those
// registered, and this keeps the two lists the same size.
GLIDESLOPE_TEST(every_published_figure_has_a_flight_and_every_flight_a_figure) {
    const PublishedFigures figures = read_published_figures(figures_file);
    std::set<std::string> in_file;
    for (const auto& f : figures.figures) {
        check(in_file.insert(f.name).second,
              "figure " + f.name + " is in the file twice");
    }
    const auto flights = known_figures();
    const std::set<std::string> flown(flights.begin(), flights.end());
    for (const auto& name : flown) {
        check(in_file.count(name) == 1,
              "flight " + name + " has no figure in the file");
    }
    for (const auto& name : in_file) {
        check(flown.count(name) == 1, "figure " + name + " has no flight");
    }
    check(in_file.size() == 9,
          "nine figures, one test each above; found " + std::to_string(in_file.size()));
}
