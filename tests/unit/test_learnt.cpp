#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/controller.hpp"
#include "sim/lander.hpp"
#include "sim/learnt.hpp"
#include "sim/terrain.hpp"
#include "sim/weather.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using glideslope::sim::ApproachSpeeds;
using glideslope::sim::Controls;
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

// **What "on the runway" is at the stop**: within 15 m of the centreline, the
// edge of a 30 m runway - as narrow as the runways a C172P is flown from -
// and between the threshold and the far end.
constexpr double runway_half_width_m = 15.0;

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

std::shared_ptr<const LearntPolicy> the_policy() {
    return std::make_shared<const LearntPolicy>(LearntPolicy::read(policy_file()));
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
// **The committed checkpoint was chosen on other starts** (landing.py's
// `held_out_starts`), never these. Its lineage was not blind of them: a
// 21-input ancestor, thirteen million decisions before it, was chosen among
// checkpoints by flying these starts (the policy file's header).
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

// The C172P at `start`, trimmed on the glidepath, in its wind.
// `fuel_lbs` in each of her two tanks; below nothing, as the model has them - full.
std::unique_ptr<glideslope::sim::Aircraft> at(const LearntPolicy& policy, const Start& start,
                                              double fuel_lbs = -1.0) {
    const Runway runway = a_runway();
    auto aircraft = std::make_unique<glideslope::sim::Aircraft>(data() / "jsbsim", policy.aircraft);
    aircraft->set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    if (start.crosswind_kts != 0.0) {
        // From the left, as tests/unit/test_lander.cpp's crosswind: blowing
        // towards the right of the landing direction.
        glideslope::sim::Conditions conditions;
        const double towards = (runway.heading_deg + 90.0) / degrees;
        const double mps = start.crosswind_kts * 0.514444;
        conditions.wind_north_mps = mps * std::cos(towards);
        conditions.wind_east_mps = mps * std::sin(towards);
        aircraft->set_weather(std::make_shared<glideslope::sim::SteadyWeather>(conditions));
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
    if (fuel_lbs >= 0.0) {
        glideslope::sim::Loading loading;
        loading.tank_lbs = {{0, fuel_lbs}, {1, fuel_lbs}};
        aircraft->load(loading);
    }
    aircraft->initialize(ic);
    return aircraft;
}

ApproachSpeeds speeds() {
    return glideslope::sim::approach_speeds(data(), "c172p");
}

struct Landing {
    bool touched = false;
    double sink_fpm = 0.0;
    double across_m = 0.0;
    double along_m = 0.0;
    double highest_after_touch_ft = 0.0;
    double worst_roll_after_touch_deg = 0.0;
    double least_pitch_after_touch_deg = 0.0;
    bool stopped = false;
    double stopped_across_m = 0.0;
    double stopped_along_m = 0.0;
    long decisions = 0;
};

// What a flight did after the touch, step by step.
struct AfterTouch {
    long touch_tick = -1;
    double touch_agl_ft = 0.0;
    void see(const glideslope::sim::Aircraft& aircraft, long tick, Landing& out) {
        if (touch_tick < 0) {
            touch_tick = tick;
            touch_agl_ft = aircraft.property("position/h-agl-ft");
        }
        // The first five seconds after the touch, as training judged them
        // and sim::Lander's landings are held.
        if (tick - touch_tick > 5L * steps_per_second) {
            return;
        }
        const auto s = aircraft.state();
        out.highest_after_touch_ft = std::max(
            out.highest_after_touch_ft, aircraft.property("position/h-agl-ft") - touch_agl_ft);
        out.worst_roll_after_touch_deg =
            std::max(out.worst_roll_after_touch_deg, std::abs(s.roll_deg));
        out.least_pitch_after_touch_deg = std::min(out.least_pitch_after_touch_deg, s.pitch_deg);
    }
};

void print(const std::string& name, const Landing& l) {
    std::printf("  %s: %.0f ft/min, %+.2f m across, %.0f m along; after the touch rose "
                "%.1f ft, banked %.1f, nose down to %.1f; %s %.0f m along, %+.2f m across; "
                "%ld decisions\n",
                name.c_str(), l.sink_fpm, l.across_m, l.along_m, l.highest_after_touch_ft,
                l.worst_roll_after_touch_deg, l.least_pitch_after_touch_deg,
                l.stopped ? "stopped" : "not stopped by", l.stopped_along_m, l.stopped_across_m,
                l.decisions);
    std::fflush(stdout);
}

// **Flown from `start` by the learnt lander**: to five seconds after the
// wheels first touch or, `to_stop`, on through the rollout to the stop. Five minutes to the touch at most, the
// longest a training flight was let run, and two more to the stop.
Landing land(const std::shared_ptr<const LearntPolicy>& policy, const Start& start,
             bool to_stop, double fuel_lbs = -1.0) {
    auto aircraft = at(*policy, start, fuel_lbs);
    LearntLander lander(*aircraft, a_runway(), policy, speeds());
    Landing out;
    AfterTouch after;
    for (long tick = 0; tick < 420L * steps_per_second; ++tick) {
        aircraft->set_controls(lander.fly());
        aircraft->step();
        if (lander.touched()) {
            after.see(*aircraft, tick, out);
            if (!to_stop && tick - after.touch_tick >= 5L * steps_per_second) {
                break;
            }
        } else if (tick >= 300L * steps_per_second) {
            break;
        }
        if (lander.stage() == LearntLander::Stage::stopped) {
            out.stopped = true;
            out.stopped_along_m = -lander.rollout().along_m();
            out.stopped_across_m = lander.rollout().across_m();
            break;
        }
    }
    out.touched = lander.touched();
    out.sink_fpm = lander.touchdown_sink_fpm();
    out.across_m = lander.touchdown_across_m();
    out.along_m = lander.touchdown_along_m();
    out.decisions = lander.decisions();
    print(named(start), out);
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

// Every way a landing falls short of the item's verification: touched on
// the runway, within 5 m of the centreline - asked only when `centreline` -
// sinking under 300 ft/min, down and upright for the five seconds after, as
// sim::Lander's are held.
std::vector<std::string> short_of_the_limits(const Landing& l, bool centreline) {
    std::vector<std::string> wrong;
    if (!l.touched) {
        wrong.push_back("never touched down");
        return wrong;
    }
    if (!(l.sink_fpm < 300.0)) {
        wrong.push_back("sank " + std::to_string(l.sink_fpm) + " ft/min, not under 300");
    }
    if (centreline && !(std::abs(l.across_m) <= 5.0)) {
        wrong.push_back(std::to_string(l.across_m) + " m from the centreline, not within 5");
    }
    if (!(l.along_m >= 0.0 && l.along_m <= a_runway().length_m)) {
        wrong.push_back("touched " + std::to_string(l.along_m) + " m along the runway");
    }
    if (!(l.highest_after_touch_ft < 3.0)) {
        wrong.push_back("went " + std::to_string(l.highest_after_touch_ft) +
                        " ft back into the air");
    }
    if (!(l.worst_roll_after_touch_deg < 15.0)) {
        wrong.push_back("banked " + std::to_string(l.worst_roll_after_touch_deg) +
                        " degrees on the ground");
    }
    if (!(l.least_pitch_after_touch_deg > -10.0)) {
        wrong.push_back("put the nose " + std::to_string(l.least_pitch_after_touch_deg) +
                        " degrees down");
    }
    return wrong;
}

// And whether it stopped on the runway.
std::vector<std::string> not_stopped_on_the_runway(const Landing& l) {
    std::vector<std::string> wrong;
    if (!l.stopped) {
        wrong.push_back("did not stop");
    } else if (!(l.stopped_along_m >= 0.0 && l.stopped_along_m <= a_runway().length_m &&
                 std::abs(l.stopped_across_m) <= runway_half_width_m)) {
        wrong.push_back("stopped " + std::to_string(l.stopped_along_m) + " m along and " +
                        std::to_string(l.stopped_across_m) + " m across, off the runway");
    }
    return wrong;
}

void none_wrong(const std::vector<std::string>& failures, std::size_t flown,
                const std::string& what) {
    std::string listed;
    for (const std::string& f : failures) {
        listed += "\n    " + f;
    }
    check(failures.empty(), std::to_string(failures.size()) + " of " + std::to_string(flown) +
                                " " + what + ":" + listed);
}

} // namespace

// **What the policy sees and does in the simulation is what it saw and did
// in training.** tools/rl/export.py flew the committed policy in JSBSim's
// Python bindings and recorded, decision by decision, the JSBSim readings,
// the last action and the remembered drift, the observation the training
// made of them and the action the policy file gave - in the approach, the
// flare and on the ground. Here the simulation's own functions make the
// observation from the same readings and take the action from the same file:
// they must agree to a billionth.
GLIDESLOPE_TEST(the_learnt_landing_sees_and_acts_in_the_simulation_as_it_did_in_training) {
    const LearntPolicy policy = LearntPolicy::read(policy_file());
    std::ifstream in(parity_file());
    check(static_cast<bool>(in), "the parity fixture " + parity_file().string() + " is there");
    Runway runway;
    std::size_t said = 0;
    std::size_t walked = 0;
    std::size_t on_the_ground = 0;
    std::size_t drifting = 0;
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
            check(v.size() == n + 1 + m + o + n,
                  "a case has " + std::to_string(n + 1 + m + o + n) + " numbers, not " +
                      std::to_string(v.size()));
            std::array<double, n> previous{};
            std::copy_n(v.begin(), n, previous.begin());
            const double integral = v[n];
            LandingReadings r;
            std::copy_n(v.begin() + n + 1, m, r.values.begin());
            const std::vector<double> obs =
                glideslope::sim::landing_observation(r, runway, policy, previous, integral);
            check(obs.size() == o, "the observation has " + std::to_string(o) + " numbers");
            for (std::size_t k = 0; k < o; ++k) {
                const double want = v[n + 1 + m + k];
                worst_obs = std::max(worst_obs, std::abs(obs[k] - want));
                check(std::abs(obs[k] - want) <= 1e-9,
                      "case " + std::to_string(walked) + ": observation " + std::to_string(k) +
                          " is " + std::to_string(obs[k]) + " here and " +
                          std::to_string(want) + " in training");
            }
            const auto action = policy.act(obs);
            for (std::size_t k = 0; k < n; ++k) {
                const double want = v[n + 1 + m + o + k];
                worst_action = std::max(worst_action, std::abs(action[k] - want));
                check(std::abs(action[k] - want) <= 1e-9,
                      "case " + std::to_string(walked) + ": action " + std::to_string(k) +
                          " is " + std::to_string(action[k]) + " here and " +
                          std::to_string(want) + " in training");
            }
            on_the_ground += r.values[15] > 0.5 ? 1U : 0U;
            drifting += std::abs(integral) > 1.0 ? 1U : 0U;
            ++walked;
        }
    }
    std::printf("%zu cases, %zu on the ground, %zu remembering drift; worst difference %.3g "
                "in an observation, %.3g in an action\n",
                walked, on_the_ground, drifting, worst_obs, worst_action);
    check(said > 0 && walked == said,
          "every case the fixture says it has was compared: " + std::to_string(walked) +
              " of " + std::to_string(said));
    check(on_the_ground > 0, "some cases are on the ground, where the wheels are part of it");
    check(drifting > 0, "some cases remember a drift, which is part of it");

    // The memory of the drift: a decision's time, a tenth of a second, of
    // the distance across added, and what was there decayed by the ten-second
    // memory - tools/rl/landing.py's `remember`.
    const double once = glideslope::sim::remember_drift(0.0, 10.0, policy);
    const double twice = glideslope::sim::remember_drift(once, 10.0, policy);
    check(std::abs(once - 1.0) < 1e-12 && std::abs(twice - (std::exp(-0.01) + 1.0)) < 1e-12,
          "the drift remembered is 10 m for a tenth of a second, decaying over ten");

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

// **The wind the policy sees is the wind that blows**, as its instruments
// would estimate it - ground velocity less the true airspeed along the
// heading and sideslip - and not JSBSim's own wind, which it never reads.
// Flown in each steady wind from the gate for ten seconds by the policy -
// hands off, the propeller's torque banked her twenty degrees, and a turn is
// not what the estimate is for - in ten knots across from either side, five
// down the runway either way, and calm. The estimate leaves out the flight
// path's slope, three degrees, and the bank's share of the sideslip, which
// on the approach are well under the tolerance of 0.3 m/s.
GLIDESLOPE_TEST(the_wind_the_learnt_landing_estimates_is_the_steady_wind_blowing) {
    const auto flown_by = the_policy();
    const LearntPolicy& policy = *flown_by;
    const Runway runway = a_runway();
    struct Wind {
        double across_kts; // blowing towards the right of the landing direction
        double along_kts;  // blowing down the runway: a tailwind
    };
    const std::vector<Wind> winds{{0.0, 0.0}, {10.0, 0.0}, {-10.0, 0.0}, {0.0, 5.0}, {0.0, -5.0}};
    std::size_t flown = 0;
    for (const Wind& wind : winds) {
        auto aircraft = at(policy, Start{0.0, 0.0, 0.0});
        const double h = runway.heading_deg / degrees;
        const double right = h + 90.0 / degrees;
        const double across = wind.across_kts * 0.514444;
        const double along = wind.along_kts * 0.514444;
        glideslope::sim::Conditions conditions;
        conditions.wind_north_mps = across * std::cos(right) + along * std::cos(h);
        conditions.wind_east_mps = across * std::sin(right) + along * std::sin(h);
        aircraft->set_weather(std::make_shared<glideslope::sim::SteadyWeather>(conditions));
        LearntLander lander(*aircraft, runway, flown_by, speeds());
        for (int tick = 0; tick < 10 * steps_per_second; ++tick) {
            aircraft->set_controls(lander.fly());
            aircraft->step();
        }
        const std::vector<double> obs = glideslope::sim::landing_observation(
            LandingReadings::of(*aircraft), runway, policy, {}, 0.0);
        const double seen_across = obs.at(22) * 5.0;
        const double seen_along = obs.at(23) * 5.0;
        std::printf("  blowing %+.2f m/s across and %+.2f along; estimated %+.3f and %+.3f\n",
                    across, along, seen_across, seen_along);
        check(std::abs(seen_across - across) < 0.3 && std::abs(seen_along - along) < 0.3,
              "the wind across and along is estimated within 0.3 m/s: " +
                  std::to_string(seen_across) + " for " + std::to_string(across) + ", " +
                  std::to_string(seen_along) + " for " + std::to_string(along));
        ++flown;
    }
    check(flown == 5, "all five winds were flown: " + std::to_string(flown));
}

// **The simulation flies the committed policy to the landing training flew
// it to, from every start** - not just one decision at a time, as the test
// above holds it: tools/rl/export.py flew the same 27 starts in JSBSim's
// Python bindings and recorded where each touched. The two JSBSims are the
// same version on the same model, but not the same ground or atmosphere code
// around it - this simulation's terrain callback and weather, JSBSim's own
// there - so they agree closely, not exactly: within 10 ft/min, 0.5 m across
// and 5 m along.
GLIDESLOPE_TEST(the_learnt_policy_touches_down_in_the_simulation_where_it_did_in_training) {
    const auto policy = the_policy();
    const std::vector<Start> all = starts();
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
    double most_sink_apart = 0.0;
    double most_across_apart = 0.0;
    double most_along_apart = 0.0;
    std::vector<std::string> failures;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const Landing l = land(policy, all[i], false);
        const std::vector<double>& t = trained[i];
        ++flown;
        std::vector<std::string> wrong;
        if (!l.touched || t[0] < 0.5) {
            wrong.push_back(l.touched ? "never touched in training" : "never touched down");
        } else {
            most_sink_apart = std::max(most_sink_apart, std::abs(l.sink_fpm - t[1]));
            most_across_apart = std::max(most_across_apart, std::abs(l.across_m - t[2]));
            most_along_apart = std::max(most_along_apart, std::abs(l.along_m - t[3]));
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
        }
        if (!wrong.empty()) {
            std::string text = "from " + named(all[i]) + ":";
            for (const std::string& w : wrong) {
                text += " " + w + ";";
            }
            failures.push_back(text);
        }
    }
    std::printf("%zu starts flown; apart from training by at most %.2f ft/min, %.3f m across, "
                "%.2f m along\n",
                flown, most_sink_apart, most_across_apart, most_along_apart);
    check(flown == all.size() && flown == 27,
          "every one of the 27 starts was flown: " + std::to_string(flown));
    none_wrong(failures, flown, "landings were not where training's were");
}

