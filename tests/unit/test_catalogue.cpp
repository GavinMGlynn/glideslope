#include "harness.hpp"

#include "frontend/held_levers.hpp"
#include "net/inputs.hpp"
#include "net/state.hpp"
#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/catalogue.hpp"
#include "sim/learnt.hpp"
#include "sim/orbit_trial.hpp"
#include "sim/terrain.hpp"
#include "sim/test_pilot.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using glideslope::sim::CatalogueEntry;
using glideslope::sim::CatalogueError;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;

// The data directory the programs read: the one the flight models are in.
std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// Flown from its catalogue start at 3,000 ft for a minute, the autopilot
// holding it: where it ends.
struct Held {
    double altitude_ft;
    double airspeed_kts;
};
Held hold_its_start(const std::filesystem::path& from, const CatalogueEntry& e) {
    glideslope::sim::Aircraft aircraft(from / "jsbsim", e.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 3000.0;
    ic.airspeed_kts = e.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    aircraft.initialize(ic);
    glideslope::sim::Controls controls;
    controls.throttle = e.start_throttle;
    controls.gear = 0.0;
    glideslope::sim::Autopilot autopilot(aircraft, controls);
    glideslope::sim::AutopilotModes modes = autopilot.modes();
    modes.altitude_ft = 3000.0;
    modes.airspeed_kts = e.start_airspeed_kts;
    autopilot.set(modes);
    for (int i = 0; i < 60 * steps_per_second; ++i) {
        aircraft.set_controls(autopilot.fly());
        aircraft.step();
    }
    return {aircraft.property("position/h-sl-ft"),
            aircraft.property("velocities/vc-kts")};
}

// Standing on a runway at sea level: the brakes off and the throttle opened
// over three seconds, steering by rudder and, below 60 knots, differential
// brake, as the figures' take-offs do - a tail-wheel aircraft's tail wheel
// castors, and the Mosquito swings as its Pilot's Notes warn; the stick
// neutral until, from six tenths of its catalogue airspeed - a speed well
// above any aircraft's stall and below its start's - the nose is raised to ten
// degrees. The height above the runway, in feet, after two minutes, or once
// it passes 200 ft.
//
// A seaplane takes off from open water instead, settled afloat first; its
// hull is held at a running attitude of eight degrees from the start, as a
// flying boat's is, and raised to ten.
double take_off(const std::filesystem::path& from, const CatalogueEntry& e) {
    glideslope::sim::Aircraft aircraft(from / "jsbsim", e.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 0.0;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = 0.0;
    ic.engine_running = true;
    ic.gear = 1.0;
    if (e.seaplane) {
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return true; }));
        ic.altitude_ft = 6.0;
    }
    aircraft.initialize(ic);
    if (e.seaplane) {
        const glideslope::sim::Controls idle;
        for (int i = 0; i < 30 * steps_per_second; ++i) {
            aircraft.set_controls(idle);
            aircraft.step();
        }
    }
    glideslope::sim::TestPilot pilot(aircraft);
    std::optional<glideslope::sim::TestPilot> rotating;
    glideslope::sim::Controls c;
    c.gear = 1.0;
    const double heading = aircraft.property("attitude/psi-deg");
    const double standing = aircraft.property("position/h-agl-ft");
    double pitch = 0.0;
    for (int i = 0; i < 120 * steps_per_second; ++i) {
        const double kcas = aircraft.property("velocities/vc-kts");
        c.throttle = std::min(i / (3.0 * steps_per_second), 1.0);
        c.rudder = pilot.steer_to(heading);
        const double error = std::remainder(heading - aircraft.property("attitude/psi-deg"), 360.0);
        const double r_degps = aircraft.property("velocities/r-rad_sec") * 57.29578;
        const double turn = std::clamp(0.1 * error - 0.3 * r_degps, -1.0, 1.0);
        c.left_brake = kcas < 60.0 ? std::max(-turn, 0.0) : 0.0;
        c.right_brake = kcas < 60.0 ? std::max(turn, 0.0) : 0.0;
        if (e.seaplane && !rotating) {
            c.elevator = pilot.pitch_to(8.0);
            c.aileron = pilot.roll_to(0.0);
        }
        if (kcas >= 0.6 * e.start_airspeed_kts && !rotating) {
            rotating.emplace(aircraft);
            pitch = aircraft.property("attitude/theta-deg");
        }
        if (rotating) {
            pitch = std::min(pitch + 3.0 / steps_per_second, 10.0);
            c.elevator = rotating->pitch_to(pitch);
            c.aileron = rotating->roll_to(0.0);
        }
        aircraft.set_controls(c);
        aircraft.step();
        const double height = aircraft.property("position/h-agl-ft") - standing;
        if (height >= 200.0) {
            return height;
        }
    }
    return aircraft.property("position/h-agl-ft") - standing;
}

