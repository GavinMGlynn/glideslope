#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/crash.hpp"
#include "sim/departure.hpp"
#include "sim/figures.hpp"
#include "sim/runway_condition.hpp"
#include "sim/lander.hpp"
#include "sim/takeoff_trial.hpp"
#include "sim/test_pilot.hpp"
#include "sim/terrain.hpp"
#include "sim/weather.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using glideslope::sim::Departure;
using glideslope::sim::DepartureSpeeds;
using glideslope::sim::Runway;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;

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

    // An aircraft with no climb speed at all has nothing to climb at.
    // **This was the B-2A until 2026-09-23**, and the 747-400 and F-22A until
    // 2026-10-06, when they were given take-off speeds measured from their
    // models. None in the data is without one now, so the example is the
    // 747-400's figures with its measured speeds taken out.
    const auto scratch = std::filesystem::path(GLIDESLOPE_TEST_DOWNLOADS_DIR) / "test-scratch" /
                         "no-climb-speed";
    std::filesystem::create_directories(scratch / "figures");
    {
        std::ifstream in(data() / "figures" / "747-400.xml", std::ios::binary);
        std::string text(std::istreambuf_iterator<char>(in), {});
        const auto from = text.find("<takeoff_speeds");
        const auto to = text.find("</takeoff_speeds>");
        check(from != std::string::npos && to != std::string::npos,
              "the 747-400's figures give its measured take-off speeds");
        text.erase(from, to + std::string("</takeoff_speeds>").size() - from);
        std::ofstream(scratch / "figures" / "747-400.xml", std::ios::binary) << text;
    }
    bool refused = false;
    try {
        (void)glideslope::sim::departure_speeds(scratch, "747-400");
    } catch (const std::runtime_error&) {
        refused = true;
    }
    check(refused, "an aircraft with no climb speed at all is refused");
}

// **Their `<takeoff_speeds>` are what the measurement makes**: the
// measurement `glideslope_cli takeoff-speeds` prints (sim::
// measure_takeoff_speeds, sought from 100 kt as the files say), run again for
// each aircraft whose speeds are measured, gives its file's rotation and
// climb within the 5 kt they are rounded to, its flap and its weight to the
// pound. A file edited by hand, or a model that has moved, fails here.
GLIDESLOPE_TEST(the_measured_take_off_speeds_in_the_747_and_f22_figures_are_what_the_measurement_makes) {
    std::size_t walked = 0;
    std::size_t matched = 0;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        if (entry.seaplane) {
            continue; // a flying boat's take-off is its published water take-off
        }
        const auto file = glideslope::sim::read_published_figures(data() / "figures" /
                                                                  (entry.model + ".xml"));
        if (file.measured_rotate_kcas <= 0.0) {
            continue;
        }
        ++walked;
        const glideslope::sim::MeasuredTakeoff m = glideslope::sim::measure_takeoff_speeds(
            data(), entry, 100.0, [](const std::string&) {});
        std::printf("  %s: measured rotate %.0f, climb %.0f (%.1f), flaps %.0f, %.0f lb; the "
                    "file %.0f, %.0f, %.0f, %.0f lb%s\n",
                    entry.id.c_str(), m.rotate_kts, m.climb_kts, m.slowest_away_kts, m.flaps_deg,
                    m.weight_lbs, file.measured_rotate_kcas, file.measured_climb_kcas,
                    file.measured_takeoff_flaps_deg, file.measured_takeoff_lbs,
                    m.why_not.empty() ? "" : (", " + m.why_not).c_str());
        std::fflush(stdout);
        const bool same = m.why_not.empty() &&
                          std::abs(m.rotate_kts - file.measured_rotate_kcas) < 5.0 &&
                          std::abs(m.climb_kts - file.measured_climb_kcas) < 5.0 &&
                          m.flaps_deg == file.measured_takeoff_flaps_deg &&
                          std::abs(m.weight_lbs - file.measured_takeoff_lbs) <= 1.0;
        check(same, entry.id + "'s <takeoff_speeds> are not what the measurement makes");
        matched += same ? 1U : 0U;
    }
    check(walked == 2 && matched == walked,
          "the 747-400's and the F-22A's measured, and each as its file says: " +
              std::to_string(matched) + " of " + std::to_string(walked));
}

// **The 747-400 and the F-22A take off at speeds measured from their own
// models** (`<takeoff_speeds>`, glideslope_cli takeoff-speeds; the owner's
// decision of 2026-10-06), and say so: flown at them, the take-off holds -
// lifted off by its rotation within 15 kt of it, inside the runway, never
// 5 kt slower once off, unwrecked, and at 500 ft no slower than 10 kt under
// its climb-away speed (sim::TakeoffFlown::held). Every aircraft whose speeds
// are measured is walked, and they must be those two.
GLIDESLOPE_TEST(the_747_and_the_f22_take_off_at_the_speeds_measured_from_their_models) {
    std::vector<std::string> measured;
    std::size_t held = 0;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        if (entry.seaplane) {
            continue; // a flying boat's take-off is its published water take-off
        }
        const DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), entry.model);
        if (!speeds.measured_from_model) {
            continue;
        }
        measured.push_back(entry.id);
        check(!speeds.rotate_is_published, entry.id + "'s measured rotation is called published");
        const glideslope::sim::TakeoffFlown t =
            glideslope::sim::fly_takeoff_trial(data(), entry, speeds);
        std::printf("  %s: rotate %.0f, climb %.0f kt: unstuck %.0f m at %.0f kt, slowest %.0f "
                    "airborne, %.0f kt at 500 ft%s%s\n",
                    entry.id.c_str(), speeds.rotate_kts, speeds.climb_kts, t.unstuck_m,
                    t.unstuck_kts, t.slowest_airborne_kts, t.handed_over_kts,
                    t.wrecked.empty() ? "" : ", wrecked: ", t.wrecked.c_str());
        std::fflush(stdout);
        if (t.held(speeds)) {
            ++held;
        }
    }
    check(measured == std::vector<std::string>{"747-400", "f22"},
          "the aircraft with measured take-off speeds are the 747-400 and the F-22A, not " +
              std::to_string(measured.size()) + " others");
    check(held == measured.size(), "each took off at its measured speeds: " +
                                       std::to_string(held) + " of " +
                                       std::to_string(measured.size()));
}

namespace {

// The Cherokee standing on the runway, or flying over it at `height_ft`
// and `kts` with `flaps` out.
std::unique_ptr<glideslope::sim::Aircraft> a_cherokee(double height_ft, double kts, double flaps) {
    const Runway runway = a_runway();
    auto aircraft = std::make_unique<glideslope::sim::Aircraft>(data() / "jsbsim", "pa28");
    aircraft->set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg;
    ic.longitude_deg = runway.threshold_lon_deg;
    ic.altitude_ft = runway.elevation_ft + height_ft;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = kts;
    ic.engine_running = true;
    ic.gear = 1.0;
    ic.flaps = flaps;
    aircraft->initialize(ic);
    return aircraft;
}

} // namespace

