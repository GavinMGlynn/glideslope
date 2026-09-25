#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/catalogue.hpp"
#include "sim/departure.hpp"
#include "sim/test_pilot.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using glideslope::sim::Aircraft;
using glideslope::sim::Autopilot;
using glideslope::sim::AutopilotModes;
using glideslope::sim::CatalogueEntry;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;

// How a hold met a step: how far past the new target it went, and when it
// was last outside the band about it.
struct Response {
    double overshoot = 0.0;
    double settled_s = 0.0;
    double final_error = 0.0;
    double entered_s = -1.0;
    double worst_after_60 = 0.0;
};

// The Cessna at 4,000 ft and 100 KCAS heading north, the autopilot engaged and
// holding all that for a minute - in calm air, or in moderate turbulence -
// then changed by `change`, and flown `seconds` more: what `read` reads each
// step after the change, averaged over the last `smoothing_s` seconds.
std::vector<double> after_a_step(const std::function<void(AutopilotModes&)>& change,
                                 const std::function<double(const Aircraft&)>& read,
                                 bool turbulent, double seconds, double smoothing_s) {
    Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 4000.0;
    ic.heading_deg = 0.0;
    ic.airspeed_kts = 100.0;
    ic.engine_running = true;
    aircraft.initialize(ic);
    if (turbulent) {
        glideslope::world::WeatherReport report;
        report.surface.metar =
            glideslope::world::parse_metar("XXXX 181200Z 00000KT 9999 SKC 15/05 Q1013");
        report.surface.latitude_deg = -33.9;
        report.surface.longitude_deg = 151.2;
        report.turbulence_severity = 3; // moderate
        report.air_seed = 0xa170;
        aircraft.set_weather(
            std::make_shared<glideslope::world::ReportedWeather>(report, nullptr, 0.0));
    }
    glideslope::sim::Controls controls;
    controls.throttle = 0.7;
    Autopilot autopilot(aircraft, controls);
    AutopilotModes modes = autopilot.modes();
    modes.heading_deg = 0.0;
    modes.altitude_ft = 4000.0;
    modes.airspeed_kts = 100.0;
    autopilot.set(modes);
    for (int i = 0; i < 60 * steps_per_second; ++i) {
        aircraft.set_controls(autopilot.fly());
        aircraft.step();
    }
    change(modes);
    autopilot.set(modes);
    std::vector<double> raw;
    std::vector<double> smoothed;
    const auto window = static_cast<std::size_t>(smoothing_s * steps_per_second);
    double sum = 0.0;
    for (int i = 0; i < static_cast<int>(seconds * steps_per_second); ++i) {
        aircraft.set_controls(autopilot.fly());
        aircraft.step();
        raw.push_back(read(aircraft));
        sum += raw.back();
        if (window > 0 && raw.size() > window) {
            sum -= raw[raw.size() - 1 - window];
        }
        smoothed.push_back(
            window == 0 ? raw.back()
                        : sum / static_cast<double>(std::min(raw.size(), window)));
    }
    return smoothed;
}

// Overshoot past `target`, coming from `from`, and when the reading last left
// `band` about it.
// A heading's difference is taken round the circle.
Response measure(const std::vector<double>& values, double from, double target,
                 double band, bool circular = false) {
    Response r;
    const double direction = target > from ? 1.0 : -1.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double off =
            circular ? std::remainder(values[i] - target, 360.0) : values[i] - target;
        r.overshoot = std::max(r.overshoot, direction * off);
        if (std::abs(off) > band) {
            r.settled_s = static_cast<double>(i + 1) / steps_per_second;
        } else if (r.entered_s < 0.0) {
            r.entered_s = static_cast<double>(i + 1) / steps_per_second;
        }
        if (i >= 60 * steps_per_second) {
            r.worst_after_60 = std::max(r.worst_after_60, std::abs(off));
        }
    }
    r.final_error = circular ? std::remainder(values.back() - target, 360.0)
                             : values.back() - target;
    return r;
}

// From north, -180 to 180 degrees: a turn from north to east averages without
// wrapping.
double heading(const Aircraft& a) {
    return std::remainder(a.property("attitude/psi-deg"), 360.0);
}
double altitude(const Aircraft& a) {
    return a.property("position/h-sl-ft");
}
double airspeed(const Aircraft& a) {
    return a.property("velocities/vc-kts");
}
double climb(const Aircraft& a) {
    return a.property("velocities/h-dot-fps") * 60.0;
}

void report(const std::string& what, const Response& r) {
    std::fprintf(
        stderr,
        "%s: overshoot %.2f, in the band at %.1f s, settled at %.1f s, finally "
        "%.2f off, at worst %.2f after a minute\n",
        what.c_str(), r.overshoot, r.entered_s, r.settled_s, r.final_error,
        r.worst_after_60);
}

} // namespace

