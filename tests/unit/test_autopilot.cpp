#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/catalogue.hpp"
#include "sim/controller.hpp"
#include "sim/departure.hpp"
#include "sim/figures.hpp"
#include "sim/lander.hpp"
#include "sim/leaner.hpp"
#include "sim/orbit_trial.hpp"
#include "sim/test_pilot.hpp"
#include "sim/weather.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
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
    // The climb's 45 s is a measured margin, not a handbook figure: it
    // settles in 38.2 s. At 100 kt and 4,000 ft the throttle is at its stop
    // for most of it, and on the FAA's mixture curve (tools/piston_mixture.py)
    // the 172P leaned makes 190 hp there against 205 on JSBSim's; on
    // JSBSim's it settled in 21 s.
    const Limit climb_rough{200.0, 150.0, 45.0};
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
// none. It is found by flying it - full throttle, the mixture as the AI
// leans it where the aeroplane has a lever (sim/leaner.hpp) and full rich
// where it has none, its published best-climb speed held on the elevator by
// the test pilot - from 6,000 ft until a half-minute's climb is 50 ft/min or
// less, and it is the height reached then.
//
// **The mixture it got there on is kept**, and an aeroplane handed over near
// its ceiling is handed over on it, as a pilot leaning in the climb would
// hand it over: full rich there the engine will not fire.
constexpr double near_the_ceiling_fpm = 50.0;
struct NearTheCeiling {
    double ft = 0.0;
    double mixture = 1.0; // the mixture it got there on
};
NearTheCeiling near_the_ceiling(const CatalogueEntry& e, double climb_kts) {
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
    std::optional<glideslope::sim::MixtureLeaner> leaner;
    if (aircraft.mixture_lever()) {
        leaner.emplace(aircraft, c.mixture);
    }
    constexpr int window = 30 * steps_per_second;
    double window_start_ft = altitude(aircraft);
    // An hour is far longer than any of these takes; reaching it says the
    // climb never slowed, which is a flight model fault and not a ceiling.
    for (int i = 1; i <= 3600 * steps_per_second; ++i) {
        c.elevator = pilot.pitch_to(pilot.pitch_for_speed(climb_kts));
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        if (leaner) {
            c.mixture = leaner->lean(c.throttle);
        }
        aircraft.set_controls(c);
        aircraft.step();
        if (i % window == 0) {
            const double climbed = altitude(aircraft) - window_start_ft;
            // Settled first: the first two windows are the pilot finding its
            // pitch.
            if (i > 2 * window && climbed * 2.0 <= near_the_ceiling_fpm) {
                return {altitude(aircraft), leaner ? leaner->resting() : 1.0};
            }
            window_start_ft = altitude(aircraft);
        }
    }
    check(false, e.id + " was still climbing after an hour at full throttle");
    return {};
}

// **The throttle that holds an aeroplane level at `altitude_ft` and `kts`**,
// on the mixture it climbed there on, as the test pilot finds it: the height
// held on the elevator and the speed on the throttle, by an integral, for two
// minutes, and the throttle it has then; at its stop if the speed cannot be
// held. **Near the ceiling an aeroplane is handed over on this, not on the
// throttle a flight starts at.** Handed over high on the start throttle, it
// sinks while the throttle comes up to what the height needs - 42 to 90 ft at
// the leaned ceilings, against 21 to 31 at 3,000 ft - and that sag is the
// mismatch between the throttle handed over and the height, not anything the
// autopilot does.
double level_throttle(const CatalogueEntry& e, double altitude_ft, double kts,
                      double mixture) {
    Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = altitude_ft;
    ic.airspeed_kts = kts;
    ic.engine_running = true;
    aircraft.initialize(ic);
    glideslope::sim::TestPilot pilot(aircraft);
    glideslope::sim::Controls c;
    c.throttle = 1.0;
    c.mixture = mixture;
    for (int i = 0; i < 120 * steps_per_second; ++i) {
        c.throttle = std::clamp(
            c.throttle + 0.02 * (kts - airspeed(aircraft)) / steps_per_second, 0.0, 1.0);
        c.elevator = pilot.pitch_to(pilot.pitch_for_altitude(altitude_ft));
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        aircraft.set_controls(c);
        aircraft.step();
    }
    return c.throttle;
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
    const NearTheCeiling ceiling = near_the_ceiling(e, climb_kts);
    const double ceiling_ft = ceiling.ft;
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
            if (altitude_ft == ceiling_ft) {
                controls.mixture = ceiling.mixture;
                controls.throttle = level_throttle(e, altitude_ft, kts, ceiling.mixture);
            }
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
// situation: with the mixture full rich, as the AI leaves the Cub's, which
// has no lever, the engine stops a thousand feet above the ceiling - JSBSim's
// piston engine runs too rich to fire as the air thins - and a glide is not
// what is being asked.
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
    const NearTheCeiling ceiling = near_the_ceiling(e, climb_kts);
    const double ceiling_ft = ceiling.ft;
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
            if (s.from_ft == ceiling_ft) {
                controls.mixture = ceiling.mixture;
                controls.throttle = level_throttle(e, s.from_ft, kts, ceiling.mixture);
            }
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

// **Every aircraft turns ninety degrees without overbanking or overshooting.**
// The bank limit is 25 degrees, and while the bank is at it an integral finds
// the aileron that holds it there (src/sim/autopilot.cpp, `aileron_trim_`):
// a Cessna 182 left proportional alone banked 29-30 to the left. That
// integral acts on every aircraft, and a heavy that rolls slowly lags its
// command for longer; so every aircraft the data holds is turned ninety
// degrees each way at 10,000 ft, at the airspeed and throttle it starts a
// flight at, after a minute level, and its most bank and its overshoot of the
// heading are reported and held to the limit's 25 degrees and 3 more for the
// roll-in, and to 4 degrees past the heading - the Mosquito overshoots most,
// 3.45 degrees turning left, with the integral or without it. On 2026-10-02
// the most bank was 27.6 (the Mosquito turning left; 28.0 without the
// integral), the airliners' 25.2-27.2 (25.1-26.2 without); an integral left
// to run while the bank lagged its command took the 787 and the A380 to 29.8,
// and this to red.
GLIDESLOPE_TEST(every_aircraft_turns_ninety_degrees_without_overbanking_or_overshooting) {
    constexpr double most_bank_deg = 28.0;
    constexpr double most_overshoot_deg = 4.0;
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data());
    std::string failures;
    std::size_t turns = 0;
    for (const CatalogueEntry& e : catalogue) {
        for (const double by : {90.0, -90.0}) {
            Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = -33.9;
            ic.longitude_deg = 151.2;
            ic.altitude_ft = 10000.0;
            ic.heading_deg = 0.0;
            ic.airspeed_kts = e.start_airspeed_kts;
            ic.engine_running = true;
            aircraft.initialize(ic);
            glideslope::sim::Controls controls;
            controls.throttle = e.start_throttle;
            Autopilot autopilot(aircraft, controls);
            AutopilotModes modes = autopilot.modes();
            modes.heading_deg = 0.0;
            modes.altitude_ft = 10000.0;
            modes.airspeed_kts = e.start_airspeed_kts;
            autopilot.set(modes);
            for (int i = 0; i < 60 * steps_per_second; ++i) {
                aircraft.set_controls(autopilot.fly());
                aircraft.step();
            }
            modes.heading_deg = std::fmod(by + 360.0, 360.0);
            autopilot.set(modes);
            double bank = 0.0;
            double overshoot = 0.0;
            for (int i = 0; i < 120 * steps_per_second; ++i) {
                aircraft.set_controls(autopilot.fly());
                aircraft.step();
                bank = std::max(bank, std::abs(aircraft.property("attitude/phi-deg")));
                const double past = std::remainder(heading(aircraft) - by, 360.0) *
                                    (by > 0.0 ? 1.0 : -1.0);
                overshoot = std::max(overshoot, past);
            }
            ++turns;
            char line[200];
            std::snprintf(line, sizeof line,
                          "%s turning %+.0f: %.1f degrees of bank at most, %.2f past the "
                          "heading",
                          e.id.c_str(), by, bank, overshoot);
            std::printf("%s\n", line);
            if (!(bank <= most_bank_deg && overshoot <= most_overshoot_deg)) {
                failures += std::string("\n  ") + line;
            }
        }
    }
    std::printf("%zu aircraft x 2 turns = %zu turns\n", catalogue.size(), turns);
    check(turns == 2 * catalogue.size(),
          "every aircraft turned both ways: " + std::to_string(turns));
    check(failures.empty(), "each turn banks no more than " + std::to_string(most_bank_deg) +
                                " degrees and overshoots no more than " +
                                std::to_string(most_overshoot_deg) + ":" + failures);
}

