#include "after_touch.hpp"
#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/figures.hpp"
#include "sim/lander.hpp"
#include "sim/runway_condition.hpp"
#include "sim/terrain.hpp"
#include "sim/weather.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
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

// **A data directory whose F-22A has no stall figure**: her figures file,
// as committed, with its `stall_speed_landing` figure cut out, so the path
// an aircraft with no stall and no approach speed takes is still walked
// though no aircraft in the catalogue takes it. A name no other run shares;
// the caller removes it.
std::filesystem::path data_without_the_f22a_s_stall() {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("glideslope_no_stall_" + std::to_string(std::random_device{}()) + "_" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir / "figures");
    std::ifstream in(data() / "figures" / "f22.xml");
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto begin = text.find("<figure name=\"stall_speed_landing\"");
    const auto end = text.find("</figure>", begin);
    if (begin == std::string::npos || end == std::string::npos) {
        throw std::runtime_error("the F-22A's figures give no stall_speed_landing to take out");
    }
    text.erase(begin, end + std::string("</figure>").size() - begin);
    std::ofstream(dir / "figures" / "f22.xml") << text;
    return dir;
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
    // The step each of her first three gear units first carried weight, or
    // -1: for the J-3 Cub, her tail wheel and her two mains.
    std::array<long, 3> first_weight_step{-1, -1, -1};
    double strike_pitch_deg = 90.0;  // her nose where her tail strikes (Aircraft::stance)
    // On the flare's first step: how far her nose was short of the flare's
    // bound, degrees (`flared` says she did); and whether any step of her
    // flare was flown on her power (Lander::power_flare).
    bool flared = false;
    double flare_margin_deg = 0.0;
    bool power_flare = false;
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
             double out_nm = 5.0, const ApproachSpeeds* given = nullptr) {
    std::printf("landing the %s in %.0f knots of crosswind from the %s%s\n", id.c_str(),
                std::abs(crosswind_kts), crosswind_kts < 0.0 ? "right" : "left",
                wet ? ", wet" : "");
    std::fflush(stdout);
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const Runway runway = a_runway();
    const ApproachSpeeds speeds =
        given != nullptr ? *given : glideslope::sim::approach_speeds(data(), entry.model);

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
        const double pitch_deg = aircraft.state().pitch_deg;
        aircraft.set_controls(lander.fly());
        if (lander.stage() == Lander::Stage::flare && !out.flared) {
            out.flared = true;
            out.flare_margin_deg = lander.most_flare_pitch_deg() - pitch_deg;
        }
        out.power_flare = out.power_flare || lander.power_flare();
        aircraft.step();
        after.watch(aircraft);
        for (std::size_t g = 0; g < out.first_weight_step.size(); ++g) {
            const std::string wow = "gear/unit[" + std::to_string(g) + "]/WOW";
            if (out.first_weight_step[g] < 0 && aircraft.has_property(wow) &&
                aircraft.property(wow) > 0.5) {
                out.first_weight_step[g] = tick;
            }
        }
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
    out.strike_pitch_deg = aircraft.stance().strike_pitch_deg;
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

// **A round out from 1,200 ft/min at seven hundredths of a g takes 89 ft**:
// (20 ft/s)^2 / (2 x 0.07 x 32.174) = 88.80 - and a climb takes none. Written
// with `std::max(0.0, -climb_fpm) / 60.0` and squared, GCC 14.3.1 at -O2
// returned nought for every sink (docs/status, 2026-11-03), and the gusty
// flare began at the path's height whatever her sink: seen red so.
GLIDESLOPE_TEST(a_round_out_from_1200_fpm_at_seven_hundredths_of_a_g_takes_89_ft) {
    const double ft = glideslope::sim::round_out_height_ft(-1200.0, 0.07);
    std::printf("  1,200 ft/min at 0.07 g: %.3f ft\n", ft);
    check(std::abs(ft - 88.803) < 0.01, "88.80 ft, not " + std::to_string(ft));
    const double climbing = glideslope::sim::round_out_height_ft(300.0, 0.07);
    check(climbing == 0.0, "a climb takes none, not " + std::to_string(climbing));
}

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

// **The F-22A rolls out on the centreline, in calm air and 15 kt across**:
// landed by the AI from five miles out and stopped, she is never farther
// from the centreline after her first touch than the runway's half-width
// (0.85 m in calm air, 5.66 in 15 kt across), and stops on the
// runway the right way up. Her approach speed is not this test's: she has
// none yet, so she is flown down at 150 kt made by hand - inside the 165
// KCAS AFMAN 11-2F-22A volume 3 holds an instrument final approach to (its
// figures file) - at which she flies the approach well, flared to her
// figures' 200 ft/min; it is her roll-out that is pinned, and she is held
// upright (10 degrees of bank at most) but not to staying down. Until 2026-11-02
// her pedals reached her rudders on her wheels only through her flight
// controls' yaw rate integrator and its limits, a third of their travel at
// most: her roll-out's yaw swung wider every second and she left the
// runway - from `glideslope_cli land`'s gate 2 nm out 196 m across in calm
// air and 380 in 15 kt across, and here 117 m in 15 kt across. Her yaw
// loop now has a ground mode (tools/make_f22.py, `with_yaw_ground_mode`).
// **Seen to fail** on her model before it, and again on 2026-11-02 with her
// lander's yaw damper at her figures' 0.3 and only the ground mode taken
// out: 212 m across in 15 kt across. Coverage: two winds, both landed.
GLIDESLOPE_TEST(the_f22a_landed_at_150_kt_rolls_out_on_the_centreline_in_calm_air_and_15_kt_across) {
    ApproachSpeeds speeds;
    speeds.vref_kts = 150.0;
    speeds.stall_kts = 150.0 / 1.3;
    speeds.flap = 0.0;
    speeds.flare_ft = std::clamp(0.25 * speeds.vref_kts, 8.0, 30.0);
    speeds.touchdown_fpm = glideslope::sim::read_published_figures(
                               data() / "figures" / "f22.xml").touchdown_fpm;
    speeds.spool_s = glideslope::sim::spool_up_s(data() / "jsbsim", "f22");
    speeds.yaw_damper_per_degps =
        glideslope::sim::read_published_figures(data() / "figures" / "f22.xml")
            .yaw_rate_pedal_per_degps;
    std::size_t landed = 0;
    for (const double crosswind : {0.0, 15.0}) {
        const Landing l = land("f22", crosswind, false, 5.0, &speeds);
        std::printf("  %.0f kt across: touched %.0f ft/min %.2f m across, farthest after %.2f m, "
                    "stopped %.0f m along, rose %.2f ft\n",
                    crosswind, l.sink_fpm, l.across_m, l.worst_across_after_touch_m,
                    l.stopped_along_m, l.highest_after_touch_ft);
        const std::string where = " in " + std::to_string(crosswind) + " kt across";
        check(l.touched && l.stopped && l.stopped_along_m <= a_runway().length_m,
              "the F-22A stopped on the runway" + where);
        check(l.worst_across_after_touch_m <= Lander::runway_half_width_m,
              "the F-22A went " + std::to_string(l.worst_across_after_touch_m) +
                  " m from the centreline on her roll" + where + ", off the runway");
        // Upright, but not held to staying down: at 150 kt, 21 per cent over
        // 1.3 times her VS0, her wing still lifts her at the touch and she
        // rises 3.3 to 3.7 ft; at her own 123.8 she rises under half a foot,
        // which the test below pins.
        check(l.worst_roll_after_touch_deg <= 10.0,
              "the F-22A banked " + std::to_string(l.worst_roll_after_touch_deg) +
                  " degrees on her roll" + where);
        ++landed;
    }
    check(landed == 2, std::to_string(landed) + " of 2 winds landed");
}

// **The F-22A flies her final at 1.3 times her measured VS0 on the
// centreline, in calm air and 15 kt across either way**: her slowest
// steady flight landing configured, 90.0 KCAS at 54,960 lb
// (assets/figures/f22.xml), is 123.8 kt at her model's weight times 1.3,
// 17 degrees of incidence. Down the last mile from five out she is never
// farther than 5 m from the centreline, and rolls out on the runway the
// right way up, and never rises 0.75 ft after her touch (0.00 now).
// Her pedals command a yaw rate through her flight controls,
// and with the lander's yaw damper at every other aircraft's 0.05 a slow
// Dutch roll grew down her final: in calm air to 9.8 degrees of bank and
// 8.9 m off by 60 ft, and in 15 kt across she departed and struck the
// ground short. Her figures' `yaw_rate_pedal_per_degps` sets it. **Seen to
// fail** at 0.05. **And she touches within NASA's 360 ft/min** (Zaal et
// al., NTRS 20205010611), short of the lander's bound on her nose, two
// degrees under her tail strike: her nose has a quarter of a degree left to
// that bound as her flare begins, so it is flown on her power
// (sim/lander.cpp, least_flare_pitch_deg) - 176 to 202 ft/min. **Seen to
// fail** with the power flare taken out (least_flare_pitch_deg 0): 392 ft/min
// in calm air. Coverage: three
// winds, all landed.
namespace {

// **The F-22A's speeds at 1.3 times her measured VS0**, 90.0 KCAS at 54,960
// lb, at her model's own weight, at which `land` flies her and her plan
// speeds were measured (assets/figures/f22.xml, `plan_speeds`): her figures
// give no stall yet, so they are made here.
ApproachSpeeds f22_at_1_3_times_vs0() {
    const auto figures = glideslope::sim::read_published_figures(data() / "figures" / "f22.xml");
    constexpr double weight_lbs = 61570.0;
    ApproachSpeeds speeds;
    speeds.stall_kts = 90.0 * std::sqrt(weight_lbs / 54960.0);
    speeds.vref_kts = 1.3 * speeds.stall_kts;
    speeds.flap = 0.0;
    speeds.flare_ft = std::clamp(0.25 * speeds.vref_kts, 8.0, 30.0);
    speeds.touchdown_fpm = figures.touchdown_fpm;
    speeds.spool_s = glideslope::sim::spool_up_s(data() / "jsbsim", "f22");
    speeds.yaw_damper_per_degps = figures.yaw_rate_pedal_per_degps;
    return speeds;
}

} // namespace

GLIDESLOPE_TEST(the_f22a_lands_at_1_3_times_her_measured_vs0_on_the_centreline_within_360_ft_min_in_calm_air_and_15_kt_across) {
    const ApproachSpeeds speeds = f22_at_1_3_times_vs0();
    std::size_t landed = 0;
    for (const double crosswind : {0.0, 15.0, -15.0}) {
        const Landing l = land("f22", crosswind, false, 5.0, &speeds);
        std::printf("  %.0f kt across at %.1f kt: final within %.2f m, touched %.0f ft/min %.2f m "
                    "across, farthest after %.2f m, rose %.2f ft\n",
                    crosswind, speeds.vref_kts, l.worst_across_on_final_m, l.sink_fpm, l.across_m,
                    l.worst_across_after_touch_m, l.highest_after_touch_ft);
        const std::string where = " in " + std::to_string(crosswind) + " kt across";
        check(l.touched && l.worst_across_on_final_m <= 5.0,
              "the F-22A flew her final " + std::to_string(l.worst_across_on_final_m) +
                  " m from the centreline" + where + ", not within 5");
        check(l.stopped && l.stopped_along_m <= a_runway().length_m &&
                  l.worst_across_after_touch_m <= Lander::runway_half_width_m &&
                  l.worst_roll_after_touch_deg <= 10.0,
              "the F-22A rolled out on the runway, upright" + where);
        check(l.highest_after_touch_ft < 0.75,
              "the F-22A rose " + std::to_string(l.highest_after_touch_ft) +
                  " ft after her touch" + where + ", not under 0.75");
        check(l.sink_fpm > 0.0 && l.sink_fpm <= 360.0,
              "the F-22A touched at " + std::to_string(l.sink_fpm) + " ft/min" + where +
                  ", not within 360");
        check(l.touch_pitch_deg < l.strike_pitch_deg - 2.0,
              "the F-22A touched with her nose at " + std::to_string(l.touch_pitch_deg) +
                  " degrees" + where + ", not short of the lander's bound, " +
                  std::to_string(l.strike_pitch_deg - 2.0));
        check(l.flared && l.flare_margin_deg < 1.0 && l.power_flare,
              "the F-22A began her flare " + std::to_string(l.flare_margin_deg) +
                  " degrees short of her bound" + where + ", and flew it on her power");
        ++landed;
    }
    check(landed == 3, std::to_string(landed) + " of 3 winds landed");
}

// **A flare given back is never flown on her power**: the nose is the
// pilot's then (sim/lander.cpp, `resume`). The F-22A is flown by the AI at
// 1.3 times her measured VS0 from 2 nm to her flare's first step, which she
// begins within a degree of her bound - the situation, asserted - and takes
// on her power; a pilot then holds her controls as they were for half a
// second, the lander only watching, and gives her back. From there to her
// touch no step of her flare is flown on her power, and her throttle at the
// touch is under where the pilot gave it back. (The flare's own law, as
// before, opens it a little on the first step - 0.534 to 0.538 - her nose
// being out and her sink 300 ft/min past what it wants.) **Seen to fail**
// with `!resumed_` taken out of the gate: her power flare taken, and her
// throttle at the touch 0.629, over the 0.534 she was given back at.
GLIDESLOPE_TEST(the_f22a_given_back_in_her_flare_near_her_bound_closes_her_throttle_and_takes_no_power_flare) {
    const ApproachSpeeds speeds = f22_at_1_3_times_vs0();
    const auto entry = glideslope::sim::find_aircraft(data(), "f22");
    const Runway runway = a_runway();
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    const double out_m = 2.0 * metres_per_nm;
    const double heading = runway.heading_deg / degrees;
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg - out_m * std::cos(heading) /
                                                     metres_per_degree_latitude(runway.threshold_lat_deg);
    ic.longitude_deg = runway.threshold_lon_deg -
                       out_m * std::sin(heading) /
                           metres_per_degree_longitude(runway.threshold_lat_deg);
    ic.altitude_ft = runway.elevation_ft +
                     (out_m + speeds.aim_m) * std::tan(3.0 / degrees) * feet_per_metre;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = speeds.vref_kts;
    ic.engine_running = true;
    ic.gear = 1.0;
    aircraft.initialize(ic);
    Lander lander(aircraft, runway, speeds);
    glideslope::sim::Controls last;
    double margin_deg = 1e9;
    for (int tick = 0; tick < 300 * steps_per_second && lander.stage() != Lander::Stage::flare;
         ++tick) {
        const double pitch_deg = aircraft.state().pitch_deg;
        last = lander.fly();
        margin_deg = lander.most_flare_pitch_deg() - pitch_deg;
        aircraft.set_controls(last);
        aircraft.step();
    }
    check(lander.stage() == Lander::Stage::flare && margin_deg < 1.0 && lander.power_flare(),
          "she reached her flare " + std::to_string(margin_deg) +
              " degrees short of her bound, and took it on her power");
    const glideslope::sim::Controls pilot = last;
    for (int i = 0; i < steps_per_second / 2; ++i) {
        aircraft.set_controls(pilot);
        aircraft.step();
        lander.watch();
    }
    const double given_back_deg = lander.most_flare_pitch_deg() - aircraft.state().pitch_deg;
    lander.resume(pilot.throttle);
    bool on_power = false;
    double throttle = pilot.throttle;
    double first_throttle = -1.0;
    for (int tick = 0; tick < 60 * steps_per_second && lander.touchdown_sink_fpm() == 0.0;
         ++tick) {
        const glideslope::sim::Controls c = lander.fly();
        on_power = on_power || lander.power_flare();
        if (first_throttle < 0.0) {
            first_throttle = c.throttle;
        }
        throttle = c.throttle;
        aircraft.set_controls(c);
        aircraft.step();
    }
    std::printf("  given back %.2f degrees short of her bound at throttle %.3f: power flare %s, "
                "throttle %.3f on the first step; touched at %.0f ft/min, throttle %.3f\n",
                given_back_deg, pilot.throttle, on_power ? "TAKEN" : "not taken", first_throttle,
                lander.touchdown_sink_fpm(), throttle);
    check(given_back_deg < 1.0,
          "she was given back " + std::to_string(given_back_deg) + " degrees short of her bound");
    check(lander.touchdown_sink_fpm() != 0.0, "she touched after she was given back");
    check(!on_power, "her flare given back was not flown on her power");
    check(throttle < pilot.throttle,
          "her throttle came off from the " + std::to_string(pilot.throttle) +
              " she was given back at: " + std::to_string(first_throttle) +
              " on the first step, " + std::to_string(throttle) + " at the touch");
}

namespace {

struct FlareBegun {
    bool flared = false;
    double margin_deg = 0.0; // her bound less her nose, on the flare's first step
    bool power = false;      // any step of the flare flown on her power
};

// **Flown as `glideslope_cli land` flies her** (src/frontend/cli/main.cpp):
// 2 nm out on the glidepath, in calm air, at her approach speed for what she
// weighs, her landing flap out, trimmed on a 3-degree path, handed to the
// lander, to her touch.
FlareBegun flare_begun(const std::string& id) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const Runway runway = a_runway();
    const ApproachSpeeds speeds = glideslope::sim::approach_speeds(data(), entry.model);
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    const double out_m = 2.0 * metres_per_nm;
    const double heading = runway.heading_deg / degrees;
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg - out_m * std::cos(heading) /
                                                     metres_per_degree_latitude(runway.threshold_lat_deg);
    ic.longitude_deg = runway.threshold_lon_deg -
                       out_m * std::sin(heading) /
                           metres_per_degree_longitude(runway.threshold_lat_deg);
    ic.altitude_ft = runway.elevation_ft +
                     (out_m + speeds.aim_m) * std::tan(3.0 / degrees) * feet_per_metre;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts =
        glideslope::sim::for_weight(speeds, aircraft.loaded_weight_lbs()).vref_kts;
    ic.engine_running = true;
    ic.flaps = speeds.flap;
    ic.flight_path_deg = -3.0;
    ic.trim = true;
    aircraft.initialize(ic);
    Lander lander(aircraft, runway, speeds);
    FlareBegun out;
    for (int tick = 0; tick < 300 * steps_per_second && lander.touchdown_sink_fpm() == 0.0;
         ++tick) {
        const double pitch_deg = aircraft.state().pitch_deg;
        aircraft.set_controls(lander.fly());
        if (lander.stage() == Lander::Stage::flare && !out.flared) {
            out.flared = true;
            out.margin_deg = lander.most_flare_pitch_deg() - pitch_deg;
        }
        out.power = out.power || lander.power_flare();
        aircraft.step();
    }
    return out;
}

} // namespace

