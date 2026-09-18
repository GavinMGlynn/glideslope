#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/catalogue.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
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
    aircraft.initialize(ic);
    glideslope::sim::Controls controls;
    controls.throttle = e.start_throttle;
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
        out << "# A trainer, for the test.\nname Slow Trainer\nmodel c172p\nstart 80 "
               "0.55\n";
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
        out << "name Ghost\nmodel no_such_model\nstart 100 0.7\n";
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

    check(refused("name A\nmodel c172p\nstart 100 0.7\nwings 2\n",
                  "line 4: no command \"wings\""),
          "a command it does not know, by its line");
    check(refused("name A\nmodel c172p\nstart 100 1.5\n", "line 3: the throttle"),
          "a throttle past full");
    check(refused("name A\nmodel c172p\n",
                  "must give the aircraft's name, model and start"),
          "an aircraft with no start");
    check(glideslope::sim::parse_catalogue_entry("x",
                                                 "name Two  Words # a comment\nmodel "
                                                 "m\nstart 90 0.6\n")
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