namespace {

// What a crosswind test leaves out of an aircraft, and why: its approach
// speed's two cases, or all four.
struct NotHeld {
    bool every_case = false;
    std::string reason;
};

// **Every aircraft in `expected` holds a heading in a crosswind without
// yawing.** Each is put at 3,000 ft heading north at the slowest a plan may
// fly it (its figures file's `<plan_speeds>`: its approach speed, or more
// where it cannot hold that clean), where the wind is the largest part of
// its airspeed - and at the airspeed it starts a flight at, the autopilot holding
// the heading, the height and the speed, in calm air and in a 20 kt wind from
// the west arriving all at once: a sideslip of 10 to 28 degrees to settle.
// After `settle_s` each must hold its sideslip within a degree and its heading
// within two until two minutes after the start. The space is every aircraft in
// `expected` (the catalogue's of `in_class`, asserted to be exactly those), at
// both speeds - asserted to differ, so no case is flown twice - in both airs;
// each case is flown or named in `left_out` with its reason.
void holds_a_heading_in_a_crosswind(const std::vector<std::string>& expected,
                                    const std::function<bool(const CatalogueEntry&)>& in_class,
                                    const std::map<std::string, NotHeld>& left_out,
                                    double settle_s) {
    constexpr double most_sideslip_deg = 1.0;
    constexpr double most_heading_off_deg = 2.0;
    std::string failures;
    std::vector<std::string> met;
    std::size_t flown = 0;
    std::size_t not_flown = 0;
    for (const CatalogueEntry& e : glideslope::sim::read_catalogue(data())) {
        if (!in_class(e)) {
            continue;
        }
        met.push_back(e.id);
        const auto out = left_out.find(e.id);
        if (out != left_out.end() && out->second.every_case) {
            std::printf("%s not flown: %s\n", e.id.c_str(), out->second.reason.c_str());
            not_flown += 4;
            continue;
        }
        std::vector<double> speeds;
        if (out != left_out.end()) {
            std::printf("%s not flown at the slowest a plan may fly it: %s\n", e.id.c_str(),
                        out->second.reason.c_str());
            not_flown += 2;
        } else {
            const double slowest = glideslope::sim::plan_speeds(data(), e.model).slowest_kts;
            check(slowest != e.start_airspeed_kts,
                  e.id + "'s slowest and starting speeds differ, so no case is flown twice");
            speeds.push_back(slowest);
        }
        speeds.push_back(e.start_airspeed_kts);
        for (const double speed_kts : speeds) {
            for (const bool windy : {false, true}) {
                const glideslope::sim::CrosswindFlown f =
                    glideslope::sim::fly_heading_in_crosswind(data(), e, speed_kts, windy,
                                                              settle_s);
                const double least_beta = f.least_sideslip_deg;
                const double most_beta = f.most_sideslip_deg;
                const double worst_heading = f.worst_heading_deg;
                const double most_beta_ever = f.most_sideslip_ever_deg;
                const double worst_height_ft = f.worst_height_ft;
                const double slowest_kts = f.slowest_kts;
                ++flown;
                char line[300];
                std::snprintf(line, sizeof line,
                              "%s at %.0f kt %s: sideslip %+.2f to %+.2f after %.0f s (%.1f at "
                              "most), heading within %.2f (height within %.0f ft, slowest %.0f "
                              "kt)",
                              e.id.c_str(), speed_kts,
                              windy ? "in a 20 kt crosswind" : "in calm air", least_beta,
                              most_beta, settle_s, most_beta_ever, worst_heading,
                              worst_height_ft, slowest_kts);
                std::printf("%s\n", line);
                if (!f.held()) {
                    failures += std::string("\n  ") + line;
                }
            }
        }
    }
    std::sort(met.begin(), met.end());
    std::vector<std::string> wanted = expected;
    std::sort(wanted.begin(), wanted.end());
    std::string listed;
    for (const std::string& id : met) {
        listed += " " + id;
    }
    check(met == wanted, "the aircraft flown are exactly those expected; the catalogue gives" +
                             listed);
    for (const auto& [id, out] : left_out) {
        check(std::find(met.begin(), met.end(), id) != met.end(),
              "what is left out is in the space: " + id);
    }
    std::printf("%zu aircraft x 2 speeds x 2 airs = %zu cases: %zu flown, %zu left out\n",
                met.size(), 4 * met.size(), flown, not_flown);
    check(flown + not_flown == 4 * wanted.size() && flown > 0,
          "every case flown or named as left out: " + std::to_string(flown) + " flown and " +
              std::to_string(not_flown) + " left out of " + std::to_string(4 * wanted.size()));
    check(failures.empty(), "each holds its sideslip within " +
                                std::to_string(most_sideslip_deg) + " degrees and its heading "
                                "within " + std::to_string(most_heading_off_deg) + ":" +
                                failures);
}

bool is_light(const CatalogueEntry& e) {
    return e.aircraft_class == glideslope::sim::AircraftClass::light_aircraft;
}

} // namespace

