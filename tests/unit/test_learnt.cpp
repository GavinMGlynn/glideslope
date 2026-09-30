#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/learnt.hpp"
#include "sim/terrain.hpp"
#include "sim/weather.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using glideslope::sim::LandingReadings;
using glideslope::sim::LearntLander;
using glideslope::sim::LearntPolicy;
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

std::filesystem::path policy_file() {
    return data() / "rl" / "c172p-landing.txt";
}

std::filesystem::path parity_file() {
    return std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) / "data" / "rl" /
           "c172p-landing-parity.txt";
}

// The runway sim::Lander's tests land on, and the training flew to
// (tools/rl/landing.py's Runway): sea level, pointing 070.
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

// **The starts the policy is judged from**, tools/rl/landing.py's
// `verification_starts`: a final-approach gate two miles out; on the
// centreline and 50 m either side of it; on the glidepath and 15 m - about
// fifty feet - above and below it; in calm air and a ten-knot crosswind from
// either side. At the reference speed, pointing down the runway, trimmed.
struct Start {
    double across_m;
    double high_m;
    double crosswind_kts; // from the left, positive
};

std::vector<Start> starts() {
    std::vector<Start> out;
    for (const double wind : {0.0, 10.0, -10.0}) {
        for (const double across : {-50.0, 0.0, 50.0}) {
            for (const double high : {-15.0, 0.0, 15.0}) {
                out.push_back({across, high, wind});
            }
        }
    }
    return out;
}

std::string named(const Start& s) {
    char text[96];
    std::snprintf(text, sizeof text, "across %+.0f m, high %+.0f m, crosswind %+.0f kt",
                  s.across_m, s.high_m, s.crosswind_kts);
    return text;
}

struct Landing {
    bool touched = false;
    double sink_fpm = 0.0;
    double across_m = 0.0;
    double along_m = 0.0;
    double highest_after_touch_ft = 0.0;
    double worst_roll_after_touch_deg = 0.0;
    double least_pitch_after_touch_deg = 0.0;
    long decisions = 0;
};

// **Flown from `start` by the policy**, to five seconds after the wheels
// first touch - the time it was trained to - or five minutes, the longest a
// training flight was let run.
Landing land(const LearntPolicy& policy, const Start& start) {
    const Runway runway = a_runway();
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", policy.aircraft);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    if (start.crosswind_kts != 0.0) {
        // From the left, as tests/unit/test_lander.cpp's crosswind: blowing
        // towards the right of the landing direction.
        glideslope::sim::Conditions conditions;
        const double towards = (runway.heading_deg + 90.0) / degrees;
        const double mps = start.crosswind_kts * 0.514444;
        conditions.wind_north_mps = mps * std::cos(towards);
        conditions.wind_east_mps = mps * std::sin(towards);
        aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(conditions));
    }
    const double out_m = 2.0 * metres_per_nm;
    const double heading = runway.heading_deg / degrees;
    const double north_m = -out_m * std::cos(heading) - start.across_m * std::sin(heading);
    const double east_m = -out_m * std::sin(heading) + start.across_m * std::cos(heading);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg +
                      north_m / metres_per_degree_latitude(runway.threshold_lat_deg);
    ic.longitude_deg = runway.threshold_lon_deg +
                       east_m / metres_per_degree_longitude(runway.threshold_lat_deg);
    ic.altitude_ft = runway.elevation_ft +
                     ((out_m + policy.aim_m) * std::tan(policy.glidepath_deg / degrees) +
                      start.high_m) *
                         feet_per_metre;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = policy.vref_kts;
    ic.engine_running = true;
    ic.flaps = policy.flaps;
    ic.flight_path_deg = -policy.glidepath_deg;
    ic.trim = true;
    aircraft.initialize(ic);

    LearntLander lander(aircraft, runway, policy);
    Landing out;
    double touch_agl_ft = 0.0;
    long touch_tick = -1;
    for (long tick = 0; tick < 300L * steps_per_second; ++tick) {
        aircraft.set_controls(lander.fly());
        aircraft.step();
        if (lander.touched()) {
            if (touch_tick < 0) {
                touch_tick = tick;
                touch_agl_ft = aircraft.property("position/h-agl-ft");
            }
            const auto s = aircraft.state();
            out.highest_after_touch_ft = std::max(
                out.highest_after_touch_ft, aircraft.property("position/h-agl-ft") - touch_agl_ft);
            out.worst_roll_after_touch_deg =
                std::max(out.worst_roll_after_touch_deg, std::abs(s.roll_deg));
            out.least_pitch_after_touch_deg =
                std::min(out.least_pitch_after_touch_deg, s.pitch_deg);
            if (tick - touch_tick >= 5L * steps_per_second) {
                break;
            }
        }
    }
    out.touched = lander.touched();
    out.sink_fpm = lander.touchdown_sink_fpm();
    out.across_m = lander.touchdown_across_m();
    out.along_m = lander.touchdown_along_m();
    out.decisions = lander.decisions();
    std::printf("  %s: %.0f ft/min, %+.2f m across, %.0f m along; after the touch rose "
                "%.1f ft, banked %.1f, nose down to %.1f; %ld decisions\n",
                named(start).c_str(), out.sink_fpm, out.across_m, out.along_m,
                out.highest_after_touch_ft, out.worst_roll_after_touch_deg,
                out.least_pitch_after_touch_deg, out.decisions);
    std::fflush(stdout);
    return out;
}