// Set down at idle on level ground, or on water: flown in from 30 ft at six
// tenths of its catalogue airspeed, as take_off rotates at, its gear down and
// its brakes off, the nose held two degrees up and the wings level, for one
// minute - three for a seaplane, which drifts to rest - or until it has been
// still for two seconds.
//
// A seaplane is flown in with its nose six degrees up, to alight on its step,
// and has touched when its hull is in the water; ten seconds later its engines
// are stopped, as it would be moored, and it drifts to rest.
struct Alighting {
    bool touched = false;            // a wheel took weight, it ditched or its hull met the water
    bool ditched = false;            // at the end
    bool wheels_took_weight = false; // any wheel, at any time
    double speed_kts = 0.0;          // over the surface, at the end
    double moved_ft = 0.0;           // from where it touched to where it ended
    double lowest_ft = 1e9;          // its centre of gravity above the surface, after touching
    bool finite = true;
};
Alighting alight(const std::filesystem::path& from, const CatalogueEntry& e, bool water) {
    glideslope::sim::Aircraft aircraft(from / "jsbsim", e.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [water](double, double) { return water; }));
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 30.0;
    ic.airspeed_kts = 0.6 * e.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 1.0;
    aircraft.initialize(ic);
    glideslope::sim::TestPilot pilot(aircraft);
    glideslope::sim::Controls c;
    c.gear = 1.0;
    c.throttle = 0.0;
    const int units = static_cast<int>(aircraft.property("gear/num-units"));
    const auto a_wheel_takes_weight = [&] {
        for (int i = 0; i < units; ++i) {
            try {
                if (aircraft.property("gear/unit[" + std::to_string(i) + "]/WOW") != 0.0) {
                    return true;
                }
            } catch (const std::out_of_range&) {
                // structure, which JSBSim names contact/unit[i]
            }
        }
        return false;
    };
    Alighting a;
    int still = 0;
    int since_touching = 0;
    double touched_lat = 0.0;
    double touched_lon = 0.0;
    // A boat drifts to rest more slowly than a wheel rolls to it.
    const int limit = (e.seaplane ? 180 : 60) * steps_per_second;
    for (int i = 0; i < limit && still < 2 * steps_per_second; ++i) {
        c.elevator = pilot.pitch_to(e.seaplane ? 6.0 : 2.0);
        c.aileron = pilot.roll_to(0.0);
        aircraft.set_controls(c);
        aircraft.step();
        const glideslope::sim::AircraftState state = aircraft.state();
        const bool wheels = a_wheel_takes_weight();
        a.wheels_took_weight = a.wheels_took_weight || wheels;
        const bool hull_in_water =
            e.seaplane && water && aircraft.property("hydro/active-norm") > 0.0;
        a.ditched = state.ditched;
        if (!a.touched && (wheels || state.ditched || hull_in_water)) {
            a.touched = true;
            touched_lat = state.latitude_deg;
            touched_lon = state.longitude_deg;
        }
        // Over the surface, JSBSim's ground speed lags a step behind a
        // ditching; the speed along the body's axes does not.
        const double speed_kts =
            std::hypot(state.u_fps, state.v_fps, state.w_fps) / 1.68781;
        if (!std::isfinite(speed_kts) || !std::isfinite(state.height_above_ground_ft)) {
            a.finite = false;
            break;
        }
        if (a.touched) {
            constexpr double feet_per_degree = 60.0 * 6076.12;
            a.moved_ft = std::hypot((state.latitude_deg - touched_lat) * feet_per_degree,
                                    (state.longitude_deg - touched_lon) * feet_per_degree *
                                        std::cos(touched_lat * 3.14159265358979 / 180.0));
            a.lowest_ft = std::min(a.lowest_ft, state.height_above_ground_ft);
            still = speed_kts < 0.5 ? still + 1 : 0;
            if (e.seaplane && ++since_touching == 10 * steps_per_second) {
                for (int engine = 0; engine < aircraft.figures().engines; ++engine) {
                    aircraft.fail_engine(engine, false);
                }
            }
        }
        a.speed_kts = speed_kts;
    }
    return a;
}

bool refused(const std::string& text, const std::string& says) {
    try {
        glideslope::sim::parse_catalogue_entry("test", text);
    } catch (const CatalogueError& e) {
        return std::string(e.what()).find(says) != std::string::npos;
    }
    return false;
}

} // namespace

GLIDESLOPE_TEST(every_aircraft_the_data_holds_loads_and_holds_its_start_in_the_air) {
    const std::vector<CatalogueEntry> catalogue =
        glideslope::sim::read_catalogue(data());
    check(!catalogue.empty(), "the data holds aircraft");
    for (const CatalogueEntry& e : catalogue) {
        const Held h = hold_its_start(data(), e);
        check(std::isfinite(h.altitude_ft) &&
                  std::abs(h.altitude_ft - 3000.0) < 100.0 &&
                  std::abs(h.airspeed_kts - e.start_airspeed_kts) < 5.0,
              e.id + " (" + e.name + ") held at 3,000 ft and " +
                  std::to_string(e.start_airspeed_kts) +
                  " KCAS for a minute: " + std::to_string(h.altitude_ft) + " ft, " +
                  std::to_string(h.airspeed_kts) + " KCAS");
    }
}

// **Every aircraft the data holds, started as the server starts a player's
// and left untouched, holds her height**: begun at 3,000 ft at her catalogue
// airspeed, her gear up and trimmed level, wings and all, and held thirty
// seconds on what the server holds until her player's first input
// (sim::held_until_moved), she is within 100 ft of where she began. Held at
// six tenths of her throttle with the stick neutral, as the server held her
// until 2026-10-20, an F-35B was in a dive; trimmed in pitch alone, a C172P
// rolled off on her engine's torque and lost 280 ft.
//
// **Three are left out, named, because JSBSim cannot trim them**: the
// F-22A, whose trim does not converge, and the Mosquito and the S.23, whose
// constant-speed propellers leave the throttle unable to trim her speed
// ("udot doesn't appear to be trimmable"). They are held as before, on six
// tenths of the throttle, and do not hold their height; that they are still
// untrimmable is checked, so that one which becomes trimmable is moved in.
//
// **And the C182 is left out, named, because she is trimmed before her
// propeller is governed**: her constant-speed propeller is started at its
// fine-pitch stop, the trim finds her power at 2,015 rpm, and her governor
// takes it to 2,400 in the first seconds - the thrust over, she climbs about
// 2 ft a second, 95 ft in thirty seconds and 210 in ninety, the same to the
// foot with fused multiply-adds and without. 95 of a 100 ft bound is a rule
// tested by luck, so she is not held to it; that she is trimmed and still
// climbs more than 50 ft is checked, so that she is moved in once her trim
// is found governed.
//
// Every aircraft is flown and counted, and every one that fails is named
// before the test fails.
GLIDESLOPE_TEST(every_aircraft_a_player_is_given_and_leaves_untouched_holds_her_start_height) {
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data());
    const std::vector<std::string> untrimmable = {"f22", "mosquito-fb6", "short_s23"};
    const std::string ungoverned = "c182";
    std::size_t held_height = 0;
    std::size_t left_out = 0;
    std::size_t left_out_ungoverned = 0;
    std::string failed;
    for (const CatalogueEntry& e : catalogue) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = 3000.0;
        ic.airspeed_kts = e.start_airspeed_kts;
        ic.engine_running = true;
        ic.gear = 0.0;
        ic.trim = true;
        ic.trim_lateral = true;
        aircraft.initialize(ic);
        const glideslope::sim::Controls held = glideslope::sim::held_until_moved(aircraft);
        double lowest_ft = 3000.0;
        double highest_ft = 3000.0;
        for (int i = 0; i < 30 * steps_per_second; ++i) {
            aircraft.set_controls(held);
            aircraft.step();
            const double ft = aircraft.property("position/h-sl-ft");
            lowest_ft = std::min(lowest_ft, ft);
            highest_ft = std::max(highest_ft, ft);
        }
        const bool named = std::find(untrimmable.begin(), untrimmable.end(), e.id) !=
                           untrimmable.end();
        std::printf("  %-13s %s: %.0f to %.0f ft in 30 s%s\n", e.id.c_str(),
                    aircraft.trimmed_laterally() ? "trimmed"
                    : aircraft.trimmed()         ? "trimmed in pitch alone"
                                                 : "not trimmed",
                    lowest_ft, highest_ft,
                    named                  ? ", left out: JSBSim cannot trim her"
                    : e.id == ungoverned   ? ", left out: trimmed before her governor"
                                           : "");
        if (e.id == ungoverned) {
            if (aircraft.trimmed_laterally() && highest_ft - 3000.0 > 50.0) {
                ++left_out_ungoverned;
            } else {
                failed += " " + e.id + " (no longer climbs off: move her in)";
            }
            continue;
        }
        if (named) {
            if (aircraft.trimmed()) {
                failed += " " + e.id + " (trims now: move her in)";
            } else {
                ++left_out;
            }
            continue;
        }
        if (aircraft.trimmed_laterally() && std::isfinite(lowest_ft) &&
            std::isfinite(highest_ft) && 3000.0 - lowest_ft <= 100.0 &&
            highest_ft - 3000.0 <= 100.0) {
            ++held_height;
        } else {
            failed += " " + e.id;
        }
    }
    check(failed.empty(), "every aircraft not named, untouched for 30 s, was trimmed and held "
                          "within 100 ft of 3,000; these did not:" + failed);
    check(!catalogue.empty() &&
              held_height + left_out + left_out_ungoverned == catalogue.size() &&
              left_out == untrimmable.size() && left_out_ungoverned == 1,
          "every aircraft in the catalogue was flown: " + std::to_string(held_height) +
              " held their height and " +
              std::to_string(left_out + left_out_ungoverned) + " were left out, of " +
              std::to_string(catalogue.size()));
}