// Before the yaw damper the Cub and the Cherokee swung their sideslip 35
// degrees either way every few seconds in the crosswind and never settled,
// at both speeds; the Cessnas held it at their approach speeds but swung 14
// degrees either way at cruise (PROJECT_STATUS.md). Every case is flown.
GLIDESLOPE_TEST(every_light_aeroplane_holds_a_heading_in_a_20_kt_crosswind_and_in_calm_air_without_yawing) {
    holds_a_heading_in_a_crosswind(light_aeroplanes, is_light, {}, 30.0);
}

// The yaw damper's one gain acts on every aircraft, so every other one is
// flown the same way. On the old law, without the damper, the F-22 at 300 kt
// swung 4.8 degrees either way in the crosswind and never settled; every
// other case flown held it as it does with the damper. Five jets came down to
// the ground clean at their approach speed - a flaps-down figure - and were
// left out there, and the 747-400 and F-22 had no approach speed, until each
// aircraft had the slowest a plan may fly it: now every case is flown.
GLIDESLOPE_TEST(every_aircraft_but_the_light_aeroplanes_holds_a_heading_in_a_20_kt_crosswind_and_in_calm_air_without_yawing) {
    std::vector<std::string> others;
    for (const CatalogueEntry& e : glideslope::sim::read_catalogue(data())) {
        if (!is_light(e)) {
            others.push_back(e.id);
        }
    }
    check(others.size() + light_aeroplanes.size() ==
              glideslope::sim::read_catalogue(data()).size(),
          "the light aeroplanes and the rest are the whole catalogue");
    holds_a_heading_in_a_crosswind(
        others, [](const CatalogueEntry& e) { return !is_light(e); }, {}, 30.0);
}

// **Every aircraft holds a heading in a 20 kt crosswind at every speed a plan
// may fly it up to its start speed**, from its figures file's slowest in 5 kt
// steps, and the start speed itself, at 3,000 ft on the autopilot
// (sim::fly_heading_in_crosswind): its sideslip within a degree and its
// heading within two after 30 s. The other crosswind tests fly two speeds
// each; a swing lives between them. Before the B-2A's and the F-22A's own
// rudder integral (their catalogue's `yaw-damper`), the B-2A swung 7.3
// degrees of sideslip either way at 159 kt and 1.5 at 179, and the F-22A 7.8
// at 220 and 2.4 at 240, and their plans were kept above 194 and 255 kt for
// it. Below the slowest some do not hold it: flown from their approach speeds
// on these gains (2026-10-07) the 737-300, A380, B-2A, Learjet and F-35B
// swing or leave their tables (PROJECT_STATUS.md; an open tail). The space is every
// aircraft at every step, counted; none is left out. **Each aircraft in a test
// of its own**: all sixteen in one ran past 900 s in CI's linux-debug.
namespace {

const std::vector<std::string> crosswind_swept = {
    "c172p",
    "c182",
    "pa28",
    "j3cub",
    "short_s23",
    "mosquito-fb6",
    "737-300",
    "747-400",
    "787-8",
    "a320",
    "a380",
    "learjet35a",
    "b2",
    "f15c",
    "f22",
    "f35b",
};

void holds_a_heading_at_every_plan_speed(const std::string& id) {
    const CatalogueEntry e = glideslope::sim::find_aircraft(data(), id);
    const double low_kts = glideslope::sim::plan_speeds(data(), e.model).slowest_kts;
    std::vector<double> speeds;
    for (double kts = low_kts; kts < e.start_airspeed_kts; kts += 5.0) {
        speeds.push_back(kts);
    }
    speeds.push_back(e.start_airspeed_kts);
    std::string failures;
    std::size_t flown = 0;
    std::string worst;
    double worst_beta = 0.0;
    for (const double kts : speeds) {
        const glideslope::sim::CrosswindFlown f =
            glideslope::sim::fly_heading_in_crosswind(data(), e, kts, true);
        ++flown;
        const double beta = std::max(-f.least_sideslip_deg, f.most_sideslip_deg);
        char line[200];
        std::snprintf(line, sizeof line, "%s at %.0f kt: sideslip %+.2f to %+.2f, heading within %.2f",
                      e.id.c_str(), kts, f.least_sideslip_deg, f.most_sideslip_deg,
                      f.worst_heading_deg);
        std::printf("%s\n", line);
        if (!f.held()) {
            failures += std::string("\n  ") + line;
        }
        if (beta >= worst_beta) {
            worst_beta = beta;
            worst = line;
        }
    }
    std::printf("%s: %zu of %zu speeds from %.0f to %.0f kt flown; the most sideslip: %s\n",
                e.id.c_str(), flown, speeds.size(), low_kts, e.start_airspeed_kts, worst.c_str());
    check(flown == speeds.size() && flown > 0, "every speed flown");
    check(failures.empty(), "each holds its sideslip within a degree and its heading within two:" +
                                failures);
}

} // namespace

GLIDESLOPE_TEST(every_aircraft_has_its_own_test_of_a_heading_in_a_crosswind_at_every_speed_a_plan_may_fly_it) {
    std::vector<std::string> catalogue;
    for (const CatalogueEntry& e : glideslope::sim::read_catalogue(data())) {
        catalogue.push_back(e.id);
    }
    std::vector<std::string> tested = crosswind_swept;
    std::sort(catalogue.begin(), catalogue.end());
    std::sort(tested.begin(), tested.end());
    check(tested == catalogue && std::adjacent_find(tested.begin(), tested.end()) == tested.end(),
          "each of the " + std::to_string(catalogue.size()) +
              " aircraft has its own test, once: " + std::to_string(tested.size()));
}

GLIDESLOPE_TEST(the_c172p_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("c172p");
}

GLIDESLOPE_TEST(the_c182_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("c182");
}

GLIDESLOPE_TEST(the_pa28_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("pa28");
}

GLIDESLOPE_TEST(the_j3cub_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("j3cub");
}

GLIDESLOPE_TEST(the_short_s23_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("short_s23");
}

GLIDESLOPE_TEST(the_mosquito_fb6_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("mosquito-fb6");
}

GLIDESLOPE_TEST(the_boeing_737_300_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("737-300");
}