// **The flap lever's notches are its model's own**: the settings of the
// kinematic its flap command drives, as lever positions. Every aircraft in
// the catalogue is read; six are held to what their model files say. Every
// model's first setting is 0 - none here has a flap whose lowest detent is
// not up - so a first setting other than 0 is exercised by no model.
GLIDESLOPE_TEST(the_flap_levers_notches_are_the_settings_of_each_models_flap_kinematic) {
    const std::map<std::string, std::vector<double>> expected{
        {"737-300", {0.0, 1.0 / 40, 2.0 / 40, 5.0 / 40, 10.0 / 40, 15.0 / 40, 25.0 / 40, 30.0 / 40, 1.0}},
        {"a320", {0.0, 1.0 / 40, 2.0 / 40, 5.0 / 40, 10.0 / 40, 15.0 / 40, 25.0 / 40, 30.0 / 40, 1.0}},
        {"c172p", {0.0, 1.0 / 3, 2.0 / 3, 1.0}},
        {"pa28", {0.0, 0.25, 0.625, 1.0}},
        {"learjet35a", {0.0, 0.2, 0.5, 1.0}},
        {"mosquito-fb6", {0.0, 1.0}},
    };
    std::size_t walked = 0;
    std::size_t held = 0;
    std::string failures;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        const std::vector<double>& notches = aircraft.flap_notches();
        std::string listed;
        for (const double n : notches) {
            listed += " " + std::to_string(n);
        }
        std::printf("  %s:%s\n", entry.id.c_str(), listed.empty() ? " none" : listed.c_str());
        ++walked;
        // Up and fully down among them, in order, wherever there are any.
        if (!notches.empty() &&
            (notches.front() != 0.0 || notches.back() != 1.0 ||
             !std::is_sorted(notches.begin(), notches.end()))) {
            failures += "\n  " + entry.id + "'s notches do not run from 0 to 1 in order:" + listed;
        }
        const auto want = expected.find(entry.id);
        if (want == expected.end()) {
            continue;
        }
        ++held;
        bool same = want->second.size() == notches.size();
        for (std::size_t i = 0; same && i < notches.size(); ++i) {
            same = std::abs(want->second[i] - notches[i]) < 1e-12;
        }
        if (!same) {
            failures += "\n  " + entry.id + "'s notches are" + listed;
        }
    }
    check(walked == 16 && held == expected.size(),
          "all 16 aircraft read, and the six held: " + std::to_string(walked) + ", " +
              std::to_string(held));
    check(failures.empty(), "each lever's notches as its model gives them:" + failures);
}

// **A take-off that never makes the speed its flap comes up at still ends**,
// with its flap still out. Built explicitly: the Cherokee asked to climb
// away at 140 KCAS, past her top speed, with her 25-degree take-off flap,
// and to end the take-off at 20 ft. Her flap would come up only at 138
// KCAS, which she never reaches; without a way out the take-off climbs on
// for ever. Flown so, she climbs on and it ends at 1,500 ft above the
// runway, where the take-off path ends (14 CFR 25.111(a)). Flown again with
// her throttle held to six tenths from 300 ft - short of thrust - she holds
// about 300 ft and climbs no higher, and it ends five minutes after the
// throttle opened
// (14 CFR 1.1's limit on take-off power). Both times the lever never left
// the take-off flap.
GLIDESLOPE_TEST(a_take_off_that_never_makes_its_flap_speed_ends_with_the_flap_still_out) {
    const auto fly = [](double most_throttle_off, const std::string& how) {
        auto aircraft = a_cherokee(0.0, 0.0, 0.0);
        DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), "pa28");
        speeds.climb_kts = 140.0;
        speeds.initial_climb_kts = 140.0;
        speeds.reference_lbs = 0.0; // asked for 140 whatever she weighs
        Departure departure(*aircraft, a_runway(), speeds, 20.0);
        int steps = 0;
        double fastest_kcas = 0.0;
        double lowest_lever = 1.0;
        for (; steps < 10 * 60 * steps_per_second; ++steps) {
            glideslope::sim::Controls c = departure.fly();
            if (departure.above_m() * 3.280839895013123 > 300.0) {
                c.throttle = std::min(c.throttle, most_throttle_off);
            }
            lowest_lever = std::min(lowest_lever, c.flaps);
            aircraft->set_controls(c);
            aircraft->step();
            fastest_kcas = std::max(fastest_kcas, aircraft->state().airspeed_kts);
            if (departure.stage() == Departure::Stage::done) {
                break;
            }
        }
        const double ended_ft = departure.above_m() * 3.280839895013123;
        std::printf("  %s: ended after %d s at %.0f ft, never faster than %.1f KCAS, the lever "
                    "never below %.3f\n",
                    how.c_str(), steps / steps_per_second, ended_ft, fastest_kcas, lowest_lever);
        check(departure.stage() == Departure::Stage::done && steps <= 300 * steps_per_second,
              how + ": the take-off ended within five minutes: after " +
                  std::to_string(steps / steps_per_second) + " s");
        check(fastest_kcas < speeds.initial_climb_kts - 2.0 && lowest_lever == speeds.flap,
              how + ": the flap stayed at the take-off flap, the speed never at the flap's: "
                    "lever at least " + std::to_string(lowest_lever) + ", at most " +
                  std::to_string(fastest_kcas) + " KCAS");
        return std::pair{steps, ended_ft};
    };
    const auto [climbing_steps, climbing_ft] = fly(1.0, "at full throttle");
    check(climbing_ft >= 1500.0 && climbing_steps < 300 * steps_per_second,
          "at full throttle it ended at 1,500 ft: " + std::to_string(climbing_ft));
    const auto [short_steps, short_ft] = fly(0.6, "at six tenths throttle from 300 ft");
    check(short_ft < 1500.0 && short_steps >= 300 * steps_per_second - 1,
          "at six tenths throttle from 300 ft it ended at five minutes, below 1,500 ft: " +
              std::to_string(short_ft) + " ft, " + std::to_string(short_steps) + " steps");
}

// **Taken over in the air, the take-off keeps the flap it found**: the
// Cherokee flying at 300 ft and 70 KCAS with her flaps up, given to a
// take-off whose flap is 25 degrees, never has her lever put down - a
// take-off handed back mid-climb once put the take-off flap out again.
GLIDESLOPE_TEST(a_take_off_taken_over_in_the_air_never_puts_out_a_flap_it_found_up) {
    auto aircraft = a_cherokee(300.0, 70.0, 0.0);
    const DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), "pa28");
    check(speeds.flap > 0.0, "the Cherokee takes off with flap: " + std::to_string(speeds.flap));
    Departure departure(*aircraft, a_runway(), speeds);
    double most_lever = 0.0;
    int steps = 0;
    for (; steps < 120 * steps_per_second && departure.stage() != Departure::Stage::done;
         ++steps) {
        const glideslope::sim::Controls c = departure.fly();
        most_lever = std::max(most_lever, c.flaps);
        aircraft->set_controls(c);
        aircraft->step();
    }
    std::printf("  the lever at most %.3f in %d s, the flaps at %.2f deg at the end\n", most_lever,
                steps / steps_per_second, aircraft->property("fcs/flap-pos-deg"));
    check(most_lever == 0.0, "the lever stayed up: at most " + std::to_string(most_lever));
    check(departure.stage() == Departure::Stage::done,
          "the take-off ended: " + std::to_string(steps / steps_per_second) + " s");
}