// **The item's verification**: from each of the 27 starts the committed
// policy lands the C172P within the autopilot's limits - on the runway,
// within 5 m of the centreline, sinking under 300 ft/min, down and upright
// after - in calm air and a ten-knot crosswind from either side, and the
// rollout it hands over to stops her on the runway. Flown in the
// simulation, by the simulation's code, with full tanks: no Python.
GLIDESLOPE_TEST(the_learnt_policy_lands_the_c172p_within_5_m_of_the_centreline_under_300_ft_a_minute_in_calm_air_and_a_ten_knot_crosswind) {
    const auto policy = the_policy();
    check(policy->aircraft == "c172p", "the policy is the C172P's");
    const std::vector<Start> all = starts();
    std::size_t flown = 0;
    std::size_t calm = 0;
    std::size_t from_left = 0;
    std::size_t from_right = 0;
    std::size_t within_5_m = 0;
    double worst_sink = 0.0;
    double worst_across = 0.0;
    std::vector<std::string> failures;
    for (const Start& s : all) {
        const Landing l = land(policy, s, true);
        ++flown;
        calm += s.crosswind_kts == 0.0 ? 1U : 0U;
        from_left += s.crosswind_kts == 10.0 ? 1U : 0U;
        from_right += s.crosswind_kts == -10.0 ? 1U : 0U;
        worst_sink = std::max(worst_sink, l.sink_fpm);
        worst_across = std::max(worst_across, std::abs(l.across_m));
        within_5_m += std::abs(l.across_m) <= 5.0 ? 1U : 0U;
        std::vector<std::string> wrong = short_of_the_limits(l, true);
        for (const std::string& w : not_stopped_on_the_runway(l)) {
            wrong.push_back(w);
        }
        if (!wrong.empty()) {
            std::string text = "from " + named(s) + ":";
            for (const std::string& w : wrong) {
                text += " " + w + ";";
            }
            failures.push_back(text);
        }
    }
    std::printf("%zu of %zu within the limits and stopped on the runway; worst sink %.0f "
                "ft/min; within 5 m of the centreline at %zu, worst %.2f m\n",
                flown - failures.size(), flown, worst_sink, within_5_m, worst_across);
    check(all.size() == 27 && flown == all.size(),
          "the starts are 3 offsets across, 3 heights and 3 winds, and all 27 were flown: " +
              std::to_string(flown));
    check(calm == 9 && from_left == 9 && from_right == 9,
          "nine starts each in calm air and ten knots from either side");
    none_wrong(failures, flown, "landings fell short of the limits");
}

