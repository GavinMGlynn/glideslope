#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/controller.hpp"
#include "sim/lander.hpp"
#include "sim/learnt.hpp"
#include "sim/plan.hpp"
#include "sim/terrain.hpp"
#include "sim/weather.hpp"
#include "sim/catalogue.hpp"
#include "world/runway_ground.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
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
    const std::size_t tanks = aircraft->tank_capacities_lbs().size();
    if (fuel_lbs >= 0.0) {
        glideslope::sim::Loading loading;
        for (std::size_t t = 0; t < tanks; ++t) {
            loading.tank_lbs[static_cast<int>(t)] = fuel_lbs;
        }
        aircraft->load(loading);
    }
    aircraft->initialize(ic);
    if (fuel_lbs >= 0.0) {
        // **She weighs what the load says**: the empty aeroplane, what is on
        // board, and the fuel in every tank - so a load that stopped taking
        // effect fails here, not quietly as a full-tank flight.
        double want = aircraft->property("inertia/empty-weight-lbs") +
                      static_cast<double>(tanks) * fuel_lbs;
        for (int i = 0; aircraft->has_property("inertia/pointmass-weight-lbs[" +
                                               std::to_string(i) + "]");
             ++i) {
            want += aircraft->property("inertia/pointmass-weight-lbs[" + std::to_string(i) + "]");
        }
        const double weight = aircraft->property("inertia/weight-lbs");
        check(tanks == 2 && std::abs(weight - want) < 0.01,
              "with " + std::to_string(fuel_lbs) + " lb in each of " + std::to_string(tanks) +
                  " tanks she weighs " + std::to_string(weight) + " lb, not " +
                  std::to_string(want));
    }
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
// the runway, within 5 m of the centreline,
// sinking under 300 ft/min, down and upright for the five seconds after, as
// sim::Lander's are held.
std::vector<std::string> short_of_the_limits(const Landing& l) {
    std::vector<std::string> wrong;
    if (!l.touched) {
        wrong.push_back("never touched down");
        return wrong;
    }
    if (!(l.sink_fpm < 300.0)) {
        wrong.push_back("sank " + std::to_string(l.sink_fpm) + " ft/min, not under 300");
    }
    if (!(std::abs(l.across_m) <= 5.0)) {
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
        std::vector<std::string> wrong = short_of_the_limits(l);
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
// one of the 27 starts again at 25, 50 and 75 lb a tank - full is the test
// above - within the same limits and stopped on the runway. One test a load,
// so that each fits well inside ctest's time on the slowest build.
namespace {

void lands_every_start_with(double lbs) {
    const auto policy = the_policy();
    const std::vector<Start> all = starts();
    std::size_t flown = 0;
    double worst_sink = 0.0;
    double worst_across = 0.0;
    std::vector<std::string> failures;
    for (const Start& s : all) {
        std::printf("  with %.0f lb a tank:\n", lbs);
        const Landing l = land(policy, s, true, lbs);
        ++flown;
        worst_sink = std::max(worst_sink, l.sink_fpm);
        worst_across = std::max(worst_across, std::abs(l.across_m));
        std::vector<std::string> wrong = short_of_the_limits(l);
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
    std::printf("with %.0f lb a tank, %zu of %zu within the limits; worst sink %.0f ft/min, "
                "worst %.2f m across\n",
                lbs, flown - failures.size(), flown, worst_sink, worst_across);
    check(flown == all.size() && flown == 27,
          "every start was flown: " + std::to_string(flown) + " of 27");
    none_wrong(failures, flown, "landings fell short of the limits");
}

} // namespace

GLIDESLOPE_TEST(the_learnt_policy_lands_the_c172p_within_the_limits_from_every_start_with_a_quarter_of_its_fuel) {
    lands_every_start_with(25.0);
}

GLIDESLOPE_TEST(the_learnt_policy_lands_the_c172p_within_the_limits_from_every_start_with_half_its_fuel) {
    lands_every_start_with(50.0);
}

GLIDESLOPE_TEST(the_learnt_policy_lands_the_c172p_within_the_limits_from_every_start_with_three_quarters_of_its_fuel) {
    lands_every_start_with(75.0);
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
    std::vector<std::string> wrong = short_of_the_limits(out);
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

// **A policy flies the aircraft it was trained on, and no other**: the
// C172P's landing handed a Cessna 182S is refused, and nothing is handed
// over - the pilot still has her, with the controls where they were.
GLIDESLOPE_TEST(a_learnt_landing_handed_another_aircraft_is_refused_and_the_pilot_keeps_her) {
    const auto policy = the_policy();
    glideslope::sim::Aircraft skylane(data() / "jsbsim", "c182");
    Controls pilot;
    pilot.throttle = 0.6;
    glideslope::sim::Controller controller(skylane, pilot);
    controller.set_pilot(pilot);
    bool refused = false;
    try {
        controller.to_ai_learnt_approach(a_runway(), speeds(), policy);
    } catch (const std::invalid_argument& e) {
        refused = true;
        std::printf("  refused: %s\n", e.what());
    }
    check(refused, "the C172P's landing is refused for a C182S");
    check(controller.flying() == glideslope::sim::Controller::Flying::pilot &&
              controller.learnt() == nullptr,
          "the pilot still has her, and no learnt landing does");
    check(controller.fly() == pilot, "her controls are the pilot's, as they were");
    bool refused_alone = false;
    try {
        LearntLander lander(skylane, a_runway(), policy, speeds());
    } catch (const std::invalid_argument&) {
        refused_alone = true;
    }
    check(refused_alone, "the learnt lander itself refuses her");
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

// **A copilot's route arriving during a learnt landing replaces it**, as it
// does an approach (sim::Controller::replan): the learnt landing is let go,
// the autopilot is engaged from the controls it had - so no control moves at
// the switch by more than a hand moves it in a step - and the route, a climb
// away along the runway, is flown.
GLIDESLOPE_TEST(a_copilots_route_during_a_learnt_landing_replaces_it_with_no_step_in_its_controls) {
    const auto policy = the_policy();
    auto aircraft = at(*policy, Start{0.0, 0.0, 0.0});
    glideslope::sim::Controller controller(*aircraft, glideslope::sim::Controls{});
    controller.to_ai_learnt_approach(a_runway(), speeds(), policy);
    Controls before;
    for (int tick = 0; tick < 5 * steps_per_second; ++tick) {
        before = controller.fly();
        aircraft->set_controls(before);
        aircraft->step();
    }
    check(controller.learnt() != nullptr, "the learnt landing has her before the route");
    // Away along the runway, 5 km past its threshold, at 1,500 ft.
    const Runway r = a_runway();
    const double h = r.heading_deg / degrees;
    const double lat = r.threshold_lat_deg + 5000.0 * std::cos(h) / 111195.0;
    const double lon =
        r.threshold_lon_deg + 5000.0 * std::sin(h) / (111195.0 * std::cos(lat / degrees));
    char text[256];
    std::snprintf(text, sizeof text,
                  "aircraft c172p\nstart 0 0 0 0 1\nwaypoint AWAY %.6f %.6f 1500 75\n", lat, lon);
    controller.replan(glideslope::sim::parse_flight_plan(text));
    controller.set_glide(std::nullopt);
    check(controller.learnt() == nullptr, "the learnt landing is let go");
    check(controller.navigator() != nullptr && controller.autopilot() != nullptr,
          "the route is flown by the navigator and the autopilot");
    constexpr double hand = 1.0 / steps_per_second;
    double worst_step = 0.0;
    const double away_then = glideslope::sim::distance_m(
        aircraft->property("position/lat-geod-deg"), aircraft->property("position/long-gc-deg"),
        lat, lon);
    for (int tick = 0; tick < 30 * steps_per_second; ++tick) {
        const Controls now = controller.fly();
        if (tick < steps_per_second) {
            const auto a = before.as_list();
            const auto b = now.as_list();
            for (std::size_t k = 0; k < a.size(); ++k) {
                worst_step = std::max(worst_step, std::abs(b[k] - a[k]));
            }
        }
        before = now;
        aircraft->set_controls(now);
        aircraft->step();
    }
    const double away_now = glideslope::sim::distance_m(
        aircraft->property("position/lat-geod-deg"), aircraft->property("position/long-gc-deg"),
        lat, lon);
    std::printf("  the most any control moved in a step of the second after: %.5f; %.0f m "
                "nearer the route's waypoint in 30 s\n",
                worst_step, away_then - away_now);
    check(worst_step <= hand + 1e-12,
          "no control moved more than a hand's step at the switch: " + std::to_string(worst_step));
    check(away_then - away_now > 500.0, "and the route is flown: " +
                                           std::to_string(away_then - away_now) + " m nearer");
}

// ---- the learnt landing offered in a session --------------------------------

namespace {

// **An aeroplane put somewhere about `runway`'s final**: `out_m` before the
// threshold, `across_m` right of the centreline, `high_m` above the policy's
// glidepath, `heading_off_deg` right of the runway's heading, at `kts` with
// `flaps` out - over level ground at the threshold's elevation.
std::unique_ptr<glideslope::sim::Aircraft> placed(const std::string& model, const Runway& runway,
                                                  const LearntPolicy& policy, double out_m,
                                                  double across_m, double high_m,
                                                  double heading_off_deg, double kts,
                                                  double flaps,
                                                  std::shared_ptr<glideslope::sim::Weather> weather = nullptr) {
    auto aircraft = std::make_unique<glideslope::sim::Aircraft>(data() / "jsbsim", model);
    const double ground_m = runway.elevation_ft / feet_per_metre;
    aircraft->set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [ground_m](double, double) { return ground_m; }, [](double, double) { return false; }));
    glideslope::sim::InitialConditions ic = glideslope::sim::final_approach_start(
        runway, out_m, kts, flaps, policy.aim_m, policy.glidepath_deg);
    const double h = runway.heading_deg / degrees;
    ic.latitude_deg +=
        -across_m * std::sin(h) / metres_per_degree_latitude(runway.threshold_lat_deg);
    ic.longitude_deg +=
        across_m * std::cos(h) / metres_per_degree_longitude(runway.threshold_lat_deg);
    ic.altitude_ft += high_m * feet_per_metre;
    ic.heading_deg = runway.heading_deg + heading_off_deg;
    if (weather) {
        aircraft->set_weather(std::move(weather));
    }
    aircraft->initialize(ic);
    return aircraft;
}

} // namespace

// **Every aircraft that has a learnt landing is offered it, and no other**:
// the catalogue walked whole, each aircraft's landing looked for as a
// session looks for it. Only the Cessna 172P has one; every other is offered
// none - and every learnt landing in the data is some catalogue aircraft's.
GLIDESLOPE_TEST(the_learnt_landing_is_offered_for_every_aircraft_that_has_one_and_for_no_other) {
    const std::vector<glideslope::sim::CatalogueEntry> catalogue =
        glideslope::sim::read_catalogue(data());
    std::size_t walked = 0;
    std::vector<std::string> offered;
    std::vector<std::string> models;
    for (const glideslope::sim::CatalogueEntry& entry : catalogue) {
        ++walked;
        models.push_back(entry.model);
        const auto policy = glideslope::sim::learnt_landing(data(), entry.model);
        if (policy) {
            check(policy->aircraft == entry.model,
                  entry.id + "'s learnt landing is its own model's");
            offered.push_back(entry.id);
        }
    }
    std::printf("  %zu aircraft walked of %zu; offered the learnt landing: %zu\n", walked,
                catalogue.size(), offered.size());
    check(walked == catalogue.size() && walked == 16,
          "every one of the catalogue's sixteen aircraft was walked: " + std::to_string(walked));
    check(offered == std::vector<std::string>{"c172p"},
          "the Cessna 172P is offered it, and no other aircraft");
    std::size_t files = 0;
    for (const auto& file : std::filesystem::directory_iterator(data() / "rl")) {
        const std::string name = file.path().filename().string();
        const std::string suffix = "-landing.txt";
        if (name.size() > suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            ++files;
            const std::string model = name.substr(0, name.size() - suffix.size());
            check(std::find(models.begin(), models.end(), model) != models.end(),
                  name + " is the learnt landing of an aircraft in the catalogue");
        }
    }
    check(files == offered.size(),
          "one learnt landing in the data for each offered: " + std::to_string(files));
}

// **Offered at its gate, and refused outside it saying why**: the CLI's start
// - two miles out on the centreline and the glidepath at the reference speed
// with the landing flap - and two opposite corners of the gate just inside
// it, are at it; each condition just outside it - too far, too near, either
// side, above and below, turned either way, too slow and too fast, the flaps
// up, and another aeroplane - is refused, with that condition named.
GLIDESLOPE_TEST(an_aeroplane_is_at_the_learnt_landings_gate_only_inside_it_and_is_told_why_not) {
    const auto policy = the_policy();
    const Runway runway = a_runway();
    const double vref = policy->vref_kts;
    const double nm = metres_per_nm;
    const double f = policy->flaps;
    struct Case {
        std::string name;
        std::string model;
        double out_m, across_m, high_m, heading_deg, kts, flaps;
        std::string why; // empty: at the gate
        double cross_kts = 0.0; // the wind: from the left, positive
        double head_kts = 0.0;  // and from ahead
    };
    const std::vector<Case> cases{
        {"the CLI's start", "c172p", 2.0 * nm, 0.0, 0.0, 0.0, vref, f, ""},
        {"near, left, low, turned left, slow", "c172p", 1.65 * nm, -55.0, -18.0, -4.5,
         vref - 2.5, f, ""},
        {"far, right, high, turned right, fast", "c172p", 2.35 * nm, 55.0, 18.0, 4.5,
         vref + 7.5, f, ""},
        {"too far", "c172p", 2.5 * nm, 0.0, 0.0, 0.0, vref, f, "2.5 miles out"},
        {"too near", "c172p", 1.5 * nm, 0.0, 0.0, 0.0, vref, f, "1.5 miles out"},
        {"too far right", "c172p", 2.0 * nm, 65.0, 0.0, 0.0, vref, f, "right of the centreline"},
        {"too far left", "c172p", 2.0 * nm, -65.0, 0.0, 0.0, vref, f, "left of the centreline"},
        {"too high", "c172p", 2.0 * nm, 0.0, 25.0, 0.0, vref, f, "above the glidepath"},
        {"too low", "c172p", 2.0 * nm, 0.0, -25.0, 0.0, vref, f, "below the glidepath"},
        {"turned too far right", "c172p", 2.0 * nm, 0.0, 0.0, 6.0, vref, f,
         "right of the runway's"},
        {"turned too far left", "c172p", 2.0 * nm, 0.0, 0.0, -6.0, vref, f,
         "left of the runway's"},
        {"too slow", "c172p", 2.0 * nm, 0.0, 0.0, 0.0, vref - 4.0, f, "56 kt; the gate is"},
        {"too fast", "c172p", 2.0 * nm, 0.0, 0.0, 0.0, vref + 9.0, f, "69 kt; the gate is"},
        {"the flaps up", "c172p", 2.0 * nm, 0.0, 0.0, 0.0, vref, 0.0, "flaps at 0%"},
        {"another aeroplane", "c182", 2.0 * nm, 0.0, 0.0, 0.0, vref, f, "this is the c182"},
        {"the flaps a little short", "c172p", 2.0 * nm, 0.0, 0.0, 0.0, vref, f - 0.03, "flaps at 97%"},
        {"too much crosswind", "c172p", 2.0 * nm, 0.0, 0.0, 0.0, vref, f, "17 kt of crosswind", 17.0, 0.0},
        {"too much headwind", "c172p", 2.0 * nm, 0.0, 0.0, 0.0, vref, f, "10 kt of headwind", 0.0, 10.0},
        {"too much tailwind", "c172p", 2.0 * nm, 0.0, 0.0, 0.0, vref, f, "7 kt of tailwind", 0.0, -7.0},
    };
    // Every refusal outside_learnt_gate gives, each way it gives it: two in
    // distance, two across, two in height, two in heading, two in speed, the
    // flaps short and up, the aeroplane, and the wind across, ahead and
    // behind - sixteen - and three inside.
    std::size_t inside = 0;
    std::size_t outside = 0;
    for (const Case& c : cases) {
        const double h = runway.heading_deg / degrees;
        glideslope::sim::Conditions wind;
        wind.wind_north_mps = (c.cross_kts * std::cos(h + 3.14159265358979323846 / 2.0) -
                               c.head_kts * std::cos(h)) * 0.514444;
        wind.wind_east_mps = (c.cross_kts * std::sin(h + 3.14159265358979323846 / 2.0) -
                              c.head_kts * std::sin(h)) * 0.514444;
        const auto aircraft =
            placed(c.model, runway, *policy, c.out_m, c.across_m, c.high_m, c.heading_deg, c.kts,
                   c.flaps, std::make_shared<glideslope::sim::SteadyWeather>(wind));
        // A second flown, so that the instruments have the wind.
        for (int tick = 0; tick < steps_per_second; ++tick) {
            aircraft->step();
        }
        const std::string why = glideslope::sim::outside_learnt_gate(*aircraft, runway, *policy);
        std::printf("  %s: %s\n", c.name.c_str(), why.empty() ? "at the gate" : why.c_str());
        if (c.why.empty()) {
            check(why.empty(), c.name + " is at the gate, not refused: " + why);
            ++inside;
        } else {
            check(why.find(c.why) != std::string::npos,
                  c.name + " is refused for that, '" + c.why + "', not: '" + why + "'");
            ++outside;
        }
    }
    check(inside == 3 && outside == 16, "three inside and sixteen refusals walked: " +
                                            std::to_string(inside) + " and " +
                                            std::to_string(outside));
}

// **A runway of the world's found at its gate, and landed on**: a Cessna 172P
// put two miles out on Sydney's 16R, over ground at its threshold's height,
// is found at the gate of 16R and of no other runway - not 16L beside it, nor
// 34L at its far end - with the heading from that end to the other; handed
// to the learnt landing there, the policy lands her on it within the
// verification's limits and stops her. 2.6 miles out she is refused, the
// runway named and why, and four miles out; in mid-Pacific no runway is near.
GLIDESLOPE_TEST(a_c172p_on_final_to_a_runway_of_the_worlds_is_found_at_its_gate_and_landed_on_it) {
    const auto policy = the_policy();
    const auto surfaces = glideslope::world::runway_surfaces(data());
    // 16R and its other end, 34L: the file's two ends of one runway, so
    // that each end's heading is held, the `le_` and the `he_`.
    std::optional<Runway> sixteen_right;
    std::optional<Runway> thirty_four_left;
    for (std::size_t i = 0; i < surfaces->size(); ++i) {
        const auto& strip = surfaces->at(i).strip;
        if (strip.airport == "YSSY") {
            for (const bool he : {false, true}) {
                const std::string ident = he ? strip.he_ident : strip.le_ident;
                if (ident == "16R") {
                    sixteen_right = glideslope::world::runway_end(*surfaces, i, he, 21.0);
                } else if (ident == "34L") {
                    thirty_four_left = glideslope::world::runway_end(*surfaces, i, he, 21.0);
                }
            }
        }
    }
    check(sixteen_right.has_value() && thirty_four_left.has_value(),
          "Sydney's 16R and 34L are among the world's runways");
    std::printf("  YSSY 34L: heading %.1f\n", thirty_four_left->heading_deg);
    check(std::abs(std::remainder(thirty_four_left->heading_deg - sixteen_right->heading_deg -
                                      180.0,
                                  360.0)) < 0.1,
          "34L points the other way from 16R");
    const Runway runway = *sixteen_right;
    std::printf("  %s: heading %.1f, %.0f m long\n", runway.name.c_str(), runway.heading_deg,
                runway.length_m);
    check(std::abs(std::remainder(runway.heading_deg - 168.0, 360.0)) < 2.0,
          "16R points about 168 degrees true - 155 magnetic - from its threshold to its far end");
    const auto ground = [](double, double) { return 21.0 / feet_per_metre; };

    const auto far = placed("c172p", runway, *policy, 4.0 * metres_per_nm, 30.0, 0.0, 0.0,
                            policy->vref_kts, policy->flaps);
    const glideslope::world::LearntGateFound refused =
        glideslope::world::learnt_gate_runway(*surfaces, *far, *policy, ground);
    std::printf("  four miles out: %s\n", refused.why.c_str());
    check(!refused.runway && refused.why.find("YSSY 16R: 4.0 miles out") == 0,
          "four miles out and 30 m right, 16R is named, not 16L beside it: " + refused.why);
    const auto out = placed("c172p", runway, *policy, 2.6 * metres_per_nm, 0.0, 0.0, 0.0,
                            policy->vref_kts, policy->flaps);
    const glideslope::world::LearntGateFound short_of =
        glideslope::world::learnt_gate_runway(*surfaces, *out, *policy, ground);
    std::printf("  2.6 miles out: %s\n", short_of.why.c_str());
    check(!short_of.runway && short_of.why.find("YSSY 16R: 2.6 miles out") == 0,
          "2.6 miles out, 16R is named and how far out she is: " + short_of.why);
    Runway pacific = runway;
    pacific.threshold_lat_deg = 0.0;
    pacific.threshold_lon_deg = -150.0;
    const auto nowhere = placed("c172p", pacific, *policy, 2.0 * metres_per_nm, 0.0, 0.0, 0.0,
                                policy->vref_kts, policy->flaps);
    check(glideslope::world::learnt_gate_runway(*surfaces, *nowhere, *policy, ground).why ==
              "no runway's threshold is within 4.4 miles",
          "in mid-Pacific, no runway is near");

    auto aircraft = placed("c172p", runway, *policy, 2.0 * metres_per_nm, 0.0, 0.0, 0.0,
                           policy->vref_kts, policy->flaps);
    const glideslope::world::LearntGateFound found =
        glideslope::world::learnt_gate_runway(*surfaces, *aircraft, *policy, ground);
    check(found.runway.has_value() && found.runway->name == "YSSY 16R",
          "at 16R's gate, 16R is found: " + (found.runway ? found.runway->name : found.why));
    check(std::abs(found.runway->heading_deg - runway.heading_deg) < 1e-9 &&
              found.runway->threshold_lat_deg == runway.threshold_lat_deg,
          "its threshold and heading are 16R's");

    Controls pilot = glideslope::sim::trimmed_controls(*aircraft);
    glideslope::sim::Controller controller(*aircraft, pilot);
    controller.to_ai_learnt_approach(*found.runway, speeds(), policy);
    Landing landed;
    for (long tick = 0; tick < 420L * steps_per_second; ++tick) {
        aircraft->set_controls(controller.fly());
        aircraft->step();
        const LearntLander* l = controller.learnt();
        check(l != nullptr, "the learnt landing has her throughout");
        if (l->touched() && !landed.touched) {
            landed.touched = true;
            landed.sink_fpm = l->touchdown_sink_fpm();
            landed.across_m = l->touchdown_across_m();
            landed.along_m = l->touchdown_along_m();
        }
        if (l->stage() == LearntLander::Stage::stopped) {
            landed.stopped = true;
            landed.stopped_along_m = -l->rollout().along_m();
            landed.stopped_across_m = l->rollout().across_m();
            landed.decisions = l->decisions();
            break;
        }
    }
    print("on final to YSSY 16R", landed);
    check(landed.touched && landed.stopped, "she touched down and was stopped");
    check(landed.sink_fpm < 300.0 && std::abs(landed.across_m) < 5.0,
          "within 5 m of 16R's centreline and under 300 ft/min");
    check(landed.along_m > 0.0 && landed.stopped_along_m < runway.length_m &&
              std::abs(landed.stopped_across_m) < runway_half_width_m,
          "touched past the threshold and stopped on the runway");
}

// **Taken back from the learnt landing, no control steps**: handed over at
// the gate and flown ten seconds, then taken back by a pilot whose controls
// are far from the policy's, every control moves at most a hand's step for
// the next two seconds - and the pilot has her, the learnt landing gone.
GLIDESLOPE_TEST(an_aeroplane_taken_back_from_the_learnt_landing_moves_no_control_more_than_a_hand_in_a_step) {
    const auto policy = the_policy();
    auto aircraft = at(*policy, Start{0.0, 0.0, 0.0});
    Controls pilot = glideslope::sim::trimmed_controls(*aircraft);
    glideslope::sim::Controller controller(*aircraft, pilot);
    controller.to_ai_learnt_approach(a_runway(), speeds(), policy);
    Controls before;
    for (int tick = 0; tick < 10 * steps_per_second; ++tick) {
        before = controller.fly();
        aircraft->set_controls(before);
        aircraft->step();
    }
    check(controller.learnt() != nullptr, "the learnt landing has her before the take-back");
    pilot.elevator = 0.6;
    pilot.aileron = 0.5;
    pilot.rudder = -0.5;
    pilot.throttle = 1.0;
    pilot.flaps = 0.0;
    controller.set_pilot(pilot);
    controller.to_pilot();
    check(controller.flying() == glideslope::sim::Controller::Flying::pilot &&
              controller.learnt() == nullptr,
          "the pilot has her, and the learnt landing does not");
    constexpr double hand = 1.0 / steps_per_second;
    double gap = 0.0;
    {
        const auto a = before.as_list();
        const auto b = pilot.as_list();
        for (std::size_t k = 0; k < a.size(); ++k) {
            gap = std::max(gap, std::abs(b[k] - a[k]));
        }
    }
    double worst = 0.0;
    for (int tick = 0; tick < 2 * steps_per_second; ++tick) {
        const Controls now = controller.fly();
        const auto a = before.as_list();
        const auto b = now.as_list();
        for (std::size_t k = 0; k < a.size(); ++k) {
            worst = std::max(worst, std::abs(b[k] - a[k]));
        }
        before = now;
        aircraft->set_controls(now);
        aircraft->step();
    }
    std::printf("  the gap to the pilot's controls %.3f; the most any moved in a step %.5f\n",
                gap, worst);
    check(gap > 10.0 * hand, "there was a gap to close, so the take-back was tested");
    check(worst <= hand + 1e-12,
          "no control moved more than a hand's step: " + std::to_string(worst));
}

// **From every corner of the gate, in every wind it admits, the policy lands
// within its limits.** The gate (sim::LearntGate) is a box in five things -
// how far out, how far across, how far off the glidepath, how far off the
// heading, how fast - and every one of its 32 corners, all five at their
// edges at once, is flown in each of the winds at its own edges: calm, the
// most crosswind it admits from either side, the most headwind and the most
// tailwind - 160 landings, counted. Each must touch down within 5 m of the
// centreline under 300 ft/min, down and upright, and be stopped on the
// runway: the policy's own verification, from the gate's worst.
GLIDESLOPE_TEST(the_learnt_policy_lands_within_its_limits_from_every_corner_of_its_gate_in_every_wind_it_admits) {
    using Gate = glideslope::sim::LearntGate;
    const auto policy = the_policy();
    const Runway runway = a_runway();
    struct Wind {
        std::string name;
        double across_kts; // from the left, positive
        double head_kts;   // down the runway towards her, positive
    };
    const std::vector<Wind> winds{{"calm", 0.0, 0.0},
                                  {"across from the left", Gate::most_crosswind_kts, 0.0},
                                  {"across from the right", -Gate::most_crosswind_kts, 0.0},
                                  {"ahead", 0.0, Gate::most_headwind_kts},
                                  {"behind", 0.0, -Gate::most_tailwind_kts}};
    std::size_t flown = 0;
    std::vector<std::string> failures;
    double worst_across = 0.0;
    double worst_sink = 0.0;
    for (int corner = 0; corner < 32; ++corner) {
        const auto edge = [&](int bit, double low, double high) {
            return (corner & (1 << bit)) != 0 ? high : low;
        };
        // A whisker inside each edge, so that rounding does not put it out.
        const double out_m = edge(0, Gate::nearest_m + 5.0, Gate::furthest_m - 5.0);
        const double across_m = edge(1, 0.5 - Gate::most_across_m, Gate::most_across_m - 0.5);
        const double high_m =
            edge(2, 0.5 - Gate::most_off_glidepath_m, Gate::most_off_glidepath_m - 0.5);
        const double heading =
            edge(3, 0.1 - Gate::most_off_heading_deg, Gate::most_off_heading_deg - 0.1);
        const double kts = edge(4, policy->vref_kts - Gate::most_under_vref_kts + 0.2,
                                policy->vref_kts + Gate::most_over_vref_kts - 0.2);
        for (const Wind& w : winds) {
            // Blowing towards the right of the landing direction, and
            // towards the aeroplane from ahead.
            const double h = runway.heading_deg / degrees;
            const double cross = w.across_kts * 0.514444;
            const double head = w.head_kts * 0.514444;
            glideslope::sim::Conditions conditions;
            conditions.wind_north_mps =
                cross * std::cos(h + 3.14159265358979323846 / 2.0) - head * std::cos(h);
            conditions.wind_east_mps =
                cross * std::sin(h + 3.14159265358979323846 / 2.0) - head * std::sin(h);
            auto aircraft = placed("c172p", runway, *policy, out_m, across_m, high_m, heading, kts,
                                   policy->flaps,
                                   std::make_shared<glideslope::sim::SteadyWeather>(conditions));
            const std::string why =
                glideslope::sim::outside_learnt_gate(*aircraft, runway, *policy);
            char name[200];
            std::snprintf(name, sizeof name,
                          "%.2f nm, %+.0f m across, %+.0f m high, %+.0f deg, %.1f kt, %s",
                          out_m / metres_per_nm, across_m, high_m, heading, kts, w.name.c_str());
            check(why.empty(), std::string(name) + " is at the gate: " + why);
            LearntLander lander(*aircraft, runway, policy, speeds());
            Landing l;
            AfterTouch after;
            for (long tick = 0; tick < 420L * steps_per_second; ++tick) {
                aircraft->set_controls(lander.fly());
                aircraft->step();
                if (lander.touched()) {
                    after.see(*aircraft, tick, l);
                }
                if (lander.stage() == LearntLander::Stage::stopped) {
                    l.stopped = true;
                    l.stopped_along_m = -lander.rollout().along_m();
                    l.stopped_across_m = lander.rollout().across_m();
                    break;
                }
            }
            l.touched = lander.touched();
            l.sink_fpm = lander.touchdown_sink_fpm();
            l.across_m = lander.touchdown_across_m();
            l.along_m = lander.touchdown_along_m();
            ++flown;
            worst_across = std::max(worst_across, std::abs(l.across_m));
            worst_sink = std::max(worst_sink, l.sink_fpm);
            std::vector<std::string> wrong = short_of_the_limits(l);
            for (const std::string& s : not_stopped_on_the_runway(l)) {
                wrong.push_back(s);
            }
            if (!wrong.empty()) {
                std::string text = name;
                for (const std::string& s : wrong) {
                    text += "; " + s;
                }
                failures.push_back(text);
                std::printf("  SHORT %s\n", text.c_str());
            }
        }
    }
    std::printf("  %zu landings from the gate's corners: worst %.2f m across, %.0f ft/min; %zu "
                "short\n",
                flown, worst_across, worst_sink, failures.size());
    check(flown == 32 * winds.size(), "every corner flown in every wind: " + std::to_string(flown));
    none_wrong(failures, flown, "landings from the gate's corners fell short");
}

// **An AI's approach is handed to the learnt landing at its gate, and only
// there**: the C172P started three miles out on final - outside the gate -
// trimmed at its approach speed, given to the approach autopilot with the
// learnt landing to hand to (`Controller::to_ai_approach` with a policy, how
// the server's own AI aircraft are landed). The approach autopilot must fly
// her into the gate and hand her over inside it - between 1.6 and 2.4 miles
// out - and the learnt landing must touch her down within its limits and
// stop her on the runway. The same approach given no policy is never handed
// over: the approach autopilot lands her itself.
GLIDESLOPE_TEST(an_ai_approach_is_handed_to_the_learnt_landing_inside_its_gate_and_landed_within_its_limits) {
    const auto policy = the_policy();
    const Runway runway = a_runway();
    std::size_t flown = 0;
    for (const bool offered : {true, false}) {
        auto aircraft =
            std::make_unique<glideslope::sim::Aircraft>(data() / "jsbsim", policy->aircraft);
        aircraft->set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        aircraft->initialize(glideslope::sim::final_approach_start(
            runway, 3.0 * metres_per_nm, speeds().vref_kts, speeds().flap, speeds().aim_m, 3.0));
        glideslope::sim::Controller controller(*aircraft,
                                               glideslope::sim::trimmed_controls(*aircraft));
        if (offered) {
            controller.to_ai_approach(runway, speeds(), policy);
        } else {
            controller.to_ai_approach(runway, speeds());
        }
        std::optional<double> handed_out_m;
        Landing out;
        AfterTouch after;
        bool stopped = false;
        for (long tick = 0; tick < 420L * steps_per_second && !stopped; ++tick) {
            aircraft->set_controls(controller.fly());
            aircraft->step();
            const LearntLander* l = controller.learnt();
            if (l != nullptr && !handed_out_m) {
                const glideslope::sim::AircraftState s = aircraft->state();
                const double north = (s.latitude_deg - runway.threshold_lat_deg) *
                                     metres_per_degree_latitude(runway.threshold_lat_deg);
                const double east = (s.longitude_deg - runway.threshold_lon_deg) *
                                    metres_per_degree_longitude(runway.threshold_lat_deg);
                const double h = runway.heading_deg / degrees;
                handed_out_m = -(north * std::cos(h) + east * std::sin(h));
                check(tick > 0, "she was not handed over on the first step, outside the gate");
            }
            if (l != nullptr && l->touched()) {
                after.see(*aircraft, tick, out);
                out.touched = true;
                out.sink_fpm = l->touchdown_sink_fpm();
                out.across_m = l->touchdown_across_m();
                out.along_m = l->touchdown_along_m();
            }
            if (l != nullptr && l->stage() == LearntLander::Stage::stopped) {
                stopped = true;
                out.stopped = true;
                out.stopped_along_m = -l->rollout().along_m();
                out.stopped_across_m = l->rollout().across_m();
                out.decisions = l->decisions();
            }
            if (!offered && controller.lander() != nullptr &&
                controller.lander()->stage() == glideslope::sim::Lander::Stage::stopped) {
                stopped = true;
            }
        }
        ++flown;
        if (!offered) {
            check(!handed_out_m, "an approach given no learnt landing is never handed to one");
            check(stopped, "and the approach autopilot stopped her itself");
            continue;
        }
        check(handed_out_m.has_value(), "the approach was handed to the learnt landing");
        if (!handed_out_m) {
            continue;
        }
        std::printf("  handed over %.2f miles out\n", *handed_out_m / metres_per_nm);
        check(*handed_out_m <= glideslope::sim::LearntGate::furthest_m + 1.0 &&
                  *handed_out_m >= glideslope::sim::LearntGate::nearest_m,
              "handed over inside the gate, not " +
                  std::to_string(*handed_out_m / metres_per_nm) + " miles out");
        print("an AI's approach handed over at the gate", out);
        check(stopped, "the learnt landing stopped her");
        std::vector<std::string> wrong = short_of_the_limits(out);
        for (const std::string& w : not_stopped_on_the_runway(out)) {
            wrong.push_back(w);
        }
        std::vector<std::string> failures;
        if (!wrong.empty()) {
            std::string text = "an AI's approach handed over at the gate:";
            for (const std::string& w : wrong) {
                text += " " + w + ";";
            }
            failures.push_back(text);
        }
        none_wrong(failures, 1, "landings fell short");
    }
    check(flown == 2, "both approaches were flown, offered the learnt landing and not");
}
