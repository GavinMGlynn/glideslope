#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/crash.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

bool wheels_retract(const std::string& model) {
    std::ifstream in(data() / "jsbsim" / "aircraft" / model / (model + ".xml"),
                     std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    return text.find("<retractable>1</retractable>") != std::string::npos;
}

// Where an aircraft's centre of gravity rests above the ground on its wheels:
// started on the ground and left to settle.
double resting_height_ft(const glideslope::sim::CatalogueEntry& entry) {
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.terrain_elevation_ft = 0.0;
    ic.altitude_ft = 0.0;
    ic.airspeed_kts = 0.0;
    ic.engine_running = false;
    aircraft.initialize(ic);
    glideslope::sim::Controls c;
    c.throttle = 0.0;
    c.left_brake = 1.0;
    c.right_brake = 1.0;
    for (int i = 0; i < 5 * steps_per_second; ++i) {
        aircraft.set_controls(c);
        aircraft.step();
    }
    return aircraft.property("position/h-agl-ft");
}

struct Dropped {
    std::optional<std::string> wrecked;
    bool broke = false;
};

// **Let go above the ground**, with no speed, the engines off and the brakes
// on, and judged every step for ten seconds. A drop from h feet meets the
// ground at sqrt(2 g h): 25 feet is 40 ft/s, four times what gear takes; half
// a foot is 5.7 ft/s, which gear takes easily.
Dropped drop(const glideslope::sim::CatalogueEntry& entry, double above_rest_ft,
             double gear, bool on_water) {
    const double rest_ft = resting_height_ft(entry);
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; },
        [on_water](double, double) { return on_water; }));
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.terrain_elevation_ft = 0.0;
    ic.altitude_ft = rest_ft + above_rest_ft;
    ic.airspeed_kts = 0.0;
    ic.engine_running = false;
    ic.gear = gear;
    aircraft.initialize(ic);
    glideslope::sim::GroundJudge judge(entry.seaplane);
    glideslope::sim::Controls c;
    c.throttle = 0.0;
    c.gear = gear;
    c.left_brake = 1.0;
    c.right_brake = 1.0;
    Dropped out;
    for (int i = 0; i < 10 * steps_per_second; ++i) {
        aircraft.set_controls(c);
        aircraft.step();
        if (!std::isfinite(aircraft.property("position/h-agl-ft"))) {
            out.broke = true;
            break;
        }
        if (const auto why = judge.judge(aircraft)) {
            out.wrecked = why;
            break;
        }
    }
    return out;
}

} // namespace

