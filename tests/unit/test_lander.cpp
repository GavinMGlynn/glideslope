#include "after_touch.hpp"
#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/lander.hpp"
#include "sim/runway_condition.hpp"
#include "sim/terrain.hpp"
#include "sim/weather.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using glideslope::sim::ApproachSpeeds;
using glideslope::sim::Lander;
using glideslope::sim::Runway;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;
constexpr double degrees = 180.0 / 3.14159265358979323846;
constexpr double metres_per_nm = 1852.0;
constexpr double feet_per_metre = 3.280839895013123;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// The light aircraft, which is what this item's verification names.
const std::vector<std::string>& light_aircraft() {
    static const std::vector<std::string> ids{"c172p", "c182", "pa28", "j3cub"};
    return ids;
}

// A runway at sea level, pointing 070 - the one the published figures are
// flown from, so the ground beneath it is ground this project already uses.
Runway a_runway() {
    Runway r;
    r.name = "the runway";
    r.threshold_lat_deg = -33.9461;
    r.threshold_lon_deg = 151.1772;
    r.elevation_ft = 0.0;
    r.heading_deg = 70.0;
    r.length_m = 3000.0;
    return r;
}

double metres_per_degree_latitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111132.92 - 559.82 * std::cos(2.0 * lat) + 1.175 * std::cos(4.0 * lat) -
           0.0023 * std::cos(6.0 * lat);
}

double metres_per_degree_longitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111412.84 * std::cos(lat) - 93.5 * std::cos(3.0 * lat) +
           0.118 * std::cos(5.0 * lat);
}

struct Landing {
    bool touched = false;
    bool stopped = false;
    double sink_fpm = 0.0;
    double across_m = 0.0;
    double along_m = 0.0; // from the threshold, positive down the runway
    double touch_pitch_deg = 0.0; // her nose as her wheels first touched
    double standing_pitch_deg = 0.0; // her nose standing on her wheels (Aircraft::stance)
    double stopped_along_m = 0.0;
    double worst_across_on_final_m = 0.0; // inside a mile
    double seconds = 0.0;
    // From the first touch to the stop: how far she banked, how far her nose
    // went down, and how high she went again. A landing that bounces, or ends
    // on its nose or its back, is not one - and stopping says nothing about
    // which way up she stopped.
    double worst_roll_after_touch_deg = 0.0;
    double least_pitch_after_touch_deg = 0.0;
    double highest_after_touch_ft = 0.0;
    double worst_across_after_touch_m = 0.0; // from the centreline, on the roll
    // As the server judges the touch (sim/crash.hpp), and whether she
    // settled: what was wrong, one line each.
    std::vector<std::string> gear_took_it;
};