std::vector<double> numbers(const std::string& line) {
    std::istringstream in(line);
    std::string word;
    in >> word;
    std::vector<double> out;
    double v = 0.0;
    while (in >> v) {
        out.push_back(v);
    }
    return out;
}

} // namespace

// **What the policy sees and does in the simulation is what it saw and did
// in training.** tools/rl/export.py flew the committed policy in JSBSim's
// Python bindings and recorded, decision by decision, the JSBSim readings,
// the last action, the observation the training made of them and the
// action the policy file gave - in the approach, the flare and on the
// ground. Here the simulation's own functions make the observation from the
// same readings and take the action from the same file: they must agree to
// a billionth.
GLIDESLOPE_TEST(the_learnt_landing_sees_and_acts_in_the_simulation_as_it_did_in_training) {
    const LearntPolicy policy = LearntPolicy::read(policy_file());
    std::ifstream in(parity_file());
    check(static_cast<bool>(in), "the parity fixture " + parity_file().string() + " is there");
    Runway runway;
    std::size_t said = 0;
    std::size_t walked = 0;
    std::size_t on_the_ground = 0;
    double worst_obs = 0.0;
    double worst_action = 0.0;
    constexpr std::size_t n = LearntPolicy::actions;
    constexpr std::size_t m = LandingReadings::count;
    constexpr std::size_t o = LearntPolicy::observations;
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("runway ", 0) == 0) {
            const std::vector<double> v = numbers(line);
            check(v.size() == 5, "the runway line has five numbers");
            runway.threshold_lat_deg = v[0];
            runway.threshold_lon_deg = v[1];
            runway.elevation_ft = v[2];
            runway.heading_deg = v[3];
            runway.length_m = v[4];
        } else if (line.rfind("cases ", 0) == 0) {
            said = static_cast<std::size_t>(numbers(line).at(0));
        } else if (line.rfind("case ", 0) == 0) {
            const std::vector<double> v = numbers(line);
            check(v.size() == n + m + o + n,
                  "a case has " + std::to_string(n + m + o + n) + " numbers, not " +
                      std::to_string(v.size()));
            std::array<double, n> previous{};
            std::copy_n(v.begin(), n, previous.begin());
            LandingReadings r;
            std::copy_n(v.begin() + n, m, r.values.begin());
            const std::vector<double> obs =
                glideslope::sim::landing_observation(r, runway, policy, previous);
            check(obs.size() == o, "the observation has " + std::to_string(o) + " numbers");
            for (std::size_t k = 0; k < o; ++k) {
                const double want = v[n + m + k];
                worst_obs = std::max(worst_obs, std::abs(obs[k] - want));
                check(std::abs(obs[k] - want) <= 1e-9,
                      "case " + std::to_string(walked) + ": observation " + std::to_string(k) +
                          " is " + std::to_string(obs[k]) + " here and " +
                          std::to_string(want) + " in training");
            }
            const auto action = policy.act(obs);
            for (std::size_t k = 0; k < n; ++k) {
                const double want = v[n + m + o + k];
                worst_action = std::max(worst_action, std::abs(action[k] - want));
                check(std::abs(action[k] - want) <= 1e-9,
                      "case " + std::to_string(walked) + ": action " + std::to_string(k) +
                          " is " + std::to_string(action[k]) + " here and " +
                          std::to_string(want) + " in training");
            }
            on_the_ground += r.values[15] > 0.5 ? 1U : 0U;
            ++walked;
        }
    }
    std::printf("%zu cases, %zu on the ground; worst difference %.3g in an observation, "
                "%.3g in an action\n",
                walked, on_the_ground, worst_obs, worst_action);
    check(said > 0 && walked == said,
          "every case the fixture says it has was compared: " + std::to_string(walked) +
              " of " + std::to_string(said));
    check(on_the_ground > 0, "some cases are on the ground, where the wheels are part of it");

    // The controls an action gives, one at a time.
    const auto c = glideslope::sim::landing_controls({0.25, -0.5, 0.75, -1.0}, policy);
    check(c.elevator == 0.25 && c.aileron == -0.5 && c.rudder == 0.75 && c.throttle == 0.0,
          "an action is the elevator, the ailerons, the rudder and the throttle, in order");
    check(glideslope::sim::landing_controls({0, 0, 0, 1.0}, policy).throttle == 1.0,
          "the throttle runs from -1 closed to 1 open");
    check(c.flaps == policy.flaps && c.pitch_trim == 0.0 && c.left_brake == 0.0 &&
              c.right_brake == 0.0,
          "the flaps are the landing flap, and there is no trim and no brake");
}

