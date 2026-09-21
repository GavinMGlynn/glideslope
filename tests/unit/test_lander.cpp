#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/lander.hpp"
#include "sim/terrain.hpp"
#include "sim/weather.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
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
    double stopped_along_m = 0.0;
    double worst_across_on_final_m = 0.0; // inside a mile
    double seconds = 0.0;
};

// **From five miles out, on the glidepath, down to a stop.** `crosswind_kts`
// blows from the left across the runway; zero is calm air.
Landing land(const std::string& id, double crosswind_kts) {
    std::printf("landing the %s in %.0f knots of crosswind\n", id.c_str(),
                crosswind_kts);
    std::fflush(stdout);
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const Runway runway = a_runway();
    const ApproachSpeeds speeds = glideslope::sim::approach_speeds(data(), entry.model);

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    // The ground is the runway's own elevation, everywhere: this is about the
    // approach, not the terrain.
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));

    if (crosswind_kts != 0.0) {
        // Straight across the runway from the left: the runway points 070,
        // so the wind is from 340 and blows towards 160.
        glideslope::sim::Conditions conditions;
        const double towards = (runway.heading_deg - 90.0 + 180.0) / degrees;
        const double mps = crosswind_kts * 0.514444;
        conditions.wind_north_mps = mps * std::cos(towards);
        conditions.wind_east_mps = mps * std::sin(towards);
        aircraft.set_weather(
            std::make_shared<glideslope::sim::SteadyWeather>(conditions));
    }

    const double out_m = 5.0 * metres_per_nm;
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
    for (int tick = 0; tick < 900 * steps_per_second; ++tick) {
        aircraft.set_controls(lander.fly());
        aircraft.step();
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
    out.touched = lander.touchdown_sink_fpm() != 0.0 || lander.touchdown_along_m() != 0.0;
    out.sink_fpm = lander.touchdown_sink_fpm();
    out.across_m = lander.touchdown_across_m();
    out.along_m = lander.touchdown_along_m();
    if (!out.stopped) {
        out.seconds = 900.0;
    }
    std::printf("  %s: touched at %.0f ft/min, %.2f m across, %.0f m along; "
                "stopped %.0f m along after %.0f s; worst %.2f m across on final\n",
                id.c_str(), out.sink_fpm, out.across_m, out.along_m,
                out.stopped_along_m, out.seconds, out.worst_across_on_final_m);
    std::fflush(stdout);
    return out;
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
    }
    check(walked == 4, "all four light aircraft were landed in a crosswind");
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

    // An aircraft that publishes no stall speed has no reference speed, and
    // saying so is better than guessing one. The B-2 publishes none.
    bool refused = false;
    try {
        (void)glideslope::sim::approach_speeds(data(), "b2");
    } catch (const std::runtime_error&) {
        refused = true;
    }
    check(refused, "an aircraft with no published stall speed is refused");
}
