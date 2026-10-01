#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/figures.hpp"
#include "sim/fixed_step.hpp"
#include "sim/terrain.hpp"
#include "sim/test_pilot.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

// A jet engine that fails stays failed: JSBSim's turbine, stopped with fuel
// still flowing, relights as it spools down, and the take-off field lengths
// and climbs with an engine out would fly on two. Its fuel is cut off, and it
// windmills.
GLIDESLOPE_TEST(a_failed_jet_engine_stays_failed) {
    Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "a320");
    InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 5000.0;
    ic.terrain_elevation_ft = -3000.0;
    ic.airspeed_kts = 200.0;
    ic.gear = 0.0;
    a.initialize(ic);
    TestPilot pilot(a);
    Controls c;
    c.gear = 0.0;
    c.throttle = 0.8;
    for (int i = 0; i < 60 * steps_per_second; ++i) {
        if (i == 5 * steps_per_second) {
            a.fail_engine(0, false);
        }
        c.elevator = pilot.pitch_to(pilot.pitch_for_speed(200.0));
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        a.set_controls(c);
        a.step();
    }
    const double failed = a.property("propulsion/engine[0]/thrust-lbs");
    const double live = a.property("propulsion/engine[1]/thrust-lbs");
    std::printf("a minute after the failure: %.0f lb and %.0f lb of thrust, N1 %.1f%%\n", failed,
                live, a.property("propulsion/engine[0]/n1"));
    check(failed == 0.0 && a.property("propulsion/engine[0]/set-running") == 0.0,
          "the failed engine gives no thrust and is not running: " + std::to_string(failed));
    check(live > 5000.0, "the other still runs: " + std::to_string(live) + " lb");
}

// **Held at full aft stick, the F-15C's nose settles, past the peak of her
// lift.** T.O. 1F-15A-1, section VI, 1 g stalls: "With full aft stick, AOA
// stabilizes at 45 units or above with airspeed 100 knots or less", the
// vertical velocity "probably pegged going down", after wing rock above 30
// units. From 200 knots at 10,000 ft, throttles idle, wings held level, the
// stick full aft for ninety seconds; over the last twenty the angle of
// attack must hold within four degrees - settled, not still rising, as it
// went on to 80 degrees when her whole pitching moment was scaled to NASA's
// - and beyond 32 degrees, where her lift peaks. **What it does not meet,
// recorded and not asserted:** she settles at 116.6 knots, not 100 or
// less. The manual's units are not degrees and it gives no conversion, and
// for 100 knots the model's lift and drag tables, which end at 50 degrees,
// would need her settled past 60 (docs/PROJECT_STATUS.md).
GLIDESLOPE_TEST(the_f15c_held_at_full_aft_stick_settles_past_the_peak_of_her_lift) {
    const auto figures = glideslope::sim::read_published_figures(
        std::filesystem::path(GLIDESLOPE_TEST_FIGURES_DIR) / "f15c.xml");
    Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "f15c");
    a.load(figures.loadings.at("clean").loading);
    InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 10000.0;
    ic.terrain_elevation_ft = -3000.0;
    ic.airspeed_kts = 200.0;
    ic.gear = 0.0;
    a.initialize(ic);
    TestPilot pilot(a);
    Controls c;
    c.gear = 0.0;
    c.throttle = 0.0;
    c.elevator = 1.0; // full aft
    double least_alpha = 1e9;
    double most_alpha = -1e9;
    double most_kcas = 0.0;
    double sink_fpm = 0.0;
    for (int i = 0; i < 90 * steps_per_second; ++i) {
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        a.set_controls(c);
        a.step();
        if (i >= 70 * steps_per_second) {
            const double alpha = a.property("aero/alpha-deg");
            least_alpha = std::min(least_alpha, alpha);
            most_alpha = std::max(most_alpha, alpha);
            most_kcas = std::max(most_kcas, a.property("velocities/vc-kts"));
            sink_fpm = -a.property("velocities/h-dot-fps") * 60.0;
        }
    }
    std::printf("  full aft stick, the last 20 s: alpha %.1f to %.1f, at most %.1f KCAS, "
                "sinking %.0f ft/min, stabilator %.2f of its nose-up travel\n",
                least_alpha, most_alpha, most_kcas, sink_fpm,
                -a.property("fcs/elevator-pos-norm"));
    check(most_alpha - least_alpha <= 4.0,
          "the angle of attack settled: it moved " + std::to_string(most_alpha - least_alpha) +
              " degrees in the last 20 s");
    check(least_alpha > 32.0,
          "past the peak of her lift, 32 degrees, not at " + std::to_string(least_alpha));
    std::printf("  the manual's: 100 knots or less; hers: %.1f\n", most_kcas);
}