// **The simulation flies the committed policy to the landing training
// flew it to, from every start, and every one of those touches down on the
// runway under 300 ft/min and stays down.** What the item's verification
// also asks - within 5 m of the centreline - the policy does not yet do
// (docs/PROJECT_STATUS.md): a ten-knot crosswind carries it 9 to 21 m off
// the centreline, and one start in calm air 6 m. This holds what it does do, and
// that the simulation's controller is the trained one flown whole - not just
// one decision at a time, as the test above holds it: tools/rl/export.py flew
// the same 27 starts in JSBSim's Python bindings and recorded where each
// touched. The two JSBSims are the same version on the same model, but not
// the same ground or atmosphere code around it - this simulation's terrain
// callback and weather, JSBSim's own there - so they agree closely, not
// exactly: within 10 ft/min, 0.5 m across and 5 m along (measured on
// 2026-09-30: 0.4 ft/min, 0.08 m and 0.6 m at most).
GLIDESLOPE_TEST(the_learnt_policy_touches_down_gently_on_the_runway_from_every_start_as_it_did_in_training) {
    const LearntPolicy policy = LearntPolicy::read(policy_file());
    check(policy.aircraft == "c172p", "the policy is the C172P's");
    const std::vector<Start> all = starts();
    // What training's flights did, by start.
    std::vector<std::vector<double>> trained;
    std::size_t said = 0;
    {
        std::ifstream in(parity_file());
        for (std::string line; std::getline(in, line);) {
            if (line.rfind("flights ", 0) == 0) {
                said = static_cast<std::size_t>(numbers(line).at(0));
            } else if (line.rfind("flight ", 0) == 0) {
                std::vector<double> v = numbers(line);
                check(v.size() == 8, "a flight has its index and seven figures");
                check(static_cast<std::size_t>(v[0]) == trained.size(),
                      "the flights are in the starts' order");
                trained.emplace_back(v.begin() + 1, v.end());
            }
        }
    }
    check(said == all.size() && trained.size() == all.size(),
          "training flew every start this does: " + std::to_string(trained.size()) + " of " +
              std::to_string(all.size()));
    std::size_t flown = 0;
    double worst_sink = 0.0;
    double worst_across = 0.0;
    double least_along = 1e9;
    double most_sink_apart = 0.0;
    double most_across_apart = 0.0;
    double most_along_apart = 0.0;
    std::vector<std::string> failures;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const Start& s = all[i];
        const Landing l = land(policy, s);
        const std::vector<double>& t = trained[i];
        ++flown;
        std::vector<std::string> wrong;
        if (!l.touched || t[0] < 0.5) {
            wrong.push_back(l.touched ? "never touched in training" : "never touched down");
        } else {
            worst_sink = std::max(worst_sink, l.sink_fpm);
            worst_across = std::max(worst_across, std::abs(l.across_m));
            least_along = std::min(least_along, l.along_m);
            most_sink_apart = std::max(most_sink_apart, std::abs(l.sink_fpm - t[1]));
            most_across_apart = std::max(most_across_apart, std::abs(l.across_m - t[2]));
            most_along_apart = std::max(most_along_apart, std::abs(l.along_m - t[3]));
            if (!(l.sink_fpm < 300.0)) {
                wrong.push_back("sank " + std::to_string(l.sink_fpm) + " ft/min, not under 300");
            }
            if (!(l.along_m >= 0.0 && l.along_m <= a_runway().length_m)) {
                wrong.push_back("touched " + std::to_string(l.along_m) +
                                " m along a runway of " +
                                std::to_string(a_runway().length_m));
            }
            if (!(std::abs(l.sink_fpm - t[1]) <= 10.0)) {
                wrong.push_back("sank " + std::to_string(l.sink_fpm) + " ft/min, and " +
                                std::to_string(t[1]) + " in training");
            }
            if (!(std::abs(l.across_m - t[2]) <= 0.5)) {
                wrong.push_back("touched " + std::to_string(l.across_m) + " m across, and " +
                                std::to_string(t[2]) + " in training");
            }
            if (!(std::abs(l.along_m - t[3]) <= 5.0)) {
                wrong.push_back("touched " + std::to_string(l.along_m) + " m along, and " +
                                std::to_string(t[3]) + " in training");
            }
            // Down and upright after it, as sim::Lander's landings are held.
            if (!(l.highest_after_touch_ft < 3.0)) {
                wrong.push_back("went " + std::to_string(l.highest_after_touch_ft) +
                                " ft back into the air");
            }
            if (!(l.worst_roll_after_touch_deg < 15.0)) {
                wrong.push_back("banked " + std::to_string(l.worst_roll_after_touch_deg) +
                                " degrees on the ground");
            }
            if (!(l.least_pitch_after_touch_deg > -10.0)) {
                wrong.push_back("put the nose " +
                                std::to_string(l.least_pitch_after_touch_deg) + " degrees down");
            }
        }
        if (!wrong.empty()) {
            std::string text = "from " + named(s) + ":";
            for (const std::string& w : wrong) {
                text += " " + w + ";";
            }
            failures.push_back(text);
        }
    }
    std::printf("%zu starts flown; worst sink %.0f ft/min, worst %.2f m across, least %.0f m "
                "along; apart from training by at most %.1f ft/min, %.2f m across, %.1f m "
                "along\n",
                flown, worst_sink, worst_across, least_along, most_sink_apart,
                most_across_apart, most_along_apart);
    check(all.size() == 27, "the starts are 3 offsets across, 3 heights and 3 winds: 27, not " +
                                std::to_string(all.size()));
    check(flown == all.size(), "every start was flown: " + std::to_string(flown) + " of " +
                                   std::to_string(all.size()));
    std::string listed;
    for (const std::string& f : failures) {
        listed += "\n    " + f;
    }
    check(failures.empty(), std::to_string(failures.size()) + " of " + std::to_string(flown) +
                                " landings were not as trained, or not gentle:" + listed);
}