// **A player's first input is what the server held her on, every control,
// every aircraft.** Each of the catalogue's aircraft started as the server
// starts a player's (`Fleet::give`), trimmed level and held on
// held_until_moved as a 16-bit fraction each, her levers read as the server
// says them (frontend::levers_of, `WATCH`) and sent through a state update
// and read back; her client's first input begun from the windowed client's
// own airborne controls, put where those levers say (frontend::keep_held),
// and rounded as the wire rounds an input. All nineteen controls must be the
// server's to the bit - but the gear of an aircraft whose gear does not
// retract and the speedbrake lever of one without speedbrakes, which her
// model ignores and her levers do not say. Until 2026-10-21 the levers said
// no pitch trim and the client kept its own nought: every trimmed aircraft's
// first input took away her trim. Every aircraft counted, the untrimmable
// three among them (held on six tenths of the throttle). The on-final start
// (`--players-on-final`) is not walked here: it is held the same way, on the
// same controls, with her flaps and gear as she was started, and the levers
// say both.
GLIDESLOPE_TEST(every_aircraft_a_players_first_input_sends_is_what_the_server_held_her_on) {
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data());
    std::size_t alike = 0;
    std::size_t with_trim = 0;
    std::string failed;
    for (const CatalogueEntry& e : catalogue) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = 3000.0;
        ic.airspeed_kts = e.start_airspeed_kts;
        ic.engine_running = true;
        ic.gear = 0.0;
        ic.trim = true;
        ic.trim_lateral = true;
        aircraft.initialize(ic);
        const auto held = glideslope::net::as_sent(
            glideslope::sim::held_until_moved(aircraft).as_list());
        aircraft.set_controls(glideslope::sim::Controls::from_list(held));
        aircraft.step();
        glideslope::net::StatePacket packet;
        packet.watched = glideslope::frontend::levers_of(aircraft, 3);
        const auto bytes = glideslope::net::write_state(packet);
        const auto heard =
            bytes ? glideslope::net::read_state(std::span<const std::uint8_t>(*bytes))
                  : std::nullopt;
        if (!heard || !heard->watched) {
            failed += " " + e.id + " (her levers were not read back)";
            continue;
        }
        // The windowed client's own, in the air, before the levers.
        glideslope::sim::Controls first;
        first.throttle = e.start_throttle;
        first.gear = 0.0;
        glideslope::frontend::keep_held(*heard->watched, first);
        const auto sent = glideslope::net::as_sent(first.as_list());
        std::string apart;
        for (std::size_t i = 0; i < held.size(); ++i) {
            const bool ignored = (i == 10 && !aircraft.gear_retracts()) ||
                                 (i == 12 && !aircraft.speedbrakes());
            if (!ignored && sent[i] != held[i]) {
                apart += " control " + std::to_string(i) + " " + std::to_string(sent[i]) +
                         " for " + std::to_string(held[i]);
            }
        }
        const glideslope::sim::Controls h = glideslope::sim::Controls::from_list(held);
        std::printf("  %-13s pitch trim %+.4f, aileron %+.4f, rudder %+.4f held: %s\n",
                    e.id.c_str(), h.pitch_trim, h.aileron, h.rudder,
                    apart.empty() ? "sent alike" : apart.c_str());
        if (apart.empty()) {
            ++alike;
        } else {
            failed += " " + e.id + ":" + apart;
        }
        if (h.pitch_trim != 0.0) {
            ++with_trim;
        }
    }
    check(failed.empty(), "every aircraft's first input is what she was held on; these were "
                          "not:" + failed);
    check(!catalogue.empty() && alike == catalogue.size(),
          "every aircraft in the catalogue was walked: " + std::to_string(alike) + " of " +
              std::to_string(catalogue.size()));
    // A trim of nought everywhere would make the rule trivially kept.
    check(with_trim > 0, "and some were held on a pitch trim: " + std::to_string(with_trim));
}