GLIDESLOPE_TEST(the_boeing_747_400_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("747-400");
}

GLIDESLOPE_TEST(the_boeing_787_8_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("787-8");
}

GLIDESLOPE_TEST(the_a320_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("a320");
}

GLIDESLOPE_TEST(the_a380_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("a380");
}

GLIDESLOPE_TEST(the_learjet35a_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("learjet35a");
}

GLIDESLOPE_TEST(the_b2_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("b2");
}

GLIDESLOPE_TEST(the_f15c_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("f15c");
}

GLIDESLOPE_TEST(the_f22_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("f22");
}

GLIDESLOPE_TEST(the_f35b_holds_a_heading_in_a_20_kt_crosswind_at_every_speed_a_plan_may_fly_it) {
    holds_a_heading_at_every_plan_speed("f35b");
}

// **Every aircraft holds a heading in a 20 kt crosswind from her approach
// speed up to the slowest a plan flies her clean, flown as an approach is**:
// gear down, her own landing flap and approach speedbrake
// (sim::approach_speeds, sim::TrialConfiguration::landing), at the loading
// her approach speed is for, at 3,000 ft on the autopilot
// (sim::fly_heading_in_crosswind), from her approach speed in 5 kt steps to
// her plan floor, both ends flown. **The landing flap is flown no faster than
// her approach speed and 40 kt** - our choice, a conservative bound on the
// speeds a landing flap is flown at, as nothing in the data gives a flap's
// limit speed; between that and her plan floor (the A380's 181 to 196 kt
// alone) she is flown with half her landing flap, an intermediate setting,
// also our choice. Above the floor she is flown clean by the tests before
// these. Each speed must hold her sideslip within a degree after 30 s, her
// heading within two from 40 s, and her height within 300 ft throughout.
// The 747-400 and F-22A publish no approach speed
// (sim::publishes_approach_speed) and are named; every other aircraft has its
// own test, asserted below. **Seen to fail** flown clean at her model's own
// weight, as the plan-speed sweep flies, from her approach speed: the 737-300,
// 787-8, A380, B-2A and F-35B red - the B-2A leaving her tables at 124 to 139
// kt and swinging 53 degrees at 144, the F-35B 3.1 degrees at 159 and 3,000 ft
// lost (PROJECT_STATUS.md, 2026-10-10).
namespace {

const std::map<std::string, std::string> no_approach_speed = {
    {"747-400", "its measured stalls would not hold still, so it publishes no approach speed"},
    {"f22", "its measured stalls would not hold still, so it publishes no approach speed"},
};

constexpr double landing_flap_above_vref_kts = 40.0;
// **Her heading within two degrees by 40 s**, judged apart from her yaw:
// with full flap above about 165 kt the 787-8 and the A380, their sideslip
// held, are rolled by it 6 degrees off and the heading loop overshoots back
// through north, 2.0 to 2.9 degrees off at 30 s. The latest any speed
// settles is 32.4 s, the 787-8 at 188 kt (PROJECT_STATUS.md, 2026-10-10).
constexpr double heading_settled_by_s = 40.0;
constexpr double most_height_lost_ft = 300.0;

// **Named, not judged: the F-35B to 179 kt.** Her model needs 15 to 20
// degrees of incidence to fly level there - the approach incidence of the
// open tail "The F-35B lands on her power" - and the autopilot's nose stops
// at 15 degrees, so she sinks at up to 1,300 ft/min: 2,836 ft lost at 159
// kt, 301 at 179, 270 at 184. Her sideslip stays within 0.01.
const std::map<std::string, std::pair<double, std::string>> approach_named = {
    {"f35b", {181.0, "to 179 kt her model's approach incidence, 18 to 20 degrees, is past the "
                     "autopilot's 15 degrees of nose, and she cannot hold her height"}},
};

const std::vector<std::string> approach_swept = {
    "c172p", "c182", "pa28",       "j3cub", "short_s23", "mosquito-fb6", "737-300",
    "787-8", "a320", "a380",       "learjet35a", "b2",   "f15c",         "f35b",
};

void holds_a_heading_from_its_approach_speed(const std::string& id) {
    const CatalogueEntry e = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::ApproachSpeeds approach =
        glideslope::sim::approach_speeds(data(), e.model);
    const double floor_kts = glideslope::sim::plan_speeds(data(), e.model).slowest_kts;
    const auto landing = glideslope::sim::TrialConfiguration::landing(approach);
    auto intermediate = landing;
    intermediate.flaps = 0.5 * landing.flaps;
    const double landing_flap_to_kts = approach.vref_kts + landing_flap_above_vref_kts;
    std::vector<double> speeds;
    for (double kts = approach.vref_kts; kts < floor_kts - 0.5; kts += 5.0) { // the floor once
        speeds.push_back(kts);
    }
    speeds.push_back(std::max(floor_kts, approach.vref_kts));
    const auto named = approach_named.find(id);
    std::string failures;
    std::size_t flown = 0;
    std::size_t not_judged = 0;
    std::string worst;
    double worst_beta = 0.0;
    double latest_settled_s = 0.0;
    double most_lost_ft = 0.0;
    for (const double kts : speeds) {
        const auto& configuration = kts <= landing_flap_to_kts + 0.01 ? landing : intermediate;
        const glideslope::sim::CrosswindFlown f = glideslope::sim::fly_heading_in_crosswind(
            data(), e, kts, true, 30.0, configuration, 0.0);
        const double beta = std::max(-f.least_sideslip_deg, f.most_sideslip_deg);
        char line[300];
        std::snprintf(line, sizeof line,
                      "%s at %.0f kt, gear down, flap %.2f, speedbrake %.2f: sideslip %+.2f to "
                      "%+.2f, heading within 2 from %.1f s (%.2f after 30 s), height within "
                      "%.0f ft%s",
                      e.id.c_str(), kts, configuration.flaps, configuration.speedbrake,
                      f.least_sideslip_deg, f.most_sideslip_deg, f.heading_settled_s,
                      f.worst_heading_deg, f.worst_height_ft,
                      f.left_tables ? ", left its tables" : "");
        std::printf("%s\n", line);
        if (named != approach_named.end() && kts < named->second.first) {
            std::printf("  named, not judged: %s\n", named->second.second.c_str());
            ++not_judged;
            continue;
        }
        ++flown;
        latest_settled_s = std::max(latest_settled_s, f.heading_settled_s);
        most_lost_ft = std::max(most_lost_ft, f.worst_height_ft);
        if (!f.held_heading_by(heading_settled_by_s) || f.worst_height_ft > most_height_lost_ft) {
            failures += std::string("\n  ") + line;
        }
        if (f.left_tables || beta >= worst_beta) {
            worst_beta = f.left_tables ? 1e9 : beta;
            worst = line;
        }
    }
    std::printf("%s: %zu speeds from %.0f to %.0f kt, %zu judged and %zu named; landing flap to "
                "%.0f kt; heading settled by %.1f s at the latest, height within %.0f ft; the "
                "most sideslip: %s\n",
                e.id.c_str(), speeds.size(), approach.vref_kts, speeds.back(), flown, not_judged,
                landing_flap_to_kts, latest_settled_s, most_lost_ft, worst.c_str());
    check(flown + not_judged == speeds.size() && flown > 0,
          "every speed flown, and judged or named");
    check(failures.empty(), "each holds its sideslip within a degree after 30 s, its heading "
                            "within two from 40 s and its height within 300 ft:" +
                                failures);
}

} // namespace