// **And with any fuel on board**: a session's aircraft arrive with whatever
// they have left, and the policy was trained on 25 to 100 lb a tank. Every
// one of the 27 starts again, at 25, 50 and 75 lb a tank - full is the test
// above - within the same limits and stopped on the runway: 81 landings.
GLIDESLOPE_TEST(the_learnt_policy_lands_the_c172p_within_the_limits_from_every_start_with_a_quarter_a_half_and_three_quarters_of_its_fuel) {
    const auto policy = the_policy();
    const std::vector<Start> all = starts();
    const std::vector<double> loads{25.0, 50.0, 75.0};
    std::size_t flown = 0;
    double worst_sink = 0.0;
    double worst_across = 0.0;
    std::vector<std::string> failures;
    for (const double lbs : loads) {
        for (const Start& s : all) {
            std::printf("  with %.0f lb a tank:\n", lbs);
            const Landing l = land(policy, s, true, lbs);
            ++flown;
            worst_sink = std::max(worst_sink, l.sink_fpm);
            worst_across = std::max(worst_across, std::abs(l.across_m));
            std::vector<std::string> wrong = short_of_the_limits(l, true);
            for (const std::string& w : not_stopped_on_the_runway(l)) {
                wrong.push_back(w);
            }
            if (!wrong.empty()) {
                std::string text = "from " + named(s) + " with " + std::to_string(lbs) + " lb a tank:";
                for (const std::string& w : wrong) {
                    text += " " + w + ";";
                }
                failures.push_back(text);
            }
        }
    }
    std::printf("%zu of %zu within the limits; worst sink %.0f ft/min, worst %.2f m across\n",
                flown - failures.size(), flown, worst_sink, worst_across);
    check(flown == loads.size() * all.size() && flown == 81,
          "every start at every load was flown: " + std::to_string(flown) + " of 81");
    none_wrong(failures, flown, "landings fell short of the limits");
}