GLIDESLOPE_TEST(the_autopilot_captures_a_new_heading_altitude_airspeed_and_climb) {
    // What each hold must do, stated from what was measured (PROJECT_STATUS.md).
    // In calm air, on the instruments as they read: overshoot, the band, and
    // the time by which it is in the band for good.
    struct Limit {
        double overshoot;
        double band;
        double settled_s;
    };
    const auto meets = [](const std::string& what, const Response& r, const Limit& l) {
        report(what, r);
        check(r.overshoot <= l.overshoot && r.settled_s <= l.settled_s &&
                  std::abs(r.final_error) <= l.band,
              what + ": overshoot " + std::to_string(r.overshoot) + " (at most " +
                  std::to_string(l.overshoot) + "), within " + std::to_string(l.band) +
                  " for good at " + std::to_string(r.settled_s) + " s (at most " +
                  std::to_string(l.settled_s) + ")");
    };
    const Limit heading_calm{3.0, 2.0, 50.0};
    const Limit altitude_calm{20.0, 20.0, 80.0};
    const Limit speed_calm{2.0, 2.0, 15.0};
    const Limit climb_calm{100.0, 50.0, 15.0};
    meets("heading 0 to 90 in calm air",
          measure(after_a_step([](AutopilotModes& m) { m.heading_deg = 90.0; }, heading,
                               false, 120.0, 0.0),
                  0.0, 90.0, heading_calm.band),
          heading_calm);
    meets("altitude 4,000 to 4,500 ft in calm air",
          measure(after_a_step([](AutopilotModes& m) { m.altitude_ft = 4500.0; },
                               altitude, false, 150.0, 0.0),
                  4000.0, 4500.0, altitude_calm.band),
          altitude_calm);
    meets("airspeed 100 to 110 KCAS in calm air",
          measure(after_a_step([](AutopilotModes& m) { m.airspeed_kts = 110.0; },
                               airspeed, false, 150.0, 0.0),
                  100.0, 110.0, speed_calm.band),
          speed_calm);
    const auto climb_500 = [](AutopilotModes& m) {
        m.altitude_ft.reset();
        m.vertical_speed_fpm = 500.0;
    };
    meets("climb 0 to 500 ft/min in calm air",
          measure(after_a_step(climb_500, climb, false, 60.0, 1.0), 0.0, 500.0,
                  climb_calm.band),
          climb_calm);

    // In moderate turbulence, on ten-second averages - the turbulence alone
    // moves the airspeed 4 kt and the climb 400 ft/min - in wider bands. Holding
    // altitude through the vertical gusts trades airspeed, so the airspeed is
    // held to its band once it is in it, and to twice the band after a minute.
    const Limit heading_rough{5.0, 5.0, 40.0};
    const Limit altitude_rough{50.0, 50.0, 70.0};
    const Limit climb_rough{200.0, 150.0, 30.0};
    meets("heading 0 to 90 in moderate turbulence",
          measure(after_a_step([](AutopilotModes& m) { m.heading_deg = 90.0; }, heading,
                               true, 120.0, 10.0),
                  0.0, 90.0, heading_rough.band),
          heading_rough);
    meets("altitude 4,000 to 4,500 ft in moderate turbulence",
          measure(after_a_step([](AutopilotModes& m) { m.altitude_ft = 4500.0; },
                               altitude, true, 150.0, 10.0),
                  4000.0, 4500.0, altitude_rough.band),
          altitude_rough);
    meets("climb 0 to 500 ft/min in moderate turbulence",
          measure(after_a_step(climb_500, climb, true, 60.0, 10.0), 0.0, 500.0,
                  climb_rough.band),
          climb_rough);
    const Response speed =
        measure(after_a_step([](AutopilotModes& m) { m.airspeed_kts = 110.0; },
                             airspeed, true, 150.0, 10.0),
                100.0, 110.0, 5.0);
    report("airspeed 100 to 110 KCAS in moderate turbulence", speed);
    check(speed.overshoot <= 3.0 && speed.entered_s >= 0.0 && speed.entered_s <= 20.0 &&
              speed.worst_after_60 <= 10.0,
          "airspeed in moderate turbulence: overshoot " +
              std::to_string(speed.overshoot) + " (at most 3 kt), within 5 kt at " +
              std::to_string(speed.entered_s) + " s (at most 20), at worst " +
              std::to_string(speed.worst_after_60) +
              " kt off after a minute (at most 10)");
}

namespace {

// GLIDESLOPE_TEST_DATA_DIR is the JSBSim root; the aircraft catalogue is
// beside it, one level up.
std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// A state an aeroplane can be handed over in: the controls a pilot held, and
// for how long, to fly it into that state.
struct Handed {
    const char* what;
    double elevator;
    double aileron;
    double rudder;
    double throttle;
    double seconds;
};

// **The states an autopilot can be handed, not a sample of them.** The first
// three are where one is normally engaged. The rest are where a pilot gives
// up and asks for help - and they are the ones that found the defect: the
// laws used to cancel their own damping through an integral clamped to the
// control's travel, so a large sideslip or pitch rate broke the cancellation
// and the control jumped to its stop. A Mosquito handed over skidding in its
// landing roll moved the rudder its whole travel in one frame.
const std::vector<Handed> handovers{
    {"trimmed and level", 0.0, 0.0, 0.0, 0.65, 20.0},
    {"a climbing turn", -0.05, 0.10, 0.03, 0.85, 20.0},
    {"gliding, power off", 0.05, 0.0, 0.0, 0.0, 15.0},
    {"skidding on full rudder", 0.0, 0.0, 1.0, 0.65, 6.0},
    {"rolling on full aileron", 0.0, 1.0, 0.0, 0.65, 4.0},
    {"bunted hard nose down", -1.0, 0.0, 0.0, 0.65, 4.0},
    {"pulled into a stall", 1.0, 0.0, 0.0, 0.30, 8.0},
    {"a spiral on crossed controls", 0.4, 0.8, -0.8, 0.85, 8.0},
};

// **A flight model is only asked what it can answer.** JSBSim's aerodynamic
// tables are looked up on angle of attack, sideslip and Mach, and it asserts
// on an index outside them - a 1930s flying boat put into a spiral at 20,000
// ft left its own tables and aborted the run. So the fly-in stops as soon as
// the aeroplane reaches the edge of what its model covers, and the autopilot
// is handed the most extreme state that model can actually produce. This is a
// limit of the flight models, not of the rule being tested.
bool inside(const Aircraft& a, double most_deg, double most_mach, double least_ft) {
    return std::abs(a.property("aero/alpha-deg")) <= most_deg &&
           std::abs(a.property("aero/beta-deg")) <= most_deg &&
           a.property("velocities/mach") <= most_mach &&
           a.property("position/h-sl-ft") > least_ft;
}

// **The fly-in stops inside the envelope the measurement needs, not at its
// edge.** Stopping at the same bound left the aeroplane already outside it
// when the autopilot was handed over, and not one step could be measured - 19
// of the 128 handovers measured nothing and the test said so rather than
// passing on an empty walk.
bool flying_into_it(const Aircraft& a) {
    return inside(a, 18.0, 0.85, 3000.0);
}

bool answerable(const Aircraft& a) {
    return inside(a, 25.0, 0.90, 1000.0);
}

// The largest a control moved in one step: the first step alone, which is the
// handover, and the worst of the five seconds after, which is the autopilot
// flying.
struct Engaged {
    double first = 0.0;
    double after = 0.0;
    int measured = 0;
};

Engaged engage_after(const CatalogueEntry& e, const Handed& h) {
    Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    // High enough that four seconds of full forward stick does not reach the
    // ground before the autopilot is handed the aeroplane, and low enough to
    // be within reach of every aeroplane the data holds - the slowest is a
    // flying boat whose ceiling is about 16,000 ft.
    ic.altitude_ft = 10000.0;
    ic.airspeed_kts = e.start_airspeed_kts;
    ic.engine_running = true;
    aircraft.initialize(ic);
    glideslope::sim::Controls pilot;
    pilot.elevator = h.elevator;
    pilot.aileron = h.aileron;
    pilot.rudder = h.rudder;
    pilot.throttle = h.throttle;
    const int into_it = static_cast<int>(h.seconds * steps_per_second);
    for (int i = 0; i < into_it && flying_into_it(aircraft); ++i) {
        aircraft.set_controls(pilot);
        aircraft.step();
    }
    Autopilot autopilot(aircraft, pilot);
    Engaged worst;
    glideslope::sim::Controls before = pilot;
    for (int i = 0; i < 5 * steps_per_second && answerable(aircraft); ++i) {
        ++worst.measured;
        const glideslope::sim::Controls c = autopilot.fly();
        const double step = std::max({std::abs(c.aileron - before.aileron),
                                      std::abs(c.elevator - before.elevator),
                                      std::abs(c.rudder - before.rudder),
                                      std::abs(c.throttle - before.throttle)});
        if (i == 0) {
            worst.first = step;
        } else {
            worst.after = std::max(worst.after, step);
        }
        aircraft.set_controls(c);
        aircraft.step();
        before = c;
    }
    return worst;
}

} // namespace