GLIDESLOPE_TEST(every_aircraft_with_an_approach_speed_has_its_own_test_of_a_heading_in_a_crosswind_from_it) {
    std::vector<std::string> with;
    std::size_t without = 0;
    for (const CatalogueEntry& e : glideslope::sim::read_catalogue(data())) {
        if (glideslope::sim::publishes_approach_speed(data(), e.model)) {
            with.push_back(e.id);
        } else {
            check(no_approach_speed.count(e.id) == 1,
                  e.id + " publishes no approach speed and is named as such");
            ++without;
        }
    }
    std::vector<std::string> tested = approach_swept;
    std::sort(with.begin(), with.end());
    std::sort(tested.begin(), tested.end());
    std::printf("%zu aircraft: %zu tested from their approach speeds, %zu named without one\n",
                with.size() + without, tested.size(), without);
    check(tested == with && std::adjacent_find(tested.begin(), tested.end()) == tested.end(),
          "each of the " + std::to_string(with.size()) +
              " with an approach speed has its own test, once: " +
              std::to_string(tested.size()));
    check(without == no_approach_speed.size(), "every aircraft named without one is without one");
}

GLIDESLOPE_TEST(the_c172p_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("c172p");
}

GLIDESLOPE_TEST(the_c182_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("c182");
}

GLIDESLOPE_TEST(the_pa28_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("pa28");
}

GLIDESLOPE_TEST(the_j3cub_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("j3cub");
}

GLIDESLOPE_TEST(the_short_s23_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("short_s23");
}

GLIDESLOPE_TEST(the_mosquito_fb6_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("mosquito-fb6");
}

GLIDESLOPE_TEST(the_boeing_737_300_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("737-300");
}

GLIDESLOPE_TEST(the_boeing_787_8_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("787-8");
}

GLIDESLOPE_TEST(the_a320_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("a320");
}

GLIDESLOPE_TEST(the_a380_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("a380");
}

GLIDESLOPE_TEST(the_learjet35a_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("learjet35a");
}

GLIDESLOPE_TEST(the_b2_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("b2");
}

GLIDESLOPE_TEST(the_f15c_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("f15c");
}

