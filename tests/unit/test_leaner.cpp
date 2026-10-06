#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/leaner.hpp"
#include "sim/test_pilot.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

using glideslope::sim::Aircraft;
using glideslope::sim::Controls;
using glideslope::sim::MixtureLeaner;
using glideslope::sim::TestPilot;
using glideslope::test::check;

// The leaner's rules (sim/leaner.hpp), each built explicitly on a Cessna
// 172P flown by the test pilot at a speed, the leaner alone on the mixture.

namespace {

constexpr int steps_per_second = 120;

struct Flown {
    double first_afr = 0.0;   // as handed to the leaner
    double most_afr = 0.0;    // after `settle_s`
    double least_afr = 1e9;   // after `settle_s`
    double least_resting = 1.0;
    double from_ft = 0.0;
    double to_ft = 0.0;
    bool running = true;
};

double afr(const Aircraft& a) {
    return a.property("propulsion/engine/AFR");
}

// The 172P at `altitude_ft` and `kts`, its engine on `mixture`, flown for
// `seconds` on `throttle` at that speed - or at that height, `level` - the leaner on the mixture from the
// first step; what the engine's ratio of air to fuel did after `settle_s`.
Flown fly(double altitude_ft, double kts, double mixture, double throttle, double seconds,
          double settle_s, bool level = false) {
    Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = altitude_ft;
    ic.airspeed_kts = kts;
    ic.engine_running = true;
    aircraft.initialize(ic);
    TestPilot pilot(aircraft);
    Controls c;
    c.throttle = throttle;
    c.mixture = mixture;
    // A step on the mixture handed over, so the engine reports its ratio.
    c.elevator = pilot.pitch_to(pilot.pitch_for_speed(kts));
    aircraft.set_controls(c);
    aircraft.step();
    Flown f;
    f.first_afr = afr(aircraft);
    f.from_ft = aircraft.property("position/h-sl-ft");
    MixtureLeaner leaner(aircraft, mixture);
    for (int i = 0; i < static_cast<int>(seconds * steps_per_second); ++i) {
        c.elevator = pilot.pitch_to(level ? pilot.pitch_for_altitude(altitude_ft)
                                          : pilot.pitch_for_speed(kts));
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        c.mixture = leaner.lean(c.throttle);
        aircraft.set_controls(c);
        aircraft.step();
        f.running = f.running && aircraft.property("propulsion/engine/set-running") > 0.0;
        if (i >= static_cast<int>(settle_s * steps_per_second)) {
            f.most_afr = std::max(f.most_afr, afr(aircraft));
            f.least_afr = std::min(f.least_afr, afr(aircraft));
            f.least_resting = std::min(f.least_resting, leaner.resting());
        }
    }
    f.to_ft = aircraft.property("position/h-sl-ft");
    return f;
}

} // namespace

// **Never leaner than chemically correct, as a bound.** Handed an engine at
// 6,000 ft running lean of 14.7 parts of air to one of fuel, the leaner
// richens it past 14.7 within ten seconds - a tenth of the lever's travel a
// second - and it stays there for the two minutes after, whatever the power's
// answer. Twice: at full throttle, where the power's answer would richen it
// anyway, and on a quarter of the throttle, where the answer is not felt for
// and the ratio handed over is held - there, once, it stayed lean for good.
GLIDESLOPE_TEST(the_leaner_richens_an_engine_lean_of_chemically_correct_within_ten_seconds) {
    std::size_t flown = 0;
    for (const double throttle : {1.0, 0.25}) {
        ++flown;
        const Flown f = fly(6000.0, 75.0, 0.6, throttle, 130.0, 10.0);
        std::printf("on %.2f throttle: handed over at %.2f to 1; after ten seconds %.2f to "
                    "%.2f; the lever at least %.3f; the engine %s\n",
                    throttle, f.first_afr, f.least_afr, f.most_afr, f.least_resting,
                    f.running ? "running" : "STOPPED");
        const std::string on = "on " + std::to_string(throttle) + " throttle: ";
        check(f.first_afr > 14.7,
              on + "the engine is handed over lean of 14.7: " + std::to_string(f.first_afr));
        check(f.running, on + "the engine runs throughout");
        check(f.most_afr <= 14.7, on + "from ten seconds on, never leaner than 14.7: " +
                                      std::to_string(f.most_afr));
    }
    check(flown == 2, "both throttles flown: " + std::to_string(flown));
}

// **Throttled back it holds the ratio it was handed.** Below four tenths of
// the throttle the power is too little to lean by, so the ratio of air to
// fuel is held: descending from 8,000 ft on a quarter of the throttle, three
// minutes, the mixture richens as the air thickens and the ratio stays
// within half a part of where it was. Without the hold the lever stays put
// and the ratio richens with the air.
GLIDESLOPE_TEST(the_leaner_holds_the_ratio_it_was_handed_below_four_tenths_of_the_throttle) {
    const Flown f = fly(8000.0, 90.0, 0.8, 0.25, 180.0, 0.0);
    std::printf("from %.0f ft to %.0f ft: handed over at %.2f to 1, then %.2f to %.2f\n",
                f.from_ft, f.to_ft, f.first_afr, f.least_afr, f.most_afr);
    check(f.from_ft - f.to_ft >= 2000.0,
          "it came down 2,000 ft at least: " + std::to_string(f.from_ft - f.to_ft));
    check(f.running, "the engine runs throughout");
    check(f.least_afr >= f.first_afr - 0.5 && f.most_afr <= f.first_afr + 0.5,
          "the ratio stays within half a part of where it was handed over: " +
              std::to_string(f.least_afr) + " to " + std::to_string(f.most_afr) +
              " against " + std::to_string(f.first_afr));
}