// **An aeroplane is handed to the learnt landing as it is to the AI pilot**:
// flown by its pilot at the approach gate, handed over, and landed on the
// runway and stopped on it - with no control moved at the switch by more
// than a hand moves it in a step, full travel in a second. The pilot's
// controls are set away from anything the policy flies with, so there is a
// gap to close. In a ten-knot crosswind, 20 m right of the centreline and
// 5 m high: landed within the verification's limits, and stopped.
GLIDESLOPE_TEST(an_aeroplane_handed_to_the_learnt_landing_at_the_gate_lands_within_5_m_of_the_centreline_with_no_step_in_its_controls) {
    const auto policy = the_policy();
    const Start start{20.0, 5.0, 10.0};
    auto aircraft = at(*policy, start);
    Controls pilot;
    pilot.elevator = 0.35;
    pilot.aileron = -0.2;
    pilot.rudder = 0.3;
    pilot.throttle = 0.9;
    pilot.flaps = policy->flaps;
    glideslope::sim::Controller controller(*aircraft, pilot);
    controller.set_pilot(pilot);
    for (int tick = 0; tick < steps_per_second / 2; ++tick) {
        aircraft->set_controls(controller.fly());
        aircraft->step();
    }
    Controls before = controller.fly();
    aircraft->set_controls(before);
    aircraft->step();
    controller.to_ai_learnt_approach(a_runway(), speeds(), policy);
    check(controller.learnt() != nullptr, "the learnt landing has her");

    constexpr double hand = 1.0 / steps_per_second;
    double worst_step = 0.0;
    Landing out;
    AfterTouch after;
    bool stopped = false;
    for (long tick = 0; tick < 420L * steps_per_second; ++tick) {
        const Controls now = controller.fly();
        if (tick < steps_per_second / 4) {
            // The first quarter second: every control on its way from the
            // pilot's by at most a hand's step.
            const auto a = before.as_list();
            const auto b = now.as_list();
            for (std::size_t k = 0; k < a.size(); ++k) {
                worst_step = std::max(worst_step, std::abs(b[k] - a[k]));
            }
        }
        before = now;
        aircraft->set_controls(now);
        aircraft->step();
        const LearntLander* l = controller.learnt();
        if (l == nullptr) {
            break;
        }
        if (l->touched()) {
            after.see(*aircraft, tick, out);
            out.touched = true;
            out.sink_fpm = l->touchdown_sink_fpm();
            out.across_m = l->touchdown_across_m();
            out.along_m = l->touchdown_along_m();
        }
        if (l->stage() == LearntLander::Stage::stopped) {
            stopped = true;
            out.stopped = true;
            out.stopped_along_m = -l->rollout().along_m();
            out.stopped_across_m = l->rollout().across_m();
            out.decisions = l->decisions();
        }
    }
    print("handed over at the gate", out);
    std::printf("  the most any control moved in a step of the first quarter second: %.5f\n",
                worst_step);
    check(worst_step <= hand + 1e-12,
          "no control moved more than a hand's step at the switch: " +
              std::to_string(worst_step));
    check(worst_step > hand / 2.0,
          "the controls did have a gap to close, so the switch was tested");
    check(stopped, "the learnt landing stopped her, and then gave her to the autopilot");
    std::vector<std::string> wrong = short_of_the_limits(out, true);
    for (const std::string& w : not_stopped_on_the_runway(out)) {
        wrong.push_back(w);
    }
    std::vector<std::string> failures;
    if (!wrong.empty()) {
        std::string text = "handed over at the gate:";
        for (const std::string& w : wrong) {
            text += " " + w + ";";
        }
        failures.push_back(text);
    }
    none_wrong(failures, 1, "handed-over landings fell short");
}