// **A policy file that does not fit is refused**, not flown: one for another
// number of observations, one cut short, one with a word for a weight.
GLIDESLOPE_TEST(a_policy_file_that_does_not_fit_the_simulation_is_refused) {
    std::ifstream in(policy_file());
    std::stringstream whole;
    whole << in.rdbuf();
    const std::string good = whole.str();
    check(good.find("observations 21") != std::string::npos, "the policy says 21 observations");
    const auto dir = std::filesystem::temp_directory_path() / "glideslope_learnt_test";
    std::filesystem::create_directories(dir);
    const auto refused = [&](const std::string& text, const std::string& what) {
        const auto file = dir / "policy.txt";
        {
            std::ofstream out(file, std::ios::binary);
            out << text;
        }
        bool threw = false;
        try {
            (void)LearntPolicy::read(file);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, what + " is refused");
    };
    std::string wrong = good;
    wrong.replace(wrong.find("observations 21"), 15, "observations 22");
    refused(wrong, "a policy for 22 observations");
    refused(good.substr(0, good.size() / 2), "a policy cut in half");
    std::string word = good;
    const auto bias = word.find("\nbias ");
    word.replace(bias + 6, 1, "x");
    refused(word, "a policy with a word for a number");
    refused("", "an empty file");
    std::filesystem::remove_all(dir);
    // And the good one is read: three layers, 21 in and 4 out.
    const LearntPolicy p = LearntPolicy::read(policy_file());
    check(!p.layers.empty() && p.layers.front().inputs == 21 && p.layers.back().outputs == 4,
          "the committed policy takes 21 observations and gives 4 actions");
}