// **Every aircraft's flap lever is found where her flaps answer one**
// (Aircraft::flap_lever, which the autopilot's clean configuration reads):
// flown 30 s at 3,000 ft and her start speed on the autopilot, once with the
// flap control at 0 and once at 1, her flaps move by more than half a degree
// between the two exactly where her model is found to read the lever. The
// F-22A's flaps are scheduled on her speed - up at her start speed, out by 15
// degrees at her climb speed - and read none: she is the case the lever is
// found for, and is checked by name. Every aircraft is flown, counted.
GLIDESLOPE_TEST(every_aircraft_has_a_flap_lever_exactly_where_her_flaps_answer_one) {
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data());
    const auto flaps_after = [&](const CatalogueEntry& e, double lever) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = 3000.0;
        ic.airspeed_kts = e.start_airspeed_kts;
        ic.engine_running = true;
        ic.gear = 0.0;
        aircraft.initialize(ic);
        glideslope::sim::Controls controls;
        controls.throttle = e.start_throttle;
        controls.gear = 0.0;
        controls.flaps = lever;
        glideslope::sim::Autopilot autopilot(aircraft, controls);
        glideslope::sim::AutopilotModes modes = autopilot.modes();
        modes.altitude_ft = 3000.0;
        modes.airspeed_kts = e.start_airspeed_kts;
        autopilot.set(modes);
        for (int i = 0; i < 30 * steps_per_second; ++i) {
            aircraft.set_controls(autopilot.fly());
            aircraft.step();
        }
        return std::pair{aircraft.has_property("fcs/flap-pos-deg")
                             ? aircraft.property("fcs/flap-pos-deg")
                             : 0.0,
                         aircraft.flap_lever()};
    };
    std::size_t with_lever = 0;
    std::size_t without = 0;
    bool f22_flown = false;
    for (const CatalogueEntry& e : catalogue) {
        const auto [up_deg, lever] = flaps_after(e, 0.0);
        const auto [down_deg, lever_again] = flaps_after(e, 1.0);
        const bool answers = down_deg - up_deg > 0.5;
        std::printf("  %-13s flaps %5.1f with the lever up, %5.1f down; lever found: %s\n",
                    e.id.c_str(), up_deg, down_deg, lever ? "yes" : "no");
        check(lever == lever_again && lever == answers,
              e.id + "'s flaps " + (answers ? "answer" : "do not answer") +
                  " the lever, and her model is found " + (lever ? "to read" : "not to read") +
                  " one");
        (lever ? with_lever : without) += 1;
        f22_flown = f22_flown || (e.id == "f22" && !lever);
    }
    check(with_lever + without == catalogue.size(),
          "every aircraft flown: " + std::to_string(with_lever) + " with a lever and " +
              std::to_string(without) + " without, of " + std::to_string(catalogue.size()));
    check(f22_flown, "the F-22A, whose flaps are her own, was flown and has no lever");
}

// Every aircraft the data holds, chosen to fly, takes off: from a runway at
// full throttle - a seaplane from water - rotated at six tenths of its
// catalogue airspeed, it climbs through 200 ft within two minutes.
GLIDESLOPE_TEST(every_aircraft_the_data_holds_takes_off_from_a_runway) {
    for (const CatalogueEntry& e : glideslope::sim::read_catalogue(data())) {
        const double height = take_off(data(), e);
        std::printf("%s: %.0f ft above the %s\n", e.id.c_str(), height,
                    e.seaplane ? "water" : "runway");
        check(height >= 200.0, e.id + " (" + e.name + ") took off and climbed through 200 ft: " +
                                   std::to_string(height) + " ft");
    }
}

// A landplane that alights on water does not roll out on it: set down on
// water as on land, no wheel of any aircraft the data holds takes its weight;
// it ditches - brought to rest where it met the water, no deeper than it was
// then - where on land it rolls on.
GLIDESLOPE_TEST(every_aircraft_the_data_holds_that_alights_on_water_does_not_roll_out_on_it) {
    // Every aircraft is flown, and every one that fails named, before the test fails.
    std::string failures;
    const auto expect = [&](bool condition, const std::string& what) {
        if (!condition) {
            failures += "\n  " + what;
        }
    };
    for (const CatalogueEntry& e : glideslope::sim::read_catalogue(data())) {
        if (e.seaplane) {
            // Not a landplane: it alights on water, and comes to rest afloat.
            const Alighting water = alight(data(), e, true);
            std::printf("%s: on water, %s, at %.1f kt at the end, its centre of gravity at "
                        "least %.1f ft above the water\n",
                        e.id.c_str(), water.ditched ? "ditched" : "afloat", water.speed_kts,
                        water.lowest_ft);
            expect(water.touched && water.finite && !water.ditched && water.lowest_ft > 0.0 &&
                       water.speed_kts < 0.5,
                   e.id + " alit on water and came to rest afloat: " +
                       (water.ditched ? "ditched" : "not ditched") + ", " +
                       std::to_string(water.speed_kts) + " kt, " +
                       std::to_string(water.lowest_ft) + " ft");
            continue;
        }
        const Alighting land = alight(data(), e, false);
        const Alighting water = alight(data(), e, true);
        std::printf("%s: on land, rolled %.0f ft and was at %.1f kt; on water, %s and moved "
                    "%.1f ft, its centre of gravity %.1f ft above the water\n",
                    e.id.c_str(), land.moved_ft, land.speed_kts,
                    water.touched ? "ditched" : "did not ditch", water.moved_ft,
                    water.lowest_ft);
        expect(land.touched && land.wheels_took_weight && land.moved_ft > 500.0,
               e.id + " landed on its wheels on land and rolled on, as a control: " +
                   std::to_string(land.moved_ft) + " ft");
        expect(water.touched && water.finite && !water.wheels_took_weight,
               e.id + " ditched on the water, no wheel taking its weight");
        expect(water.speed_kts < 0.01 && water.moved_ft < 0.01,
               e.id + " was brought to rest where it met the water: it moved " +
                   std::to_string(water.moved_ft) + " ft");
        expect(water.lowest_ft > 0.0, e.id + " stays with its centre of gravity above the water: " +
                                          std::to_string(water.lowest_ft) + " ft");
    }
    check(failures.empty(), "every aircraft that alit on water ditched and did not roll out:" +
                                failures);
}

// No flight model opens a network socket. JSBSim's 737 opened a telnet port
// and a UDP port whenever it was loaded, for other programs to drive it;
// glideslope's aircraft talk to nothing but glideslope. Every model file the
// data holds, its comments set aside, has no <input port> and no socket
// output.
GLIDESLOPE_TEST(no_flight_model_opens_a_network_socket) {
    int files = 0;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(data() / "jsbsim" / "aircraft")) {
        if (entry.path().extension() != ".xml") {
            continue;
        }
        std::ifstream in(entry.path());
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        for (std::size_t open = text.find("<!--"); open != std::string::npos;
             open = text.find("<!--", open)) {
            const std::size_t close = text.find("-->", open);
            text.erase(open, close == std::string::npos ? std::string::npos : close + 3 - open);
        }
        ++files;
        check(text.find("<input port") == std::string::npos &&
                  text.find("type=\"SOCKET\"") == std::string::npos,
              entry.path().string() + " opens no network socket");
    }
    check(files > 0, "the data holds flight models");
}