// A pilot's hand moves a control through its full travel in about a second -
// 1/120 of it in a 120 Hz step. **The autopilot must not move a control
// faster than that: not on the step it is engaged, whatever state it is
// handed, and not on any step it flies after.** This walks every aeroplane
// the data holds through every state above to say so.
GLIDESLOPE_TEST(the_autopilot_never_moves_a_control_faster_than_a_pilots_hand) {
    // The loops rate limit to exactly this, so a step lands on the bar rather
    // than under it and the comparison carries a rounding tolerance.
    constexpr double a_hands_pace =
        1.0 / static_cast<double>(steps_per_second) + 1e-9;
    const std::vector<CatalogueEntry> catalogue =
        glideslope::sim::read_catalogue(data());
    check(!catalogue.empty(), "the data holds aircraft");
    std::string failures;
    std::size_t walked = 0;
    double worst_first = 0.0;
    double worst_after = 0.0;
    for (const CatalogueEntry& e : catalogue) {
        for (const Handed& h : handovers) {
            ++walked;
            const Engaged w = engage_after(e, h);
            worst_first = std::max(worst_first, w.first);
            worst_after = std::max(worst_after, w.after);
            if (w.measured == 0) {
                failures += "\n  " + e.id + ", " + h.what +
                            ": not one step was measured - the aeroplane was already "
                            "outside its flight model's tables before the handover";
            } else if (!(w.first <= a_hands_pace)) {
                failures += "\n  " + e.id + ", " + h.what + ": the first step moved " +
                            std::to_string(w.first) + " of a control's travel";
            } else if (!(w.after <= a_hands_pace)) {
                failures += "\n  " + e.id + ", " + h.what + ": flying on, a step moved " +
                            std::to_string(w.after) + " of a control's travel";
            }
        }
    }
    std::printf("%zu aeroplanes x %zu states = %zu handovers; worst first step %.4f, "
                "worst step in the five seconds after %.4f (a hand moves %.4f)\n",
                catalogue.size(), handovers.size(), walked, worst_first, worst_after,
                a_hands_pace);
    check(walked == catalogue.size() * handovers.size(),
          "every aeroplane was handed over in every state: " + std::to_string(walked) +
              " of " + std::to_string(catalogue.size() * handovers.size()));
    check(failures.empty(),
          "no step the autopilot takes moves a control faster than a pilot's hand:" +
              failures);
}