// **The F-15C's nose wheel comes off where its flight manual's does.** T.O.
// 1F-15A-1, figure A3-6, maximum performance take-off at military thrust,
// stick full aft from low speed: the nose wheel off at 88, 97 and 109 knots
// at 35,000, 40,000 and 45,000 lb, so 91.5, 100.1 and 110.7 at the 36,946,
// 41,286 and 45,713 lb of the F-15C's three loadings. tools/make_f15c.py
// places the centre of gravity over the main wheels by Raymer's tipback
// angle, and the stabilator works against NASA TM-4604's pitching moment.
// **Within a tenth of the manual's at each**, and no closer, for a reason:
// the manual's speed rises 19 knots across the three, twice the 10 that the
// root of the weight gives, because the real aeroplane's centre of gravity
// moves forward with its fuel and stores, and the model carries both at its
// own. No single centre of gravity meets all three; the model is 5 knots
// late light and 6 early heavy (96.6, 100.9, 105.0).
GLIDESLOPE_TEST(the_f15c_lifts_its_nose_wheel_near_its_flight_manuals_speed_at_each_weight) {
    const auto figures = glideslope::sim::read_published_figures(
        std::filesystem::path(GLIDESLOPE_TEST_FIGURES_DIR) / "f15c.xml");
    const std::pair<const char*, double> manual[] = {
        {"clean", 91.5}, {"combat", 100.1}, {"mission-i", 110.7}};
    std::size_t flown = 0;
    for (const auto& [loading, published] : manual) {
        Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "f15c");
        a.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        a.load(figures.loadings.at(loading).loading);
        InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.airspeed_kts = 0.0;
        ic.gear = 1.0;
        a.initialize(ic);
        Controls c;
        c.throttle = 0.99; // military
        c.elevator = 1.0;  // full aft
        double off_kts = 0.0;
        for (int i = 0; i < 90 * steps_per_second && off_kts == 0.0; ++i) {
            a.set_controls(c);
            a.step();
            if (a.property("gear/unit[0]/WOW") == 0.0 &&
                a.property("velocities/vc-kts") > 30.0) {
                off_kts = a.property("velocities/vc-kts");
            }
        }
        std::printf("  %-9s %6.0f lb: nose wheel off at %5.1f kt, the manual's %3.0f\n", loading,
                    a.property("inertia/weight-lbs"), off_kts, published);
        const double allowed = 0.1 * published;
        check(off_kts > 0.0 && std::abs(off_kts - published) <= allowed,
              std::string(loading) + ": the nose wheel came off at " + std::to_string(off_kts) +
                  " kt, not within " + std::to_string(allowed) + " of the manual's " +
                  std::to_string(published));
        if (off_kts > 0.0) {
            ++flown;
        }
    }
    check(flown == 3, "the nose wheel came off at all three of the figures' loadings, not " +
                          std::to_string(flown));
}

