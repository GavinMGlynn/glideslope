#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/departure.hpp"
#include "sim/figures.hpp"
#include "sim/terrain.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using glideslope::sim::Departure;
using glideslope::sim::DepartureSpeeds;
using glideslope::sim::Runway;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;
constexpr double degrees = 180.0 / 3.14159265358979323846;
constexpr double feet_per_metre = 3.280839895013123;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

const std::vector<std::string>& light_aircraft() {
    static const std::vector<std::string> ids{"c172p", "c182", "pa28", "j3cub"};
    return ids;
}

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

// The aeroplane's published take-off ground roll in metres, or 0 where it
// publishes none - the Cub has no take-off figure at all.
double published_ground_roll_m(const std::string& id) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const auto figures = glideslope::sim::read_published_figures(
        data() / "figures" / (entry.model + ".xml"));
    for (const auto& spec : figures.figures) {
        if (spec.flight == "takeoff_ground_roll") {
            return spec.published * 0.3048; // its unit is feet
        }
    }
    return 0.0;
}

struct TakeOff {
    bool flew = false;
    double unstuck_m = 0.0;
    double worst_across_m = 0.0;
    double worst_across_on_the_ground_m = 0.0;
    double to_500_s = 0.0;
    double climb_kcas = 0.0;
};

// **Standing on the threshold, to five hundred feet.**
TakeOff take_off(const std::string& id) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const Runway runway = a_runway();
    const DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), entry.model);

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));

    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg;
    ic.longitude_deg = runway.threshold_lon_deg;
    ic.altitude_ft = runway.elevation_ft;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = 0.0;
    ic.engine_running = true;
    ic.gear = 1.0;
    aircraft.initialize(ic);

    Departure departure(aircraft, runway, speeds);
    TakeOff out;
    for (int tick = 0; tick < 300 * steps_per_second; ++tick) {
        aircraft.set_controls(departure.fly());
        aircraft.step();
        out.worst_across_m = std::max(out.worst_across_m, std::abs(departure.across_m()));
        if (departure.stage() == Departure::Stage::roll ||
            departure.stage() == Departure::Stage::rotate) {
            out.worst_across_on_the_ground_m =
                std::max(out.worst_across_on_the_ground_m, std::abs(departure.across_m()));
        }
        if (departure.stage() == Departure::Stage::done) {
            out.flew = true;
            out.to_500_s = static_cast<double>(tick) / steps_per_second;
            out.climb_kcas = aircraft.state().airspeed_kts;
            break;
        }
    }
    out.unstuck_m = departure.unstuck_along_m();
    std::printf("  %s: unstuck %.0f m (rotate %.1f kt%s), 500 ft in %.0f s at "
                "%.1f kt (best climb %.1f), worst %.2f m across, %.2f m on the ground\n",
                id.c_str(), out.unstuck_m, speeds.rotate_kts,
                speeds.rotate_is_published ? "" : ", worked from the stall",
                out.to_500_s, out.climb_kcas, speeds.climb_kts, out.worst_across_m,
                out.worst_across_on_the_ground_m);
    std::fflush(stdout);
    return out;
}

} // namespace

// **Every light aircraft takes itself off**, down the centreline and away.
GLIDESLOPE_TEST(every_light_aircraft_takes_itself_off_and_climbs_away) {
    std::size_t walked = 0;
    for (const std::string& id : light_aircraft()) {
        const TakeOff t = take_off(id);
        ++walked;
        check(t.flew, id + " did not reach 500 ft in five minutes");
        check(t.unstuck_m > 0.0, id + " never left the ground");
        // **Against the aeroplane's own published ground roll**, where it
        // publishes one. A take-off flown properly uses about the runway the
        // handbook says it will; one that uses half as much again is not
        // being flown properly, whatever else it manages.
        const double published_m = published_ground_roll_m(id);
        if (published_m > 0.0) {
            check(t.unstuck_m > 0.6 * published_m && t.unstuck_m < 1.35 * published_m,
                  id + " was airborne in " + std::to_string(t.unstuck_m) +
                      " m where its handbook says " + std::to_string(published_m));
        } else {
            check(t.unstuck_m < 400.0,
                  id + " publishes no ground roll and used " +
                      std::to_string(t.unstuck_m) + " m, which is too much for it");
        }
        // The nose is held on the centreline, which is the whole difficulty
        // of a take-off: a tail-wheel aeroplane swings as the power comes on.
        check(t.worst_across_on_the_ground_m <= 5.0,
              id + " wandered " + std::to_string(t.worst_across_on_the_ground_m) +
                  " m off the centreline on the ground");
    }
    check(walked == 4, "all four light aircraft took themselves off");
}

// **The speeds are the aeroplane's own.** The best climb speed is the one its
// published climb rate was measured at; the lift-off speed is the one its
// published take-off roll was measured at, or a seventh above its stall where
// it publishes no take-off roll - and it says which.
GLIDESLOPE_TEST(the_take_off_speeds_come_from_each_aircrafts_published_figures) {
    std::size_t published = 0;
    std::size_t worked_out = 0;
    for (const std::string& id : light_aircraft()) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        const DepartureSpeeds s = glideslope::sim::departure_speeds(data(), entry.model);
        check(s.rotate_kts > 20.0 && s.rotate_kts < 200.0,
              id + "'s rotation speed is " + std::to_string(s.rotate_kts) + " knots");
        check(s.climb_kts > s.rotate_kts * 0.8,
              id + " climbs at " + std::to_string(s.climb_kts) +
                  " but rotates at " + std::to_string(s.rotate_kts));
        check(s.flap >= 0.0 && s.flap <= 1.0, id + "'s take-off flap is out of range");
        (s.rotate_is_published ? published : worked_out) += 1;
    }
    // Three publish a take-off roll and so a lift-off speed; the Cub does
    // not, and its rotation speed is worked from its published stall. If
    // either number moves, this says so.
    check(published == 3, "three light aircraft publish a lift-off speed, not " +
                              std::to_string(published));
    check(worked_out == 1, "one has it worked from its stall, not " +
                               std::to_string(worked_out));

    // An aircraft that publishes no climb speed has nothing to climb at.
    bool refused = false;
    try {
        (void)glideslope::sim::departure_speeds(data(), "b2");
    } catch (const std::runtime_error&) {
        refused = true;
    }
    check(refused, "an aircraft with no published climb speed is refused");
}