GLIDESLOPE_TEST(an_aircraft_is_added_by_its_file_alone_and_refused_where_it_is_wrong) {
    // A copy of the data, and one file more: another aircraft on the Cessna's
    // model, started slower. No code knows of it.
    const std::filesystem::path copy =
        std::filesystem::temp_directory_path() /
        ("glideslope-catalogue-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::remove_all(copy);
    std::filesystem::create_directories(copy);
    std::filesystem::copy(data() / "aircraft", copy / "aircraft",
                          std::filesystem::copy_options::recursive);
    std::filesystem::copy(data() / "jsbsim", copy / "jsbsim",
                          std::filesystem::copy_options::recursive);
    // The models' published figures too: a light aeroplane's climb speed,
    // which its autopilot's floor is, is read from them as it loads. They are
    // the model's, so the new aircraft on the Cessna's model shares them.
    std::filesystem::copy(data() / "figures", copy / "figures",
                          std::filesystem::copy_options::recursive);
    {
        std::ofstream out(copy / "aircraft" / "trainer.aircraft", std::ios::binary);
        out << "# A trainer, for the test.\nname Slow Trainer\nmodel c172p\nclass "
               "light-aircraft\nstart 80 0.55\nbrakes 29 none\n";
    }
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(copy);
    check(catalogue.size() == glideslope::sim::read_catalogue(data()).size() + 1,
          "the new file is one aircraft more");
    const CatalogueEntry trainer = glideslope::sim::find_aircraft(copy, "trainer");
    check(trainer.name == "Slow Trainer" && trainer.model == "c172p" &&
              trainer.start_airspeed_kts == 80.0 && trainer.start_throttle == 0.55,
          "the trainer is as its file says");
    const Held h = hold_its_start(copy, trainer);
    check(std::abs(h.altitude_ft - 3000.0) < 100.0 &&
              std::abs(h.airspeed_kts - 80.0) < 5.0,
          "and flies, held at its start: " + std::to_string(h.altitude_ft) + " ft, " +
              std::to_string(h.airspeed_kts) + " KCAS");

    // A model the data does not hold is refused.
    {
        std::ofstream out(copy / "aircraft" / "ghost.aircraft", std::ios::binary);
        out << "name Ghost\nmodel no_such_model\nclass light-aircraft\nstart 100 "
               "0.7\nbrakes 29 none\n";
    }
    bool refused_model = false;
    try {
        glideslope::sim::read_catalogue(copy);
    } catch (const CatalogueError& e) {
        refused_model =
            std::string(e.what()).find("no_such_model") != std::string::npos;
    }
    check(refused_model, "an aircraft whose model is not in the data is refused");
    std::filesystem::remove_all(copy);

    check(refused("name A\nmodel c172p\nclass light-aircraft\nstart 100 0.7\nwings 2\n",
                  "line 5: no command \"wings\""),
          "a command it does not know, by its line");
    check(refused("name A\nmodel c172p\nclass light-aircraft\nstart 100 1.5\n",
                  "line 4: the throttle"),
          "a throttle past full");
    check(refused("name A\nmodel c172p\nclass light-aircraft\n",
                  "must give the aircraft's name, model, start and class"),
          "an aircraft with no start");
    // **An aircraft that does not say which class it is, is refused.** A
    // lesson is found by the class its aeroplane declares, so an aeroplane
    // without one can be taught nothing - and until now nothing said so.
    check(refused("name A\nmodel c172p\nstart 100 0.7\n",
                  "must give the aircraft's name, model, start and class"),
          "an aircraft with no class");
    check(refused("name A\nmodel c172p\nclass biplane\nstart 100 0.7\n", "biplane"),
          "an aircraft whose class is not one of the seven");
    check(refused("name A\nmodel c172p\nclass light-aircraft\nstart 100 0.7\nseaplane yes\n",
                  "line 5: seaplane, alone"),
          "a seaplane line with more on it");
    check(glideslope::sim::parse_catalogue_entry(
              "x", "name A\nmodel m\nclass seaplane\nstart 90 0.6\nseaplane\n")
                  .seaplane &&
              !glideslope::sim::parse_catalogue_entry(
                   "x", "name A\nmodel m\nclass light-aircraft\nstart 90 0.6\nbrakes 29 none\n")
                   .seaplane,
          "an aircraft is a seaplane only if its file says so");
    // The autopilot's rudder gains: both numbers, each 0 to 1, or the
    // defaults where the file says nothing.
    const std::string gains = "name A\nmodel m\nclass bomber\nstart 90 0.6\nbrakes 200 fully-modulating\n";
    const CatalogueEntry given = glideslope::sim::parse_catalogue_entry(
        "x", gains + "yaw-damper 0.07 0.01\n");
    const CatalogueEntry not_given = glideslope::sim::parse_catalogue_entry("x", gains);
    check(given.yaw_damper_per_degps == 0.07 && given.rudder_integral_rate == 0.01 &&
              not_given.yaw_damper_per_degps == 0.05 && not_given.rudder_integral_rate == 0.05,
          "a yaw-damper line gives both gains, and none gives 0.05 and 0.05");
    check(refused(gains + "yaw-damper 0.05\n", "line 6: yaw-damper PER_DEGPS INTEGRAL_RATE"),
          "a yaw-damper line with one number");
    check(refused(gains + "yaw-damper 0.05 2\n", "rudder integral"),
          "a yaw-damper line whose integral rate is past 1");
    check(glideslope::sim::parse_catalogue_entry("x",
                                                 "name Two  Words # a comment\nmodel "
                                                 "m\nclass light-aircraft\nstart 90 0.6\nbrakes 29 none\n")
                  .name == "Two Words",
          "a name of words, a comment after it");
    // **A landplane's brakes**: her tyres' pressure and her anti-skid, each
    // of the four kinds; a landplane without them is refused, a seaplane not.
    const std::string plane = "name A\nmodel m\nclass light-aircraft\nstart 90 0.6\n";
    using glideslope::sim::AntiSkid;
    std::size_t kinds = 0;
    for (const auto& [word, kind] :
         std::vector<std::pair<std::string, AntiSkid>>{
             {"fully-modulating", AntiSkid::fully_modulating},
             {"quasi-modulating", AntiSkid::quasi_modulating},
             {"on-off", AntiSkid::on_off},
             {"none", AntiSkid::none}}) {
        const auto b =
            glideslope::sim::parse_catalogue_entry("x", plane + "brakes 42.5 " + word + "\n")
                .brakes;
        check(b && b->tyre_pressure_psi == 42.5 && b->anti_skid == kind,
              "brakes 42.5 " + word + " read as it says");
        ++kinds;
    }
    check(kinds == 4, "all four anti-skids read: " + std::to_string(kinds));
    check(refused(plane, "must give its brakes"), "a landplane without brakes");
    check(refused(plane + "brakes 42 abs\n", "line 5: no anti-skid \"abs\""),
          "an anti-skid that is not one of the four");
    check(refused(plane + "brakes 42\n", "line 5: brakes TYRE_PSI ANTI_SKID"),
          "a brakes line without its anti-skid");
    check(refused(plane + "brakes 0 none\n", "line 5: the tyre pressure"),
          "a tyre with no pressure");
    bool none = false;
    try {
        glideslope::sim::find_aircraft(data(), "no_such_aircraft");
    } catch (const CatalogueError&) {
        none = true;
    }
    check(none, "an aircraft the data does not hold is not found");
}

// **Every aircraft says what kind of flying it is for, and the roster agrees
// with `REQUIREMENTS.md`'s own table.** A lesson is written against a class -
// a circuit in a Cub and a circuit in a 747 are not the same lesson - so the
// class has to be data rather than prose, and the prose has to go on
// agreeing with it.
GLIDESLOPE_TEST(every_aircraft_says_which_class_it_is_and_the_requirements_agree) {
    const std::filesystem::path data = GLIDESLOPE_TEST_DATA_DIR;
    const auto roster = glideslope::sim::read_catalogue(data.parent_path());
    check(roster.size() == 16, "sixteen aircraft, not " + std::to_string(roster.size()));

    // The seven names, and nothing else, round-trip through the pair that
    // reads and writes them. All 7 walked and counted.
    const std::vector<std::string> seven = {
        "light-aircraft", "seaplane", "second-world-war", "business-jet",
        "airliner",       "fighter",  "bomber"};
    check(seven.size() == glideslope::sim::aircraft_class_count,
          "seven classes, and the header says " +
              std::to_string(glideslope::sim::aircraft_class_count));
    std::size_t walked = 0;
    for (const std::string& name : seven) {
        const auto of = glideslope::sim::class_from_name(name);
        check(of.has_value(), name + " is a class");
        check(glideslope::sim::name_of(*of) == name, name + " reads back as itself");
        ++walked;
    }
    check(walked == 7, "every class was walked");
    check(!glideslope::sim::class_from_name("glider").has_value(),
          "and a name that is not one of the seven is refused");
    check(!glideslope::sim::class_from_name("").has_value(), "as is nothing");

    // **The table in REQUIREMENTS.md, read and compared.** It names each
    // class against the aircraft in it; every aircraft in the roster must
    // appear in the row its own file claims.
    const std::filesystem::path doc =
        std::filesystem::path(GLIDESLOPE_TEST_PROJECT_DIR) / "docs" / "REQUIREMENTS.md";
    std::ifstream in(doc);
    check(in.good(), "docs/REQUIREMENTS.md is there");
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());

    // What the document calls each class, against what a file calls it.
    const std::vector<std::pair<std::string, std::string>> rows = {
        {"light-aircraft", "| Light aircraft |"},
        {"seaplane", "| Seaplane |"},
        {"second-world-war", "| Second World War |"},
        {"business-jet", "| Business jet |"},
        {"airliner", "| Airliners |"},
        {"fighter", "| Fighter"},
        {"bomber", "| Bomber |"},
    };
    std::size_t said = 0;
    for (const auto& [name, heading] : rows) {
        check(text.find(heading) != std::string::npos,
              "REQUIREMENTS.md still has a row for " + name);
        ++said;
    }
    check(said == 7, "a row was looked for for every class");

    // Each aircraft named in the document's table, in the row its file
    // claims. The names are the document's, which are not the file ids.
    const std::vector<std::pair<std::string, std::string>> named = {
        {"j3cub", "Piper J-3 Cub"},      {"c172p", "Cessna 172P"},
        {"pa28", "Piper PA-28"},         {"c182", "Cessna 182"},
        {"short_s23", "Short S.23"},     {"mosquito-fb6", "Mosquito"},
        {"learjet35a", "Learjet 35A"},   {"a320", "Airbus A320"},
        {"737-300", "Boeing 737"},       {"747-400", "747"},
        {"787-8", "787-8"},              {"a380", "Airbus A380"},
        {"f15c", "F-15 Eagle"},          {"f22", "F-22 Raptor"},
        {"f35b", "F-35B Lightning II"},  {"b2", "B-2 Spirit"},
    };
    check(named.size() == 16, "every aircraft has a name in the table");
    std::size_t checked = 0;
    for (const auto& [id, in_doc] : named) {
        const auto it = std::find_if(roster.begin(), roster.end(),
                                     [&](const auto& e) { return e.id == id; });
        check(it != roster.end(), id + " is in the roster");
        check(text.find(in_doc) != std::string::npos,
              "REQUIREMENTS.md still names " + in_doc);
        // The row it is named in must be the row its file claims. Find the
        // line holding the aircraft, and check it begins with that class.
        const std::size_t at = text.find(in_doc);
        const std::size_t line_from = text.rfind('\n', at) + 1;
        const std::string line = text.substr(line_from, text.find('\n', at) - line_from);
        const std::string want =
            std::string(glideslope::sim::name_of(it->aircraft_class));
        bool right = false;
        for (const auto& [name, heading] : rows) {
            if (line.rfind(heading, 0) == 0) {
                right = name == want;
                break;
            }
        }
        check(right, id + " says it is a " + want +
                         ", and REQUIREMENTS.md names it on the line: " + line);
        ++checked;
    }
    check(checked == 16, "all sixteen were held to the table");
}