// **A take-off on a wet runway rolls as it does on a dry one**: every
// aircraft that takes off from a runway (the flying boat named) takes off by
// the take-off autopilot at its own speeds, dry and on runway condition code
// 5, and leaves the ground within 1 per cent and 2 m of where it left dry,
// and wanders no more than 0.5 m farther from the centreline. A wet runway's
// water (3 mm or less, AC 25-32's code 5) takes grip from a braked wheel,
// not from a free-rolling one's rolling or its steering, which the wet
// runway's braking coefficient is not a measure of.
GLIDESLOPE_TEST(every_aircraft_takes_off_from_a_wet_runway_as_from_a_dry_one) {
    const auto catalogue = glideslope::sim::read_catalogue(data());
    std::size_t flown = 0;
    std::size_t left_out = 0;
    glideslope::sim::Conditions wet;
    wet.runway_condition = glideslope::sim::wet_runway;
    for (const auto& entry : catalogue) {
        if (entry.seaplane) {
            std::printf("  left out - %s: a flying boat takes off from water\n", entry.id.c_str());
            ++left_out;
            continue;
        }
        const DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), entry.model);
        const glideslope::sim::TakeoffFlown dry =
            glideslope::sim::fly_takeoff_trial(data(), entry, speeds);
        const glideslope::sim::TakeoffFlown on_wet = glideslope::sim::fly_takeoff_trial(
            data(), entry, speeds, std::make_shared<glideslope::sim::SteadyWeather>(wet));
        std::printf("  %-13s dry: unstuck %6.1f m, %.2f m across; wet: unstuck %6.1f m, %.2f m "
                    "across\n",
                    entry.id.c_str(), dry.unstuck_m, dry.widest_across_m, on_wet.unstuck_m,
                    on_wet.widest_across_m);
        check(dry.unstuck_m > 0.0 && on_wet.unstuck_m > 0.0 && on_wet.wrecked.empty(),
              entry.id + " left the ground dry and wet, unwrecked");
        check(std::abs(on_wet.unstuck_m - dry.unstuck_m) <= std::max(2.0, 0.01 * dry.unstuck_m),
              entry.id + " left a wet runway at " + std::to_string(on_wet.unstuck_m) +
                  " m, against " + std::to_string(dry.unstuck_m) + " dry");
        check(on_wet.widest_across_m <= dry.widest_across_m + 0.5,
              entry.id + " wandered " + std::to_string(on_wet.widest_across_m) +
                  " m from the centreline wet, against " + std::to_string(dry.widest_across_m) +
                  " dry");
        ++flown;
    }
    check(flown + left_out == catalogue.size() && left_out == 1,
          "every aircraft but the flying boat taken off dry and wet: " + std::to_string(flown) +
              " of " + std::to_string(catalogue.size() - 1));
}

namespace {

// **A take-off stopped at 20 kt**: an aircraft on the take-off trial's
// threshold, run up by the take-off autopilot until her wheels roll at 20
// kt, then her throttle closed and both brakes fully on, steered as the
// take-off autopilot steers - a tailwheel aeroplane's stick held fully
// back and her brakes half on - to a stop - on `conditions`' runway, with her
// own tyres and brakes or `fitted`. How far she ran braked, and whether at
// every braked step the runway gave her wheels what her tyres and anti-skid
// read on it (Aircraft::runway_braking_coefficient) - none where dry.
struct Stopped {
    double braked_m = 0.0;
    std::string wrecked;
    bool stopped = false;
    bool read_for_her_brakes = true;
    double least_share = 1.0;
    // Braked steps on which the brakes were given the pedal (the share above
    // it) and the share (the share below it), and whether every braked step
    // gave the brakes exactly the less of the two.
    std::size_t given_the_pedal = 0;
    std::size_t given_the_share = 0;
    bool the_less_of_the_two = true;
};

Stopped stopped_from_20_kt(const glideslope::sim::CatalogueEntry& entry,
                           const glideslope::sim::Conditions& conditions,
                           std::optional<glideslope::sim::WheelBrakes> fitted = std::nullopt,
                           double pedal = 1.0) {
    using glideslope::sim::Aircraft;
    const DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), entry.model);
    Runway runway = a_runway();
    Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(conditions));
    if (fitted) {
        aircraft.set_wheel_brakes(*fitted);
    }
    const glideslope::sim::WheelBrakes brakes = *aircraft.wheel_brakes();
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
    glideslope::sim::GroundJudge judge(false);
    Stopped out;
    bool braking = false;
    constexpr double fps_per_kt = 1.68781;
    for (int step = 0; step < 300 * steps_per_second; ++step) {
        const double kts = aircraft.property("velocities/vg-fps") / fps_per_kt;
        if (!braking && kts >= 20.0) {
            braking = true;
        }
        if (braking && kts < 0.5) {
            out.stopped = true;
            break;
        }
        glideslope::sim::Controls c = departure.fly();
        if (braking) {
            c.throttle = 0.0;
            c.throttle_offset = {};
            c.left_brake = pedal;
            c.right_brake = pedal;
            // A tailwheel aeroplane braked holds her stick fully back, and
            // her pedals only half on: fully on, the Mosquito stands on her
            // nose even so.
            if (departure.tail_wheel()) {
                c.elevator = 1.0;
                c.left_brake = std::min(pedal, 0.5);
                c.right_brake = std::min(pedal, 0.5);
            }
        }
        aircraft.set_controls(c);
        aircraft.step();
        if (std::optional<std::string> why = judge.judge(aircraft)) {
            out.wrecked = *why;
            break;
        }
        if (braking) {
            out.braked_m += kts * fps_per_kt * 0.3048 / steps_per_second;
            const double given = aircraft.runway_braking_coefficient();
            const bool read =
                conditions.runway_condition == glideslope::sim::dry_runway
                    ? std::isnan(given)
                    : given == glideslope::sim::wheel_braking_coefficient(
                                   conditions.runway_condition, kts, brakes);
            out.read_for_her_brakes = out.read_for_her_brakes && read;
            out.least_share = std::min(out.least_share, aircraft.brake_share());
            const double asked = c.left_brake;
            const double share = aircraft.brake_share();
            const double commanded = aircraft.property("fcs/left-brake-cmd-norm");
            out.the_less_of_the_two =
                out.the_less_of_the_two && commanded == std::min(asked, share);
            if (share > asked) {
                ++out.given_the_pedal;
            } else {
                ++out.given_the_share;
            }
        }
    }
    return out;
}

} // namespace