// **A hard landing wrecks and a good one does not**, for every landplane in
// the catalogue. Each is let go 25 feet above where it rests on its wheels -
// meeting the ground at about 40 ft/s, four times the 10 ft/s landing gear is
// certified to take (sim/crash.hpp) - and must be wrecked; and let go half a
// foot above it - about 5.7 ft/s - and must not be. The flying boat has no
// wheels and is judged on water below.
GLIDESLOPE_TEST(every_landplane_dropped_harder_than_its_gear_takes_is_wrecked_and_set_down_gently_is_not) {
    const auto roster = glideslope::sim::read_catalogue(data());
    check(roster.size() == 16, "the roster is sixteen aircraft, not " +
                                   std::to_string(roster.size()));
    std::size_t walked = 0, afloat = 0;
    std::vector<std::string> wrong;
    for (const auto& entry : roster) {
        if (entry.seaplane) {
            ++afloat;
            continue;
        }
        const Dropped hard = drop(entry, 25.0, 1.0, false);
        const Dropped gentle = drop(entry, 0.5, 1.0, false);
        std::printf("  %-14s from 25 ft: %-45s from half a foot: %s\n", entry.id.c_str(),
                    hard.wrecked ? hard.wrecked->c_str() : "not wrecked",
                    gentle.wrecked ? gentle.wrecked->c_str() : "not wrecked");
        std::fflush(stdout);
        if (hard.broke || gentle.broke) {
            wrong.push_back(entry.id + "'s state stopped being a number");
        }
        if (!hard.wrecked) {
            wrong.push_back(entry.id + " dropped 25 ft was not wrecked");
        }
        if (gentle.wrecked) {
            wrong.push_back(entry.id + " set down from half a foot was wrecked: " +
                            *gentle.wrecked);
        }
        ++walked;
    }
    for (const std::string& w : wrong) {
        std::printf("  %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " landplanes were judged wrongly");
    check(walked + afloat == roster.size(),
          "every aircraft was walked: " + std::to_string(walked) + " landplanes and " +
              std::to_string(afloat) + " afloat, of " + std::to_string(roster.size()));
    check(walked == 15 && afloat == 1, "fifteen landplanes and one flying boat");
}

// **An airframe on the ground is a wreck.** Every aircraft whose wheels retract,
// put down from two feet above where it would rest on them with them up, comes
// down on its belly - its STRUCTURE contacts (test_wheels_up.cpp) - and must be
// wrecked for it. The five with fixed wheels cannot be put down on anything
// else, and are named in the count.
GLIDESLOPE_TEST(an_aircraft_that_comes_down_on_its_airframe_is_wrecked) {
    const auto roster = glideslope::sim::read_catalogue(data());
    std::size_t retracting = 0, fixed = 0, afloat = 0;
    std::vector<std::string> wrong;
    for (const auto& entry : roster) {
        if (entry.seaplane) {
            ++afloat;
            continue;
        }
        if (!wheels_retract(entry.model)) {
            ++fixed;
            continue;
        }
        ++retracting;
        const Dropped belly = drop(entry, 2.0, 0.0, false);
        std::printf("  %-14s wheels up: %s\n", entry.id.c_str(),
                    belly.wrecked ? belly.wrecked->c_str() : "not wrecked");
        if (!belly.wrecked || belly.wrecked->find("airframe") == std::string::npos) {
            wrong.push_back(entry.id + " came down on its belly and was " +
                            (belly.wrecked ? "wrecked for something else: " + *belly.wrecked
                                           : std::string("not wrecked")));
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " were judged wrongly");
    check(retracting + fixed + afloat == roster.size(), "every aircraft was counted");
    check(retracting == 10 && fixed == 5, "ten retract and five do not, not " +
                                              std::to_string(retracting) + " and " +
                                              std::to_string(fixed));
}

// **Water: a landplane that comes down on it is wrecked, and the flying boat,
// whose home it is, is not.** The Cessna is set down gently on water - it
// ditches - and the Short S.23 let go half a foot above it and left to float.
GLIDESLOPE_TEST(a_landplane_on_water_is_wrecked_and_a_flying_boat_afloat_is_not) {
    const auto roster = glideslope::sim::read_catalogue(data());
    const auto cessna = std::find_if(roster.begin(), roster.end(),
                                     [](const auto& e) { return e.id == "c172p"; });
    const auto boat = std::find_if(roster.begin(), roster.end(),
                                   [](const auto& e) { return e.seaplane; });
    check(cessna != roster.end() && boat != roster.end(), "the Cessna and the flying boat");
    const Dropped ditched = drop(*cessna, 0.5, 1.0, true);
    std::printf("  c172p on water: %s\n",
                ditched.wrecked ? ditched.wrecked->c_str() : "not wrecked");
    check(ditched.wrecked && ditched.wrecked->find("water") != std::string::npos,
          "a Cessna set down on water is wrecked for coming down on water");
    const Dropped afloat = drop(*boat, 0.5, 1.0, true);
    std::printf("  %s on water: %s\n", boat->id.c_str(),
                afloat.wrecked ? afloat.wrecked->c_str() : "not wrecked");
    check(!afloat.wrecked, "the flying boat afloat is not wrecked");
}

// **Two aircraft closer than the mean of their wingspans have collided, and
// further apart have not** - for every pair of wingspans in the catalogue - and
// **nothing passes through anything between two steps**: at 120 steps a second
// two aircraft closing at 1,000 knots move 4.3 m a step, and the smallest reach
// between any two is far more than that.
GLIDESLOPE_TEST(two_aircraft_within_their_wingspans_have_collided_and_none_can_pass_between_steps) {
    const auto roster = glideslope::sim::read_catalogue(data());
    std::vector<std::pair<std::string, double>> spans;
    for (const auto& entry : roster) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        const double span = aircraft.figures().wingspan_ft;
        check(span > 10.0 && span < 300.0,
              entry.id + "'s wingspan is " + std::to_string(span) + " ft");
        spans.emplace_back(entry.id, span);
    }
    constexpr double feet_per_metre = 3.28083989501312;
    const double step_at_1000_kts_m = 1000.0 * 0.514444 / steps_per_second;
    std::size_t pairs = 0;
    double least_reach_m = 1e9;
    for (const auto& [a_id, a_span] : spans) {
        for (const auto& [b_id, b_span] : spans) {
            const double reach_m = (a_span + b_span) / 2.0 / feet_per_metre;
            least_reach_m = std::min(least_reach_m, reach_m);
            const double here[3] = {-4646000.0, 2553000.0, -3534000.0};
            const double inside[3] = {here[0] + 0.9 * reach_m, here[1], here[2]};
            const double outside[3] = {here[0], here[1] + 1.1 * reach_m, here[2]};
            check(glideslope::sim::collided(here, a_span, inside, b_span),
                  a_id + " and " + b_id + " inside their reach have collided");
            check(!glideslope::sim::collided(here, a_span, outside, b_span),
                  a_id + " and " + b_id + " beyond their reach have not");
            ++pairs;
        }
    }
    std::printf("  %zu pairs; the least reach is %.1f m, and 1,000 knots is %.1f m a step\n",
                pairs, least_reach_m, step_at_1000_kts_m);
    check(pairs == spans.size() * spans.size(), "every pair was tried");
    check(pairs == 256, "sixteen aircraft make 256 pairs, not " + std::to_string(pairs));
    check(least_reach_m > 2.0 * step_at_1000_kts_m,
          "the least reach is more than two steps at 1,000 knots");
}