// **An id from the wire names an aircraft in the catalogue, or none.** A
// server's `AIRCRAFT` message gives the id a client loads a model by, and an
// id joined to a path as it stood would open whatever it was told to. Every
// aircraft's own id is known; ids that climb out of the data, or name nothing,
// are not - each case counted.
GLIDESLOPE_TEST(an_aircraft_named_from_the_wire_is_one_in_the_catalogue_or_none) {
    const auto roster = glideslope::sim::read_catalogue(data());
    std::size_t known = 0;
    for (const auto& e : roster) {
        const auto found = glideslope::sim::known_aircraft(data(), e.id);
        check(found.has_value() && found->id == e.id && found->model == e.model,
              e.id + " is known, with its own model");
        ++known;
    }
    check(known == roster.size() && known == 16, "all sixteen are known");
    const std::vector<std::string> strangers = {
        "", "../../../etc/passwd", "../jsbsim/aircraft/c172p/c172p", "c172p/../f15c",
        "/etc/passwd", "C172P", "c172p ", "no-such-aircraft"};
    for (const std::string& id : strangers) {
        check(!glideslope::sim::known_aircraft(data(), id).has_value(),
              "\"" + id + "\" names no aircraft");
    }
    check(strangers.size() == 8, "eight strangers tried");
}