namespace {

// **Near an aeroplane's ceiling, as the AI flies it**: the height at which
// its best rate of climb has fallen to 50 ft/min - halfway from its service
// ceiling, where it is 100 (the FAA's Pilot's Handbook of Aeronautical
// Knowledge, FAA-H-8083-25, chapter 11), to its absolute ceiling, where it is
// none. It is found by flying it - full throttle, the mixture where the AI
// leaves it, its published best-climb speed held on the elevator by the test
// pilot - from 6,000 ft until a half-minute's climb is 50 ft/min or less, and
// it is the height reached then.
constexpr double near_the_ceiling_fpm = 50.0;
double near_the_ceiling_ft(const CatalogueEntry& e, double climb_kts) {
    Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 6000.0;
    ic.airspeed_kts = climb_kts;
    ic.engine_running = true;
    aircraft.initialize(ic);
    glideslope::sim::TestPilot pilot(aircraft);
    glideslope::sim::Controls c;
    c.throttle = 1.0;
    constexpr int window = 30 * steps_per_second;
    double window_start_ft = altitude(aircraft);
    // An hour is far longer than any of these takes; reaching it says the
    // climb never slowed, which is a flight model fault and not a ceiling.
    for (int i = 1; i <= 3600 * steps_per_second; ++i) {
        c.elevator = pilot.pitch_to(pilot.pitch_for_speed(climb_kts));
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        aircraft.set_controls(c);
        aircraft.step();
        if (i % window == 0) {
            const double climbed = altitude(aircraft) - window_start_ft;
            // Settled first: the first two windows are the pilot finding its
            // pitch.
            if (i > 2 * window && climbed * 2.0 <= near_the_ceiling_fpm) {
                return altitude(aircraft);
            }
            window_start_ft = altitude(aircraft);
        }
    }
    check(false, e.id + " was still climbing after an hour at full throttle");
    return 0.0;
}

// Level flight on the autopilot, holding its height, its speed and its
// heading, until it has settled: what it held over the last half minute.
struct Level {
    double worst_ft = 0.0;       // off the height
    double kts = 0.0;            // the speed it held
    bool throttle_at_stop = true; // the throttle full throughout
    bool settled = false;        // the speed and height settled in ten minutes
    // From the first step: the most it was off the height, and when it was
    // last off it by more than the calm-air altitude band.
    double sag_ft = 0.0;
    double back_s = 0.0;
};

// The calm-air altitude band of
// the_autopilot_captures_a_new_heading_altitude_airspeed_and_climb.
constexpr double altitude_band_ft = 20.0;

// **Level until it has settled, not for a set time**: an aeroplane asked for
// a speed it cannot reach near its ceiling slows to the one it can for
// minutes, and a turn begun while it is still slowing would be charged with
// that. Settled is half a minute in which the airspeed moves less than half a
// knot and the height less than 5 ft - after a first half minute, when the
// autopilot has just been handed the aeroplane. **The height too**: handed
// over near its ceiling at its best-climb speed, a Cessna 182 sinks 23 ft
// before the throttle reaches its stop, and the altitude hold, which may no
// longer buy that back with speed, climbs back at the 30 ft/min the aeroplane
// has there. Its speed is steady long before its height, and a turn begun
// then was charged with the handover.
Level settle(Aircraft& aircraft, Autopilot& autopilot, double altitude_ft, bool handed) {
    constexpr int window = 30 * steps_per_second;
    Level l;
    int flown = 0;
    for (int w = 0; !l.settled && w < 20; ++w) {
        const double from_kts = airspeed(aircraft);
        const double from_ft = altitude(aircraft);
        l.worst_ft = 0.0;
        l.throttle_at_stop = true;
        for (int i = 0; i < window; ++i) {
            const glideslope::sim::Controls c = autopilot.fly();
            aircraft.set_controls(c);
            aircraft.step();
            ++flown;
            const double off_ft = std::abs(altitude(aircraft) - altitude_ft);
            l.worst_ft = std::max(l.worst_ft, off_ft);
            l.sag_ft = std::max(l.sag_ft, off_ft);
            if (off_ft > altitude_band_ft) {
                l.back_s = static_cast<double>(flown) / steps_per_second;
            }
            l.throttle_at_stop = l.throttle_at_stop && c.throttle >= 0.999;
        }
        l.settled = (w >= 1 || !handed) && std::abs(airspeed(aircraft) - from_kts) < 0.5 &&
                    std::abs(altitude(aircraft) - from_ft) < 5.0;
    }
    l.kts = airspeed(aircraft);
    return l;
}

// A turn on the autopilot of `by_deg`, left or right - ninety degrees given
// three minutes to turn and settle, right round given six. A turn of more
// than ninety degrees is asked for ninety degrees ahead at a time, as a
// heading bug is wound round, until the last ninety.
struct Turn {
    double worst_ft = 0.0;        // off the height
    double least_kts = 1e9;       // the slowest it flew
    double most_bank_deg = 0.0;
    double turned_deg = 0.0;
    double heading_off_deg = 0.0; // at the end
};

Turn turn(Aircraft& aircraft, Autopilot& autopilot, double altitude_ft, double by_deg) {
    AutopilotModes modes = autopilot.modes();
    const double start_deg = heading(aircraft);
    const double to = std::remainder(start_deg + by_deg, 360.0);
    double last = start_deg;
    Turn t;
    const double seconds = std::abs(by_deg) > 90.0 ? 360.0 : 180.0;
    for (int i = 0; i < static_cast<int>(seconds * steps_per_second); ++i) {
        const double left = by_deg - t.turned_deg;
        modes.heading_deg =
            std::abs(left) > 90.0
                ? std::fmod(heading(aircraft) + std::copysign(90.0, left) + 720.0, 360.0)
                : std::fmod(to + 360.0, 360.0);
        autopilot.set(modes);
        aircraft.set_controls(autopilot.fly());
        aircraft.step();
        t.turned_deg += std::remainder(heading(aircraft) - last, 360.0);
        last = heading(aircraft);
        t.worst_ft = std::max(t.worst_ft, std::abs(altitude(aircraft) - altitude_ft));
        t.least_kts = std::min(t.least_kts, airspeed(aircraft));
        t.most_bank_deg =
            std::max(t.most_bank_deg, std::abs(aircraft.property("attitude/phi-deg")));
    }
    t.heading_off_deg = std::remainder(heading(aircraft) - to, 360.0);
    return t;
}

// The light aeroplanes the data holds, each flown by a test of its own below
// so that they run side by side.
const std::vector<std::string> light_aeroplanes{"c172p", "c182", "j3cub", "pa28"};

// **An aeroplane near its ceiling has almost no power to spare, and a turn
// asks for more**: the lift that holds the height in a turn is its weight over
// the cosine of the bank, and the induced drag grows with the square of it.
// The autopilot used to bank to its full limit regardless, and the pitch that
// held the height bled the speed - a Cessna 182 turning once round slowed
// from 82 knots to 60. This turns the aeroplane through ninety degrees and
// through a full circle, each way, at 3,000 ft and near its ceiling, asked for
// its best-climb speed and for the speed it starts a flight at - 16 turns -
// and holds every one to the bands a turn at 3,000 ft is held to. **It is
// the full circles that find the fault**: a ninety-degree turn is over
// before the old autopilot had spent more than four knots, and the Cub has
// the power to spare for either.
void turns_near_the_ceiling_as_at_3000_ft(const std::string& id) {
    // The height band: the calm-air altitude band of
    // the_autopilot_captures_a_new_heading_altitude_airspeed_and_climb.
    constexpr double band_ft = altitude_band_ft;
    // **The hand-over, pinned on its own.** Handed over at its best-climb
    // speed with the throttle it starts a flight at, an aeroplane sinks while
    // the throttle comes up: 21 to 31 ft at 3,000 ft, back within the band in
    // 13 to 18 s, with the altitude hold's speed floor or without it. Near its
    // ceiling the floor will not buy the height back with speed, so it comes
    // back at the little climb the aeroplane has there: 26 to 39 ft, 2 to 8
    // ft more than at 3,000 ft, back in 35 to 50 s. So near the ceiling the
    // hand-over may cost no more than 15 ft beyond what the same hand-over
    // costs at 3,000 ft, and every hand-over is back within the band in a
    // minute and a half - room for the drift between machines, and far short
    // of a hand-over the floor let sink away.
    constexpr double handover_more_than_at_3000_ft = 15.0;
    constexpr double handover_back_s = 90.0;
    std::map<double, double> sag_at_3000_ft; // by the speed asked
    // The turn ends on its heading within the calm-air heading band.
    constexpr double heading_band_deg = 2.0;
    // And near the speed it held level: a turn that keeps its height by
    // bleeding the airspeed toward the stall is not a turn the aeroplane
    // sustains.
    constexpr double speed_band_kts = 5.0;
    const CatalogueEntry e = glideslope::sim::find_aircraft(data(), id);
    const double climb_kts = glideslope::sim::departure_speeds(data(), e.model).climb_kts;
    const double ceiling_ft = near_the_ceiling_ft(e, climb_kts);
    std::string failures;
    std::size_t turns = 0;
    // One flight for each height and speed: the autopilot handed the
    // aeroplane there, and each turn flown from level flight settled after
    // the last.
    for (const double altitude_ft : {3000.0, ceiling_ft}) {
        for (const double kts : {climb_kts, e.start_airspeed_kts}) {
            Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = -33.9;
            ic.longitude_deg = 151.2;
            ic.altitude_ft = altitude_ft;
            ic.heading_deg = 0.0;
            ic.airspeed_kts = kts;
            ic.engine_running = true;
            aircraft.initialize(ic);
            glideslope::sim::Controls controls;
            controls.throttle = e.start_throttle;
            Autopilot autopilot(aircraft, controls);
            AutopilotModes modes = autopilot.modes();
            modes.heading_deg = 0.0;
            modes.altitude_ft = altitude_ft;
            modes.airspeed_kts = kts;
            autopilot.set(modes);
            bool handed = true;
            for (const double by : {90.0, -90.0, 360.0, -360.0}) {
                ++turns;
                const Level l = settle(aircraft, autopilot, altitude_ft, handed);
                const Turn t = turn(aircraft, autopilot, altitude_ft, by);
                char line[400];
                std::snprintf(line, sizeof line,
                              "%s at %.0f ft, %.1f kt asked, turning %+.0f: level within "
                              "%.1f ft at %.1f kt%s; in the turn within %.1f ft, %.1f kt "
                              "at the slowest, %.1f degrees of bank at most, %.0f degrees "
                              "turned, finally %.2f off the heading",
                              e.id.c_str(), altitude_ft, kts, by, l.worst_ft, l.kts,
                              l.throttle_at_stop ? ", the throttle at its stop" : "",
                              t.worst_ft, t.least_kts, t.most_bank_deg, t.turned_deg,
                              t.heading_off_deg);
                std::printf("%s\n", line);
                if (handed && altitude_ft == 3000.0) {
                    sag_at_3000_ft[kts] = l.sag_ft;
                }
                if (handed) {
                    std::printf("  handed over: %.1f ft off at most, back within %.0f ft "
                                "after %.0f s\n",
                                l.sag_ft, altitude_band_ft, l.back_s);
                }
                if (!l.settled) {
                    failures += std::string("\n  ") + line +
                                " - the speed or the height had not settled after ten "
                                "minutes level";
                } else if (handed &&
                           !((altitude_ft == 3000.0 ||
                              l.sag_ft <= sag_at_3000_ft.at(kts) +
                                              handover_more_than_at_3000_ft) &&
                             l.back_s <= handover_back_s)) {
                    char sag[200];
                    std::snprintf(sag, sizeof sag,
                                  " - handed over, it was %.1f ft off (%.1f at 3,000 ft) "
                                  "and back within %.0f ft after %.0f s (at most %.0f)",
                                  l.sag_ft, sag_at_3000_ft.count(kts) ? sag_at_3000_ft.at(kts) : 0.0,
                                  altitude_band_ft, l.back_s, handover_back_s);
                    failures += std::string("\n  ") + line + sag;
                } else if (altitude_ft != 3000.0 && kts != climb_kts && !l.throttle_at_stop) {
                    // **The situation is built, not hoped for**: near its
                    // ceiling, asked for the speed it starts a flight at, the
                    // aeroplane has no more throttle to give.
                    failures += std::string("\n  ") + line +
                                " - the throttle was not at its stop, so this is not "
                                "near the ceiling";
                } else if (!(l.worst_ft <= band_ft && t.worst_ft <= band_ft &&
                             t.least_kts >= l.kts - speed_band_kts &&
                             std::abs(t.turned_deg - by) <= heading_band_deg &&
                             std::abs(t.heading_off_deg) <= heading_band_deg)) {
                    failures += std::string("\n  ") + line;
                }
                handed = false;
            }
        }
    }
    std::printf("%s: 2 heights x 2 speeds x 4 turns = %zu turns, near the ceiling at "
                "%.0f ft\n",
                id.c_str(), turns, ceiling_ft);
    check(turns == 16, "every turn was flown: " + std::to_string(turns) + " of 16");
    check(failures.empty(), "each turn holds its height within " + std::to_string(band_ft) +
                                " ft and its speed within " + std::to_string(speed_band_kts) +
                                " kt, and ends on its heading:" + failures);
}

} // namespace

