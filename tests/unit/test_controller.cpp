#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/controller.hpp"
#include "sim/test_pilot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using glideslope::sim::Aircraft;
using glideslope::sim::Controller;
using glideslope::sim::Controls;
using glideslope::sim::TestPilot;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;

double largest_step(const Controls& a, const Controls& b) {
    return std::max({std::abs(a.aileron - b.aileron), std::abs(a.elevator - b.elevator),
                     std::abs(a.rudder - b.rudder), std::abs(a.throttle - b.throttle),
                     std::abs(a.mixture - b.mixture), std::abs(a.flaps - b.flaps),
                     std::abs(a.left_brake - b.left_brake),
                     std::abs(a.right_brake - b.right_brake),
                     std::abs(a.pitch_trim - b.pitch_trim)});
}

struct Phase {
    std::string name;
    glideslope::sim::InitialConditions ic;
    // What the pilot's hands do each step: the test pilot flying the phase.
    std::function<Controls(TestPilot&)> hands;
};

} // namespace

GLIDESLOPE_TEST(handing_the_aircraft_between_pilot_and_ai_steps_nothing_in_any_phase) {
    const auto airborne = [](double feet, double knots) {
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = feet;
        ic.airspeed_kts = knots;
        ic.engine_running = true;
        return ic;
    };
    glideslope::sim::InitialConditions on_the_runway;
    on_the_runway.latitude_deg = -33.9461;
    on_the_runway.longitude_deg = 151.1772;
    on_the_runway.altitude_ft = 21.0;
    on_the_runway.terrain_elevation_ft = 21.0;
    on_the_runway.heading_deg = 340.0;
    on_the_runway.engine_running = true;

    const std::vector<Phase> phases = {
        {"the takeoff roll", on_the_runway,
         [](TestPilot& p) {
             Controls c;
             c.throttle = 1.0;
             c.rudder = p.steer_to(340.0);
             return c;
         }},
        {"the climb", airborne(1000.0, 75.0),
         [](TestPilot& p) {
             Controls c;
             c.throttle = 1.0;
             c.elevator = p.pitch_to(p.pitch_for_speed(75.0));
             c.aileron = p.roll_to(0.0);
             c.rudder = p.coordinate();
             return c;
         }},
        {"the cruise", airborne(3000.0, 100.0),
         [](TestPilot& p) {
             Controls c;
             c.throttle = 0.7;
             c.elevator = p.pitch_to(p.pitch_for_altitude(3000.0));
             c.aileron = p.roll_to(0.0);
             c.rudder = p.coordinate();
             return c;
         }},
        {"a turn", airborne(3000.0, 100.0),
         [](TestPilot& p) {
             Controls c;
             c.throttle = 0.8;
             c.elevator = p.pitch_to(p.pitch_for_altitude(3000.0));
             c.aileron = p.roll_to(30.0);
             c.rudder = p.coordinate();
             return c;
         }},
        {"the descent", airborne(3000.0, 90.0),
         [](TestPilot& p) {
             Controls c;
             c.throttle = 0.2;
             c.elevator = p.pitch_to(p.pitch_for_speed(90.0));
             c.aileron = p.roll_to(0.0);
             c.rudder = p.coordinate();
             return c;
         }},
        {"the approach", airborne(1000.0, 65.0),
         [](TestPilot& p) {
             Controls c;
             c.throttle = 0.3;
             c.flaps = 1.0;
             c.elevator = p.pitch_to(p.pitch_for_speed(65.0));
             c.aileron = p.roll_to(0.0);
             c.rudder = p.coordinate();
             return c;
         }},
    };

    // A pilot's hand: full travel in a second.
    constexpr double hand = 1.0 / steps_per_second;
    for (const Phase& phase : phases) {
        Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
        aircraft.initialize(phase.ic);
        TestPilot pilot(aircraft);
        Controls hands = phase.hands(pilot);
        Controller controller(aircraft, hands);
        Controls before = hands;
        double load_before = aircraft.property("accelerations/n-pilot-z-norm");
        // To the AI: the largest step in any control over the five seconds
        // after. Back to the pilot: the largest while the controls catch up
        // with the pilot's hands, how long that takes, and whether the controls
        // are the pilot's after. And at each, the largest step in the load
        // factor over the three seconds after.
        double to_ai_control = 0.0;
        double to_ai_load = 0.0;
        double catching_control = 0.0;
        double catching_load = 0.0;
        double caught_up_s = -1.0;
        bool the_pilots_after = true;
        const int second = steps_per_second;
        for (int i = 0; i < 40 * second; ++i) {
            if (i == 20 * second) {
                controller.to_ai();
            } else if (i == 30 * second) {
                controller.to_pilot();
            }
            hands = phase.hands(pilot);
            controller.set_pilot(hands);
            const Controls c = controller.fly();
            aircraft.set_controls(c);
            aircraft.step();
            const double load = aircraft.property("accelerations/n-pilot-z-norm");
            const double control_step = largest_step(c, before);
            const double load_step = std::abs(load - load_before);
            if (i >= 20 * second && i < 25 * second) {
                to_ai_control = std::max(to_ai_control, control_step);
                if (i < 23 * second) {
                    to_ai_load = std::max(to_ai_load, load_step);
                }
            } else if (i >= 30 * second) {
                if (controller.catching_up() || caught_up_s < 0.0) {
                    catching_control = std::max(catching_control, control_step);
                }
                // The aircraft answers its controls over some steps: its load
                // over the three seconds after.
                if (i < 33 * second) {
                    catching_load = std::max(catching_load, load_step);
                }
                if (!controller.catching_up() && caught_up_s < 0.0) {
                    caught_up_s = static_cast<double>(i + 1 - 30 * second) / second;
                } else if (!controller.catching_up()) {
                    the_pilots_after =
                        the_pilots_after && largest_step(c, hands) == 0.0;
                }
            }
            before = c;
            load_before = load;
        }
        std::fprintf(stderr,
                     "%s: to the AI, a control %.4f and the load %.4f g at most in a "
                     "step; back, catching up in %.2f s, %.4f and %.4f\n",
                     phase.name.c_str(), to_ai_control, to_ai_load, caught_up_s,
                     catching_control, catching_load);
        check(to_ai_control <= 0.01,
              phase.name + ": to the AI, no control moves more than 0.01 in a step: " +
                  std::to_string(to_ai_control));
        check(catching_control <= hand + 1e-12,
              phase.name +
                  ": back to the pilot, no control moves faster than a hand: " +
                  std::to_string(catching_control));
        check(caught_up_s >= 0.0 && caught_up_s <= 2.0,
              phase.name +
                  ": the controls meet the pilot's hands within two seconds: " +
                  std::to_string(caught_up_s) + " s");
        check(the_pilots_after, phase.name + ": then the controls are the pilot's");
        // A step in the controls shakes the aircraft over the steps that
        // follow: jumping straight to the pilot's hands changed the load factor
        // by as much as 0.36 g in a step in the climb, 0.24 in the descent and
        // 0.08 in the turn. At a hand's pace, 0.033 at most.
        check(to_ai_load <= 0.05 && catching_load <= 0.05,
              phase.name + ": over the three seconds after either hand-over, the " +
                  "load factor changes by no more than 0.05 g in a step: " +
                  std::to_string(to_ai_load) + ", " + std::to_string(catching_load));
    }
}