// **Low down it rests at full rich.** Below about 4,000 ft even full rich is
// leaner than the peak of the model's power, so at full throttle level at 2,000 ft
// the lever rests at its rich stop - as the handbooks' "full rich below
// 3,000 ft" has it - for three minutes, within the hundredth it feels by.
GLIDESLOPE_TEST(the_leaner_rests_at_full_rich_at_full_throttle_low_down) {
    const Flown f = fly(2000.0, 110.0, 1.0, 1.0, 180.0, 0.0, true);
    std::printf("from %.0f ft to %.0f ft: the lever rested at %.3f at the least\n", f.from_ft,
                f.to_ft, f.least_resting);
    check(f.to_ft < 4000.0, "it stayed below 4,000 ft: " + std::to_string(f.to_ft));
    check(f.least_resting >= 0.99,
          "the lever rests within a hundredth of full rich: " + std::to_string(f.least_resting));
}

// **An engine the leaner was leaning that stops is given its mixture back,
// and runs again.** The 172P level at full throttle, leaned for a minute and
// a half; then its mixture lever pulled to cut-off for five seconds - the
// leaner still asked each step, its answer not used - and the engine stops;
// then the leaner's answer used again. While the engine is stopped the
// leaner richens its lever, and given it back the engine fires, windmilling,
// and makes nine tenths of its power again within thirty seconds. Twice:
// at 7,000 ft, where full rich burns, and at 12,000 ft, where it is richer
// than 8 to 1 and does not - there, richened to full rich, it never ran
// again.
GLIDESLOPE_TEST(an_engine_the_leaner_was_leaning_that_stops_is_richened_and_runs_again) {
    std::size_t flown = 0;
    for (const double altitude_ft : {7000.0, 12000.0}) {
        ++flown;
        Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = altitude_ft;
        ic.airspeed_kts = 90.0;
        ic.engine_running = true;
        aircraft.initialize(ic);
        TestPilot pilot(aircraft);
        Controls c;
        c.throttle = 1.0;
        // Handed over on a mixture it burns at both heights.
        c.mixture = 0.7;
        MixtureLeaner leaner(aircraft, c.mixture);
        const auto fly_for = [&](double seconds, bool cut_off) {
            for (int i = 0; i < static_cast<int>(seconds * steps_per_second); ++i) {
                c.elevator = pilot.pitch_to(pilot.pitch_for_altitude(altitude_ft));
                c.aileron = pilot.roll_to(0.0);
                c.rudder = pilot.coordinate();
                const double mixture = leaner.lean(c.throttle);
                c.mixture = cut_off ? 0.0 : mixture;
                aircraft.set_controls(c);
                aircraft.step();
            }
        };
        const auto running = [&] {
            return aircraft.property("propulsion/engine/set-running") > 0.0;
        };
        fly_for(90.0, false);
        const double leaned_hp = aircraft.property("propulsion/engine/power-hp");
        const double leaned_lever = leaner.resting();
        const bool ran_leaned = running();
        fly_for(5.0, true);
        const bool stopped = !running();
        const double stopped_lever = leaner.resting();
        const double stopped_rpm = aircraft.property("propulsion/engine/engine-rpm");
        fly_for(30.0, false);
        const double hp = aircraft.property("propulsion/engine/power-hp");
        std::printf("at %.0f ft: leaned, the lever at %.3f and %.1f hp; cut off five seconds, "
                    "the engine %s at %.0f rpm and the lever at %.3f; given back, %s at %.1f hp, the lever "
                    "at %.3f\n",
                    altitude_ft, leaned_lever, leaned_hp, stopped ? "stopped" : "RUNNING", stopped_rpm,
                    stopped_lever, running() ? "running" : "STOPPED", hp, leaner.resting());
        const std::string at = "at " + std::to_string(altitude_ft) + " ft: ";
        check(ran_leaned && leaned_lever < 0.95,
              at + "the engine ran, leaned: the lever at " + std::to_string(leaned_lever));
        check(stopped, at + "the engine stopped with its mixture cut off");
        check(stopped_lever > leaned_lever + 0.05,
              at + "the leaner richened the stopped engine: " + std::to_string(leaned_lever) +
                  " to " + std::to_string(stopped_lever));
        check(running() && hp >= 0.9 * leaned_hp,
              at + "given its mixture back the engine runs at nine tenths of its power: " +
                  std::to_string(hp) + " hp against " + std::to_string(leaned_hp));
    }
    check(flown == 2, "both heights flown: " + std::to_string(flown));
}
