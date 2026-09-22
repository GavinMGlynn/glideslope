#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/catalogue.hpp"
#include "sim/terrain.hpp"
#include "sim/test_pilot.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
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
    {
        std::ofstream out(copy / "aircraft" / "trainer.aircraft", std::ios::binary);
        out << "# A trainer, for the test.\nname Slow Trainer\nmodel c172p\nclass "
               "light-aircraft\nstart 80 0.55\n";
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
               "0.7\n";
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
                   "x", "name A\nmodel m\nclass light-aircraft\nstart 90 0.6\n")
                   .seaplane,
          "an aircraft is a seaplane only if its file says so");
    check(glideslope::sim::parse_catalogue_entry("x",
                                                 "name Two  Words # a comment\nmodel "
                                                 "m\nclass light-aircraft\nstart 90 0.6\n")
                  .name == "Two Words",
          "a name of words, a comment after it");
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