// **From five miles out, on the glidepath, down to a stop**, or from
// `out_nm`. `crosswind_kts` blows from the left across the runway, or from
// the right where it is negative; zero is calm air.
Landing land(const std::string& id, double crosswind_kts, bool wet = false,
             double out_nm = 5.0) {
    std::printf("landing the %s in %.0f knots of crosswind from the %s%s\n", id.c_str(),
                std::abs(crosswind_kts), crosswind_kts < 0.0 ? "right" : "left",
                wet ? ", wet" : "");
    std::fflush(stdout);
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const Runway runway = a_runway();
    const ApproachSpeeds speeds = glideslope::sim::approach_speeds(data(), entry.model);

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    // The ground is the runway's own elevation, everywhere: this is about the
    // approach, not the terrain.
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));

    if (crosswind_kts != 0.0 || wet) {
        // Straight across the runway from the left: the runway points 070,
        // so the wind is from 340 and blows towards 160 (from 160 towards
        // 340 where it is negative).
        glideslope::sim::Conditions conditions;
        conditions.runway_condition =
            wet ? glideslope::sim::wet_runway : glideslope::sim::dry_runway;
        const double towards = (runway.heading_deg - 90.0 + 180.0) / degrees;
        const double mps = crosswind_kts * 0.514444;
        conditions.wind_north_mps = mps * std::cos(towards);
        conditions.wind_east_mps = mps * std::sin(towards);
        aircraft.set_weather(
            std::make_shared<glideslope::sim::SteadyWeather>(conditions));
    }

    const double out_m = out_nm * metres_per_nm;
    const double heading = runway.heading_deg / degrees;
    const double north_m = -out_m * std::cos(heading);
    const double east_m = -out_m * std::sin(heading);

    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg +
                      north_m / metres_per_degree_latitude(runway.threshold_lat_deg);
    ic.longitude_deg = runway.threshold_lon_deg +
                       east_m / metres_per_degree_longitude(runway.threshold_lat_deg);
    // On the glidepath where it starts, which aims past the threshold.
    ic.altitude_ft = runway.elevation_ft +
                     (out_m + speeds.aim_m) * std::tan(3.0 / degrees) * feet_per_metre;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = speeds.vref_kts;
    ic.engine_running = true;
    ic.gear = 1.0;
    aircraft.initialize(ic);

    Lander lander(aircraft, runway, speeds);
    Landing out;
    glideslope::test::AfterTouch after;
    after.judged_as(entry.seaplane);
    double on_the_ground_agl_ft = -1.0;
    for (int tick = 0; tick < 900 * steps_per_second; ++tick) {
        aircraft.set_controls(lander.fly());
        aircraft.step();
        after.watch(aircraft);
        if (lander.touchdown_sink_fpm() != 0.0 || lander.touchdown_along_m() != 0.0) {
            const auto s = aircraft.state();
            // The height of her centre of gravity as the wheels first met
            // the runway is what "on the ground" is for this aeroplane.
            if (on_the_ground_agl_ft < 0.0) {
                on_the_ground_agl_ft = aircraft.property("position/h-agl-ft");
            }
            out.worst_roll_after_touch_deg =
                std::max(out.worst_roll_after_touch_deg, std::abs(s.roll_deg));
            out.worst_across_after_touch_m =
                std::max(out.worst_across_after_touch_m, std::abs(lander.across_m()));
            out.least_pitch_after_touch_deg =
                std::min(out.least_pitch_after_touch_deg, s.pitch_deg);
            out.highest_after_touch_ft = std::max(
                out.highest_after_touch_ft,
                aircraft.property("position/h-agl-ft") - on_the_ground_agl_ft);
        }
        if (lander.stage() == Lander::Stage::approach && lander.along_m() < metres_per_nm &&
            lander.along_m() > 0.0) {
            out.worst_across_on_final_m =
                std::max(out.worst_across_on_final_m, std::abs(lander.across_m()));
        }
        if (lander.stage() == Lander::Stage::stopped) {
            out.stopped = true;
            out.stopped_along_m = -lander.along_m();
            out.seconds = static_cast<double>(tick) / steps_per_second;
            break;
        }
    }
    // Half a foot, as the lessons hold every aeroplane the AI lands
    // (test_lesson.cpp, `settled_within_ft`).
    out.gear_took_it = after.how_the_gear_took_it(id, 0.5);
    out.touched = lander.touchdown_sink_fpm() != 0.0 || lander.touchdown_along_m() != 0.0;
    out.sink_fpm = lander.touchdown_sink_fpm();
    out.across_m = lander.touchdown_across_m();
    out.along_m = lander.touchdown_along_m();
    out.touch_pitch_deg = lander.touchdown_pitch_deg();
    out.standing_pitch_deg = aircraft.stance().standing_pitch_deg;
    if (!out.stopped) {
        out.seconds = 900.0;
    }
    std::printf("  %s: touched at %.0f ft/min, %.2f m across, %.0f m along; "
                "stopped %.0f m along after %.0f s; worst %.2f m across on final; "
                "after touching, rolled %.1f, pitched down to %.1f, rose %.1f ft; "
                "touched at %.2f degrees, stands at %.2f\n",
                id.c_str(), out.sink_fpm, out.across_m, out.along_m,
                out.stopped_along_m, out.seconds, out.worst_across_on_final_m,
                out.worst_roll_after_touch_deg, out.least_pitch_after_touch_deg,
                out.highest_after_touch_ft, out.touch_pitch_deg, out.standing_pitch_deg);
    std::fflush(stdout);
    return out;
}