// **No other jet takes the power flare**: every jet in the catalogue
// (sim::AircraftFigures::jet) but the F-22A is flown as `glideslope_cli
// land` flies her, from 2 nm in calm air at her approach speed, and begins
// her flare more than `least_flare_pitch_deg`, a degree, short of her bound,
// and flies none of it on her power. The F-22A is the one that does, and is
// held to it above; she has no approach speed of her own yet. Coverage: the
// jets are counted from the catalogue, ten with the F-22A, and every one but
// her flared.
GLIDESLOPE_TEST(every_jet_but_the_f22a_begins_her_flare_more_than_a_degree_short_of_her_bound_and_takes_no_power_flare) {
    std::size_t jets = 0;
    std::size_t flared = 0;
    for (const auto& e : glideslope::sim::read_catalogue(data())) {
        const glideslope::sim::Aircraft probe(data() / "jsbsim", e.model);
        if (!probe.figures().jet) {
            continue;
        }
        ++jets;
        if (e.id == "f22") {
            continue;
        }
        const FlareBegun f = flare_begun(e.id);
        std::printf("  %-11s flare %s %.2f degrees short of her bound, %s\n", e.id.c_str(),
                    f.flared ? "begun" : "NEVER BEGUN", f.margin_deg,
                    f.power ? "ON HER POWER" : "not on her power");
        check(f.flared && f.margin_deg > 1.0 && !f.power,
              e.id + " began her flare " + std::to_string(f.margin_deg) +
                  " degrees short of her bound" + (f.power ? ", on her power" : ""));
        flared += f.flared ? 1U : 0U;
    }
    check(jets == 10 && flared == jets - 1,
          std::to_string(jets) + " jets, not 10, and " + std::to_string(flared) +
              " but the F-22A flared");
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
// on all three. The FAA's Airplane Flying Handbook (FAA-H-8083-3C, ch. 14,
// page 14-4) has her touch "the tailwheels at the same time as the main
// wheels". The margin is derived from her model's wheels, not published:
// unloaded, all three meet a level runway at atan(43.31 / 201.97) = 12.10
// degrees, her standing attitude; touching on her mains a little flatter,
// her tail wheel is 202 in x tan(the shortfall) up, and her main tyres'
// standing deflection, about 1.5 in, brings it down by 1.5 / 202 of a
// radian, 0.43 degrees, as they take her weight - so within it her tail
// wheel meets the runway with her mains. Tail first, steeper, the AFH
// allows "from no more than a foot off the surface"; the same margin holds
// that side. Until 2026-11-02 it was 2.5, fitted to the 10.2 degrees she
// touched at; she now touches at 11.82 to 11.90.
constexpr double cub_three_point_margin_deg = 0.43;

// **Her tail wheel with her mains**: on the runway within this of her first
// main wheel - with her mains in calm air, with her upwind main in a
// crosswind, as the AFH has it (ch. 14, "Crosswinds", page 14-5: "only one
// main wheel makes initial contact, either in concert with the tailwheel in
// three-point landings"). A tenth of a second is this project's judgement,
// not a published figure: her main tyres take 0.065 to 0.073 s to squash
// by their standing deflection, 1.5 to 1.7 in, at the 116 ft/min she
// touches at, and she touches within it (0.033 to 0.067 s, 4 to 8 steps).
// Held off half a foot, as she was until 2026-11-02, her tail came down
// well after (seen red: see the status file).
constexpr double cub_tail_wheel_within_s = 0.1;

std::vector<std::string> not_three_point(const Landing& l, const std::string& where) {
    std::vector<std::string> wrong;
    if (l.touched && !(std::abs(l.touch_pitch_deg - l.standing_pitch_deg) <=
                       cub_three_point_margin_deg)) {
        wrong.push_back("the Cub touched with her nose at " + std::to_string(l.touch_pitch_deg) +
                        " degrees, where she stands at " + std::to_string(l.standing_pitch_deg) +
                        where);
    }
    const long tail = l.first_weight_step[0];
    const long left = l.first_weight_step[1];
    const long right = l.first_weight_step[2];
    const long first_main = left < 0 ? right : right < 0 ? left : std::min(left, right);
    const double tail_after_s =
        static_cast<double>(tail - first_main) / static_cast<double>(steps_per_second);
    std::printf("  tail wheel %.3f s after her first main\n", tail_after_s);
    if (l.touched &&
        (tail < 0 || first_main < 0 || std::abs(tail_after_s) > cub_tail_wheel_within_s)) {
        wrong.push_back("the Cub's tail wheel met the runway " + std::to_string(tail_after_s) +
                        " s after her first main wheel, not within " +
                        std::to_string(cub_tail_wheel_within_s) + " s" + where);
    }
    return wrong;
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
    // (docs/ASSETS.md), the 747-400 until 2026-11-01 (Boeing's approach
    // speed), and the F-22A until 2026-11-02 (her slowest steady flight,
    // measured). No aircraft in the catalogue is without one now, so the
    // example is built: the F-22A's own figures with her stall taken out.
    const std::filesystem::path without = data_without_the_f22a_s_stall();
    bool refused = false;
    try {
        (void)glideslope::sim::approach_speeds(without, "f22");
    } catch (const std::runtime_error&) {
        refused = true;
    }
    check(!glideslope::sim::publishes_approach_speed(without, "f22") &&
              !glideslope::sim::landing_speeds(without, "f22").has_value() &&
              glideslope::sim::publishes_approach_speed(data(), "f22"),
          "her figures without her stall publish no approach speed and tell no landing; "
          "with it, they do");
    std::filesystem::remove_all(without);
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
        // Every aircraft has approach speeds worked out now; were one to
        // have none, hers would be the defaults, which the refusal does not
        // read.
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
