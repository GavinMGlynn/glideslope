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

namespace {

// The aircraft whose catalogue gives them a mixture lever, full rich below a
// height: the two Cessnas below 3,000 ft (the 172P handbook's figure 5-6),
// the Cherokee below 5,000 (its handbook's section III).
const char* const lever_aircraft[] = {"c172p", "c182", "pa28"};
constexpr std::size_t lever_aircraft_count = 3;

// The height its handbook has it full rich below, which its catalogue says.
double handbook_full_rich_ft(const std::string& model) {
    return model == "pa28" ? 5000.0 : 3000.0;
}

// How long the lever is cut off: long enough to stop the engine, short
// enough that its propeller still turns. The 182S's model charges a
// stopped engine its friction (tools/make_c182.py), which stops its
// propeller within five seconds; the 172P's turns on for longer.
double cut_off_s(const std::string& model) {
    return model == "c182" ? 1.5 : 5.0;
}

// One of them, its engine on `mixture`, the leaner on the mixture from the
// first step, flown by the test pilot level at `altitude_ft` at full throttle
// or, `descend`ing, at 100 kt on a fifth of it. Its lever can be cut off.
struct LeverFlight {
    Aircraft aircraft;
    TestPilot pilot;
    Controls c;
    MixtureLeaner leaner;
    double altitude_ft;

    LeverFlight(const std::string& model, double altitude, double kts, double mixture)
        : aircraft(GLIDESLOPE_TEST_DATA_DIR, model), pilot(aircraft),
          leaner((start(aircraft, altitude, kts), aircraft), mixture), altitude_ft(altitude) {
        c.throttle = 1.0;
        c.mixture = mixture;
    }

    static void start(Aircraft& a, double altitude, double kts) {
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = altitude;
        ic.airspeed_kts = kts;
        ic.engine_running = true;
        a.initialize(ic);
    }

    void step(bool cut_off, bool descend) {
        c.throttle = descend ? 0.2 : 1.0;
        c.elevator = pilot.pitch_to(descend ? pilot.pitch_for_speed(100.0)
                                            : pilot.pitch_for_altitude(altitude_ft));
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        const double mixture = leaner.lean(c.throttle);
        c.mixture = cut_off ? 0.0 : mixture;
        aircraft.set_controls(c);
        aircraft.step();
    }

    void fly(double seconds, bool cut_off = false) {
        for (int i = 0; i < static_cast<int>(seconds * steps_per_second); ++i) {
            step(cut_off, false);
        }
    }

    bool running() const {
        return aircraft.property("propulsion/engine/set-running") > 0.0;
    }

    double hp() const {
        return aircraft.property("propulsion/engine/power-hp");
    }

    double height() const {
        return aircraft.property("position/h-sl-ft");
    }
};

} // namespace

// **An engine the leaner was leaning that stops is given its mixture back,
// and runs again.** Each aeroplane with a mixture lever level at full
// throttle, leaned for a minute and a half; then its lever pulled to cut-off
// for five seconds - the leaner still asked each step, its answer not used -
// and the engine stops; then the leaner's answer used again. While the
// engine is stopped the leaner richens its lever, and given it back the
// engine fires and makes nine tenths of its power again within thirty
// seconds. At 7,000 ft, where full rich burns, and at 12,000 ft, where it is
// richer than 8 to 1 and does not - there, richened to full rich, the 172P
// never ran again.
GLIDESLOPE_TEST(an_engine_the_leaner_was_leaning_that_stops_is_richened_and_runs_again) {
    std::size_t flown = 0;
    for (const char* model : lever_aircraft) {
        for (const double altitude_ft : {7000.0, 12000.0}) {
            ++flown;
            // Handed over on a mixture it burns at both heights.
            LeverFlight f(model, altitude_ft, 90.0, 0.7);
            f.fly(90.0);
            const double leaned_hp = f.hp();
            const double leaned_lever = f.leaner.resting();
            const bool ran_leaned = f.running();
            f.fly(cut_off_s(model), true);
            const bool stopped = !f.running();
            const double stopped_lever = f.leaner.resting();
            f.fly(30.0);
            std::printf("%s at %.0f ft: leaned, the lever at %.3f and %.1f hp; cut off %.1f "
                        "seconds, the engine %s and the lever at %.3f; given back, %s at %.1f "
                        "hp, the lever at %.3f\n",
                        model, altitude_ft, leaned_lever, leaned_hp, cut_off_s(model),
                        stopped ? "stopped" : "RUNNING", stopped_lever,
                        f.running() ? "running" : "STOPPED", f.hp(), f.leaner.resting());
            const std::string at = std::string(model) + " at " + std::to_string(altitude_ft) +
                                   " ft: ";
            check(ran_leaned && leaned_lever < 0.95,
                  at + "the engine ran, leaned: the lever at " + std::to_string(leaned_lever));
            check(stopped, at + "the engine stopped with its mixture cut off");
            check(stopped_lever > leaned_lever + 0.05,
                  at + "the leaner richened the stopped engine: " +
                      std::to_string(leaned_lever) + " to " + std::to_string(stopped_lever));
            check(f.running() && f.hp() >= 0.9 * leaned_hp,
                  at + "given its mixture back the engine runs at nine tenths of its power: " +
                      std::to_string(f.hp()) + " hp against " + std::to_string(leaned_hp));
        }
    }
    check(flown == 2 * lever_aircraft_count,
          "both heights of every lever aircraft flown: " + std::to_string(flown));
}