GLIDESLOPE_TEST(the_f35b_holds_a_heading_in_a_20_kt_crosswind_in_its_landing_configuration_from_its_approach_speed) {
    holds_a_heading_from_its_approach_speed("f35b");
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
// speed, at 1,500 ft/min: with no floor for a flapped aeroplane, the throttle
// is the speed's again and comes off its stop - within two minutes, as the
// throttle's integral unwinds from its stop: 18 to 32 s at the leaned
// ceilings, 2026-10-01. A hold left over from before the flaps went out once
// kept the throttle at its stop for good, and put back it turns this red. The Cub has no flaps and fixed gear, so it
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
        const NearTheCeiling ceiling = near_the_ceiling(e, climb_kts);
        const double ceiling_ft = ceiling.ft;
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
        controls.mixture = ceiling.mixture;
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
        // **Down steeply enough that full throttle is more than it needs**:
        // at the 700 ft/min the autopilot descends at unasked, the Cherokee
        // with its flaps out near its leaned ceiling, about 20,200 ft, needs
        // all its throttle to hold the speed, and the throttle stays at its
        // stop for that reason and not a hold left over.
        modes.vertical_speed_fpm = 1500.0;
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

namespace {

// **The upset rule, alone**: the Cessna rolled with full left aileron past
// 65 degrees of bank at 6,000 ft, held there with its nose let fall past 20
// degrees down, then
// handed to the AI - `on_elevator`, the airspeed held on the elevator (an
// approach, a glide, a stall recovery), or not (the plain holds). What is
// measured from the hand-over until the wings are within 45 degrees.
struct Upset {
    double bank_at_handover_deg = 0.0;
    double pitch_at_handover_deg = 0.0;
    double to_45_s = -1.0;
    double most_pitch_rise_deg = 0.0; // the pitch asked, above where it was
    double trim_moved = 0.0;          // the elevator's trim, while past 45
    double most_control_step = 0.0;   // aileron, elevator, rudder, a step
    double bank_after_20_s = 0.0;
};

Upset handed_an_upset(bool on_elevator) {
    Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 6000.0;
    ic.heading_deg = 0.0;
    ic.airspeed_kts = 110.0;
    ic.gear = 0.0;
    ic.engine_running = true;
    aircraft.initialize(ic);
    glideslope::sim::Controls pilot;
    pilot.throttle = 0.7;
    pilot.aileron = -1.0;
    int rolled = 0;
    while (aircraft.property("attitude/phi-deg") > -65.0 && rolled < 30 * steps_per_second) {
        aircraft.set_controls(pilot);
        aircraft.step();
        ++rolled;
    }
    // Held there, the nose let fall past 20 degrees down.
    pilot.aileron = 0.0;
    for (int i = 0; i < 20 * steps_per_second && aircraft.property("attitude/theta-deg") > -20.0;
         ++i) {
        pilot.aileron = aircraft.property("attitude/phi-deg") > -70.0 ? -0.3 : 0.0;
        aircraft.set_controls(pilot);
        aircraft.step();
    }
    Upset out;
    out.bank_at_handover_deg = aircraft.property("attitude/phi-deg");
    out.pitch_at_handover_deg = aircraft.property("attitude/theta-deg");
    glideslope::sim::Controller controller(aircraft, pilot);
    controller.to_ai();
    AutopilotModes m = controller.autopilot()->modes();
    // On the elevator, a speed slower than hers - a glide's, an approach's -
    // so that its law would raise the nose at once.
    m.airspeed_kts = on_elevator ? 60.0 : 100.0;
    m.speed_on_elevator = on_elevator;
    controller.autopilot()->set(m);
    const double pitch0 = controller.autopilot()->pitch_asked_deg();
    const double trim0 = controller.autopilot()->elevator_trim();
    glideslope::sim::Controls last = pilot;
    for (int step = 0; step < 20 * steps_per_second; ++step) {
        const glideslope::sim::Controls c = controller.fly();
        out.most_control_step =
            std::max({out.most_control_step, std::abs(c.aileron - last.aileron),
                      std::abs(c.elevator - last.elevator), std::abs(c.rudder - last.rudder)});
        last = c;
        aircraft.set_controls(c);
        aircraft.step();
        if (out.to_45_s < 0.0) {
            out.most_pitch_rise_deg = std::max(
                out.most_pitch_rise_deg, controller.autopilot()->pitch_asked_deg() - pitch0);
            out.trim_moved =
                std::max(out.trim_moved, std::abs(controller.autopilot()->elevator_trim() - trim0));
            if (std::abs(aircraft.property("attitude/phi-deg")) <= 45.0) {
                out.to_45_s = static_cast<double>(step + 1) / steps_per_second;
            }
        }
    }
    out.bank_after_20_s = aircraft.property("attitude/phi-deg");
    return out;
}

} // namespace

// **Handed an upset, the AI rolls level before it pulls** - unload, roll,
// then pull, AC 120-111's nose-low recovery, past its 45 degrees of bank -
// in both of the autopilot's pitch laws: the bank comes back at about the
// upset's 15 degrees a second, the pitch asked does not rise and the trim
// does not wind until the wings are within 45 degrees, and no control moves
// faster than a hand (full travel in a second: 1/120 a step).
GLIDESLOPE_TEST(an_autopilot_handed_a_spiral_rolls_its_wings_level_before_it_raises_the_nose_in_both_its_pitch_laws) {
    int flown = 0;
    for (const bool on_elevator : {false, true}) {
        const Upset u = handed_an_upset(on_elevator);
        const std::string law = on_elevator ? "the airspeed on the elevator" : "the plain holds";
        const double rate =
            u.to_45_s > 0.0 ? (std::abs(u.bank_at_handover_deg) - 45.0) / u.to_45_s : 0.0;
        std::fprintf(stderr,
                     "%s: handed over at %.0f deg of bank, %.0f pitch; within 45 deg in %.2f s "
                     "(%.1f deg/s); pitch asked rose %.2f deg, trim moved %.4f, meanwhile; most "
                     "control step %.5f; bank after 20 s %.1f\n",
                     law.c_str(), u.bank_at_handover_deg, u.pitch_at_handover_deg, u.to_45_s, rate,
                     u.most_pitch_rise_deg, u.trim_moved, u.most_control_step, u.bank_after_20_s);
        check(u.bank_at_handover_deg <= -65.0 && u.pitch_at_handover_deg < -10.0,
              law + ": handed over banked past 65 degrees, nose down - the upset is built");
        check(u.to_45_s > 0.0 && rate >= 10.0 && rate <= 20.0,
              law + ": the bank comes back at about 15 degrees a second");
        check(u.most_pitch_rise_deg <= 1e-9, law + ": the nose is not raised past 45 degrees");
        check(u.trim_moved <= 1e-12, law + ": the trim is not wound past 45 degrees");
        check(u.most_control_step <= 1.0 / steps_per_second + 1e-9,
              law + ": no control moves faster than a hand");
        check(std::abs(u.bank_after_20_s) < 30.0, law + ": recovered within 20 s");
        ++flown;
    }
    check(flown == 2, "both pitch laws were flown");
}

namespace {

// What holding a speed in rough air costs: the airspeed's spread, its RMS
// off the speed asked, and how often the throttle turns back.
struct RoughSpeedHold {
    double spread_kts = 0.0;
    double rms_kts = 0.0;
    int reversals = 0;
    double travel = 0.0;
};

// `id` at its catalogue start speed and throttle, at 4,000 ft heading north
// in moderate turbulence (MIL-F-8785C severity 3), held on the autopilot at
// that speed and height: a minute to settle, then three measured. A reversal
// is the throttle turning back after moving at least 0.02 of its travel one
// way, so a hand's jitter is not counted.
RoughSpeedHold hold_speed_in_moderate_turbulence(const std::string& id) {
    const CatalogueEntry e = glideslope::sim::find_aircraft(data(), id);
    Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 4000.0;
    ic.heading_deg = 0.0;
    ic.airspeed_kts = e.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    aircraft.initialize(ic);
    glideslope::world::WeatherReport report;
    report.surface.metar =
        glideslope::world::parse_metar("XXXX 181200Z 00000KT 9999 SKC 15/05 Q1013");
    report.surface.latitude_deg = -33.9;
    report.surface.longitude_deg = 151.2;
    report.turbulence_severity = 3;
    report.air_seed = 0xa170;
    aircraft.set_weather(
        std::make_shared<glideslope::world::ReportedWeather>(report, nullptr, 0.0));
    glideslope::sim::Controls controls;
    controls.throttle = e.start_throttle;
    Autopilot autopilot(aircraft, controls);
    AutopilotModes modes = autopilot.modes();
    modes.heading_deg = 0.0;
    modes.altitude_ft = 4000.0;
    modes.airspeed_kts = e.start_airspeed_kts;
    autopilot.set(modes);
    RoughSpeedHold out;
    double lowest = 1e9;
    double highest = 0.0;
    double sum_sq = 0.0;
    int measured = 0;
    double last_throttle = controls.throttle;
    double extreme = controls.throttle;
    int direction = 0;
    for (int i = 0; i < 240 * steps_per_second; ++i) {
        const glideslope::sim::Controls c = autopilot.fly();
        aircraft.set_controls(c);
        aircraft.step();
        if (i < 60 * steps_per_second) {
            last_throttle = c.throttle;
            extreme = c.throttle;
            continue;
        }
        const double kts = aircraft.property("velocities/vc-kts");
        lowest = std::min(lowest, kts);
        highest = std::max(highest, kts);
        sum_sq += (kts - e.start_airspeed_kts) * (kts - e.start_airspeed_kts);
        ++measured;
        out.travel += std::abs(c.throttle - last_throttle);
        last_throttle = c.throttle;
        if (direction >= 0 && c.throttle > extreme) {
            extreme = c.throttle;
            direction = 1;
        } else if (direction <= 0 && c.throttle < extreme) {
            extreme = c.throttle;
            direction = -1;
        } else if (std::abs(c.throttle - extreme) >= 0.02) {
            ++out.reversals;
            direction = -direction;
            extreme = c.throttle;
        }
    }
    out.spread_kts = highest - lowest;
    out.rms_kts = std::sqrt(sum_sq / measured);
    return out;
}

} // namespace

// **The throttle holds a speed in rough air without reading the speed's
// trend.** Five classes, held at their start speeds at 4,000 ft in moderate
// turbulence for three minutes (hold_speed_in_moderate_turbulence): the
// slow-spooling 747-400 and A380, the business jet, the Mosquito and the
// 172P. Each one's RMS off the speed asked is held to 5% over what it
// measured on 2026-10-10 (linux-release), and its throttle reversals to 10%
// over. **A lead on the speed's trend was tried for the take-off's hand-over
// and taken out** (PROJECT_STATUS, 2026-10-10): reading the speed two
// seconds ahead, the RMS went 747-400 3.35 -> 3.61, A380 3.81 -> 4.27,
// Learjet 3.21 -> 3.63, Mosquito 6.14 -> 8.18, 172P 2.99 -> 3.23, with
// 10 to 25% fewer reversals; the turbulence's own trend is noise to it.
GLIDESLOPE_TEST(the_autopilot_holds_five_classes_speeds_in_moderate_turbulence_to_their_measured_spread) {
    struct Measured {
        const char* id;
        double rms_kts;
        int reversals;
    };
    const Measured measured[] = {{"747-400", 3.35, 210},
                                 {"a380", 3.81, 200},
                                 {"learjet35a", 3.21, 210},
                                 {"mosquito-fb6", 6.14, 156},
                                 {"c172p", 2.99, 221}};
    std::size_t flown = 0;
    for (const Measured& m : measured) {
        const RoughSpeedHold h = hold_speed_in_moderate_turbulence(m.id);
        std::fprintf(stderr,
                     "%s: speed spread %.2f kt, RMS %.2f kt; throttle reversals %d, travel %.2f\n",
                     m.id, h.spread_kts, h.rms_kts, h.reversals, h.travel);
        check(h.rms_kts <= 1.05 * m.rms_kts,
              std::string(m.id) + " held its speed to an RMS of " + std::to_string(h.rms_kts) +
                  " kt (at most " + std::to_string(1.05 * m.rms_kts) + ")");
        check(h.reversals <= static_cast<int>(1.1 * m.reversals),
              std::string(m.id) + " reversed its throttle " + std::to_string(h.reversals) +
                  " times (at most " + std::to_string(static_cast<int>(1.1 * m.reversals)) + ")");
        ++flown;
    }
    check(flown == 5, "five classes flown: " + std::to_string(flown));
}

namespace {

// What engaging the autopilot to hold height in a descent does: the most
// load off 1 g, the most any control moved in a step, and how far she sank
// below the height she was engaged at.
struct LevelOff {
    double most_g_off = 0.0;
    double fastest_control = 0.0;
    double sank_ft = 0.0;
    double descent_fpm = 0.0;
};

// `id` at its catalogue start speed at 6,000 ft, flown down at 1,000 ft/min
// by one autopilot for 40 s, then handed to a new one engaged as she is -
// holding her height, the autopilot's own on engaging - and flown 30 s.
LevelOff level_off_from_a_descent(const std::string& id) {
    const CatalogueEntry e = glideslope::sim::find_aircraft(data(), id);
    Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 6000.0;
    ic.heading_deg = 0.0;
    ic.airspeed_kts = e.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    aircraft.initialize(ic);
    glideslope::sim::Controls controls;
    controls.throttle = e.start_throttle;
    std::optional<Autopilot> autopilot(std::in_place, aircraft, controls);
    AutopilotModes modes = autopilot->modes();
    modes.heading_deg = 0.0;
    modes.altitude_ft.reset();
    modes.vertical_speed_fpm = -1000.0;
    modes.airspeed_kts = e.start_airspeed_kts;
    autopilot->set(modes);
    for (int i = 0; i < 40 * steps_per_second; ++i) {
        controls = autopilot->fly();
        aircraft.set_controls(controls);
        aircraft.step();
    }
    LevelOff out;
    out.descent_fpm = aircraft.property("velocities/h-dot-fps") * 60.0;
    const double engaged_ft = aircraft.property("position/h-sl-ft");
    autopilot.emplace(aircraft, controls);
    glideslope::sim::Controls last = controls;
    for (int i = 0; i < 30 * steps_per_second; ++i) {
        const glideslope::sim::Controls c = autopilot->fly();
        out.fastest_control = std::max(
            {out.fastest_control, std::abs(c.elevator - last.elevator),
             std::abs(c.aileron - last.aileron), std::abs(c.rudder - last.rudder),
             std::abs(c.throttle - last.throttle)});
        last = c;
        aircraft.set_controls(c);
        aircraft.step();
        out.most_g_off =
            std::max(out.most_g_off, std::abs(aircraft.property("accelerations/Nz") - 1.0));
        out.sank_ft = std::max(out.sank_ft, engaged_ft - aircraft.property("position/h-sl-ft"));
    }
    return out;
}

} // namespace

// **Engaged in a descent, the autopilot levels her off within half a g, no
// control faster than a hand.** Its climb loop starts from the pitch she
// has whatever climb is asked first (sim/autopilot.cpp); engaged holding her
// height while coming down at 1,000 ft/min, that is the seed it always had,
// and the nose comes up as the integral winds. Half a g is AC 25.1329-1C's
// most for a pilot's recovery to a normal flight path (chapter 8, page 78:
// "an incremental normal acceleration in the order of 0.5 g is considered
// the maximum for this type of maneuver"), the nearest stated figure; a hand
// is 1/120 of a control's travel a step. Measured (2026-10-10), g off 1 /
// sank below: the 172P 0.12 / 56 ft; the 737-300 0.29 / 38; the 747-400
// 0.22 / 49. A seed that started from her pitch whatever was asked, tried
// on the way, levelled them off firmer - 0.18, 0.39, 0.27 g - and failed to
// recover a stalled 737-300 engaged at 36 degrees of alpha.
GLIDESLOPE_TEST(the_autopilot_engaged_in_a_descent_levels_off_within_half_a_g_at_a_hands_pace) {
    std::size_t flown = 0;
    for (const std::string id : {"c172p", "737-300", "747-400"}) {
        const LevelOff l = level_off_from_a_descent(id);
        std::fprintf(stderr,
                     "%s: engaged descending %.0f ft/min; at most %.3f g off 1, a control "
                     "moving at most %.5f a step, sinking %.0f ft below\n",
                     id.c_str(), l.descent_fpm, l.most_g_off, l.fastest_control, l.sank_ft);
        check(l.descent_fpm <= -900.0,
              id + " was engaged coming down at " + std::to_string(l.descent_fpm) + " ft/min");
        check(l.most_g_off <= 0.5, id + " levelled off at " + std::to_string(l.most_g_off) +
                                       " g off 1 (at most 0.5)");
        check(l.fastest_control <= 1.0 / 120.0 + 1e-9,
              id + " moved a control " + std::to_string(l.fastest_control) +
                  " in a step (at most 1/120)");
        ++flown;
    }
    check(flown == 3, "a light aeroplane and two jets flown: " + std::to_string(flown));
}

namespace {

// One flight for the trim's stall rule: `e` at 4,000 ft in moderate
// turbulence, on the autopilot, either on an approach - her landing flap and
// gear down, at her approach speed, descending at 700 ft/min - or in a climb
// at her best-climb speed, clean, at 1,500 ft/min asked; ninety seconds.
struct TrimFlown {
    long held_steps = 0;
    double mean_pitch_off_deg = 0.0;
    double slowest_kts = 1e9;
    double most_alpha_deg = -1e9;
    double flown_kts = 0.0;
};

TrimFlown fly_for_the_trim(const CatalogueEntry& e, bool approach) {
    Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, e.model);
    const std::optional<glideslope::sim::ApproachSpeeds> lands =
        glideslope::sim::landing_speeds(data(), e.model);
    const glideslope::sim::DepartureSpeeds departs =
        glideslope::sim::departure_speeds(data(), e.model);
    const double kts = approach ? lands->vref_kts : departs.climb_kts;
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 4000.0;
    ic.heading_deg = 0.0;
    ic.airspeed_kts = kts;
    ic.engine_running = true;
    ic.gear = approach ? 1.0 : 0.0;
    aircraft.initialize(ic);
    glideslope::world::WeatherReport report;
    report.surface.metar =
        glideslope::world::parse_metar("XXXX 181200Z 00000KT 9999 SKC 15/05 Q1013");
    report.surface.latitude_deg = -33.9;
    report.surface.longitude_deg = 151.2;
    report.turbulence_severity = 3; // moderate
    report.air_seed = 0xa170;
    aircraft.set_weather(
        std::make_shared<glideslope::world::ReportedWeather>(report, nullptr, 0.0));
    glideslope::sim::Controls controls;
    controls.throttle = 0.7;
    controls.flaps = approach ? lands->flap : 0.0;
    controls.gear = approach ? 1.0 : 0.0;
    controls.speedbrake = approach ? lands->speedbrake : 0.0;
    Autopilot autopilot(aircraft, controls);
    AutopilotModes modes = autopilot.modes();
    modes.heading_deg = 0.0;
    modes.airspeed_kts = kts;
    modes.altitude_ft = approach ? 2000.0 : 9000.0;
    modes.vertical_speed_fpm = approach ? 700.0 : 1500.0;
    autopilot.set(modes);
    TrimFlown out;
    out.flown_kts = kts;
    double off_sum = 0.0;
    const int steps = 90 * steps_per_second;
    for (int i = 0; i < steps; ++i) {
        aircraft.set_controls(autopilot.fly());
        aircraft.step();
        off_sum +=
            std::abs(autopilot.pitch_asked_deg() - aircraft.property("attitude/theta-deg"));
        out.slowest_kts = std::min(out.slowest_kts, aircraft.property("velocities/vc-kts"));
        out.most_alpha_deg = std::max(out.most_alpha_deg, aircraft.property("aero/alpha-deg"));
    }
    out.held_steps = autopilot.trim_held_steps();
    out.mean_pitch_off_deg = off_sum / steps;
    return out;
}

} // namespace