// **Each aircraft's wet stopping is her own tyres' and brakes'**: every
// landplane (the flying boat named), stopped from 20 kt on a wet runway with
// her brakes fully on, is given at every braked step the coefficient AC
// 25-32 and 25.109(c) read for her catalogue's tyre pressure and anti-skid,
// and her pedals the share of it they take; and fitted instead with the
// tyre every aircraft was given before - 100 psi, fully modulating - she
// stops sooner where that tyre grips more at 10 kt and later where it grips
// less. A wet runway stops her later than a dry one.
GLIDESLOPE_TEST(each_aircraft_stops_on_a_wet_runway_on_her_own_tyres_and_brakes) {
    const auto catalogue = glideslope::sim::read_catalogue(data());
    glideslope::sim::Conditions dry;
    glideslope::sim::Conditions wet;
    wet.runway_condition = glideslope::sim::wet_runway;
    std::size_t stopped = 0;
    std::size_t left_out = 0;
    for (const auto& entry : catalogue) {
        if (entry.seaplane) {
            std::printf("  left out - %s: a flying boat has no wheels to brake\n",
                        entry.id.c_str());
            ++left_out;
            continue;
        }
        const glideslope::sim::WheelBrakes own = *entry.brakes;
        const Stopped on_dry = stopped_from_20_kt(entry, dry);
        const Stopped on_wet = stopped_from_20_kt(entry, wet);
        const Stopped reference =
            stopped_from_20_kt(entry, wet, glideslope::sim::reference_wheel_brakes);
        const double mu_own = glideslope::sim::wet_wheel_braking_coefficient(10.0, own);
        const double mu_reference = glideslope::sim::wet_wheel_braking_coefficient(
            15.0, glideslope::sim::reference_wheel_brakes);
        std::printf("  %-13s %5.0f psi %-12s mu(10 kt) %.3f against %.3f: dry %6.1f m, wet "
                    "%6.1f m (share %.2f), 100 psi modulating %6.1f m\n",
                    entry.id.c_str(), own.tyre_pressure_psi,
                    own.anti_skid == glideslope::sim::AntiSkid::none ? "no anti-skid"
                                                                     : "anti-skid",
                    mu_own, mu_reference, on_dry.braked_m, on_wet.braked_m, on_wet.least_share,
                    reference.braked_m);
        check(on_dry.stopped && on_wet.stopped && reference.stopped && on_dry.wrecked.empty() &&
                  on_wet.wrecked.empty() && reference.wrecked.empty(),
              entry.id + " stopped, unwrecked, every time: " + on_dry.wrecked + on_wet.wrecked +
                  reference.wrecked);
        check(on_wet.read_for_her_brakes && on_dry.read_for_her_brakes &&
                  reference.read_for_her_brakes,
              entry.id + "'s wheels were given what the runway gives her tyres and brakes");
        check(on_wet.least_share < 1.0,
              entry.id + "'s brakes were held back on the wet runway: share " +
                  std::to_string(on_wet.least_share));
        check(on_wet.braked_m > on_dry.braked_m,
              entry.id + " stopped in " + std::to_string(on_wet.braked_m) + " m wet, against " +
                  std::to_string(on_dry.braked_m) + " dry");
        check(mu_own < mu_reference ? on_wet.braked_m > reference.braked_m
                                    : on_wet.braked_m < reference.braked_m,
              entry.id + " on her own tyres and brakes stopped in " +
                  std::to_string(on_wet.braked_m) + " m, on 100 psi fully modulating " +
                  std::to_string(reference.braked_m));
        ++stopped;
    }
    check(stopped + left_out == catalogue.size() && left_out == 1,
          "every landplane stopped: " + std::to_string(stopped) + " of " +
              std::to_string(catalogue.size() - 1));
}

// **An aeroplane without anti-skid stops later on a wet runway than with
// one, all else equal**: every landplane whose catalogue says her brakes
// have no anti-skid, stopped from 20 kt on a wet runway, runs farther than
// the same aeroplane on the same tyres with a fully modulating anti-skid -
// and on a dry runway, where her model's own friction is what she grips,
// exactly as far. The count of them is asserted.
GLIDESLOPE_TEST(an_aeroplane_without_anti_skid_stops_later_on_a_wet_runway_than_with_it) {
    const auto catalogue = glideslope::sim::read_catalogue(data());
    glideslope::sim::Conditions dry;
    glideslope::sim::Conditions wet;
    wet.runway_condition = glideslope::sim::wet_runway;
    std::size_t without = 0;
    std::size_t with = 0;
    for (const auto& entry : catalogue) {
        if (entry.seaplane) {
            continue;
        }
        if (entry.brakes->anti_skid != glideslope::sim::AntiSkid::none) {
            ++with;
            continue;
        }
        glideslope::sim::WheelBrakes modulating = *entry.brakes;
        modulating.anti_skid = glideslope::sim::AntiSkid::fully_modulating;
        const Stopped wet_none = stopped_from_20_kt(entry, wet);
        const Stopped wet_modulating = stopped_from_20_kt(entry, wet, modulating);
        const Stopped dry_none = stopped_from_20_kt(entry, dry);
        const Stopped dry_modulating = stopped_from_20_kt(entry, dry, modulating);
        std::printf("  %-13s wet: %6.1f m without anti-skid, %6.1f m with; dry %6.1f and %6.1f\n",
                    entry.id.c_str(), wet_none.braked_m, wet_modulating.braked_m,
                    dry_none.braked_m, dry_modulating.braked_m);
        check(wet_none.stopped && wet_modulating.stopped && wet_none.wrecked.empty() &&
                  wet_modulating.wrecked.empty(),
              entry.id + " stopped, unwrecked");
        check(wet_none.braked_m > wet_modulating.braked_m,
              entry.id + " without anti-skid stopped in " + std::to_string(wet_none.braked_m) +
                  " m wet, with it " + std::to_string(wet_modulating.braked_m));
        check(dry_none.braked_m == dry_modulating.braked_m,
              entry.id + " dry: " + std::to_string(dry_none.braked_m) + " and " +
                  std::to_string(dry_modulating.braked_m));
        ++without;
    }
    check(without + with + 1 == catalogue.size() && without == 5,
          "every landplane without anti-skid stopped both ways: " + std::to_string(without) +
              ", and " + std::to_string(with) + " with one");
}