// **Full rich below the catalogue's height, leaned above it.** Each
// aeroplane whose catalogue has it full rich below a height, at full
// throttle: level at 2,000 ft, handed over on 0.8 of the lever, it is
// richened to its stop within ten seconds and held there for two minutes;
// level at 6,000 ft, handed over full rich, it is leaned within ninety
// seconds and stays leaned. With the height at 0 the 182S, whose engine is
// on the FAA's curve and always rich of best power full rich, is leaned at
// 2,000 ft.
GLIDESLOPE_TEST(the_leaner_holds_full_rich_below_the_catalogues_height_and_leans_above_it) {
    std::size_t flown = 0;
    for (const char* model : lever_aircraft) {
        ++flown;
        LeverFlight low(model, 2000.0, 100.0, 0.8);
        const double rich_ft = handbook_full_rich_ft(model);
        check(low.aircraft.full_rich_below_ft() == rich_ft,
              std::string(model) + "'s catalogue has it full rich below " +
                  std::to_string(rich_ft) + " ft");
        low.fly(10.0);
        double least = 1.0;
        for (int i = 0; i < 120 * steps_per_second; ++i) {
            low.step(false, false);
            least = std::min(least, low.leaner.resting());
        }
        LeverFlight high(model, 6000.0, 100.0, 1.0);
        high.fly(90.0);
        double most = 0.0;
        for (int i = 0; i < 60 * steps_per_second; ++i) {
            high.step(false, false);
            most = std::max(most, high.leaner.resting());
        }
        std::printf("%s: at %.0f ft the lever rested at %.3f at the least; at %.0f ft at "
                    "%.3f at the most\n",
                    model, low.height(), least, high.height(), most);
        check(low.height() < rich_ft && least >= 0.999,
              std::string(model) + " below its full-rich height is full rich: " +
                  std::to_string(least));
        check(high.height() > rich_ft && most < 0.95,
              std::string(model) + " above its full-rich height is leaned: " +
                  std::to_string(most));
    }
    check(flown == lever_aircraft_count, "every lever aircraft flown: " + std::to_string(flown));
}

// **An engine leaned high up that stops low down is given full rich**, not
// the ratio it was leaned to up there. Each aeroplane leaned at 8,000 ft for a
// minute and a half, then brought down on a fifth of the throttle to below
// 2,500 ft, where the leaner richens it to its stop; then its lever cut off
// five seconds, and given back: it runs, and the lever stays at full rich.
// Holding the ratio last found at 8,000 ft, the leaner walked it lean again.
GLIDESLOPE_TEST(an_engine_leaned_high_up_that_stops_below_the_full_rich_height_is_given_full_rich) {
    std::size_t flown = 0;
    for (const char* model : lever_aircraft) {
        ++flown;
        LeverFlight f(model, 8000.0, 90.0, 1.0);
        f.fly(90.0);
        const double leaned = f.leaner.resting();
        for (int i = 0; i < 900 * steps_per_second && f.height() > 2500.0; ++i) {
            f.step(false, true);
        }
        const double low_ft = f.height();
        f.altitude_ft = low_ft;
        f.fly(15.0);
        f.fly(cut_off_s(model), true);
        double least = 1.0;
        for (int i = 0; i < 30 * steps_per_second; ++i) {
            f.step(false, false);
            least = std::min(least, f.leaner.resting());
        }
        std::printf("%s: leaned at 8,000 ft to %.3f; down to %.0f ft; cut off and given back, "
                    "the lever at %.3f at the least, the engine %s\n",
                    model, leaned, low_ft, least, f.running() ? "running" : "STOPPED");
        const std::string m(model);
        check(leaned < 0.95, m + " was leaned at 8,000 ft: " + std::to_string(leaned));
        check(low_ft < 2500.0, m + " came down below 2,500 ft: " + std::to_string(low_ft));
        check(least >= 0.999 && f.running(),
              m + " given back below the height runs full rich: " + std::to_string(least));
    }
    check(flown == lever_aircraft_count, "every lever aircraft flown: " + std::to_string(flown));
}

// **Leaned for best power, every engine sits where the FAA puts best
// power**: between 12 and 13.8 parts of air to one of fuel (FAA-H-8083-32,
// volume 1, page 2-4: best power at "approximately 12 parts of air to 1 part
// of gasoline", the power "essentially constant" from 0.0725 to 0.080 fuel
// to air). Each aeroplane with a mixture lever, level at full throttle at
// 8,000 ft, handed over full rich and leaned for two minutes; over the
// minute after, its ratio never leaves that band. On JSBSim's own mixture
// curve the leaner settled near 9.9 to 1. The Cub has no lever, and is not
// leaned.
GLIDESLOPE_TEST(every_engine_the_leaner_leans_sits_between_12_and_13_8_parts_of_air_to_one_of_fuel) {
    std::size_t flown = 0;
    std::printf("%zu aeroplanes with a mixture lever leaned; none left out\n",
                lever_aircraft_count);
    for (const char* model : lever_aircraft) {
        ++flown;
        LeverFlight f(model, 8000.0, 100.0, 1.0);
        f.fly(120.0);
        double least = 1e9;
        double most = 0.0;
        for (int i = 0; i < 60 * steps_per_second; ++i) {
            f.step(false, false);
            least = std::min(least, afr(f.aircraft));
            most = std::max(most, afr(f.aircraft));
        }
        std::printf("%s at %.0f ft, leaned: %.2f to %.2f parts of air to one of fuel\n", model,
                    f.height(), least, most);
        check(least >= 12.0 && most <= 13.8,
              std::string(model) + " leaned sits between 12 and 13.8 to 1: " +
                  std::to_string(least) + " to " + std::to_string(most));
    }
    check(flown == lever_aircraft_count, "every lever aircraft flown: " + std::to_string(flown));
}