namespace {

// The Cessna off Bondi at `feet`, heading north at 100 kt, flown by the AI up
// the coast to Manly.
struct UpTheCoast {
    Aircraft aircraft{GLIDESLOPE_TEST_DATA_DIR, "c172p"};
    Controller controller{aircraft, Controls{}};
    explicit UpTheCoast(double feet) {
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.89;
        ic.longitude_deg = 151.28;
        ic.altitude_ft = feet;
        ic.heading_deg = 0.0;
        ic.airspeed_kts = 100.0;
        ic.engine_running = true;
        aircraft.initialize(ic);
        controller.to_ai(glideslope::sim::parse_flight_plan(
            "aircraft c172p\nstart -33.89 151.28 " + std::to_string(feet) +
            " 0 100\nwaypoint MANLY -33.80 151.30 " + std::to_string(feet) + " 100\n"));
    }
    Controls step() {
        const Controls c = controller.fly();
        aircraft.set_controls(c);
        aircraft.step();
        return c;
    }
    double latitude() const {
        return aircraft.property("position/lat-geod-deg");
    }
    double longitude() const {
        return aircraft.property("position/long-gc-deg");
    }
};

// A route from where the aircraft is to one waypoint, at `feet` and `knots`.
glideslope::sim::FlightPlan route_to(const UpTheCoast& f, const char* name, double latitude,
                                     double longitude, double feet, double knots) {
    char text[256];
    std::snprintf(text, sizeof text,
                  "aircraft c172p\nstart %.6f %.6f 2000 0 100\n"
                  "waypoint %s %.4f %.4f %.0f %.0f\n",
                  f.latitude(), f.longitude(), name, latitude, longitude, feet, knots);
    return glideslope::sim::parse_flight_plan(text);
}

} // namespace

