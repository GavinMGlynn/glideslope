#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/figures.hpp"
#include "sim/fixed_step.hpp"
#include "sim/test_pilot.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

using glideslope::sim::Aircraft;
using glideslope::sim::Controls;
using glideslope::sim::InitialConditions;
using glideslope::sim::TestPilot;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = static_cast<int>(glideslope::sim::steps_per_second);

} // namespace

// The Mosquito's Pilot's Notes (1950, para. 43): "At the stall the aircraft
// pitches, the A.S.I. fluctuates and the nose drops gently." Stalled with the
// stick held back, it must not settle into a deep stall with its nose up -
// which the model did, at 49 degrees of incidence, before it had the nose-down
// moment a wing's separating flow and a tailplane losing its downwash give.
GLIDESLOPE_TEST(the_mosquito_drops_its_nose_at_the_stall_as_its_pilots_notes_say) {
    const auto figures = glideslope::sim::read_published_figures(
        std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/mosquito-fb6.xml");
    Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "mosquito-fb6");
    a.load(figures.loadings.at("pn-18000").loading);
    InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 5000.0;
    ic.terrain_elevation_ft = -3000.0;
    ic.airspeed_kts = 150.0;
    ic.gear = 0.0;
    a.initialize(ic);
    TestPilot pilot(a);
    Controls c;
    c.gear = 0.0;
    c.throttle = 0.0;
    double highest_alpha = -90.0;
    double lowest_pitch_after = 90.0;
    bool stalled = false;
    for (int i = 0; i < 80 * steps_per_second; ++i) {
        const double t = i / static_cast<double>(steps_per_second);
        // The speed wanted falls a knot a second, past the stall; the stick
        // follows it back to its stop.
        const double target = t < 10.0 ? 150.0 : 150.0 - (t - 10.0);
        c.elevator = pilot.pitch_to(pilot.pitch_for_speed(target));
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        a.set_controls(c);
        a.step();
        const double alpha = a.property("aero/alpha-deg");
        highest_alpha = std::max(highest_alpha, alpha);
        stalled = stalled || alpha > 13.0;
        if (stalled) {
            lowest_pitch_after = std::min(lowest_pitch_after, a.property("attitude/theta-deg"));
        }
    }
    std::printf("highest incidence %.1f deg; lowest pitch after the stall %.1f deg\n",
                highest_alpha, lowest_pitch_after);
    check(stalled, "the aircraft stalled");
    check(highest_alpha < 30.0,
          "with the stick held back it did not stay in a deep stall: " +
              std::to_string(highest_alpha) + " degrees of incidence");
    check(lowest_pitch_after < 0.0, "its nose dropped below the horizon: " +
                                        std::to_string(lowest_pitch_after) + " degrees");
}

// Fixed gear stays down whatever the gear lever says. The Cherokee's model
// charges its gear's drag by the gear's position; raised with the lever, as
// every flight begun in the air raises it, the drag went with it, and at full
// throttle the aircraft flew 33 knots faster than with it down. Retractable
// gear still goes up.
GLIDESLOPE_TEST(fixed_gear_stays_down_when_the_lever_is_raised) {
    for (const auto& [model, retracts] :
         {std::pair<std::string, bool>{"pa28", false}, {"mosquito-fb6", true}}) {
        Aircraft a(GLIDESLOPE_TEST_DATA_DIR, model);
        InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = 5000.0;
        ic.terrain_elevation_ft = -3000.0;
        ic.airspeed_kts = 120.0;
        ic.gear = 0.0;
        a.initialize(ic);
        Controls c;
        c.gear = 0.0;
        c.throttle = 0.7;
        for (int i = 0; i < 2 * steps_per_second; ++i) {
            a.set_controls(c);
            a.step();
        }
        const double position = a.property("gear/gear-pos-norm");
        std::printf("%s: gear at %.2f\n", model.c_str(), position);
        check(retracts ? position < 0.01 : position > 0.99,
              model + (retracts ? "'s gear is up: " : "'s fixed gear is down: ") +
                  std::to_string(position));
    }
}