// **The runway's share is a ceiling on the pedals, not a scale**: every
// landplane (the flying boat named), stopped from 20 kt with her pedals a
// fifth of the way down, is given at every braked step exactly the less of
// the pedal and the runway's share - on a wet runway, where her share is
// above a fifth, the pedal as pressed; on ice (code 1, AC 25-32's 0.083),
// where it is below, the share. Scaled, as the brakes were until
// 2026-10-13, a light pedal on a wet runway lost grip the runway would have
// given it.
GLIDESLOPE_TEST(a_light_pedal_on_a_slippery_runway_is_given_as_pressed_up_to_the_runways_share) {
    const auto catalogue = glideslope::sim::read_catalogue(data());
    glideslope::sim::Conditions wet;
    wet.runway_condition = glideslope::sim::wet_runway;
    glideslope::sim::Conditions ice;
    ice.runway_condition = glideslope::sim::most_slippery_runway;
    constexpr double pedal = 0.2;
    std::size_t flown = 0;
    std::size_t left_out = 0;
    for (const auto& entry : catalogue) {
        if (entry.seaplane) {
            std::printf("  left out - %s: a flying boat has no wheels to brake\n",
                        entry.id.c_str());
            ++left_out;
            continue;
        }
        const Stopped on_wet = stopped_from_20_kt(entry, wet, std::nullopt, pedal);
        const Stopped on_ice = stopped_from_20_kt(entry, ice, std::nullopt, pedal);
        std::printf("  %-13s wet: %zu steps given the pedal, %zu the share (least %.3f); ice: %zu "
                    "and %zu (least %.3f)\n",
                    entry.id.c_str(), on_wet.given_the_pedal, on_wet.given_the_share,
                    on_wet.least_share, on_ice.given_the_pedal, on_ice.given_the_share,
                    on_ice.least_share);
        // (On ice a light aeroplane's idle propeller out-pulls a fifth of
        // her brakes - the C182S rolled the whole 300 s - so only the wet
        // runway is asked to stop her.)
        check(on_wet.stopped && on_wet.wrecked.empty() && on_ice.wrecked.empty(),
              entry.id + " stopped wet, unwrecked: " + on_wet.wrecked + on_ice.wrecked);
        check(on_wet.the_less_of_the_two && on_ice.the_less_of_the_two,
              entry.id + "'s brakes were given the less of the pedal and the share");
        check(on_wet.given_the_pedal > 0 && on_wet.given_the_share == 0,
              entry.id + " on a wet runway was given the pedal as pressed: " +
                  std::to_string(on_wet.given_the_pedal) + " steps, and the share " +
                  std::to_string(on_wet.given_the_share));
        check(on_ice.given_the_share > 0,
              entry.id + " on ice was given the share below the pedal: " +
                  std::to_string(on_ice.given_the_share) + " steps");
        ++flown;
    }
    check(flown + left_out == catalogue.size() && left_out == 1,
          "every landplane braked lightly: " + std::to_string(flown) + " of " +
              std::to_string(catalogue.size() - 1));
}

namespace {

// One aircraft standing on the runway with her brakes on for `seconds`, her
// throttle at `throttle` and her reverse lever at `reverse`: each engine's
// force at the end, pounds, forward positive - JSBSim's thrust property is
// the thruster's, after the reverser's cosine (FGThruster::Calculate) - and
// how long the engines took to reach nine tenths of their reverse.
struct GroundRun {
    std::vector<double> thrust_lbs;
    double to_most_reverse_s = 0.0;
    glideslope::sim::AircraftState state;
    bool says_reversers = false;
};

// **Standing on her brakes as a pilot stands her**: all of the pedals - and
// for a tail-wheel aeroplane, her stick fully back and her pedals half on, as
// the wet stopping test above brakes her. Braked fully with the stick
// central, from the level start JSBSim gives her and her engine still at
// 1,790 rpm, the J-3 Cub was on the edge of going over on her nose: on Linux
// she rocked back on to her tail, built with fused multiply-adds (as macOS's
// arm64 is, and Linux with -mfma) she went over - 47 degrees nose down
// after two seconds - and her flight then blew up to NaN (CI runs
// 38443070583 and 38447161811; reproduced 2026-10-14 with -mfma
// -ffp-contract=fast).
void stand_on_her_brakes(const glideslope::sim::Aircraft& aircraft,
                         glideslope::sim::Controls& c) {
    const bool tail_wheel = aircraft.stance().tail_wheel;
    c.elevator = tail_wheel ? 1.0 : 0.0;
    c.left_brake = tail_wheel ? 0.5 : 1.0;
    c.right_brake = c.left_brake;
}

// **Every number in a state, by name**, for comparing two to the last bit
// and finding a NaN: AircraftState's own == takes a NaN as unequal to itself,
// so two identical flights with a NaN anywhere in them compare unequal (as
// the J-3 Cub's did on macOS, 2026-10-14).
std::vector<std::pair<const char*, double>> fields_of(const glideslope::sim::AircraftState& s) {
    return {{"time", s.sim_time_s},
            {"latitude", s.latitude_deg},
            {"longitude", s.longitude_deg},
            {"altitude", s.altitude_ft},
            {"height", s.height_above_ground_ft},
            {"terrain", s.terrain_elevation_ft},
            {"roll", s.roll_deg},
            {"pitch", s.pitch_deg},
            {"heading", s.heading_deg},
            {"u", s.u_fps},
            {"v", s.v_fps},
            {"w", s.w_fps},
            {"p", s.p_radps},
            {"q", s.q_radps},
            {"r", s.r_radps},
            {"airspeed", s.airspeed_kts},
            {"climb", s.climb_rate_fpm},
            {"rpm", s.engine_rpm},
            {"on water", s.on_water ? 1.0 : 0.0},
            {"ditched", s.ditched ? 1.0 : 0.0}};
}

// Each field two states differ in, both values to the last bit; empty where
// they are the same bit for bit.
std::string differences(const glideslope::sim::AircraftState& a,
                        const glideslope::sim::AircraftState& b) {
    const auto x = fields_of(a);
    const auto y = fields_of(b);
    std::string out;
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (std::memcmp(&x[i].second, &y[i].second, sizeof(double)) != 0) {
            char line[160];
            std::snprintf(line, sizeof line, "%s %a against %a; ", x[i].first, x[i].second,
                          y[i].second);
            out += line;
        }
    }
    return out;
}

// The fields of `s` that are NaN, named; empty where none is.
std::string nans(const glideslope::sim::AircraftState& s) {
    std::string out;
    for (const auto& [name, value] : fields_of(s)) {
        if (std::isnan(value)) {
            out += std::string(out.empty() ? "" : ", ") + name;
        }
    }
    return out;
}