// **An aircraft says its gear retracts where its model has retracting gear,
// and nowhere else** - every one in the catalogue, against its model's own
// file. JSBSim has a gear command for fixed gear too, and a HUD that asked
// for that showed a Cessna's gear as down and able to come up.
GLIDESLOPE_TEST(an_aircraft_says_its_gear_retracts_only_where_its_model_has_it) {
    const auto roster = glideslope::sim::read_catalogue(data());
    std::size_t retracting = 0;
    std::size_t fixed = 0;
    for (const auto& e : roster) {
        std::ifstream in(data() / "jsbsim" / "aircraft" / e.model / (e.model + ".xml"),
                         std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        // Written `<retractable>1</retractable>`, or with spaces inside.
        static const std::regex retractable(R"(<retractable>\s*1\s*</retractable>)");
        const bool in_its_file = std::regex_search(text, retractable);
        const glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        check(aircraft.gear_retracts() == in_its_file,
              e.id + (in_its_file ? " has retracting gear" : " has fixed gear"));
        if (in_its_file) {
            ++retracting;
        } else {
            ++fixed;
        }
    }
    check(retracting + fixed == roster.size() && roster.size() == 16,
          "all sixteen were asked: " + std::to_string(retracting) + " retracting and " +
              std::to_string(fixed) + " fixed");
}

// **Every aircraft with speedbrakes slows on its lever, and every other
// ignores it** - every one in the catalogue, against its flight model. Each
// is flown twice for ten seconds from its catalogue start at 5,000 ft, its
// gear up, held level by a test pilot (sim/test_pilot.hpp) on the throttle it
// starts with: once with the lever stowed and once fully out, which every
// model's surfaces reach in two seconds. Held level, because an airliner's
// spoilers dump lift as well as adding drag: left alone she sinks and gathers
// speed, and the Learjet, whose spoilers take 0.15 of lift and add only 80% of
// her zero-lift drag, gained energy so. One whose catalogue says
// `speedbrakes` must then be at least 2 knots slower with them out - the
// 747-400 is the least, at 3.1, and the B-2A the most, at 30 - and one whose
// catalogue says nothing must fly exactly the same flight both times, to the
// last bit of its state. So the catalogue, which the HUD and a state update
// read to show the lever only where there is one, is held to the flight
// model in both directions.
GLIDESLOPE_TEST(every_aircraft_with_speedbrakes_slows_on_its_lever_held_level_and_every_other_ignores_it) {
    const auto roster = glideslope::sim::read_catalogue(data());
    struct Flown {
        glideslope::sim::AircraftState state;
        bool told = false; // the aircraft says what the catalogue says
    };
    const auto fly = [&](const CatalogueEntry& e, double lever) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = 5000.0;
        ic.airspeed_kts = e.start_airspeed_kts;
        ic.engine_running = true;
        ic.gear = 0.0;
        aircraft.initialize(ic);
        glideslope::sim::TestPilot pilot(aircraft);
        glideslope::sim::Controls c;
        c.throttle = e.start_throttle;
        c.gear = 0.0;
        c.speedbrake = lever;
        for (int i = 0; i < 10 * steps_per_second; ++i) {
            c.elevator = pilot.pitch_to(pilot.pitch_for_altitude(5000.0));
            c.aileron = pilot.roll_to(0.0);
            c.rudder = pilot.coordinate();
            aircraft.set_controls(c);
            aircraft.step();
        }
        return Flown{aircraft.state(), aircraft.speedbrakes() == e.speedbrakes};
    };
    std::size_t with = 0;
    std::size_t without = 0;
    std::vector<std::string> wrong;
    for (const CatalogueEntry& e : roster) {
        const Flown stowed = fly(e, 0.0);
        const Flown out = fly(e, 1.0);
        const double slower_kts = stowed.state.airspeed_kts - out.state.airspeed_kts;
        std::printf("  %-13s %-11s %6.1f kt stowed, %6.1f out: %+6.2f kt; %+5.0f ft\n",
                    e.id.c_str(), e.speedbrakes ? "speedbrakes" : "none",
                    stowed.state.airspeed_kts, out.state.airspeed_kts, -slower_kts,
                    out.state.altitude_ft - stowed.state.altitude_ft);
        if (!stowed.told || !out.told) {
            wrong.push_back(e.id + ": the aircraft does not say what its catalogue says");
        }
        if (e.speedbrakes) {
            if (!(slower_kts >= 2.0)) {
                wrong.push_back(e.id + " has speedbrakes, and with them out is " +
                                std::to_string(slower_kts) +
                                " kt slower after ten seconds, not 2 or more");
            }
            ++with;
        } else {
            if (!(stowed.state == out.state)) {
                wrong.push_back(e.id + " has no speedbrakes, and the lever changed its flight");
            }
            ++without;
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " aircraft are not as the catalogue says");
    check(with + without == roster.size() && roster.size() == 16,
          "all sixteen were flown: " + std::to_string(with) + " with speedbrakes and " +
              std::to_string(without) + " without");
}

// **The F-15C's speedbrake does not deploy past 15 degrees of alpha**, as the
// 1988 F-15 aerobase has it (AFIT/GAE/ENY/90D-16, page 81; tools/make_f15c.py).
// She is flown at 300 knots at 5,000 ft by a test pilot, the lever fully out
// throughout: held level for five seconds, under the limit, the surface runs
// out; then pulled with the stick fully back until she has been past the
// limit for three seconds - longer than the surface's two to run in - it is
// in, with the lever still out. Both sides of the limit are built, and the
// alpha each is flown at checked.
GLIDESLOPE_TEST(the_f15cs_speedbrake_stays_in_past_fifteen_degrees_of_alpha_with_the_lever_out) {
    const CatalogueEntry e = glideslope::sim::find_aircraft(data(), "f15c");
    const auto fly = [&](bool pulled) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = 5000.0;
        ic.airspeed_kts = 300.0;
        ic.engine_running = true;
        ic.gear = 0.0;
        aircraft.initialize(ic);
        glideslope::sim::TestPilot pilot(aircraft);
        glideslope::sim::Controls c;
        c.throttle = e.start_throttle;
        c.gear = 0.0;
        c.speedbrake = 1.0;
        for (int i = 0; i < 5 * steps_per_second; ++i) {
            c.elevator = pilot.pitch_to(pilot.pitch_for_altitude(5000.0));
            c.aileron = pilot.roll_to(0.0);
            c.rudder = pilot.coordinate();
            aircraft.set_controls(c);
            aircraft.step();
        }
        // Past the limit for three seconds on end, or ten seconds pulling.
        int past = 0;
        for (int i = 0; pulled && past < 3 * steps_per_second && i < 10 * steps_per_second;
             ++i) {
            c.elevator = 1.0;
            c.aileron = pilot.roll_to(0.0);
            c.rudder = pilot.coordinate();
            aircraft.set_controls(c);
            aircraft.step();
            past = aircraft.property("aero/alpha-deg") > 15.0 ? past + 1 : 0;
        }
        const double alpha = aircraft.property("aero/alpha-deg");
        const double out = aircraft.property("fcs/speedbrake-pos-norm");
        std::printf("  %s: alpha %.1f, speedbrake %.2f out with the lever at %.2f\n",
                    pulled ? "pulled" : "level", alpha, out,
                    aircraft.property("fcs/speedbrake-cmd-norm"));
        return std::pair{alpha, out};
    };
    const auto [level_alpha, level_out] = fly(false);
    check(level_alpha < 15.0, "level at 300 kt she flies under 15 degrees of alpha");
    check(level_out > 0.99, "and her speedbrake is out");
    const auto [pulled_alpha, pulled_out] = fly(true);
    check(pulled_alpha > 15.0, "pulled, she has been past 15 degrees of alpha three seconds");
    check(pulled_out < 0.01, "and her speedbrake is in, the lever still out");
}