// **She stays on her wheels, the right way up, from the touch to the stop.**
// Without this the J3 Cub passed while bouncing forty feet back into the air,
// stalling and ending on its back: it touched gently, and it did stop.
// Fifteen degrees of bank puts a light aeroplane's wingtip near the runway;
// a nose ten degrees down is a nosewheel aeroplane on its nose or a
// tailwheel one over it; and three feet up is a bounce, not a rollout.
// What was wrong with her from the touch to the stop, one line each; none
// for a landing that stayed down and upright.
std::vector<std::string> wrong_after_the_touch(const std::string& id, const Landing& l,
                                               const std::string& where) {
    std::vector<std::string> wrong;
    if (!(l.worst_roll_after_touch_deg < 15.0)) {
        wrong.push_back(id + " banked " + std::to_string(l.worst_roll_after_touch_deg) +
                        " degrees after touching down" + where);
    }
    if (!(l.least_pitch_after_touch_deg > -10.0)) {
        wrong.push_back(id + "'s nose went " + std::to_string(l.least_pitch_after_touch_deg) +
                        " degrees down after touching down" + where);
    }
    if (!(l.highest_after_touch_ft < 3.0)) {
        wrong.push_back(id + " went " + std::to_string(l.highest_after_touch_ft) +
                        " ft back into the air after touching down" + where);
    }
    // **Within what her gear takes, and settled**: unwrecked by the server's
    // rule, and risen no more than half a foot after the touch.
    for (const std::string& gear : l.gear_took_it) {
        wrong.push_back(gear + where);
    }
    return wrong;
}

void stayed_down_and_upright(const std::string& id, const Landing& l, const std::string& where) {
    for (const std::string& wrong : wrong_after_the_touch(id, l, where)) {
        check(false, wrong);
    }
}

} // namespace

// **The item's own verification**, in calm air: from 5 nm out, down a
// 3-degree glidepath, landing within 5 m of the centreline, sinking under
// 300 ft/min, and stopping on the runway.
GLIDESLOPE_TEST(every_light_aircraft_is_flown_down_a_glidepath_and_lands_in_calm_air) {
    std::size_t walked = 0;
    for (const std::string& id : light_aircraft()) {
        const Landing l = land(id, 0.0);
        ++walked;
        check(l.touched, id + " never touched down");
        check(l.sink_fpm < 300.0,
              id + " touched down sinking " + std::to_string(l.sink_fpm) +
                  " ft/min, which is not under 300");
        check(std::abs(l.across_m) <= 5.0,
              id + " touched down " + std::to_string(l.across_m) +
                  " m from the centreline, which is not within 5");
        check(l.along_m >= 0.0 && l.along_m <= a_runway().length_m,
              id + " touched down " + std::to_string(l.along_m) +
                  " m along a " + std::to_string(a_runway().length_m) + " m runway");
        check(l.stopped, id + " did not stop within 15 minutes");
        check(l.stopped_along_m <= a_runway().length_m,
              id + " ran " + std::to_string(l.stopped_along_m) +
                  " m along a " + std::to_string(a_runway().length_m) + " m runway");
        stayed_down_and_upright(id, l, "");
    }
    check(walked == light_aircraft().size(),
          "every light aircraft was landed: " + std::to_string(walked) + " of " +
              std::to_string(light_aircraft().size()));
    check(walked == 4, "there are four light aircraft, not " + std::to_string(walked));
}