GroundRun ground_run(const glideslope::sim::CatalogueEntry& entry, double throttle, double reverse,
                     double seconds) {
    const Runway runway = a_runway();
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
    glideslope::sim::Controls c;
    c.throttle = throttle;
    c.reverse = reverse;
    stand_on_her_brakes(aircraft, c);
    GroundRun out;
    out.says_reversers = aircraft.has_reversers();
    const auto force_lbs = [&](int e) {
        return aircraft.property("propulsion/engine[" + std::to_string(e) + "]/thrust-lbs");
    };
    const int steps = static_cast<int>(seconds * steps_per_second);
    std::vector<double> total_lbs;
    for (int step = 0; step < steps; ++step) {
        aircraft.set_controls(c);
        aircraft.step();
        double sum = 0.0;
        for (int e = 0; e < aircraft.engine_count(); ++e) {
            sum += force_lbs(e);
        }
        total_lbs.push_back(sum);
    }
    for (int e = 0; e < aircraft.engine_count(); ++e) {
        out.thrust_lbs.push_back(force_lbs(e));
    }
    const double most = total_lbs.empty() ? 0.0 : total_lbs.back();
    for (std::size_t i = 0; i < total_lbs.size(); ++i) {
        if (most < 0.0 && total_lbs[i] <= 0.9 * most) {
            out.to_most_reverse_s = static_cast<double>(i + 1) / steps_per_second;
            break;
        }
    }
    out.state = aircraft.state();
    return out;
}

} // namespace

// **Every aircraft with thrust reversers is pushed back by them on the
// ground, by the cascade's share, and every other ignores the lever.** Every
// landplane in the catalogue (the flying boat named: afloat she has no brakes
// to stand on) stands on the runway with her brakes on for twenty seconds,
// three ways: at full throttle, at full reverse and at idle reverse. One
// whose catalogue says `reversers` must say so herself; at full reverse each
// reversing engine must push back 0.31 of what it pushed forward at full
// throttle (tools/airliner.py: NASA TM-2000-210300's cascade reverser),
// within 0.01, and the engines reach nine tenths of it within eight seconds
// of the lever; an engine without one - the A380's outboard pair - stays
// forward, at idle; and at idle reverse the reversing engines push back. The
// eight is what they take, 7.2 to 7.8 s - the sleeves' two and each engine's
// spool from idle to full throttle - against the TSB's A340, at maximum
// reverse "approximately five seconds" after it was selected (A05H0002,
// 1.16): a real engine's maximum reverse is a lower N1 than its full
// throttle, and is reached sooner. One without reversers stands exactly as she stands with the lever
// stowed, to the last bit of her state - compared field by field bit for bit,
// so that a flight is the same as itself whatever is in it - and the same
// as she stands stowed twice, so that a flight that differs run to run is
// told apart from one the lever changed. And no standing aircraft's state
// has a NaN in it.
GLIDESLOPE_TEST(every_aircraft_with_reversers_is_pushed_back_by_them_on_the_ground_and_every_other_ignores_the_lever) {
    const auto roster = glideslope::sim::read_catalogue(data());
    constexpr double seconds = 20.0;
    std::size_t with = 0;
    std::size_t without = 0;
    std::vector<std::string> left_out;
    std::vector<std::string> wrong;
    for (const glideslope::sim::CatalogueEntry& e : roster) {
        if (e.seaplane) {
            left_out.push_back(e.id);
            std::printf("  left out - %s: a flying boat, afloat, has no brakes to stand on\n",
                        e.id.c_str());
            continue;
        }
        const bool reversers = e.reverse_idle_kts.has_value();
        const GroundRun most = ground_run(e, 0.0, 1.0, seconds);
        // **A standing aircraft's state has no NaN in it** (the J-3 Cub's
        // went NaN on macOS standing on her brakes as no pilot stands her;
        // stand_on_her_brakes).
        if (const std::string nan = nans(most.state); !nan.empty()) {
            wrong.push_back(e.id + " standing has a NaN in her state: " + nan);
        }
        if (most.says_reversers != reversers) {
            wrong.push_back(e.id + ": her flight model's reversers are not her catalogue's");
        }
        if (!reversers) {
            // Stowed twice, so that a flight that differs run to run - the
            // lever not involved - is told apart from one the lever changed.
            const GroundRun stowed = ground_run(e, 0.0, 0.0, seconds);
            const GroundRun again = ground_run(e, 0.0, 0.0, seconds);
            if (const std::string differ = differences(stowed.state, again.state);
                !differ.empty()) {
                wrong.push_back(e.id + " stood differently twice with the lever stowed - not the "
                                "lever: " + differ);
            } else if (const std::string changed = differences(stowed.state, most.state);
                       !changed.empty()) {
                wrong.push_back(e.id + " has no reversers, and the lever changed her: " + changed);
            }
            ++without;
            continue;
        }
        const GroundRun forward = ground_run(e, 1.0, 0.0, seconds);
        const GroundRun idle = ground_run(e, 0.0, glideslope::sim::Controls::reverse_idle, seconds);
        double idle_lbs = 0.0;
        std::size_t reversing = 0;
        for (std::size_t i = 0; i < forward.thrust_lbs.size(); ++i) {
            const double share = most.thrust_lbs[i] / forward.thrust_lbs[i];
            std::printf("  %-10s engine %zu: %7.0f lb forward, %7.0f in reverse (%+.3f), "
                        "%6.0f at idle reverse; nine tenths of reverse in %.2f s\n",
                        e.id.c_str(), i, forward.thrust_lbs[i], most.thrust_lbs[i], share,
                        idle.thrust_lbs[i], most.to_most_reverse_s);
            if (most.thrust_lbs[i] < 0.0) {
                ++reversing;
                idle_lbs += idle.thrust_lbs[i];
                if (std::abs(share + 0.31) > 0.01) {
                    wrong.push_back(e.id + " engine " + std::to_string(i) + " reversed " +
                                    std::to_string(-share) + " of her thrust, not 0.31");
                }
            } else if (most.thrust_lbs[i] > 0.1 * forward.thrust_lbs[i]) {
                wrong.push_back(e.id + " engine " + std::to_string(i) +
                                " has no reverser and was not at idle: " +
                                std::to_string(most.thrust_lbs[i]) + " lb");
            }
        }
        if (reversing == 0) {
            wrong.push_back(e.id + " has reversers and no engine reversed");
        }
        if (!(idle_lbs < 0.0)) {
            wrong.push_back(e.id + "'s reversing engines at idle reverse pushed " +
                            std::to_string(idle_lbs) + " lb, not back");
        }
        if (!(most.to_most_reverse_s > 0.0 && most.to_most_reverse_s <= 8.0)) {
            wrong.push_back(e.id + " took " + std::to_string(most.to_most_reverse_s) +
                            " s to reach her reverse, not eight or less");
        }
        ++with;
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " aircraft are not as the catalogue says, "
                         "the first: " + (wrong.empty() ? "" : wrong.front()));
    check(with == 5 && with + without + left_out.size() == roster.size() && roster.size() == 16 &&
              left_out.size() == 1U,
          "all sixteen were asked: " + std::to_string(with) + " with reversers, " +
              std::to_string(without) + " without, " + std::to_string(left_out.size()) +
              " left out");
}