GLIDESLOPE_TEST(every_light_aeroplane_the_data_holds_is_turned_near_its_ceiling) {
    std::vector<std::string> found;
    for (const CatalogueEntry& e : glideslope::sim::read_catalogue(data())) {
        if (e.aircraft_class == glideslope::sim::AircraftClass::light_aircraft) {
            found.push_back(e.id);
        }
    }
    std::sort(found.begin(), found.end());
    std::string listed;
    for (const std::string& id : found) {
        listed += " " + id;
    }
    check(found == light_aeroplanes,
          "the light aeroplanes turned near their ceilings are the four the data holds; "
          "it holds" + listed);
}

GLIDESLOPE_TEST(a_cessna_172p_near_its_ceiling_holds_its_height_through_a_turn_as_at_3000_ft) {
    turns_near_the_ceiling_as_at_3000_ft("c172p");
}
GLIDESLOPE_TEST(a_cessna_182_near_its_ceiling_holds_its_height_through_a_turn_as_at_3000_ft) {
    turns_near_the_ceiling_as_at_3000_ft("c182");
}
// The Cub has power to spare near its ceiling, so this passes with the bank
// limit or without it: it pins that the Cub turns as well as it did, not the
// limit. The other three are the ones seen to fail without it.
GLIDESLOPE_TEST(a_piper_cub_near_its_ceiling_holds_its_height_through_a_turn_as_at_3000_ft) {
    turns_near_the_ceiling_as_at_3000_ft("j3cub");
}
GLIDESLOPE_TEST(a_cherokee_near_its_ceiling_holds_its_height_through_a_turn_as_at_3000_ft) {
    turns_near_the_ceiling_as_at_3000_ft("pa28");
}