// Fuel can be frozen for a measurement, as a flight test's weight is taken as
// one: the F-15C in afterburner burns a tenth of its fuel in the minute a
// level acceleration takes. Frozen, the tanks hold what they held, and the
// engines still run on it.
GLIDESLOPE_TEST(frozen_fuel_is_not_burned) {
    Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "f15c");
    InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 20000.0;
    ic.terrain_elevation_ft = -3000.0;
    ic.airspeed_kts = 350.0;
    ic.gear = 0.0;
    a.initialize(ic);
    TestPilot pilot(a);
    Controls c;
    c.gear = 0.0;
    c.throttle = 1.0;
    const auto fly = [&](double seconds) {
        for (int i = 0; i < seconds * steps_per_second; ++i) {
            c.elevator = pilot.pitch_to(pilot.pitch_for_altitude(20000.0));
            c.aileron = pilot.roll_to(0.0);
            c.rudder = pilot.coordinate();
            a.set_controls(c);
            a.step();
        }
        return a.property("propulsion/total-fuel-lbs");
    };
    const double start = a.property("propulsion/total-fuel-lbs");
    const double burning = fly(20.0);
    a.freeze_fuel(true);
    const double frozen = fly(20.0);
    std::printf("fuel %.0f lb, %.0f after 20 s in afterburner, %.0f after 20 s more frozen; "
                "thrust %.0f lb\n",
                start, burning, frozen, a.property("propulsion/engine[0]/thrust-lbs"));
    check(start - burning > 100.0, "afterburners burn fuel: " + std::to_string(start - burning));
    check(frozen == burning, "frozen, none is burned: " + std::to_string(burning - frozen));
    check(a.property("propulsion/engine[0]/thrust-lbs") > 10000.0, "the engines still run");
}

// The speedbrake lever moves the flight spoilers of an aircraft that has them,
// and the ground spoilers of one that has those too - the 737's.
GLIDESLOPE_TEST(the_speedbrake_lever_moves_the_spoilers) {
    Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "737-300");
    InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 5000.0;
    ic.terrain_elevation_ft = -3000.0;
    ic.airspeed_kts = 250.0;
    ic.gear = 0.0;
    a.initialize(ic);
    Controls c;
    c.gear = 0.0;
    c.throttle = 0.5;
    c.speedbrake = 1.0;
    for (int i = 0; i < 5 * steps_per_second; ++i) {
        a.set_controls(c);
        a.step();
    }
    check(a.property("fcs/speedbrake-pos-norm") > 0.99 &&
              a.property("fcs/spoiler-pos-norm") > 0.99,
          "flight and ground spoilers out: " +
              std::to_string(a.property("fcs/speedbrake-pos-norm")) + ", " +
              std::to_string(a.property("fcs/spoiler-pos-norm")));
}