// **And in a 10-knot crosswind**, which is the other half of the item's
// verification.
GLIDESLOPE_TEST(every_light_aircraft_lands_on_the_centreline_in_a_ten_knot_crosswind) {
    std::size_t walked = 0;
    for (const std::string& id : light_aircraft()) {
        const Landing l = land(id, 10.0);
        ++walked;
        check(l.touched, id + " never touched down in the crosswind");
        check(l.sink_fpm < 300.0,
              id + " touched down sinking " + std::to_string(l.sink_fpm) +
                  " ft/min in the crosswind, which is not under 300");
        check(std::abs(l.across_m) <= 5.0,
              id + " touched down " + std::to_string(l.across_m) +
                  " m from the centreline in the crosswind, which is not within 5");
        check(l.stopped, id + " did not stop in the crosswind");
        check(l.stopped_along_m <= a_runway().length_m,
              id + " ran " + std::to_string(l.stopped_along_m) + " m in the crosswind");
        stayed_down_and_upright(id, l, " in the crosswind");
    }
    check(walked == 4, "all four light aircraft were landed in a crosswind");
}

// **On a wet runway in a 15 kt crosswind, she stays on it as she does on a
// dry one**: every light aircraft landed by the AI from five miles out with
// 15 kt straight across from the left, dry and on runway condition code 5,
// stops on the wet runway, the right way up, never farther from its
// centreline than the runway's half-width nor than 1 m beyond the farthest
// she went dry. Since 2026-10-14 her wheels keep, sideways, only the share
// of their dry grip a wet tyre keeps (sim/runway_condition.hpp).
//
// **The J-3 Cub is in it since 2026-10-29.** She was left out from
// 2026-10-10, ground-looping after the touch in 15 kt across; with her lift
// and wing drag off the crosswind (2026-10-15, tools/make_j3cub.py) she no
// longer does. Seen red with her model from just before that (a793ee39):
// "j3cub banked 15.303546 degrees after touching down on a wet runway" (16.0
// dry); and with 2026-10-14's (40045241) on her back, dry and wet.
GLIDESLOPE_TEST(every_light_aircraft_landed_on_a_wet_runway_in_a_15_kt_crosswind_stays_on_it_as_on_a_dry_one) {
    std::size_t walked = 0;
    std::size_t compared = 0;
    for (const std::string& id : light_aircraft()) {
        ++walked;
        const Landing dry = land(id, 15.0);
        const Landing wet = land(id, 15.0, true);
        std::printf("  %s: farthest across after the touch %.2f m dry, %.2f m wet\n", id.c_str(),
                    dry.worst_across_after_touch_m, wet.worst_across_after_touch_m);
        check(wet.stopped && wet.stopped_along_m <= a_runway().length_m,
              id + " stopped on the wet runway in the crosswind, " +
                  std::to_string(wet.stopped_along_m) + " m along");
        stayed_down_and_upright(id, wet, " on a wet runway in a 15 kt crosswind");
        // The dry landing it is compared with is a landing too.
        stayed_down_and_upright(id, dry, " on a dry runway in a 15 kt crosswind");
        check(wet.worst_across_after_touch_m <= glideslope::sim::Lander::runway_half_width_m,
              id + " went " + std::to_string(wet.worst_across_after_touch_m) +
                  " m from the centreline wet in the crosswind");
        check(dry.stopped && wet.worst_across_after_touch_m <= dry.worst_across_after_touch_m + 1.0,
              id + " went " + std::to_string(wet.worst_across_after_touch_m) +
                  " m from the centreline wet in the crosswind, against " +
                  std::to_string(dry.worst_across_after_touch_m) + " dry");
        ++compared;
    }
    check(walked == 4 && compared == 4,
          "every light aircraft landed dry and wet in the crosswind: " +
              std::to_string(walked) + " and " + std::to_string(compared));
}

