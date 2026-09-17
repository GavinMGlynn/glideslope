#include "sim/selftest.hpp"

#include "sim/fixed_step.hpp"
#include "sim/test_pilot.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace glideslope::sim {

namespace {

struct Command {
    double time_s = 0.0;
    std::string name;
    double value = 0.0;
};

struct InputLog {
    std::string model;
    Loading loading;
    InitialConditions start;
    bool has_start = false;
    std::vector<Command> commands;
    double end_s = -1.0;
};

InputLog read_log(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
        throw std::runtime_error("cannot read " + file.string());
    }
    static const std::vector<std::string> known = {
        "throttle",  "mixture",    "flaps",         "brakes",
        "rotate_at", "hold_speed", "hold_altitude", "bank"};
    InputLog log;
    std::string line;
    int number = 0;
    while (std::getline(in, line)) {
        ++number;
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line.erase(hash);
        }
        std::istringstream words(line);
        std::string first;
        if (!(words >> first)) {
            continue;
        }
        const auto bad = [&](const std::string& why) {
            return std::runtime_error(file.string() + ":" + std::to_string(number) +
                                      ": " + why);
        };
        if (log.end_s >= 0.0) {
            throw bad("nothing may follow end");
        }
        if (first == "aircraft") {
            if (!(words >> log.model)) {
                throw bad("aircraft needs a name");
            }
        } else if (first == "load") {
            std::string what;
            int index = 0;
            double lbs = 0.0;
            if (!(words >> what >> index >> lbs) ||
                (what != "pointmass" && what != "tank")) {
                throw bad("load needs pointmass|tank INDEX LBS");
            }
            (what == "pointmass" ? log.loading.pointmass_lbs
                                 : log.loading.tank_lbs)[index] = lbs;
        } else if (first == "start") {
            InitialConditions& ic = log.start;
            if (!(words >> ic.latitude_deg >> ic.longitude_deg >>
                  ic.terrain_elevation_ft >> ic.heading_deg)) {
                throw bad("start needs LATITUDE LONGITUDE TERRAIN_FT HEADING_DEG");
            }
            ic.altitude_ft = ic.terrain_elevation_ft;
            ic.engine_running = true;
            log.has_start = true;
        } else {
            Command c;
            std::istringstream all(line);
            if (!(all >> c.time_s >> c.name)) {
                throw bad("expected TIME COMMAND [VALUE]");
            }
            if (!log.commands.empty() && c.time_s < log.commands.back().time_s) {
                throw bad("times must not go backwards");
            }
            if (c.name == "end") {
                log.end_s = c.time_s;
                continue;
            }
            if (std::find(known.begin(), known.end(), c.name) == known.end()) {
                throw bad("unknown command " + c.name);
            }
            if (!(all >> c.value)) {
                throw bad(c.name + " needs a value");
            }
            log.commands.push_back(c);
        }
    }
    if (log.model.empty() || !log.has_start || log.end_s <= 0.0) {
        throw std::runtime_error(file.string() +
                                 " needs an aircraft, a start and an end");
    }
    return log;
}

// FNV-1a, 64-bit, over the bit patterns of the doubles. Two runs agree only if
// every value at every step agrees to the last bit.
struct Fnv1a {
    std::uint64_t value = 14695981039346656037u;
    void add(double d) {
        std::uint64_t bits = std::bit_cast<std::uint64_t>(d);
        for (int i = 0; i < 8; ++i) {
            value ^= bits & 0xffu;
            value *= 1099511628211u;
            bits >>= 8;
        }
    }
    void add(const AircraftState& s) {
        for (double d :
             {s.sim_time_s, s.latitude_deg, s.longitude_deg, s.altitude_ft, s.roll_deg,
              s.pitch_deg, s.heading_deg, s.u_fps, s.v_fps, s.w_fps, s.p_radps,
              s.q_radps, s.r_radps, s.airspeed_kts, s.climb_rate_fpm, s.engine_rpm}) {
            add(d);
        }
    }
};

enum class Elevator { free, rotate, speed, altitude };

} // namespace

SelftestResult run_selftest(const std::filesystem::path& jsbsim_root,
                            const std::filesystem::path& log_file) {
    const InputLog log = read_log(log_file);
    Aircraft aircraft(jsbsim_root, log.model);
    aircraft.load(log.loading);
    aircraft.initialize(log.start);
    TestPilot pilot(aircraft);

    Controls c;
    Elevator mode = Elevator::free;
    double target = 0.0;
    double bank = 0.0;
    std::size_t next = 0;

    Fnv1a hash;
    const auto end_step = static_cast<std::int64_t>(
        std::llround(log.end_s * static_cast<double>(steps_per_second)));
    for (std::int64_t step = 0; step < end_step; ++step) {
        const double t =
            static_cast<double>(step) / static_cast<double>(steps_per_second);
        for (; next < log.commands.size() && log.commands[next].time_s <= t; ++next) {
            const Command& cmd = log.commands[next];
            if (cmd.name == "throttle") {
                c.throttle = cmd.value;
            } else if (cmd.name == "mixture") {
                c.mixture = cmd.value;
            } else if (cmd.name == "flaps") {
                c.flaps = cmd.value / 30.0;
            } else if (cmd.name == "brakes") {
                c.left_brake = c.right_brake = cmd.value;
            } else if (cmd.name == "rotate_at") {
                mode = Elevator::rotate;
                target = cmd.value;
            } else if (cmd.name == "hold_speed") {
                mode = Elevator::speed;
                target = cmd.value;
            } else if (cmd.name == "hold_altitude") {
                mode = Elevator::altitude;
                target = cmd.value;
            } else if (cmd.name == "bank") {
                bank = cmd.value;
            }
        }

        switch (mode) {
        case Elevator::free: c.elevator = 0.0; break;
        case Elevator::rotate:
            c.elevator = aircraft.property("velocities/vc-kts") >= target
                             ? pilot.pitch_to(8.0)
                             : 0.0;
            break;
        case Elevator::speed:
            c.elevator = pilot.pitch_to(pilot.pitch_for_speed(target));
            break;
        case Elevator::altitude:
            c.elevator = pilot.pitch_to(pilot.pitch_for_altitude(target));
            break;
        }
        c.aileron = pilot.roll_to(bank);
        c.rudder = aircraft.property("gear/wow") > 0.5
                       ? pilot.steer_to(log.start.heading_deg)
                       : pilot.coordinate();

        aircraft.set_controls(c);
        aircraft.step();
        hash.add(aircraft.state());
    }

    SelftestResult result;
    result.model = log.model;
    result.steps = end_step;
    result.hash = hash.value;
    result.final_state = aircraft.state();
    return result;
}

} // namespace glideslope::sim
