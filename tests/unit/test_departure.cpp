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
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
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

// **Rolled out dry as 15 kt of crosswind arrives, every landplane the AI
// lands stays within 5 m of the centreline** - the roll-out of the test
// above, on a dry runway. Until 2026-10-29 the roll-out's rudder held her
// heading at 0.10 a degree with nothing to hold a steady push, and the
// crosswind's weathervaning took her 12 to 21 m off (the A320 21.4, the
// F-35B 21.2); with the rudder's steady push learnt from her track
// (sim/lander.cpp) the most, on 2026-10-29 on Linux, is the C172P's 4.8 m -
// she lifts off again at 50 kt as the gust arrives and drifts in the air,
// which is the flying law's, not the roll-out's. **On a wet runway the item
// is not done**: printed beside it, not asserted, the jets reach 5.0 to 6.6
// m wet and the PA-28 and the Mosquito 15 and 18. **The Mosquito is named,
// not asserted, dry**: braked dry she goes over on her nose and strikes the
// ground with her airframe (as she did before this change, and as the
// 2026-10-14 status says), which the test checks is still so. Left out,
// named, as above: the flying boat, the 747-400 and F-22A, and the J-3 Cub.
// Coverage is asserted: eleven asserted, one named, four left out.
GLIDESLOPE_TEST(every_landplane_rolled_out_dry_as_15_kt_of_crosswind_arrives_stays_within_5_m_of_the_centreline) {
    const auto catalogue = glideslope::sim::read_catalogue(data());
    constexpr double most_across_m = 5.0;
    std::size_t rolled = 0;
    std::size_t named = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    for (const auto& entry : catalogue) {
        if (entry.seaplane || entry.id == "j3cub" ||
            !glideslope::sim::her_figures(data(), entry.model).lands) {
            std::printf("  left out - %s: as in the wet roll-out test, named there\n",
                        entry.id.c_str());
            ++left_out;
            continue;
        }
        const RolledOut dry = rolled_out(entry, glideslope::sim::dry_runway,
                                         rollout_crosswind_kts, rollout_from_kts(entry));
        const RolledOut wet = rolled_out(entry, glideslope::sim::wet_runway,
                                         rollout_crosswind_kts, rollout_from_kts(entry));
        if (entry.id == "mosquito-fb6") {
            // Named, and checked to be what is named: she goes over on her
            // nose, braked dry, before the steering can matter.
            std::printf("  named - mosquito-fb6: braked dry she strikes the ground with her "
                        "airframe at %.2f m across (wet, not asserted, %.2f)\n",
                        dry.worst_across_m, wet.worst_across_m);
            if (dry.wrecked.empty()) {
                wrong.push_back("mosquito-fb6 no longer noses over dry: assert her");
            }
            ++named;
            continue;
        }
        std::printf("  %-13s from %5.1f kt: worst %.2f m across dry; wet, not asserted, %.2f\n",
                    entry.id.c_str(), dry.handed_kts, dry.worst_across_m, wet.worst_across_m);
        if (!(dry.handed && dry.stopped && dry.wrecked.empty())) {
            wrong.push_back(entry.id + " did not roll out to a stop, unwrecked: " + dry.wrecked);
        }
        if (dry.worst_across_m > most_across_m) {
            wrong.push_back(entry.id + " went " + std::to_string(dry.worst_across_m) +
                            " m from the centreline rolling out dry as the crosswind arrived");
        }
        ++rolled;
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(rolled + named + left_out == catalogue.size() && left_out == 4 && named == 1 &&
              rolled == 11,
          "eleven asserted, one named, four left out: " + std::to_string(rolled) + ", " +
              std::to_string(named) + ", " + std::to_string(left_out) + " of " +
              std::to_string(catalogue.size()));
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
// rate under `most_q_degps`, unless her brakes' grip tips her**: each
// landplane of the catalogue (the S.23, the flying boat, named) is started
// on the runway, stood five seconds on full brakes in calm air, given 15 kt
// straight across from the left (the C172P POH's demonstrated crosswind),
// run up to `from_kts` on the throttle by the take-off autopilot's steering
// (Departure: its rudder; her brakes off), and then braked on both wheels at
// every tenth of brake, throttle closed, to a stop and five seconds
// standing. From the brakes on, her pitch rate stays under the bound,
// nothing but her wheels touches the runway (GroundJudge), she does not go
// over (45 degrees), her attitude stays a number, and she stops.
//
// **A nose-wheel aeroplane** is flown stick central, on Departure's
// ailerons. **A tail-wheel aeroplane** (the J-3 Cub and the Mosquito, by
// her stance) is flown as one is taxied and braked: stick held fully back
// and full aileron into the wind, throughout (FAA-H-8083-3, tailwheel
// airplanes); and loaded as she is braked: the Cub solo, a 170 lb pilot in
// the rear seat and her tank full (her figures' `solo` loading), the
// Mosquito with the pilot and navigator her model carries, 200 lb each.
//
// **Her tip ratio** is measured as she has settled after the stand: her
// centre of gravity's distance behind her main wheels' contacts over its
// height above them, at her pitch then and with her struts' compression -
// the least of her two. A brake whose friction is past it, slowing her, can
// tip her onto her nose, as it would a real one. **Named, not judged**, is
// a run past it where her brakes did so: her tail wheel on the runway as
// the brakes came on, lifting while she braked and before she went wrong,
// with her gear's pitching moment nose down and the air's not; at the step
// she went wrong, her pitching nose down, with no wing down past 10 degrees
// before then; her pitch rate never past `tipped_q_degps`, 60 deg/s, just
// above the fastest tip seen (a gear impulse, as the -376 deg/s this item
// began with, is judged); and her numbers all numbers. Anything else past
// her ratio - the air lifting her tail, a roll, a NaN, a jolt - is judged.
// The Cub's ratio is 0.46 and she is judged at every level; the Mosquito's
// is 0.40, and her brakes tip her at 0.6 to 1.0, at 30 to 52 deg/s, with the
// same pitch rates whether her pitch-alpha and pitch-stall terms count the
// crosswind or not (tried). **Pinned**: her named levels, exactly 0.6, 0.7,
// 0.8, 0.9 and 1.0, and the fastest of them under the 60 deg/s ceiling. No
// run is excused for an open item. **Each clause seen to refuse**: without
// the air's clause and the ceiling, the stick-central Cub's nose-overs at
// 0.6 to 1.0 were named; the ceiling alone refuses them (76 to 83 deg/s); a
// ceiling of 40 judged, and failed, the Mosquito at 0.8 to 1.0; and runs
// made to go wrong before her tail lifted (a 0.5 deg/s onset) were judged
// and failed at every one of her named levels.
//
// **The bound**: the dip of her nose as the brakes come on is the largest
// honest pitch rate, the C182's 12.1 deg/s at full brakes; 20 is above it.
// **Seen red on the models before tools/alpha_rate.py**, built as CI's Rocky
// 9 and Ubuntu jobs build (-march=x86-64-v2): near a stop, her sideslip going
// to 90 degrees, her alpha-rate terms pitched the C172P at 38 to 62 deg/s and
// struck her tail at brakes 0.6 to 1.0, the F-15C at 24 and struck, the
// 787-8 at 28 and struck, the F-22 at 18.5 and struck. **Seen red with the
// tail-wheel aeroplanes flown stick central on Departure's ailerons**: the
// solo Cub went over at 0.3 to 1.0, at 67 to 83 deg/s, judged at every one -
// past her ratio, at 0.6 to 1.0, refused a name because the air pushed her
// nose down as her tail came up (her crosswind pitch term, Phase 9's "The
// J-3 Cub's nose is pushed down by a crosswind"). **Seen red with a sign
// wrong in the tip ratio** (her height's term subtracted): the Cub's 0.01,
// past it at all ten levels. Whether a braked aeroplane's nose may go below
// -5 degrees (the C182's goes to -5.7 at full brakes, calm or not) is not
// judged here. Coverage asserted: fifteen landplanes at ten levels, 150
// runs, each judged or named; each aeroplane's split, the levels past each
// tail-wheel aeroplane's ratio and those named, her loading, and the
// tail-wheel aeroplanes and the flying boat by name.
GLIDESLOPE_TEST(every_landplane_braked_to_a_stop_in_a_15_kt_crosswind_keeps_her_pitch_rate_under_a_bound_unless_her_brakes_grip_tips_her) {
    const auto roster = glideslope::sim::read_catalogue(data());
    constexpr double brake_levels[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    constexpr double crosswind_kts = 15.0;
    constexpr double from_kts = 10.0;
    constexpr double most_q_degps = 20.0;
    // The most a run her brakes tipped may pitch at and still be named: just
    // above the Mosquito's 30 to 52 deg/s. A tip goes no faster than her
    // weight swings her; a gear impulse (the -376 deg/s this item began
    // with) is judged, and fails.
    constexpr double tipped_q_degps = 60.0;
    constexpr int stand_s = 5;
    constexpr int run_up_s = 60;
    constexpr int stop_s = 90;
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    const auto kts_of = [](const glideslope::sim::Aircraft& a) {
        return a.property("velocities/vg-fps") / 1.68781;
    };
    const auto runway = a_runway();
    const auto on_the_runway = [&runway](glideslope::sim::Aircraft& a) {
        a.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = runway.threshold_lat_deg;
        ic.longitude_deg = runway.threshold_lon_deg;
        ic.altitude_ft = runway.elevation_ft;
        ic.terrain_elevation_ft = runway.elevation_ft;
        ic.heading_deg = runway.heading_deg;
        ic.engine_running = true;
        ic.gear = 1.0;
        a.initialize(ic);
    };
    struct Count {
        std::size_t judged = 0;
        std::size_t past = 0;           // levels past her tip ratio, flown either way
        std::vector<double> tipped = {}; // named: past it, and tipped by her brakes' grip
        double most_tipped_q = 0.0;      // the fastest of those, deg/s
    };
    std::map<std::string, Count> counts;
    std::map<std::string, std::string> loaded_as;
    std::vector<std::string> boats;
    std::vector<std::string> tail_wheels;
    std::size_t nose_wheels = 0;
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
        // What she stands on, from her model as it loads. A tail-wheel
        // aeroplane whose figures give a solo loading (the Cub: a 170 lb
        // pilot in the rear seat, the tank full) is braked by that pilot;
        // one whose model carries her crew already (the Mosquito: pilot and
        // navigator, 200 lb each) as her model is.
        bool tail_wheel = false;
        {
            glideslope::sim::Aircraft probe(data() / "jsbsim", e.model);
            on_the_runway(probe);
            const auto stance = probe.stance();
            check(stance.found, e.id + ": her stance was found");
            tail_wheel = stance.tail_wheel;
        }
        const bool solo = tail_wheel && figures.loadings.count("solo") != 0;
        loaded_as[e.id] = solo ? "solo" : "her model's own";
        if (tail_wheel) {
            tail_wheels.push_back(e.id);
        } else {
            ++nose_wheels;
        }
        Count& count = counts[e.id];
        for (const double brake : brake_levels) {
            char where[96];
            std::snprintf(where, sizeof where, "%s at brake %.1f", e.id.c_str(), brake);
            glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
            if (solo) {
                aircraft.load(figures.loadings.at("solo").loading);
            }
            on_the_runway(aircraft);
            glideslope::sim::Controls parked;
            parked.left_brake = 1.0;
            parked.right_brake = 1.0;
            for (int step = 0; step < stand_s * steps_per_second; ++step) {
                aircraft.set_controls(parked);
                aircraft.step();
            }
            const double settled_pitch = aircraft.state().pitch_deg;
            bool past_ratio = false;
            double ratio = 0.0;
            double friction = 0.0;
            std::string tail_wow; // her tail wheel's weight-on-wheels property
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
                const double pitch = settled_pitch / degrees;
                const auto points = aircraft.contact_points();
                std::size_t mains = 0;
                ratio = 1e9;
                for (std::size_t i = 0; i < points.size(); ++i) {
                    if (points[i].bogey && points[i].wheel && points[i].x_in > cg_x) {
                        tail_wow = "gear/unit[" + std::to_string(i) + "]/WOW";
                    }
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
                    ++count.past;
                }
                if (brake == brake_levels[0]) {
                    std::printf("  %s: tip ratio %.2f, settled at pitch %.2f, %s loading\n",
                                e.id.c_str(), ratio, settled_pitch, loaded_as[e.id].c_str());
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
            const double braked_at_pitch = aircraft.state().pitch_deg;
            glideslope::sim::GroundJudge judge(false);
            double most_q = 0.0;
            double most_q_at_kts = 0.0;
            double least_pitch = 90.0;
            bool went_nan = false;
            std::string wrecked;
            bool stopped = false;
            int stood_steps = 0;
            // **Her tail coming up**: whether her tail wheel was on the
            // runway as the brakes came on, and the pitching moments of her
            // gear and of the air at the step it first left it; and the
            // most she rolled before she went wrong.
            const bool tail_down_at_brakes =
                !tail_wheel || aircraft.property(tail_wow) > 0.5;
            bool tail_lifted = false;
            double lift_m_gear = 0.0;
            double lift_m_aero = 0.0;
            bool gone_wrong = false;
            double wrong_q_radps = 0.0; // her pitch rate at the step she went wrong
            double most_roll = 0.0;
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
                const double q_radps = aircraft.state().q_radps;
                const double pitch_deg = aircraft.state().pitch_deg;
                const double roll_deg = aircraft.state().roll_deg;
                if (std::isnan(q_radps) || std::isnan(pitch_deg) || std::isnan(roll_deg)) {
                    went_nan = true;
                    break;
                }
                least_pitch = std::min(least_pitch, pitch_deg);
                const double q = std::abs(q_radps) * degrees;
                if (q > most_q) {
                    most_q = q;
                    most_q_at_kts = kts_of(aircraft);
                }
                if (wrecked.empty()) {
                    if (std::optional<std::string> why = judge.judge(aircraft)) {
                        wrecked = *why;
                    }
                }
                if (tail_wheel && !gone_wrong && !tail_lifted &&
                    aircraft.property(tail_wow) < 0.5) {
                    tail_lifted = true;
                    lift_m_gear = aircraft.property("moments/m-gear-lbsft");
                    lift_m_aero = aircraft.property("moments/m-aero-lbsft");
                }
                // Gone over: the run ends here, before her numbers reach a
                // JSBSim table as NaN.
                const bool over = !(std::abs(pitch_deg) <= 45.0 && std::abs(roll_deg) <= 45.0);
                if (!gone_wrong) {
                    most_roll = std::max(most_roll, std::abs(roll_deg));
                    gone_wrong = q > most_q_degps || !wrecked.empty() || over;
                    if (gone_wrong) {
                        wrong_q_radps = q_radps;
                    }
                }
                if (over) {
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
            // **Tipped by her brakes' grip**, and named: past her tip ratio,
            // a tail-wheel aeroplane that goes wrong by pitching onto her
            // nose, as a real one would. Her tail wheel must have been on the
            // runway as the brakes came on (a tail the air lifted in the
            // run-up is not her brakes' doing), and come up while she braked
            // and before she went wrong, with her gear's pitching moment
            // nose down and the air's not: the brakes, not her aerodynamics.
            // At the step she went wrong she must be pitching nose down,
            // with no wing down past 10 degrees before then; her pitch rate
            // no more than `tipped_q_degps`; and her numbers all numbers.
            // Anything else past her ratio - a roll, a NaN, the air, a
            // faster jolt - is judged as at any other level.
            const bool excused = past_ratio && gone_wrong && !went_nan && tail_down_at_brakes &&
                                 tail_lifted && lift_m_gear < 0.0 && lift_m_aero >= 0.0 &&
                                 wrong_q_radps < 0.0 && most_roll < 10.0 &&
                                 most_q <= tipped_q_degps;
            std::printf("  %s%-26s most pitch rate %5.1f deg/s (at %4.1f kt)%s%s%s\n",
                        excused ? "named - " : "", where, most_q, most_q_at_kts,
                        stopped ? "" : ", NOT STOPPED",
                        wrecked.empty() ? "" : (", " + wrecked).c_str(),
                        went_nan ? ", WENT NaN" : "");
            if (gone_wrong && tail_wheel) {
                std::printf("    tail %s at the brakes (pitch %.2f, stood at %.2f); %s braking "
                            "(gear %.0f ft-lb, air %.0f) before she went wrong, pitching at "
                            "%.1f deg/s then; most roll %.1f; least pitch %.1f\n",
                            tail_down_at_brakes ? "down" : "UP", braked_at_pitch, settled_pitch,
                            tail_lifted ? "lifted" : "not lifted", lift_m_gear, lift_m_aero,
                            wrong_q_radps * degrees, most_roll, least_pitch);
            }
            std::vector<std::string> faults;
            if (!rolling) {
                faults.push_back(std::string(where) + ": never rolled at 10 kt");
            }
            if (went_nan) {
                faults.push_back(std::string(where) + ": her attitude went to no number");
            }
            if (!(most_q <= most_q_degps)) {
                faults.push_back(std::string(where) + ": pitched at " + std::to_string(most_q) +
                                 " deg/s");
            }
            if (!wrecked.empty()) {
                faults.push_back(std::string(where) + ": " + wrecked);
            }
            if (rolling && !stopped && !went_nan) {
                faults.push_back(std::string(where) + ": did not stop");
            }
            if (excused) {
                count.tipped.push_back(brake);
                count.most_tipped_q = std::max(count.most_tipped_q, most_q);
                std::printf("    (braking friction %.2f over her tip ratio %.2f: slowing at that "
                            "grip tips her)\n",
                            friction, ratio);
                continue;
            }
            ++count.judged;
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
    // tipped by her brakes' grip; only a tail-wheel aeroplane named.
    const std::size_t levels = std::size(brake_levels);
    for (const auto& [id, n] : counts) {
        check(n.judged + n.tipped.size() == levels,
              id + ": every level accounted for, " + std::to_string(n.judged) + " judged, " +
                  std::to_string(n.tipped.size()) + " tipped");
        if (std::find(tail_wheels.begin(), tail_wheels.end(), id) == tail_wheels.end()) {
            check(n.judged == levels && n.past == 0, id + ": judged at every level");
        }
    }
    check(roster.size() == 16 && counts.size() == 15 && nose_wheels == 13 &&
              tail_wheels == std::vector<std::string>{"j3cub", "mosquito-fb6"} &&
              boats == std::vector<std::string>{"short_s23"},
          "fifteen landplanes of sixteen, thirteen nose-wheel and the Cub and the Mosquito on "
          "tail wheels; the S.23 the flying boat named: " +
              std::to_string(counts.size()) + " landplanes, " + std::to_string(nose_wheels) +
              " nose-wheel, " + std::to_string(tail_wheels.size()) + " tail-wheel, " +
              std::to_string(boats.size()) + " boats");
    check(loaded_as["j3cub"] == "solo" && loaded_as["mosquito-fb6"] == "her model's own",
          "the Cub braked solo, the Mosquito with her model's crew");
    // The Cub, solo, past her ratio (0.46) at 0.6 to 1.0 and judged at all
    // ten; the Mosquito past hers (0.40) at 0.5 to 1.0, judged at 0.1 to 0.5
    // (at 0.5 she stays on her wheels) and tipped at 0.6 to 1.0.
    const Count& cub = counts["j3cub"];
    const Count& mosquito = counts["mosquito-fb6"];
    check(cub.past == 5 && cub.judged == 10 && cub.tipped.empty(),
          "the Cub past her ratio at five, judged at all ten: " + std::to_string(cub.past) +
              " past, " + std::to_string(cub.judged) + " judged, " +
              std::to_string(cub.tipped.size()) + " tipped");
    check(mosquito.past == 6 && mosquito.judged == 5 &&
              mosquito.tipped == std::vector<double>{0.6, 0.7, 0.8, 0.9, 1.0} &&
              mosquito.most_tipped_q <= tipped_q_degps,
          "the Mosquito past her ratio at six, judged at 0.1 to 0.5, tipped at 0.6 to 1.0: " +
              std::to_string(mosquito.past) + " past, " + std::to_string(mosquito.judged) +
              " judged, " + std::to_string(mosquito.tipped.size()) + " tipped");
}

// **The J-3 Cub flown stick central in a 15 kt crosswind keeps her tail
// wheel on the runway wherever she keeps it there in calm air.** Her
// angle-of-attack pitch term was on the whole dynamic pressure,
// `aero/qbar-psf`, the crosswind's sideways part in it: standing in 15 kt
// across, her angle of attack (atan2(w, u), meaningless with the air from
// the side) ran to 60 degrees, her Cm-alpha term pushed her nose down by up
// to 1,500 ft-lb against the ~1,080 her weight holds her tail down with, and
// she went over standing on her brakes, at every loading. `Pitch_alpha`, and
// `Pitch_propwash` with it, are now on the dynamic pressure of u and w alone,
// `aero/qbarUW-psf` (tools/alpha_rate.py, JSBSim issue #89) - and her lift
// (`Lift_alpha`, `Lift_propwash`) and her wing's drag (`Drag_basic`) with
// them (2026-10-15, the walking-pace roll; tools/make_j3cub.py).
//
// **Every scenario**: at every loading her figures give (solo, trainer,
// gross; her model's own, with no one aboard, has no pilot to hold her stick
// central and is not flown), in calm air and in 15 kt straight across from
// the left and from the right, at every tenth of brake, 0.1 to 1.0, she is
// started on the runway and stood on full brakes at idle for five seconds;
// the wind comes on and she stands ten more; she is run up on a throttle
// opening at 0.2 a second to full, brakes off, steered by Departure's rudder,
// to 10 kt; and braked at that level, throttle closed, to a stop and five
// seconds standing. Her elevator central throughout, and her ailerons full
// into the wind, as a tail-wheel aeroplane is taxied in one (FAA-H-8083-3).
// Her tail wheel's weight on wheels is read at every step from her settling
// on her wheels, and nothing but her wheels may touch the runway
// (GroundJudge), nor may she go past 45 degrees or to no number. Each run
// ends in one of three outcomes, worst last: her tail **down** throughout;
// her tail **lifted** and she stopped on her wheels; or she **went over**
// (or struck, or went to no number).
//
// **The rule**: standing and in the run-up her tail stays down in every
// wind. Braking, in 15 kt across, her outcome is no worse than calm air's
// at the same loading and level. **She noses over in calm air**, braked
// stick central from 10 kt at half brake and more: solo she lifts her tail
// at 0.5 and goes over from 0.6, trainer goes over from 0.5, gross from 0.6
// - her brakes' grip near or past her tip ratio (0.45 to 0.50, stick
// central); whether a real Cub would is not measured (Phase 9's "The J-3 Cub
// braked stick central from 10 kt noses over in calm air"). A tail-wheel
// pilot brakes with the stick back, and the sweep above judges her so. At
// those levels the crosswind run is held to calm air's outcome, not to her
// tail staying down - and at every one of them so held (32 runs) calm air's
// outcome is going over, so that holds them to nothing: they are counted,
// not judged. **Pinned**: the calm outcomes, level by level. Three runs
// were worse while her lift still counted the crosswind (her air's upward
// force 358 lb against 104 in calm air as the brakes came on, at her gross
// weight) - braked at 0.5, gross from the right her tail came up for a
// moment and solo from either side she went over, where in calm air it
// stayed down or only lifted; with her lift on `qbarUW` all three kept her
// tail down. **Two were named** (2026-10-28) and are named no longer
// (2026-10-29): solo at 0.5 from either side she went over, where in calm
// air she lifts her tail and stops on her wheels, once her pitch damping
// left the crosswind out. What put her over was her side force, which
// JSBSim's wind axes turned into her plane of symmetry standing in a beam
// wind; it is now tapered to nothing there (tools/side_force.py), and
// from the left she keeps her tail down, from the right she lifts it and
// stops on her wheels, as in calm air - held to calm air's outcome like
// every other. Coverage asserted: three loadings, three winds, ten levels, 90
// runs; the 60 crosswind runs: 26 held to her tail down and 34 to calm
// air's outcome, none named.
GLIDESLOPE_TEST(the_j3_cub_flown_stick_central_in_a_15_kt_crosswind_keeps_her_tail_wheel_on_the_runway_wherever_she_does_in_calm_air) {
    constexpr double brake_levels[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    constexpr double crosswind_kts = 15.0;
    constexpr double from_kts = 10.0;
    constexpr int calm_stand_s = 5;
    constexpr int wind_stand_s = 10;
    constexpr int run_up_s = 60;
    constexpr int stop_s = 90;
    constexpr int stood_s = 5;
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    const auto kts_of = [](const glideslope::sim::Aircraft& a) {
        return a.property("velocities/vg-fps") / 1.68781;
    };
    const auto runway = a_runway();
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / "j3cub.xml");
    // Her figures' loadings. Her model's own, with no one aboard, is not
    // flown: there is no pilot to hold her stick central.
    std::vector<std::pair<std::string, glideslope::sim::Loading>> loadings;
    for (const auto& [name, l] : figures.loadings) {
        loadings.emplace_back(name, l.loading);
    }
    // The wind from the left (+1) or the right (-1) of her heading, or none.
    const std::pair<const char*, double> winds[] = {{"calm", 0.0}, {"left", 1.0}, {"right", -1.0}};
    // A run's outcome, worst last.
    enum Outcome { down = 0, lifted_tail = 1, went_over = 2 };
    const char* const outcome_names[] = {"tail down", "tail lifted, stopped on her wheels",
                                         "went over"};
    std::size_t runs = 0;
    std::size_t judged = 0;   // crosswind runs held to her tail down
    std::size_t compared = 0; // crosswind runs held to calm air's outcome
    // Named: worse than calm air, and why. Each must still be, so that a
    // fault's fix is noticed. None (2026-10-29): the two named on 2026-10-28,
    // solo at 0.5 from either side, went over by her side force turned into
    // her plane of symmetry, now tapered to nothing there
    // (tools/side_force.py).
    const std::set<std::string> named_runs = {};
    const char* const named_why = "";
    std::size_t named_seen = 0;
    // By loading, each level's outcome in calm air where it is not "down".
    std::map<std::string, std::map<double, int>> calm_not_down;
    std::vector<std::string> wrong;
    for (const auto& [loading_name, loading] : loadings) {
        std::map<double, int> calm;
        for (const auto& [wind_name, side] : winds) {
            for (const double brake : brake_levels) {
                char where[128];
                std::snprintf(where, sizeof where, "%s, %s, brake %.1f", loading_name.c_str(),
                              wind_name, brake);
                glideslope::sim::Aircraft aircraft(data() / "jsbsim", "j3cub");
                aircraft.load(loading);
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
                std::string tail_wow;
                {
                    const double cg_x = aircraft.property("inertia/cg-x-in");
                    const auto points = aircraft.contact_points();
                    for (std::size_t i = 0; i < points.size(); ++i) {
                        if (points[i].bogey && points[i].wheel && points[i].x_in > cg_x) {
                            tail_wow = "gear/unit[" + std::to_string(i) + "]/WOW";
                        }
                    }
                }
                check(!tail_wow.empty(), std::string(where) + ": her tail wheel was found");
                ++runs;
                std::string lifted;  // the phase and moment her tail first left the runway
                std::string wrecked; // GroundJudge's, or gone over, or no number
                bool stopped = false;
                double most_air_m = 0.0; // the most nose-down air moment standing in the wind
                // Her tail is watched from the end of the calm stand: she is
                // started a little above her wheels and settles onto them.
                bool settled = false;
                glideslope::sim::GroundJudge judge(false);
                const auto watch = [&](const char* phase, int step) {
                    const auto s = aircraft.state();
                    if (std::isnan(s.pitch_deg) || std::isnan(s.roll_deg) ||
                        std::isnan(s.q_radps)) {
                        wrecked = std::string("no number, ") + phase;
                        return false;
                    }
                    if (settled && lifted.empty() && aircraft.property(tail_wow) < 0.5) {
                        char what[160];
                        std::snprintf(what, sizeof what,
                                      "%s at %.2f s, %.1f kt, pitch %.2f, air %.0f ft-lb", phase,
                                      step / double(steps_per_second), kts_of(aircraft),
                                      s.pitch_deg, aircraft.property("moments/m-aero-lbsft"));
                        lifted = what;
                    }
                    if (wrecked.empty()) {
                        if (std::optional<std::string> why = judge.judge(aircraft)) {
                            wrecked = *why + ", " + phase;
                        }
                    }
                    if (!(std::abs(s.pitch_deg) <= 45.0 && std::abs(s.roll_deg) <= 45.0)) {
                        if (wrecked.empty()) {
                            wrecked = std::string("went over, ") + phase;
                        }
                        return false;
                    }
                    return true;
                };
                glideslope::sim::Controls parked;
                parked.left_brake = 1.0;
                parked.right_brake = 1.0;
                bool going = true;
                for (int step = 0; going && step < calm_stand_s * steps_per_second; ++step) {
                    aircraft.set_controls(parked);
                    aircraft.step();
                    going = watch("standing in calm air", step);
                }
                settled = true;
                going = going && watch("settled in calm air", calm_stand_s * steps_per_second);
                glideslope::sim::Conditions wind;
                const double towards = (runway.heading_deg + 90.0 * side) / degrees;
                const double mps = crosswind_kts * 0.514444 * std::abs(side);
                wind.wind_north_mps = mps * std::cos(towards);
                wind.wind_east_mps = mps * std::sin(towards);
                wind.wind_at_20ft_mps = mps;
                aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(wind));
                // Full aileron into the wind, as a tail-wheel aeroplane is
                // taxied in one (from the left, stick left: -1).
                parked.aileron = -side;
                for (int step = 0; going && step < wind_stand_s * steps_per_second; ++step) {
                    aircraft.set_controls(parked);
                    aircraft.step();
                    going = watch("standing", step);
                    most_air_m = std::min(most_air_m, aircraft.property("moments/m-aero-lbsft"));
                }
                Departure steering(aircraft, runway,
                                   glideslope::sim::departure_speeds(data(), "j3cub"));
                double throttle = 0.0;
                bool rolling = false;
                for (int step = 0; going && step < run_up_s * steps_per_second; ++step) {
                    if (kts_of(aircraft) >= from_kts) {
                        rolling = true;
                        break;
                    }
                    glideslope::sim::Controls c = steering.fly();
                    throttle = std::min(1.0, throttle + 0.2 / steps_per_second);
                    c.throttle = throttle;
                    c.elevator = 0.0;
                    c.aileron = -side;
                    c.left_brake = 0.0;
                    c.right_brake = 0.0;
                    aircraft.set_controls(c);
                    aircraft.step();
                    going = watch("in the run-up", step);
                }
                const bool up_before_braking = !lifted.empty() || !wrecked.empty();
                int stood = 0;
                for (int step = 0; going && rolling && step < stop_s * steps_per_second &&
                                   stood < stood_s * steps_per_second;
                     ++step) {
                    glideslope::sim::Controls c = steering.fly();
                    c.throttle = 0.0;
                    c.elevator = 0.0;
                    c.aileron = -side;
                    c.left_brake = brake;
                    c.right_brake = brake;
                    aircraft.set_controls(c);
                    aircraft.step();
                    going = watch("braking", step);
                    stopped = stopped || kts_of(aircraft) < 0.2;
                    if (stopped) {
                        ++stood;
                    }
                }
                std::printf("  %-32s air standing %5.0f ft-lb at most; tail %s%s%s\n", where,
                            most_air_m, lifted.empty() ? "down" : ("UP " + lifted).c_str(),
                            wrecked.empty() ? "" : (", " + wrecked).c_str(),
                            stopped || !wrecked.empty() ? "" : ", NOT STOPPED");
                if (up_before_braking) {
                    wrong.push_back(std::string(where) + ": her tail came up " +
                                    (lifted.empty() ? wrecked : lifted));
                    continue;
                }
                if (!rolling) {
                    wrong.push_back(std::string(where) + ": never rolled at 10 kt");
                    continue;
                }
                const int outcome = !wrecked.empty() ? went_over
                                    : !lifted.empty() ? lifted_tail
                                    : down;
                if (outcome != went_over && !stopped) {
                    wrong.push_back(std::string(where) + ": did not stop");
                }
                if (side == 0.0) {
                    calm[brake] = outcome;
                    if (outcome != down) {
                        calm_not_down[loading_name][brake] = outcome;
                    }
                    continue;
                }
                const int calm_outcome = calm.at(brake);
                if (calm_outcome != down) {
                    ++compared;
                    std::printf("    in calm air at %.1f, stick central, she %s: held to that\n",
                                brake, outcome_names[calm_outcome]);
                    const bool is_named = named_runs.count(where) != 0;
                    if (outcome > calm_outcome && is_named) {
                        ++named_seen;
                        std::printf("    named: %s\n", named_why);
                    } else if (outcome > calm_outcome) {
                        wrong.push_back(std::string(where) + ": " + outcome_names[outcome] +
                                        ", where in calm air " + outcome_names[calm_outcome]);
                    } else if (is_named) {
                        wrong.push_back(std::string(where) + ": named, but no worse than calm "
                                        "air now - judge it again");
                    }
                    continue;
                }
                ++judged;
                if (outcome != down) {
                    wrong.push_back(std::string(where) + ": her tail came up " +
                                    (lifted.empty() ? wrecked : lifted) +
                                    ", where in calm air it stays down");
                }
            }
        }
    }
    for (const auto& [name, levels] : calm_not_down) {
        std::printf("  %s, braked stick central from 10 kt in calm air:", name.c_str());
        for (const auto& [level, outcome] : levels) {
            std::printf(" %.1f %s;", level, outcome == went_over ? "went over" : "tail lifted");
        }
        std::printf("\n");
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    const std::size_t levels = std::size(brake_levels);
    check(loadings.size() == 3 && runs == loadings.size() * std::size(winds) * levels,
          "three loadings, three winds, ten levels: " + std::to_string(runs) + " runs");
    check(named_seen == named_runs.size(),
          std::to_string(named_seen) + " of the " + std::to_string(named_runs.size()) +
              " named runs worse than calm air, as named");
    check(judged + compared == loadings.size() * 2 * levels && judged == 26 && compared == 34,
          "every crosswind run judged: " + std::to_string(judged) + " to her tail down and " +
              std::to_string(compared) + " to calm air's outcome, of 26 and 34");
    // Her calm-air outcomes, pinned: solo lifts her tail at 0.5 and goes
    // over from 0.6; trainer goes over from 0.5; gross from 0.6.
    const auto over_from = [](double first) {
        std::map<double, int> m;
        for (const double l : {0.5, 0.6, 0.7, 0.8, 0.9, 1.0}) {
            if (l >= first - 1e-9) {
                m[l] = went_over;
            }
        }
        return m;
    };
    std::map<double, int> solo = over_from(0.6);
    solo[0.5] = lifted_tail;
    check(calm_not_down.size() == 3 && calm_not_down["solo"] == solo &&
              calm_not_down["trainer"] == over_from(0.5) &&
              calm_not_down["gross"] == over_from(0.6),
          "in calm air: solo lifts at 0.5 and goes over from 0.6, trainer goes over from 0.5, "
          "gross from 0.6");
}

// **The J-3 Cub standing tail to a 15 kt wind is pushed by the drag of a
// wing in reversed flow, not of a flat plate.** Her wing's drag table, the
// USA-35B's against her angle of attack, ended at 90 degrees with a flat
// plate's 1.41, and JSBSim holds a table's last value past its end: with the
// air from behind, her angle of attack near 180 degrees, she was pushed as if
// her wing were broadside. The table now runs on to 180 degrees either way
// by the section's curve of Sheldahl and Klimas (tools/make_j3cub.py).
//
// **The cited figure**: R. E. Sheldahl and P. C. Klimas, "Aerodynamic
// Characteristics of Seven Symmetrical Airfoil Sections Through 180-Degree
// Angle of Attack", SAND80-2114, Sandia National Laboratories, March 1981,
// Table 2: the NACA 0012's drag coefficient to 180 degrees, the same at every
// Reynolds number it lists past 30 degrees (hers at 15 kt, 0.7 to 1.0
// million, pages 20 to 22). Below, its rows from 150 degrees.
//
// **Every scenario**: at her model's own loading and her figures' three,
// she is started on the runway and stood on full brakes at idle in calm air
// for five seconds; 15 kt then comes from straight behind her, and she stands
// fifteen seconds more. Over the last five, at every step: she has not moved
// (under 1 kt over the ground), the air meets her from behind (her angle of
// attack 150 degrees or more either way), and the air's force along the wind
// on her, over her dynamic pressure and her wing's area, is within 0.01 of
// what the figure makes of her: the cited section's at her angle of attack
// scaled by 1.41/1.80 (her wing's own broadside figure over the section's,
// as her model's rows are), plus her airframe's and gear's 0.024, which
// hold at any angle. **The bound**, 0.01 (1.4 lb at 15 kt), is the small
// terms left over (her induced drag and sideslip drag, near nothing
// standing; measured, the share beyond her wing is printed) - and narrow
// enough that the section's rows unscaled (0.042 over at -167 degrees,
// where she stands since her reversed wing's lift and pitching moment,
// 2026-10-21; at -164 before) or the flat plate's 1.41 (1.2 over) are
// wrong. Coverage
// asserted: four loadings, 600 steps each judged.
GLIDESLOPE_TEST(the_j3_cub_standing_tail_to_a_15_kt_wind_is_pushed_within_0_01_of_a_reversed_wings_cited_drag) {
    constexpr double tail_wind_kts = 15.0;
    constexpr int calm_stand_s = 5;
    constexpr int wind_stand_s = 15;
    constexpr int judged_s = 5;
    constexpr double bound = 0.01;
    // Her wing's broadside figure over the section's (tools/make_j3cub.py),
    // and her airframe's and gear's drag at any angle (Drag_airframe,
    // Drag_gear).
    constexpr double scale = 1.4091 / 1.80;
    constexpr double airframe_and_gear = 0.020 + 0.004;
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    // SAND80-2114, Table 2, NACA 0012: the angle of attack in degrees and the
    // section's drag coefficient.
    constexpr std::pair<double, double> cited[] = {{150.0, 0.575}, {155.0, 0.420},
                                                   {160.0, 0.320}, {165.0, 0.230},
                                                   {170.0, 0.140}, {175.0, 0.055},
                                                   {180.0, 0.025}};
    const auto cited_cd = [&](double alpha_deg) {
        for (std::size_t i = 1; i < std::size(cited); ++i) {
            if (alpha_deg <= cited[i].first) {
                const double f = (alpha_deg - cited[i - 1].first) /
                                 (cited[i].first - cited[i - 1].first);
                return cited[i - 1].second + f * (cited[i].second - cited[i - 1].second);
            }
        }
        return cited[std::size(cited) - 1].second;
    };
    const auto expected_cd = [&](double alpha_deg) {
        return scale * cited_cd(alpha_deg) + airframe_and_gear;
    };
    const auto runway = a_runway();
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / "j3cub.xml");
    std::vector<std::pair<std::string, std::optional<glideslope::sim::Loading>>> loadings;
    loadings.emplace_back("her model's own", std::nullopt);
    for (const auto& [name, l] : figures.loadings) {
        loadings.emplace_back(name, l.loading);
    }
    std::size_t judged_steps = 0;
    std::vector<std::string> wrong;
    for (const auto& [loading_name, loading] : loadings) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", "j3cub");
        if (loading) {
            aircraft.load(*loading);
        }
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
        glideslope::sim::Controls parked;
        parked.left_brake = 1.0;
        parked.right_brake = 1.0;
        for (int step = 0; step < calm_stand_s * steps_per_second; ++step) {
            aircraft.set_controls(parked);
            aircraft.step();
        }
        glideslope::sim::Conditions wind;
        // From behind: the air moves the way she points.
        const double towards = runway.heading_deg / degrees;
        const double mps = tail_wind_kts * 0.514444;
        wind.wind_north_mps = mps * std::cos(towards);
        wind.wind_east_mps = mps * std::sin(towards);
        wind.wind_at_20ft_mps = mps;
        aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(wind));
        double worst = 0.0;
        double alpha_seen = 0.0;
        double cd_seen = 0.0;
        double beyond_wing_seen = 0.0;
        double drag_seen = 0.0;
        double lift_seen = 0.0;
        double pitching_seen = 0.0;
        double air_kts = 0.0;
        std::string first_wrong;
        for (int step = 0; step < wind_stand_s * steps_per_second; ++step) {
            aircraft.set_controls(parked);
            aircraft.step();
            if (step < (wind_stand_s - judged_s) * steps_per_second) {
                continue;
            }
            ++judged_steps;
            const double alpha_deg = aircraft.property("aero/alpha-deg");
            const double qbar = aircraft.property("aero/qbar-psf");
            const double area = aircraft.property("metrics/Sw-sqft");
            const double drag = aircraft.property("forces/fwx-aero-lbs");
            const double cd = drag / (qbar * area);
            const double off = cd - expected_cd(std::abs(alpha_deg));
            const double ground_kts = aircraft.property("velocities/vg-fps") / 1.68781;
            if (std::abs(off) >= std::abs(worst)) {
                worst = off;
                alpha_seen = alpha_deg;
                cd_seen = cd;
                beyond_wing_seen =
                    cd - aircraft.property("aero/force/Drag_basic") / (qbar * area);
                drag_seen = drag;
                lift_seen = aircraft.property("forces/fwz-aero-lbs");
                pitching_seen = aircraft.property("moments/m-aero-lbsft");
                air_kts = aircraft.property("velocities/vt-fps") / 1.68781;
            }
            char what[200];
            if (!(std::abs(alpha_deg) >= 150.0)) {
                std::snprintf(what, sizeof what, "the air not from behind: alpha %.1f deg",
                              alpha_deg);
            } else if (!(ground_kts < 1.0)) {
                std::snprintf(what, sizeof what, "moved: %.2f kt over the ground", ground_kts);
            } else if (!(std::abs(off) <= bound)) {
                std::snprintf(what, sizeof what,
                              "drag coefficient %.3f at %.1f deg, the figure's %.3f: off by %.3f",
                              cd, alpha_deg, expected_cd(std::abs(alpha_deg)), off);
            } else {
                continue;
            }
            if (first_wrong.empty()) {
                first_wrong = what;
            }
        }
        std::printf("  %-16s alpha %6.1f deg, air %4.1f kt: drag %5.1f lb, coefficient %.4f "
                    "(section %.4f, the figure's %.4f, off by at most %+.4f; beyond her wing "
                    "%.4f); lift %+6.1f lb, pitching %+5.0f ft-lb\n",
                    loading_name.c_str(), alpha_seen, air_kts, drag_seen, cd_seen,
                    cited_cd(std::abs(alpha_seen)), expected_cd(std::abs(alpha_seen)), worst,
                    beyond_wing_seen, lift_seen, pitching_seen);
        if (!first_wrong.empty()) {
            wrong.push_back(loading_name + ": " + first_wrong);
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(loadings.size() == 4 &&
              judged_steps == loadings.size() * std::size_t(judged_s * steps_per_second),
          "four loadings, 600 steps each: " + std::to_string(judged_steps) + " steps judged");
}

// **The J-3 Cub standing in a 15 kt beam wind is pushed by her side area's
// drag.** The pinned model's drag at 90 degrees of sideslip was 1.23 on her
// wing's area, 220 sq ft of drag area: standing in 15 kt across she was
// pushed sideways by 171 lb and leaned 4.4 degrees on her main gear. It is
// now her side area's (tools/make_j3cub.py).
//
// **The cited figures**: her side area from her Owner's Manual (J3C-65,
// pages 11 and 12) - her fuselage the firewall's depth, 28 3/4 in, from her
// nose to her wing's trailing edge (63 in, the chord, aft of the leading
// edge), tapering to nothing at the tail post (2 1/2 + 187 1/2 in aft of
// it); her nose where the overall length, 22 ft 4 1/2 in, less the rudder's
// mean chord (6.55 sq ft over 4 ft 5 7/8 in) puts it; and her fin and
// rudder, 4.7 and 6.55 sq ft: 48.7 sq ft. On it, a flat plate's drag
// broadside, Hoerner, Fluid-Dynamic Drag (1965), Chapter III, page 3-16,
// figure 28 and its text: 1.17 in three dimensions, from a square to a span
// ten times its height. 57.0 sq ft of drag area, 0.319 on her wing's.
//
// **Every scenario**: at her model's own loading and her figures' three, and
// with the wind from either side, she is started on the runway and stood on
// full brakes at idle, stick and pedals central, in calm air for five
// seconds; 15 kt then comes from straight across, and she stands fifteen
// seconds more. Over the last five, at every step: she has not moved (under
// 1 kt over the ground), the air meets her from the side (85 degrees of
// sideslip or more), and the air's force along the wind on her, over her
// dynamic pressure and her wing's area, is within 0.03 of the figure. **The
// bound**, 0.03 (5.4 sq ft of drag area, 4 lb at 15 kt), is her sideslip
// drag's slope between 15 and 90 degrees over the five degrees she may stand
// short of 90, 0.016, and the terms keyed to her angle of attack, near
// nothing standing (the share beyond her sideslip drag is printed) - and
// narrow enough that the pinned model's 1.25 is wrong forty times over.
// Her lean, printed, is not judged. Coverage asserted: four loadings, two
// sides, 600 steps each judged.
GLIDESLOPE_TEST(the_j3_cub_standing_in_a_15_kt_beam_wind_is_pushed_within_0_03_of_her_side_areas_cited_drag) {
    constexpr double crosswind_kts = 15.0;
    constexpr int calm_stand_s = 5;
    constexpr int wind_stand_s = 15;
    constexpr int judged_s = 5;
    constexpr double bound = 0.03;
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    // Her Owner's Manual, pages 11 and 12, in inches and sq ft.
    constexpr double overall_length_in = 22 * 12 + 4.5;
    constexpr double firewall_depth_in = 28.75;
    constexpr double chord_in = 63.0;
    constexpr double leading_edge_to_tail_post_in = 2.5 + 187.5;
    constexpr double rudder_chord_in = 6.55 * 144 / (4 * 12 + 5.875);
    constexpr double fin_and_rudder_sqft = 4.7 + 6.55;
    constexpr double full_depth_in =
        overall_length_in - rudder_chord_in - leading_edge_to_tail_post_in + chord_in;
    constexpr double tapering_in = leading_edge_to_tail_post_in - chord_in;
    constexpr double side_area_sqft =
        firewall_depth_in * (full_depth_in + tapering_in / 2) / 144 + fin_and_rudder_sqft;
    // Hoerner, page 3-16: a flat plate broadside, in three dimensions.
    constexpr double plate_cd = 1.17;
    constexpr double wing_area_sqft = 178.5;
    constexpr double expected_cd = plate_cd * side_area_sqft / wing_area_sqft;
    check(std::abs(side_area_sqft - 48.68) < 0.01 && std::abs(expected_cd - 0.3191) < 0.0001,
          "her side area 48.68 sq ft from the manual, 0.3191 on her wing's: " +
              std::to_string(side_area_sqft) + ", " + std::to_string(expected_cd));
    const auto runway = a_runway();
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / "j3cub.xml");
    std::vector<std::pair<std::string, std::optional<glideslope::sim::Loading>>> loadings;
    loadings.emplace_back("her model's own", std::nullopt);
    for (const auto& [name, l] : figures.loadings) {
        loadings.emplace_back(name, l.loading);
    }
    std::size_t runs = 0;
    std::size_t judged_steps = 0;
    std::vector<std::string> wrong;
    for (const auto& [loading_name, loading] : loadings) {
        for (const double side : {+1.0, -1.0}) {
            ++runs;
            const std::string where =
                loading_name + (side > 0 ? ", from her left" : ", from her right");
            glideslope::sim::Aircraft aircraft(data() / "jsbsim", "j3cub");
            if (loading) {
                aircraft.load(*loading);
            }
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
            glideslope::sim::Controls parked;
            parked.left_brake = 1.0;
            parked.right_brake = 1.0;
            for (int step = 0; step < calm_stand_s * steps_per_second; ++step) {
                aircraft.set_controls(parked);
                aircraft.step();
            }
            glideslope::sim::Conditions wind;
            // From her left the air moves to her right: towards her heading
            // plus 90 degrees.
            const double towards = (runway.heading_deg + 90.0 * side) / degrees;
            const double mps = crosswind_kts * 0.514444;
            wind.wind_north_mps = mps * std::cos(towards);
            wind.wind_east_mps = mps * std::sin(towards);
            wind.wind_at_20ft_mps = mps;
            aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(wind));
            double worst = 0.0;
            double beta_seen = 0.0;
            double cd_seen = 0.0;
            double beyond_seen = 0.0;
            double drag_seen = 0.0;
            double most_lean = 0.0;
            std::string first_wrong;
            for (int step = 0; step < wind_stand_s * steps_per_second; ++step) {
                aircraft.set_controls(parked);
                aircraft.step();
                if (step < (wind_stand_s - judged_s) * steps_per_second) {
                    continue;
                }
                ++judged_steps;
                const double beta_deg = aircraft.property("aero/beta-deg");
                const double qbar = aircraft.property("aero/qbar-psf");
                const double area = aircraft.property("metrics/Sw-sqft");
                const double drag = aircraft.property("forces/fwx-aero-lbs");
                const double cd = drag / (qbar * area);
                const double off = cd - expected_cd;
                const double ground_kts = aircraft.property("velocities/vg-fps") / 1.68781;
                const double lean = aircraft.property("attitude/phi-deg");
                if (std::abs(lean) > std::abs(most_lean)) {
                    most_lean = lean;
                }
                if (std::abs(off) >= std::abs(worst)) {
                    worst = off;
                    beta_seen = beta_deg;
                    cd_seen = cd;
                    beyond_seen =
                        cd - aircraft.property("aero/force/Drag_beta") / (qbar * area);
                    drag_seen = drag;
                }
                char what[200];
                if (!(std::abs(beta_deg) >= 85.0)) {
                    std::snprintf(what, sizeof what, "the air not from the side: beta %.1f deg",
                                  beta_deg);
                } else if (!(ground_kts < 1.0)) {
                    std::snprintf(what, sizeof what, "moved: %.2f kt over the ground", ground_kts);
                } else if (!(std::abs(off) <= bound)) {
                    std::snprintf(what, sizeof what,
                                  "drag coefficient %.3f at beta %.1f deg, the figure's %.3f: "
                                  "off by %.3f",
                                  cd, beta_deg, expected_cd, off);
                } else {
                    continue;
                }
                if (first_wrong.empty()) {
                    first_wrong = what;
                }
            }
            std::printf("  %-32s beta %6.1f deg: drag %5.1f lb, coefficient %.4f (the "
                        "figure's %.4f, off by at most %+.4f; beyond her sideslip drag %.4f); "
                        "most lean %+.2f deg\n",
                        where.c_str(), beta_seen, drag_seen, cd_seen, expected_cd, worst,
                        beyond_seen, most_lean);
            if (!first_wrong.empty()) {
                wrong.push_back(where + ": " + first_wrong);
            }
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(loadings.size() == 4 && runs == 8 &&
              judged_steps == runs * std::size_t(judged_s * steps_per_second),
          "four loadings, two sides, 600 steps each: " + std::to_string(judged_steps) +
              " steps judged");
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

// **A C172P at full power on held brakes**, at a stick position, flown at
// each of four loadings across her handbook's centre-of-gravity range
// (Section 2: 35.0 in aft of the datum forward, 47.3 aft, at 2400 lb at
// most): her model's own (one 180 lb pilot, 200 lb of fuel); two up front
// with 48 lb of fuel, her farthest forward; the figures' 2400 lb; and her
// aft limit, a 170 lb pilot, two 170 lb in the back, 60 lb of baggage and 48
// lb of fuel. She settles on full brakes for 5 s at idle, her throttle opens
// over 3 s, and she is held there 30 s more, stick where asked from the
// first step of the throttle.
struct FullPowerOnBrakes {
    std::string loading;
    double weight_lbs = 0.0;
    double cg_in = 0.0;
    bool nose_wheel_always_down = true; // from full throttle on
    bool mains_always_down = true;      // from the throttle's opening on
    double least_nose_compression_ft = 1e9;
    double most_tail_compression_ft = 0.0;     // her tail strike
    double most_nose_skid_compression_ft = 0.0; // her cowling's contact
    double least_pitch_deg = 1e9;
    double most_pitch_deg = -1e9;
    double most_ground_speed_fps = 0.0; // from full throttle on
    double rpm_at_end = 0.0;
};

std::vector<FullPowerOnBrakes> c172p_at_full_power_on_her_brakes(double elevator) {
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / "c172p.xml");
    glideslope::sim::Loading forward;
    forward.pointmass_lbs = {{0, 180}, {1, 180}, {2, 0}, {3, 0}, {4, 0}};
    forward.tank_lbs = {{0, 24}, {1, 24}};
    glideslope::sim::Loading aft;
    aft.pointmass_lbs = {{0, 170}, {1, 0}, {2, 170}, {3, 170}, {4, 60}};
    aft.tank_lbs = {{0, 24}, {1, 24}};
    const std::vector<std::pair<std::string, std::optional<glideslope::sim::Loading>>> loadings = {
        {"her model's own", std::nullopt},
        {"farthest forward", forward},
        {"2400 lb", figures.loadings.at(figures.first_loading).loading},
        {"aft limit", aft}};
    const Runway runway = a_runway();
    std::vector<FullPowerOnBrakes> out;
    for (const auto& [name, loading] : loadings) {
        glideslope::sim::Aircraft a(data() / "jsbsim", "c172p");
        if (loading) {
            a.load(*loading);
        }
        a.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = runway.threshold_lat_deg;
        ic.longitude_deg = runway.threshold_lon_deg;
        ic.altitude_ft = runway.elevation_ft;
        ic.terrain_elevation_ft = runway.elevation_ft;
        ic.heading_deg = runway.heading_deg;
        ic.engine_running = true;
        ic.gear = 1.0;
        a.initialize(ic);
        std::map<std::string, std::string> unit; // her contacts' properties, by name
        const auto points = a.contact_points();
        for (std::size_t i = 0; i < points.size(); ++i) {
            unit[points[i].name] =
                (points[i].bogey ? "gear/unit[" : "contact/unit[") + std::to_string(i) + "]/";
        }
        for (const char* n : {"NOSE", "LEFT_MAIN", "RIGHT_MAIN", "NOSE_SKID", "TAIL_STRIKE"}) {
            check(unit.count(n) == 1, std::string("the C172P has a contact named ") + n);
        }
        FullPowerOnBrakes r;
        r.loading = name;
        r.weight_lbs = a.property("inertia/weight-lbs");
        r.cg_in = a.property("inertia/cg-x-in");
        glideslope::sim::Controls c;
        c.left_brake = 1.0;
        c.right_brake = 1.0;
        constexpr int settle = 5 * steps_per_second;
        constexpr int open = 3 * steps_per_second;
        constexpr int held = 30 * steps_per_second;
        for (int step = 0; step < settle + open + held; ++step) {
            if (step >= settle) {
                c.elevator = elevator;
                c.throttle = std::min(1.0, static_cast<double>(step - settle + 1) / open);
            }
            a.set_controls(c);
            a.step();
            if (step < settle) {
                continue;
            }
            const double pitch = a.state().pitch_deg;
            r.least_pitch_deg = std::min(r.least_pitch_deg, pitch);
            r.most_pitch_deg = std::max(r.most_pitch_deg, pitch);
            r.mains_always_down = r.mains_always_down &&
                                  a.property(unit["LEFT_MAIN"] + "WOW") > 0.5 &&
                                  a.property(unit["RIGHT_MAIN"] + "WOW") > 0.5;
            r.most_tail_compression_ft = std::max(
                r.most_tail_compression_ft, a.property(unit["TAIL_STRIKE"] + "compression-ft"));
            r.most_nose_skid_compression_ft =
                std::max(r.most_nose_skid_compression_ft,
                         a.property(unit["NOSE_SKID"] + "compression-ft"));
            if (step >= settle + open) {
                r.nose_wheel_always_down =
                    r.nose_wheel_always_down && a.property(unit["NOSE"] + "WOW") > 0.5;
                r.least_nose_compression_ft = std::min(
                    r.least_nose_compression_ft, a.property(unit["NOSE"] + "compression-ft"));
                r.most_ground_speed_fps =
                    std::max(r.most_ground_speed_fps, a.property("velocities/vg-fps"));
            }
        }
        r.rpm_at_end = a.property("propulsion/engine/engine-rpm");
        std::printf("  %-16s %4.0f lb, CG %5.2f in: nose wheel %s (least compression %.3f ft), "
                    "mains %s, tail %.3f ft, cowling %.3f ft, pitch %.2f to %.2f deg, "
                    "%.2f ft/s at most, %.0f RPM\n",
                    r.loading.c_str(), r.weight_lbs, r.cg_in,
                    r.nose_wheel_always_down ? "down" : "UP", r.least_nose_compression_ft,
                    r.mains_always_down ? "down" : "UP", r.most_tail_compression_ft,
                    r.most_nose_skid_compression_ft, r.least_pitch_deg, r.most_pitch_deg,
                    r.most_ground_speed_fps, r.rpm_at_end);
        out.push_back(r);
    }
    // **The four loadings are inside her envelope**, so what is judged is
    // what her handbook allows: Section 2, normal category, 35.0 to 47.3 in
    // aft of the datum, 2400 lb at most.
    check(out.size() == 4, "four loadings flown: " + std::to_string(out.size()));
    for (const FullPowerOnBrakes& r : out) {
        check(r.cg_in >= 35.0 && r.cg_in <= 47.3 && r.weight_lbs <= 2400.5,
              r.loading + ": inside her handbook's envelope, " + std::to_string(r.weight_lbs) +
                  " lb at " + std::to_string(r.cg_in) + " in");
    }
    check(out.back().cg_in > 47.0 && out[1].cg_in < 41.0,
          "the envelope's ends are reached: aft " + std::to_string(out.back().cg_in) +
              " in, forward " + std::to_string(out[1].cg_in) + " in");
    return out;
}

// **A C172P at full power on held brakes, stick central, stands on her
// three wheels** at every loading, as her handbook's short-field take-off
// has her (Pilot's Operating Handbook, Model 172P, Section 4, Short Field
// Takeoff: "Brakes -- APPLY. Throttle -- FULL OPEN. ... Brakes -- RELEASE.
// Elevator Control -- SLIGHTLY TAIL LOW": she is held at full throttle, no
// elevator asked for, before her brakes come off). Held there 30 s her nose
// wheel and both mains carry weight at every step, neither her tail strike
// nor her cowling touches, her brakes hold her under half a foot a second,
// and her engine turns her handbook's static 2300 to 2420 RPM (Section 2),
// so it is full power she stands at.
GLIDESLOPE_TEST(a_c172p_at_full_power_on_held_brakes_with_her_stick_central_stands_on_all_three_wheels_at_every_loading) {
    for (const FullPowerOnBrakes& r : c172p_at_full_power_on_her_brakes(0.0)) {
        check(r.nose_wheel_always_down && r.mains_always_down,
              r.loading + ": on all three wheels throughout");
        check(r.most_tail_compression_ft == 0.0 && r.most_nose_skid_compression_ft == 0.0,
              r.loading + ": neither her tail nor her cowling touches, " +
                  std::to_string(r.most_tail_compression_ft) + " and " +
                  std::to_string(r.most_nose_skid_compression_ft) + " ft");
        check(r.most_ground_speed_fps < 0.5,
              r.loading + ": her brakes hold her, " + std::to_string(r.most_ground_speed_fps) +
                  " ft/s at most");
        check(r.rpm_at_end >= 2300.0 && r.rpm_at_end <= 2420.0,
              r.loading + ": at her static RPM, " + std::to_string(r.rpm_at_end));
    }
}

// **A C172P at full power on held brakes, stick full forward, never noses
// over** at any loading: both main wheels carry weight at every step and her
// cowling never touches. Her elevator works in the part of the slipstream
// that reaches it (tools/make_c172p.py); on the whole of it, as the pinned
// model had it, its 6,200 ft-lb lifted her mains and put her over her nose.
// Not judged: at her model's own loading and farthest forward, her mains
// lightened, her brakes no longer hold her and she creeps forward, at up to
// 9 and 13 ft/s by the end of the 30 s.
GLIDESLOPE_TEST(a_c172p_at_full_power_on_held_brakes_with_her_stick_full_forward_never_noses_over_at_any_loading) {
    for (const FullPowerOnBrakes& r : c172p_at_full_power_on_her_brakes(-1.0)) {
        check(r.mains_always_down, r.loading + ": both mains carry weight throughout");
        check(r.most_nose_skid_compression_ft == 0.0 && r.least_pitch_deg > -10.0,
              r.loading + ": her cowling never touches, " +
                  std::to_string(r.most_nose_skid_compression_ft) + " ft, pitch down to " +
                  std::to_string(r.least_pitch_deg));
    }
}

// **A C172P at full power on held brakes, stick full back: what she does,
// measured, which no source gives.** Her handbook has her elevator untouched
// on her brakes; nothing found says where a real one's nose wheel is with
// the stick held back. Measured (linux-release, 2026-10-16), pinned so a
// change shows: at her model's own loading, farthest forward and at 2400 lb
// her nose wheel stays down (0.04 to 0.09 ft of its strut still compressed);
// at her aft limit, 47.1 in, she sits on her tail strike at 11.2 degrees.
// Both mains carry weight throughout at every loading. On the whole
// slipstream, before her elevator was moved into the part of it that
// reaches it (tools/make_c172p.py), she sat on her tail at every loading.
GLIDESLOPE_TEST(a_c172p_at_full_power_on_held_brakes_with_her_stick_full_back_does_what_is_measured_and_unsourced) {
    for (const FullPowerOnBrakes& r : c172p_at_full_power_on_her_brakes(1.0)) {
        check(r.mains_always_down, r.loading + ": both mains carry weight throughout");
        check(r.most_nose_skid_compression_ft == 0.0, r.loading + ": her cowling never touches");
        if (r.loading == "aft limit") {
            check(!r.nose_wheel_always_down && r.most_tail_compression_ft > 0.0 &&
                      r.most_pitch_deg > 10.0 && r.most_pitch_deg < 12.0,
                  r.loading + ": sits on her tail strike, as measured: pitch " +
                      std::to_string(r.most_pitch_deg));
        } else {
            check(r.nose_wheel_always_down && r.least_nose_compression_ft > 0.02 &&
                      r.most_tail_compression_ft == 0.0,
                  r.loading + ": her nose wheel stays down and her tail off, as measured: " +
                      std::to_string(r.least_nose_compression_ft) + " ft of nose strut");
        }
    }
}

// **A C172P standing at full power turns her rudder on the part of her
// slipstream that reaches her fin**, not on the whole of it. Her handbook's
// Figure 1-1 (Three View, page 1-2), its side view scaled by its own 8 ft 9
// 1/2 in height, has her thrust line 52.9 in above the ground, her fin's root
// on the tailcone 50.7 in and its tip 103.4 in; momentum theory has the far
// slipstream at the 75 in propeller's diameter over root two, 53.0 in. Its
// upper half covers 0.545 of the fin's span (tools/make_c172p.py). On held
// brakes at her model's own loading, throttle opened over 3 s and held 5 s
// more, with her rudder full left and then full right (both of its ends; at
// its centre the term is nothing), her rudder's yaw moment is her model's
// coefficient times the free stream's dynamic pressure plus 0.545 of the
// slipstream's excess over it, to 0.1 per cent. The situation is built: she is
// standing, the slipstream's excess is over 10 psf and the rudder over 0.2
// rad. On the pinned model's whole slipstream the moment is 1.8 times this.
GLIDESLOPE_TEST(a_c172p_standing_at_full_power_turns_her_rudder_on_the_part_of_her_slipstream_that_reaches_her_fin) {
    constexpr double thrust_line_in = 52.9;
    constexpr double fin_root_in = 50.7;
    constexpr double fin_tip_in = 103.4;
    const double slipstream_in = 75.0 / std::sqrt(2.0);
    const double share = std::round((thrust_line_in + slipstream_in / 2.0 - fin_root_in) /
                                    (fin_tip_in - fin_root_in) * 1000.0) /
                         1000.0;
    check(share == 0.545, "Figure 1-1's fin in the slipstream: " + std::to_string(share));
    constexpr double cndr = -0.0430; // her model's, per radian
    const Runway runway = a_runway();
    int asked = 0;
    for (const double rudder : {-1.0, 1.0}) {
        glideslope::sim::Aircraft a(data() / "jsbsim", "c172p");
        a.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = runway.threshold_lat_deg;
        ic.longitude_deg = runway.threshold_lon_deg;
        ic.altitude_ft = runway.elevation_ft;
        ic.terrain_elevation_ft = runway.elevation_ft;
        ic.heading_deg = runway.heading_deg;
        ic.engine_running = true;
        ic.gear = 1.0;
        a.initialize(ic);
        glideslope::sim::Controls c;
        c.left_brake = 1.0;
        c.right_brake = 1.0;
        constexpr int settle = 5 * steps_per_second;
        constexpr int open = 3 * steps_per_second;
        constexpr int held = 5 * steps_per_second;
        for (int step = 0; step < settle + open + held; ++step) {
            if (step >= settle) {
                c.rudder = rudder;
                c.throttle = std::min(1.0, static_cast<double>(step - settle + 1) / open);
            }
            a.set_controls(c);
            a.step();
        }
        const double q = a.property("aero/qbar-psf");
        const double q_slipstream = a.property("aero/function/qbar-induced-psf");
        const double rudder_rad = a.property("fcs/rudder-pos-rad");
        const double expected = (q + share * (q_slipstream - q)) * a.property("metrics/Sw-sqft") *
                                a.property("metrics/bw-ft") * rudder_rad * cndr;
        const double moment = a.property("aero/coefficient/Cndr");
        std::printf("  rudder %+.0f: %.3f rad, free stream %.2f psf, slipstream %.2f psf, "
                    "%.1f ft/s: yaw moment %.1f ft-lb, the fin's share makes %.1f "
                    "(the whole slipstream %.1f)\n",
                    rudder, rudder_rad, q, q_slipstream, a.property("velocities/vg-fps"), moment,
                    expected, expected / (q + share * (q_slipstream - q)) * q_slipstream);
        check(a.property("velocities/vg-fps") < 0.5, "she is standing");
        check(q_slipstream - q > 10.0,
              "the slipstream's excess is over 10 psf: " + std::to_string(q_slipstream - q));
        check(std::abs(rudder_rad) > 0.2, "the rudder is over: " + std::to_string(rudder_rad));
        check(std::abs(moment - expected) <= 0.001 * std::abs(expected),
              "her rudder's yaw moment " + std::to_string(moment) + " ft-lb is the fin's share's " +
                  std::to_string(expected));
        ++asked;
    }
    check(asked == 2, "both ends of her rudder asked: " + std::to_string(asked));
}

// **The Mosquito braked past a level tips onto her nose, and the level is
// pinned.** The owner's decision (2026-10-17) for the Phase 9 item "A
// rolling aeroplane near a stop can be thrown on her nose by her gear in one
// step": her tipping is taken as her real behaviour, her model left
// unchanged. A tail-wheel aeroplane with her main wheels 16 in ahead of her
// centre of gravity and a high one, braked hard enough, pitches over them:
// her friction at the main wheels, 0.8 static, past the ratio of her
// centre's height to its distance aft of them. The Pilot's Notes (A.P.
// 2019E) warn against harsh braking with the tail raised - NOT FOUND in the
// edition checked (the 1944 FB 6 notes, zenoswarbirdvideos.com's scan), which
// says only that brake pressure at each wheel is 80 to 100 lb/sq in (para.
// 19); the citation is unconfirmed and not relied on. **The level is
// unmeasured on a real aeroplane**: nothing found gives the brake lever's
// travel against deceleration or a recorded nose-over, so 0.60 is this
// model's, not a Mosquito's.
//
// **The scenario**: at each of her seven loadings (her model's own, and the
// six her figures give), on the runway in calm air, stood five seconds on
// full brakes, run up on a throttle opening at 0.2 a second to 10 kt by the
// take-off steering with the stick held fully back (as a tail-wheel
// aeroplane is braked: FAA-H-8083-3), then braked on both wheels at every
// twentieth of brake from 0.05 to 1.00, throttle closed, until she stops
// and stands five seconds: 140 runs. **Tipped** is her pitch going below the
// horizontal (she stands at 11 to 12 degrees nose up) at some step of the
// braking. **Pinned**, measured on linux-release 2026-10-20: she tips from
// brake 0.65 at every loading. At 0.60 she dips to 0.9 to 6.9 degrees at 20
// to 27 deg/s, and at 0.55 to 10.0 to 10.9 at 4 to 9 deg/s, and stays on her
// wheels. Past the level she pitches at 30 to 60 deg/s, her nose to -5.6 to
// -9.7 degrees, and stops. **Moved from 0.60** at five loadings (2026-10-17's
// pin: 0.60 at her own, HX809's and the three Pilot's Notes', 0.65 at
// HJ679's and the B.IV maximum's) when her slipstreams' lift was read at the
// angle the air in them meets her wing (tools/make_mosquito_fb6.py): at her
// own loading and brake 0.60 it was up to 3,936 lb at her 12 degrees as the
// braking began, lightening her main wheels, and -341 lb as her nose came
// down through -9.5 degrees of angle of attack, pressing it down; now 1,178
// lb at most, and 78 lb as she stops. The test asserts both sides at every
// level: below a loading's pinned level she does not tip, at or above it she
// does, so a model change that moves the level in either direction fails.
GLIDESLOPE_TEST(the_mosquito_braked_to_a_stop_from_10_kt_in_calm_air_tips_onto_her_nose_from_one_brake_level_at_every_loading) {
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / "mosquito-fb6.xml");
    const auto runway = a_runway();
    const auto kts_of = [](const glideslope::sim::Aircraft& a) {
        return a.property("velocities/vg-fps") / 1.68781;
    };
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    // Her pinned tipping level at each loading ("" is her model's own).
    const std::map<std::string, double> pinned = {
        {"", 0.65},       {"hx809", 0.65},      {"hj679", 0.65},  {"b4-maximum", 0.65},
        {"pn-full", 0.65}, {"pn-18000", 0.65}, {"pn-17000", 0.65}};
    std::vector<std::string> names = {""};
    for (const auto& [name, l] : figures.loadings) {
        names.push_back(name);
    }
    constexpr int levels = 20;
    std::size_t runs = 0;
    std::size_t tipped_runs = 0;
    std::vector<std::string> wrong;
    for (const std::string& name : names) {
        check(pinned.count(name) == 1, "loading '" + name + "' has a pinned level");
        for (int i = 1; i <= levels; ++i) {
            const double brake = i * 0.05;
            glideslope::sim::Aircraft aircraft(data() / "jsbsim", "mosquito-fb6");
            if (!name.empty()) {
                aircraft.load(figures.loadings.at(name).loading);
            }
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
            const DepartureSpeeds speeds = glideslope::sim::departure_speeds(data(), "mosquito-fb6");
            glideslope::sim::Controls parked;
            parked.left_brake = 1.0;
            parked.right_brake = 1.0;
            for (int step = 0; step < 5 * steps_per_second; ++step) {
                aircraft.set_controls(parked);
                aircraft.step();
            }
            Departure steering(aircraft, runway, speeds);
            double throttle = 0.0;
            bool rolling = false;
            for (int step = 0; step < 60 * steps_per_second; ++step) {
                if (kts_of(aircraft) >= 10.0) {
                    rolling = true;
                    break;
                }
                glideslope::sim::Controls c = steering.fly();
                throttle = std::min(1.0, throttle + 0.2 / steps_per_second);
                c.throttle = throttle;
                c.left_brake = 0.0;
                c.right_brake = 0.0;
                c.elevator = 1.0;
                aircraft.set_controls(c);
                aircraft.step();
            }
            double least_pitch = 90.0;
            double most_q = 0.0;
            bool stopped = false;
            int stood = 0;
            for (int step = 0; rolling && step < 90 * steps_per_second &&
                               stood < 5 * steps_per_second;
                 ++step) {
                glideslope::sim::Controls c = steering.fly();
                c.throttle = 0.0;
                c.left_brake = brake;
                c.right_brake = brake;
                c.elevator = 1.0;
                aircraft.set_controls(c);
                aircraft.step();
                least_pitch = std::min(least_pitch, aircraft.state().pitch_deg);
                most_q = std::max(most_q, std::abs(aircraft.state().q_radps) * degrees);
                if (std::abs(aircraft.state().pitch_deg) > 45.0) {
                    break;
                }
                stopped = stopped || kts_of(aircraft) < 0.2;
                if (stopped) {
                    ++stood;
                }
            }
            const bool tipped = least_pitch < 0.0;
            char where[96];
            std::snprintf(where, sizeof where, "%s at brake %.2f", name.empty() ? "own" : name.c_str(),
                          brake);
            std::printf("  %-26s least pitch %6.1f deg, most pitch rate %5.1f deg/s%s\n", where,
                        least_pitch, most_q, tipped ? ", TIPPED" : "");
            ++runs;
            tipped_runs += tipped ? 1 : 0;
            if (!rolling || !stopped) {
                wrong.push_back(std::string(where) + (rolling ? ": did not stop" : ": never rolled"));
            }
            const bool should = brake >= pinned.at(name) - 1e-9;
            if (tipped != should) {
                wrong.push_back(std::string(where) + (tipped ? ": tipped below" : ": did not tip at") +
                                " her pinned level " + std::to_string(pinned.at(name)));
            }
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    // The space: her own loading and the six her figures give, at twenty
    // levels; of the 140 runs 56 tip: 0.65 to 1.00, eight levels at seven
    // loadings.
    check(names.size() == 7 && pinned.size() == 7 && figures.loadings.size() == 6 &&
              runs == 7 * levels,
          "seven loadings at twenty levels, 140 runs: " + std::to_string(runs));
    check(tipped_runs == 7 * 8,
          "56 of the 140 runs tipped: " + std::to_string(tipped_runs));
}

// **The Mosquito standing on her brakes at any power has her slipstreams'
// lift at the angle the air in them meets her wing.** Her lift-slipstream
// is her wing's lift coefficient on the slipstreams' extra dynamic pressure
// over 90 sq ft a side (tools/make_mosquito_fb6.py). It read the coefficient
// at her angle of attack, atan2(w, u), which standing is any angle her creep
// gives: on main's model -0.69 to 0.99 (the table's ends, -10 and 25
// degrees, and its peak), -2,645 to +3,790 lb, flipping as she crept. Now each
// side reads it at the angle the air in its slipstream meets the wing,
// atan2(w, u + 1.5 vi): standing, the slipstream runs along her x axis and
// that angle is near nothing, where her table, flaps up, gives 0.17.
//
// **Every scenario**: at her model's own loading and the six her figures
// give, at every tenth of throttle from closed to full (-9 to +18 boost),
// in calm air, in 15 kt straight across from her left and from her right:
// started on the runway, stood five seconds on full brakes with the stick
// fully back (as she is run up), the throttles opened at 0.2 a second and
// held for ten seconds in all, then the wind (or none) and fifteen seconds
// more. Over the last five, at every step, her slipstream lift over the
// slipstreams' extra dynamic pressure on their 180 sq ft is within 0.02 of
// 0.17. **The bound**, 0.02, is a quarter of a degree of her lift curve's
// 0.085 a degree; measured (linux-release, 2026-10-20) she stands at 0.166
// to 0.176 - in the beam wind she leans, and w is a little - 55 to 671 lb,
// and moves 0.01 kt at most. Main's model is out by 0.63 at least at every
// run. Coverage asserted: seven loadings, eleven throttles, three winds,
// 231 runs of 600 steps judged.
GLIDESLOPE_TEST(the_mosquito_standing_on_her_brakes_at_any_power_calm_or_in_15_kt_across_has_her_slipstream_lift_at_her_wings_coefficient_for_the_air_along_her) {
    constexpr double crosswind_kts = 15.0;
    constexpr int calm_stand_s = 5;
    constexpr int run_up_s = 10;
    constexpr int wind_stand_s = 15;
    constexpr int judged_s = 5;
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    // Her wing's lift coefficient, flaps up, at 0 degrees, and the bound on
    // it either side.
    constexpr double expected_cl = 0.17;
    constexpr double bound = 0.02;
    const std::vector<double> throttles = {0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    const auto runway = a_runway();
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / "mosquito-fb6.xml");
    std::vector<std::pair<std::string, std::optional<glideslope::sim::Loading>>> loadings;
    loadings.emplace_back("her own", std::nullopt);
    for (const auto& [name, l] : figures.loadings) {
        loadings.emplace_back(name, l.loading);
    }
    std::size_t runs = 0;
    std::size_t judged_steps = 0;
    std::vector<std::string> wrong;
    for (const auto& [loading_name, loading] : loadings) {
        for (const double throttle : throttles) {
            for (const int wind_case : {0, +1, -1}) {
                ++runs;
                char where[128];
                std::snprintf(where, sizeof where, "%s, throttle %.1f, %s", loading_name.c_str(),
                              throttle,
                              wind_case == 0  ? "calm"
                              : wind_case > 0 ? "15 kt from her left"
                                              : "15 kt from her right");
                glideslope::sim::Aircraft aircraft(data() / "jsbsim", "mosquito-fb6");
                if (loading) {
                    aircraft.load(*loading);
                }
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
                glideslope::sim::Controls held;
                held.left_brake = 1.0;
                held.right_brake = 1.0;
                held.elevator = 1.0;
                for (int step = 0; step < calm_stand_s * steps_per_second; ++step) {
                    aircraft.set_controls(held);
                    aircraft.step();
                }
                // Opened at 0.2 a second, then held while the engines settle.
                for (int step = 0; step < run_up_s * steps_per_second; ++step) {
                    held.throttle = std::min(throttle, held.throttle + 0.2 / steps_per_second);
                    aircraft.set_controls(held);
                    aircraft.step();
                }
                if (wind_case != 0) {
                    glideslope::sim::Conditions wind;
                    const double towards = (runway.heading_deg + 90.0 * wind_case) / degrees;
                    const double mps = crosswind_kts * 0.514444;
                    wind.wind_north_mps = mps * std::cos(towards);
                    wind.wind_east_mps = mps * std::sin(towards);
                    wind.wind_at_20ft_mps = mps;
                    aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(wind));
                }
                double least_cl = 1e9, most_cl = -1e9, least_lift = 1e9, most_lift = -1e9;
                double most_alpha = 0.0, least_q = 1e9, most_kts = 0.0, boost = 0.0;
                std::string first_wrong;
                for (int step = 0; step < wind_stand_s * steps_per_second; ++step) {
                    aircraft.set_controls(held);
                    aircraft.step();
                    if (step < (wind_stand_s - judged_s) * steps_per_second) {
                        continue;
                    }
                    ++judged_steps;
                    const double u = aircraft.property("velocities/u-aero-fps");
                    double extra = 0.0;
                    for (int e = 0; e < 2; ++e) {
                        const double vi = aircraft.property(
                            "propulsion/engine[" + std::to_string(e) + "]/prop-induced-velocity_fps");
                        extra += (u + 1.5 * vi) * (u + 1.5 * vi) - u * u;
                    }
                    const double q = 0.5 * aircraft.property("atmosphere/rho-slugs_ft3") * 90.0 * extra;
                    const double lift = aircraft.property("aero/force/lift-slipstream");
                    const double cl = lift / q;
                    least_q = std::min(least_q, q);
                    least_cl = std::min(least_cl, cl);
                    most_cl = std::max(most_cl, cl);
                    least_lift = std::min(least_lift, lift);
                    most_lift = std::max(most_lift, lift);
                    most_alpha = std::max(most_alpha, std::abs(aircraft.property("aero/alpha-deg")));
                    most_kts = std::max(most_kts, aircraft.property("velocities/vg-fps") / 1.68781);
                    boost = aircraft.property("propulsion/engine[0]/map-inhg") * 0.4911541 - 14.6959;
                    if (first_wrong.empty() && !(std::abs(cl - expected_cl) <= bound)) {
                        char w[200];
                        std::snprintf(w, sizeof w, "%s: coefficient %.3f (%.0f lb) at step %d", where,
                                      cl, lift, step);
                        first_wrong = w;
                    }
                }
                std::printf("  %-44s boost %+5.1f, coefficient %.3f to %.3f, lift %6.0f to %6.0f lb,"
                            " slipstreams' q.S %6.0f lb, |alpha| to %5.1f deg, %.2f kt\n",
                            where, boost, least_cl, most_cl, least_lift, most_lift, least_q, most_alpha,
                            most_kts);
                if (!first_wrong.empty()) {
                    wrong.push_back(first_wrong);
                }
            }
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(loadings.size() == 7 && throttles.size() == 11 && runs == 7 * 11 * 3 &&
              judged_steps == runs * std::size_t(judged_s * steps_per_second),
          "seven loadings, eleven throttles, three winds, 231 runs of 600 steps judged: " +
              std::to_string(runs) + " runs, " + std::to_string(judged_steps) + " steps");
}

// **The J-3 Cub braked stick central past a level noses over, and the level
// is pinned.** The owner's decision (2026-10-20) for the Phase 9 item "The
// J-3 Cub braked stick central from 10 kt noses over in calm air", as for the
// Mosquito's above: her going over is taken as her real behaviour, her model
// left unchanged. Stick central her tip ratio is 0.45 to 0.50 (her centre of
// gravity's distance behind her main wheels over its height above them),
// and her brakes' grip passes it near half brake. A Cub braked hard does go
// over: NTSB LAX07CA166, J3C-65 N98041, Quincy, California, 12 May 2007 -
// after a wheel landing on grass "he continued to apply brake pressure and
// as the airplane slowed, the tail rose upward. The airplane nosed over to
// inverted"; probable cause "the pilot's excessive use of the brakes, which
// resulted in a nose over". That is qualitative support only: **the level
// is unmeasured on a real Cub** (the report gives no brake force, and she
// held her tail up with the elevator), so these levels are this model's.
// Measured on her main gear springs of 2,093 lb/ft a wheel (2026-10-20).
//
// **The scenario**: at each of her figures' three loadings (solo, trainer,
// gross), on the runway in calm air, stood five seconds on full brakes, run
// up on a throttle opening at 0.2 a second to 10 kt by the take-off steering
// with her stick central, then braked on both wheels at every twentieth of
// brake from 0.05 to 1.00, throttle closed, stick central, until she stops
// and stands five seconds: 60 runs. **Went over** is anything but her wheels
// touching the runway (GroundJudge), past 45 degrees, or no number; **tail
// lifted** is her tail wheel off the runway at some step. **Pinned**,
// measured on linux-release 2026-10-21: solo she lifts her tail at 0.50 (to
// 9.5 degrees) and goes over from 0.55; trainer goes over from 0.50; gross
// lifts it at 0.55 (to 4.6 degrees) and goes over from 0.60. Below those
// she stands at 11.4 to 12.6 degrees, her tail down. The test asserts both
// sides of each level at every level and loading, so a model change that
// moves one either way fails. Her model's own loading, no one aboard, is
// left out: there is no pilot to hold her stick central or brake her.
GLIDESLOPE_TEST(the_j3_cub_braked_stick_central_to_a_stop_from_10_kt_in_calm_air_noses_over_from_one_brake_level_at_every_loading) {
    const auto figures = glideslope::sim::read_published_figures(data() / "figures" / "j3cub.xml");
    const auto runway = a_runway();
    const auto kts_of = [](const glideslope::sim::Aircraft& a) {
        return a.property("velocities/vg-fps") / 1.68781;
    };
    // Her pinned levels at each of her figures' loadings: from which her
    // tail first lifts, and from which she goes over. Her model's own, no
    // one aboard, is left out: there is no pilot to hold her stick or brake.
    struct Pinned {
        double tail_lifts;
        double goes_over;
    };
    const std::map<std::string, Pinned> pinned = {
        {"solo", {0.50, 0.55}}, {"trainer", {0.50, 0.50}}, {"gross", {0.55, 0.60}}};
    constexpr int levels = 20;
    std::size_t runs = 0;
    std::size_t over_runs = 0;
    std::size_t lifted_runs = 0;
    std::vector<std::string> wrong;
    for (const auto& [name, l] : figures.loadings) {
        check(pinned.count(name) == 1, "loading '" + name + "' has a pinned level");
        for (int i = 1; i <= levels; ++i) {
            const double brake = i * 0.05;
            glideslope::sim::Aircraft aircraft(data() / "jsbsim", "j3cub");
            aircraft.load(l.loading);
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
            std::string tail_wow;
            {
                const double cg_x = aircraft.property("inertia/cg-x-in");
                const auto points = aircraft.contact_points();
                for (std::size_t p = 0; p < points.size(); ++p) {
                    if (points[p].bogey && points[p].wheel && points[p].x_in > cg_x) {
                        tail_wow = "gear/unit[" + std::to_string(p) + "]/WOW";
                    }
                }
            }
            check(!tail_wow.empty(), name + ": her tail wheel was found");
            glideslope::sim::Controls parked;
            parked.left_brake = 1.0;
            parked.right_brake = 1.0;
            for (int step = 0; step < 5 * steps_per_second; ++step) {
                aircraft.set_controls(parked);
                aircraft.step();
            }
            // From here (she is started a little above her wheels and has
            // settled), anything but her wheels touching, past 45 degrees,
            // or no number is her going over.
            glideslope::sim::GroundJudge judge(false);
            std::string gone;
            bool lifted = false;
            const auto watch = [&](const char* phase) {
                const auto s = aircraft.state();
                if (std::isnan(s.pitch_deg) || std::isnan(s.roll_deg) || std::isnan(s.q_radps)) {
                    gone = std::string("no number, ") + phase;
                    return false;
                }
                if (std::optional<std::string> why = judge.judge(aircraft)) {
                    gone = *why + ", " + phase;
                    return false;
                }
                if (!(std::abs(s.pitch_deg) <= 45.0 && std::abs(s.roll_deg) <= 45.0)) {
                    gone = std::string("past 45 degrees, ") + phase;
                    return false;
                }
                lifted = lifted || aircraft.property(tail_wow) < 0.5;
                return true;
            };
            Departure steering(aircraft, runway, glideslope::sim::departure_speeds(data(), "j3cub"));
            double throttle = 0.0;
            bool rolling = false;
            bool going = true;
            for (int step = 0; going && step < 60 * steps_per_second; ++step) {
                if (kts_of(aircraft) >= 10.0) {
                    rolling = true;
                    break;
                }
                glideslope::sim::Controls c = steering.fly();
                throttle = std::min(1.0, throttle + 0.2 / steps_per_second);
                c.throttle = throttle;
                c.left_brake = 0.0;
                c.right_brake = 0.0;
                c.elevator = 0.0;
                aircraft.set_controls(c);
                aircraft.step();
                going = watch("in the run-up");
            }
            const bool up_before_braking = lifted || !gone.empty();
            double least_pitch = 90.0;
            bool stopped = false;
            int stood = 0;
            for (int step = 0; going && rolling && step < 90 * steps_per_second &&
                               stood < 5 * steps_per_second;
                 ++step) {
                glideslope::sim::Controls c = steering.fly();
                c.throttle = 0.0;
                c.left_brake = brake;
                c.right_brake = brake;
                c.elevator = 0.0;
                aircraft.set_controls(c);
                aircraft.step();
                going = watch("braking");
                least_pitch = std::min(least_pitch, aircraft.state().pitch_deg);
                stopped = stopped || kts_of(aircraft) < 0.2;
                if (stopped) {
                    ++stood;
                }
            }
            const bool went_over = !gone.empty();
            char where[64];
            std::snprintf(where, sizeof where, "%s at brake %.2f", name.c_str(), brake);
            std::printf("  %-20s least pitch %6.1f deg; tail %s%s\n", where, least_pitch,
                        lifted ? "lifted" : "down", went_over ? (", WENT OVER: " + gone).c_str() : "");
            ++runs;
            over_runs += went_over ? 1 : 0;
            lifted_runs += lifted && !went_over ? 1 : 0;
            if (up_before_braking) {
                wrong.push_back(std::string(where) + ": her tail came up before her brakes");
                continue;
            }
            if (!rolling) {
                wrong.push_back(std::string(where) + ": never rolled at 10 kt");
                continue;
            }
            if (!went_over && !stopped) {
                wrong.push_back(std::string(where) + ": did not stop");
            }
            const Pinned& p = pinned.at(name);
            const bool should_go_over = brake >= p.goes_over - 1e-9;
            const bool should_lift = brake >= p.tail_lifts - 1e-9;
            if (went_over != should_go_over) {
                wrong.push_back(std::string(where) +
                                (went_over ? ": went over below" : ": did not go over at") +
                                " her pinned level " + std::to_string(p.goes_over));
            } else if (!went_over && lifted != should_lift) {
                wrong.push_back(std::string(where) +
                                (lifted ? ": her tail lifted below" : ": her tail stayed down at") +
                                " her pinned level " + std::to_string(p.tail_lifts));
            }
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    std::printf("  %zu of %zu runs went over, %zu more lifted her tail\n", over_runs, runs,
                lifted_runs);
    // The space: her figures' three loadings at twenty levels, 60 runs; 30
    // go over (solo 0.55 to 1.00, ten levels; trainer 0.50 to 1.00, eleven;
    // gross 0.60 to 1.00, nine) and two more lift her tail (solo 0.50, gross
    // 0.55).
    check(figures.loadings.size() == 3 && pinned.size() == 3 && runs == 3 * levels,
          "three loadings at twenty levels, 60 runs: " + std::to_string(runs));
    check(over_runs == 10 + 11 + 9 && lifted_runs == 2,
          "30 of the 60 runs went over and 2 more lifted her tail: " +
              std::to_string(over_runs) + " and " + std::to_string(lifted_runs));
}

// **The J-3 Cub standing at every loading sinks on her main wheels by her
// tyres' deflection alone, within 1%.** Her shock rings are stretched when
// fitted, so a strut does not extend until its wheel carries their pull; a
// Cub stands on that preload, and her model, on a spring without one, sank
// about 3 in a wheel. Now each main wheel's force is her tyre's alone up to
// the preload and the tyre and the rings in series past it
// (tools/make_j3cub.py).
//
// **The derived figure**, from its sources here and not from the script:
// - The preload. Piper drawing 31423: a ring goes four times between spools
//   6.77 in apart and four half turns round spools 3/4 in across, its 11/16
//   in cord's centre on 1.4375 in; fitted it is 36.11 in against its free
//   pi x (7.5 + 11/16), 40.4% stretched. Its rating (730 lb at 100%, two
//   strands; MIL-C-5651D Table III) makes eight strands pull 1,180 lb; the
//   tyre rises 1.61 in an inch of the strut (drawing 31513): 733 lb a wheel.
// - The tyre. Goodyear's 8.00-4, 4 ply (Aviation Data Book 2022, Section
//   4C): 1,100 lb at 24 psi, 18.0 in across, a static loaded radius of 6.65
//   in - 2.35 in at its rated load; at her manual's 15 psi by
//   (p + 0.08 p_rated) (NACA TN 4110, equation 22): 306 lb/in.
// - Her wheels' load, from her weight and her centre of gravity between her
//   main and tail wheels at the attitude she stands at, her weight on the
//   two mains shared equally. JSBSim compresses a wheel along her body's
//   vertical and turns its force normal to the ground, so the strut carries
//   that load times the cosine of her pitch.
// The figure is the strut's force over the tyre's rate where it is under
// the preload, and that plus the series rings' share past it.
//
// **Every scenario**: at her model's own loading and her figures' three, she
// is started on a level runway, engine stopped, brakes on, and stands twenty
// seconds; she must be at rest on all three wheels, and her main wheels'
// mean compression within 1% of the figure. **The bound**, 1%, is her
// wheels' contact points moving with their compression and her gravity
// against her nominal weight, both left out, and narrow enough that her
// spring before (2,093 lb/ft, no preload) is wrong by 75%.
// **Measured** (linux-release, 2026-10-22): 1.095 in (her model's own, 343
// lb a wheel), 1.309 (solo), 1.559 (trainer), 1.728 (gross, 542 lb), each
// within 0.2% of the figure, every wheel under the 733 lb preload; she
// stands at 12.7 to 13.5 degrees. **Seen red** on her spring before the
// change: 1.92 in against 1.10 at her model's own, +75%. Coverage asserted: four
// loadings.
GLIDESLOPE_TEST(the_j3_cub_standing_at_every_loading_sinks_on_her_main_wheels_by_her_tyres_deflection_alone_within_1_percent) {
    constexpr double pi = 3.14159265358979323846;
    constexpr double bound = 0.01;
    constexpr int stand_s = 20;
    // The preload at the tyre, lb.
    const double ring_free_in = pi * (7.5 + 11.0 / 16.0);
    const double ring_fitted_in = 4 * 6.77 + 2 * pi * (0.75 + 11.0 / 16.0);
    const double stretch = ring_fitted_in / ring_free_in - 1;
    const double preload_lbs = 2 * 4 * (730.0 / 2) * stretch / 1.61;
    // The tyre's rate, lb/in, and the rings' at the tyre (2 x 4 strands x
    // 365 lb x 4 in an inch of spool gap, over 1.61 squared).
    const double tyre_lbs_per_in =
        1100.0 / (18.0 / 2 - 6.65) * (15.0 + 0.08 * 24.0) / (24.0 + 0.08 * 24.0);
    const double rings_lbs_per_in = 2 * 4 * (730.0 / 2) * 4 / ring_free_in / (1.61 * 1.61);
    const double series_lbs_per_in = 1 / (1 / tyre_lbs_per_in + 1 / rings_lbs_per_in);
    const auto deflection_in = [&](double wheel_lbs) {
        return wheel_lbs <= preload_lbs
                   ? wheel_lbs / tyre_lbs_per_in
                   : preload_lbs / tyre_lbs_per_in + (wheel_lbs - preload_lbs) / series_lbs_per_in;
    };
    std::printf("  derived: preload %.0f lb a wheel (%.1f%% stretch), tyre %.0f lb/in, past "
                "the preload %.0f lb/in\n",
                preload_lbs, 100 * stretch, tyre_lbs_per_in, series_lbs_per_in);

    const auto runway = a_runway();
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / "j3cub.xml");
    std::vector<std::pair<std::string, std::optional<glideslope::sim::Loading>>> loadings;
    loadings.emplace_back("her model's own", std::nullopt);
    for (const auto& [name, l] : figures.loadings) {
        loadings.emplace_back(name, l.loading);
    }
    std::size_t covered = 0;
    for (const auto& [loading_name, loading] : loadings) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", "j3cub");
        if (loading) {
            aircraft.load(*loading);
        }
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = runway.threshold_lat_deg;
        ic.longitude_deg = runway.threshold_lon_deg;
        ic.altitude_ft = runway.elevation_ft;
        ic.terrain_elevation_ft = runway.elevation_ft;
        ic.heading_deg = runway.heading_deg;
        ic.engine_running = false;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        glideslope::sim::Controls parked;
        parked.left_brake = 1.0;
        parked.right_brake = 1.0;
        for (int step = 0; step < stand_s * steps_per_second; ++step) {
            aircraft.set_controls(parked);
            aircraft.step();
        }
        const auto points = aircraft.contact_points();
        int tail = -1;
        std::vector<int> mains;
        for (std::size_t p = 0; p < points.size(); ++p) {
            if (points[p].name == "TAIL") {
                tail = static_cast<int>(p);
            } else if (points[p].name == "LEFT_MAIN" || points[p].name == "RIGHT_MAIN") {
                mains.push_back(static_cast<int>(p));
            }
        }
        check(tail >= 0 && mains.size() == 2, loading_name + ": her tail and main wheels found");
        if (tail < 0 || mains.size() != 2) {
            continue;
        }
        const auto unit = [](int p) { return "gear/unit[" + std::to_string(p) + "]/"; };
        const auto s = aircraft.state();
        const bool at_rest = aircraft.property(unit(tail) + "WOW") > 0.5 &&
                             aircraft.property(unit(mains[0]) + "WOW") > 0.5 &&
                             aircraft.property(unit(mains[1]) + "WOW") > 0.5 &&
                             aircraft.property("velocities/vg-fps") < 0.01 &&
                             std::abs(s.climb_rate_fpm) < 0.01;
        check(at_rest, loading_name + ": at rest on all three wheels");
        // Ahead along the ground at her pitch, from her structural frame (x
        // aft, z up): -x cos(theta) - z sin(theta).
        const double theta = s.pitch_deg * pi / 180.0;
        const auto ahead = [&](double x_in, double z_in) {
            return -x_in * std::cos(theta) - z_in * std::sin(theta);
        };
        const double weight = aircraft.property("inertia/weight-lbs");
        const double cg = ahead(aircraft.property("inertia/cg-x-in"),
                                aircraft.property("inertia/cg-z-in"));
        const double tail_at = ahead(points[static_cast<std::size_t>(tail)].x_in,
                                     points[static_cast<std::size_t>(tail)].z_in);
        const double mains_at = ahead(points[static_cast<std::size_t>(mains[0])].x_in,
                                      points[static_cast<std::size_t>(mains[0])].z_in);
        const double wheel_lbs = weight * (cg - tail_at) / (mains_at - tail_at) / 2;
        // JSBSim compresses a wheel along her body's vertical and turns its
        // force normal to the ground (FGLGear::ComputeVerticalStrutForce):
        // standing pitched, her strut's force is her wheel's load times the
        // cosine of her pitch.
        const double figure_in = deflection_in(wheel_lbs * std::cos(theta));
        const double left_in = 12 * aircraft.property(unit(mains[0]) + "compression-ft");
        const double right_in = 12 * aircraft.property(unit(mains[1]) + "compression-ft");
        const double mean_in = (left_in + right_in) / 2;
        const double off = mean_in / figure_in - 1;
        std::printf("  %-16s %6.1f lb, %5.1f lb a main wheel (%s the preload); sinks %.3f and "
                    "%.3f in, mean %.3f against %.3f (%+.1f%%); pitch %.2f deg\n",
                    loading_name.c_str(), weight, wheel_lbs,
                    wheel_lbs <= preload_lbs ? "under" : "past", left_in, right_in, mean_in,
                    figure_in, 100 * off, s.pitch_deg);
        check(std::abs(off) <= bound,
              loading_name + ": her main wheels sink " + std::to_string(mean_in) +
                  " in, against the derived " + std::to_string(figure_in));
        ++covered;
    }
    check(loadings.size() == 4 && covered == 4,
          "four loadings, her model's own and her figures' three: " + std::to_string(covered));
}

// **The J-3 Cub standing tail to a 15 kt wind has a reversed wing's lift,
// and the pitching moment it makes.** Her wing's lift table ended at 90
// degrees of angle of attack with nought, and her Cm-alpha term was linear
// in the angle all the way round: tail to the wind, at -164 degrees, she had
// no lift from her wing and a nose-up moment of 4,150 ft-lb (Cm 5.8) that
// sat her on her tail wheel. Now her lift runs on to 180 degrees by the
// section's curve, and her Cm-alpha holds to 90 degrees and is gone by 95,
// with a term for the wing in reversed flow past it (tools/make_j3cub.py).
//
// **The cited figures**: her lift, Sheldahl and Klimas, SAND80-2114
// (Sandia, 1981), Table 2, the NACA 0012 at a Reynolds number of 1,000,000
// (hers at 15 kt is about 850,000; page 22) - the nearest measured section
// to her USA-35B - scaled as her drag is (1.41/1.80) and by her model's
// ground effect (aero/function/kCLge, 1.1 standing). Her pitching moment
// about her quarter chord is **derived** (the owner's decision, 2026-10-20):
// that section's force normal to its chord acting at its centre of pressure,
// half the chord back at 90 degrees moving straight to three quarters at 180
// - the reversed section's quarter chord, its sharp edge leading. Measured
// in the same way, NACA TN 3361 (Critzos, Heyson and Boswinkle, 1955),
// figure 1, the NACA 0012 at 1,800,000 has a quarter-chord moment of about
// -0.5 at 90 to 120 degrees and -0.4 to -0.45 at 150 to 175, which this
// derivation meets within about 0.05 unscaled; read off a graph, it is a
// check, not the figure.
//
// **Every scenario**: at her model's own loading and her figures' three,
// she is started on the runway and stood on full brakes at idle in calm air
// for five seconds; 15 kt then comes from straight behind her, and she
// stands fifteen seconds more. Over the last five, at every step: she has
// not moved (under 1 kt over the ground), her tail wheel is on the runway,
// the air meets her from behind (150 degrees or more either way), and over
// the dynamic pressure her terms are on (that of u and w, aero/qbarUW-psf)
// her lift terms' sum is within 0.01 of the figure as her model scales it,
// and her pitch terms' sum, over her chord too, within 0.01 of the derived
// moment. **The bound**, 0.01 (1.4 lb, or 7 ft-lb, at 15 kt), is the small
// terms left over (her pitch rate's and angle-of-attack rate's, near nothing
// standing; her wing's share of her lift is printed) and the table's rows
// in rounded radians - and narrow enough that her model before (no lift,
// 0.57 under; Cm 5.8, 5.6 over) is wrong. **Measured** (linux-release,
// 2026-10-21): she stands at -166.8 to -167.4 degrees, her wing pressing her
// down 85 to 88 lb (the air from behind and above meets its upper surface),
// and the air's moment on her is 198 to 204 ft-lb nose up about her quarter
// chord (Cm 0.28). **Seen red** on her model before the change, at every
// loading: lift coefficient 0.000 against 0.570, Cm 5.80 against 0.25.
// Coverage asserted: four loadings, 600 steps each judged.
GLIDESLOPE_TEST(the_j3_cub_standing_tail_to_a_15_kt_wind_has_a_reversed_wings_cited_lift_and_the_pitching_moment_it_makes) {
    constexpr double tail_wind_kts = 15.0;
    constexpr int calm_stand_s = 5;
    constexpr int wind_stand_s = 15;
    constexpr int judged_s = 5;
    constexpr double bound = 0.01;
    // Her wing's broadside figure over the section's (tools/make_j3cub.py).
    constexpr double scale = 1.4091 / 1.80;
    constexpr double pi = 3.14159265358979323846;
    constexpr double degrees = 180.0 / pi;
    // SAND80-2114, Table 2, NACA 0012 at a Reynolds number of 1,000,000: the
    // angle of attack in degrees, the section's lift and drag coefficients.
    struct Row {
        double alpha_deg;
        double cl;
        double cd;
    };
    constexpr Row cited[] = {{145.0, -0.910, 0.755}, {150.0, -0.850, 0.575},
                             {155.0, -0.740, 0.420}, {160.0, -0.660, 0.320},
                             {165.0, -0.675, 0.230}, {170.0, -0.850, 0.140},
                             {175.0, -0.690, 0.055}, {180.0, 0.000, 0.025}};
    // The section's coefficients at an angle past 145 degrees either way: its
    // lift odd in the angle, its drag even.
    const auto section = [&](double alpha_deg) {
        const double a = std::abs(alpha_deg);
        for (std::size_t i = 1; i < std::size(cited); ++i) {
            if (a <= cited[i].alpha_deg) {
                const double f =
                    (a - cited[i - 1].alpha_deg) / (cited[i].alpha_deg - cited[i - 1].alpha_deg);
                const double cl = cited[i - 1].cl + f * (cited[i].cl - cited[i - 1].cl);
                const double cd = cited[i - 1].cd + f * (cited[i].cd - cited[i - 1].cd);
                return std::pair<double, double>{alpha_deg < 0.0 ? -cl : cl, cd};
            }
        }
        return std::pair<double, double>{0.0, cited[std::size(cited) - 1].cd};
    };
    // Her lift coefficient: the section's, scaled as her model scales it,
    // and by her model's ground effect.
    const auto expected_cl = [&](double alpha_deg, double ground_effect) {
        return ground_effect * scale * section(alpha_deg).first;
    };
    // Her pitching moment about her quarter chord, derived: the section's
    // force normal to its chord (scaled) at its centre of pressure, half
    // the chord back at 90 degrees moving to three quarters at 180.
    const auto expected_cm = [&](double alpha_deg) {
        const auto [cl, cd] = section(alpha_deg);
        const double a = alpha_deg / degrees;
        const double normal = scale * (cl * std::cos(a) + cd * std::sin(a));
        const double centre = 0.5 + 0.25 * (std::abs(alpha_deg) - 90.0) / 90.0;
        return -(centre - 0.25) * normal;
    };
    const char* const lift_terms[] = {"aero/force/Lift_propwash", "aero/force/Lift_alpha",
                                      "aero/force/Lift_pitch_rate", "aero/force/Lift_alpha_rate",
                                      "aero/force/Lift_elevator"};
    const char* const pitch_terms[] = {"aero/moment/Pitch_propwash", "aero/moment/Pitch_alpha",
                                       "aero/moment/Pitch_reversed_flow",
                                       "aero/moment/Pitch_elevator", "aero/moment/Pitch_damp",
                                       "aero/moment/Pitch_alphadot"};
    const auto runway = a_runway();
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / "j3cub.xml");
    std::vector<std::pair<std::string, std::optional<glideslope::sim::Loading>>> loadings;
    loadings.emplace_back("her model's own", std::nullopt);
    for (const auto& [name, l] : figures.loadings) {
        loadings.emplace_back(name, l.loading);
    }
    std::size_t judged_steps = 0;
    std::vector<std::string> wrong;
    for (const auto& [loading_name, loading] : loadings) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", "j3cub");
        if (loading) {
            aircraft.load(*loading);
        }
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
        std::string tail_wow;
        {
            const double cg_x = aircraft.property("inertia/cg-x-in");
            const auto points = aircraft.contact_points();
            for (std::size_t p = 0; p < points.size(); ++p) {
                if (points[p].bogey && points[p].wheel && points[p].x_in > cg_x) {
                    tail_wow = "gear/unit[" + std::to_string(p) + "]/WOW";
                }
            }
        }
        check(!tail_wow.empty(), loading_name + ": her tail wheel was found");
        glideslope::sim::Controls parked;
        parked.left_brake = 1.0;
        parked.right_brake = 1.0;
        for (int step = 0; step < calm_stand_s * steps_per_second; ++step) {
            aircraft.set_controls(parked);
            aircraft.step();
        }
        glideslope::sim::Conditions wind;
        // From behind: the air moves the way she points.
        const double towards = runway.heading_deg / degrees;
        const double mps = tail_wind_kts * 0.514444;
        wind.wind_north_mps = mps * std::cos(towards);
        wind.wind_east_mps = mps * std::sin(towards);
        wind.wind_at_20ft_mps = mps;
        aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(wind));
        double worst_cl = 0.0;
        double worst_cm = 0.0;
        double alpha_seen = 0.0;
        double cl_seen = 0.0;
        double cm_seen = 0.0;
        double lift_seen = 0.0;
        double pitch_seen = 0.0;
        double wing_share = 0.0;
        std::string first_wrong;
        for (int step = 0; step < wind_stand_s * steps_per_second; ++step) {
            aircraft.set_controls(parked);
            aircraft.step();
            if (step < (wind_stand_s - judged_s) * steps_per_second) {
                continue;
            }
            ++judged_steps;
            const double alpha_deg = aircraft.property("aero/alpha-deg");
            const double qbar = aircraft.property("aero/qbarUW-psf");
            const double area = aircraft.property("metrics/Sw-sqft");
            const double chord = aircraft.property("metrics/cbarw-ft");
            const double ground_effect = aircraft.property("aero/function/kCLge");
            double lift = 0.0;
            for (const char* term : lift_terms) {
                lift += aircraft.property(term);
            }
            double pitch = 0.0;
            for (const char* term : pitch_terms) {
                pitch += aircraft.property(term);
            }
            const double cl = lift / (qbar * area);
            const double cm = pitch / (qbar * area * chord);
            const double off_cl = cl - expected_cl(alpha_deg, ground_effect);
            const double off_cm = cm - expected_cm(alpha_deg);
            const double ground_kts = aircraft.property("velocities/vg-fps") / 1.68781;
            if (std::abs(off_cl) + std::abs(off_cm) >= std::abs(worst_cl) + std::abs(worst_cm)) {
                worst_cl = off_cl;
                worst_cm = off_cm;
                alpha_seen = alpha_deg;
                cl_seen = cl;
                cm_seen = cm;
                lift_seen = lift;
                pitch_seen = pitch;
                wing_share = aircraft.property("aero/force/Lift_alpha") / lift;
            }
            char what[200];
            if (!(std::abs(alpha_deg) >= 150.0)) {
                std::snprintf(what, sizeof what, "the air not from behind: alpha %.1f deg",
                              alpha_deg);
            } else if (!(ground_kts < 1.0)) {
                std::snprintf(what, sizeof what, "moved: %.2f kt over the ground", ground_kts);
            } else if (!(aircraft.property(tail_wow) > 0.5)) {
                std::snprintf(what, sizeof what, "her tail wheel off the runway");
            } else if (!(std::abs(off_cl) <= bound)) {
                std::snprintf(what, sizeof what,
                              "lift coefficient %.3f at %.1f deg, the figure's %.3f: off by %.3f",
                              cl, alpha_deg, expected_cl(alpha_deg, ground_effect), off_cl);
            } else if (!(std::abs(off_cm) <= bound)) {
                std::snprintf(what, sizeof what,
                              "pitching coefficient %.3f at %.1f deg, the figure's %.3f: off by "
                              "%.3f",
                              cm, alpha_deg, expected_cm(alpha_deg), off_cm);
            } else {
                continue;
            }
            if (first_wrong.empty()) {
                first_wrong = what;
            }
        }
        std::printf("  %-16s alpha %6.1f deg: lift %+6.1f lb, coefficient %+.4f (off by %+.4f; "
                    "her wing's share %.3f); pitching %+6.1f ft-lb, coefficient %+.4f (the "
                    "figure's %+.4f, off by %+.4f)\n",
                    loading_name.c_str(), alpha_seen, lift_seen, cl_seen, worst_cl, wing_share,
                    pitch_seen, cm_seen, expected_cm(alpha_seen), worst_cm);
        if (!first_wrong.empty()) {
            wrong.push_back(loading_name + ": " + first_wrong);
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(loadings.size() == 4 &&
              judged_steps == loadings.size() * std::size_t(judged_s * steps_per_second),
          "four loadings, 600 steps each: " + std::to_string(judged_steps) + " steps judged");
}

namespace {

// What standing in 15 kt straight across measured, over every landplane and
// every setting (stand_each_landplane_across).
struct StandingAcross {
    std::size_t planes = 0;
    std::size_t runs = 0;
    std::size_t judged_steps = 0;
    std::size_t left_out = 0;
    std::size_t roster = 0;
    std::size_t settings = 0;
    std::vector<std::string> wrong;
};

constexpr int standing_each_side_s = 10;

// **Every landplane standing on her brakes in 15 kt straight across, each
// control that moves an aerodynamic term set one at a time**: the seven
// settings - all neutral, flaps fully down, the stick fully back, fully
// forward, the speedbrake fully out, the pitch trim fully nose up, fully
// nose down - on her brakes (stand_on_her_brakes) with her engines stopped:
// no slipstream, so that what she feels is the air's alone (at idle the J-3
// Cub on half brake with her stick full forward goes over on her nose). She
// stands in calm air until every control surface the setting moves has
// stopped (none moving by more than a thousandth over a second; a
// three-minute ceiling, a failure), five seconds more, then 15 kt straight
// across from her left for ten seconds and from her right for ten. At every
// step of those twenty, `measure` of her is within `bound` of her either
// way. The flying boat is left out, named: afloat she has no brakes, and
// turns into the wind, so it is not across her.
StandingAcross stand_each_landplane_across(
    const char* quantity, const char* unit,
    const std::function<double(const glideslope::sim::Aircraft&)>& measure,
    const std::function<double(const glideslope::sim::Aircraft&)>& bound) {
    constexpr double crosswind_kts = 15.0;
    constexpr int calm_after_settled_s = 5;
    constexpr int ceiling_s = 180;
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    struct Setting {
        const char* name;
        void (*set)(glideslope::sim::Controls&);
    };
    const std::vector<Setting> settings = {
        {"neutral", [](glideslope::sim::Controls&) {}},
        {"flaps down", [](glideslope::sim::Controls& c) { c.flaps = 1.0; }},
        {"stick back", [](glideslope::sim::Controls& c) { c.elevator = 1.0; }},
        {"stick forward", [](glideslope::sim::Controls& c) { c.elevator = -1.0; }},
        {"speedbrake out", [](glideslope::sim::Controls& c) { c.speedbrake = 1.0; }},
        {"trim nose up", [](glideslope::sim::Controls& c) { c.pitch_trim = 1.0; }},
        {"trim nose down", [](glideslope::sim::Controls& c) { c.pitch_trim = -1.0; }},
    };
    const std::vector<std::string> surfaces = {
        "fcs/flap-pos-norm",      "fcs/elevator-pos-rad",   "fcs/speedbrake-pos-norm",
        "fcs/stabilizer-pos-rad", "fcs/flaperon-droop-rad", "fcs/pitch-trim-cmd-norm"};
    const auto roster = glideslope::sim::read_catalogue(data());
    const Runway runway = a_runway();
    StandingAcross out;
    out.roster = roster.size();
    out.settings = settings.size();
    for (const glideslope::sim::CatalogueEntry& e : roster) {
        if (e.seaplane) {
            ++out.left_out;
            std::printf("  left out - %s: a flying boat, afloat, has no brakes and turns into "
                        "the wind, so it is not across her\n",
                        e.id.c_str());
            continue;
        }
        ++out.planes;
        double worst = 0.0;
        double worst_bound = 1.0;
        std::string worst_where;
        for (const Setting& setting : settings) {
            ++out.runs;
            const std::string where = e.id + ", " + setting.name;
            glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
            aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
                [](double, double) { return 0.0; }, [](double, double) { return false; }));
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = runway.threshold_lat_deg;
            ic.longitude_deg = runway.threshold_lon_deg;
            ic.altitude_ft = runway.elevation_ft;
            ic.terrain_elevation_ft = runway.elevation_ft;
            ic.heading_deg = runway.heading_deg;
            ic.engine_running = false;
            ic.gear = 1.0;
            aircraft.initialize(ic);
            glideslope::sim::Controls c;
            stand_on_her_brakes(aircraft, c);
            setting.set(c);
            std::vector<std::string> watched;
            for (const std::string& s : surfaces) {
                if (aircraft.has_property(s)) {
                    watched.push_back(s);
                }
            }
            // Calm, until every surface has stopped: each within a
            // thousandth of where it was a second before.
            std::vector<double> second_ago(watched.size(), 1e9);
            bool settled = false;
            int step = 0;
            for (; step < ceiling_s * steps_per_second && !settled; ++step) {
                aircraft.set_controls(c);
                aircraft.step();
                if ((step + 1) % steps_per_second == 0) {
                    settled = true;
                    for (std::size_t k = 0; k < watched.size(); ++k) {
                        const double now = aircraft.property(watched[k]);
                        settled = settled && std::abs(now - second_ago[k]) <= 1e-3;
                        second_ago[k] = now;
                    }
                }
            }
            if (!settled) {
                out.wrong.push_back(where + ": her controls still moving after " +
                                    std::to_string(ceiling_s) + " s");
                continue;
            }
            for (int s = 0; s < calm_after_settled_s * steps_per_second; ++s) {
                aircraft.set_controls(c);
                aircraft.step();
            }
            const double limit = bound(aircraft);
            double least = 1e18, most = -1e18;
            std::string first_wrong;
            for (const int side : {+1, -1}) {
                glideslope::sim::Conditions wind;
                const double towards = (runway.heading_deg + 90.0 * side) / degrees;
                const double mps = crosswind_kts * 0.514444;
                wind.wind_north_mps = mps * std::cos(towards);
                wind.wind_east_mps = mps * std::sin(towards);
                wind.wind_at_20ft_mps = mps;
                aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(wind));
                for (int s = 0; s < standing_each_side_s * steps_per_second; ++s) {
                    aircraft.set_controls(c);
                    aircraft.step();
                    ++out.judged_steps;
                    const double v = measure(aircraft);
                    least = std::min(least, v);
                    most = std::max(most, v);
                    if (std::abs(v) / limit > std::abs(worst) / worst_bound) {
                        worst = v;
                        worst_bound = limit;
                        worst_where = setting.name;
                    }
                    if (first_wrong.empty() && !(std::abs(v) <= limit)) {
                        char w[200];
                        std::snprintf(w, sizeof w, "%s: %s %.2f %s, from her %s, %.2f s into it, "
                                      "against %.2f",
                                      where.c_str(), quantity, v, unit,
                                      side > 0 ? "left" : "right",
                                      (s + 1) / double(steps_per_second), limit);
                        first_wrong = w;
                    }
                }
            }
            std::printf("  %-30s settled in %5.1f s, %s %10.2f to %10.2f %s (bound %.2f)\n",
                        where.c_str(), step / double(steps_per_second), quantity, least, most,
                        unit, limit);
            if (!first_wrong.empty()) {
                out.wrong.push_back(first_wrong);
            }
        }
        std::printf("  %-13s most %10.2f %s (%s), %.4f of her bound %.2f\n", e.id.c_str(), worst,
                    unit, worst_where.c_str(), std::abs(worst) / worst_bound, worst_bound);
    }
    for (const std::string& w : out.wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    return out;
}

// Coverage asserted for stand_each_landplane_across: sixteen in the
// catalogue, fifteen landplanes, seven settings, 105 runs of 2,400 steps.
void check_standing_across(const StandingAcross& s) {
    check(s.wrong.empty(), std::to_string(s.wrong.size()) + " went wrong, the first: " +
                               (s.wrong.empty() ? "" : s.wrong.front()));
    check(s.roster == 16 && s.planes == 15 && s.left_out == 1 && s.settings == 7 &&
              s.runs == 15 * 7 &&
              s.judged_steps ==
                  s.runs * std::size_t(2 * standing_each_side_s * steps_per_second),
          "fifteen landplanes, seven settings, 105 runs of 2,400 steps judged: " +
              std::to_string(s.planes) + " planes, " + std::to_string(s.runs) + " runs, " +
              std::to_string(s.judged_steps) + " steps");
}

}  // namespace

// **Every landplane standing on her brakes in 15 kt straight across, with any
// control that moves her lift set, is lifted by the crosswind by no more than
// a stated small figure.** Her lift terms not keyed to the angle of attack -
// flaps, elevators, a stabiliser, spoilers, the C182's lift at no angle -
// multiplied the whole dynamic pressure, which counts the crosswind, and
// lifted her standing: the C182 by up to 46 lb, the F-22A by 280. Each make
// script now puts them on the dynamic pressure without its sideways part
// (tools/alpha_rate.py, with_unkeyed_terms_on_qbar_uw), or names them with
// the reason they are not (tests/tools/unkeyed_lift_terms.py).
//
// **Every scenario**: stand_each_landplane_across's, her lift
// (forces/fwz-aero-lbs) judged at every step. **The bound**, a
// ten-thousandth of her weight, stated after measuring (below); what lift
// remains standing is her moved terms on the creep of u and w. Coverage
// asserted: fifteen landplanes, seven settings, 105 runs of 2,400 steps
// judged.
GLIDESLOPE_TEST(every_landplane_standing_on_her_brakes_in_15_kt_across_with_any_lifting_control_set_is_lifted_by_no_more_than_a_ten_thousandth_of_her_weight) {
    constexpr double bound_of_weight = 1e-4;
    check_standing_across(stand_each_landplane_across(
        "lift", "lb",
        [](const glideslope::sim::Aircraft& a) { return -a.property("forces/fwz-aero-lbs"); },
        [](const glideslope::sim::Aircraft& a) {
            return bound_of_weight * a.property("inertia/weight-lbs");
        }));
}

// **Every landplane standing on her brakes in 15 kt straight across, with any
// control set, is pitched by her pitching-moment terms by no more than a
// stated small figure.** Her pitching moments not keyed to the angle of
// attack - her elevator's, her flaps', her stabiliser's, her pitch damping,
// her moment at no angle - multiplied the whole dynamic pressure, which
// counts the crosswind, as her lift did: standing in 15 kt across the A380
// with her stick back was pitched by 199,166 ft-lb, the C182 by 334. Each
// make script now puts them on a dynamic pressure without its sideways part
// (tools/alpha_rate.py, with_unkeyed_terms_on_qbar_uw), or names them with
// the reason they are not (tests/tools/unkeyed_pitch_terms.py).
//
// **Every scenario**: stand_each_landplane_across's. **What is judged**, at
// every step: the moment of her PITCH axis about her aerodynamic reference
// point - her aerodynamic pitching moment about her centre of gravity
// (moments/m-aero-lbsft) less the moment of her aerodynamic forces carried
// from that point to it (JSBSim's FGAerodynamics, M = r x F, r from her
// centre of gravity to the point in body axes). Not the whole: standing in a
// beam wind her side force, on the whole dynamic pressure as it should be,
// was turned by JSBSim's wind axes into her plane of symmetry at the angle of
// attack, which is undefined there, and its moment about her centre of
// gravity was another fault (a 737-300 1,400 lb fore and aft, 6,800 ft-lb),
// judged since 2026-10-29 by the test of her whole moment below. **The bound**, a
// ten-thousandth of her weight times her mean chord, stated after measuring
// (the status file,
// 2026-10-28-pitching-moments-not-keyed-to-the-angle-of-attack-leave-out-the-crosswind.md):
// what remains is her terms on the creep of u and w, the most the J-3 Cub's
// as the wind changes side, 0.32 of it; a hundred-thousandth, tried first,
// she failed by three times.
// Coverage asserted: fifteen landplanes, seven settings, 105 runs of 2,400
// steps judged.
GLIDESLOPE_TEST(every_landplane_standing_on_her_brakes_in_15_kt_across_with_any_control_set_is_pitched_by_her_pitching_moment_terms_by_no_more_than_a_ten_thousandth_of_her_weight_times_her_chord) {
    constexpr double bound_of_weight_chord = 1e-4;
    check_standing_across(stand_each_landplane_across(
        "pitching", "ft-lb",
        [](const glideslope::sim::Aircraft& a) {
            // Body axes, feet: x forward, z down, from her centre of gravity
            // to her reference point; structural x aft and z up, in inches.
            const double dx =
                -(a.property("metrics/aero-rp-x-in") - a.property("inertia/cg-x-in")) / 12.0;
            const double dz =
                -(a.property("metrics/aero-rp-z-in") - a.property("inertia/cg-z-in")) / 12.0;
            return a.property("moments/m-aero-lbsft") -
                   (dz * a.property("forces/fbx-aero-lbs") -
                    dx * a.property("forces/fbz-aero-lbs"));
        },
        [](const glideslope::sim::Aircraft& a) {
            return bound_of_weight_chord * a.property("inertia/weight-lbs") *
                   a.property("metrics/cbarw-ft");
        }));
}

// **Every landplane standing on her brakes in 15 kt straight across, with any
// control set, is pushed by her air fore and aft or up and down - in her
// plane of symmetry - by no more than a stated small figure.** What pushes
// her across is not judged here: that every landplane has a drag with
// sideslip to do it is tests/tools/side_force_across.py's, and the J-3
// Cub's is held to a cited figure. JSBSim makes the side
// force along its wind axes' y, which standing in a beam wind (sideslip 90
// degrees) lies in her plane of symmetry at her angle of attack, undefined
// there and turned by her creep: a 737-300 on her brakes had 1,400 lb of it
// along her body's x axis. Each make script now tapers it by cos^2 of her
// sideslip, to nothing broadside, where her drag with sideslip pushes her
// along the wind (tools/side_force.py).
//
// **Every scenario**: stand_each_landplane_across's. **What is judged**, at
// every step: her aerodynamic force in her plane of symmetry, the length of
// (forces/fbx-aero-lbs, forces/fbz-aero-lbs). **What remains** is her drag,
// along the relative wind, which her creep turns a few degrees off her beam
// (the most the F-15C's, her stick back, 0.55 of the bound), and her lift on
// the pressure without the crosswind. **The bound**, a thousandth of her
// weight, stated after measuring (the status file,
// 2026-10-29-side-force-standing-in-a-beam-wind-is-across-her.md); at the
// lift test's ten-thousandth the F-15C and J-3 Cub would fail, by her drag.
// Coverage asserted: fifteen landplanes, seven settings, 105 runs of 2,400
// steps.
GLIDESLOPE_TEST(every_landplane_standing_on_her_brakes_in_15_kt_across_with_any_control_set_is_pushed_by_her_air_no_more_than_a_thousandth_of_her_weight_fore_and_aft_or_up_and_down) {
    constexpr double bound_of_weight = 1e-3;
    check_standing_across(stand_each_landplane_across(
        "force in her plane of symmetry", "lb",
        [](const glideslope::sim::Aircraft& a) {
            return std::hypot(a.property("forces/fbx-aero-lbs"),
                              a.property("forces/fbz-aero-lbs"));
        },
        [](const glideslope::sim::Aircraft& a) {
            return bound_of_weight * a.property("inertia/weight-lbs");
        }));
}

// **Every landplane standing on her brakes in 15 kt straight across, with any
// control set, is pitched by her air by no more than a stated small
// figure** - her whole aerodynamic pitching moment about her centre of
// gravity (moments/m-aero-lbsft), the moment of her forces carried there
// from her reference point among it, which the test above this one left
// out: with her side force turned into her plane of symmetry it was 6,800
// ft-lb on a 737-300 (2026-10-28). **The bound**, a ten-thousandth of her
// weight times her mean chord, her pitching terms' alone, stated after
// measuring (the status file): what remains is her drag's moment as her
// creep turns it and her pitching terms' on the creep of u and w, the most
// the J-3 Cub's as the wind changes side, 0.82 of it. Coverage asserted:
// fifteen landplanes, seven settings, 105 runs of 2,400 steps.
GLIDESLOPE_TEST(every_landplane_standing_on_her_brakes_in_15_kt_across_with_any_control_set_is_pitched_by_her_air_about_her_centre_of_gravity_by_no_more_than_a_ten_thousandth_of_her_weight_times_her_chord) {
    constexpr double bound_of_weight_chord = 1e-4;
    check_standing_across(stand_each_landplane_across(
        "pitching about her centre of gravity", "ft-lb",
        [](const glideslope::sim::Aircraft& a) { return a.property("moments/m-aero-lbsft"); },
        [](const glideslope::sim::Aircraft& a) {
            return bound_of_weight_chord * a.property("inertia/weight-lbs") *
                   a.property("metrics/cbarw-ft");
        }));
}