namespace {

// **Asked for a height it cannot hold, an aeroplane gives up height, not
// airspeed.** The altitude hold pitches up for the height asked of it, and
// once the throttle has no more to give the speed pays for every foot: the
// old autopilot flew a Cessna asked for a height above its ceiling to 46
// knots and sinking, in the stall. It must fly no slower than the
// aeroplane's best-climb speed - the speed at which it climbs fastest, so
// that a height it cannot hold there it can hold nowhere - and let the height
// go.
//
// Two situations, each handed over at the two speeds the turns above are
// flown at - the best-climb speed and the speed a flight starts at - and
// flown until it has settled:
//
//   level near its ceiling at the speed it was handed over at, settled there
//     as the turns above are, then asked for 3,000 ft higher, which it cannot
//     reach: the throttle goes to its stop - ten minutes;
//   level at 6,000 ft with the throttle left at a fifth, as a pilot might
//     leave it, and no speed held - so the throttle will give no more - asked
//     to hold that height, which on that power it cannot: it must come down -
//     five minutes, and it must stay clear of the ground.
//
// **Not handed over above its ceiling**, which would be the plainer second
// situation: with the mixture full rich, as the AI leaves it, the engine
// stops a thousand feet above the ceiling - JSBSim's piston engine runs too
// rich to fire as the air thins - and a glide is not what is being asked.
// The engine running throughout is checked, not assumed.
//
// The airspeed may never fall more than the calm-air airspeed band of
// the_autopilot_captures_a_new_heading_altitude_airspeed_and_climb below the
// best-climb speed, and ends within that band of it. On too little power the
// aeroplane ends lower than it began; near its ceiling it climbs what it can,
// and ends no lower than it began, within the calm-air altitude band. **That
// last is not given**: asked for the best-climb speed itself, the throttle and
// the elevator once shared the speed between them, and the Cherokee came down
// 1,250 ft on part throttle.
void gives_up_height_not_airspeed(const std::string& id) {
    constexpr double speed_band_kts = 2.0;
    // The calm-air altitude band of the same test.
    constexpr double height_band_ft = 20.0;
    const CatalogueEntry e = glideslope::sim::find_aircraft(data(), id);
    const double climb_kts = glideslope::sim::departure_speeds(data(), e.model).climb_kts;
    const double ceiling_ft = near_the_ceiling_ft(e, climb_kts);
    struct Situation {
        const char* what;
        double from_ft;
        double asked_ft;
        bool speed_held; // the throttle holds the speed handed over at
        double throttle; // as handed over
        double seconds;  // flown for
    };
    const Situation situations[] = {
        {"near its ceiling, asked for 3,000 ft more", ceiling_ft, ceiling_ft + 3000.0,
         true, e.start_throttle, 600.0},
        {"at 6,000 ft on a fifth of its throttle, asked to hold it", 6000.0, 6000.0, false,
         0.2, 300.0},
    };
    std::string failures;
    std::size_t flights = 0;
    for (const Situation& s : situations) {
        for (const double kts : {climb_kts, e.start_airspeed_kts}) {
            ++flights;
            Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = -33.9;
            ic.longitude_deg = 151.2;
            ic.altitude_ft = s.from_ft;
            ic.heading_deg = 0.0;
            ic.airspeed_kts = kts;
            ic.engine_running = true;
            aircraft.initialize(ic);
            glideslope::sim::Controls controls;
            controls.throttle = s.throttle;
            Autopilot autopilot(aircraft, controls);
            AutopilotModes modes = autopilot.modes();
            modes.heading_deg = 0.0;
            modes.altitude_ft = s.asked_ft;
            bool settled = true;
            if (s.speed_held) {
                // Level where it was handed over, settled, before it is asked
                // for more: what the handover itself does to the speed is
                // not the altitude hold's to answer for.
                modes.airspeed_kts = kts;
                modes.altitude_ft = s.from_ft;
                autopilot.set(modes);
                settled = settle(aircraft, autopilot, s.from_ft, true).settled;
                modes.altitude_ft = s.asked_ft;
            } else {
                modes.airspeed_kts.reset();
            }
            autopilot.set(modes);
            const double start_ft = altitude(aircraft);
            double least_kts = airspeed(aircraft);
            double highest_ft = altitude(aircraft);
            double lowest_ft = altitude(aircraft);
            double least_rpm = aircraft.property("propulsion/engine/engine-rpm");
            for (int i = 0; i < static_cast<int>(s.seconds * steps_per_second); ++i) {
                aircraft.set_controls(autopilot.fly());
                aircraft.step();
                least_rpm =
                    std::min(least_rpm, aircraft.property("propulsion/engine/engine-rpm"));
                least_kts = std::min(least_kts, airspeed(aircraft));
                highest_ft = std::max(highest_ft, altitude(aircraft));
                lowest_ft = std::min(lowest_ft, altitude(aircraft));
            }
            const double end_ft = altitude(aircraft);
            const double end_kts = airspeed(aircraft);
            char line[400];
            std::snprintf(line, sizeof line,
                          "%s %s, handed over at %.1f kt (best climb %.1f): %.1f kt at "
                          "the slowest, %.1f kt at the end; from %.0f ft to %.0f ft, "
                          "%.0f at the highest, %.0f at the lowest",
                          e.id.c_str(), s.what, kts, climb_kts, least_kts, end_kts,
                          start_ft, end_ft, highest_ft, lowest_ft);
            std::printf("%s\n", line);
            if (!settled) {
                failures += std::string("\n  ") + line +
                            " - the speed had not settled after ten minutes level";
            } else if (!(least_rpm > 0.0)) {
                // **The situation is built, not hoped for**: an engine that
                // stops leaves a glide, which is not what is being asked.
                failures += std::string("\n  ") + line + " - the engine stopped";
            } else if (!(lowest_ft > 1000.0)) {
                failures += std::string("\n  ") + line + " - it came too near the ground";
            } else if (!(least_kts >= climb_kts - speed_band_kts &&
                         std::abs(end_kts - climb_kts) <= speed_band_kts &&
                         (s.speed_held ? end_ft >= start_ft - height_band_ft
                                       : end_ft < start_ft))) {
                failures += std::string("\n  ") + line;
            }
        }
    }
    std::printf("%s: 2 situations x 2 speeds = %zu flights, near the ceiling at %.0f ft\n",
                id.c_str(), flights, ceiling_ft);
    check(flights == 4, "every flight was flown: " + std::to_string(flights) + " of 4");
    check(failures.empty(),
          "asked for a height it cannot hold, each gives up height and flies within " +
              std::to_string(speed_band_kts) + " kt of its best-climb speed:" + failures);
}

} // namespace

