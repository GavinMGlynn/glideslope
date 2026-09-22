#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/catalogue.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstdio>
#include <functional>
#include <memory>
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