// **A policy file that does not fit is refused**, not flown: one for another
// number of observations; cut short; with a word, a NaN or an infinity for a
// number (std::stod reads "nan" and "inf" as numbers, and a NaN weight would
// fly NaN controls); with a count that is not a whole number or is out of
// range - a fractional decision interval, a negative or enormous layer - so
// that nothing is truncated or wrapped on its way to an integer.
GLIDESLOPE_TEST(a_policy_file_that_does_not_fit_the_simulation_is_refused) {
    std::ifstream in(policy_file());
    std::stringstream all;
    all << in.rdbuf();
    const std::string good = all.str();
    check(good.find("observations 25") != std::string::npos, "the policy says 25 observations");
    // A directory of its own, removed however the test ends.
    struct Scratch {
        std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                    ("glideslope-learnt-test-" +
                                     std::to_string(std::chrono::steady_clock::now()
                                                        .time_since_epoch()
                                                        .count()));
        Scratch() {
            std::filesystem::create_directories(dir);
        }
        ~Scratch() {
            std::error_code ignored;
            std::filesystem::remove_all(dir, ignored);
        }
    } scratch;
    std::size_t tried = 0;
    const auto refused = [&](const std::string& text, const std::string& what) {
        const auto file = scratch.dir / "policy.txt";
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
        ++tried;
        check(threw, what + " is refused");
    };
    // `good` with the text from `at` to the end of its line put as `now`.
    const auto changed = [&](const std::string& at, const std::string& now) {
        const auto from = good.find(at);
        check(from != std::string::npos, "the policy has '" + at + "'");
        const auto to = good.find('\n', from);
        std::string out = good;
        out.replace(from, to - from, now);
        return out;
    };
    // `good` with the first number after `at` put as `now`.
    const auto first_number = [&](const std::string& at, const std::string& now) {
        const auto from = good.find(at);
        check(from != std::string::npos, "the policy has '" + at + "'");
        const auto start = from + at.size();
        const auto end = good.find_first_of(" \n", start);
        std::string out = good;
        out.replace(start, end - start, now);
        return out;
    };
    refused(changed("observations 25", "observations 21"), "a policy for 21 observations");
    refused(good.substr(0, good.size() / 2), "a policy cut in half");
    refused(first_number("\nbias ", "x"), "a policy with a word for a number");
    refused(first_number("layer 25 64 tanh\n", "nan"), "a policy with a NaN weight");
    refused(first_number("layer 25 64 tanh\n", "inf"), "a policy with an infinite weight");
    refused(first_number("\nbias ", "-inf"), "a policy with an infinite bias");
    refused(first_number("obs_mean ", "nan"), "a policy with a NaN in its normalisation");
    refused(first_number("obs_scale ", "inf"), "a policy with an infinity in its normalisation");
    refused(changed("obs_clip ", "obs_clip nan"), "a policy with a NaN clip");
    refused(changed("vref_kts ", "vref_kts inf"), "a policy with an infinite reference speed");
    refused(changed("decision_steps ", "decision_steps 12.5"), "a fractional decision interval");
    refused(changed("decision_steps ", "decision_steps 0"), "a decision interval of nothing");
    refused(changed("decision_steps ", "decision_steps 1e30"), "an enormous decision interval");
    refused(changed("layer 25 64 tanh", "layer 25 -64 tanh"), "a layer of negative size");
    refused(changed("layer 25 64 tanh", "layer 25 64.5 tanh"), "a layer of fractional size");
    refused(changed("layer 25 64 tanh", "layer 25 99999999 tanh"), "an enormous layer");
    refused(changed("layers ", "layers 3.5"), "a fractional number of layers");
    refused("", "an empty file");
    check(tried == 18, "every one of the 18 wrong files was tried: " + std::to_string(tried));
    const LearntPolicy p = LearntPolicy::read(policy_file());
    check(!p.layers.empty() && p.layers.front().inputs == 25 && p.layers.back().outputs == 4,
          "the committed policy takes 25 observations and gives 4 actions");
}