GLIDESLOPE_TEST(every_light_aeroplane_the_data_holds_is_asked_for_a_height_it_cannot_hold) {
    std::vector<std::string> found;
    for (const CatalogueEntry& e : glideslope::sim::read_catalogue(data())) {
        if (e.aircraft_class == glideslope::sim::AircraftClass::light_aircraft) {
            found.push_back(e.id);
        }
    }
    std::sort(found.begin(), found.end());
    std::string listed;
    for (const std::string& id : found) {
        listed += " " + id;
    }
    check(found == light_aeroplanes,
          "the light aeroplanes asked for a height they cannot hold are the four "
          "the data holds; it holds" + listed);
}

GLIDESLOPE_TEST(a_cessna_172p_asked_for_a_height_it_cannot_hold_gives_up_height_not_airspeed) {
    gives_up_height_not_airspeed("c172p");
}
GLIDESLOPE_TEST(a_cessna_182_asked_for_a_height_it_cannot_hold_gives_up_height_not_airspeed) {
    gives_up_height_not_airspeed("c182");
}
GLIDESLOPE_TEST(a_piper_cub_asked_for_a_height_it_cannot_hold_gives_up_height_not_airspeed) {
    gives_up_height_not_airspeed("j3cub");
}
GLIDESLOPE_TEST(a_cherokee_asked_for_a_height_it_cannot_hold_gives_up_height_not_airspeed) {
    gives_up_height_not_airspeed("pa28");
}

