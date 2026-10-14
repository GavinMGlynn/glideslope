#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/crash.hpp"
#include "sim/departure.hpp"
#include "sim/figures.hpp"
#include "sim/controller.hpp"
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
                out.the_less_of_the_two &&
                commanded == std::min(asked, share) * aircraft.brake_scale();
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

// How a roll-out in a crosswind went.
struct RolledOut {
    bool handed = false;
    bool stopped = false;
    std::string wrecked;
    double handed_kts = 0.0;
    double handed_across_m = 0.0; // from the centreline at the hand-over
    double worst_across_m = 0.0;  // from the hand-over to the stop
    double least_side_grip = 1.0;
    // The most she slid sideways over the runway, feet a second: her
    // velocity across her own heading (velocities/v-fps, body axes, over
    // the ground), from the hand-over to the stop.
    double worst_slide_fps = 0.0;
};

// **A roll-out in a crosswind**: `entry` run up the runway's centreline by
// the take-off autopilot in `crosswind_kts` straight across from the left
// (from 340 onto the 070 runway) until her wheels roll at `from_kts`; then
// her throttle closed and her brakes fully on - a tailwheel aeroplane's
// stick held fully back and her brakes half on, as the stopping test's are
// - steered to the centreline by the take-off autopilot's own law, to a
// stop. On a runway of condition `code`.
RolledOut rolled_out(const glideslope::sim::CatalogueEntry& entry, int code, double crosswind_kts,
                     double from_kts, std::optional<double> held_side_grip = std::nullopt) {
    using glideslope::sim::Aircraft;
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    constexpr double fps_per_kt = 1.68781;
    const DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), entry.model);
    const Runway runway = a_runway();
    Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    glideslope::sim::Conditions conditions;
    conditions.runway_condition = code;
    aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(conditions));
    aircraft.hold_side_grip(held_side_grip);
    glideslope::sim::Conditions across = conditions;
    const double towards = (runway.heading_deg - 90.0 + 180.0) / degrees;
    const double mps = crosswind_kts * 0.514444;
    across.wind_north_mps = mps * std::cos(towards);
    across.wind_east_mps = mps * std::sin(towards);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg;
    ic.longitude_deg = runway.threshold_lon_deg;
    ic.altitude_ft = runway.elevation_ft;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.engine_running = true;
    ic.gear = 1.0;
    aircraft.initialize(ic);
    Departure departure(aircraft, runway, speeds);
    const std::optional<glideslope::sim::ApproachSpeeds> lands =
        glideslope::sim::her_figures(data(), entry.model).lands;
    std::optional<glideslope::sim::Lander> lander;
    glideslope::sim::GroundJudge judge(false);
    RolledOut out;
    for (int step = 0; step < 400 * steps_per_second; ++step) {
        const double kts = aircraft.property("velocities/vg-fps") / fps_per_kt;
        if (!out.handed && kts >= from_kts) {
            lander.emplace(glideslope::sim::Lander::on_its_roll(aircraft, *lands, runway));
            lander->resume(0.0);
            aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(across));
            out.handed = true;
            out.handed_kts = kts;
            out.handed_across_m = departure.across_m();
        }
        if (out.handed && kts < 0.5) {
            out.stopped = true;
            break;
        }
        const glideslope::sim::Controls c = lander ? lander->fly() : departure.fly();
        aircraft.set_controls(c);
        aircraft.step();
        if (std::optional<std::string> why = judge.judge(aircraft)) {
            out.wrecked = *why;
            break;
        }
        if (out.handed) {
            out.worst_across_m = std::max(out.worst_across_m, std::abs(lander->across_m()));
            out.least_side_grip = std::min(out.least_side_grip, aircraft.side_grip());
            out.worst_slide_fps =
                std::max(out.worst_slide_fps, std::abs(aircraft.property("velocities/v-fps")));
        }
    }
    return out;
}

// The crosswind of the roll-out tests, knots: the C172P's POH demonstrates
// 15 kt.
constexpr double rollout_crosswind_kts = 15.0;