// **In the air the reverse lever does nothing**: the reversers' air-ground
// interlock. Every aircraft in the catalogue, flown for ten seconds from her
// catalogue start at 5,000 ft, gear up, on the throttle she starts with,
// flies exactly the same flight with the lever at maximum reverse as stowed.
GLIDESLOPE_TEST(no_aircraft_deploys_her_reversers_in_the_air) {
    const auto roster = glideslope::sim::read_catalogue(data());
    const auto fly = [&](const glideslope::sim::CatalogueEntry& e, double lever) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = 5000.0;
        ic.airspeed_kts = e.start_airspeed_kts;
        ic.engine_running = true;
        ic.gear = 0.0;
        aircraft.initialize(ic);
        glideslope::sim::Controls c;
        c.throttle = e.start_throttle;
        c.gear = 0.0;
        c.reverse = lever;
        for (int i = 0; i < 10 * steps_per_second; ++i) {
            aircraft.set_controls(c);
            aircraft.step();
        }
        return aircraft.state();
    };
    std::size_t flown = 0;
    std::vector<std::string> wrong;
    for (const glideslope::sim::CatalogueEntry& e : roster) {
        if (!(fly(e, 0.0) == fly(e, 1.0))) {
            wrong.push_back(e.id);
        }
        ++flown;
    }
    check(wrong.empty(), std::to_string(wrong.size()) +
                             " aircraft flew differently with the reverse lever out, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(flown == roster.size() && flown == 16,
          "all sixteen were flown: " + std::to_string(flown));
}

// **Bounced with her reversers out, she gets her forward thrust back only
// once they are stowed, and climbs away.** Every aircraft whose catalogue
// gives her reversers takes off along the runway at full throttle, take-off
// flap out, her stick a little forward, to her rotation speed (her departure
// speeds); then closes her throttles and runs her reversers fully out, her
// spoilers out to hold her down, for four seconds - long enough for the
// sleeves to be home and the lever's power to follow. Then the runway drops
// ten feet beneath her, as a bounce leaves her: the lever is stowed, the
// spoilers in, the throttles to full and the nose held at eight degrees.
// With no weight on her wheels the sleeves stow on their own; at every step
// until they are in, each reversing engine's throttle must be held at idle
// (tools/airliner.py) - a sleeve half out at full power is reverse thrust in
// the air - and once they are in it must be the full throttle asked for.
// Forty seconds after the bounce she must be more than 100 ft above the
// dropped runway and climbing.
GLIDESLOPE_TEST(bounced_with_her_reversers_out_she_gets_her_thrust_back_once_they_are_stowed_and_climbs_away) {
    const auto roster = glideslope::sim::read_catalogue(data());
    std::size_t flown = 0;
    std::vector<std::string> wrong;
    for (const glideslope::sim::CatalogueEntry& e : roster) {
        if (!e.reverse_idle_kts) {
            continue;
        }
        const DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), e.model);
        const Runway runway = a_runway();
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        const auto dropped_m = std::make_shared<double>(0.0);
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [dropped_m](double, double) { return -*dropped_m; },
            [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = runway.threshold_lat_deg;
        ic.longitude_deg = runway.threshold_lon_deg;
        ic.altitude_ft = runway.elevation_ft;
        ic.terrain_elevation_ft = runway.elevation_ft;
        ic.heading_deg = runway.heading_deg;
        ic.engine_running = true;
        ic.gear = 1.0;
        ic.flaps = speeds.flap;
        aircraft.initialize(ic);
        glideslope::sim::TestPilot pilot(aircraft);
        glideslope::sim::Controls c;
        c.gear = 1.0;
        c.flaps = speeds.flap;
        c.throttle = 1.0;
        c.elevator = -0.3;
        std::vector<int> reversing;
        bool out_before = false;
        bool held_at_idle = true;
        bool full_once_home = false;
        int reversed_at = -1;
        int bounce = -1;
        double climb_fpm = 0.0;
        for (int step = 0; step < 240 * steps_per_second; ++step) {
            const double kts = aircraft.property("velocities/vg-fps") / 1.68781;
            if (reversed_at < 0 && kts >= speeds.rotate_kts) {
                reversed_at = step;
                c.throttle = 0.0;
                c.reverse = 1.0;
                c.speedbrake = 1.0;
            }
            if (reversed_at >= 0 && bounce < 0 && step == reversed_at + 4 * steps_per_second) {
                bounce = step;
                out_before = aircraft.property("fcs/reverser-pos-norm") >= 0.999 &&
                             aircraft.property("gear/wow") > 0.5;
                for (int n = 0; n < aircraft.engine_count(); ++n) {
                    if (std::cos(aircraft.property("propulsion/engine[" + std::to_string(n) +
                                                   "]/reverser-angle-rad")) < 0.0) {
                        reversing.push_back(n);
                    }
                }
                *dropped_m = 10.0 * 0.3048;
                c.reverse = 0.0;
                c.throttle = 1.0;
                c.speedbrake = 0.0;
            }
            if (bounce >= 0) {
                c.elevator = pilot.pitch_to(8.0);
                c.aileron = pilot.roll_to(0.0);
            }
            aircraft.set_controls(c);
            aircraft.step();
            if (bounce < 0) {
                continue;
            }
            const bool home = aircraft.property("fcs/reverser-pos-norm") <= 0.0;
            for (int n : reversing) {
                const double power =
                    aircraft.property("fcs/throttle-pos-norm[" + std::to_string(n) + "]");
                if (!home && power != 0.0) {
                    held_at_idle = false;
                }
                if (home && power == 1.0) {
                    full_once_home = true;
                }
            }
            climb_fpm = aircraft.state().climb_rate_fpm;
            if (step >= bounce + 40 * steps_per_second) {
                break;
            }
        }
        const double above_ft = aircraft.property("position/h-agl-ft");
        std::printf("  %-10s %zu reversing, out at the bounce %s (%.0f kt), idle until stowed %s, "
                    "full once in %s; 40 s on %.0f ft above the dropped runway, %+.0f ft/min\n",
                    e.id.c_str(), reversing.size(), out_before ? "yes" : "NO",
                    speeds.rotate_kts, held_at_idle ? "yes" : "NO",
                    full_once_home ? "yes" : "NO", above_ft, climb_fpm);
        if (bounce < 0 || !out_before || reversing.empty()) {
            wrong.push_back(e.id + ": her reversers were not out on her wheels when she bounced");
        }
        if (!held_at_idle) {
            wrong.push_back(e.id + ": a reversing engine had power before its sleeves were in");
        }
        if (!full_once_home) {
            wrong.push_back(e.id + ": her full throttle never came back once they were");
        }
        if (!(above_ft > 100.0 && climb_fpm > 0.0)) {
            wrong.push_back(e.id + " did not climb away: " + std::to_string(above_ft) + " ft, " +
                            std::to_string(climb_fpm) + " ft/min");
        }
        ++flown;
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(flown == 5, "all five with reversers were bounced: " + std::to_string(flown));
}