// **The J-3 Cub is landed by the AI in every crosswind within her published
// limit, from either side, dry and wet.** Piper published none - she was
// certificated before a demonstrated crosswind was asked of any aeroplane -
// and the figure is one instructor's, in an EAA Inspire article: "the
// demonstrated crosswind component for the Piper J-3 Cub is about 12 knots
// at 90 degrees" (Steve Krog, "Crosswind Takeoffs 2.0", 2019). The owner
// accepted it as her limit on 2026-10-30; the status file of 2026-10-29
// lists what else was searched. From a mile out, every whole knot from 1 to
// 12 straight across, she touches within 5 m of the centreline, stops on the
// runway the right way up, never past its half-width, as the server judges
// her gear, and touches three-point (`not_three_point`). Both sides, as a wind from the left adds to her propeller's
// swing to the left and one from the right takes from it; dry and on runway
// condition code 5: four tests, one a side and runway. Coverage: 12 winds in
// each, counting only the landings that passed every check (seen to fail,
// 1 of 12, with the half-width cut to a metre; reverted). **Seen red, all
// four**, with the roll-out's rudder held central (lander.cpp): 3 of 12 dry
// and 6 of 12 wet from the left, 3 and 4 of 12 from the right, the first
// "the Cub went 34.255694 m from the centreline after the touch in 4 kt from
// the left, dry"; reverted.
namespace {

constexpr int cub_published_crosswind_kts = 12;

// **She is landed three-point** (assets/figures/j3cub.xml, `lands`): her
// nose as her wheels first touch within this of the attitude she stands at
// on all three. The FAA's Airplane Flying Handbook (FAA-H-8083-3C, ch. 14)
// has her touch on main wheels and tail wheel together at about the stall;
// her model's lift hardly grows above seven and a half degrees of incidence,
// and she touches on her main wheels a moment before her tail - in calm air
// at 10.2 degrees, where she stands at 12.1. Wheel-landed, as she was until
// 2026-11-01, she touched at 2.8.
constexpr double cub_three_point_margin_deg = 2.5;

std::vector<std::string> not_three_point(const Landing& l, const std::string& where) {
    if (l.touched && !(std::abs(l.touch_pitch_deg - l.standing_pitch_deg) <=
                       cub_three_point_margin_deg)) {
        return {"the Cub touched with her nose at " + std::to_string(l.touch_pitch_deg) +
                " degrees, where she stands at " + std::to_string(l.standing_pitch_deg) +
                where};
    }
    return {};
}

// From a mile out on the glidepath: the item is the touch and the roll-out,
// and a mile is a minute of her final, still settling on the line (1.7 to 2.1 m
// off it at 12 kt inside the mile) but touching as from 5 nm out.
constexpr double cub_start_nm = 1.0;

void the_cub_lands_in_every_crosswind_within_her_limit(double side, bool wet) {
    int landed = 0;
    std::vector<std::string> wrong;
    for (int kts = 1; kts <= cub_published_crosswind_kts; ++kts) {
        const Landing l = land("j3cub", side * kts, wet, cub_start_nm);
        const std::string where = " in " + std::to_string(kts) + " kt from the " +
                                  (side > 0.0 ? "left" : "right") + (wet ? ", wet" : ", dry");
        std::printf("  farthest across after the touch %.2f m\n", l.worst_across_after_touch_m);
        // Every check is gathered, not thrown at the first, so that the count
        // below counts only the landings that passed them all.
        std::vector<std::string> this_one = wrong_after_the_touch("j3cub", l, where);
        if (!l.touched) {
            this_one.push_back("the Cub never touched down" + where);
        }
        for (std::string& w : not_three_point(l, where)) {
            this_one.push_back(std::move(w));
        }
        if (!(std::abs(l.across_m) <= 5.0)) {
            this_one.push_back("the Cub touched down " + std::to_string(l.across_m) +
                               " m from the centreline" + where);
        }
        if (!l.stopped || !(l.stopped_along_m <= a_runway().length_m)) {
            this_one.push_back("the Cub did not stop on the runway" + where + ": " +
                               std::to_string(l.stopped_along_m) + " m along");
        }
        if (!(l.worst_across_after_touch_m <= glideslope::sim::Lander::runway_half_width_m)) {
            this_one.push_back("the Cub went " + std::to_string(l.worst_across_after_touch_m) +
                               " m from the centreline after the touch" + where);
        }
        if (this_one.empty()) {
            ++landed;
        }
        wrong.insert(wrong.end(), this_one.begin(), this_one.end());
    }
    std::string why;
    for (const std::string& w : wrong) {
        why += "\n    " + w;
    }
    check(landed == cub_published_crosswind_kts,
          "the Cub landed in every whole knot to her limit: " + std::to_string(landed) + " of " +
              std::to_string(cub_published_crosswind_kts) + why);
}

} // namespace