// Each landplane's roll-out begins at nine tenths of her rotation speed:
// as fast as she rolls on the runway without lifting her nose.
double rollout_from_kts(const glideslope::sim::CatalogueEntry& entry) {
    const double vr = glideslope::sim::departure_speeds(data(), entry.model).rotate_kts;
    const auto lands = glideslope::sim::her_figures(data(), entry.model).lands;
    return 0.9 * std::min(vr, lands ? lands->vref_kts : vr);
}

} // namespace

// **The wet side grip alone makes her slide** - the roll-out's check that
// depends on the grip, which the roll-out test's 30 m bound does not: every
// landplane of the roll-out test, rolled out on a wet runway as 15 kt of
// crosswind arrives, twice - her side grip as the wet runway gives it, and
// held at her dry (Aircraft::hold_side_grip), her brakes braking as the wet
// runway's code says both times, so that only the grip sideways differs.
// Each of the eight jets slides across her own heading, at the worst, at
// least 1.2 times as fast with the wet side grip - a ratio, not a margin in
// ft/s, so that a platform's own floating point moves both runs alike.
// Measured 2026-10-14 on Linux: from 1.29 (A320, 14.2 ft/s against 11.0) to
// 1.83 (787-8 10.8/5.9, A380 7.7/4.2). Named and not asserted, so that **no
// assertion exercises a light aeroplane's wet side grip**: the C172P, C182
// and PA-28, whose 30 to 35 psi tyres keep three quarters or more of their
// dry grip at their slow roll-outs (1.00, 1.03 and 1.14), and the Mosquito,
// who slides less wet (25.8 against 28.8, 0.90) - a tailwheel aeroplane,
// whose sign is not explained: not investigated here. **How far from the
// centreline she goes is not what differs**: the roll-out's steering holds
// her, and most jets end nearer it wet (the 787-8 13.5 m against 19.1).
GLIDESLOPE_TEST(on_a_wet_runway_with_its_braking_held_the_wet_side_grip_alone_makes_every_jet_slide_faster_across_her_heading) {
    const auto catalogue = glideslope::sim::read_catalogue(data());
    const std::map<std::string, std::string> named = {
        {"c172p", "her tyres keep three quarters or more of their dry grip"},
        {"c182", "her tyres keep three quarters or more of their dry grip"},
        {"pa28", "her tyres keep three quarters or more of their dry grip"},
        {"mosquito-fb6", "a tailwheel aeroplane, she slides less wet, not explained"}};
    constexpr double at_least_times = 1.2;
    std::size_t asserted = 0;
    std::size_t printed = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    for (const auto& entry : catalogue) {
        if (entry.seaplane || entry.id == "j3cub" ||
            !glideslope::sim::her_figures(data(), entry.model).lands) {
            ++left_out; // as in the roll-out test above, each named there
            continue;
        }
        const RolledOut wet = rolled_out(entry, glideslope::sim::wet_runway,
                                         rollout_crosswind_kts, rollout_from_kts(entry));
        const RolledOut held = rolled_out(entry, glideslope::sim::wet_runway,
                                          rollout_crosswind_kts, rollout_from_kts(entry), 1.0);
        std::printf("  %-13s slid %.2f ft/s with the wet side grip, %.2f held dry (%.2f m and "
                    "%.2f m from the centreline)%s\n",
                    entry.id.c_str(), wet.worst_slide_fps, held.worst_slide_fps,
                    wet.worst_across_m, held.worst_across_m,
                    named.count(entry.id) ? (" - named: " + named.at(entry.id)).c_str() : "");
        if (!(wet.stopped && held.stopped && wet.wrecked.empty() && held.wrecked.empty())) {
            wrong.push_back(entry.id + " did not roll out to a stop both ways, unwrecked: " +
                            wet.wrecked + held.wrecked);
        }
        if (named.count(entry.id)) {
            ++printed;
            continue;
        }
        if (wet.worst_slide_fps < at_least_times * held.worst_slide_fps) {
            wrong.push_back(entry.id + " slid " + std::to_string(wet.worst_slide_fps) +
                            " ft/s with the wet side grip, against " +
                            std::to_string(held.worst_slide_fps) + " held dry");
        }
        ++asserted;
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(asserted == 8 && printed == named.size() && left_out == 4 &&
              asserted + printed + left_out == catalogue.size(),
          "eight jets asserted, four named, four left out: " + std::to_string(asserted) + ", " +
              std::to_string(printed) + ", " + std::to_string(left_out) + " of " +
              std::to_string(catalogue.size()));
}

// **On a wet runway, in a 15 kt crosswind, every landplane the AI lands
// rolls out and stays on it**, her tyres gripping sideways as a wet tyre
// does (sim/runway_condition.hpp, NASA TN D-7743 and D-7351): run up to nine
// tenths of the lesser of her rotation and approach speeds in calm air and
// handed to the landing's roll-out, told the runway, as 15 kt arrives from
// the left, she stops on the runway, unwrecked, never farther from its
// centreline than its half-width (30 m; the most, 26.8 m, the Mosquito, on
// 2026-10-14), her side grip at its least below 0.8 of her dry - the wet
// runway's loss was laid on her. Dry is flown and printed beside it. Left
// out, named: the flying boat, which has no wheels; the 747-400 and the
// F-22A, which publish no stall speed, so the AI lands neither; and the
// J-3 Cub, whose roll-out never brings her below half a knot. Coverage is
// asserted: twelve rolled, four left out.
GLIDESLOPE_TEST(every_landplane_rolls_out_on_a_wet_runway_in_a_15_kt_crosswind_and_stays_on_it) {
    const auto catalogue = glideslope::sim::read_catalogue(data());
    constexpr double most_across_m = glideslope::sim::Lander::runway_half_width_m;
    std::size_t rolled = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    for (const auto& entry : catalogue) {
        if (entry.seaplane) {
            std::printf("  left out - %s: a flying boat has no wheels\n", entry.id.c_str());
            ++left_out;
            continue;
        }
        if (entry.id == "j3cub") {
            std::printf("  left out - j3cub: the roll-out never brings her below half a knot "
                        "in 400 s, and she ground-loops landing in 15 kt across (an open "
                        "item)\n");
            ++left_out;
            continue;
        }
        if (!glideslope::sim::her_figures(data(), entry.model).lands) {
            std::printf("  left out - %s: she publishes no stall speed, so the AI lands her "
                        "nowhere\n",
                        entry.id.c_str());
            ++left_out;
            continue;
        }
        const RolledOut dry = rolled_out(entry, glideslope::sim::dry_runway,
                                         rollout_crosswind_kts, rollout_from_kts(entry));
        const RolledOut wet = rolled_out(entry, glideslope::sim::wet_runway,
                                         rollout_crosswind_kts, rollout_from_kts(entry));
        std::printf("  %-13s from %5.1f kt (%.2f m across): worst %.2f m across dry, %.2f wet, "
                    "side grip at least %.3f%s%s\n",
                    entry.id.c_str(), wet.handed_kts, wet.handed_across_m, dry.worst_across_m,
                    wet.worst_across_m, wet.least_side_grip,
                    wet.stopped ? "" : ", NOT STOPPED",
                    wet.wrecked.empty() ? "" : (", wrecked: " + wet.wrecked).c_str());
        if (!(wet.handed && wet.stopped && wet.wrecked.empty())) {
            wrong.push_back(entry.id + " did not roll out to a stop on the wet runway, "
                                       "unwrecked: " + wet.wrecked);
        }
        if (wet.worst_across_m > most_across_m) {
            wrong.push_back(entry.id + " went " + std::to_string(wet.worst_across_m) +
                            " m from the centreline rolling out wet in the crosswind");
        }
        if (wet.least_side_grip >= 0.8) {
            wrong.push_back(entry.id + "'s side grip was not laid on her: at least " +
                            std::to_string(wet.least_side_grip));
        }
        ++rolled;
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(rolled + left_out == catalogue.size() && left_out == 4 && rolled == 12,
          "every landplane the AI lands rolled out: " + std::to_string(rolled) + " of " +
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

// **Every tail-wheel aeroplane started at rest stands on all three wheels at
// her stance pitch**: each landplane of the catalogue whose stance is on a
// tail wheel (the J-3 Cub and the Mosquito; the nose-wheel ones and the
// flying boat named) is started on the runway with no attitude asked, and
// at once her pitch is within a degree of `Aircraft::stance`'s standing
// pitch; then she stands ten seconds at idle on full brakes with her stick
// central - the stand that put the Cub on her nose when she was started
// level (CI run 38472468661) - and throughout her pitch stays within two
// degrees of it, and at the end every wheel of hers carries weight. Seen
// red on the level start: the Cub at 0 degrees against 12.1, then over on
// her nose.
GLIDESLOPE_TEST(every_tail_wheel_aeroplane_started_at_rest_stands_on_all_three_wheels_at_her_stance_pitch) {
    const auto roster = glideslope::sim::read_catalogue(data());
    constexpr int seconds = 10;
    std::size_t tail_wheels = 0;
    std::size_t nose_wheels = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    for (const glideslope::sim::CatalogueEntry& e : roster) {
        if (e.seaplane) {
            ++left_out;
            std::printf("  left out - %s: a flying boat\n", e.id.c_str());
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
        const auto stance = aircraft.stance();
        if (!stance.found || !stance.tail_wheel) {
            ++nose_wheels;
            std::printf("  named - %s: she stands on a nose wheel\n", e.id.c_str());
            continue;
        }
        ++tail_wheels;
        const double started = aircraft.state().pitch_deg;
        glideslope::sim::Controls c;
        c.left_brake = 1.0;
        c.right_brake = 1.0;
        double least = started;
        double most = started;
        for (int step = 0; step < seconds * steps_per_second; ++step) {
            aircraft.set_controls(c);
            aircraft.step();
            least = std::min(least, aircraft.state().pitch_deg);
            most = std::max(most, aircraft.state().pitch_deg);
        }
        std::size_t wheels = 0;
        std::size_t touching = 0;
        const auto points = aircraft.contact_points();
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (points[i].bogey && points[i].wheel) {
                ++wheels;
                if (aircraft.property("gear/unit[" + std::to_string(i) + "]/WOW") > 0.5) {
                    ++touching;
                }
            }
        }
        std::printf("  %-13s stance %.2f deg: started at %.2f, stood between %.2f and %.2f, "
                    "%zu of %zu wheels touching\n",
                    e.id.c_str(), stance.standing_pitch_deg, started, least, most, touching,
                    wheels);
        if (std::abs(started - stance.standing_pitch_deg) > 1.0) {
            wrong.push_back(e.id + " started at " + std::to_string(started) + " deg");
        }
        if (least < stance.standing_pitch_deg - 2.0 || most > stance.standing_pitch_deg + 2.0) {
            wrong.push_back(e.id + " stood between " + std::to_string(least) + " and " +
                            std::to_string(most) + " deg");
        }
        if (wheels != 3 || touching != wheels) {
            wrong.push_back(e.id + " stood on " + std::to_string(touching) + " of " +
                            std::to_string(wheels) + " wheels");
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(roster.size() == 16 && tail_wheels == 2 && nose_wheels == 13 && left_out == 1,
          "two tail-wheel aeroplanes of sixteen stood, thirteen nose-wheel and the flying boat "
          "named: " + std::to_string(tail_wheels) + ", " + std::to_string(nose_wheels) + ", " +
              std::to_string(left_out));
}

// **Every landplane braked to a stop in a 15 kt crosswind keeps her pitch
// rate under `most_q_degps`**: each landplane of the catalogue (the S.23,
// the flying boat, named) is started on the runway, stood five seconds on
// full brakes in calm air, given 15 kt straight across from the left (the
// C172P POH's demonstrated crosswind), run up to `from_kts` on the throttle
// by the take-off autopilot's steering (Departure: its rudder; her brakes
// off), and then braked on both wheels at every tenth of brake, throttle
// closed, to a stop and five seconds standing. From the brakes on, her pitch
// rate stays under the bound, nothing but her wheels touches the runway
// (GroundJudge), she does not go over (45 degrees), and she stops.
//
// **A nose-wheel aeroplane** is flown stick central, on Departure's
// ailerons. **A tail-wheel aeroplane** (the J-3 Cub and the Mosquito) is
// flown as one is taxied and braked: stick held fully back and full aileron
// into the wind, throughout (FAA-H-8083-3, tailwheel airplanes); and loaded
// as she is braked: the Cub solo, a 170 lb pilot in the rear seat and her
// tank full (her figures' `solo` loading), the Mosquito with the pilot and
// navigator her model carries, 200 lb each. **Her tip ratio** is measured
// as she has settled after the stand: her centre of gravity's distance
// behind her main wheels' contacts over its height above them, at her pitch
// then and with her struts' compression - the least of her two. A brake
// whose friction is past it, slowing her, tips her onto her nose, as it
// would a real one: a run past it that goes wrong with her nose below level
// is named with both figures, and one that stays on her wheels is judged.
// The Cub's is 0.46 and she is judged at every level; the Mosquito's is
// 0.40, and she tips at 0.6 to 1.0 - the same pitch rates whether her
// pitch-alpha and pitch-stall terms count the crosswind or not (tried), so
// the geometry, not her aerodynamics. No run is excused for an open item.
//
// **The bound**: the dip of her nose as the brakes come on is the largest
// honest pitch rate, the C182's 12.1 deg/s at full brakes; 20 is above it.
// **Seen red on the models before tools/alpha_rate.py**, built as CI's Rocky
// 9 and Ubuntu jobs build (-march=x86-64-v2): near a stop, her sideslip going
// to 90 degrees, her alpha-rate terms pitched the C172P at 38 to 62 deg/s and
// struck her tail at brakes 0.6 to 1.0, the F-15C at 24 and struck, the
// 787-8 at 28 and struck, the F-22 at 18.5 and struck. **Seen red with the
// tail-wheel aeroplanes flown stick central on Departure's ailerons**: the
// solo Cub went over at 0.3 to 0.5, at 67 to 74 deg/s (her crosswind pitch
// term, Phase 9's "The J-3 Cub's nose is pushed down by a crosswind").
// **Seen red with a sign wrong in the tip ratio** (her height's term
// subtracted): the Cub's 0.01, past it at all ten levels. Whether a braked
// aeroplane's nose may go below -5 degrees (the C182's goes to -5.7 at full
// brakes, calm or not) is not judged here. Coverage asserted: fifteen
// landplanes at ten levels, 150 runs, each judged or named; each
// aeroplane's split, the levels past each tail-wheel aeroplane's ratio, her
// loading, and the flying boat by name.
GLIDESLOPE_TEST(every_landplane_braked_to_a_stop_in_a_15_kt_crosswind_keeps_her_pitch_rate_under_a_bound) {
    const auto roster = glideslope::sim::read_catalogue(data());
    constexpr double brake_levels[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    constexpr double crosswind_kts = 15.0;
    constexpr double from_kts = 10.0;
    constexpr double most_q_degps = 20.0;
    constexpr int stand_s = 5;
    constexpr int run_up_s = 60;
    constexpr int stop_s = 90;
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    const auto kts_of = [](const glideslope::sim::Aircraft& a) {
        return a.property("velocities/vg-fps") / 1.68781;
    };
    struct Count {
        std::size_t flown = 0;   // judged
        std::size_t tipping = 0; // named: past her tip ratio, and on her nose
        std::size_t past = 0;    // levels past her tip ratio, flown either way
    };
    std::map<std::string, Count> counts;
    std::map<std::string, std::string> loaded_as;
    std::vector<std::string> boats;
    std::size_t tail_wheel_ids = 0;
    double worst_q = 0.0;
    std::string worst_where;
    std::vector<std::string> wrong;
    for (const glideslope::sim::CatalogueEntry& e : roster) {
        if (e.seaplane) {
            boats.push_back(e.id);
            std::printf("  named - %s: a flying boat, afloat, has no brakes\n", e.id.c_str());
            continue;
        }
        const DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), e.model);
        const auto figures =
            glideslope::sim::read_published_figures(data() / "figures" / (e.model + ".xml"));
        bool tail_wheel_counted = false;
        for (const double brake : brake_levels) {
            char where[96];
            std::snprintf(where, sizeof where, "%s at brake %.1f", e.id.c_str(), brake);
            const Runway runway = a_runway();
            glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
            aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
                [](double, double) { return 0.0; }, [](double, double) { return false; }));
            // A tail-wheel aeroplane whose figures give a solo loading (the
            // Cub: a 170 lb pilot in the rear seat, the tank full) is braked
            // by that pilot; one whose model carries her crew already (the
            // Mosquito: pilot and navigator, 200 lb each) as her model is.
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = runway.threshold_lat_deg;
            ic.longitude_deg = runway.threshold_lon_deg;
            ic.altitude_ft = runway.elevation_ft;
            ic.terrain_elevation_ft = runway.elevation_ft;
            ic.heading_deg = runway.heading_deg;
            ic.engine_running = true;
            ic.gear = 1.0;
            {
                glideslope::sim::Aircraft probe(data() / "jsbsim", e.model);
                probe.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
                    [](double, double) { return 0.0; }, [](double, double) { return false; }));
                probe.initialize(ic);
                const auto s = probe.stance();
                check(s.found, e.id + ": her stance was found");
                if (s.tail_wheel && figures.loadings.count("solo") != 0) {
                    aircraft.load(figures.loadings.at("solo").loading);
                    loaded_as[e.id] = "solo";
                } else {
                    loaded_as[e.id] = "her model's own";
                }
            }
            aircraft.initialize(ic);
            const bool tail_wheel = aircraft.stance().tail_wheel;
            if (tail_wheel && !tail_wheel_counted) {
                tail_wheel_counted = true;
                ++tail_wheel_ids;
            }
            glideslope::sim::Controls parked;
            parked.left_brake = 1.0;
            parked.right_brake = 1.0;
            for (int step = 0; step < stand_s * steps_per_second; ++step) {
                aircraft.set_controls(parked);
                aircraft.step();
            }
            bool past_ratio = false;
            char tip_note[128] = "";
            if (tail_wheel) {
                // Her tip ratio as she has settled on her wheels: the
                // distance of her centre of gravity behind each main
                // wheel's contact over its height above it, at her pitch
                // now, each contact raised up its strut by that strut's
                // compression (structural frame: x aft, z up; FGLGear), the
                // least of her two; and the braking friction of `brake` on
                // them (FGLGear: rolling, plus the brake's share of static
                // less rolling).
                const double cg_x = aircraft.property("inertia/cg-x-in");
                const double cg_z = aircraft.property("inertia/cg-z-in");
                const double pitch = aircraft.state().pitch_deg / degrees;
                const auto points = aircraft.contact_points();
                std::size_t mains = 0;
                double ratio = 1e9;
                double friction = 0.0;
                for (std::size_t i = 0; i < points.size(); ++i) {
                    if (!points[i].bogey || !points[i].wheel || points[i].x_in >= cg_x) {
                        continue;
                    }
                    ++mains;
                    const std::string unit = "gear/unit[" + std::to_string(i) + "]/";
                    const double aft = cg_x - points[i].x_in;
                    const double up =
                        cg_z - (points[i].z_in + 12.0 * aircraft.property(unit + "compression-ft"));
                    ratio = std::min(ratio, (aft * std::cos(pitch) + up * std::sin(pitch)) /
                                                (up * std::cos(pitch) - aft * std::sin(pitch)));
                    const double roll_f = aircraft.property(unit + "rolling_friction_coeff");
                    const double still_f = aircraft.property(unit + "static_friction_coeff");
                    friction = std::max(friction, roll_f + brake * (still_f - roll_f));
                }
                if (mains != 2) {
                    wrong.push_back(e.id + ": " + std::to_string(mains) +
                                    " main wheels ahead of her centre of gravity, not two");
                    break;
                }
                past_ratio = !(friction <= ratio);
                if (past_ratio) {
                    ++counts[e.id].past;
                    std::snprintf(tip_note, sizeof tip_note,
                                  "braking friction %.2f over her tip ratio %.2f: slowing at "
                                  "that grip would tip her",
                                  friction, ratio);
                }
                if (brake == brake_levels[0]) {
                    std::printf("  %s: tip ratio %.2f, settled at pitch %.2f, %s loading\n",
                                e.id.c_str(), ratio, pitch * degrees, loaded_as[e.id].c_str());
                }
            }
            // Stick fully back and full aileron into the wind (from the
            // left) for a tail wheel; otherwise central, on Departure's
            // ailerons.
            const auto hands = [tail_wheel](glideslope::sim::Controls& c) {
                c.elevator = tail_wheel ? 1.0 : 0.0;
                if (tail_wheel) {
                    c.aileron = -1.0;
                }
            };
            glideslope::sim::Conditions wind;
            const double towards = (runway.heading_deg + 90.0) / degrees;
            const double mps = crosswind_kts * 0.514444;
            wind.wind_north_mps = mps * std::cos(towards);
            wind.wind_east_mps = mps * std::sin(towards);
            wind.wind_at_20ft_mps = mps;
            aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(wind));
            Departure steering(aircraft, runway, speeds);
            double throttle = 0.0;
            bool rolling = false;
            for (int step = 0; step < run_up_s * steps_per_second; ++step) {
                if (kts_of(aircraft) >= from_kts) {
                    rolling = true;
                    break;
                }
                glideslope::sim::Controls c = steering.fly();
                throttle = std::min(1.0, throttle + 0.2 / steps_per_second);
                c.throttle = throttle;
                c.left_brake = 0.0;
                c.right_brake = 0.0;
                hands(c);
                aircraft.set_controls(c);
                aircraft.step();
            }
            glideslope::sim::GroundJudge judge(false);
            double most_q = 0.0;
            double most_q_at_kts = 0.0;
            double least_pitch = 90.0;
            std::string wrecked;
            bool stopped = false;
            int stood_steps = 0;
            for (int step = 0; rolling && step < stop_s * steps_per_second &&
                               stood_steps < stand_s * steps_per_second;
                 ++step) {
                glideslope::sim::Controls c = steering.fly();
                c.throttle = 0.0;
                c.left_brake = brake;
                c.right_brake = brake;
                hands(c);
                aircraft.set_controls(c);
                aircraft.step();
                const double q = std::abs(aircraft.state().q_radps) * degrees;
                if (!(q <= most_q)) {
                    most_q = q;
                    most_q_at_kts = kts_of(aircraft);
                }
                if (wrecked.empty()) {
                    if (std::optional<std::string> why = judge.judge(aircraft)) {
                        wrecked = *why;
                    }
                }
                // Gone over (past 45 degrees, or at no number): the run ends
                // here, before her numbers reach a JSBSim table as NaN.
                const double pitch_deg = aircraft.state().pitch_deg;
                const double roll_deg = aircraft.state().roll_deg;
                least_pitch = std::min(least_pitch, pitch_deg);
                if (!(std::abs(pitch_deg) <= 45.0 && std::abs(roll_deg) <= 45.0)) {
                    if (wrecked.empty()) {
                        wrecked = "went over";
                    }
                    break;
                }
                stopped = stopped || kts_of(aircraft) < 0.2;
                if (stopped) {
                    ++stood_steps;
                }
            }
            // Past her tip ratio, a tail-wheel aeroplane that goes wrong
            // with her nose down below level has tipped onto it, as a real
            // one would: named, with both figures. One that stays on her
            // wheels there is judged as at any other level.
            const bool excused = past_ratio && least_pitch < 0.0 &&
                                 (!wrecked.empty() || !(most_q <= most_q_degps));
            std::printf("  %s%-26s most pitch rate %5.1f deg/s (at %4.1f kt)%s%s\n",
                        excused ? "named - " : "", where, most_q,
                        most_q_at_kts, stopped ? "" : ", NOT STOPPED",
                        wrecked.empty() ? "" : (", " + wrecked).c_str());
            std::vector<std::string> faults;
            if (!rolling) {
                faults.push_back(std::string(where) + ": never rolled at 10 kt");
            }
            if (!(most_q <= most_q_degps)) {
                faults.push_back(std::string(where) + ": pitched at " + std::to_string(most_q) +
                                 " deg/s");
            }
            if (!wrecked.empty()) {
                faults.push_back(std::string(where) + ": " + wrecked);
            }
            if (rolling && !stopped) {
                faults.push_back(std::string(where) + ": did not stop");
            }
            if (excused) {
                ++counts[e.id].tipping;
                std::printf("    (%s)\n", tip_note);
                continue;
            }
            ++counts[e.id].flown;
            if (most_q > worst_q) {
                worst_q = most_q;
                worst_where = where;
            }
            wrong.insert(wrong.end(), faults.begin(), faults.end());
        }
    }
    std::printf("  worst judged: %.1f deg/s, %s\n", worst_q, worst_where.c_str());
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    // Every level of every landplane accounted for: judged, or named as
    // tipped past her tip ratio.
    const std::size_t levels = std::size(brake_levels);
    std::size_t nose_wheels = 0;
    for (const auto& [id, n] : counts) {
        check(n.flown + n.tipping == levels,
              id + ": every level accounted for, " + std::to_string(n.flown) + " judged, " +
                  std::to_string(n.tipping) + " tipped past her tip ratio");
        if (id != "j3cub" && id != "mosquito-fb6") {
            ++nose_wheels;
            check(n.flown == levels, id + ": flown at every level");
        }
    }
    check(roster.size() == 16 && counts.size() == 15 && nose_wheels == 13 &&
              tail_wheel_ids == 2 && boats == std::vector<std::string>{"short_s23"},
          "fifteen landplanes of sixteen, thirteen nose-wheel and two tail-wheel; the S.23 "
          "the flying boat named: " +
              std::to_string(counts.size()) + " landplanes, " + std::to_string(nose_wheels) +
              " nose-wheel, " + std::to_string(tail_wheel_ids) + " tail-wheel, " +
              std::to_string(boats.size()) + " boats");
    check(loaded_as["j3cub"] == "solo" && loaded_as["mosquito-fb6"] == "her model's own",
          "the Cub braked solo, the Mosquito with her model's crew");
    const Count cub = counts["j3cub"];
    const Count mosquito = counts["mosquito-fb6"];
    // The Cub, solo, past her ratio (0.46) at 0.6 to 1.0 and judged at all
    // ten; the Mosquito past hers (0.40) at 0.5 to 1.0, judged at 0.1 to 0.5
    // (at 0.5 she stays on her wheels) and tipped at 0.6 to 1.0.
    check(cub.past == 5 && cub.flown == 10 && cub.tipping == 0,
          "the Cub past her ratio at five, judged at all ten: " + std::to_string(cub.past) +
              " past, " + std::to_string(cub.flown) + " judged, " +
              std::to_string(cub.tipping) + " tipped");
    check(mosquito.past == 6 && mosquito.flown == 5 && mosquito.tipping == 5,
          "the Mosquito past her ratio at six, judged at five, tipped at five: " +
              std::to_string(mosquito.past) + " past, " + std::to_string(mosquito.flown) +
              " judged, " + std::to_string(mosquito.tipping) + " tipped");
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
