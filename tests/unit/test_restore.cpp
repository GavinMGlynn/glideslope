#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/figures.hpp"
#include "sim/fixed_step.hpp"
#include "sim/test_pilot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using glideslope::sim::Aircraft;
using glideslope::sim::AircraftState;
using glideslope::sim::Controls;
using glideslope::sim::InitialConditions;
using glideslope::sim::PublishedFigures;
using glideslope::sim::read_published_figures;
using glideslope::sim::steps_per_second;
using glideslope::sim::TestPilot;
using glideslope::test::check;
using glideslope::test::fail;

namespace {

const char* const data_dir = GLIDESLOPE_TEST_DATA_DIR;
const char* const figures_file = GLIDESLOPE_TEST_FIGURES_DIR "/c172p.xml";

constexpr double earth_radius_ft = 20925646.0;
constexpr double to_rad = 3.141592653589793 / 180.0;

int steps(double seconds) {
    return static_cast<int>(
        std::lround(seconds * static_cast<double>(steps_per_second)));
}

// How far apart two aircraft are.
struct Separation {
    double position_ft = 0.0;
    double attitude_deg = 0.0;
    double airspeed_kts = 0.0;

    void widen(const Separation& o) {
        position_ft = std::max(position_ft, o.position_ft);
        attitude_deg = std::max(attitude_deg, o.attitude_deg);
        airspeed_kts = std::max(airspeed_kts, o.airspeed_kts);
    }
};

double angle_between(double a_deg, double b_deg) {
    double d = std::fmod(std::abs(a_deg - b_deg), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

Separation separation(const AircraftState& a, const AircraftState& b) {
    Separation s;
    const double north = (a.latitude_deg - b.latitude_deg) * to_rad * earth_radius_ft;
    const double east = (a.longitude_deg - b.longitude_deg) * to_rad * earth_radius_ft *
                        std::cos(a.latitude_deg * to_rad);
    const double up = a.altitude_ft - b.altitude_ft;
    s.position_ft = std::sqrt(north * north + east * east + up * up);
    s.attitude_deg = std::max({angle_between(a.roll_deg, b.roll_deg),
                               angle_between(a.pitch_deg, b.pitch_deg),
                               angle_between(a.heading_deg, b.heading_deg)});
    s.airspeed_kts = std::abs(a.airspeed_kts - b.airspeed_kts);
    return s;
}

InitialConditions on_the_runway() {
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 70.0;
    return ic;
}

InitialConditions airborne(double altitude_ft, double kcas, bool engine_running) {
    InitialConditions ic = on_the_runway();
    ic.altitude_ft = altitude_ft;
    ic.terrain_elevation_ft = std::min(0.0, altitude_ft) - 3000.0;
    ic.airspeed_kts = kcas;
    ic.engine_running = engine_running;
    return ic;
}

// A phase of flight: where it starts, how long before the capture, and the
// pilot, as a function of the time since the start.
struct Phase {
    std::string name;
    InitialConditions start;
    double capture_at_s;
    std::function<Controls(TestPilot&, double)> fly;
};

std::vector<Phase> phases() {
    return {
        {"static run-up", on_the_runway(), 10.0,
         [](TestPilot&, double) {
             Controls c;
             c.throttle = 1.0;
             c.left_brake = c.right_brake = 1.0;
             return c;
         }},
        {"take-off roll", on_the_runway(), 12.0,
         [](TestPilot&, double t) {
             Controls c;
             c.throttle = 1.0;
             c.flaps = 1.0 / 3.0;
             c.left_brake = c.right_brake = t < 4.0 ? 1.0 : 0.0;
             return c;
         }},
        {"full-throttle climb", airborne(-600.0, 75.4, true), 20.0,
         [](TestPilot& p, double) {
             Controls c;
             c.throttle = 1.0;
             c.elevator = p.pitch_to(p.pitch_for_speed(75.4));
             c.aileron = p.roll_to(0.0);
             c.rudder = p.coordinate();
             return c;
         }},
        {"cruise", airborne(8000.0, 110.0, true), 30.0,
         [](TestPilot& p, double) {
             Controls c;
             c.throttle = 0.9;
             c.mixture = 0.8;
             c.elevator = p.pitch_to(p.pitch_for_altitude(8000.0));
             c.aileron = p.roll_to(0.0);
             c.rudder = p.coordinate();
             return c;
         }},
        {"glide, engine stopped", airborne(6500.0, 66.0, false), 30.0,
         [](TestPilot& p, double) {
             Controls c;
             c.mixture = 0.0;
             c.elevator = p.pitch_to(p.pitch_for_speed(66.0));
             c.aileron = p.roll_to(0.0);
             c.rudder = p.coordinate();
             return c;
         }},
        {"stall approach, flaps up", airborne(5000.0, 70.0, true), 20.0,
         [](TestPilot& p, double t) {
             Controls c;
             c.elevator =
                 p.pitch_to(p.pitch_for_speed(t < 5.0 ? 70.0 : 70.0 - (t - 5.0)));
             c.aileron = p.roll_to(0.0);
             c.rudder = p.coordinate();
             return c;
         }},
        {"stall approach, full flaps", airborne(5000.0, 70.0, true), 20.0,
         [](TestPilot& p, double t) {
             Controls c;
             c.flaps = 1.0;
             c.elevator =
                 p.pitch_to(p.pitch_for_speed(t < 5.0 ? 70.0 : 70.0 - (t - 5.0)));
             c.aileron = p.roll_to(0.0);
             c.rudder = p.coordinate();
             return c;
         }},
        {"level turn, 30 degrees of bank", airborne(3000.0, 100.0, true), 30.0,
         [](TestPilot& p, double) {
             Controls c;
             c.throttle = 0.85;
             c.elevator = p.pitch_to(p.pitch_for_altitude(3000.0));
             c.aileron = p.roll_to(30.0);
             c.rudder = p.coordinate();
             return c;
         }},
    };
}

struct Tracking {
    Separation at_restore;
    Separation after_1s;
    Separation over_10s;
};

// Flies the phase to its capture, restores the capture into a fresh aircraft,
// and flies both on together with the original's controls for ten seconds.
Tracking fly_and_restore(const Phase& phase, const PublishedFigures& figures) {
    Aircraft original(data_dir, figures.model);
    original.load(figures.loading);
    original.initialize(phase.start);
    TestPilot pilot(original);

    int step = 0;
    for (; step < steps(phase.capture_at_s); ++step) {
        original.set_controls(
            phase.fly(pilot, step / static_cast<double>(steps_per_second)));
        original.step();
    }

    const auto snapshot = original.capture();
    Aircraft copy(data_dir, figures.model);
    copy.load(figures.loading);
    copy.restore(snapshot);

    Tracking t;
    t.at_restore = separation(original.state(), copy.state());
    for (int i = 0; i < steps(10.0); ++i, ++step) {
        const Controls c =
            phase.fly(pilot, step / static_cast<double>(steps_per_second));
        original.set_controls(c);
        copy.set_controls(c);
        original.step();
        copy.step();
        const Separation s = separation(original.state(), copy.state());
        if (i + 1 == steps(1.0)) {
            t.after_1s = s;
        }
        t.over_10s.widen(s);
    }
    return t;
}

// **The tolerance.** Reconciliation (Phase 6) resets a client's aircraft to the
// server's state and replays the inputs the server has not yet seen - at 200 ms
// of latency, a quarter of a second of flying - so the second after a restore
// is what it depends on, and ten seconds is a margin well beyond it. Measured
// on the development machine, the worst of every phase was 0.12 ft, 0.42 degrees
// and 0.06 kt after a second, and 10.8 ft, 1.27 degrees and 0.57 kt over ten,
// both approaching a stall, where small differences grow fastest.
constexpr Separation tolerance_1s{0.5, 1.0, 0.25};
constexpr Separation tolerance_10s{25.0, 3.0, 1.0};

bool within(const Separation& s, const Separation& limit) {
    return s.position_ft <= limit.position_ft && s.attitude_deg <= limit.attitude_deg &&
           s.airspeed_kts <= limit.airspeed_kts;
}

} // namespace

GLIDESLOPE_TEST(an_aircraft_restored_in_every_phase_of_flight_tracks_the_original) {
    const PublishedFigures figures = read_published_figures(figures_file);
    int walked = 0;
    std::string failures;
    for (const auto& phase : phases()) {
        const Tracking t = fly_and_restore(phase, figures);
        if (!within(t.at_restore, Separation{0.001, 0.001, 0.001}) ||
            !within(t.after_1s, tolerance_1s) || !within(t.over_10s, tolerance_10s)) {
            failures += "\n  " + phase.name;
        }
        std::printf(
            "%-32s restore %.4f ft %.4f deg %.4f kt | 1 s %.4f ft %.4f deg %.4f kt | "
            "10 s %.3f ft %.3f deg %.3f kt\n",
            phase.name.c_str(), t.at_restore.position_ft, t.at_restore.attitude_deg,
            t.at_restore.airspeed_kts, t.after_1s.position_ft, t.after_1s.attitude_deg,
            t.after_1s.airspeed_kts, t.over_10s.position_ft, t.over_10s.attitude_deg,
            t.over_10s.airspeed_kts);
        ++walked;
    }
    check(walked == 8, "eight phases, every one the figure checks fly; walked " +
                           std::to_string(walked));
    if (!failures.empty()) {
        fail("out of tolerance (0.5 ft, 1 deg, 0.25 kt after 1 s; 25 ft, 3 deg, 1 kt "
             "over "
             "10 s):" +
             failures);
    }
}

// The case reconciliation meets: not a fresh copy, but an aircraft that has flown
// on past the snapshot and is put back to it.
GLIDESLOPE_TEST(
    an_aircraft_put_back_to_an_earlier_snapshot_flies_on_as_a_fresh_restore_does) {
    const PublishedFigures figures = read_published_figures(figures_file);
    const Phase cruise = phases()[3];
    check(cruise.name == "cruise", "phase 3 is the cruise");

    Aircraft rewound(data_dir, figures.model);
    rewound.load(figures.loading);
    rewound.initialize(cruise.start);
    TestPilot pilot(rewound);
    int step = 0;
    for (; step < steps(cruise.capture_at_s); ++step) {
        rewound.set_controls(
            cruise.fly(pilot, step / static_cast<double>(steps_per_second)));
        rewound.step();
    }
    const auto snapshot = rewound.capture();
    // Fly on for five seconds with different controls, then put it back.
    Controls astray;
    astray.throttle = 0.3;
    astray.aileron = 0.4;
    astray.elevator = -0.2;
    for (int i = 0; i < steps(5.0); ++i) {
        rewound.set_controls(astray);
        rewound.step();
    }
    rewound.restore(snapshot);

    Aircraft fresh(data_dir, figures.model);
    fresh.load(figures.loading);
    fresh.restore(snapshot);

    check(within(separation(rewound.state(), fresh.state()),
                 Separation{0.001, 0.001, 0.001}),
          "put back and freshly restored start in the same place");
    Separation worst;
    Controls c;
    c.throttle = 0.9;
    c.mixture = 0.8;
    for (int i = 0; i < steps(10.0); ++i) {
        rewound.set_controls(c);
        fresh.set_controls(c);
        rewound.step();
        fresh.step();
        worst.widen(separation(rewound.state(), fresh.state()));
    }
    std::printf("rewound against fresh over 10 s: %.4f ft %.4f deg %.4f kt\n",
                worst.position_ft, worst.attitude_deg, worst.airspeed_kts);
    check(within(worst, tolerance_10s),
          "the rewound aircraft strayed from the fresh restore");
}

GLIDESLOPE_TEST(a_snapshot_of_one_model_cannot_be_restored_into_another) {
    const PublishedFigures figures = read_published_figures(figures_file);
    Aircraft a(data_dir, figures.model);
    a.load(figures.loading);
    a.initialize(phases()[3].start);
    auto snapshot = a.capture();
    snapshot.model = "c172x";
    try {
        a.restore(snapshot);
    } catch (const std::invalid_argument&) {
        return;
    }
    fail("a snapshot labelled c172x was restored into a c172p");
}