// **The AI lands the J-3 Cub three-point in calm air**, dry and wet: her nose
// within `cub_three_point_margin_deg` of the attitude she stands at as her
// wheels first touch, stopped on the runway the right way up. Every
// crosswind to her limit is held to the same, in the four tests below.
// Coverage: both runway conditions, counted.
GLIDESLOPE_TEST(the_j3_cub_is_landed_by_the_ai_three_point_in_calm_air_dry_and_wet) {
    int landed = 0;
    std::string why;
    for (const bool wet : {false, true}) {
        const Landing l = land("j3cub", 0.0, wet, cub_start_nm);
        const std::string where = wet ? " in calm air, wet" : " in calm air, dry";
        std::vector<std::string> wrong = wrong_after_the_touch("j3cub", l, where);
        if (!l.touched) {
            wrong.push_back("the Cub never touched down" + where);
        }
        for (std::string& w : not_three_point(l, where)) {
            wrong.push_back(std::move(w));
        }
        if (!l.stopped || !(l.stopped_along_m <= a_runway().length_m)) {
            wrong.push_back("the Cub did not stop on the runway" + where);
        }
        if (wrong.empty()) {
            ++landed;
        }
        for (const std::string& w : wrong) {
            why += "\n    " + w;
        }
    }
    check(landed == 2, "the Cub landed three-point dry and wet: " + std::to_string(landed) +
                           " of 2" + why);
}

GLIDESLOPE_TEST(the_j3_cub_is_landed_by_the_ai_on_a_dry_runway_in_every_crosswind_from_the_left_within_her_published_12_kt) {
    the_cub_lands_in_every_crosswind_within_her_limit(1.0, false);
}

GLIDESLOPE_TEST(the_j3_cub_is_landed_by_the_ai_on_a_wet_runway_in_every_crosswind_from_the_left_within_her_published_12_kt) {
    the_cub_lands_in_every_crosswind_within_her_limit(1.0, true);
}

GLIDESLOPE_TEST(the_j3_cub_is_landed_by_the_ai_on_a_dry_runway_in_every_crosswind_from_the_right_within_her_published_12_kt) {
    the_cub_lands_in_every_crosswind_within_her_limit(-1.0, false);
}

GLIDESLOPE_TEST(the_j3_cub_is_landed_by_the_ai_on_a_wet_runway_in_every_crosswind_from_the_right_within_her_published_12_kt) {
    the_cub_lands_in_every_crosswind_within_her_limit(-1.0, true);
}

