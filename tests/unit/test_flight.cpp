#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/fixed_step.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using glideslope::sim::Aircraft;
using glideslope::sim::AircraftState;
using glideslope::sim::Controls;
using glideslope::sim::FixedStep;
using glideslope::sim::InitialConditions;
using glideslope::test::check;
using glideslope::test::fail;

namespace {

const char* const data_dir = GLIDESLOPE_TEST_DATA_DIR;

InitialConditions cruising() {
    InitialConditions ic;
    ic.latitude_deg = -33.9461; // over Sydney Airport
    ic.longitude_deg = 151.1772;
    ic.altitude_ft = 3000.0;
    ic.terrain_elevation_ft = 21.0;
    ic.heading_deg = 90.0;
    ic.airspeed_kts = 100.0;
    ic.engine_running = true;
    return ic;
}

// The pilot, as a function of the step number and nothing else, so that two
// flights taking the same steps are flown identically.
Controls scripted(std::int64_t step) {
    Controls c;
    c.throttle = 0.8;
    c.elevator = 0.1 * std::sin(static_cast<double>(step) * 0.01);
    c.aileron = (step >= 300 && step < 420) ? 0.2 : 0.0;
    c.rudder = (step >= 600 && step < 660) ? -0.1 : 0.0;
    return c;
}

struct Uneven {
    std::uint64_t state = 0x2545F4914F6CDD1Du;
    std::chrono::nanoseconds next() {
        state = state * 6364136223846793005u + 1442695040888963407u;
        return std::chrono::nanoseconds(
            static_cast<std::int64_t>((state >> 33) % 50'000'001u));
    }
};

// Flies `total` of scripted flight, with time arriving in chunks from `chunk`.
AircraftState fly(std::chrono::nanoseconds total,
                  const std::function<std::chrono::nanoseconds()>& chunk,
                  std::int64_t& steps_out) {
    Aircraft aircraft(data_dir, "c172p");
    aircraft.initialize(cruising());
    FixedStep fixed;
    std::int64_t step = 0;
    std::chrono::nanoseconds fed{0};
    while (fed < total) {
        std::chrono::nanoseconds c = chunk();
        if (fed + c > total) {
            c = total - fed;
        }
        fed += c;
        for (std::int64_t due = fixed.advance(c); due > 0; --due) {
            aircraft.set_controls(scripted(step));
            aircraft.step();
            ++step;
        }
    }
    steps_out = step;
    return aircraft.state();
}

} // namespace

GLIDESLOPE_TEST(a_flight_fed_its_time_in_any_chunks_ends_in_the_same_state) {
    struct Pattern {
        std::string name;
        std::function<std::chrono::nanoseconds()> chunk;
    };
    auto uneven = std::make_shared<Uneven>();
    const std::vector<Pattern> patterns = {
        {"1 ms", [] { return 1ms; }},
        {"16 ms", [] { return 16ms; }},
        {"100 ms", [] { return 100ms; }},
        {"uneven, 0 to 50 ms", [uneven] { return uneven->next(); }},
        {"all ten seconds at once", [] { return 10s; }},
    };

    std::int64_t reference_steps = 0;
    const AircraftState reference = fly(10s, patterns.front().chunk, reference_steps);
    check(reference_steps == 1200, "ten seconds is 1200 steps");
    // A flight that went nowhere would make every pattern agree trivially.
    check(std::abs(reference.roll_deg) > 1.0 && reference.engine_rpm > 1000.0,
          "the scripted flight must actually roll and run its engine");

    int walked = 1;
    for (std::size_t i = 1; i < patterns.size(); ++i) {
        std::int64_t steps = 0;
        const AircraftState s = fly(10s, patterns[i].chunk, steps);
        if (steps != reference_steps) {
            fail(patterns[i].name + ": took " + std::to_string(steps) + " steps");
        }
        if (!(s == reference)) {
            fail(patterns[i].name + ": ended in a different state from " +
                 patterns.front().name);
        }
        ++walked;
    }
    check(walked == static_cast<int>(patterns.size()), "every pattern was flown");
}

GLIDESLOPE_TEST(the_elevator_pulled_back_pitches_the_nose_up) {
    for (const double pull : {0.5, -0.5}) {
        Aircraft aircraft(data_dir, "c172p");
        aircraft.initialize(cruising());
        Controls c;
        c.throttle = 0.7;
        c.elevator = pull;
        aircraft.set_controls(c);
        const double before = aircraft.state().pitch_deg;
        for (int i = 0; i < 60; ++i) {
            aircraft.step();
        }
        const AircraftState after = aircraft.state();
        const bool nose_up = after.pitch_deg > before && after.q_radps > 0.0;
        const bool nose_down = after.pitch_deg < before && after.q_radps < 0.0;
        if (pull > 0.0 && !nose_up) {
            fail("elevator +0.5 for half a second: pitch " + std::to_string(before) +
                 " -> " + std::to_string(after.pitch_deg) + " deg, q " +
                 std::to_string(after.q_radps));
        }
        if (pull < 0.0 && !nose_down) {
            fail("elevator -0.5 for half a second: pitch " + std::to_string(before) +
                 " -> " + std::to_string(after.pitch_deg) + " deg, q " +
                 std::to_string(after.q_radps));
        }
    }
}