// **The trim is held from winding nose-up only by a wing seen to stall, never
// in ordinary flight** (sim/autopilot.cpp). One aeroplane of each class, the
// first in the roster that publishes how it lands, on an approach and in a
// best-rate climb, each in moderate turbulence for ninety seconds: the rule
// never engages, and the nose follows the pitch asked within 4 degrees on
// average (the 737-300's approach, the worst, 3.3); the B-2A's approach is
// named, below. Read off the angle alone, past a peak a gust had set low, it could
// refuse the trim in just such flight. Coverage: every class flown.
GLIDESLOPE_TEST(the_trim_is_never_held_for_a_stall_on_approaches_and_climbs_in_moderate_turbulence) {
    const auto roster = glideslope::sim::read_catalogue(data());
    std::set<glideslope::sim::AircraftClass> flown_classes;
    std::vector<std::string> faults;
    std::size_t flights = 0;
    std::size_t left_out = 0;
    for (const CatalogueEntry& e : roster) {
        if (flown_classes.count(e.aircraft_class) != 0 ||
            !glideslope::sim::landing_speeds(data(), e.model)) {
            continue;
        }
        flown_classes.insert(e.aircraft_class);
        for (const bool approach : {true, false}) {
            if (approach && e.id == "b2") {
                // **Left out, named**: at her approach speed on the plain
                // autopilot in moderate turbulence the B-2A departs - 177
                // degrees of alpha - with the rule or without it; her
                // approaches are the approach autopilot's (sim/lander.hpp).
                std::printf("  %-13s approach left out: departs on the plain autopilot\n",
                            e.id.c_str());
                ++left_out;
                continue;
            }
            const TrimFlown f = fly_for_the_trim(e, approach);
            ++flights;
            const char* what = approach ? "approach" : "climb";
            std::printf("  %-13s %-8s at %5.1f kt: trim held %ld steps, pitch off %.2f deg on "
                        "average, slowest %5.1f kt, most alpha %4.1f\n",
                        e.id.c_str(), what, f.flown_kts, f.held_steps, f.mean_pitch_off_deg,
                        f.slowest_kts, f.most_alpha_deg);
            if (f.held_steps != 0) {
                faults.push_back(e.id + " " + what + ": the trim was held for a stall " +
                                 std::to_string(f.held_steps) + " steps");
            }
            if (f.mean_pitch_off_deg > 4.0) {
                faults.push_back(e.id + " " + what + ": the nose was " +
                                 std::to_string(f.mean_pitch_off_deg) +
                                 " deg off the pitch asked on average");
            }
        }
    }
    for (const std::string& fault : faults) {
        std::printf("  %s\n", fault.c_str());
    }
    check(faults.empty(), std::to_string(faults.size()) + " flights failed; the first: " +
                              (faults.empty() ? "" : faults.front()));
    check(flights + left_out == 2 * flown_classes.size() && left_out == 1,
          "every flight flown or named: " + std::to_string(flights) + " flown, " +
              std::to_string(left_out) + " named");
    check(flown_classes.size() == glideslope::sim::aircraft_class_count,
          "one aeroplane of every class flown: " + std::to_string(flown_classes.size()) +
              " of " + std::to_string(glideslope::sim::aircraft_class_count));
}