// **The reference speed comes from the aeroplane's own published figures**,
// not from a number written here: a third above the stall in the landing
// configuration.
GLIDESLOPE_TEST(the_approach_speed_is_a_third_above_the_published_landing_stall) {
    std::size_t walked = 0;
    for (const std::string& id : light_aircraft()) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        const ApproachSpeeds speeds = glideslope::sim::approach_speeds(data(), entry.model);
        ++walked;
        check(speeds.vref_kts > 30.0 && speeds.vref_kts < 120.0,
              id + "'s reference speed is " + std::to_string(speeds.vref_kts) +
                  " knots, which is not a light aircraft's approach speed");
        check(speeds.flap >= 0.0 && speeds.flap <= 1.0,
              id + "'s landing flap is out of range");
    }
    check(walked == 4, "every light aircraft's approach speed was worked out");

    // An aircraft with no stall speed has no reference speed, and saying so
    // is better than guessing one. **This was the B-2A until 2026-09-23**,
    // when it got a stall speed measured from its own model
    // (docs/ASSETS.md). The F-22A is the example now: measured the same
    // way it came out anywhere between 128 and 149 knots depending only on
    // the speed the deceleration started from, so it has none, and no
    // approach speed is published for her. (The 747-400 was, until
    // 2026-11-01, when she was given Boeing's approach speed.)
    bool refused = false;
    try {
        (void)glideslope::sim::approach_speeds(data(), "f22");
    } catch (const std::runtime_error&) {
        refused = true;
    }
    check(refused, "an aircraft with no stall speed at all is refused");
}

// **Figures that land three-point an aeroplane that does not stand on a tail
// wheel are refused**, by the lander that would fly them (sim::Lander), for
// every aircraft in the catalogue: each given `lands="three_point"`'s flag,
// every nose-wheel aeroplane and hull refused, naming it, and the two that
// stand on a tail wheel - the J-3 Cub and the Mosquito - accepted. Coverage:
// the catalogue's size, all of it walked, and the two accepted named.
GLIDESLOPE_TEST(a_three_point_landing_is_refused_every_aircraft_in_the_catalogue_without_a_tail_wheel) {
    const auto catalogue = glideslope::sim::read_catalogue(data());
    std::size_t walked = 0;
    std::size_t refused = 0;
    std::vector<std::string> accepted;
    std::string why;
    for (const auto& entry : catalogue) {
        // The 747-400 and the F-22A publish no stall speed and have no
        // approach speeds worked out; theirs are the defaults, which the
        // refusal does not read.
        ApproachSpeeds speeds;
        try {
            speeds = glideslope::sim::approach_speeds(data(), entry.model);
        } catch (const std::runtime_error&) {
        }
        speeds.three_point = true;
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        // On final, her gear down, as the lander is given her: a retracting
        // gear's contacts stand where they are only once it is down.
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = a_runway().threshold_lat_deg;
        ic.longitude_deg = a_runway().threshold_lon_deg;
        ic.altitude_ft = 1000.0;
        ic.heading_deg = a_runway().heading_deg;
        ic.airspeed_kts = speeds.vref_kts > 0.0 ? speeds.vref_kts : 140.0;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        const bool tail_wheel = aircraft.stance().found && aircraft.stance().tail_wheel;
        bool was_refused = false;
        try {
            Lander lander(aircraft, a_runway(), speeds);
        } catch (const std::runtime_error& e) {
            was_refused = std::string(e.what()).find("three-point") != std::string::npos;
            if (!was_refused) {
                why += "\n    " + entry.id + " refused for another reason: " + e.what();
            }
        }
        ++walked;
        if (was_refused) {
            ++refused;
        } else {
            accepted.push_back(entry.id);
        }
        if (was_refused == tail_wheel) {
            why += "\n    " + entry.id + (tail_wheel ? " stands on a tail wheel and was refused"
                                                   : " has no tail wheel and was accepted");
        }
    }
    std::string named;
    for (const std::string& id : accepted) {
        named += " " + id;
    }
    check(walked == catalogue.size() && catalogue.size() >= 16,
          "every aircraft in the catalogue was walked: " + std::to_string(walked) + " of " +
              std::to_string(catalogue.size()));
    check(why.empty(), "a three-point landing refused each aircraft without a tail wheel:" + why);
    check(refused == catalogue.size() - 2 &&
              accepted == std::vector<std::string>{"j3cub", "mosquito-fb6"},
          "all but the two tail-wheel aeroplanes refused: " + std::to_string(refused) + " of " +
              std::to_string(catalogue.size()) + ", accepted:" + named);
}