// **Every aircraft knows the angles of attack and sideslip its aerodynamics'
// tables hold** (Aircraft::alpha_range_rad, beta_range_rad), read from its
// model: finite, and holding level flight's - 0 to 10 degrees of alpha and
// either side of no sideslip - for all sixteen. A trial stops a flight past
// them (sim/orbit_trial.hpp). A model with no table keyed on sideslip has no
// bound on it, and is named.
GLIDESLOPE_TEST(every_aircraft_knows_the_alpha_and_sideslip_its_tables_hold) {
    const std::vector<std::string> no_sideslip_table = {
        "a380", "b2", "f15c", "f35b", "learjet35a", "mosquito-fb6", "pa28", "short_s23"};
    const auto roster = glideslope::sim::read_catalogue(data());
    std::size_t read = 0;
    std::size_t unbounded = 0;
    std::string failures;
    constexpr double deg = 180.0 / std::numbers::pi;
    for (const CatalogueEntry& e : roster) {
        const glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        const auto [alpha_low, alpha_high] = aircraft.alpha_range_rad();
        const auto [beta_low, beta_high] = aircraft.beta_range_rad();
        std::printf("  %-13s alpha %7.1f to %6.1f, sideslip %7.1f to %6.1f degrees\n",
                    e.id.c_str(), alpha_low * deg, alpha_high * deg, beta_low * deg,
                    beta_high * deg);
        if (!(std::isfinite(alpha_low) && std::isfinite(alpha_high) && alpha_low <= 0.0 &&
              alpha_high >= 10.0 / deg)) {
            failures += "\n  " + e.id + "'s alpha tables do not hold 0 to 10 degrees";
        }
        if (std::find(no_sideslip_table.begin(), no_sideslip_table.end(), e.id) !=
            no_sideslip_table.end()) {
            if (!(std::isinf(beta_low) && std::isinf(beta_high))) {
                failures += "\n  " + e.id + " is named without a sideslip table and has one";
            }
            ++unbounded;
        } else {
            if (!(std::isfinite(beta_low) && std::isfinite(beta_high) && beta_low < 0.0 &&
                  beta_high > 0.0)) {
                failures += "\n  " + e.id + "'s sideslip tables do not hold either side of none";
            }
        }
        ++read;
    }
    check(failures.empty(), "each as stated:" + failures);
    check(read == roster.size() && read == 16 && unbounded == no_sideslip_table.size(),
          "all sixteen read, and each named without a sideslip table is one");
}

// **A trial stops a flight that leaves its tables, and judges it not held**:
// the F-22A at 120 kt in calm air, at 3,000 ft on the autopilot with her
// gear down - 20 kt under her slowest - departs, her alpha past the 90
// degrees her tables hold. Gear down, as she was measured: with it up -
// the trials' clean since 2026-10-10 - her flight controls give her their
// thrust vectoring and full pitch rate, and she holds 115 kt without
// departing (sinking 2,700 ft in the two minutes). It was 130 kt, 10 under; that is a flight figure of its own
// now (the next test), so that the rule here does not hang on where between
// 120 and 130 she stops holding. Flown on, MSVC's debug JSBSim asserted in a table lookup
// (CI run 37609045925); stopped there, nothing is looked up past them.
GLIDESLOPE_TEST(a_trial_stops_a_flight_whose_alpha_or_sideslip_leaves_its_tables_and_judges_it_not_held) {
    const CatalogueEntry e = glideslope::sim::find_aircraft(data(), "f22");
    const glideslope::sim::CrosswindFlown f =
        glideslope::sim::fly_heading_in_crosswind(data(), e, 120.0, false, 30.0,
                                                  glideslope::sim::TrialConfiguration::gear_down());
    std::printf("  f22 at 120 kt in calm air, gear down: left its tables %s, sideslip at most %.1f\n",
                f.left_tables ? "yes" : "no", f.most_sideslip_ever_deg);
    check(f.left_tables, "she left her tables, and the flight was stopped");
    check(f.most_sideslip_ever_deg <= 90.0, "no step was looked up past them");
    check(!f.held(), "and it is not held");
}

// **The F-22A held at 130 kt on the autopilot departs**, 10 kt under her
// slowest, **with her gear down**: a stated flight figure, so that a change
// to the autopilot that moves it shows. Measured with the gear down, the
// trial's default until 2026-10-10; flown with it up - the trials' clean
// since - she holds 130 without departing, her flight controls giving her
// their thrust vectoring and full pitch rate only with the gear up, so the
// gear is put down here to keep the situation it pins. Measured (2026-10-10): she leaves her tables, her
// sideslip 10.4 by then. Reading the speed's trend on the throttle, tried
// and taken out (PROJECT_STATUS, 2026-10-10), held her there: the throttle
// opened as her speed began to fall rather than once it had, and she never
// slowed into the departure.
// **Her sideslip as she goes is no longer pinned** (2026-10-13): since the
// throttle stopped chasing the speed a climb trades for height, up to 3 kt
// of it (sim/autopilot.cpp, `climb_as_speed_s`), she still departs at 130
// but her sideslip by then is 0.0, and from 125 to 129 kt it was 4.0 to
// 15.8 - not a figure to hold her to, where the departure itself is.
// Unbounded, that term held her at 130 kt, giving up 240 ft. **The edge**:
// she departs at 125, 127, 129 and 130 kt and holds at 131, 133 and 135 -
// within a knot of it here (PROJECT_STATUS, 2026-10-13).
GLIDESLOPE_TEST(the_f22a_held_at_130_kt_on_the_autopilot_departs) {
    const CatalogueEntry e = glideslope::sim::find_aircraft(data(), "f22");
    const glideslope::sim::CrosswindFlown f =
        glideslope::sim::fly_heading_in_crosswind(data(), e, 130.0, false, 30.0,
                                                  glideslope::sim::TrialConfiguration::gear_down());
    std::printf("  f22 at 130 kt in calm air, gear down: left its tables %s, sideslip at most %.1f\n",
                f.left_tables ? "yes" : "no", f.most_sideslip_ever_deg);
    check(f.left_tables, "she left her tables at 130 kt");
}