GLIDESLOPE_TEST(a_plan_changed_while_the_ai_flies_moves_no_control_at_the_change_and_is_flown) {
    UpTheCoast f(2000.0);
    const int second = steps_per_second;
    Controls before;
    for (int i = 0; i < 30 * second; ++i) {
        before = f.step();
    }
    // A new route at the aircraft's height and speed: the autopilot, kept,
    // turns to it at its own pace. Stated from what was measured: the
    // largest step in any control the second after is under a fiftieth of
    // its travel.
    f.controller.replan(route_to(f, "ROSE_BAY", -33.87, 151.26, 2000.0, 100.0));
    double largest = 0.0;
    for (int i = 0; i < second; ++i) {
        const Controls c = f.step();
        largest = std::max(largest, largest_step(c, before));
        before = c;
    }
    std::fprintf(stderr, "the largest step in a control the second after the change: %.4f\n",
                 largest);
    check(largest < 0.02, "no control steps at the change: " + std::to_string(largest));
    check(f.controller.navigator() != nullptr &&
              f.controller.navigator()->plan().waypoints.front().name == "ROSE_BAY",
          "the new route is the one flown");
    int steps = 0;
    while (!f.controller.navigator()->finished() && steps < 5 * 60 * second) {
        f.step();
        ++steps;
    }
    check(f.controller.navigator()->finished(),
          "and flown to its end, in " + std::to_string(steps / second) + " s");
}

GLIDESLOPE_TEST(a_glide_with_the_engine_stopped_holds_its_airspeed_on_the_elevator_along_its_route) {
    UpTheCoast f(5000.0);
    const int second = steps_per_second;
    for (int i = 0; i < 10 * second; ++i) {
        f.step();
    }
    f.aircraft.fail_engine(0, false);
    f.controller.replan(route_to(f, "AIRPORT", -33.95, 151.18, 5000.0, 68.0));
    f.controller.set_glide(68.0);
    // Stated from what was measured: slowed from 100 kt, it dips to 62 and
    // comes back to 68 without passing it; from 45 s on, when it has slowed
    // to it, the glide is held within 5 kt for a minute and a half, down all
    // the way, and towards the airport.
    double slowest = 1e9;
    double fastest = 0.0;
    double highest_after = 0.0;
    double feet_at_45 = 0.0;
    double away_at_45 = 0.0;
    for (int i = 0; i < 135 * second; ++i) {
        f.step();
        if (i == 45 * second) {
            feet_at_45 = f.aircraft.property("position/h-sl-ft");
            away_at_45 = glideslope::sim::distance_m(f.latitude(), f.longitude(), -33.95, 151.18);
        } else if (i > 45 * second) {
            const double kts = f.aircraft.property("velocities/vc-kts");
            slowest = std::min(slowest, kts);
            fastest = std::max(fastest, kts);
            highest_after = std::max(highest_after, f.aircraft.property("position/h-sl-ft"));
        }
    }
    const double feet = f.aircraft.property("position/h-sl-ft");
    const double away = glideslope::sim::distance_m(f.latitude(), f.longitude(), -33.95, 151.18);
    std::fprintf(stderr,
                 "gliding at 68 kt: %.1f to %.1f kt, %.0f ft down in a minute and a half, %.0f m "
                 "nearer the airport\n",
                 slowest, fastest, feet_at_45 - feet, away_at_45 - away);
    check(slowest >= 63.0 && fastest <= 73.0,
          "the glide's airspeed held within 5 kt of 68: " + std::to_string(slowest) + " to " +
              std::to_string(fastest));
    check(highest_after <= feet_at_45 && feet_at_45 - feet > 500.0,
          "and it glides down: " + std::to_string(feet_at_45 - feet) + " ft in a minute and a half");
    check(away_at_45 - away > 1500.0,
          "towards its waypoint: " + std::to_string(away_at_45 - away) +
              " m nearer in a minute and a half");
    check(f.controller.glide() == 68.0, "the glide is what the controller says it flies");
}