// **The Learjet 35A trims in cruise on her stabilizer**, the elevator left at
// neutral. She trims by moving the whole horizontal stabilizer, and her
// maintenance manual gives its travel (27-40-00): from 1 deg 30' to 1 deg 55'
// leading edge down at the nose-down stop. Her model's nose-down stop had been
// the tunnel aircraft's setting, 0.4 degrees, which is not tied to the zero of
// the flight model's pitching moment, and it reached a fifth of what cruise
// needs: from 250 to 350 knots JSBSim could not trim her on it, and a pilot
// flying by hand held the stick forward.
//
// Here every loading her figures name is started level at 250, 300 and 350
// KCAS at 10,000, 20,000, 30,000 and 40,000 ft, trimmed by JSBSim on her
// pitch trim alone - the elevator stays where it starts, at neutral - and then
// flown for thirty seconds hands off: the elevator at neutral, the stabilizer
// where the trim left it, the wings held level. She must trim, with the
// stabilizer inside its travel, and hold her height within 100 ft and her
// speed within 3 knots. **Left out, and named**: each speed and height past
// her Mach limit, 0.81 (the C-21A's cruise Mach; her figures' cruise_mach) -
// 350 knots at 30,000 ft, and all three at 40,000, where 250 knots is Mach
// 0.82. So that 40,000 ft is flown at all, each loading is flown there too at
// 230 knots, Mach 0.76. The worst case's share of the stabilizer's nose-down
// travel is printed, and must be short of all of it.
GLIDESLOPE_TEST(the_learjet_35a_trims_level_from_250_to_350_knots_with_her_elevator_at_neutral) {
    const auto figures = glideslope::sim::read_published_figures(
        std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/learjet35a.xml");
    const double speeds_kts[] = {250.0, 300.0, 350.0};
    const double heights_ft[] = {10000.0, 20000.0, 30000.0, 40000.0};
    constexpr double mach_limit = 0.81;
    struct Case {
        double kts;
        double ft;
    };
    std::vector<Case> cases;
    for (const double speed : speeds_kts) {
        for (const double height : heights_ft) {
            cases.push_back({speed, height});
        }
    }
    cases.push_back({230.0, 40000.0});
    const std::size_t space = figures.loadings.size() * cases.size();
    std::size_t flown = 0;
    std::vector<std::string> left_out;
    std::vector<std::string> failed;
    double worst_share = 0.0;
    std::string worst_case;
    for (const auto& [name, loading] : figures.loadings) {
        for (const auto& [speed, height] : cases) {
            {
                Aircraft a(GLIDESLOPE_TEST_DATA_DIR, "learjet35a");
                a.load(loading.loading);
                InitialConditions ic;
                ic.latitude_deg = -33.9;
                ic.longitude_deg = 151.2;
                ic.altitude_ft = height;
                ic.terrain_elevation_ft = 0.0;
                ic.airspeed_kts = speed;
                ic.gear = 0.0;
                ic.trim = true;
                a.initialize(ic);
                const std::string what = name + " at " + std::to_string(static_cast<int>(speed)) +
                                         " kt and " + std::to_string(static_cast<int>(height)) + " ft";
                const double mach = a.property("velocities/mach");
                if (mach > mach_limit) {
                    left_out.push_back(what + ": Mach " + std::to_string(mach) + ", past her limit");
                    continue;
                }
                ++flown;
                const bool trimmed = a.trimmed();
                Controls c;
                c.gear = 0.0;
                c.elevator = 0.0;
                c.throttle = a.property("fcs/throttle-cmd-norm[0]");
                c.pitch_trim = -a.property("fcs/pitch-trim-cmd-norm");
                const double stabilizer = a.property("fcs/stabilizer-pos-rad") * 57.29577951308232;
                // Her share of the stabilizer's travel on the side she uses:
                // the pitch trim's command is -1 to 1 over the whole travel.
                const double share = std::abs(c.pitch_trim);
                if (trimmed && share > worst_share) {
                    worst_share = share;
                    worst_case = what;
                }
                const auto start = a.state();
                TestPilot pilot(a);
                double worst_height = 0.0;
                double worst_speed = 0.0;
                for (int i = 0; i < 30 * steps_per_second; ++i) {
                    c.aileron = pilot.roll_to(0.0);
                    a.set_controls(c);
                    a.step();
                    const auto s = a.state();
                    worst_height = std::max(worst_height, std::abs(s.altitude_ft - start.altitude_ft));
                    worst_speed = std::max(worst_speed, std::abs(s.airspeed_kts - start.airspeed_kts));
                }
                std::printf("%s: Mach %.2f, %s, pitch trim %+.3f, stabilizer %+.2f deg, throttle %.2f; "
                            "hands off for 30 s, height within %.0f ft, speed within %.1f kt\n",
                            what.c_str(), mach, trimmed ? "trimmed" : "NOT TRIMMED", c.pitch_trim,
                            stabilizer, c.throttle, worst_height, worst_speed);
                if (!trimmed || share >= 1.0 || worst_height >= 100.0 || worst_speed >= 3.0) {
                    failed.push_back(what);
                }
            }
        }
    }
    for (const std::string& why : left_out) {
        std::printf("left out - %s\n", why.c_str());
    }
    std::printf("the worst: %s, at %.3f of the stabilizer's travel nose down\n", worst_case.c_str(),
                worst_share);
    std::string failures;
    for (const std::string& what : failed) {
        failures += "; " + what;
    }
    check(failed.empty(), std::to_string(failed.size()) +
                              " cases did not trim, or did not hold height and speed hands off" + failures);
    check(figures.loadings.size() == 4, "her figures name four loadings, not " +
                                            std::to_string(figures.loadings.size()));
    // Four loadings, three speeds at four heights and 230 knots at 40,000 ft:
    // 52. Each loading leaves out the same four, past Mach 0.81: sixteen left
    // out, 36 flown.
    check(space == 52 && flown + left_out.size() == space,
          "flown " + std::to_string(flown) + " and left out " + std::to_string(left_out.size()) +
              " of " + std::to_string(space) + " cases, of 52");
    check(left_out.size() == 16, "sixteen cases past her Mach limit left out, not " +
                                     std::to_string(left_out.size()));
}