// **Every aircraft standing on her brakes at idle stays finite, and on the
// ground**: every
// landplane in the catalogue (the flying boat named: afloat she has no
// brakes to stand on) stands on the runway for twenty seconds, throttle
// closed and brakes fully on, as the reversers test stands her; and at every
// step every floating-point number in her JSBSim property tree
// (Aircraft::numbers, 549 to 775 a model) must be finite. On the first
// that is not, she fails naming the step, the first properties to go and,
// from the step before, those and her gear, propeller, engine, forces and
// accelerations - so that wherever it happens (the J-3 Cub on macOS release,
// CI run 38443070583) the run says what went first. And she must stand:
// more than ten feet up or ten feet a second, she fails, printing her trace
// every half second to there (on macOS the Cub's ratio of air to fuel went
// infinite only once she was a million feet up, CI run 38447161811).
GLIDESLOPE_TEST(every_aircraft_standing_on_her_brakes_at_idle_stays_finite_and_on_the_ground) {
    const auto roster = glideslope::sim::read_catalogue(data());
    constexpr int seconds = 20;
    std::size_t stood = 0;
    std::vector<std::string> left_out;
    std::vector<std::string> wrong;
    for (const glideslope::sim::CatalogueEntry& e : roster) {
        if (e.seaplane) {
            left_out.push_back(e.id);
            std::printf("  left out - %s: a flying boat, afloat, has no brakes to stand on\n",
                        e.id.c_str());
            continue;
        }
        const Runway runway = a_runway();
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = runway.threshold_lat_deg;
        ic.longitude_deg = runway.threshold_lon_deg;
        ic.altitude_ft = runway.elevation_ft;
        ic.terrain_elevation_ft = runway.elevation_ft;
        ic.heading_deg = runway.heading_deg;
        ic.engine_running = true;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        glideslope::sim::Controls c;
        stand_on_her_brakes(aircraft, c);
        const std::vector<std::string> names = aircraft.number_names();
        std::vector<double> before = aircraft.numbers();
        // Her trace, every half second, for when she goes wrong: the
        // numbers that say whether she is still standing, and her engine's.
        const std::vector<std::string> traced = {
            "position/h-agl-ft", "velocities/vt-fps", "velocities/u-fps", "velocities/w-fps",
            "velocities/q-rad_sec", "attitude/theta-deg", "aero/alpha-deg", "aero/qbar-psf",
            "propulsion/engine/engine-rpm", "propulsion/engine/AFR",
            "propulsion/engine/fuel-flow-rate-pps", "propulsion/engine/thrust-lbs",
            "propulsion/engine/map-inhg", "fcs/mixture-pos-norm", "fcs/throttle-pos-norm",
            "forces/fbx-gear-lbs", "forces/fbz-gear-lbs"};
        std::vector<std::ptrdiff_t> traced_at;
        for (const std::string& n : traced) {
            const auto it = std::find(names.begin(), names.end(), n);
            traced_at.push_back(it == names.end() ? -1 : it - names.begin());
        }
        std::vector<std::string> trace;
        const double agl_at_start_ft = aircraft.property("position/h-agl-ft");
        bool finite = true;
        bool standing = true;
        for (int step = 0; step < seconds * steps_per_second && finite && standing; ++step) {
            aircraft.set_controls(c);
            aircraft.step();
            const std::vector<double> now = aircraft.numbers();
            if (step % (steps_per_second / 2) == 0) {
                std::string line = "    " + std::to_string(step + 1) + ":";
                for (std::size_t k = 0; k < traced.size(); ++k) {
                    char v[96];
                    std::snprintf(v, sizeof v, " %s %.6g", traced[k].c_str(),
                                  traced_at[k] < 0 ? 0.0 / 1.0 : now[static_cast<std::size_t>(traced_at[k])]);
                    line += v;
                }
                trace.push_back(line);
            }
            // **Standing is standing**: ten feet higher or lower than she
            // stood at the start (her centre of gravity's height, which is
            // fourteen feet on a 747), or moving at more than ten feet a
            // second, she has blown up
            // before any number went non-finite - the J-3 Cub on macOS was
            // 1,000,000 ft up with 10^25 lb on her by the step her mixture's
            // ratio went infinite.
            const double agl_ft = aircraft.property("position/h-agl-ft");
            const double vt_fps = aircraft.property("velocities/vt-fps");
            if (std::isfinite(agl_ft) && std::isfinite(vt_fps) &&
                (std::abs(agl_ft - agl_at_start_ft) > 10.0 || vt_fps > 10.0)) {
                standing = false;
                wrong.push_back(e.id + " standing left the ground or moved at step " +
                                std::to_string(step + 1) + ": " + std::to_string(agl_ft) +
                                " ft up, " + std::to_string(vt_fps) + " ft/s");
                std::printf("  %s's trace to step %d:\n", e.id.c_str(), step + 1);
                for (const std::string& line : trace) {
                    std::printf("%s\n", line.c_str());
                }
            }
            std::vector<std::size_t> gone;
            for (std::size_t i = 0; i < now.size(); ++i) {
                if (!std::isfinite(now[i])) {
                    gone.push_back(i);
                }
            }
            if (!gone.empty()) {
                finite = false;
                std::string first;
                for (std::size_t k = 0; k < gone.size() && k < 12; ++k) {
                    first += (k == 0 ? "" : ", ") + names[gone[k]];
                }
                wrong.push_back(e.id + " went non-finite at step " + std::to_string(step + 1) +
                                " (" + std::to_string(gone.size()) + " of " +
                                std::to_string(names.size()) + " numbers), first: " + first);
                std::printf("  %s, step %d: %zu numbers not finite; the step before:\n",
                            e.id.c_str(), step + 1, gone.size());
                std::size_t shown = 0;
                for (std::size_t i = 0; i < names.size() && shown < 80; ++i) {
                    const std::string& n = names[i];
                    const bool watched =
                        std::find(gone.begin(), gone.begin() + std::min<std::ptrdiff_t>(12, static_cast<std::ptrdiff_t>(gone.size())), i) !=
                            gone.begin() + std::min<std::ptrdiff_t>(12, static_cast<std::ptrdiff_t>(gone.size())) ||
                        n.rfind("gear/unit", 0) == 0 || n.rfind("propulsion/engine[0]", 0) == 0 ||
                        n.rfind("accelerations/", 0) == 0 || n.rfind("forces/", 0) == 0;
                    if (watched) {
                        std::printf("    %-48s %a -> %a\n", n.c_str(), before[i], now[i]);
                        ++shown;
                    }
                }
            }
            before = now;
        }
        std::printf("  %-13s %s for %d s, %zu numbers watched\n", e.id.c_str(),
                    !finite ? "NOT FINITE" : !standing ? "NOT STANDING" : "finite and standing",
                    seconds, names.size());
        if (!finite && standing) {
            std::printf("  %s's trace:\n", e.id.c_str());
            for (const std::string& line : trace) {
                std::printf("%s\n", line.c_str());
            }
        }
        ++stood;
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went non-finite standing, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(stood + left_out.size() == roster.size() && roster.size() == 16 && left_out.size() == 1U,
          "all sixteen were asked: " + std::to_string(stood) + " stood, " +
              std::to_string(left_out.size()) + " left out");
}