// **Which aircraft have a speed floor, and which have none, said by name.**
// Every light aeroplane has one, its published best-climb speed, read when
// its model loads; every other aircraft has none, and is named here with the
// reason. A light aeroplane whose figures give no climb speed does not load
// at all, rather than fly without the floor unnoticed.
GLIDESLOPE_TEST(every_aircraft_the_data_holds_has_a_speed_floor_or_is_named_without_one) {
    const std::map<std::string, std::string> without = {
        {"737-300", "an airliner"},
        {"747-400", "an airliner, and it publishes no climb speed"},
        {"787-8", "an airliner"},
        {"a320", "an airliner"},
        {"a380", "an airliner"},
        {"b2", "a bomber"},
        {"f15c", "a fighter"},
        {"f22", "a fighter, and it publishes no climb speed"},
        {"f35b", "a fighter"},
        {"learjet35a", "a business jet"},
        {"mosquito-fb6", "a Second World War aeroplane"},
        {"short_s23", "a seaplane"},
    };
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data());
    std::string failures;
    std::size_t with_floor = 0;
    std::size_t named = 0;
    for (const CatalogueEntry& e : catalogue) {
        const Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
        const std::optional<double> floor_kts = aircraft.climb_floor_kts();
        const bool light = e.aircraft_class == glideslope::sim::AircraftClass::light_aircraft;
        if (light) {
            const double climb_kts =
                glideslope::sim::departure_speeds(data(), e.model).climb_kts;
            if (!floor_kts || *floor_kts != climb_kts) {
                failures += "\n  " + e.id + " is a light aeroplane without its best-climb "
                            "speed, " + std::to_string(climb_kts) + " kt, as its floor";
            } else {
                ++with_floor;
                std::printf("%s: a floor at %.1f kt\n", e.id.c_str(), *floor_kts);
            }
        } else if (floor_kts) {
            failures += "\n  " + e.id + " is not a light aeroplane and has a floor";
        } else if (without.count(e.id) == 0) {
            failures += "\n  " + e.id + " has no floor and is not named here";
        } else {
            ++named;
            std::printf("%s: no floor - %s\n", e.id.c_str(), without.at(e.id).c_str());
        }
    }
    std::printf("%zu aircraft: %zu with a floor, %zu named without one\n", catalogue.size(),
                with_floor, named);
    check(failures.empty(), "each aircraft has a floor or is named without one:" + failures);
    check(with_floor + named == catalogue.size() && named == without.size(),
          "every aircraft is accounted for, and every name here is an aircraft: " +
              std::to_string(with_floor) + " with a floor and " + std::to_string(named) +
              " named of " + std::to_string(catalogue.size()));
}

// **Flaps out while the floor holds the speed, and the floor lets go.** Near
// its ceiling and asked for 3,000 ft more, a light aeroplane is held at its
// best-climb speed by the floor with the throttle at its stop. Then the flaps
// go out, and once they are out it is asked to come down 1,000 ft at that
// speed: with no floor for a flapped aeroplane, the throttle is the speed's
// again and comes off its stop - within two minutes, as the throttle's
// integral unwinds from its stop: 40 to 67 s. A hold left over from before the flaps went out once kept the
// throttle at its stop for good. The Cub has no flaps and fixed gear, so it
// is never anything but clean; it is left out for that.
GLIDESLOPE_TEST(a_light_aeroplane_whose_flaps_go_out_while_the_floor_holds_it_flies_on_its_throttle) {
    const std::vector<std::string> flapped{"c172p", "c182", "pa28"};
    const std::vector<std::string> left_out{"j3cub"};
    std::string failures;
    std::size_t flown = 0;
    for (const std::string& id : light_aeroplanes) {
        if (std::find(left_out.begin(), left_out.end(), id) != left_out.end()) {
            std::printf("%s: left out - no flaps, and fixed gear\n", id.c_str());
            continue;
        }
        const CatalogueEntry e = glideslope::sim::find_aircraft(data(), id);
        const double climb_kts =
            glideslope::sim::departure_speeds(data(), e.model).climb_kts;
        const double ceiling_ft = near_the_ceiling_ft(e, climb_kts);
        Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = ceiling_ft;
        ic.heading_deg = 0.0;
        ic.airspeed_kts = climb_kts;
        ic.engine_running = true;
        aircraft.initialize(ic);
        glideslope::sim::Controls controls;
        controls.throttle = 1.0;
        Autopilot autopilot(aircraft, controls);
        AutopilotModes modes = autopilot.modes();
        modes.heading_deg = 0.0;
        modes.altitude_ft = ceiling_ft + 3000.0;
        modes.airspeed_kts = climb_kts;
        autopilot.set(modes);
        // Two minutes on the floor: the throttle at its stop throughout the
        // last of them, and the speed on the best-climb speed - built, not
        // hoped for.
        bool at_stop = true;
        for (int i = 0; i < 120 * steps_per_second; ++i) {
            const glideslope::sim::Controls c = autopilot.fly();
            aircraft.set_controls(c);
            aircraft.step();
            if (i >= 60 * steps_per_second) {
                at_stop = at_stop && c.throttle >= 0.999;
            }
        }
        const double held_kts = airspeed(aircraft);
        // The flaps out while the floor still holds it - asked for the same
        // height, which it still cannot reach - for twenty seconds, time for
        // them to run all the way out.
        for (int i = 0; i < 20 * steps_per_second; ++i) {
            glideslope::sim::Controls c = autopilot.fly();
            c.flaps = 1.0;
            aircraft.set_controls(c);
            aircraft.step();
        }
        modes.altitude_ft = altitude(aircraft) - 1000.0;
        autopilot.set(modes);
        double off_stop_s = -1.0;
        for (int i = 0; i < 180 * steps_per_second && off_stop_s < 0.0; ++i) {
            glideslope::sim::Controls c = autopilot.fly();
            c.flaps = 1.0;
            aircraft.set_controls(c);
            aircraft.step();
            if (c.throttle < 0.999) {
                off_stop_s = static_cast<double>(i + 1) / steps_per_second;
            }
        }
        ++flown;
        char line[300];
        std::snprintf(line, sizeof line,
                      "%s: on the floor at %.1f kt (best climb %.1f)%s; flaps out, the "
                      "throttle off its stop after %.1f s",
                      id.c_str(), held_kts, climb_kts,
                      at_stop ? ", the throttle at its stop" : "", off_stop_s);
        std::printf("%s\n", line);
        if (!at_stop || std::abs(held_kts - climb_kts) > 2.0) {
            failures += std::string("\n  ") + line + " - not held on the floor first";
        } else if (off_stop_s < 0.0 || off_stop_s > 120.0) {
            failures += std::string("\n  ") + line;
        }
    }
    check(flown == flapped.size() && flown + left_out.size() == light_aeroplanes.size(),
          "every light aeroplane was flown or is named as left out: " +
              std::to_string(flown) + " flown");
    check(failures.empty(),
          "with the flaps out the throttle comes off its stop within two minutes:" +
              failures);
}
